#include "downloadstringjob.h"
#include "downloadmanager.h"
#include "configs.h"
#include <QSslError>

DownloadStringJob::DownloadStringJob(QObject *parent, QNetworkAccessManager *nm, const QString &url, int timeout)
    : QObject(parent)
    , url(url)
    , isCompleted(false)
    , networkManager(nm)
    , errorString("")
    , buffer("")
    , reply(nullptr)
    , timeouttimer()
    , timeouttime(timeout)
{
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

    if (timeouttime > 0)
    {
        QObject::connect(&timeouttimer, SIGNAL(timeout()), this, SLOT(timeout()));
        timeouttimer.start(timeouttime);
    }
}

void DownloadStringJob::restart()
{
    if (reply != nullptr)
        reply->deleteLater();
    reply = nullptr;

    isCompleted = false;
    errorString = "";
    buffer = "";

    start();
}

void DownloadStringJob::downloadStringReadyRead()
{
    buffer.append(reply->readAll());
//    reply->close();
}

void DownloadStringJob::downloadStringFinished()
{
    timeouttimer.stop();

    if (reply == nullptr || QNetworkReply::NoError != reply->error())
    {
        emit downloadError();
    }
    else
    {
        isCompleted = true;

        if (reply != nullptr)
            reply->deleteLater();
        reply = nullptr;
        emit completed();
    }
}

void DownloadStringJob::onSslErrors(const QList<QSslError> &errors)
{
    QNetworkReply *reply = static_cast<QNetworkReply *>(sender());
    reply->ignoreSslErrors();
    qDebug() << "SSL error";
    foreach (const QSslError &ssle, errors)
        qDebug() << ssle.errorString();
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
    timeouttimer.stop();

    qDebug() << "ERROR";
    errorString = reply->errorString();

    qDebug() << errorString;

    if (reply != nullptr)
        reply->deleteLater();
    reply = nullptr;

    emit downloadError();
}

void DownloadStringJob::timeout()
{
    qDebug() << "ERROR";
    errorString = "timeout";

    qDebug() << errorString;

    if (reply != nullptr)
        reply->deleteLater();
    reply = nullptr;
    emit downloadError();
}

bool DownloadStringJob::await(int timeout, bool retry)
{
    timeouttimer.stop();

    if (isCompleted)
        return true;

    QTime time;
    time.start();
    QEventLoop loop;
    connect(reply, SIGNAL(finished()), &loop, SLOT(quit()));
    connect(this, SIGNAL(downloadError()), &loop, SLOT(quit()));

    QTimer timer;
    timer.setSingleShot(true);
    connect(&timer, SIGNAL(timeout()), &loop, SLOT(quit()));
    timer.start(timeout);


    if (errorString == "")
        loop.exec();

    int rem = timeout - time.elapsed();
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
    if (rem <= 20)
        errorString += "Download timeout.";


    return isCompleted;
}
