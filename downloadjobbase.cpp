#include "downloadjobbase.h"

DownloadJobBase::DownloadJobBase(QNetworkAccessManager *networkManager,
                                 const QString &url)
    : networkManager(networkManager),
      reply(),
      url(url),
      originalUrl(url),
      isCompleted(false),
      errorString("")
{
}

QList<QNetworkCookie> DownloadJobBase::getCookies()
{
    return reply->header(QNetworkRequest::SetCookieHeader)
        .value<QList<QNetworkCookie>>();
}

void DownloadJobBase::onSslErrors(const QList<QSslError> &errors)
{
    foreach (const QSslError &ssle, errors)
        qDebug() << "SSL Error" << ssle.errorString();

    auto *reply = qobject_cast<QNetworkReply *>(sender());
    if (reply)
        reply->ignoreSslErrors();
}
