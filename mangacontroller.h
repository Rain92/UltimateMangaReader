#ifndef MANGACONTROLLER_H
#define MANGACONTROLLER_H

#include <QImage>

#include "defines.h"
#include "downloadimagedescriptor.h"
#include "enums.h"
#include "mangachapter.h"
#include "mangaindex.h"
#include "mangainfo.h"
#include "preloadqueue.h"

class MangaController : public QObject
{
    Q_OBJECT

public:
    MangaController(DownloadManager *downloadManager,
                    QObject *parent = nullptr);

    QList<QSharedPointer<MangaInfo>> test;
    QSharedPointer<MangaInfo> currentManga;
    MangaIndex currentIndex;

signals:
    void indexMovedOutOfBounds();
    void downloadError(const QString &error);
    void currentMangaChanged(QSharedPointer<MangaInfo> mangaInfo);
    void currentIndexChanged(int chapter, int page, int numChapters,
                             int numPages);
    void currentImageChanged(const QString &imagePath);
    void completedImagePreloadSignal(const QString &path);

public slots:
    void setCurrentManga(QSharedPointer<MangaInfo> mangaInfo);

    void setCurrentIndex(int chapter, int page);
    void advanceMangaPage(PageTurnDirection direction);

    QString getCoverpathScaled() const;
    QString getImageLink(const MangaIndex &index);

    void preloadNeighbours(int Forward, int Backward);
    void preloadImage(const MangaIndex &index);

    void preloadPopular();
    void cancelAllPreloads();

private slots:
    void completedImagePreload(const QString &path);

private:
    void currentIndexChangedInternal(bool preload);
    void updateCurrentImage();
    bool deserializeProgress();
    void serializeProgress();
    DownloadManager *downloadManager;
    PreloadQueue preloadQueue;
};

#endif  // MANGACONTROLLER_H
