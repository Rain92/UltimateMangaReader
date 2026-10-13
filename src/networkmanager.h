#pragma once

#include <QFuture>
#include <QNetworkReply>
#include <QPointer>
#include <QSslCertificate>
#include <QThread>

#include <atomic>
#include <functional>

#include "downloadbufferjob.h"
#include "downloadfilejob.h"
#include "downloadimageandrescalejob.h"
#include "downloadstringjob.h"
#include "imagelink.h"
#include "imageprocessor.h"
#include "settings.h"

// Calls beforeRequest before every request it sends.
class NetworkAccessManager : public QNetworkAccessManager
{
public:
    NetworkAccessManager(std::function<void()> beforeRequest, QObject *parent)
        : QNetworkAccessManager(parent), beforeRequest(beforeRequest)
    {
    }

protected:
    QNetworkReply *createRequest(Operation op, const QNetworkRequest &request, QIODevice *outgoingData) override
    {
        beforeRequest();
        return QNetworkAccessManager::createRequest(op, request, outgoingData);
    }

private:
    std::function<void()> beforeRequest;
};

// The headers that the requests to some hosts need, e.g. the Referer of an image host.
class HeaderRules
{
public:
    // For the requests to domain and its subdomains. Later rules for the same header replace earlier ones.
    void add(const QString &domain, const QByteArray &name, const QByteArray &value);
    HttpHeaders forUrl(const QString &url) const;

private:
    struct Rule
    {
        QString domain;
        HttpHeader header;
    };
    QList<Rule> rules;
};

class NetworkManager : public QObject
{
    Q_OBJECT

public:
    explicit NetworkManager(QObject *parent = nullptr);
    ~NetworkManager();

    QNetworkAccessManager *networkAccessManager();

    // The requests get the headers (besides a browser's User-Agent).
    QSharedPointer<DownloadStringJob> downloadAsString(const QString &url, int timeout = 6000,
                                                       const HttpHeaders &headers = {});
    QSharedPointer<DownloadBufferJob> downloadToBuffer(const QString &url, int timeout = 6000,
                                                       const HttpHeaders &headers = {});
    // A download into a file that is running already is reused.
    QSharedPointer<DownloadFileJob> downloadAsFile(const QString &url, const QString &localPath,
                                                   const HttpHeaders &headers = {});
    // Downloads a page and saves it processed for the screen, with the headers of image. A download into the
    // same file that is running already is reused.
    QSharedPointer<DownloadFileJob> downloadAsScaledImage(const ImageLink &image, const QString &localPath);

    void setDownloadSettings(const QSize &size, Settings *settings);

    void addCookie(const QString &domain, const QByteArray &name, const QByteArray &value);

    // Blocks for up to 2 s when there is no connection.
    bool checkInternetConnection();

    // True while downloads are running or downloaded images are being processed.
    bool isBusy() const;

    // Connects on the WiFi thread, the WiFi scripts take seconds (half a minute without a DHCP answer, a minute
    // when the second attempt fails too). Emits wifiConnectFinished when done.
    void connectWifiAsync();
    bool isConnectingWifi() const;
    // Waits for a connect that is still running, which would bring the WiFi up again.
    bool disconnectWifi();

    // A download into the file is running (or waiting for a retry).
    bool isDownloading(const QString &localPath) const;

    // Reads the sizes of the images from the start of their files (in parallel, blocking with an event loop
    // for at most timeout ms). Images that couldn't be loaded or read have an invalid size.
    // timedOut gets the indices of the urls that didn't answer in time (the other failures are the servers')
    QList<QSize> probeImageSizes(const QStringList &urls, int timeout = 10000, QList<int> *timedOut = nullptr,
                                 const std::function<HttpHeaders(const QString &)> &headersFor = nullptr);

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

    // Parsing the CA certificates takes a third of a second on the device, they are loaded in the background
    // and the first request waits for them.
    QFuture<QList<QSslCertificate>> certificates;
    bool certificatesApplied;
    void ensureCertificates();
    static QList<QSslCertificate> loadCertificates(const QString &certsPath);

    QSize imageRescaleSize;
    Settings *settings;

    // the running file downloads by their file
    QMap<QString, QWeakPointer<DownloadFileJob>> fileDownloads;
    QSharedPointer<DownloadFileJob> runningFileDownload(const QString &localPath);
    QSharedPointer<DownloadFileJob> startFileDownload(DownloadFileJob *job);

    // The WiFi scripts always run on the same thread, the platform plugin runs them with a QProcess that
    // belongs to the thread that used it first.
    QThread wifiThread;
    QObject *wifiWorker;
    std::atomic<bool> connectingWifi;
    // a suspend waits for the running connect, it shouldn't start another attempt
    std::atomic<bool> disconnectRequested;
    // the WiFi was switched on (by us, or it was connected): it has to be switched off for a suspend even when
    // there's no connection (no known network around, a failed check)
    std::atomic<bool> wifiEnabled;

    bool connectWifi();
    QString fixUrl(const QString &url) const;
    void releaseFileDownload(const QString &localPath);
};
