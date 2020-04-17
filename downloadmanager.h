#ifndef DOWNLOADMANAGER_H
#define DOWNLOADMANAGER_H

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
    explicit DownloadManager(QObject *parent = nullptr);

    QNetworkAccessManager *networkAccessManager();

    QSharedPointer<DownloadStringJob> downloadAsString(const QString &url, int timeout = 6000,
                                                       const QByteArray &postData = QByteArray());
    QSharedPointer<DownloadFileJob> downloadAsFile(const QString &url, const QString &localPath);
    QSharedPointer<DownloadFileJob> downloadAsScaledImage(const QString &url, const QString &localPath);

    void setImageRescaleSize(const QSize &size);

    void addCookie(const QString &domain, const char *key, const char *value);

    bool checkConnection();
    bool connect();

    static void loadCertificates(const QString &certsPath);
    bool urlExists(const QString &url_string);

    bool connected;

signals:
    void connectionStatusChanged(bool connected);

public slots:

private:
    QNetworkAccessManager *networkManager;
    CCookieJar cookies;

    QSize imageRescaleSize;

    QMap<QString, QWeakPointer<DownloadFileJob>> fileDownloads;
};

#endif  // DOWNLOADMANAGER_H
