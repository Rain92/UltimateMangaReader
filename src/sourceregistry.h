#pragma once

#include <QObject>

#include "mangasourcebase.h"
#include "settings.h"

// The manga sources: all of them, the enabled ones and the one whose manga list is shown.
class SourceRegistry : public QObject
{
    Q_OBJECT

public:
    explicit SourceRegistry(NetworkManager *networkManager, QObject *parent = nullptr);

    const QList<QSharedPointer<MangaSourceBase>> &all() const { return sources; }
    // Null if there is none with the name.
    MangaSourceBase *source(const QString &name) const;

    // By name. The map stays the same object (e.g. the favorites keep a reference to it).
    const QMap<QString, MangaSourceBase *> &active() const { return activeSources; }
    // Null if none is chosen.
    MangaSourceBase *current() const { return currentSource; }

    // Enables the sources the settings enable, a source that isn't in the settings yet is enabled (and added).
    void updateActive(Settings &settings);
    void setCurrent(MangaSourceBase *source);

signals:
    void activeSourcesChanged(const QList<MangaSourceBase *> &sources);
    void currentSourceChanged(MangaSourceBase *source);

private:
    QList<QSharedPointer<MangaSourceBase>> sources;
    QMap<QString, MangaSourceBase *> activeSources;
    MangaSourceBase *currentSource;
};
