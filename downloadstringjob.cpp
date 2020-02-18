#include "downloadstringjob.h"
#include "downloadmanager.h"
#include "configs.h"
#include <QSslError>
#include <QElapsedTimer>

DownloadStringJob::DownloadStringJob(QObject *parent, QNetworkAccessManager *networkManager, const QString &url, int timeout)
    : QObject(parent)
    , networkManager(networkManager)
    , reply(nullptr)
    , url(url)
    , isCompleted(false)
    , errorString("")
    , buffer("")
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
    {
        reply->close();
        reply->deleteLater();
    }
    reply = nullptr;

    isCompleted = false;
    errorString = "";
    buffer = "";

    start();
}

void DownloadStringJob::downloadStringReadyRead()
{
    // read it all at once when finished
//    buffer.append(reply->readAll());
}

void DownloadStringJob::downloadStringFinished()
{
    timeouttimer.stop();

    if (errorString != "" || (reply != nullptr && reply->error() != QNetworkReply::NoError))
    {
        emit downloadError();
    }
    else
    {
        buffer.append(reply->readAll());

        isCompleted = true;

        if (reply != nullptr)
            reply->deleteLater();
        reply = nullptr;
        emit completed();
    }
}

void DownloadStringJob::onSslErrors(const QList<QSslError> &errors)
{
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    if (reply != nullptr)
    {
        foreach (const QSslError& ssle, errors)
            qDebug() << "SSL Error" << ssle.errorString();
        reply->ignoreSslErrors();
    }
}

void DownloadStringJob::onError(QNetworkReply::NetworkError)
{
    timeouttimer.stop();

    errorString = "Download error: ";

    if (reply != nullptr)
        errorString += reply->errorString();

    qDebug() << errorString;

    if (reply != nullptr)
        reply->deleteLater();
    reply = nullptr;

    emit downloadError();
}

void DownloadStringJob::timeout()
{
    errorString = "Download error: timeout";

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

    QElapsedTimer time;
    QEventLoop loop;
    time.start();

    if (reply != nullptr)
    {
        connect(reply, SIGNAL(finished()), &loop, SLOT(quit()));
        connect(this, SIGNAL(downloadError()), &loop, SLOT(quit()));

        QTimer timer;
        timer.setSingleShot(true);
        connect(&timer, SIGNAL(timeout()), &loop, SLOT(quit()));
        timer.start(timeout);


        if (errorString == "" && !isCompleted)
            loop.exec();
    }

    int rem = timeout - time.elapsed();
    if (errorString != "")
    {
        qDebug() << "url" << url;
        if (!retry)
            return false;

        qDebug() << url << rem;
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
