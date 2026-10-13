#ifndef MANGACHAPTERDOWNLOADMANAGER_H
#define MANGACHAPTERDOWNLOADMANAGER_H

#include <QQueue>

#include "downloadimagedescriptor.h"
#include "downloadqueue.h"
#include "mangainfo.h"
#include "networkmanager.h"
#include "thirdparty/result.h"

// The chapters to download, by url: an update of the chapters can move them until they are downloaded.
struct MangaChapterRange
{
    QSharedPointer<MangaInfo> mangaInfo;
    QStringList chapterUrls;
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

    // the running job: its generation, the chapters that failed and the images to check when its downloads are
    // done (all by chapter url)
    int currentGeneration;
    QSharedPointer<MangaInfo> currentManga;
    QSet<QString> currentFailed;
    QList<QPair<QString, QString>> currentImagePaths;
    QList<MangaChapterRange> failedChapters;

    NetworkManager *networkManager;
    DownloadQueue downloadQueue;

    void processNextJob();
    void downloadQueueJobsCompleted();
    void recordFailedChapters();
    int failedChapterCount() const;
};

#endif  // MANGACHAPTERDOWNLOADMANAGER_H
