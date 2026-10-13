#include "mangacontroller.h"

#include <QScopedValueRollback>

#include "perftimer.h"

MangaController::MangaController(NetworkManager *networkManager, MangaRepository *repository, QObject *parent)
    : QObject(parent),
      currentIndex(repository, nullptr, 0, 0),
      networkManager(networkManager),
      repository(repository),
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
                     [this](const QString &, const QString &path) { failedDownloads.insert(path, clock.elapsed()); });
}

bool MangaController::setCurrentManga(QSharedPointer<MangaInfo> mangaInfo)
{
    cancelAllPreloads();

    if (currentManga)
        QObject::disconnect(currentManga.get(), &MangaInfo::chaptersMoved, this,
                            &MangaController::chaptersMoved);

    currentManga.clear();
    currentManga = mangaInfo;
    currentIndex = MangaIndexTraverser(repository, currentManga, 0, 0);
    deserializeProgress();

    QObject::connect(currentManga.get(), &MangaInfo::chaptersMoved, this, &MangaController::chaptersMoved);

    emit currentMangaChanged(mangaInfo);

    auto res = assurePagesLoaded();
    // another manga was opened while the page list loaded (event loop), that one is shown
    if (currentManga != mangaInfo)
        return false;
    if (res.has_value())
        currentIndexChangedInternal(false);
    else
        emit error(res.error().message);

    emit activity();
    return res.has_value();
}

void MangaController::chaptersMoved()
{
    // the reading progress has the url of the chapter, it is found again
    deserializeProgress();
    auto res = assurePagesLoaded();
    if (res.has_value())
        currentIndexChangedInternal(false);
    else
        emit error(res.error().message);
}

Result<> MangaController::assurePagesLoaded()
{
    if (currentManga->chapters.count() == 0)
        return Err(QString("Manga has no chapters."));

    if (currentIndex.chapter >= currentManga->chapters.count() || currentIndex.chapter < 0)
        currentIndex.chapter = qMax(0, currentManga->chapters.count() - 1);

    if (!currentIndex.currentChapter().pagesLoaded)
    {
        auto res = repository->loadPageList(currentManga, currentIndex.chapter).wait();

        if (!res.has_value())
            return res;

        currentManga->serialize();
    }

    if (currentIndex.chapter >= currentManga->chapters.count() || currentIndex.chapter < 0)
        return Err(QString("Chapter number out of bounds."));

    if (currentIndex.page >= currentIndex.currentChapter().pages.count())
        currentIndex.page = 0;

    return {};
}

void MangaController::setCurrentIndex(const MangaIndex &index)
{
    auto res = currentIndex.setChecked(index.chapter, index.page);
    if (res.has_value())
        currentIndexChangedInternal(true);
    else
        emit error(res.error().message);
}

void MangaController::currentIndexChangedInternal(bool preload)
{
    const auto &chapter = currentIndex.currentChapter();
    emit currentIndexChanged({currentIndex, chapter.chapterUrl, static_cast<int>(currentManga->chapters.count()),
                              static_cast<int>(chapter.pages.count())});

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
    const QString path = manga->pagePath(index.chapter, index.page);
    if (QFile::exists(path))
    {
        emit currentImageChanged(path);
        return;
    }

    // the page list and the image can need downloads (an event loop): page turns, chapter updates and another
    // manga can come in between
    auto image = repository->pageImage(manga, index.chapter, index.page).wait();

    // another page turn came in between, it shows its own page
    if (!isCurrent())
        return;

    if (!image.has_value())
    {
        emit currentImageChanged("error");
        emit error(image.error().message);
        return;
    }

    // a running preload of the page is reused
    auto job = repository->downloadImage(manga->sourceName, image.value(), path);

    if (job->isCompleted)
    {
        emit currentImageChanged(job->filepath);
        return;
    }

    if (job->hasFailed())
    {
        emit currentImageChanged("error");
        emit error(job->error.message);
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
                         failedDownloads.insert(pageJob->filepath, clock.elapsed());
                         if (isCurrent())
                         {
                             emit currentImageChanged("error");
                             emit error(pageJob->error.message);
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
        if (res.has_value())
        {
            inbound = res.value();
        }
        else
        {
            emit error(res.error().message);
            return;
        }
    }
    else  // if (direction == backward)
    {
        auto res = currentIndex.decrement();
        if (res.has_value())
        {
            inbound = res.value();
        }
        else
        {
            emit error(res.error().message);
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
    const QString path = manga->pagePath(index.chapter, index.page);
    if (QFile::exists(path))
    {
        emit completedImagePreloadSignal(path);
        return;
    }

    auto image = repository->pageImage(manga, index.chapter, index.page).wait();

    if (!image.has_value())
    {
        emit error(image.error().message);
        return;
    }

    // the chapter can have been moved or removed meanwhile (event loop): its pages go elsewhere
    if (path != manga->pagePath(index.chapter, index.page) || QFile::exists(path))
        return;

    // it shows up when the running download is done, a queued one would start over if it failed
    if (networkManager->isDownloading(path))
        return;

    const qint64 skipFailedMs = 2 * 60 * 1000;
    auto failed = failedDownloads.constFind(path);
    if (failed != failedDownloads.constEnd() && clock.elapsed() - failed.value() < skipFailedMs)
        return;

    preloadQueue.appendDownload(repository->imageDownload(manga->sourceName, image.value(), path));

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
    preloadQueue.clearQueue();

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
            if (!res.has_value())
                emit error(res.error().message);
            else if (res.value())
                preloadImage(manga, traverser);

            if (preloadAgain || currentManga != manga)
                return;
        }
    }
}

void MangaController::cancelAllPreloads()
{
    preloadQueue.clearQueue();
}

void MangaController::completedImagePreload(const QString &, const QString &path)
{
    emit completedImagePreloadSignal(path);
}

void MangaController::serializeProgress()
{
    const auto &chapter = currentIndex.currentChapter();
    ReadingProgress progress(currentIndex, chapter.chapterUrl, currentManga->chapters.count(),
                             chapter.pages.count());
    progress.serialize(currentManga->sourceName, currentManga->url);
}

void MangaController::deserializeProgress()
{
    ReadingProgress progress(currentManga->sourceName, currentManga->url);
    progress.locate(currentManga->chapters);
    currentIndex.chapter = progress.index.chapter;
    currentIndex.page = progress.index.page;
}
