#include "preloadqueue.h"

PreloadQueue::PreloadQueue(DownloadManager* downloadManager)
    : QObject(), downloadManager(downloadManager), queue(), running(false)
{
}

void PreloadQueue::resetQueue() { running = false; }

void PreloadQueue::addJob(const FileDownloadDescriptor& descriptor)
{
    if (descriptor.url == "")
        return;

    queue.enqueue(descriptor);
    if (!running)
    {
        running = true;
        processNext();
    }
}

void PreloadQueue::clearQuene() { queue.clear(); }

void PreloadQueue::processNext()
{
    if (queue.empty())
    {
        running = false;
        return;
    }
    running = true;

    auto descriptor = queue.dequeue();

    auto job =
        downloadManager->downloadAsScaledImage(descriptor.url, descriptor.path);

    if (!job->isCompleted)
    {
        QObject::connect(job.get(), &DownloadFileJob::completed,
                         [job, this]() { sendComletedSignal(job); });
        QObject::connect(job.get(), &DownloadFileJob::downloadError, this,
                         &PreloadQueue::processNext);
    }
    else
    {
        sendComletedSignal(job);
    }
}

void PreloadQueue::sendComletedSignal(QSharedPointer<DownloadFileJob> job)
{
    emit completedDownload(job->filepath);
    job->disconnect();
    processNext();
}
