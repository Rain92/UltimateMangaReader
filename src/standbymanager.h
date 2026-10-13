#pragma once

#include <QObject>
#include <QTimer>

#include <functional>

#include "settings.h"

class MangaChapterDownloadManager;
class NetworkManager;
class SuspendManager;

// Puts the device into standby (suspend to RAM, the WiFi chip stays associated in its WoWLAN mode) after a
// few seconds without input or background work, e.g. while a page is read. A tap wakes it up again, which
// takes ~0.3 s, so this trades responsiveness for battery life and only runs when enabled in the settings.
class StandbyManager : public QObject
{
    Q_OBJECT

public:
    StandbyManager(Settings *settings, SuspendManager *suspendManager, NetworkManager *networkManager,
                   MangaChapterDownloadManager *chapterDownloadManager, QObject *parent = nullptr);

    // Restarts the idle countdown, called on input and on network or reader activity.
    void postpone();

    // No standby while set, e.g. while the manga lists are updated (which waits between its downloads).
    void setInhibited(bool inhibited);

    // Returns the ms until the device has to be awake again (< 0: no deadline). QTimers stand still while
    // suspended, so the rtc alarm wakes the device for it.
    void setWakeDeadline(std::function<qint64()> msUntilDeadline);

    // Clock that keeps counting while suspended.
    static qint64 bootTimeMs();

signals:
    // The device woke up again after sleeping for asleepMs.
    void resumed(qint64 asleepMs);

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    Settings *settings;
    SuspendManager *suspendManager;
    NetworkManager *networkManager;
    MangaChapterDownloadManager *chapterDownloadManager;
    std::function<qint64()> msUntilDeadline;

    bool supported;
    bool inhibited;
    int failures;
    int maxSleepSeconds;
    QTimer idleTimer;

    int delayMs() const;
    void idleTimeout();
    bool enterStandby(int wakeAfterSeconds);
};
