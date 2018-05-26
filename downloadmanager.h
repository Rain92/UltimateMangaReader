#ifndef WIFI_H
#define WIFI_H

#include <QNetworkReply>
#include <QList>

#include "downloadfilejob.h"
#include "downloadstringjob.h"
#include "downloadimageandrescalejob.h"


class CCookieJar : public QNetworkCookieJar
{
public:
    CCookieJar(): cookies() {}
    void addCookie(QString domain, const char *key, const char *value)
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

    bool connect();
    bool connected();

    QMap<QString, DownloadFileJob *> *fileDownloads;

    DownloadStringJob *downloadAsString(QString url);
    DownloadFileJob *downloadAsFile(QString url, QString path);
    DownloadFileJob *downloadAsScaledImage(QString url, QString path);

    bool awaitAllFileDownloads(int timeout);

    void setImageSize(int width, int height);


    void addCookie(QString domain, const char *key, const char *value);
signals:

public slots:
    void onActivity();


private:
    CCookieJar *cookies;

    QNetworkAccessManager *manager;
    int imagewidth;
    int imageheight;

};

#endif // WIFI_H
