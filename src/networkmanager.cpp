#include "networkmanager.h"

#include <QtConcurrent>

#include "greyscaleimage.h"
#include "utils.h"

#ifdef KOBO
#include "koboplatformfunctions.h"
#endif

NetworkManager::NetworkManager(QObject *parent)
    : QObject(parent),
      connected(false),
      networkManager(new NetworkAccessManager([this]() { ensureCertificates(); }, this)),
      imageProcessor(new ImageProcessor(this)),
      certificates(),
      certificatesApplied(false),
      settings(nullptr),
      customHeaders(),
      fileDownloads(),
      wifiThread(),
      wifiWorker(new QObject()),
      connectingWifi(false),
      disconnectRequested(false),
      wifiEnabled(false)
{
    wifiThread.setObjectName("wifi");
    wifiWorker->moveToThread(&wifiThread);
    connect(&wifiThread, &QThread::finished, wifiWorker, &QObject::deleteLater);
    wifiThread.start();

#ifdef KOBO
    // the certificates are deployed with Qt (the launcher sets QTPATH)
    QString sslCertPath = QLibraryInfo::path(QLibraryInfo::PrefixPath) + "/lib/ssl_certs";
    if (qEnvironmentVariableIsSet("QTPATH"))
        sslCertPath = qEnvironmentVariable("QTPATH") + "/lib/ssl_certs";

    // only while the cpu is idle: the start of the app would wait for it otherwise (single core)
    QThread *guiThread = QThread::currentThread();
    certificates = QtConcurrent::run(
        [sslCertPath, guiThread]()
        {
            // waiting for a task that didn't start yet runs it on the waiting thread
            bool background = QThread::currentThread() != guiThread;
            if (background)
                setIdleThreadPriority(true);
            auto list = loadCertificates(sslCertPath);
            if (background)
                setIdleThreadPriority(false);
            return list;
        });
#endif
}

NetworkManager::~NetworkManager()
{
    // a running connect finishes first
    wifiThread.quit();
    wifiThread.wait();
}

QNetworkAccessManager *NetworkManager::networkAccessManager()
{
    return this->networkManager;
}

void NetworkManager::connectWifiAsync()
{
    if (connectingWifi.exchange(true))
        return;
    disconnectRequested = false;

    QMetaObject::invokeMethod(
        wifiWorker,
        [this]()
        {
            bool ok = connectWifi();
            connectingWifi = false;
            QMetaObject::invokeMethod(
                this, [this, ok]() { emit wifiConnectFinished(ok); }, Qt::QueuedConnection);
        },
        Qt::QueuedConnection);
}

bool NetworkManager::isConnectingWifi() const
{
    return connectingWifi;
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
        wifiEnabled = true;
        // The association can take longer than the DHCP client waits (30 s), e.g. after Nickel was quit while
        // it switched the WiFi off. It goes on meanwhile, the second attempt then gets the address in seconds.
        for (int attempt = 0; attempt < 2 && !connected && !disconnectRequested; attempt++)
        {
            if (attempt > 0)
                qDebug() << "Connecting to Wifi again...";
            KoboPlatformFunctions::enableWiFiConnection();
            checkInternetConnection();
        }
        qDebug() << "Connected" << (connected ? "successfully" : "unsuccessfully") << "in" << t.elapsed()
                 << "ms.";
#endif
    }
    else
    {
        wifiEnabled = true;
        qDebug() << "Already connected to Wifi.";
    }

    return connected;
}

bool NetworkManager::disconnectWifi()
{
    disconnectRequested = true;
    QMetaObject::invokeMethod(
        wifiWorker,
        [this]()
        {
            if (!connected && !wifiEnabled)
                return;

#ifdef KOBO
            QElapsedTimer t;
            t.start();
            KoboPlatformFunctions::disableWiFiConnection();
            qDebug() << "Disconnected in" << t.elapsed() << "ms.";
#endif

            wifiEnabled = false;
            if (connected.exchange(false))
                emit connectionStatusChanged(false);
        },
        Qt::BlockingQueuedConnection);

    return true;
}

bool NetworkManager::checkInternetConnection()
{
    bool oldstatus = connected;
#ifdef KOBO
    qDebug() << "Checking internet connection...";
    connected = KoboPlatformFunctions::testInternetConnection(2);
#else
    connected = true;
#endif
    if (oldstatus != connected)
        emit connectionStatusChanged(connected);

    return connected;
}

