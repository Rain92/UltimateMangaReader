#ifndef MANGACHAPTERDOWNLOADMANAGER_H
#define MANGACHAPTERDOWNLOADMANAGER_H

#include <QQueue>

#include "downloadimagedescriptor.h"
#include "downloadqueue.h"
#include "mangainfo.h"
#include "networkmanager.h"
#include "thirdparty/result.h"

struct MangaChapterRange
{
public:
    MangaChapterRange() = default;
    MangaChapterRange(QSharedPointer<MangaInfo> mangaInfo, const QList<int> &chapters)
        : mangaInfo(mangaInfo), chapters(chapters){};

    QSharedPointer<MangaInfo> mangaInfo;
    QList<int> chapters;
};

class MangaChapterDownloadManager : public QObject
{
    Q_OBJECT
public:
    explicit MangaChapterDownloadManager(NetworkManager *networkManager, QObject *parent = nullptr);

    void cancelDownloads();
    bool isRunning() const { return running; }
    void downloadMangaChapters(QSharedPointer<MangaInfo> mangaInfo, int fromChapter, int toChapterInclusive);
    // downloads the chapters again that didn't complete since the downloads were started
    void retryFailedDownloads();

signals:
    void downloadStart(const QString &mangaTitle);
    void downloadPagelistProgress(int completed, int total, int errors);
    void downloadPagesProgress(int completed, int total, int errors);
    void downloadImagesProgress(int completed, int total, int errors);
    void error(const QString &error);
    // failedChapters: of all the jobs since the downloads were started, retryFailedDownloads() downloads them again
    void downloadCompleted(int failedChapters);

private:
    // incremented by a cancel
    int generation;
    bool running;

    QQueue<MangaChapterRange> downloadJobs;

    // the running job: its generation, the chapters that failed and the images to check when its downloads are done
    int currentGeneration;
    QSharedPointer<MangaInfo> currentManga;
    QSet<int> currentFailed;
    QList<QPair<int, QString>> currentImagePaths;
    // by chapter url, the indexes can change until a retry
    QList<QPair<QSharedPointer<MangaInfo>, QStringList>> failedChapters;

    NetworkManager *networkManager;
    DownloadQueue downloadQueue;

    void processNextJob();
    void downloadQueueJobsCompleted();
    void recordFailedChapters();
    int failedChapterCount() const;
};

#endif  // MANGACHAPTERDOWNLOADMANAGER_H
