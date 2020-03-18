#include "preloadqueue.h"

#include "defines.h"

PreloadQueue::PreloadQueue(AbstractMangaSource* source)
    : QObject(),
      source(source),
      queue(),
      job(nullptr),
      resettimer(),
      running(false)
{
    resettimer.setSingleShot(true);
    connect(&resettimer, SIGNAL(timeout()), this, SLOT(resetQueue()));
}

void PreloadQueue::resetQueue() { running = false; }

void PreloadQueue::addJob(const DownloadImageDescriptor& info)
{
    if (info.imagelink == "")
        return;

    queue.enqueue(info);
    if (!running)
    {
        running = true;
        processNext();
    }
}

void PreloadQueue::clearQuene() { queue.clear(); }

QSharedPointer<DownloadFileJob> PreloadQueue::PreloadQueue::currentJob()
{
    if (running)
        return job;

    return nullptr;
}

void PreloadQueue::processNext()
{
    if (queue.empty())
    {
        running = false;
        return;
    }
    running = true;
    resettimer.start(4000);

    DownloadImageDescriptor info = queue.dequeue();

    job = source->downloadImage(info);

    if (!job->isCompleted)
    {
        QObject::connect(job.get(), SIGNAL(completed()), this,
                         SLOT(sendComletedSignal()));
        QObject::connect(job.get(), SIGNAL(downloadError()), this,
                         SLOT(processNext()));
    }
    else
    {
        sendComletedSignal();
    }
}

void PreloadQueue::sendComletedSignal()
{
    emit completedDownload(job->filepath);
    processNext();
}
