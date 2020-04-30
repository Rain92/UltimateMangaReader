#include "mangachapterdownloadmanager.h"

MangaChapterDownloadManager::MangaChapterDownloadManager(NetworkManager *networkManager, QObject *parent)
    : QObject(parent),
      cancelled(false),
      running(false),
      downloadJobs(),
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
    cancelled = true;
    downloadQueue.clearQuene();
}

void MangaChapterDownloadManager::downloadQueueJobsCompleted()
{
    emit downloadCompleted();
    downloadQueue.resetJobCount();
    running = false;
    processNextJob();
}

void MangaChapterDownloadManager::processNextJob()
{
    if (cancelled || downloadJobs.empty() || running)
        return;
    running = true;

    auto job = downloadJobs.dequeue();
    auto mangaInfo = job.mangaInfo;
    auto fromChapter = job.fromChapter;
    auto toChapterInclusive = job.toChapterInclusive;

    emit downloadStart(mangaInfo->title);

    for (int c = fromChapter; c <= toChapterInclusive && !cancelled; c++)
    {
        // 3 trys
        for (int i = 0; i < 3; i++)
        {
            auto res = mangaInfo->mangaSource->updatePageList(mangaInfo, c);

            if (res.isOk())
            {
                emit downloadPagelistProgress(c + 1 - fromChapter, toChapterInclusive + 1 - fromChapter);
                break;
            }
            else if (i > 2)
            {
                cancelled = true;
                emit error(
                    QString("Couldn't download pagelst for chapter %1: %2").arg(c + 1).arg(res.unwrapErr()));
                return;
            }
        }
    }
    mangaInfo->serialize();
    int pagesErrors = 0;
    QList<FileDownloadDescriptor> imageDescriptors;
    for (int c = fromChapter; c <= toChapterInclusive && !cancelled; c++)
    {
        if (!mangaInfo->chapters[c].pagesLoaded)
            continue;
        for (int p = 0; p < mangaInfo->chapters[c].pageUrlList.count() && !cancelled; p++)
        {
            if (mangaInfo->chapters[c].imageUrlList[p] == "")
            {
                auto res = mangaInfo->mangaSource->getImageUrl(mangaInfo->chapters.at(c).pageUrlList.at(p));
                if (!res.isOk())
                {
                    pagesErrors++;
                    emit error(QString("Stoping Download. Couldn't download page %1 of chapter %2: %3")
                                   .arg(c)
                                   .arg(p)
                                   .arg(res.unwrapErr()));
                }
                mangaInfo->chapters[c].imageUrlList[p] = res.unwrap();
            }

            auto &imageUrl = mangaInfo->chapters[c].imageUrlList[p];

            if (imageUrl != "")
            {
                if (imageUrl != mangaInfo->chapters.at(c).pageUrlList.at(p))
                    emit downloadPagesProgress(c + 1 - fromChapter, toChapterInclusive + 1 - fromChapter,
                                               pagesErrors);

                DownloadImageDescriptor imageinfo(imageUrl, mangaInfo->title, c, p);
                auto path = mangaInfo->mangaSource->getImagePath(imageinfo);

                imageDescriptors.append(FileDownloadDescriptor(imageUrl, path));
            }
        }
    }
    mangaInfo->serialize();

    downloadQueue.appendDownloads(imageDescriptors);
}

void MangaChapterDownloadManager::downloadMangaChapters(QSharedPointer<MangaInfo> mangaInfo, int fromChapter,
                                                        int toChapterInclusive)
{
    downloadJobs.append(MangaChapterRange(mangaInfo, fromChapter, toChapterInclusive));
    cancelled = false;
    processNextJob();
}
