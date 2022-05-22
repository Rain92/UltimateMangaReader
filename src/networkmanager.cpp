#include "networkmanager.h"

#include "utils.h"

#ifdef KOBO
#include "koboplatformfunctions.h"
#endif

NetworkManager::NetworkManager(QObject *parent)
    : QObject(parent),
      connected(false),
      networkManager(new QNetworkAccessManager(this)),
      settings(nullptr),
      customHeaders(),
      fileDownloads()
{
#ifdef KOBO
    QString sslCertPath = "/mnt/onboard/.adds/qt-linux-5.15-kde-kobo/lib/ssl_certs";
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
    if (QNetworkProxy::applicationProxy().type() != QNetworkProxy::NoProxy)
    {
        QNetworkProxy proxy;
        proxy.setType(QNetworkProxy::applicationProxy().type());
        proxy.setHostName(QNetworkProxy::applicationProxy().hostName());
        proxy.setPort(QNetworkProxy::applicationProxy().port());
        QNetworkProxy::setApplicationProxy(proxy);
        qDebug() << "***Using proxy:\t" << QNetworkProxy::applicationProxy().hostName() << ":"
                 << QNetworkProxy::applicationProxy().port()
                 << " type:" << QNetworkProxy::applicationProxy().type();
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
        qDebug() << "Checking internet connection...";
        connected = KoboPlatformFunctions::KoboPlatformFunctions::testInternetConnection(2);
    }
    catch (const QException &e)
    {
        qDebug() << "Error while checking internet connection:" << e.what();
    }
#else
    connected = true;
#endif
    if (oldstatus != connected)
        emit connectionStatusChanged(connected);

    return connected;
}

QString NetworkManager::fixUrl(const QString &url)
{
    if (url.startsWith("//"))
        return "http:" + url;

    return url;
}

QSharedPointer<DownloadStringJob> NetworkManager::downloadAsString(const QString &url, int timeout,
                                                                   const QByteArray &postData)
{
    auto urlf = fixUrl(url);

    qDebug() << "Downloading as string:" << urlf;

    auto job = QSharedPointer<DownloadStringJob>(
        new DownloadStringJob(networkManager, urlf, timeout, postData), &QObject::deleteLater);

    job->start();

    emit activity();
    return job;
}

QSharedPointer<DownloadBufferJob> NetworkManager::downloadToBuffer(const QString &url, int timeout,
                                                                   const QByteArray &postData)
{
    auto urlf = fixUrl(url);

    qDebug() << "Downloading to buffer:" << urlf;

    auto job = QSharedPointer<DownloadBufferJob>(
        new DownloadBufferJob(networkManager, urlf, timeout, postData), &QObject::deleteLater);

    job->start();

    emit activity();
    return job;
}

QSharedPointer<DownloadFileJob> NetworkManager::downloadAsFile(const QString &url, const QString &localPath)
{
    auto urlf = fixUrl(url);

    if (fileDownloads.contains(urlf))
    {
        auto job = fileDownloads.value(urlf).toStrongRef();
        if (job)
            return job;
        else
            fileDownloads.remove(urlf);
    }

    qDebug() << "Downloading as file:" << urlf;

    QList<std::tuple<const char *, const char *>> applicableCustomHeaders;

    for (const auto &[domain, name, value] : qAsConst(customHeaders))
        if (urlf.contains(domain))
            applicableCustomHeaders.append(std::tuple<const char *, const char *>(name, value));

    auto job = QSharedPointer<DownloadFileJob>(
        new DownloadFileJob(networkManager, urlf, localPath, applicableCustomHeaders),
        [this](DownloadFileJob *j)
        {
            this->fileDownloads.remove(j->originalUrl);
            j->deleteLater();
        });

    job->start();

    fileDownloads.insert(urlf, job.toWeakRef());

    emit activity();
    return job;
}

QSharedPointer<DownloadFileJob> NetworkManager::downloadAsScaledImage(const QString &url,
                                                                      const QString &localPath)
{
    QString urlf;
    EncryptionDescriptor ed;
    if (url.contains('|'))
    {
        auto split = url.split('|');
        urlf = split[0];

        if (split[1].startsWith("xor:"))
        {
            ed.type = XorEncryption;
            ed.key = hexstr2array(split[1].mid(4));
        }
        else
            qDebug() << "Error: Encryption not supported!";
    }
    else
        urlf = url;

    urlf = fixUrl(urlf);

    if (fileDownloads.contains(urlf))
    {
        auto job = qSharedPointerCast<DownloadScaledImageJob>(fileDownloads.value(urlf).toStrongRef());
        if (job)
            return job;
        else
            fileDownloads.remove(urlf);
    }

    qDebug() << "Downloading as image:" << urlf;

    QList<std::tuple<const char *, const char *>> applicableCustomHeaders;

    for (const auto &[domain, name, value] : qAsConst(customHeaders))
        if (urlf.contains(domain))
            applicableCustomHeaders.append(std::tuple<const char *, const char *>(name, value));

    auto job = QSharedPointer<DownloadFileJob>(
        new DownloadScaledImageJob(networkManager, urlf, localPath, imageRescaleSize, settings,
                                   applicableCustomHeaders, ed),
        [this](DownloadScaledImageJob *j)
        {
            this->fileDownloads.remove(j->originalUrl);
            j->deleteLater();
        });

    job->start();

    fileDownloads.insert(urlf, job.toWeakRef());

    emit activity();
    auto sjob = qSharedPointerCast<DownloadScaledImageJob>(job);
    connect(sjob.get(), &DownloadScaledImageJob::completed, this,
            [sjob, this]() mutable
            {
                if (sjob->resultImage)
                    emit downloadedImage(sjob->filepath, {sjob->resultImage});
                sjob.clear();
            });
    return job;
}

void NetworkManager::setDownloadSettings(const QSize &size, Settings *settings)
{
    imageRescaleSize = size;
    this->settings = settings;
}

void NetworkManager::addCookie(const QString &domain, const char *key, const char *value)
{
    QNetworkCookie c = QNetworkCookie(QByteArray(key), QByteArray(value));
    c.setDomain(domain);
    c.setExpirationDate(QDateTime::currentDateTime().addDays(1));

    networkManager->cookieJar()->insertCookie(c);
}

void NetworkManager::addSetCustomRequestHeader(const QString &domain, const char *key, const char *value)
{
    customHeaders.append({domain, key, value});
}

void NetworkManager::loadCertificates(const QString &certsPath)
{
    auto sslConfig = QSslConfiguration::defaultConfiguration();
    sslConfig.setProtocol(QSsl::AnyProtocol);

    QList<QSslCertificate> caCertificates = QSslCertificate::fromPath(certsPath + "/cacert.pem", QSsl::Pem);
    if (caCertificates.size() != 0)
    {
        sslConfig.setCaCertificates(caCertificates);
    }
    else
    {
        qDebug() << "Couldn't load SSL certificates";
    }

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
