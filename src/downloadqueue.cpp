#include "downloadqueue.h"

#include "utils.h"

DownloadQueue::DownloadQueue(NetworkManager* networkManager, const QList<QString>& urls,
                             int parallelDownloads,
                             std::function<void(QSharedPointer<DownloadStringJob>)> lambda,
                             bool cancelOnError, int individualTimeout,
                             std::function<HttpHeaders(const QString&)> headersFor)
    : QObject(),
      totalJobs(urls.count()),
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
      headersFor(headersFor),
      cancellationToken(nullptr),
      cancelled(false)
{
    for (const auto& url : urls)
        jobDescriptorQueue.append(FileDownloadDescriptor(url, ""));
}

DownloadQueue::DownloadQueue(NetworkManager* networkManager, const QList<FileDownloadDescriptor>& urlAndPaths,
                             int parallelDownloads, bool cancelOnError)
    : QObject(),
      totalJobs(urlAndPaths.count()),
      completed(0),
      errors(0),
      lastErrorMessage(""),
      cancelOnError(cancelOnError),
      networkManager(networkManager),
      runningJobs(0),
      type(DownloadTypeScaledImage),
      parallelDownloads(parallelDownloads),
      jobDescriptorQueue(),
      lambda(nullptr),
      individualTimeout(-1),
      headersFor(nullptr),
      cancellationToken(nullptr),
      cancelled(false)
{
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
        job = networkManager->downloadAsString(descriptor.url, individualTimeout,
                                               headersFor ? headersFor(descriptor.url) : HttpHeaders());
    else  // if (type == DownloadTypeScaledImage)
        job = networkManager->downloadAsScaledImage(descriptor.url, descriptor.path, descriptor.headers);

    // a job that is done already (e.g. the same file was downloaded before) still signals, that may have
    // happened before the connections
    if (job->isFinished())
    {
        QMetaObject::invokeMethod(
            this, [this, job]() { downloadFinished(job, job->isCompleted); }, Qt::QueuedConnection);
        return;
    }

    QObject::connect(job.get(), &DownloadJobBase::completed, this, [this, job]() { downloadFinished(job, true); });
    QObject::connect(job.get(), &DownloadJobBase::downloadError, this,
                     [this, job]() { downloadFinished(job, false); });
}

void DownloadQueue::downloadFinished(QSharedPointer<DownloadJobBase> job, bool success)
{
    job->disconnect(this);

    auto fileJob = job.dynamicCast<DownloadFileJob>();
    QString path = fileJob ? fileJob->filepath : QString();

    if (success)
    {
        if (lambda != nullptr)
        {
            auto jobS = job.dynamicCast<DownloadStringJob>();
            if (jobS != nullptr)
                lambda(jobS);
        }
        emit singleDownloadCompleted(job->originalUrl, path);
    }
    else
    {
        errors++;
        lastErrorMessage = job->errorString;
        if (cancelOnError)
            clearQueue();
        emit singleDownloadFailed(job->originalUrl, path, job->errorString);
    }

    // counts as one error
    if (!cancelled && cancellationToken != nullptr && *cancellationToken)
    {
        cancelled = true;
        errors++;
        lastErrorMessage = "Download cancelled";
        clearQueue();
    }

    completed++;
    runningJobs--;
    emit progress(completed, totalJobs, errors);

    if (runningJobs == 0 && jobDescriptorQueue.isEmpty())
        emit allCompleted();
    else
        start();
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

void DownloadQueue::clearQueue()
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
    cancelled = false;
}

bool DownloadQueue::awaitCompletion()
{
    // allCompleted was emitted already, or never will be
    if (runningJobs > 0 || !jobDescriptorQueue.isEmpty())
        awaitSignal(this, -1, &DownloadQueue::allCompleted);

    return errors == 0;
}

void DownloadQueue::setCancellationToken(bool* token)
{
    cancellationToken = token;
}
