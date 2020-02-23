#include "downloadstringjob.h"

#include <QElapsedTimer>
#include <QSslError>

#include "configs.h"
#include "downloadmanager.h"

DownloadStringJob::DownloadStringJob(QObject *parent,
                                     QNetworkAccessManager *networkManager,
                                     const QString &url, int timeout,
                                     QByteArray *postdata)
    : QObject(parent),
      networkManager(networkManager),
      reply(),
      timeouttime(timeout),
      postdata(postdata),
      url(url),
      isCompleted(false),
      errorString(""),
      buffer("")
{
}

void DownloadStringJob::start()
{
    QNetworkRequest request(url);
    if (!postdata)
        reply.reset(networkManager->get(request));
    else
    {
        request.setHeader(QNetworkRequest::ContentTypeHeader,
                          "application/x-www-form-urlencoded");
        reply.reset(networkManager->post(request, *postdata));
    }
    reply->setParent(this);

    //    QObject::connect(reply.get(), SIGNAL(readyRead()), this,
    //                     SLOT(downloadStringReadyRead()));
    QObject::connect(reply.get(), SIGNAL(finished()), this,
                     SLOT(downloadStringFinished()));
    QObject::connect(reply.get(), SIGNAL(error(QNetworkReply::NetworkError)),
                     this, SLOT(onError(QNetworkReply::NetworkError)));
    QObject::connect(reply.get(), SIGNAL(sslErrors(const QList<QSslError> &)),
                     this, SLOT(onSslErrors(const QList<QSslError> &)));

    if (timeouttime > 0)
    {
        QObject::connect(&timeouttimer, SIGNAL(timeout()), this,
                         SLOT(timeout()));
        timeouttimer.start(timeouttime);
    }
}

void DownloadStringJob::restart()
{
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

    QUrl redirect =
        reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl();
    if (redirect.isValid() && reply->url() != redirect)
    {
        if (redirect.host() != "")
        {
            this->url = redirect.toString();
        }
        else
        {
            QUrl base(this->url);
            base.setPath(redirect.path());
            this->url = base.toString();
        }
        this->restart();
        return;
    }

    if (errorString != "" || (reply->error() != QNetworkReply::NoError))
    {
        emit downloadError();
    }
    else
    {
        buffer.append(reply->readAll());

        isCompleted = true;

        emit completed();
    }
}

void DownloadStringJob::onSslErrors(const QList<QSslError> &errors)
{
    foreach (const QSslError &ssle, errors)
        qDebug() << "SSL Error" << ssle.errorString();

    auto *reply = qobject_cast<QNetworkReply *>(sender());
    if (reply)
        reply->ignoreSslErrors();
}

void DownloadStringJob::onError(QNetworkReply::NetworkError)
{
    timeouttimer.stop();

    errorString = "Download error: ";

    errorString += reply->errorString();

    qDebug() << errorString;
    emit downloadError();
}

void DownloadStringJob::timeout()
{
    errorString = "Download error: timeout";

    qDebug() << errorString;

    emit downloadError();
}

bool DownloadStringJob::await(int timeout, bool retry)
{
    QMetaObject::invokeMethod(&timeouttimer, "stop", Qt::AutoConnection);
    //    timeouttimer.stop();

    if (isCompleted)
        return true;

    QElapsedTimer time;
    QEventLoop loop;
    time.start();

    connect(reply.get(), SIGNAL(finished()), &loop, SLOT(quit()));
    connect(this, SIGNAL(downloadError()), &loop, SLOT(quit()));

    QTimer timer;
    timer.setSingleShot(true);
    connect(&timer, SIGNAL(timeout()), &loop, SLOT(quit()));
    timer.start(timeout);

    if (errorString == "" && !isCompleted)
        loop.exec();

    int rem = timeout - time.elapsed();
    if (errorString != "")
    {
        //        qDebug() << "url" << url;
        if (!retry)
            return false;

        //        qDebug() << url << rem;
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

QList<QNetworkCookie> DownloadStringJob::getCookies()
{
    return reply->header(QNetworkRequest::SetCookieHeader)
        .value<QList<QNetworkCookie>>();
}
