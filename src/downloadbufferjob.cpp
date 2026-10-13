#include "downloadbufferjob.h"

#include "utils.h"

DownloadBufferJob::DownloadBufferJob(QNetworkAccessManager *networkManager, const QString &url, int timeout,
                                     const QList<std::tuple<const char *, const char *> > &customHeaders)
    : DownloadJobBase(networkManager, url, customHeaders),
      timeoutTime(timeout),
      buffer(),
      httpStatus(0)
{
}

void DownloadBufferJob::start()
{
    reply.reset(networkManager->get(createRequest()));
    trackReply(reply.get());

    QObject::connect(reply.get(), &QNetworkReply::finished, this, &DownloadBufferJob::downloadFinished);
    QObject::connect(reply.get(), &QNetworkReply::errorOccurred, this, &DownloadBufferJob::onError);
    QObject::connect(reply.get(), &QNetworkReply::sslErrors, this, &DownloadJobBase::onSslErrors);

    if (timeoutTime > 0)
    {
        QObject::connect(&timeoutTimer, &QTimer::timeout, this, &DownloadBufferJob::timeout,
                         Qt::UniqueConnection);
        timeoutTimer.start(timeoutTime);
    }
}

void DownloadBufferJob::restart()
{
    isCompleted = false;
    errorString = "";
    httpStatus = 0;
    buffer.clear();

    start();
}

void DownloadBufferJob::downloadFinished()
{
    timeoutTimer.stop();

    if (followRedirect())
        return;

    // errors are handled by onError()
    if (errorString == "" && reply->error() == QNetworkReply::NoError)
    {
        buffer = reply->readAll();

        isCompleted = true;

        emit completed();
    }
}

void DownloadBufferJob::onError(QNetworkReply::NetworkError)
{
    timeoutTimer.stop();

    httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    if (errorString == "")
        errorString = "Download error: " + reply->errorString();

    qDebug() << "Error string:" << errorString;
    emit downloadError();
}

void DownloadBufferJob::timeout()
{
    timeoutTimer.stop();
    errorString = "Download error: timeout";
    // only the job's own handlers: the abort finishes the reply, which releases its activity count
    reply->disconnect(this);
    reply->abort();

    emit downloadError();
}

bool DownloadBufferJob::await(int timeout, bool retry, int maxRetries)
{
    timeoutTimer.stop();

    if (isCompleted)
        return true;

    QElapsedTimer time;
    time.start();

    awaitSignal(this, timeout, &DownloadJobBase::completed, &DownloadJobBase::downloadError);

    int rem = timeout - time.elapsed();
    if (errorString != "")
    {
        // client errors won't go away by retrying right away
        bool clientError = httpStatus >= 400 && httpStatus < 500 && httpStatus != 408 && httpStatus != 429;

        if (!retry || clientError || errorString.contains("Protocol"))
            return false;

        if (rem > 0 && maxRetries > 0)
        {
            restart();
            return await(rem, retry, maxRetries - 1);
        }
    }
    if (rem <= 20)
        errorString = "Download timeout.";

    return isCompleted;
}
