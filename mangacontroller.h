#ifndef MANGACONTROLLER_H
#define MANGACONTROLLER_H

#include <QImage>

#include "defines.h"
#include "downloadimagedescriptor.h"
#include "enums.h"
#include "mangachapter.h"
#include "mangaindextraverser.h"
#include "mangainfo.h"
#include "preloadqueue.h"
#include "readingprogress.h"

class MangaController : public QObject
{
    Q_OBJECT

public:
    MangaController(DownloadManager *downloadManager,
                    QObject *parent = nullptr);

    QSharedPointer<MangaInfo> currentManga;
    MangaIndexTraverser currentIndex;

signals:
    void indexMovedOutOfBounds();
    void downloadError(const QString &error);
    void currentMangaChanged(QSharedPointer<MangaInfo> mangaInfo);
    void currentIndexChanged(const ReadingProgress &progress);
    void currentImageChanged(const QString &imagePath);
    void completedImagePreloadSignal(const QString &path);

public slots:
    void setCurrentManga(QSharedPointer<MangaInfo> mangaInfo);

    void setCurrentIndex(const MangaIndex &index);
    void advanceMangaPage(PageTurnDirection direction);

    QString getCoverpathScaled() const;
    QString getImageLink(const MangaIndex &index);

    void preloadNeighbours(int Forward, int Backward);
    void preloadImage(const MangaIndex &index);

    void preloadPopular();
    void cancelAllPreloads();

private slots:
    void completedImagePreload(const QString &path);
    bool assurePagesLoaded();

private:
    void currentIndexChangedInternal(bool preload);
    void updateCurrentImage();
    void deserializeProgress();
    void serializeProgress();
    DownloadManager *downloadManager;
    PreloadQueue preloadQueue;
};

#endif  // MANGACONTROLLER_H
