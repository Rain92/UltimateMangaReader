#include "downloadqueue.h"

#include "utils.h"

DownloadQueue::DownloadQueue(DownloadManager* downloadmanager, const QList<QString>& urls,
                             int parallelDownloads,
                             std::function<void(QSharedPointer<DownloadStringJob>)> lambda,
                             bool cancelOnError, int individualTimeout)
    : QObject(),
      completed(0),
      errors(0),
      lastErrorMessage(""),
      cancelOnError(cancelOnError),
      downloadmanager(downloadmanager),
      parallelDownloads(parallelDownloads),
      urlQueue(),
      lambda(lambda),
      individualTimeout(individualTimeout),
      cancellationToken(nullptr)
{
    totalJobs = urls.count();
    this->urlQueue.append(urls);
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

    auto job = downloadmanager->downloadAsString(url, individualTimeout);

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

void DownloadQueue::downloadFinished(QSharedPointer<DownloadStringJob> job, bool success)
{
    completed++;
    if (success)
    {
        lambda(job);
        emit singleDownloadCompleted();
    }
    else
    {
        errors++;
        lastErrorMessage = job->errorString;
        clearQuene();
        emit singleDownloadFailed();
    }

    if (cancellationToken != nullptr && *cancellationToken)
    {
        errors++;
        lastErrorMessage = "Download cancelled";
        clearQuene();
    }

    job->disconnect();

    if (completed == totalJobs)
    {
        emit allCompleted();
    }
    else
        startSingle();
}

void DownloadQueue::clearQuene()
{
    int c = urlQueue.count();
    urlQueue.clear();
    totalJobs -= c;
}
bool DownloadQueue::awaitCompletion()
{
    awaitSignal(this, {SIGNAL(allCompleted())}, -1);

    return errors == 0;
}
void DownloadQueue::setCancellationToken(bool* token)
{
    cancellationToken = token;
}
