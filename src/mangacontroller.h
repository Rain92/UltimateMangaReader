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

    void setCurrentManga(QSharedPointer<MangaInfo> mangaInfo);

    void setCurrentIndex(const MangaIndex &index);
    void advanceMangaPage(PageTurnDirection direction);

    Result<QString, QString> getImageUrl(const MangaIndex &index);

    void preloadNeighbours();
    void preloadImage(const MangaIndex &index);

    void preloadPopular();
    void cancelAllPreloads();

signals:
    void indexMovedOutOfBounds();
    void error(const QString &error);
    void currentMangaChanged(QSharedPointer<MangaInfo> mangaInfo);
    void currentIndexChanged(const ReadingProgress &progress);
    void currentImageChanged(const QString &imagePath);
    void completedImagePreloadSignal(const QString &path);
    void activity();

private:
    void completedImagePreload(const QString &, const QString &path);
    Result<void, QString> assurePagesLoaded();
    void currentIndexChangedInternal(bool preload);
    void chaptersMoved(QList<QPair<int, int>> moveMap);
    void updateCurrentImage();
    void deserializeProgress();
    void serializeProgress();
    NetworkManager *networkManager;
    DownloadQueue preloadQueue;

    // Pages that are missing on the server would be requested again (with retries) on every page turn and
    // hold up the preloads behind them: they are skipped for a while. Opening them still tries.
    QElapsedTimer clock;
    QHash<QString, qint64> failedDownloads;
};

#endif  // MANGACONTROLLER_H
