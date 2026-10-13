#include "downloadfilejob.h"

#include "utils.h"

DownloadFileJob::DownloadFileJob(QNetworkAccessManager *networkManager, const QString &url,
                                 const QString &localFilePath, const HttpHeaders &headers)
    : DownloadJobBase(networkManager, url, headers), filepath(localFilePath), partFile(), retries(0)
{
}

void DownloadFileJob::start()
{
    if (QFile::exists(filepath))
    {
        finishCompleted();
        return;
    }

    QString error = prepareTarget();
    if (!error.isEmpty())
    {
        finishFailed(error);
        return;
    }

    // abort stalled downloads, otherwise a DownloadQueue waits forever
    QNetworkRequest request = createRequest(originalUrl, headers);
    request.setTransferTimeout(20000);

    reply.reset(networkManager->get(request));
    prepareReply(reply.get());

    QObject::connect(reply.get(), &QNetworkReply::readyRead, this, &DownloadFileJob::readyRead);
    QObject::connect(reply.get(), &QNetworkReply::finished, this, &DownloadFileJob::replyFinished);
}

void DownloadFileJob::restart()
{
    resetState();
    retries = 0;
    reply.reset();
    start();
}

QString DownloadFileJob::prepareTarget()
{
    QDir().mkpath(QFileInfo(filepath).path());

    partFile.setFileName(filepath + ".part");
    if (!partFile.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return "Can't create file: " + partFile.errorString();

    return {};
}

void DownloadFileJob::readyRead()
{
    auto data = reply->readAll();
    if (partFile.write(data) != data.size())
    {
        QString error = "Can't write file: " + partFile.errorString();
        partFile.close();
        partFile.remove();
        abortWith(error);
    }
}

void DownloadFileJob::replyFinished()
{
    url = reply->url().toString();
    httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    if (reply->error() == QNetworkReply::NoError)
    {
        replySucceeded();
        return;
    }

    if (partFile.isOpen())
    {
        partFile.close();
        partFile.remove();
    }

    // overloaded servers, rate limits and connection problems often go away after a moment. Only the last
    // attempt counts as failed, until then the job is handed out to whoever asks for the same file.
    const int maxRetries = 2;
    if (isTransientError(reply.get()) && retries < maxRetries)
    {
        int delayMs = 1000 << retries++;
        qDebug() << "Retrying in" << delayMs << "ms:" << originalUrl << reply->errorString();
        retryHold = holdActivity();
        // not right away: this runs in a signal of the reply that start() replaces
        QTimer::singleShot(delayMs, this,
                           [this]()
                           {
                               if (!isFinished())
                                   start();
                           });
        return;
    }

    finishFailed(replyErrorString(reply.get()));
}

void DownloadFileJob::replySucceeded()
{
    // the last data, if there was no readyRead for it
    readyRead();
    if (isFinished())
        return;

    bool flushed = partFile.flush();
    partFile.close();
    if (!flushed || partFile.error() != QFileDevice::NoError)
    {
        QString error = "Can't write file: " + partFile.errorString();
        partFile.remove();
        finishFailed(error);
        return;
    }

    // another job may have saved it meanwhile (rename doesn't overwrite)
    if (QFile::exists(filepath))
        partFile.remove();
    else if (!partFile.rename(filepath))
    {
        QString error = "Can't save file: " + partFile.errorString();
        partFile.remove();
        finishFailed(error);
        return;
    }

    finishCompleted();
}

bool DownloadFileJob::await(int timeout)
{
    if (!isFinished())
        awaitSignal(this, qMax(1, timeout), &DownloadJobBase::completed, &DownloadJobBase::downloadError);

    return isCompleted;
}
