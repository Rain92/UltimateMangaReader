#include "downloadqueue.h"

#include "utils.h"

// Download as strings
DownloadQueue::DownloadQueue(NetworkManager* networkManager, const QList<QString>& urls,
                             int parallelDownloads,
                             std::function<void(QSharedPointer<DownloadStringJob>)> lambda,
                             bool cancelOnError, int individualTimeout)
    : QObject(),
      completed(0),
      errors(0),
      lastErrorMessage(""),
      cancelOnError(cancelOnError),
      networkManager(networkManager),
      runningJobs(0),
      type(DownloadTypeString),
      parallelDownloads(parallelDownloads),
      jobDescriptorQueue(),
      lambda(lambda),
      individualTimeout(individualTimeout),
      cancellationToken(nullptr)
{
    totalJobs = urls.count();

    std::transform(urls.begin(), urls.end(), std::back_inserter(jobDescriptorQueue),
                   [](const QString& s) { return FileDownloadDescriptor(s, ""); });
}

// Download as images
DownloadQueue::DownloadQueue(NetworkManager* networkManager, const QList<FileDownloadDescriptor>& urlAndPaths,
                             int parallelDownloads, bool cancelOnError)
    : QObject(),
      completed(0),
      errors(0),
      lastErrorMessage(""),
      cancelOnError(cancelOnError),
      networkManager(networkManager),
      type(DownloadTypeScaledImage),
      parallelDownloads(parallelDownloads),
      jobDescriptorQueue(),
      lambda(nullptr),
      individualTimeout(-1),
      cancellationToken(nullptr)
{
    totalJobs = urlAndPaths.count();

    this->jobDescriptorQueue.append(urlAndPaths);
}

void DownloadQueue::start()
{
    while (!jobDescriptorQueue.empty() && runningJobs < parallelDownloads)
        startSingle();
}

void DownloadQueue::startSingle()
{
    if (jobDescriptorQueue.empty())
        return;

    runningJobs++;

    auto descriptor = jobDescriptorQueue.dequeue();

    QSharedPointer<DownloadJobBase> job;

    if (type == DownloadTypeString)
        job = networkManager->downloadAsString(descriptor.url, individualTimeout);
    else  // if (type == DownloadTypeScaledImage)
        job = networkManager->downloadAsScaledImage(descriptor.url, descriptor.path);

    if (!job->isCompleted)
    {
        QObject::connect(job.get(), &DownloadJobBase::completed, this,
                         [this, job]() { downloadFinished(job, true); });
        QObject::connect(job.get(), &DownloadJobBase::downloadError, this,
                         [this, job]() { downloadFinished(job, false); });
    }
    else
    {
        downloadFinished(job, job->errorString == "");
    }
}

void DownloadQueue::downloadFinished(QSharedPointer<DownloadJobBase> job, bool success)
{
    if (success)
    {
        if (lambda != nullptr)
        {
            auto jobS = job.dynamicCast<DownloadStringJob>();
            if (jobS != nullptr)
                lambda(jobS);
            else
                qDebug() << "Job lambda cast failed";
        }
        auto jobF = job.dynamicCast<DownloadFileJob>();
        emit singleDownloadCompleted(job->originalUrl, jobF ? jobF->filepath : "");
    }
    else
    {
        errors++;
        lastErrorMessage = job->errorString;
        if (cancelOnError)
            clearQuene();
        emit singleDownloadFailed(job->originalUrl, job->errorString);
    }

    if (cancellationToken != nullptr && *cancellationToken)
    {
        errors++;
        lastErrorMessage = "Download cancelled";
        clearQuene();
    }

    job->disconnect();

    completed++;
    runningJobs--;
    emit progress(completed, totalJobs, errors);

    if (completed == totalJobs)
    {
        emit allCompleted();
    }
    else
        startSingle();
}

void DownloadQueue::appendDownload(const FileDownloadDescriptor& urlAndPaths)
{
    totalJobs++;

    this->jobDescriptorQueue.append(urlAndPaths);
    start();
}

void DownloadQueue::appendDownloads(const QList<FileDownloadDescriptor>& urlAndPaths)
{
    totalJobs += urlAndPaths.count();

    this->jobDescriptorQueue.append(urlAndPaths);
    start();
}

void DownloadQueue::clearQuene()
{
    int c = jobDescriptorQueue.count();
    jobDescriptorQueue.clear();
    totalJobs -= c;
}

void DownloadQueue::resetJobCount()
{
    errors = 0;
    totalJobs = 0;
    completed = 0;
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
