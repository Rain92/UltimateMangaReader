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
    currentIndex = MangaIndex(currentManga, 0, 0);
    deserializeProgress();

    emit currentMangaChanged(mangaInfo);

    if (assurePagesLoaded())
        currentIndexChangedInternal(false);
}

bool MangaController::assurePagesLoaded()
{
    if (currentIndex.chapter > currentManga->numChapters)
        currentIndex.chapter = qMax(0, currentManga->numChapters - 1);

    if (currentIndex.chapter >= currentManga->numChapters)
        return false;

    if (!currentManga->chapters[currentIndex.chapter].pagesLoaded)
    {
        if (!currentManga->mangaSource->updatePageList(currentManga,
                                                       currentIndex.chapter))
            return false;

        currentManga->serialize();
    }

    if (currentIndex.page >=
        currentManga->chapters[currentIndex.chapter].numPages)
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

void MangaController::setCurrentIndex(int chapter, int page)
{
    if (currentIndex.setChecked(chapter, page))
        currentIndexChangedInternal(true);
}

QString MangaController::getImageLink(const MangaIndex &index)
{
    if (index.chapter < 0 || index.chapter >= currentManga->numChapters)
        return "";

    if (!currentManga->chapters[index.chapter].pagesLoaded)
    {
        currentManga->mangaSource->updatePageList(currentManga, index.chapter);
        currentManga->serialize();
    }

    if (index.chapter >= currentManga->numChapters ||
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
    emit currentIndexChanged(
        currentIndex.chapter, currentIndex.page, currentManga->numChapters,
        currentManga->chapters[currentIndex.chapter].numPages);

    updateCurrentImage();

    serializeProgress();

    if (preload)
        QTimer::singleShot(50, [this]() {
            preloadNeighbours(forwardpreloads, backwardpreloads);
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
        emit downloadError("");
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
    if (currentManga->numChapters == 0)
        return;

    if (currentManga->numChapters > 1 &&
        currentIndex.chapter != currentManga->numChapters - 1)
        preloadImage(
            MangaIndex(currentManga, currentManga->numChapters - 1, 0));
}

void MangaController::preloadNeighbours(int forward, int backward)
{
    MangaIndex forwardindex(currentIndex);
    MangaIndex backwardindex(currentIndex);

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
    QFile file(mangainfodir(currentManga->hostname, currentManga->title) +
               "progress.dat");
    if (!file.open(QIODevice::WriteOnly))
        return;

    QDataStream out(&file);
    out << currentIndex << (qint32)currentManga->numChapters;

    file.close();
}

bool MangaController::deserializeProgress()
{
    QFile file(mangainfodir(currentManga->hostname, currentManga->title) +
               "progress.dat");

    if (!file.open(QIODevice::ReadOnly))
        return false;

    QDataStream in2(&file);
    in2 >> this->currentIndex;

    file.close();
    return true;
}
