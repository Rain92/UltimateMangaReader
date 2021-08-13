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
    MangaChapterRange(QSharedPointer<MangaInfo> mangaInfo, int fromChapter, int toChapterInclusive)
        : mangaInfo(mangaInfo), fromChapter(fromChapter), toChapterInclusive(toChapterInclusive){};

    QSharedPointer<MangaInfo> mangaInfo;
    int fromChapter;
    int toChapterInclusive;
};

class MangaChapterDownloadManager : public QObject
{
    Q_OBJECT
public:
    explicit MangaChapterDownloadManager(NetworkManager *networkManager, QObject *parent = nullptr);

    void cancelDownloads();
    void downloadMangaChapters(QSharedPointer<MangaInfo> mangaInfo, int fromChapter, int toChapterInclusive);

signals:
    void downloadStart(const QString &mangaTitle);
    void downloadPagelistProgress(int completed, int total);
    void downloadPagesProgress(int completed, int total, int errors);
    void downloadImagesProgress(int completed, int total, int errors);
    void error(const QString &error);
    void downloadCompleted();

private:
    bool cancelled;
    bool running;

    QQueue<MangaChapterRange> downloadJobs;

    NetworkManager *networkManager;
    DownloadQueue downloadQueue;

    void processNextJob();
    void downloadQueueJobsCompleted();
};

#endif  // MANGACHAPTERDOWNLOADMANAGER_H
