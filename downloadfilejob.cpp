#include "downloadfilejob.h"

#include "configs.h"
#include "downloadmanager.h"

DownloadFileJob::DownloadFileJob(QObject *parent,
                                 QNetworkAccessManager *networkManager,
                                 const QString &url,
                                 const QString &localFilePath)
    : QObject(parent),
      networkManager(networkManager),
      reply(),
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

void DownloadFileJob::downloadFileReadyRead() { file.write(reply->readAll()); }

void DownloadFileJob::downloadFileFinished()
{
    if (file.isOpen())
    {
        file.flush();
        file.close();
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

void DownloadFileJob::onSslErrors(const QList<QSslError> &errors)
{
    foreach (const QSslError &ssle, errors)
        qDebug() << "SSL Error" << ssle.errorString();

    auto *reply = qobject_cast<QNetworkReply *>(sender());
    if (reply)
        reply->ignoreSslErrors();
}

void DownloadFileJob::onError(QNetworkReply::NetworkError)
{
    if (file.isOpen())
    {
        file.flush();
        file.close();
    }

    file.remove();

    errorString = reply->errorString();

    qDebug() << errorString;

    emit downloadError();
}

bool DownloadFileJob::await(int timeout)
{
    if (isCompleted)
        return true;

    QEventLoop loop;
    connect(this, SIGNAL(completed()), &loop, SLOT(quit()));
    connect(this, SIGNAL(downloadError()), &loop, SLOT(quit()));

    QTimer timer;
    connect(&timer, SIGNAL(timeout()), &loop, SLOT(quit()));
    timer.start(timeout);

    if (errorString != "")
        return false;

    if (isCompleted)
        return true;

    loop.exec();

    return isCompleted;
}
