#include "mangacontroller.h"

MangaController::MangaController(DownloadManager *downloadManager, QObject *parent)
    : QObject(parent),
      currentIndex(nullptr, 0, 0),
      downloadManager(downloadManager),
      preloadQueue(downloadManager, {}, 2, false)
{
    QObject::connect(&preloadQueue, &DownloadQueue::singleDownloadCompleted, this,
                     &MangaController::completedImagePreload);
}

void MangaController::setCurrentManga(QSharedPointer<MangaInfo> mangaInfo)
{
    cancelAllPreloads();
    currentManga.clear();
    currentManga = mangaInfo;
    currentIndex = MangaIndexTraverser(currentManga, 0, 0);
    deserializeProgress();

    emit currentMangaChanged(mangaInfo);

    auto res = assurePagesLoaded();
    if (res.isOk())
        currentIndexChangedInternal(false);
    else
        emit error(res.unwrapErr());
}

Result<void, QString> MangaController::assurePagesLoaded()
{
    if (currentManga->chapters.count() == 0)
        return Err(QString("Manga has no chapters."));

    if (currentIndex.chapter >= currentManga->chapters.count() || currentIndex.chapter < 0)
        currentIndex.chapter = qMax(0, currentManga->chapters.count() - 1);

    if (!currentIndex.currentChapter().pagesLoaded)
    {
        auto res = currentManga->mangaSource->updatePageList(currentManga, currentIndex.chapter);

        if (!res.isOk())
            return res;

        currentManga->serialize();
    }

    if (currentIndex.chapter >= currentManga->chapters.count() || currentIndex.chapter < 0)
        return Err(QString("Chapter number out of bounds."));

    if (currentIndex.page >= currentIndex.currentChapter().numPages)
        currentIndex.page = 0;

    return Ok();
}

Result<QString, QString> MangaController::getCoverpathScaled() const
{
    if (currentManga->coverPath == "" || currentManga->coverPath.length() < 4)
        return Err(QString("Invalid coverpath."));

    QString scpath = currentManga->coverPath;
    scpath.insert(scpath.length() - 4, "_scaled");

    if (!QFile::exists(scpath))
    {
        qDebug() << "generating scaled:" << currentManga->title;
        QImage img;
        img.load(currentManga->coverPath);
        img = img.scaled(favoritecoverwidth, favoritecoverheight, Qt::KeepAspectRatio,
                         Qt::SmoothTransformation);
        img.save(scpath);
    }

    return Ok(scpath);
}

void MangaController::setCurrentIndex(const MangaIndex &index)
{
    auto res = currentIndex.setChecked(index.chapter, index.page);
    if (res.isOk())
        currentIndexChangedInternal(true);
    else
        emit error(res.unwrapErr());
}

Result<QString, QString> MangaController::getImageLink(const MangaIndex &index)
{
    if (index.chapter < 0 || index.chapter >= currentManga->chapters.count())
        return Err(QString("Chapter number out of bounds."));

    if (!currentManga->chapters[index.chapter].pagesLoaded)
    {
        currentManga->mangaSource->updatePageList(currentManga, index.chapter);
        currentManga->serialize();
    }

    if (index.chapter >= currentManga->chapters.count() ||
        currentManga->chapters[index.chapter].imageUrlList.count() <= index.page)
        return Err(QString("Page index out of bounds."));
    ;

    if (currentManga->chapters[index.chapter].imageUrlList[index.page] == "")
    {
        auto res = currentManga->mangaSource->getImageLink(
            currentManga->chapters[index.chapter].pageUrlList.at(index.page));
        if (!res.isOk())
            return Err(res.unwrapErr());
        currentManga->chapters[index.chapter].imageUrlList[index.page] = res.unwrap();
    }

    return Ok(currentManga->chapters[index.chapter].imageUrlList[index.page]);
}
void MangaController::currentIndexChangedInternal(bool preload)
{
    emit currentIndexChanged(
        {currentIndex, currentManga->chapters.count(), currentIndex.currentChapter().numPages});

    updateCurrentImage();

    serializeProgress();

    if (preload)
        QTimer::singleShot(50, [this]() { preloadNeighbours(CONF.forwardPreloads, CONF.backwardPreloads); });
}

