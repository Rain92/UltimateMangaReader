#include "downloadfilejob.h"

#include "utils.h"

DownloadFileJob::DownloadFileJob(QNetworkAccessManager *networkManager, const QString &url,
                                 const QString &localFilePath,
                                 const QList<std::tuple<const char *, const char *>> &customHeaders)
    : DownloadJobBase(networkManager, url, customHeaders), filepath(localFilePath)
{
}

void DownloadFileJob::start()
{
    QString dirname = QFileInfo(filepath).path();
    QDir().mkpath(dirname);

    file.setFileName(filepath + ".part");

    if (QFile::exists(filepath))
    {
        isCompleted = true;
    }
    else
    {
        if (file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        {
            QNetworkRequest request = createRequest();
            // abort stalled downloads, otherwise a DownloadQueue waits forever
            request.setTransferTimeout(20000);

            reply.reset(networkManager->get(request));
            //            reply->setParent(nullptr);

            QObject::connect(reply.get(), &QNetworkReply::readyRead, this,
                             &DownloadFileJob::downloadFileReadyRead);
            QObject::connect(reply.get(), &QNetworkReply::finished, this,
                             &DownloadFileJob::downloadFileFinished);
            QObject::connect(reply.get(), &QNetworkReply::sslErrors, this, &DownloadJobBase::onSslErrors);
        }
        else
        {
            errorString = "Can't create file.";
            emit downloadError();
        }
    }
}

void DownloadFileJob::restart()
{
    isCompleted = false;
    errorString = "";
    reply.reset();
    start();
}

void DownloadFileJob::downloadFileReadyRead()
{
    file.write(reply->readAll());
}

void DownloadFileJob::downloadFileFinished()
{
    if (file.isOpen())
    {
        file.flush();
        file.close();
    }

    QUrl redirect = reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl();
    if (redirect.isValid() && reply->url() != redirect)
    {
        this->url = redirect.toString();
        this->restart();
        return;
    }

    if (reply->error() != QNetworkReply::NoError)
    {
        file.remove();
        onError(QNetworkReply::NetworkError());
    }
    else
    {
        isCompleted = true;

        file.rename(filepath);

        emit completed();
    }
}

void DownloadFileJob::onError(QNetworkReply::NetworkError)
{
    if (file.isOpen())
    {
        file.flush();
        file.close();
    }

    file.remove();

    if (errorString == "")
        errorString = "Download error " +
                      reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toString() + ": " +
                      reply->errorString();

    qDebug() << "Error string:" << errorString;
    emit downloadError();
}

bool DownloadFileJob::await(int timeout)
{
    if (isCompleted)
        return true;

    awaitSignal(this, {SIGNAL(completed()), SIGNAL(downloadError())}, timeout);

    if (isCompleted)
        return true;

    if (errorString == "")
        errorString = "Download timeout.";

    return false;
}
