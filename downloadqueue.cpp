#include "downloadqueue.h"

DownloadQueue::DownloadQueue(
    DownloadManager* downloadmanager, const QList<QString>& urls,
    int parallelDownloads,
    std::function<void(QSharedPointer<DownloadStringJob>)> lambda)
    : QObject(),
      completed(0),
      errors(0),
      downloadmanager(downloadmanager),
      parallelDownloads(parallelDownloads),
      urlQueue(),
      lambda(lambda)
{
    this->urlQueue.append(urls);
    totalJobs = urls.size();
}

void DownloadQueue::start()
{
    for (int i = 0; i < parallelDownloads; i++)
        startSingle();
}

void DownloadQueue::startSingle()
{
    if (urlQueue.empty())
        return;

    auto url = urlQueue.dequeue();

    auto job = downloadmanager->downloadAsString(url, 8000);

    if (!job->isCompleted)
    {
        QObject::connect(job.get(), &DownloadJobBase::completed,
                         [this, job]() { downloadFinished(job, true); });
        QObject::connect(job.get(), &DownloadJobBase::downloadError,
                         [this, job]() { downloadFinished(job, false); });
    }
    else
    {
        downloadFinished(job, job->errorString == "");
    }
}

void DownloadQueue::downloadFinished(QSharedPointer<DownloadStringJob> job,
                                     bool success)
{
    completed++;
    if (success)
    {
        lambda(job);
        emit singleDownloadCompleted();
    }
    else
    {
        emit singleDownloadFailed();
        errors++;
    }

    job->disconnect();

    if (completed == totalJobs)
    {
        emit allCompleted();
    }
    else
        startSingle();
}

void DownloadQueue::clearQuene() { urlQueue.clear(); }
