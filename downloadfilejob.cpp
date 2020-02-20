#include "downloadfilejob.h"

#include "configs.h"
#include "downloadmanager.h"

DownloadFileJob::DownloadFileJob(QObject *parent,
                                 QNetworkAccessManager *networkManager,
                                 const QString &url,
                                 const QString &localFilePath)
    : QObject(parent),
      networkManager(networkManager),
      reply(nullptr),
      url(url),
      filepath(localFilePath),
      isCompleted(false),
      errorString("")
{
    QString dirname = QFileInfo(localFilePath).path();
    QDir().mkpath(dirname);

    file.setFileName(localFilePath + ".part");

    if (QFile::exists(filepath))
    {
        isCompleted = true;
    }
    else
    {
        if (file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        {
            QNetworkRequest request(url);
            reply = networkManager->get(request);

            QObject::connect(reply, SIGNAL(readyRead()), this,
                             SLOT(downloadFileReadyRead()));
            QObject::connect(reply, SIGNAL(finished()), this,
                             SLOT(downloadFileFinished()));
            QObject::connect(reply, SIGNAL(error(QNetworkReply::NetworkError)),
                             this, SLOT(onError(QNetworkReply::NetworkError)));
            QObject::connect(reply, SIGNAL(sslErrors(const QList<QSslError> &)),
                             this, SLOT(onSslErrors(const QList<QSslError> &)));
        }
        else
        {
            errorString = "Can't create file.";
            emit downloadError();
        }
    }
}

DownloadFileJob::~DownloadFileJob()
{
    if (reply != nullptr) reply->deleteLater();
    reply = nullptr;
}

void DownloadFileJob::downloadFileReadyRead()
{
    //    qDebug() << "downloadFileReadyRead:" << file.fileName();
    file.write(reply->readAll());
}

void DownloadFileJob::downloadFileFinished()
{
    //    qDebug() << "download finished:" << file.fileName();
    if (file.isOpen())
    {
        file.flush();
        file.close();
    }

    if (reply == nullptr) return;

    if (reply->error() != QNetworkReply::NoError)
    {
        file.remove();
        onError(QNetworkReply::NetworkError());
    }
    else
    {
        isCompleted = true;

        file.rename(filepath);

        if (reply != nullptr) reply->deleteLater();
        reply = nullptr;
        emit completed();
    }
}

void DownloadFileJob::onSslErrors(const QList<QSslError> &errors)
{
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    if (reply != nullptr)
    {
        foreach (const QSslError &ssle, errors)
            qDebug() << "SSL Error" << ssle.errorString();
        reply->ignoreSslErrors();
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

    if (reply != nullptr)
        errorString = reply->errorString();
    else if (errorString == "")
        errorString = "error";

    qDebug() << errorString;

    if (reply != nullptr) reply->deleteLater();
    reply = nullptr;

    emit downloadError();
}

bool DownloadFileJob::await(int timeout)
{
    if (isCompleted) return true;

    QEventLoop loop;
    connect(this, SIGNAL(completed()), &loop, SLOT(quit()));
    connect(this, SIGNAL(downloadError()), &loop, SLOT(quit()));

    QTimer timer;
    connect(&timer, SIGNAL(timeout()), &loop, SLOT(quit()));
    timer.start(timeout);

    if (errorString != "") return false;

    if (isCompleted) return true;

    loop.exec();

    return isCompleted;
}
