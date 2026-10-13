#include "downloadbufferjob.h"

#include "utils.h"

DownloadBufferJob::DownloadBufferJob(QNetworkAccessManager *networkManager, const QString &url, int timeout,
                                     const HttpHeaders &headers)
    : DownloadJobBase(networkManager, url, headers), buffer(), timeoutTimer(), timeoutTime(timeout)
{
    timeoutTimer.setSingleShot(true);
    QObject::connect(&timeoutTimer, &QTimer::timeout, this, &DownloadBufferJob::timeout);
}

void DownloadBufferJob::start()
{
    reply.reset(get(originalUrl));

    QObject::connect(reply.get(), &QNetworkReply::finished, this, &DownloadBufferJob::downloadFinished);

    if (timeoutTime > 0)
        timeoutTimer.start(timeoutTime);
}

void DownloadBufferJob::restart()
{
    timeoutTimer.stop();
    resetState();
    buffer.clear();
    reply.reset();

    start();
}

void DownloadBufferJob::downloadFinished()
{
    timeoutTimer.stop();

    url = reply->url().toString();
    httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    if (reply->error() != QNetworkReply::NoError)
    {
        finishFailed(replyErrorString(reply.get()));
        return;
    }

    buffer = reply->readAll();
    finishCompleted();
}

void DownloadBufferJob::timeout()
{
    abortWith("Download timeout.");
}

bool DownloadBufferJob::await(int timeout, bool retry, int maxRetries)
{
    // the await's timeout applies instead
    timeoutTimer.stop();
    auto hold = holdActivity();

    QDeadlineTimer deadline(qMax(0, timeout));

    for (;;)
    {
        if (!isFinished())
            awaitSignal(this, qMax<qint64>(1, deadline.remainingTime()), &DownloadJobBase::completed,
                        &DownloadJobBase::downloadError);

        if (isCompleted)
            return true;

        // not finished in time: it doesn't go on in the background
        if (!hasFailed())
        {
            abortWith("Download timeout.");
            return false;
        }

        bool transient = reply && isTransientError(reply.get());
        if (!retry || !transient || maxRetries-- <= 0 || deadline.remainingTime() < 20)
            return false;

        qDebug() << "Retrying:" << originalUrl << errorString;
        restart();
    }
}
