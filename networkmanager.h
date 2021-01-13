#ifndef DOWNLOADMANAGER_H
#define DOWNLOADMANAGER_H

#include <QNetworkReply>

#include "downloadbufferjob.h"
#include "downloadfilejob.h"
#include "downloadimageandrescalejob.h"
#include "downloadstringjob.h"
#include "settings.h"

class NetworkManager : public QObject
{
    Q_OBJECT

public:
    explicit NetworkManager(QObject *parent = nullptr);

    QNetworkAccessManager *networkAccessManager();

    QSharedPointer<DownloadStringJob> downloadAsString(const QString &url, int timeout = 6000,
                                                       const QByteArray &postData = QByteArray());
    QSharedPointer<DownloadBufferJob> downloadToBuffer(const QString &url, int timeout = 6000,
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
    bool urlExists(const QString &url);

    bool connected;

signals:
    void connectionStatusChanged(bool connected);
    void activity();
    void downloadedImage(const QString &path, QSharedPointer<QImage> img);

private:
    QNetworkAccessManager *networkManager;

    QSize imageRescaleSize;
    Settings *settings;

    QList<std::tuple<QString, const char *, const char *>> customHeaders;

    QMap<QString, QWeakPointer<DownloadFileJob>> fileDownloads;
    QString fixUrl(const QString &url);
};

#endif  // DOWNLOADMANAGER_H
