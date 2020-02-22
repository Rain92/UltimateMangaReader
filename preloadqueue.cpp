#include "preloadqueue.h"

#include "configs.h"

PreloadQueue::PreloadQueue(QObject *parent, AbstractMangaSource *source)
    : QObject(parent),
      source(source),
      queue(),
      job(nullptr),
      resettimer(new QTimer(this)),
      running(false)
{
    resettimer->setSingleShot(true);
    connect(resettimer, SIGNAL(timeout()), this, SLOT(resetQueue()));
}

void PreloadQueue::resetQueue() { running = false; }

void PreloadQueue::addJob(DownloadImageInfo info)
{
    if (info.imagelink == "") return;

    //    qDebug() << info.page;
    queue.enqueue(info);
    //    if(queue.count() == 1)
    if (!running)
    {
        running = true;
        processNext();
    }
}

void PreloadQueue::clearQuene() { queue.clear(); }

QSharedPointer<DownloadFileJob> PreloadQueue::PreloadQueue::currentJob()
{
    if (running) return job;

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
    resettimer->start(4000);

    DownloadImageInfo info = queue.dequeue();

    job = source->downloadImage(info.imagelink, info.title, info.chapter,
                                info.page);

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
