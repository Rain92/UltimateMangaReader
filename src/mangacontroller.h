#ifndef MANGACONTROLLER_H
#define MANGACONTROLLER_H

#include <QImage>

#include "downloadimagedescriptor.h"
#include "downloadqueue.h"
#include "enums.h"
#include "mangachapter.h"
#include "mangaindextraverser.h"
#include "mangainfo.h"
#include "readingprogress.h"
#include "staticsettings.h"
#include "thirdparty/result.h"

class MangaController : public QObject
{
    Q_OBJECT

public:
    MangaController(NetworkManager *networkManager, QObject *parent = nullptr);

    QSharedPointer<MangaInfo> currentManga;
    MangaIndexTraverser currentIndex;

    // Returns false if the reader can't show it (e.g. it has no chapters, the page list didn't load).
    bool setCurrentManga(QSharedPointer<MangaInfo> mangaInfo);

    void setCurrentIndex(const MangaIndex &index);
    void advanceMangaPage(PageTurnDirection direction);

    Result<QString, QString> getImageUrl(QSharedPointer<MangaInfo> manga, MangaIndex index);

    void preloadNeighbours();
    void preloadImage(QSharedPointer<MangaInfo> manga, const MangaIndex &index);

    void preloadPopular();
    void cancelAllPreloads();

signals:
    void indexMovedOutOfBounds();
    void error(const QString &error);
    void currentMangaChanged(QSharedPointer<MangaInfo> mangaInfo);
    void currentIndexChanged(const ReadingProgress &progress);
    void currentImageChanged(const QString &imagePath);
    // a page next to the current one is on disk (downloaded by a preload, or already before)
    void completedImagePreloadSignal(const QString &path);
    // the downloaded pages of the current manga moved to other chapters: cached ones are stale
    void pagesMoved();
    void activity();

private:
    void completedImagePreload(const QString &, const QString &path);
    Result<void, QString> assurePagesLoaded();
    void currentIndexChangedInternal(bool preload);
    void chaptersMoved(QList<QPair<int, int>> moveMap);
    void updateCurrentImage();
    void preloadNeighboursOnce();
    void deserializeProgress();
    void serializeProgress();
    NetworkManager *networkManager;
    DownloadQueue preloadQueue;
    bool preloading;
    bool preloadAgain;
    bool turning;

    // Pages that are missing on the server would be requested again (with retries) on every page turn and
    // hold up the preloads behind them: they are skipped for a while. Opening them still tries.
    QElapsedTimer clock;
    QHash<QString, qint64> failedDownloads;
};

#endif  // MANGACONTROLLER_H
