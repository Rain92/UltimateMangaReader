#ifndef DOWNLOADQUEUE_H
#define DOWNLOADQUEUE_H

#include "downloadmanager.h"

class DownloadQueue : public QObject
{
    Q_OBJECT
public:
    explicit DownloadQueue(DownloadManager *downloadmanager, const QList<QString> &urlQueue,
                           int parallelDownloads,
                           std::function<void(QSharedPointer<DownloadStringJob>)> lambda, bool cancelOnError,
                           int individualTimeout = 16000);

    int totalJobs;
    int completed;
    int errors;
    QString lastErrorMessage;
    bool cancelOnError;

    void start();
    void clearQuene();
    bool awaitCompletion();
    void setCancellationToken(bool *token);

signals:
    void singleDownloadCompleted();
    void singleDownloadFailed();
    void allCompleted();

private:
    DownloadManager *downloadmanager;

    int parallelDownloads;
    QQueue<QString> urlQueue;
    std::function<void(QSharedPointer<DownloadStringJob>)> lambda;
    int individualTimeout;
    bool *cancellationToken;

    void startSingle();
    void downloadFinished(QSharedPointer<DownloadStringJob> job, bool success);
};

#endif  // DOWNLOADQUEUE_H
