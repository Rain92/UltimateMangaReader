#include "downloadfilejob.h"

#include "utils.h"

DownloadFileJob::DownloadFileJob(QNetworkAccessManager *networkManager,
                                 const QString &url,
                                 const QString &localFilePath)
    : DownloadJobBase(networkManager, url), filepath(localFilePath)
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
            QNetworkRequest request(url);
            reply.reset(networkManager->get(request));
            reply->setParent(this);

            QObject::connect(reply.get(), SIGNAL(readyRead()), this,
                             SLOT(downloadFileReadyRead()));
            QObject::connect(reply.get(), SIGNAL(finished()), this,
                             SLOT(downloadFileFinished()));
            QObject::connect(reply.get(),
                             SIGNAL(error(QNetworkReply::NetworkError)), this,
                             SLOT(onError(QNetworkReply::NetworkError)));
            QObject::connect(reply.get(),
                             SIGNAL(sslErrors(const QList<QSslError> &)), this,
                             SLOT(onSslErrors(const QList<QSslError> &)));
        }
        else
        {
            errorString = "Can't create file.";
            emit downloadError();
        }
    }
}

DownloadFileJob::~DownloadFileJob() {}

void DownloadFileJob::restart()
{
    isCompleted = false;
    errorString = "";
    reply.reset();
    start();
}

void DownloadFileJob::downloadFileReadyRead() { file.write(reply->readAll()); }

void DownloadFileJob::downloadFileFinished()
{
    if (file.isOpen())
    {
        file.flush();
        file.close();
    }

    QUrl redirect =
        reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl();
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
        errorString = "Download error: " + reply->errorString();

    qDebug() << "Error string:" << errorString;
    emit downloadError();
}

bool DownloadFileJob::await(int timeout)
{
    if (isCompleted)
        return true;

    awaitSignal(this, {SIGNAL(completed()), SIGNAL(downloadError())}, timeout);

    if (errorString != "")
        return false;

    return isCompleted;
}
