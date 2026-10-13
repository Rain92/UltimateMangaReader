#ifndef DOWNLOADQUEUE_H
#define DOWNLOADQUEUE_H

#include "networkmanager.h"

enum DownloadType
{
    DownloadTypeString,
    DownloadTypeScaledImage
};

struct FileDownloadDescriptor
{
    FileDownloadDescriptor(const QString &url, const QString &path, const HttpHeaders &headers = {})
        : url(url), path(path), headers(headers)
    {
    }
    QString url;
    QString path;
    HttpHeaders headers;
};

// Runs downloads, a few at a time.
class DownloadQueue : public QObject
{
    Q_OBJECT
public:
    // Downloads texts, lambda gets the ones that completed. headersFor: the headers of the requests.
    explicit DownloadQueue(NetworkManager *networkManager, const QList<QString> &urls, int parallelDownloads,
                           std::function<void(QSharedPointer<DownloadStringJob>)> lambda, bool cancelOnError,
                           int individualTimeout = 30000,
                           std::function<HttpHeaders(const QString &)> headersFor = nullptr);

    // Downloads pages (see NetworkManager::downloadAsScaledImage()).
    explicit DownloadQueue(NetworkManager *networkManager, const QList<FileDownloadDescriptor> &urlAndPaths,
                           int parallelDownloads, bool cancelOnError);

    int totalJobs;
    // finished (completed or failed)
    int completed;
    int errors;
    QString lastErrorMessage;
    bool cancelOnError;

    void start();
    void appendDownload(const FileDownloadDescriptor &urlAndPaths);
    void appendDownloads(const QList<FileDownloadDescriptor> &urlAndPaths);
    // Drops the downloads that didn't start yet.
    void clearQueue();
    void resetJobCount();
    // Waits until all downloads finished. Returns whether all of them completed.
    bool awaitCompletion();
    // the queue stops when it is set (checked when a download finishes)
    void setCancellationToken(bool *token);

signals:
    void singleDownloadCompleted(const QString &url, const QString &path);
    void singleDownloadFailed(const QString &url, const QString &path, const QString &error);
    // when the last download finished (not for a queue without downloads)
    void allCompleted();
    void progress(int completed, int total, int failed);

private:
    NetworkManager *networkManager;

    int runningJobs;

    DownloadType type;
    int parallelDownloads;
    QQueue<FileDownloadDescriptor> jobDescriptorQueue;
    std::function<void(QSharedPointer<DownloadStringJob>)> lambda;
    int individualTimeout;
    std::function<HttpHeaders(const QString &)> headersFor;
    bool *cancellationToken;
    bool cancelled;

    void startSingle();
    void downloadFinished(QSharedPointer<DownloadJobBase> job, bool success);
};

#endif  // DOWNLOADQUEUE_H
