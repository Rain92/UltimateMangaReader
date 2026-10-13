#pragma once

#include <QElapsedTimer>
#include <QFutureWatcher>
#include <QImage>
#include <QString>
#include <QTimer>

#include "favorite.h"
#include "mangarepository.h"
#include "networkmanager.h"
#include "readingprogress.h"
#include "sourceregistry.h"
#include "task.h"

// A cover thumbnail to load, or to make from the cover if there is none yet.
struct ThumbnailRequest
{
    QString key;
    QString coverPath;
    QString thumbnailPath;
    qreal pixelRatio;
};

// The favorites, with their infos and reading progress (for the list). Every change is signalled with the
// index of the favorite.
class FavoritesManager : public QObject
{
    Q_OBJECT
public:
    FavoritesManager(SourceRegistry *sources, MangaRepository *repository, NetworkManager *networkManager,
                     QObject *parent = nullptr);
    ~FavoritesManager();

    int count() const { return entries.count(); }
    const Favorite &favorite(int i) const { return entries[i].favorite; }
    // Null until loadInfos() was called, and when the source of the favorite isn't enabled.
    QSharedPointer<MangaInfo> info(int i) const { return entries.value(i).info; }
    // Loaded with the infos.
    const ReadingProgress &progress(int i) const { return entries[i].progress; }
    // -1 if the manga isn't a favorite.
    int indexOf(const MangaInfo *info) const;

    bool isFavorite(QSharedPointer<MangaInfo> info) const;
    bool toggleFavorite(QSharedPointer<MangaInfo> info);

    void moveFavoriteToFront(int i);
    // Updates the infos from the sites in the background (new chapters). The ones that fail are reported
    // together (error()).
    void updateInfos();
    // Loads the infos and the reading progress from the cache, without downloads, so it doesn't block. An info
    // that isn't cached yet only has its title and url until updateInfos() loaded it.
    void loadInfos();
    // The loaded info of favorite i: from the cache, else downloaded (blocking), e.g. when it gets opened
    // before updateInfos() loaded it. Null if it isn't available.
    QSharedPointer<MangaInfo> requireInfo(int i);
    void clearFavorites();
    void clearInvalidFavorites();
    void resetUpdatedStatus();
    // The reader moved on in a manga.
    void setProgress(const MangaInfo *info, const ReadingProgress &progress);

    // Loads the thumbnails of the covers on worker threads (made from the covers if there are none yet), see
    // thumbnailLoaded(). A cover that is downloaded later gets its thumbnail too.
    void loadThumbnails(const QList<int> &indices, qreal pixelRatio);

signals:
    void error(const QString &error);
    // all of them changed (e.g. loaded or cleared)
    void favoritesReset();
    void favoriteInserted(int i);
    void favoriteRemoved(int i);
    void favoriteMoved(int from, int to);
    // its info or its reading progress changed
    void favoriteChanged(int i);
    void thumbnailLoaded(const QString &favoriteKey, const QImage &image);

private:
    struct Entry
    {
        Favorite favorite;
        QSharedPointer<MangaInfo> info;
        ReadingProgress progress;
    };

    SourceRegistry *sources;
    MangaRepository *repository;
    NetworkManager *networkManager;
    QList<Entry> entries;
    bool infosLoaded;
    QHash<QString, QElapsedTimer> lastUpdates;
    bool updateAfterConnecting;

    // the thumbnails are loaded in batches on worker threads
    QFutureWatcher<QImage> thumbnailWatcher;
    QList<ThumbnailRequest> thumbnailBatch;
    QList<ThumbnailRequest> queuedThumbnails;
    QElapsedTimer thumbnailBatchTimer;
    bool thumbnailBatchRunning;
    // of the last loadThumbnails(), 0 before
    qreal thumbnailPixelRatio;

    Task<> update(QList<QSharedPointer<MangaInfo>> infos);
    void serialize();
    void deserialize();

    void sourcesChanged();
    void loadEntry(Entry &entry);
    void connectInfo(const QSharedPointer<MangaInfo> &info);
    void removeAt(int i);
    int indexOf(const Favorite &fav) const;
    void infoUpdated(bool newChapters);
    void chaptersMoved();
    void coverLoaded();

    void queueThumbnail(int i);
    void startThumbnailBatch();
    void thumbnailReady(int index);
};
