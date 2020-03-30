#ifndef DOWNLOADQUEUE_H
#define DOWNLOADQUEUE_H

#include "downloadmanager.h"

class DownloadQueue : public QObject
{
    Q_OBJECT
public:
    explicit DownloadQueue(
        DownloadManager *downloadmanager, const QList<QString> &urlQueue,
        int parallelDownloads,
        std::function<void(QSharedPointer<DownloadStringJob>)> lambda);

    void start();
    void clearQuene();
    int completed;
    int errors;

signals:
    void singleDownloadCompleted();
    void singleDownloadFailed();
    void allCompleted();

private:
    DownloadManager *downloadmanager;

    int parallelDownloads;
    QQueue<QString> urlQueue;
    int totalJobs;
    std::function<void(QSharedPointer<DownloadStringJob>)> lambda;

    void startSingle();
    void downloadFinished(QSharedPointer<DownloadStringJob> job, bool success);
};

#endif  // DOWNLOADQUEUE_H
