#include "downloadfilejob.h"

DownloadFileJob::DownloadFileJob(QNetworkAccessManager *networkManager, const DownloadRequest &request,
                                 const QString &localFilePath)
    : DownloadJobBase(networkManager, request), filepath(localFilePath), partFile()
{
}

DownloadRequest DownloadFileJob::defaultRequest(const QString &url, const HttpHeaders &headers)
{
    DownloadRequest request;
    request.url = url;
    request.headers = headers;
    request.timeout = 0;
    request.transferTimeout = 20000;
    request.maxRetries = 2;
    request.retryDelay = 1000;
    return request;
}

void DownloadFileJob::startAttempt()
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

    reply.reset(send(request.url));
    QObject::connect(reply.get(), &QNetworkReply::readyRead, this, &DownloadFileJob::readyRead);
    QObject::connect(reply.get(), &QNetworkReply::finished, this, &DownloadFileJob::replyFinished);
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
    takeResponse(reply.get());

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

    // Only the last attempt counts as failed, until then the job is handed out to whoever asks for the same
    // file.
    if (!retryLater(reply.get()))
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
