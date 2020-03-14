#ifndef DOWNLOADMANAGER_H
#define DOWNLOADMANAGER_H

#include <QList>
#include <QNetworkReply>

#include "downloadfilejob.h"
#include "downloadimageandrescalejob.h"
#include "downloadstringjob.h"

class CCookieJar : public QNetworkCookieJar
{
public:
    CCookieJar() : cookies() {}
    void addCookie(const QString &domain, const char *key, const char *value)
    {
        QNetworkCookie c = QNetworkCookie(QByteArray(key), QByteArray(value));
        c.setDomain(domain);
        c.setExpirationDate(QDateTime::currentDateTime().addDays(1));
        cookies.append(c);
        setAllCookies(cookies);
    }
    QList<QNetworkCookie> cookies;
};

class DownloadManager : public QObject
{
    Q_OBJECT

public:
    explicit DownloadManager(QObject *parent);

    QNetworkAccessManager *networkAccessManager();

    QSharedPointer<DownloadStringJob> downloadAsString(const QString &url,
                                                       int timeout = 4000);
    QSharedPointer<DownloadStringJob> downloadAsStringPost(const QString &url,
                                                           QByteArray *postdata,
                                                           int timeout = 4000);
    QSharedPointer<DownloadFileJob> downloadAsFile(const QString &url,
                                                   const QString &localPath);
    QSharedPointer<DownloadFileJob> downloadAsScaledImage(
        const QString &url, const QString &localPath);

    void setImageRescaleSize(QSize size);

    void addCookie(const QString &domain, const char *key, const char *value);

    bool connected();
    bool connect();

    static void loadCertificates(const QString &certsPath);

signals:

public slots:

private:
    QNetworkAccessManager *networkmanager;
    CCookieJar cookies;

    QSize imageRescaleSize;

    QMap<QString, QWeakPointer<DownloadFileJob>> fileDownloads;
};

#endif  // DOWNLOADMANAGER_H
