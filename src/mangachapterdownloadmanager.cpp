#include "mangachapterdownloadmanager.h"

MangaChapterDownloadManager::MangaChapterDownloadManager(NetworkManager *networkManager, QObject *parent)
    : QObject(parent),
      generation(0),
      running(false),
      downloadJobs(),
      currentGeneration(0),
      currentFailed(),
      currentImagePaths(),
      failedChapters(),
      networkManager(networkManager),
      downloadQueue(networkManager, {}, 2, false)
{
    connect(&downloadQueue, &DownloadQueue::progress,
            [this](int c, int t, int e) { emit downloadImagesProgress(c, t, e); });
    connect(&downloadQueue, &DownloadQueue::singleDownloadFailed,
            [this](const QString &, const QString &errorstr) {
                emit error("Couldn't download image: " + errorstr);
            });
    connect(&downloadQueue, &DownloadQueue::allCompleted, this,
            &MangaChapterDownloadManager::downloadQueueJobsCompleted);
}

void MangaChapterDownloadManager::cancelDownloads()
{
    // A running processNextJob() (it waits for page lists in event loops) notices it and stops, running stays
    // set until then, or until the image downloads that already run are done.
    generation++;
    downloadJobs.clear();
    downloadQueue.clearQuene();
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
    for (const auto &[chapter, path] : std::as_const(currentImagePaths))
        if (!QFile::exists(path))
            currentFailed.insert(chapter);

    QStringList urls;
    for (int c : std::as_const(currentFailed))
        if (c < currentManga->chapters.count())
            urls.append(currentManga->chapters[c].chapterUrl);
    if (!urls.isEmpty())
        failedChapters.append({currentManga, urls});

    currentFailed.clear();
    currentImagePaths.clear();
}

int MangaChapterDownloadManager::failedChapterCount() const
{
    int count = 0;
    for (const auto &failed : failedChapters)
        count += failed.second.count();
    return count;
}

void MangaChapterDownloadManager::retryFailedDownloads()
{
    auto failed = failedChapters;
    failedChapters.clear();

    for (const auto &[mangaInfo, urls] : std::as_const(failed))
    {
        QList<int> chapters;
        for (int c = 0; c < mangaInfo->chapters.count(); c++)
            if (urls.contains(mangaInfo->chapters[c].chapterUrl))
                chapters.append(c);
        if (!chapters.isEmpty())
            downloadJobs.append(MangaChapterRange(mangaInfo, chapters));
    }

    // the chapters can be gone after an update
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

    const int jobGeneration = generation;
    auto cancelled = [this, jobGeneration]() { return generation != jobGeneration; };

    auto job = downloadJobs.dequeue();
    auto mangaInfo = job.mangaInfo;
    const auto &chapters = job.chapters;
    currentGeneration = jobGeneration;
    currentManga = mangaInfo;
    currentFailed.clear();
    currentImagePaths.clear();

    emit downloadStart(mangaInfo->title);

    int pagelistErrors = 0;
    for (int i = 0; i < chapters.count() && !cancelled(); i++)
    {
        int c = chapters[i];
        // 3 trys
        for (int t = 0; t < 3 && !cancelled(); t++)
        {
            auto res = mangaInfo->mangaSource->updatePageList(mangaInfo, c);

            if (res.isOk())
            {
                emit downloadPagelistProgress(i + 1, chapters.count(), pagelistErrors);
                break;
            }
            else if (t == 2)
            {
                currentFailed.insert(c);
                pagelistErrors++;
                emit downloadPagelistProgress(i + 1, chapters.count(), pagelistErrors);
                // skip chapters that can't be loaded (e.g. removed from the source) but keep going
                emit error(
                    QString("Couldn't download pagelist for chapter %1: %2").arg(c + 1).arg(res.unwrapErr()));
            }
        }
    }
    mangaInfo->serialize();
    int pagesErrors = 0;
    QList<FileDownloadDescriptor> imageDescriptors;
    for (int i = 0; i < chapters.count() && !cancelled(); i++)
    {
        int c = chapters[i];
        if (currentFailed.contains(c) || c >= mangaInfo->chapters.count() || !mangaInfo->chapters[c].pagesLoaded)
            continue;
        for (int p = 0; p < mangaInfo->chapters[c].pageUrlList.count() && !cancelled(); p++)
        {
            if (mangaInfo->chapters[c].imageUrlList[p] == "")
            {
                auto res = mangaInfo->mangaSource->getImageUrl(mangaInfo->chapters.at(c).pageUrlList.at(p));
                if (!res.isOk())
                {
                    pagesErrors++;
                    currentFailed.insert(c);
                    emit error(QString("Couldn't download page %1 of chapter %2: %3")
                                   .arg(p + 1)
                                   .arg(c + 1)
                                   .arg(res.unwrapErr()));
                }
                else
                {
                    mangaInfo->chapters[c].imageUrlList[p] = res.unwrap();
                }
            }

            auto &imageUrl = mangaInfo->chapters[c].imageUrlList[p];

            if (imageUrl != "")
            {
                if (imageUrl != mangaInfo->chapters.at(c).pageUrlList.at(p))
                    emit downloadPagesProgress(i + 1, chapters.count(), pagesErrors);

                DownloadImageDescriptor imageinfo(imageUrl, mangaInfo->title, c, p);
                auto path = mangaInfo->mangaSource->getImagePath(imageinfo);

                imageDescriptors.append(FileDownloadDescriptor(imageUrl, path));
                currentImagePaths.append({c, path});
            }
        }
    }
    mangaInfo->serialize();

    if (cancelled())
    {
        running = false;
        processNextJob();
        return;
    }

    if (imageDescriptors.isEmpty())
    {
        // the queue never emits allCompleted without jobs
        downloadQueueJobsCompleted();
        return;
    }

    downloadQueue.appendDownloads(imageDescriptors);
}

void MangaChapterDownloadManager::downloadMangaChapters(QSharedPointer<MangaInfo> mangaInfo, int fromChapter,
                                                        int toChapterInclusive)
{
    // a new start, the failures of the earlier downloads aren't retried anymore
    if (!running && downloadJobs.isEmpty())
        failedChapters.clear();

    QList<int> chapters;
    for (int c = fromChapter; c <= toChapterInclusive; c++)
        chapters.append(c);
    downloadJobs.append(MangaChapterRange(mangaInfo, chapters));
    processNextJob();
}
