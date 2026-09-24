#include "downloadjobbase.h"

DownloadJobBase::DownloadJobBase(QNetworkAccessManager *networkManager, const QString &url,
                                 const QList<std::tuple<const char *, const char *>> &customHeaders)
    : networkManager(networkManager),
      reply(),
      managerGuard(networkManager),
      customHeaders(customHeaders),
      url(url),
      originalUrl(url),
      isCompleted(false),
      errorString()
{
}

DownloadJobBase::~DownloadJobBase()
{
    // the QNetworkAccessManager deletes its replies when it is destroyed first
    if (!managerGuard)
        (void)reply.release();
}

bool DownloadJobBase::hasFailed() const
{
    // an await() that timed out sets errorString while the download is still running
    return !isCompleted && errorString != "" && (!reply || reply->isFinished());
}

QNetworkRequest DownloadJobBase::createRequest() const
{
    QNetworkRequest request(url);

    // some hosts reject Qt's default user agent
    request.setRawHeader("User-Agent",
                         "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) "
                         "Chrome/130.0.0.0 Safari/537.36");

    for (const auto &[name, value] : qAsConst(customHeaders))
        request.setRawHeader(name, value);

    return request;
}

QList<QNetworkCookie> DownloadJobBase::getCookies()
{
    return reply->header(QNetworkRequest::SetCookieHeader).value<QList<QNetworkCookie>>();
}

void DownloadJobBase::onSslErrors(const QList<QSslError> &errors)
{
    foreach (const QSslError &ssle, errors)
        qDebug() << "SSL Error" << ssle.errorString();

    auto *reply = qobject_cast<QNetworkReply *>(sender());
    if (reply)
        reply->ignoreSslErrors();
}
