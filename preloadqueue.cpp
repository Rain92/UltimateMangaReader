#include "preloadqueue.h"
#include "configs.h"

PreloadQueue::PreloadQueue(QObject *parent, AbstractMangaSource *source):
    QObject(parent),
    source(source),
    queue(),
    job(nullptr),
    running(false)
{

}

void PreloadQueue::addJob(DownloadImageInfo info)
{
//    qDebug() << info.page;
    queue.enqueue(info);
//    if(queue.count() == 1)
    if (!running)
    {
        running = true;
        processNext();
    }
}

void PreloadQueue::clearQuene()
{
    queue.clear();
}


DownloadScaledImageJob *PreloadQueue::PreloadQueue::currentJob()
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

    DownloadImageInfo info = queue.dequeue();

//        job->deleteLater();
//        job = nullptr;



    job = static_cast<DownloadScaledImageJob *>(source->downloadImage(info.imagelink, info.title, info.chapter, info.page));
    if (!job->isCompleted)
    {
        QObject::connect(job, SIGNAL(completed()), this, SLOT(sendComletedSignal()));
        QObject::connect(job, SIGNAL(completed()), this, SLOT(processNext()));
    }
    else
    {
        sendComletedSignal();
        processNext();
    }
}

void PreloadQueue::sendComletedSignal()
{
//    qDebug() << "preload completed";
    QObject *send = sender();
    if (send == nullptr)
    {
        emit completedDownload(job->file.fileName());
    }
    else
    {
        DownloadScaledImageJob *job = static_cast<DownloadScaledImageJob *>(sender());
        emit completedDownload(job->file.fileName());
    }
}
