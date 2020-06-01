#ifndef DOWNLOADMANAGER_H
#define DOWNLOADMANAGER_H

#include <QNetworkReply>

#include "downloadfilejob.h"
#include "downloadimageandrescalejob.h"
#include "downloadstringjob.h"
#include "settings.h"

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

class NetworkManager : public QObject
{
    Q_OBJECT

public:
    explicit NetworkManager(QObject *parent = nullptr);

    QNetworkAccessManager *networkAccessManager();

    QSharedPointer<DownloadStringJob> downloadAsString(const QString &url, int timeout = 6000,
                                                       const QByteArray &postData = QByteArray());
    QSharedPointer<DownloadFileJob> downloadAsFile(const QString &url, const QString &localPath);
    QSharedPointer<DownloadFileJob> downloadAsScaledImage(const QString &url, const QString &localPath);

    void setDownloadSettings(const QSize &size, Settings *settings);

    void addCookie(const QString &domain, const char *key, const char *value);
    void addSetCustomRequestHeader(const QString &domain, const char *key, const char *value);

    bool checkInternetConnection();
    bool connectWifi();
    bool disconnectWifi();

    static void loadCertificates(const QString &certsPath);
    bool urlExists(const QString &url_string);

    bool connected;

signals:
    void connectionStatusChanged(bool connected);
    void activity();

private:
    QNetworkAccessManager *networkManager;
    CCookieJar cookies;

    QSize imageRescaleSize;
    Settings *settings;

    QList<std::tuple<QString, const char *, const char *>> customHeaders;

    QMap<QString, QWeakPointer<DownloadFileJob>> fileDownloads;
};

#endif  // DOWNLOADMANAGER_H
