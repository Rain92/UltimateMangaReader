#include "mangacontroller.h"

#include <QScopedValueRollback>

#include "perftimer.h"

MangaController::MangaController(NetworkManager *networkManager, QObject *parent)
    : QObject(parent),
      currentIndex(nullptr, 0, 0),
      networkManager(networkManager),
      preloadQueue(networkManager, {}, 1, false),
      preloading(false),
      preloadAgain(false),
      turning(false),
      clock(),
      failedDownloads()
{
    clock.start();

    QObject::connect(&preloadQueue, &DownloadQueue::singleDownloadCompleted, this,
                     &MangaController::completedImagePreload);
    QObject::connect(&preloadQueue, &DownloadQueue::singleDownloadFailed, this,
                     [this](const QString &url) { failedDownloads.insert(url, clock.elapsed()); });
}

bool MangaController::setCurrentManga(QSharedPointer<MangaInfo> mangaInfo)
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
    // another manga was opened while the page list loaded (event loop), that one is shown
    if (currentManga != mangaInfo)
        return false;
    if (res.isOk())
        currentIndexChangedInternal(false);
    else
        emit error(res.unwrapErr());

    emit activity();
    return res.isOk();
}

void MangaController::chaptersMoved(QList<QPair<int, int>>)
{
    // the files of the pages were moved with their chapters, running preloads still write to the old places
    cancelAllPreloads();
    emit pagesMoved();

    // the reading progress was moved along, the chapter might have fewer pages now
    deserializeProgress();
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

// The page list and the image url can need downloads with event loops: page turns, chapter updates and another
// manga can come in between, so nothing is taken by reference and the slots are checked again before writing.
Result<QString, QString> MangaController::getImageUrl(QSharedPointer<MangaInfo> manga, MangaIndex index)
{
    if (index.chapter < 0 || index.chapter >= manga->chapters.count())
        return Err(QString("Chapter number out of bounds."));

    if (!manga->chapters[index.chapter].pagesLoaded)
    {
        manga->mangaSource->updatePageList(manga, index.chapter);
        manga->serialize();
    }

    auto pageUrl = [&manga, &index]() -> QString
    {
        if (index.chapter >= manga->chapters.count())
            return {};
        const auto &chapter = manga->chapters[index.chapter];
        if (index.page >= chapter.pageUrlList.count() || index.page >= chapter.imageUrlList.count())
            return {};
        return chapter.pageUrlList[index.page];
    };

    QString page = pageUrl();
    if (page.isEmpty())
        return Err(QString("Page index out of bounds."));

    QString imageUrl = manga->chapters[index.chapter].imageUrlList[index.page];
    if (imageUrl.isEmpty())
    {
        auto res = manga->mangaSource->getImageUrl(page);
        if (!res.isOk())
            return Err(res.unwrapErr());
        imageUrl = res.unwrap();

        if (pageUrl() == page)
            manga->chapters[index.chapter].imageUrlList[index.page] = imageUrl;
    }

    return Ok(imageUrl);
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
    auto manga = currentManga;
    MangaIndex index(currentIndex.chapter, currentIndex.page);
    auto isCurrent = [this, manga, index]()
    { return currentManga == manga && currentIndex.chapter == index.chapter && currentIndex.page == index.page; };

    // already downloaded pages don't need an image url (which might need a network request)
    auto cachedPath =
        manga->mangaSource->getImagePath(DownloadImageDescriptor("", manga->title, index.chapter, index.page));
    if (QFile::exists(cachedPath))
    {
        emit currentImageChanged(cachedPath);
        return;
    }

    auto imageUrl = getImageUrl(manga, index);

    // another page turn came in between, it shows its own page
    if (!isCurrent())
        return;

    if (!imageUrl.isOk())
    {
        emit currentImageChanged("error");
        emit error(imageUrl.unwrapErr());
        return;
    }

    auto dd = DownloadImageDescriptor(imageUrl.unwrap(), manga->title, index.chapter, index.page);

    // a running preload of the page is reused
    auto job = manga->mangaSource->downloadImage(dd);

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
    // the turn can wait for a page list (event loop), a second tap meanwhile would work on the old index
    if (turning)
        return;
    QScopedValueRollback<bool> guard(turning, true);

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

    if (perfLoggingEnabled())
        qDebug() << "[perf] turn to page" << currentIndex.chapter << currentIndex.page;

    if (inbound)
        currentIndexChangedInternal(true);
    else
        emit indexMovedOutOfBounds();
}

void MangaController::preloadImage(QSharedPointer<MangaInfo> manga, const MangaIndex &index)
{
    auto cachedPath =
        manga->mangaSource->getImagePath(DownloadImageDescriptor("", manga->title, index.chapter, index.page));
    if (QFile::exists(cachedPath))
    {
        emit completedImagePreloadSignal(cachedPath);
        return;
    }

    auto imageUrl = getImageUrl(manga, index);

    if (!imageUrl.isOk())
    {
        emit error(imageUrl.unwrapErr());
        return;
    }

    DownloadImageDescriptor imageinfo(imageUrl.unwrap(), manga->title, index.chapter, index.page);
    auto path = manga->mangaSource->getImagePath(imageinfo);

    if (QFile::exists(path))
        return;

    // it shows up when the running download is done, a queued one would start over if it failed
    if (networkManager->isDownloading(imageUrl.unwrap()))
        return;

    const qint64 skipFailedMs = 2 * 60 * 1000;
    auto failed = failedDownloads.constFind(networkManager->downloadUrl(imageUrl.unwrap()));
    if (failed != failedDownloads.constEnd() && clock.elapsed() - failed.value() < skipFailedMs)
        return;

    preloadQueue.appendDownload(FileDownloadDescriptor(imageUrl.unwrap(), path));
}

void MangaController::preloadPopular()
{
    if (currentManga->chapters.count() == 0)
        return;

    if (currentManga->chapters.count() > 1 && currentIndex.chapter != currentManga->chapters.count() - 1)
        preloadImage(currentManga, {static_cast<int>(currentManga->chapters.count() - 1), 0});
}

void MangaController::preloadNeighbours()
{
    // Image urls and page lists can need downloads with event loops, a page turn meanwhile schedules this again:
    // the running one starts over for the new page then.
    if (preloading)
    {
        preloadAgain = true;
        return;
    }

    preloading = true;
    do
    {
        preloadAgain = false;
        preloadNeighboursOnce();
    } while (preloadAgain);
    preloading = false;
}

void MangaController::preloadNeighboursOnce()
{
    int forward = CONF.forwardPreloads;
    int backward = CONF.backwardPreloads;

    // the pages queued at the previous page turns (which might be behind by now), a running one goes on
    preloadQueue.clearQuene();

    auto manga = currentManga;
    MangaIndexTraverser forwardindex(currentIndex);
    MangaIndexTraverser backwardindex(currentIndex);

    for (int i = 0; i < qMax(forward, backward); i++)
    {
        for (bool isForward : {true, false})
        {
            if (i >= (isForward ? forward : backward))
                continue;

            auto &traverser = isForward ? forwardindex : backwardindex;
            auto res = isForward ? traverser.increment() : traverser.decrement();
            if (res.isErr())
                emit error(res.unwrapErr());
            else if (res.unwrap())
                preloadImage(manga, traverser);

            if (preloadAgain || currentManga != manga)
                return;
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
