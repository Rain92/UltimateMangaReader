#ifndef PRELOADQUEUE_H
#define PRELOADQUEUE_H

#include <QObject>
#include <QQueue>

#include "abstractmangasource.h"
#include "downloadimageandrescalejob.h"
#include "downloadimagedescriptor.h"

struct FileDownloadDescriptor
{
    FileDownloadDescriptor(const QString &url, const QString &path)
        : url(url), path(path)
    {
    }
    QString url;
    QString path;
};

class PreloadQueue : public QObject
{
    Q_OBJECT

public:
    explicit PreloadQueue(DownloadManager *downloadManager);

    void addJob(const FileDownloadDescriptor &job);
    void clearQuene();

    QSharedPointer<DownloadFileJob> currentJob();

signals:
    void completedDownload(const QString &path);

public slots:
    void processNext();

private slots:
    void sendComletedSignal(QSharedPointer<DownloadFileJob> job);
    void resetQueue();

private:
    DownloadManager *downloadManager;
    QQueue<FileDownloadDescriptor> queue;
    bool running;
};

#endif  // PRELOADQUEUE_H
