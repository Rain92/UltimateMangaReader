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
    FileDownloadDescriptor(const QString &url, const QString &path) : url(url), path(path) {}
    QString url;
    QString path;
};

class DownloadQueue : public QObject
{
    Q_OBJECT
public:
    explicit DownloadQueue(NetworkManager *downloadmanager, const QList<QString> &urls, int parallelDownloads,
                           std::function<void(QSharedPointer<DownloadStringJob>)> lambda, bool cancelOnError,
                           int individualTimeout = 20000);

    explicit DownloadQueue(NetworkManager *downloadmanager, const QList<FileDownloadDescriptor> &urlAndPaths,
                           int parallelDownloads, bool cancelOnError);

    int totalJobs;
    int completed;
    int errors;
    QString lastErrorMessage;
    bool cancelOnError;

    void start();
    void appendDownload(const FileDownloadDescriptor &urlAndPaths);
    void appendDownloads(const QList<FileDownloadDescriptor> &urlAndPaths);
    void clearQuene();
    void resetJobCount();
    bool awaitCompletion();
    void setCancellationToken(bool *token);

signals:
    void singleDownloadCompleted(const QString &url, const QString &path);
    void singleDownloadFailed(const QString &url, const QString &error);
    void allCompleted();
    void progress(int completed, int total, int failed);

private:
    NetworkManager *networkManager;

    QAtomicInt runningJobs;

    DownloadType type;
    int parallelDownloads;
    QQueue<FileDownloadDescriptor> jobDescriptorQueue;
    std::function<void(QSharedPointer<DownloadStringJob>)> lambda;
    int individualTimeout;
    bool *cancellationToken;

    void startSingle();
    void downloadFinished(QSharedPointer<DownloadJobBase> job, bool success);
};

#endif  // DOWNLOADQUEUE_H
