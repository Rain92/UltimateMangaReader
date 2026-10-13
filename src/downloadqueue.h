#pragma once

#include "networkmanager.h"
#include "task.h"

enum DownloadType
{
    DownloadTypeString,
    DownloadTypeScaledImage
};

// See MangaRepository::imageDownload().
struct FileDownloadDescriptor
{
    FileDownloadDescriptor(const ImageLink &image, const QString &path) : image(image), path(path) {}
    ImageLink image;
    QString path;
};

// Runs downloads, a few at a time.
class DownloadQueue : public QObject
{
    Q_OBJECT
public:
    // Downloads texts, lambda gets the ones that completed. headersFor: the headers of the requests.
    explicit DownloadQueue(NetworkManager *networkManager, const QList<QString> &urls, int parallelDownloads,
                           std::function<void(QSharedPointer<DownloadBufferJob>)> lambda, bool cancelOnError,
                           int individualTimeout = 30000,
                           std::function<HttpHeaders(const QString &)> headersFor = nullptr);

    // Downloads pages (see NetworkManager::downloadAsScaledImage()).
    explicit DownloadQueue(NetworkManager *networkManager, const QList<FileDownloadDescriptor> &urlAndPaths,
                           int parallelDownloads, bool cancelOnError);

    int totalJobs;
    // finished (completed or failed)
    int completed;
    int errors;
    Error lastError;

    bool cancelOnError;

    void start();
    void appendDownload(const FileDownloadDescriptor &urlAndPaths);
    void appendDownloads(const QList<FileDownloadDescriptor> &urlAndPaths);
    // Drops the downloads that didn't start yet.
    void clearQueue();
    void resetJobCount();
    // Waits until all downloads finished. Returns whether all of them completed.
    bool awaitCompletion();
    // Downloads are running or waiting to start.
    bool isRunning() const { return runningJobs > 0 || !jobDescriptorQueue.isEmpty(); }
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
    std::function<void(QSharedPointer<DownloadBufferJob>)> lambda;
    int individualTimeout;
    std::function<HttpHeaders(const QString &)> headersFor;
    bool *cancellationToken;
    bool cancelled;

    void startSingle();
    void downloadFinished(QSharedPointer<DownloadJobBase> job, bool success);
};

// co_await queue: waits until its downloads finished, true if all of them completed.
struct QueueAwaiter
{
    DownloadQueue *queue;
    std::shared_ptr<void> hold;

    bool await_ready() const { return !queue->isRunning(); }
    void await_suspend(std::coroutine_handle<> handle)
    {
        hold = DownloadJobBase::holdActivity();
        resumeOnSignal(QList<DownloadQueue *>{queue}, handle, 0, &DownloadQueue::allCompleted);

    }
    bool await_resume()
    {
        hold.reset();
        return queue->errors == 0;
    }
};

inline QueueAwaiter operator co_await(DownloadQueue &queue)
{
    return {&queue, {}};
}

