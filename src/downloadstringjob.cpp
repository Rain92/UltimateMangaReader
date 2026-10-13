#include "downloadstringjob.h"

DownloadStringJob::DownloadStringJob(QNetworkAccessManager *networkManager, const QString &url, int timeout,
                                     const HttpHeaders &headers)
    : DownloadBufferJob(networkManager, url, timeout, headers), bufferStr()
{
}

void DownloadStringJob::restart()
{
    bufferStr.clear();

    DownloadBufferJob::restart();
}

void DownloadStringJob::downloadFinished()
{
    DownloadBufferJob::downloadFinished();

    if (isCompleted)
        bufferStr = QString::fromUtf8(buffer);
}
