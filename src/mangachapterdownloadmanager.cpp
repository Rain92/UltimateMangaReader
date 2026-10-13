#include "mangachapterdownloadmanager.h"

MangaChapterDownloadManager::MangaChapterDownloadManager(NetworkManager *networkManager,
                                                         MangaRepository *repository, QObject *parent)
    : QObject(parent),
      generation(0),
      running(false),
      downloadJobs(),
      currentGeneration(0),
      currentFailed(),
      currentImagePaths(),
      failedChapters(),
      networkManager(networkManager),
      repository(repository),
      downloadQueue(networkManager, {}, 2, false)
{
    connect(&downloadQueue, &DownloadQueue::progress, this,
            [this](int c, int t, int e) { emit downloadImagesProgress(c, t, e); });
    connect(&downloadQueue, &DownloadQueue::singleDownloadFailed, this,
            [this](const QString &, const QString &, const QString &errorstr)
            { emit error("Couldn't download image: " + errorstr); });
    connect(&downloadQueue, &DownloadQueue::allCompleted, this,
            &MangaChapterDownloadManager::downloadQueueJobsCompleted);
}

void MangaChapterDownloadManager::cancelDownloads()
{
    // A running runJob() (it waits for page lists) notices it and stops, running stays set until then, or until
    // the image downloads that already run are done.
    generation++;
    downloadJobs.clear();
    downloadQueue.clearQueue();
    failedChapters.clear();
}

void MangaChapterDownloadManager::downloadQueueJobsCompleted()
{
    // after a cancel the downloads that were still running complete, the others never started
    if (currentGeneration == generation)
        recordFailedChapters();
    emit downloadCompleted(failedChapterCount());
    downloadQueue.resetJobCount();
    running = false;
    processNextJob();
}

void MangaChapterDownloadManager::recordFailedChapters()
{
    // the images that couldn't be downloaded aren't there (the ones that were there already are)
    for (const auto &[chapterUrl, path] : std::as_const(currentImagePaths))
        if (!QFile::exists(path))
            currentFailed.insert(chapterUrl);

    if (!currentFailed.isEmpty())
        failedChapters.append({currentManga, QStringList(currentFailed.begin(), currentFailed.end())});

    currentFailed.clear();
    currentImagePaths.clear();
}

int MangaChapterDownloadManager::failedChapterCount() const
{
    int count = 0;
    for (const auto &failed : failedChapters)
        count += failed.chapterUrls.count();
    return count;
}

void MangaChapterDownloadManager::retryFailedDownloads()
{
    downloadJobs.append(failedChapters);
    failedChapters.clear();

    // nothing to do
    if (downloadJobs.isEmpty() && !running)
    {
        emit downloadCompleted(0);
        return;
    }
    processNextJob();
}

void MangaChapterDownloadManager::processNextJob()
{
    if (downloadJobs.empty() || running)
        return;
    running = true;

    runJob(downloadJobs.dequeue());
}

Task<> MangaChapterDownloadManager::runJob(MangaChapterRange job)
{
    const int jobGeneration = generation;
    auto cancelled = [this, jobGeneration]() { return generation != jobGeneration; };

    auto mangaInfo = job.mangaInfo;
    const QStringList chapterUrls = job.chapterUrls;
    currentGeneration = jobGeneration;
    currentManga = mangaInfo;
    currentFailed.clear();
    currentImagePaths.clear();

    // An update of the chapters can move or remove them while the page lists and the images load: they are
    // looked up by url after every download.
    auto chapterIndex = [&mangaInfo](const QString &url) { return mangaInfo->chapters.findChapter(url); };

    emit downloadStart(mangaInfo->title);

    int pagelistErrors = 0;
    for (int i = 0; i < chapterUrls.count() && !cancelled(); i++)
    {
        const QString &url = chapterUrls[i];
        // 3 trys
        for (int t = 0; t < 3 && !cancelled(); t++)
        {
            // gone with an update of the chapters, there's nothing to download anymore
            int c = chapterIndex(url);
            if (c < 0)
            {
                emit downloadPagelistProgress(i + 1, chapterUrls.count(), pagelistErrors);
                break;
            }

            auto res = co_await repository->loadPageList(mangaInfo, c);
            if (res.has_value())
            {
                emit downloadPagelistProgress(i + 1, chapterUrls.count(), pagelistErrors);
                break;
            }
            else if (t == 2)
            {
                currentFailed.insert(url);
                pagelistErrors++;
                emit downloadPagelistProgress(i + 1, chapterUrls.count(), pagelistErrors);
                // skip chapters that can't be loaded (e.g. removed from the source) but keep going
                emit error(
                    QString("Couldn't download pagelist for chapter %1: %2").arg(c + 1).arg(res.error().message));
            }
        }
    }
    mangaInfo->serialize();
    int pagesErrors = 0;
    QList<FileDownloadDescriptor> imageDescriptors;
    for (int i = 0; i < chapterUrls.count() && !cancelled(); i++)
    {
        const QString &url = chapterUrls[i];
        if (currentFailed.contains(url))
            continue;

        for (int p = 0; !cancelled(); p++)
        {
            int c = chapterIndex(url);
            if (c < 0 || !mangaInfo->chapters[c].pagesLoaded || p >= mangaInfo->chapters[c].pages.count())
                break;

            // downloaded before: it needs no image (the queue counts it as completed right away)
            auto path = mangaInfo->pagePath(url, p);
            if (QFile::exists(path))
            {
                imageDescriptors.append({mangaInfo->chapters[c].pages[p].link, path});
                currentImagePaths.append({url, path});
                continue;
            }

            const QString linkUrl = mangaInfo->chapters[c].pages[p].link.url;
            auto image = co_await repository->pageImage(mangaInfo, c, p);

            c = chapterIndex(url);
            if (c < 0 || p >= mangaInfo->chapters[c].pages.count() ||
                mangaInfo->chapters[c].pages[p].link.url != linkUrl)
                break;

            if (!image.has_value())
            {
                pagesErrors++;
                currentFailed.insert(url);
                emit error(QString("Couldn't download page %1 of chapter %2: %3")
                               .arg(p + 1)
                               .arg(c + 1)
                               .arg(image.error().message));
                continue;
            }

            // the pages of the site were resolved to their images
            if (image->url != linkUrl)
                emit downloadPagesProgress(i + 1, chapterUrls.count(), pagesErrors);

            imageDescriptors.append(repository->imageDownload(mangaInfo->mangaSource, *image, path));
            currentImagePaths.append({url, path});
        }
    }
    mangaInfo->serialize();

    if (cancelled())
    {
        running = false;
        processNextJob();
        co_return;
    }

    if (imageDescriptors.isEmpty())
    {
        // the queue never emits allCompleted without jobs
        downloadQueueJobsCompleted();
        co_return;
    }

    downloadQueue.appendDownloads(imageDescriptors);
}

void MangaChapterDownloadManager::downloadMangaChapters(QSharedPointer<MangaInfo> mangaInfo, int fromChapter,
                                                        int toChapterInclusive)
{
    // a new start, the failures of the earlier downloads aren't retried anymore
    if (!running && downloadJobs.isEmpty())
        failedChapters.clear();

    QStringList chapterUrls;
    for (int c = qMax(0, fromChapter); c <= toChapterInclusive && c < mangaInfo->chapters.count(); c++)
        chapterUrls.append(mangaInfo->chapters[c].chapterUrl);
    downloadJobs.append({mangaInfo, chapterUrls});
    processNextJob();
}
