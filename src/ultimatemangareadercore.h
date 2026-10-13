#pragma once

#include <QObject>

#include "favoritesmanager.h"
#include "mangachapterdownloadmanager.h"
#include "mangacontroller.h"
#include "mangalistupdater.h"
#include "settings.h"
#include "sourceregistry.h"
#include "standbymanager.h"
#include "suspendmanager.h"
#include "utils.h"

class UltimateMangaReaderCore : public QObject
{
    Q_OBJECT
public:
    explicit UltimateMangaReaderCore(QObject *parent = nullptr);

    NetworkManager *networkManager;
    SourceRegistry *sources;
    MangaController *mangaController;
    FavoritesManager *favoritesManager;
    MangaChapterDownloadManager *mangaChapterDownloadManager;
    SuspendManager *suspendManager;

    Settings settings;

    StandbyManager *standbyManager;
    MangaListUpdater *mangaListUpdater;

public:
    // Opens a manga of the list of the current source.
    void setCurrentManga(const QString &mangaUrl, const QString &mangatitle);

    void clearDownloadCache(ClearDownloadCacheLevel level);
    void updateActiveSources();

    void enableTimers(bool enabled);

    void activity();

signals:
    void downloadCacheCleared(ClearDownloadCacheLevel level);

    void error(const QString &error);

    void timeTick();

private:
    QTimer timer;
    QTimer clockAlignTimer;
    QTimer autoSuspendTimer;
    qint64 autoSuspendStart;
    int currentDay;
    // counts the opened mangas: loading one can take a download, another one may be opened meanwhile
    int mangaRequests;

    void timerTick();
    void alignClock();
    void restartAutoSuspend();
    qint64 msUntilAutoSuspend() const;
    void resumedFromStandby();
    void setupDirectories();
};
