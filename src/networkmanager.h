#ifndef DOWNLOADMANAGER_H
#define DOWNLOADMANAGER_H

#include <QNetworkReply>
#include <QPointer>
#include <QThread>

#include <atomic>

#include "downloadbufferjob.h"
#include "downloadfilejob.h"
#include "downloadimageandrescalejob.h"
#include "downloadstringjob.h"
#include "imageprocessor.h"
#include "settings.h"

class NetworkManager : public QObject
{
    Q_OBJECT

public:
    explicit NetworkManager(QObject *parent = nullptr);
    ~NetworkManager();

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

    // Blocks for up to 2 s when there is no connection.
    bool checkInternetConnection();

    // True while downloads are running or downloaded images are being processed.
    bool isBusy() const;

    // Connects on the WiFi thread, the WiFi scripts take seconds (half a minute without a DHCP answer).
    // Emits wifiConnectFinished when done.
    void connectWifiAsync();
    bool isConnectingWifi() const;
    // Waits for a connect that is still running, which would bring the WiFi up again.
    bool disconnectWifi();

    // The url a download of url is known by (without a decryption key, with a scheme).
    QString downloadUrl(const QString &url) const;
    // A download of the file is running (or waiting for a retry).
    bool isDownloading(const QString &url) const;

    static void loadCertificates(const QString &certsPath);
    bool urlExists(const QString &url);

    // written on the WiFi thread
    std::atomic<bool> connected;

signals:
    // emitted on the WiFi thread
    void connectionStatusChanged(bool connected);
    void wifiConnectFinished(bool connected);
    void activity();
    void downloadedImage(const QString &path, QSharedPointer<QImage> img);

private:
    QNetworkAccessManager *networkManager;
    ImageProcessor *imageProcessor;

    QSize imageRescaleSize;
    Settings *settings;

    QList<std::tuple<QString, const char *, const char *>> customHeaders;

    QMap<QString, QWeakPointer<DownloadFileJob>> fileDownloads;

    // The WiFi scripts always run on the same thread, the platform plugin runs them with a QProcess that
    // belongs to the thread that used it first.
    QThread wifiThread;
    QObject *wifiWorker;
    std::atomic<bool> connectingWifi;

    bool connectWifi();
    QString fixUrl(const QString &url) const;
    QList<std::tuple<const char *, const char *>> applicableCustomHeaders(const QString &url) const;
    void releaseFileDownload(const QString &url);
};

#endif  // DOWNLOADMANAGER_H