bool NetworkManager::isBusy() const
{
    return DownloadJobBase::activeReplies() > 0 || imageProcessor->isBusy();
}

QString NetworkManager::downloadUrl(const QString &url) const
{
    return fixUrl(url.section('|', 0, 0));
}

bool NetworkManager::isDownloading(const QString &url) const
{
    auto job = fileDownloads.value(downloadUrl(url)).toStrongRef();
    return job && !job->isCompleted && !job->hasFailed();
}

QString NetworkManager::fixUrl(const QString &url) const
{
    // protocol relative, all sources are https sites
    if (url.startsWith("//"))
        return "https:" + url;

    return url;
}

void NetworkManager::releaseFileDownload(const QString &url)
{
    // only remove the entry if it wasn't replaced by a newer job for the same url
    auto it = fileDownloads.find(url);
    if (it != fileDownloads.end() && it.value().isNull())
        fileDownloads.erase(it);
}

QList<std::tuple<const char *, const char *>> NetworkManager::applicableCustomHeaders(const QString &url) const
{
    QList<std::tuple<const char *, const char *>> headers;

    for (const auto &[domain, name, value] : std::as_const(customHeaders))
        if (url.contains(domain))
            headers.append(std::tuple<const char *, const char *>(name, value));

    return headers;
}

QSharedPointer<DownloadStringJob> NetworkManager::downloadAsString(const QString &url, int timeout)
{
    auto urlf = fixUrl(url);

    qDebug() << "Downloading as string:" << urlf;

    auto job = QSharedPointer<DownloadStringJob>(
        new DownloadStringJob(networkManager, urlf, timeout, applicableCustomHeaders(urlf)),
        &QObject::deleteLater);

    job->start();

    emit activity();
    return job;
}

