#include "downloadstringjob.h"

#include "utils.h"

DownloadStringJob::DownloadStringJob(QNetworkAccessManager *networkManager, const QString &url, int timeout,
                                     const QByteArray &postdata,
                                     const QList<std::tuple<const char *, const char *> > &customHeaders)
    : DownloadJobBase(networkManager, url, customHeaders),
      timeoutTime(timeout),
      postData(postdata),
      buffer("")
{
}

void DownloadStringJob::start()
{
    QNetworkRequest request(url);

    for (const auto &[name, value] : qAsConst(customHeaders))
        request.setRawHeader(name, value);

    if (postData.isEmpty())
    {
        reply.reset(networkManager->get(request));
    }
    else
    {
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
        reply.reset(networkManager->post(request, postData));
    }
    reply->setParent(nullptr);

    QObject::connect(reply.get(), &QNetworkReply::finished, this, &DownloadStringJob::downloadStringFinished);
    QObject::connect(reply.get(), &QNetworkReply::errorOccurred, this, &DownloadStringJob::onError);
    QObject::connect(reply.get(), &QNetworkReply::sslErrors, this, &DownloadJobBase::onSslErrors);

    if (timeoutTime > 0)
    {
        QObject::connect(&timeoutTimer, &QTimer::timeout, this, &DownloadStringJob::timeout);
        timeoutTimer.start(timeoutTime);
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
    // buffer.append(reply->readAll());
}

void DownloadStringJob::downloadStringFinished()
{
    timeoutTimer.stop();

    QUrl redirect = reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl();
    if (redirect.isValid())
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
        // already handled
        // emit downloadError();
    }
    else
    {
        buffer.append(reply->readAll());

        isCompleted = true;

        emit completed();
    }
}

void DownloadStringJob::onError(QNetworkReply::NetworkError)
{
    timeoutTimer.stop();

    if (errorString == "")
        errorString = "Download error: " + reply->errorString();

    qDebug() << "Error string:" << errorString;
    emit downloadError();
}

void DownloadStringJob::timeout()
{
    errorString = "Download error: timeout";
    QObject::disconnect(reply.get(), &QNetworkReply::finished, this,
                        &DownloadStringJob::downloadStringFinished);
    reply->abort();
}

bool DownloadStringJob::await(int timeout, bool retry)
{
    timeoutTimer.stop();

    if (isCompleted)
        return true;

    QElapsedTimer time;
    time.start();

    awaitSignal(this, {SIGNAL(completed()), SIGNAL(downloadError())}, timeout);

    int rem = timeout - time.elapsed();
    if (errorString != "")
    {
        if (!retry || errorString.contains("Protocol"))
            return false;

        if (rem > 0)
        {
            restart();
            return await(rem, retry);
        }
    }
    if (rem <= 20)
        errorString = "Download timeout.";

    return isCompleted;
}
