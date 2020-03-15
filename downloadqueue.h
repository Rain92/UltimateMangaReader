#ifndef DOWNLOADQUEUE_H
#define DOWNLOADQUEUE_H

#include "downloadmanager.h"

class DownloadQueue : public QObject
{
    Q_OBJECT
public:
    explicit DownloadQueue(
        DownloadManager *downloadmanager, const QList<QString> &urls,
        int parallelDownloads,
        std::function<void(QSharedPointer<DownloadStringJob>)> lambda);

    void start();

signals:
    void singleDownloadCompleted(QSharedPointer<DownloadJobBase> job);
    void singleDownloadFailed(QSharedPointer<DownloadJobBase> job);
    void allCompleted();

private:
    DownloadManager *downloadmanager;

    QAtomicInt completed;
    QAtomicInt errors;
    int parallelDownloads;
    QQueue<QString> urls;
    int totaljobs;
    std::function<void(QSharedPointer<DownloadStringJob>)> lambda;
    QList<QSharedPointer<DownloadStringJob>> jobs;

    void startSingle();
    void downloadFinished(QSharedPointer<DownloadStringJob> job, bool success);
};

#endif  // DOWNLOADQUEUE_H
