#ifndef WIFI_H
#define WIFI_H

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
    DownloadManager(QObject *parent);

    bool connected();

    QMap<QString, DownloadFileJob *> *fileDownloads;

    DownloadStringJob *downloadAsString(const QString &url, int timeout = 4000);
    DownloadFileJob *downloadAsFile(const QString &url,
                                    const QString &localPath,
                                    bool usedownloadmap = true);
    DownloadFileJob *downloadAsScaledImage(const QString &url,
                                           const QString &localPath);

    bool awaitAllFileDownloads(int timeout);

    void setImageRescaleSize(QSize size);

    void addCookie(const QString &domain, const char *key, const char *value);

    static void loadCertificates(const QString &certsPath);

    QNetworkAccessManager *networkmanager;

signals:

public slots:
    bool connect();

private:
    CCookieJar *cookies;

    QSize imageRescaleSize;
};

#endif  // WIFI_H