QSharedPointer<DownloadBufferJob> NetworkManager::downloadToBuffer(const QString &url, int timeout)
{
    auto urlf = fixUrl(url);

    qDebug() << "Downloading to buffer:" << urlf;

    auto job = QSharedPointer<DownloadBufferJob>(
        new DownloadBufferJob(networkManager, urlf, timeout, applicableCustomHeaders(urlf)),
        &QObject::deleteLater);

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
        // don't hand out failed jobs, start a new download instead
        if (job && !job->hasFailed())
            return job;
        else
            fileDownloads.remove(urlf);
    }

    qDebug() << "Downloading as file:" << urlf;

    QPointer<NetworkManager> self(this);
    auto job = QSharedPointer<DownloadFileJob>(
        new DownloadFileJob(networkManager, urlf, localPath, applicableCustomHeaders(urlf)),
        [self](DownloadFileJob *j)
        {
            if (self)
                self->releaseFileDownload(j->originalUrl);
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
    // url|xor:<hex key>|append:<url of the rest of the page>
    auto parts = url.split('|');
    QString urlf = fixUrl(parts.takeFirst());
    EncryptionDescriptor ed;
    QString appendUrl;
    for (const auto &part : std::as_const(parts))
    {
        if (part.startsWith("xor:"))
        {
            ed.type = XorEncryption;
            ed.key = hexstr2array(part.mid(4));
        }
        else if (part.startsWith("append:"))
        {
            appendUrl = fixUrl(part.mid(7));
        }
        else
        {
            qDebug() << "Error: Unsupported image url option:" << part;
        }
    }

    if (fileDownloads.contains(urlf))
    {
        auto job = qSharedPointerCast<DownloadScaledImageJob>(fileDownloads.value(urlf).toStrongRef());
        // don't hand out failed jobs, start a new download instead
        if (job && !job->hasFailed())
            return job;
        else
            fileDownloads.remove(urlf);
    }

    qDebug() << "Downloading as image:" << urlf;

    QPointer<NetworkManager> self(this);
    auto job = QSharedPointer<DownloadFileJob>(
        new DownloadScaledImageJob(networkManager, imageProcessor, urlf, localPath, imageRescaleSize,
                                   settings, applicableCustomHeaders(urlf), ed, appendUrl),
        [self](DownloadScaledImageJob *j)
        {
            if (self)
                self->releaseFileDownload(j->originalUrl);
            j->deleteLater();
        });

    job->start();

    fileDownloads.insert(urlf, job.toWeakRef());

    emit activity();

    if (job->isCompleted || job->errorString != "")
        return job;

    // keep the job alive until it finished, even if the caller drops it
    auto sjob = qSharedPointerCast<DownloadScaledImageJob>(job);
    auto keepAlive = QSharedPointer<QSharedPointer<DownloadScaledImageJob>>::create(sjob);
    connect(sjob.get(), &DownloadScaledImageJob::completed, this,
            [keepAlive, this]()
            {
                auto j = *keepAlive;
                keepAlive->clear();
                if (j && j->resultImage)
                    emit downloadedImage(j->filepath, {j->resultImage});
            });
    connect(sjob.get(), &DownloadJobBase::downloadError, this, [keepAlive]() { keepAlive->clear(); });

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
    c.setExpirationDate(QDateTime::currentDateTime().addYears(10));

    networkManager->cookieJar()->insertCookie(c);
}

void NetworkManager::addSetCustomRequestHeader(const QString &domain, const char *key, const char *value)
{
    customHeaders.append({domain, key, value});
}

QList<QSslCertificate> NetworkManager::loadCertificates(const QString &certsPath)
{
    // loads the TLS backend
    QSslConfiguration::defaultConfiguration();

    return QSslCertificate::fromPath(certsPath + "/cacert.pem", QSsl::Pem);
}

void NetworkManager::ensureCertificates()
{
#ifdef KOBO
    if (certificatesApplied)
        return;
    certificatesApplied = true;

    auto sslConfig = QSslConfiguration::defaultConfiguration();
    sslConfig.setProtocol(QSsl::AnyProtocol);

    QList<QSslCertificate> caCertificates = certificates.result();
    if (caCertificates.size() != 0)
        sslConfig.setCaCertificates(caCertificates);
    else
        qDebug() << "Couldn't load SSL certificates";

    QSslConfiguration::setDefaultConfiguration(sslConfig);
#endif
}

QList<QSize> NetworkManager::probeImageSizes(const QStringList &urls, int timeout, QList<int> *timedOut)
{
    // a few KB hold the size of any WebP, PNG and of JPEGs without large metadata
    const int headerBytes = 4096;

    QList<QSize> sizes;
    for (int i = 0; i < urls.size(); i++)
        sizes.append(QSize());
    QList<QNetworkReply *> replies;
    int running = 0;

    QEventLoop loop;
    for (int i = 0; i < urls.size(); i++)
    {
        auto urlf = fixUrl(urls[i]);
        QNetworkRequest request(urlf);
        request.setRawHeader("User-Agent",
                             "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) "
                             "Chrome/130.0.0.0 Safari/537.36");
        for (const auto &[name, value] : applicableCustomHeaders(urlf))
            request.setRawHeader(name, value);
        request.setRawHeader("Range", "bytes=0-" + QByteArray::number(headerBytes - 1));

        auto reply = networkManager->get(request);
        DownloadJobBase::trackReply(reply);
        replies.append(reply);
        running++;

        // servers that ignore the range send everything, the start is enough
        auto done = [this, reply, i, &sizes, &running, &loop]()
        {
            if (reply->property("probed").toBool())
                return;

            int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            bool finished = reply->isFinished();
            if (!finished && reply->bytesAvailable() < headerBytes)
                return;

            reply->setProperty("probed", true);
            if (reply->error() == QNetworkReply::NoError && (status == 200 || status == 206))
            {
                QSize size;
                if (readImageSize(reply->read(headerBytes), size))
                    sizes[i] = size;
            }
            if (!finished)
                reply->abort();

            if (--running == 0)
                loop.quit();
        };
        connect(reply, &QNetworkReply::readyRead, &loop, done);
        connect(reply, &QNetworkReply::finished, &loop, done);
    }

    if (running > 0)
    {
        QTimer::singleShot(timeout, &loop, &QEventLoop::quit);
        loop.exec();
    }

    for (int i = 0; i < replies.size(); i++)
    {
        auto reply = replies[i];
        if (timedOut && !reply->property("probed").toBool())
            timedOut->append(i);
        reply->disconnect(&loop);
        reply->abort();
        reply->deleteLater();
    }

    emit activity();
    return sizes;
}
