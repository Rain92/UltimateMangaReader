#include "downloadbufferjob.h"

#include "utils.h"

DownloadBufferJob::DownloadBufferJob(QNetworkAccessManager *networkManager, const QString &url, int timeout,
                                     const QByteArray &postdata,
                                     const QList<std::tuple<const char *, const char *> > &customHeaders)
    : DownloadJobBase(networkManager, url, customHeaders), timeoutTime(timeout), postData(postdata), buffer()
{
}

void DownloadBufferJob::start()
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
    //    reply->setParent(nullptr);

    QObject::connect(reply.get(), &QNetworkReply::finished, this, &DownloadBufferJob::downloadBufferFinished);
    QObject::connect(reply.get(), &QNetworkReply::errorOccurred, this, &DownloadBufferJob::onError);
    QObject::connect(reply.get(), &QNetworkReply::sslErrors, this, &DownloadJobBase::onSslErrors);

    if (timeoutTime > 0)
    {
        QObject::connect(&timeoutTimer, &QTimer::timeout, this, &DownloadBufferJob::timeout);
        timeoutTimer.start(timeoutTime);
    }
}

void DownloadBufferJob::restart()
{
    isCompleted = false;
    errorString = "";
    buffer.clear();

    start();
}

void DownloadBufferJob::downloadBufferReadyRead()
{
    // read it all at once when finished
    // buffer.append(reply->readAll());
}

void DownloadBufferJob::downloadBufferFinished()
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
        buffer = reply->readAll();

        isCompleted = true;

        emit completed();
    }
}

void DownloadBufferJob::onError(QNetworkReply::NetworkError)
{
    timeoutTimer.stop();

    if (errorString == "")
        errorString = "Download error: " + reply->errorString();

    qDebug() << "Error string:" << errorString;
    emit downloadError();
}

void DownloadBufferJob::timeout()
{
    errorString = "Download error: timeout";
    reply.get()->disconnect();
    reply->abort();
}

bool DownloadBufferJob::await(int timeout, bool retry)
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
