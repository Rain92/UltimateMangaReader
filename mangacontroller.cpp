#include "mangacontroller.h"

MangaController::MangaController(DownloadManager *downloadManager,
                                 QObject *parent)
    : QObject(parent),
      currentIndex(nullptr, 0, 0),
      downloadManager(downloadManager),
      preloadQueue(downloadManager)
{
    QObject::connect(&preloadQueue, SIGNAL(completedDownload(QString)), this,
                     SLOT(completedImagePreload(QString)));
}

void MangaController::setCurrentManga(QSharedPointer<MangaInfo> mangaInfo)
{
    cancelAllPreloads();
    currentManga.clear();
    currentManga = mangaInfo;
    currentIndex = MangaIndexTraverser(currentManga, 0, 0);
    deserializeProgress();

    emit currentMangaChanged(mangaInfo);

    if (assurePagesLoaded())
        currentIndexChangedInternal(false);
}

bool MangaController::assurePagesLoaded()
{
    if (currentManga->chapters.count() == 0)
        return false;

    if (currentIndex.chapter >= currentManga->chapters.count() ||
        currentIndex.chapter < 0)
        currentIndex.chapter = qMax(0, currentManga->chapters.count() - 1);

    if (!currentIndex.currentChapter().pagesLoaded)
    {
        if (!currentManga->mangaSource->updatePageList(currentManga,
                                                       currentIndex.chapter))
            return false;

        currentManga->serialize();
    }

    if (currentIndex.chapter >= currentManga->chapters.count())
        return false;

    if (currentIndex.page >= currentIndex.currentChapter().numPages)
        currentIndex.page = 0;

    return true;
}

QString MangaController::getCoverpathScaled() const
{
    if (currentManga->coverPath == "" || currentManga->coverPath.length() < 4)
        return "";

    QString scpath = currentManga->coverPath;
    scpath.insert(scpath.length() - 4, "_scaled");

    if (!QFile::exists(scpath))
    {
        qDebug() << "generating scaled:" << currentManga->title;
        QImage img;
        img.load(currentManga->coverPath);
        img = img.scaled(favoritecoverwidth, favoritecoverheight,
                         Qt::KeepAspectRatio, Qt::SmoothTransformation);
        img.save(scpath);
    }

    return scpath;
}

void MangaController::setCurrentIndex(const MangaIndex &index)
{
    if (currentIndex.setChecked(index.chapter, index.page))
        currentIndexChangedInternal(true);
}

QString MangaController::getImageLink(const MangaIndex &index)
{
    if (index.chapter < 0 || index.chapter >= currentManga->chapters.count())
        return "";

    if (!currentManga->chapters[index.chapter].pagesLoaded)
    {
        currentManga->mangaSource->updatePageList(currentManga, index.chapter);
        currentManga->serialize();
    }

    if (index.chapter >= currentManga->chapters.count() ||
        currentManga->chapters[index.chapter].imageUrlList.count() <=
            index.page)
        return "";

    if (currentManga->chapters[index.chapter].imageUrlList[index.page] == "")
        currentManga->chapters[index.chapter].imageUrlList[index.page] =
            currentManga->mangaSource->getImageLink(
                currentManga->chapters[index.chapter].pageUrlList.at(
                    index.page));

    return currentManga->chapters[index.chapter].imageUrlList[index.page];
}
void MangaController::currentIndexChangedInternal(bool preload)
{
    emit currentIndexChanged({currentIndex, currentManga->chapters.count(),
                              currentIndex.currentChapter().numPages});

    updateCurrentImage();

    serializeProgress();

    if (preload)
        QTimer::singleShot(50, [this]() {
            preloadNeighbours(CONF.forwardPreloads, CONF.backwardPreloads);
        });
}
void MangaController::updateCurrentImage()
{
    auto imageLink = getImageLink(currentIndex);

    auto imagePath = currentManga->mangaSource->downloadAwaitImage(
        DownloadImageDescriptor(imageLink, currentManga->title,
                                currentIndex.chapter, currentIndex.page));

    // TODO
    if (imagePath != "")
        emit currentImageChanged(imagePath);
    else
        emit downloadError("Download Error!");
}

void MangaController::advanceMangaPage(PageTurnDirection direction)
{
    bool inbound = false;
    if (direction == Forward)
    {
        inbound = currentIndex.increment();
    }
    else  // if (direction == backward)
    {
        inbound = currentIndex.decrement();
    }

    if (inbound)
        currentIndexChangedInternal(true);
    else
        emit indexMovedOutOfBounds();
}

void MangaController::preloadImage(const MangaIndex &index)
{
    auto link = getImageLink(index);
    DownloadImageDescriptor imageinfo(link, currentManga->title, index.chapter,
                                      index.page);
    auto path = currentManga->mangaSource->getImagePath(imageinfo);

    if (QFile::exists(path))
        return;

    //    qDebug() << "preload page" << index.page;

    preloadQueue.addJob({link, path});
}

void MangaController::preloadPopular()
{
    if (currentManga->chapters.count() == 0)
        return;

    if (currentManga->chapters.count() > 1 &&
        currentIndex.chapter != currentManga->chapters.count() - 1)
        preloadImage({currentManga->chapters.count() - 1, 0});
}

void MangaController::preloadNeighbours(int forward, int backward)
{
    MangaIndexTraverser forwardindex(currentIndex);
    MangaIndexTraverser backwardindex(currentIndex);

    for (int i = 0; i < qMax(forward, backward); i++)
    {
        if (i < forward)
            if (forwardindex.increment())
                preloadImage(forwardindex);

        if (i < backward)
            if (backwardindex.decrement())
                preloadImage(backwardindex);
    }
}

void MangaController::cancelAllPreloads() { preloadQueue.clearQuene(); }

void MangaController::completedImagePreload(const QString &path)
{
    emit completedImagePreloadSignal(path);
}

void MangaController::serializeProgress()
{
    ReadingProgress c(currentIndex, currentManga->chapters.count(),
                      currentIndex.currentChapter().numPages);
    c.serialize(currentManga->hostname, currentManga->title);
}

void MangaController::deserializeProgress()
{
    ReadingProgress progress(currentManga->hostname, currentManga->title);
    currentIndex.chapter = progress.index.chapter;
    currentIndex.page = progress.index.page;
}
