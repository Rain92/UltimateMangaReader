#pragma once

#include <QObject>

#include "abstractmangasource.h"
#include "settings.h"

// The manga sources: all of them, the enabled ones and the one whose manga list is shown.
class SourceRegistry : public QObject
{
    Q_OBJECT

public:
    explicit SourceRegistry(NetworkManager *networkManager, QObject *parent = nullptr);

    const QList<QSharedPointer<AbstractMangaSource>> &all() const { return sources; }
    // By name. The map stays the same object (e.g. the favorites keep a reference to it).
    const QMap<QString, AbstractMangaSource *> &active() const { return activeSources; }
    // Null if none is chosen.
    AbstractMangaSource *current() const { return currentSource; }

    // Enables the sources the settings enable, a source that isn't in the settings yet is enabled (and added).
    void updateActive(Settings &settings);
    void setCurrent(AbstractMangaSource *source);

signals:
    void activeSourcesChanged(const QList<AbstractMangaSource *> &sources);
    void currentSourceChanged(AbstractMangaSource *source);

private:
    QList<QSharedPointer<AbstractMangaSource>> sources;
    QMap<QString, AbstractMangaSource *> activeSources;
    AbstractMangaSource *currentSource;
};
