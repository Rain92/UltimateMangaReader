#include "downloadstringjob.h"
#include "downloadmanager.h"
#include "configs.h"

DownloadStringJob::DownloadStringJob(QObject *parent, QNetworkAccessManager *nm, const QString &url)
    : QObject(parent)
    , url(url)
    , isCompleted(false)
    , networkManager(nm)
    , errorString("")
    , buffer("")
    , reply(nullptr)
{

}
DownloadStringJob::~DownloadStringJob()
{
    if (reply != nullptr)
        delete reply;
}

void DownloadStringJob::start()
{
    QNetworkRequest request(url);
    reply = networkManager->get(request);

    QObject::connect(reply, SIGNAL(readyRead()), this, SLOT(downloadStringReadyRead()));
    QObject::connect(reply, SIGNAL(finished()), this, SLOT(downloadStringFinished()));
    QObject::connect(reply, SIGNAL(finished()), qobject_cast<DownloadManager *>(parent()), SLOT(onActivity()));
    QObject::connect(reply, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(onError(QNetworkReply::NetworkError)));
    QObject::connect(reply, SIGNAL(sslErrors(const QList<QSslError> &)), this, SLOT(onSslErrors(const QList<QSslError> &)));
}

void DownloadStringJob::restart()
{
    if (reply != nullptr)
        delete reply;
    isCompleted = false;
    errorString = "";
    buffer = "";

    start();
}

void DownloadStringJob::downloadStringReadyRead()
{
    buffer.append(reply->readAll());
}

void DownloadStringJob::downloadStringFinished()
{
//    reply->deleteLater();

    if (QNetworkReply::NoError != reply->error())
    {
        onError(QNetworkReply::NetworkError());
    }
    else
    {
        isCompleted = true;

        emit completed();
    }
}

void DownloadStringJob::onSslErrors(const QList<QSslError> &)
{
    QNetworkReply *reply = static_cast<QNetworkReply *>(sender());
    reply->ignoreSslErrors();
    qDebug() << "SSL error";
    return;

//    foreach (QSslError ssle, errors)
//    {
//        errorString += ssle.errorString();
//    }
//    errorString = reply->errorString();

//    emit downloadError();
}

void DownloadStringJob::onError(QNetworkReply::NetworkError)
{
    qDebug() << "ERROR";
    errorString = reply->errorString();

    qDebug() << errorString;
//    reply->deleteLater();

    emit downloadError();
}

bool DownloadStringJob::await(int timeout, bool retry)
{
    QTime time;
    time.start();
    QEventLoop loop;
    connect(reply, SIGNAL(finished()), &loop, SLOT(quit()));
    connect(this, SIGNAL(downloadError()), &loop, SLOT(quit()));

    QTimer timer;
    timer.setSingleShot(true);
    connect(&timer, SIGNAL(timeout()), &loop, SLOT(quit()));
    timer.start(timeout);

    if (isCompleted)
        return true;

    if (errorString == "")
        loop.exec();

    int rem = timeout - time.elapsed();
    qDebug() << rem;
    if (errorString != "")
    {
        qDebug() << url;
        if (!retry)
            return false;

        qDebug() << rem;
        if (rem > 0)
        {
            restart();
            return await(rem, retry);
        }
    }
    if(rem <= 20)
        errorString += "Download timeout.";


    return isCompleted;
}
