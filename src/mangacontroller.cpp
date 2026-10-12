#include "mangacontroller.h"

MangaController::MangaController(NetworkManager *networkManager, QObject *parent)
    : QObject(parent),
      currentIndex(nullptr, 0, 0),
      networkManager(networkManager),
      preloadQueue(networkManager, {}, 1, false),
      clock(),
      failedDownloads()
{
    clock.start();

    QObject::connect(&preloadQueue, &DownloadQueue::singleDownloadCompleted, this,
                     &MangaController::completedImagePreload);
    QObject::connect(&preloadQueue, &DownloadQueue::singleDownloadFailed, this,
                     [this](const QString &url) { failedDownloads.insert(url, clock.elapsed()); });
}

void MangaController::setCurrentManga(QSharedPointer<MangaInfo> mangaInfo)
{
    cancelAllPreloads();

    if (currentManga.get())
        QObject::disconnect(currentManga.get(), &MangaInfo::chaptersMoved, this,
                            &MangaController::chaptersMoved);

    currentManga.clear();
    currentManga = mangaInfo;
    currentIndex = MangaIndexTraverser(currentManga, 0, 0);
    deserializeProgress();

    QObject::connect(currentManga.get(), &MangaInfo::chaptersMoved, this, &MangaController::chaptersMoved);

    emit currentMangaChanged(mangaInfo);

    auto res = assurePagesLoaded();
    if (res.isOk())
        currentIndexChangedInternal(false);
    else
        emit error(res.unwrapErr());

    emit activity();
}

void MangaController::chaptersMoved(QList<QPair<int, int>>)
{
    deserializeProgress();
    currentIndexChangedInternal(false);
}

Result<void, QString> MangaController::assurePagesLoaded()
{
    if (currentManga->chapters.count() == 0)
        return Err(QString("Manga has no chapters."));

    if (currentIndex.chapter >= currentManga->chapters.count() || currentIndex.chapter < 0)
        currentIndex.chapter = qMax(0, currentManga->chapters.count() - 1);

    auto &chapter = currentIndex.currentChapter();
    bool pageOnDisk = chapter.pagesLoaded && currentIndex.page < chapter.imageUrlList.count() &&
                      QFile::exists(currentManga->mangaSource->getImagePath(DownloadImageDescriptor(
                          "", currentManga->title, currentIndex.chapter, currentIndex.page)));

    if (!pageOnDisk && (!chapter.pagesLoaded || currentIndex.page >= chapter.imageUrlList.count() ||
                        chapter.imageUrlList[currentIndex.page] == ""))
    {
        auto res = currentManga->mangaSource->updatePageList(currentManga, currentIndex.chapter);

        if (!res.isOk())
            return res;

        currentManga->serialize();
    }

    if (currentIndex.chapter >= currentManga->chapters.count() || currentIndex.chapter < 0)
        return Err(QString("Chapter number out of bounds."));

    if (currentIndex.page >= currentIndex.currentChapter().pageUrlList.count())
        currentIndex.page = 0;

    return Ok();
}

void MangaController::setCurrentIndex(const MangaIndex &index)
{
    auto res = currentIndex.setChecked(index.chapter, index.page);
    if (res.isOk())
        currentIndexChangedInternal(true);
    else
        emit error(res.unwrapErr());
}

Result<QString, QString> MangaController::getImageUrl(const MangaIndex &index)
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

    if (currentManga->chapters[index.chapter].imageUrlList[index.page] == "")
    {
        auto res = currentManga->mangaSource->getImageUrl(
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
        {currentIndex, static_cast<int>(currentManga->chapters.count()), static_cast<int>(currentIndex.currentChapter().pageUrlList.count())});

    updateCurrentImage();

    serializeProgress();

    if (preload)
        QTimer::singleShot(50, this, &MangaController::preloadNeighbours);

    emit activity();
}

