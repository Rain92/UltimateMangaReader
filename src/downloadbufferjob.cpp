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
}

void DownloadBufferJob::resetState()
{
    DownloadJobBase::resetState();
    buffer.clear();
}

void DownloadBufferJob::replyFinished()
{
    takeResponse(reply.get());
    buffer = reply->readAll();

    if (reply->error() == QNetworkReply::NoError)
    {
        finishCompleted();
        return;
    }

    if (!retryLater(reply.get()))
        finishFailed(replyErrorString(reply.get()));
}
