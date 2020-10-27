#include "networkmanager.h"

#include "utils.h"

#ifdef KOBO
#include "koboplatformfunctions.h"
#endif

NetworkManager::NetworkManager(QObject *parent)
    : QObject(parent),
      connected(false),
      networkManager(new QNetworkAccessManager(this)),
      cookies(),
      customHeaders(),
      fileDownloads()
{
    networkManager->setCookieJar(&cookies);
#ifdef KOBO
    QString sslCertPath = "/mnt/onboard/.adds/qt-linux-5.15.0-kobo/lib/ssl_certs";
    if (qEnvironmentVariableIsSet("QTPATH"))
        sslCertPath = qEnvironmentVariable("QTPATH") + "/lib/ssl_certs";
    loadCertificates(sslCertPath);
#endif
}

QNetworkAccessManager *NetworkManager::networkAccessManager()
{
    return this->networkManager;
}

bool NetworkManager::connectWifi()
{
#ifdef KOBO
    if (QNetworkProxy::applicationProxy().type()!=QNetworkProxy::NoProxy)
    {
        QNetworkProxy proxy;
        proxy.setType(QNetworkProxy::applicationProxy().type());
        proxy.setHostName(QNetworkProxy::applicationProxy().hostName());
        proxy.setPort(QNetworkProxy::applicationProxy().port());
        QNetworkProxy::setApplicationProxy(proxy);
        qDebug() << "***Using proxy:\t" << QNetworkProxy::applicationProxy().hostName() << ":" << QNetworkProxy::applicationProxy().port() << " type:" << QNetworkProxy::applicationProxy().type();
    }
#endif
    if (!checkInternetConnection())
    {
#ifdef KOBO
        qDebug() << "Connecting to Wifi...";
        QElapsedTimer t;
        t.start();
        try
        {
            KoboPlatformFunctions::enableWiFiConnection();
            checkInternetConnection();
        }
        catch (const QException &e)
        {
            qDebug() << "Error while connection to internet:" << e.what();
        }
        qDebug() << "Connected" << (connected ? "successfully" : "unsuccessfully") << "in" << t.elapsed()
                 << "ms.";
#endif
    }
    else
    {
        qDebug() << "Already connected to Wifi.";
    }

    return connected;
}

bool NetworkManager::disconnectWifi()
{
    if (!connected)
        return true;

#ifdef KOBO
    QElapsedTimer t;
    t.start();
    KoboPlatformFunctions::disableWiFiConnection();
    qDebug() << "Disconnected in" << t.elapsed() << "ms.";
#endif

    connected = false;

    return true;
}

bool NetworkManager::checkInternetConnection()
{
    bool oldstatus = connected;
#ifdef KOBO
    try
    {
        connected = KoboPlatformFunctions::KoboPlatformFunctions::testInternetConnection(2);
    }
    catch (const QException &e)
    {
        qDebug() << "Error while checkin internet connection:" << e.what();
    }
#else
    connected = true;
#endif
    if (oldstatus != connected)
        emit connectionStatusChanged(connected);

    return connected;
}

QSharedPointer<DownloadStringJob> NetworkManager::downloadAsString(const QString &url, int timeout,
                                                                   const QByteArray &postData)
{
    qDebug() << "Downloading as string:" << url;

    auto job = QSharedPointer<DownloadStringJob>(
        new DownloadStringJob(networkManager, url, timeout, postData), &QObject::deleteLater);

    job->start();

    emit activity();
    return job;
}

QSharedPointer<DownloadFileJob> NetworkManager::downloadAsFile(const QString &url, const QString &localPath)
{
    if (fileDownloads.contains(url))
    {
        auto job = fileDownloads.value(url).toStrongRef();
        if (job)
            return job;
        else
            fileDownloads.remove(url);
    }

    qDebug() << "Downloading as file:" << url;

    auto job = QSharedPointer<DownloadFileJob>(new DownloadFileJob(networkManager, url, localPath),
                                               [this](DownloadFileJob *j) {
                                                   this->fileDownloads.remove(j->originalUrl);
                                                   j->deleteLater();
                                               });

    job->start();

    fileDownloads.insert(url, job.toWeakRef());

    emit activity();
    return job;
}

QSharedPointer<DownloadFileJob> NetworkManager::downloadAsScaledImage(const QString &url,
                                                                      const QString &localPath)
{
    if (fileDownloads.contains(url))
    {
        auto job = fileDownloads.value(url).toStrongRef();
        if (job)
            return job;
        else
            fileDownloads.remove(url);
    }

    qDebug() << "Downloading as image:" << url;

    QList<std::tuple<const char *, const char *>> applicableCustomHeaders;

    for (const auto &[domain, name, value] : customHeaders)
        if (url.contains(domain))
            applicableCustomHeaders.append(std::tuple<const char *, const char *>(name, value));

    auto job = QSharedPointer<DownloadFileJob>(
        new DownloadScaledImageJob(networkManager, url, localPath, imageRescaleSize,
                                   settings->doublePageFullscreen, settings->trimPages,
                                   applicableCustomHeaders),
        [this](DownloadScaledImageJob *j) {
            this->fileDownloads.remove(j->originalUrl);
            j->deleteLater();
        });

    job->start();

    fileDownloads.insert(url, job.toWeakRef());

    emit activity();
    return job;
}

void NetworkManager::setDownloadSettings(const QSize &size, Settings *settings)
{
    imageRescaleSize = size;
    this->settings = settings;
}

void NetworkManager::addCookie(const QString &domain, const char *key, const char *value)
{
    for (int i = 0; i < cookies.cookies.count(); i++)
        if (cookies.cookies[i].name() == QByteArray(key) && cookies.cookies[i].domain() == domain)
        {
            cookies.cookies.removeAt(i);
            break;
        }

    cookies.addCookie(domain, key, value);
}

void NetworkManager::addSetCustomRequestHeader(const QString &domain, const char *key, const char *value)
{
    customHeaders.append({domain, key, value});
}

void NetworkManager::loadCertificates(const QString &certsPath)
{
    auto sslConfig = QSslConfiguration::defaultConfiguration();
    sslConfig.setProtocol(QSsl::SecureProtocols);
    auto certs = sslConfig.caCertificates();

    QDir dir(certsPath);
    dir.setFilter(QDir::Files | QDir::NoDotAndDotDot | QDir::Hidden);
    dir.setNameFilters({"*.cer", "*.crt", "*.pem"});
    foreach (auto &entry, dir.entryInfoList())
    {
        QFile cafile(entry.filePath());
        cafile.open(QIODevice::ReadOnly);
        QSslCertificate cert(&cafile);

        certs << cert;
        cafile.close();
    }

    sslConfig.setCaCertificates(certs);
    QSslConfiguration::setDefaultConfiguration(sslConfig);
}

bool NetworkManager::urlExists(const QString &url)
{
    QNetworkRequest request(url);

    auto reply = networkManager->head(request);

    awaitSignal(reply, {SIGNAL(finished())}, 2000);

    bool result = false;
    int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    if (status == 200)
        result = true;

    reply->deleteLater();

    emit activity();
    return result;
}