void MangaController::updateCurrentImage()
{
    // already downloaded pages don't need an image url (which might need a network request)
    auto cachedPath = currentManga->mangaSource->getImagePath(
        DownloadImageDescriptor("", currentManga->title, currentIndex.chapter, currentIndex.page));
    if (QFile::exists(cachedPath))
    {
        emit currentImageChanged(cachedPath);
        return;
    }

    auto imageUrl = getImageUrl(currentIndex);

    if (!imageUrl.isOk())
    {
        emit currentImageChanged("error");
        emit error(imageUrl.unwrapErr());
        return;
    }

    auto dd = DownloadImageDescriptor(imageUrl.unwrap(), currentManga->title, currentIndex.chapter,
                                      currentIndex.page);

    // a running preload of the page is reused
    auto job = currentManga->mangaSource->downloadImage(dd);

    if (job->isCompleted)
    {
        emit currentImageChanged(job->filepath);
        return;
    }

    if (job->hasFailed())
    {
        emit currentImageChanged("error");
        emit error(job->errorString);
        return;
    }

    // The page is shown when it's done, the previous one stays until then. Waiting for it with a timeout
    // showed the error page whenever downloading and processing took longer (the download went on
    // regardless). The network manager keeps the job alive until it finished, the connections end with it.
    auto manga = currentManga;
    MangaIndex index(currentIndex.chapter, currentIndex.page);
    auto isCurrent = [this, manga, index]()
    { return currentManga == manga && currentIndex.chapter == index.chapter && currentIndex.page == index.page; };

    DownloadFileJob *pageJob = job.get();
    QObject::connect(pageJob, &DownloadJobBase::completed, this,
                     [this, pageJob, isCurrent]()
                     {
                         if (isCurrent())
                             emit currentImageChanged(pageJob->filepath);
                     });
    QObject::connect(pageJob, &DownloadJobBase::downloadError, this,
                     [this, pageJob, isCurrent]()
                     {
                         failedDownloads.insert(pageJob->originalUrl, clock.elapsed());
                         if (isCurrent())
                         {
                             emit currentImageChanged("error");
                             emit error(pageJob->errorString);
                         }
                     });
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
    auto cachedPath = currentManga->mangaSource->getImagePath(
        DownloadImageDescriptor("", currentManga->title, index.chapter, index.page));
    if (QFile::exists(cachedPath))
        return;

    auto imageUrl = getImageUrl(index);

    if (!imageUrl.isOk())
    {
        emit error(imageUrl.unwrapErr());
        return;
    }

    DownloadImageDescriptor imageinfo(imageUrl.unwrap(), currentManga->title, index.chapter, index.page);
    auto path = currentManga->mangaSource->getImagePath(imageinfo);

    if (QFile::exists(path))
        return;

    // it shows up when the running download is done, a queued one would start over if it failed
    if (networkManager->isDownloading(imageUrl.unwrap()))
        return;

    const qint64 skipFailedMs = 2 * 60 * 1000;
    auto failed = failedDownloads.constFind(networkManager->downloadUrl(imageUrl.unwrap()));
    if (failed != failedDownloads.constEnd() && clock.elapsed() - failed.value() < skipFailedMs)
        return;

    //    qDebug() << "preload page" << index.page;

    preloadQueue.appendDownload(FileDownloadDescriptor(imageUrl.unwrap(), path));
}

void MangaController::preloadPopular()
{
    if (currentManga->chapters.count() == 0)
        return;

    if (currentManga->chapters.count() > 1 && currentIndex.chapter != currentManga->chapters.count() - 1)
        preloadImage({static_cast<int>(currentManga->chapters.count() - 1), 0});
}

void MangaController::preloadNeighbours()
{
    int forward = CONF.forwardPreloads;
    int backward = CONF.backwardPreloads;

    // the pages queued at the previous page turns (which might be behind by now), a running one goes on
    preloadQueue.clearQuene();

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
            auto res = backwardindex.decrement();
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
    ReadingProgress c(currentIndex, currentManga->chapters.count(),
                      currentIndex.currentChapter().pageUrlList.count());
    c.serialize(currentManga->hostname, currentManga->title);
}

void MangaController::deserializeProgress()
{
    ReadingProgress progress(currentManga->hostname, currentManga->title);
    currentIndex.chapter = progress.index.chapter;
    currentIndex.page = progress.index.page;
}