void MangaController::updateCurrentImage()
{
    auto imageLink = getImageLink(currentIndex);

    if (!imageLink.isOk())
    {
        emit error(imageLink.unwrapErr());
        return;
    }

    auto dd = DownloadImageDescriptor(imageLink.unwrap(), currentManga->title, currentIndex.chapter,
                                      currentIndex.page);

    auto imagePath = currentManga->mangaSource->downloadAwaitImage(dd);

    if (imagePath.isOk())
        emit currentImageChanged(imagePath.unwrap());
    else
        emit error(imagePath.unwrapErr());
}

void MangaController::advanceMangaPage(PageTurnDirection direction)
{
    bool inbound = false;
    if (direction == Forward)
    {
        auto res = currentIndex.increment();
        if (res.isOk())
        {
            inbound = res.unwrap();
        }
        else
        {
            emit error(res.unwrapErr());
            return;
        }
    }
    else  // if (direction == backward)
    {
        auto res = currentIndex.decrement();
        if (res.isOk())
        {
            inbound = res.unwrap();
        }
        else
        {
            emit error(res.unwrapErr());
            return;
        }
    }

    if (inbound)
        currentIndexChangedInternal(true);
    else
        emit indexMovedOutOfBounds();
}

void MangaController::preloadImage(const MangaIndex &index)
{
    auto imageLink = getImageLink(index);

    if (!imageLink.isOk())
    {
        emit error(imageLink.unwrapErr());
        return;
    }

    DownloadImageDescriptor imageinfo(imageLink.unwrap(), currentManga->title, index.chapter, index.page);
    auto path = currentManga->mangaSource->getImagePath(imageinfo);

    if (QFile::exists(path))
        return;

    //    qDebug() << "preload page" << index.page;

    preloadQueue.appendDownload(FileDownloadDescriptor(imageLink.unwrap(), path));
}

void MangaController::preloadPopular()
{
    if (currentManga->chapters.count() == 0)
        return;

    if (currentManga->chapters.count() > 1 && currentIndex.chapter != currentManga->chapters.count() - 1)
        preloadImage({currentManga->chapters.count() - 1, 0});
}

void MangaController::preloadNeighbours(int forward, int backward)
{
    MangaIndexTraverser forwardindex(currentIndex);
    MangaIndexTraverser backwardindex(currentIndex);

    for (int i = 0; i < qMax(forward, backward); i++)
    {
        if (i < forward)
        {
            auto res = forwardindex.increment();
            if (res.isOk())
            {
                if (res.unwrap())
                    preloadImage(forwardindex);
            }
            else
            {
                emit error(res.unwrapErr());
            }
        }

        if (i < backward)
        {
            auto res = forwardindex.decrement();
            if (res.isOk())
            {
                if (res.unwrap())
                    preloadImage(backwardindex);
            }
            else
            {
                emit error(res.unwrapErr());
            }
        }
    }
}

void MangaController::cancelAllPreloads()
{
    preloadQueue.clearQuene();
}

void MangaController::completedImagePreload(const QString &, const QString &path)
{
    emit completedImagePreloadSignal(path);
}

void MangaController::serializeProgress()
{
    ReadingProgress c(currentIndex, currentManga->chapters.count(), currentIndex.currentChapter().numPages);
    c.serialize(currentManga->hostname, currentManga->title);
}

void MangaController::deserializeProgress()
{
    ReadingProgress progress(currentManga->hostname, currentManga->title);
    currentIndex.chapter = progress.index.chapter;
    currentIndex.page = progress.index.page;
}
