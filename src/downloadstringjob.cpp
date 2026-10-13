#include "downloadstringjob.h"

DownloadStringJob::DownloadStringJob(QNetworkAccessManager *networkManager, const QString &url, int timeout,
                                     const QList<std::tuple<const char *, const char *> > &customHeaders)
    : DownloadBufferJob(networkManager, url, timeout, customHeaders), bufferStr("")
{
}

void DownloadStringJob::restart()
{
    bufferStr = "";

    DownloadBufferJob::restart();
}

void DownloadStringJob::downloadFinished()
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

    // errors are handled by onError()
    if (errorString == "" && reply->error() == QNetworkReply::NoError)
    {
        buffer = reply->readAll();
        bufferStr = buffer;

        isCompleted = true;

        emit completed();
    }
}
