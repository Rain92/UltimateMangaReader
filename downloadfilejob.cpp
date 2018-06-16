#include "downloadfilejob.h"
#include "downloadmanager.h"
#include "configs.h"


DownloadFileJob::DownloadFileJob(QObject *parent, QNetworkAccessManager *nm, const QString &url, const QString &path)
    : QObject(parent)
    , url(url)
    , isCompleted(false)
    , networkManager(nm)
    , errorString("")
{
    QFileInfo fi(path);
    QString dirname = fi.path();
//    qDebug() << path << dirname;

    if (!QDir(dirname).exists())
        QDir().mkpath(dirname);

    file.setFileName(path);


    QFileInfo completed_file(path + ".completed");

    if (completed_file.exists())
    {
        isCompleted = true;
    }
    else
    {
        if (file.open(QIODevice::WriteOnly | QIODevice::Truncate ))
        {

            QNetworkRequest request(url);
            reply = networkManager->get(request);

//            qDebug() << path;

            QObject::connect(reply, SIGNAL(readyRead()), this, SLOT(downloadFileReadyRead()));
            QObject::connect(reply, SIGNAL(finished()), this, SLOT(downloadFileFinished()));
            QObject::connect(reply, SIGNAL(finished()), qobject_cast<DownloadManager *>(parent), SLOT(onActivity()));
            QObject::connect(reply, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(onError(QNetworkReply::NetworkError)));
            QObject::connect(reply, SIGNAL(sslErrors(const QList<QSslError> &)), this, SLOT(onSslErrors(const QList<QSslError> &)));
        }
        else
        {
            errorString = "Can't create file.";
            emit downloadError();
        }
    }
}

void DownloadFileJob::downloadFileReadyRead()
{
//    qDebug() << "downloadFileReadyRead:" << file.fileName();
    file.write(reply->readAll());
}

void DownloadFileJob::downloadFileFinished()
{
//    qDebug() << "download finished:" << file.fileName();
    file.flush();
    file.close();
    if ( QNetworkReply::NoError != reply->error() )
    {
        file.remove();
        onError(QNetworkReply::NetworkError());
    }
    else
    {
        isCompleted = true;

        QFile completedFile(file.fileName() + ".completed");
        completedFile.open(QIODevice::WriteOnly);
        completedFile.close();

        if (reply != nullptr)
            reply->deleteLater();
        reply = nullptr;
        emit completed();
    }
}

void DownloadFileJob::onSslErrors(const QList<QSslError> &)
{
    QNetworkReply *reply = static_cast<QNetworkReply *>(sender());
    qDebug() << "SSL Error";
    reply->ignoreSslErrors();
    return;

//    if (file.isOpen())
//        file.close();

//    file.remove();

//    foreach (QSslError& ssle, errors)
//    {
//        errorString += ssle.errorString();
//    }
//    errorString = reply->errorString();

//    emit downloadError();
}

void DownloadFileJob::onError(QNetworkReply::NetworkError)
{
    if (file.isOpen())
        file.close();

    file.remove();

    errorString = reply->errorString();

    qDebug() << errorString;

    if (reply != nullptr)
        reply->deleteLater();
    reply = nullptr;

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


