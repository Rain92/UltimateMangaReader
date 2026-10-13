#include "downloadbufferjob.h"

DownloadBufferJob::DownloadBufferJob(QNetworkAccessManager *networkManager, const DownloadRequest &request)
    : DownloadJobBase(networkManager, request), buffer()
{
}

void DownloadBufferJob::startAttempt()
{
    buffer.clear();
    reply.reset(send(request.url));
    QObject::connect(reply.get(), &QNetworkReply::finished, this, &DownloadBufferJob::replyFinished);
    if (request.maxBytes > 0)
        QObject::connect(reply.get(), &QNetworkReply::readyRead, this, &DownloadBufferJob::readyRead);
}

void DownloadBufferJob::readyRead()
{
    // servers that ignore a Range header send everything
    int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (reply->bytesAvailable() < request.maxBytes || (status != 200 && status != 206))
        return;

    takeResponse(reply.get());
    buffer = reply->read(request.maxBytes);
    // only the job's own handlers: the abort finishes the reply, which releases its activity count
    reply->disconnect(this);
    reply->abort();
    finishCompleted();
}

void DownloadBufferJob::resetState()
{
    DownloadJobBase::resetState();
    buffer.clear();
}

void DownloadBufferJob::replyFinished()
{
    takeResponse(reply.get());
    buffer = request.maxBytes > 0 ? reply->read(request.maxBytes) : reply->readAll();

    if (reply->error() == QNetworkReply::NoError)
    {
        finishCompleted();
        return;
    }

    if (!retryLater(reply.get()))
        finishFailed(replyError(reply.get()));
}
