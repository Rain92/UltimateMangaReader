#include "downloadmanager.h"

#include <QDir>
#include <QList>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTime>
#include <QUrl>

#ifdef KOBO
#include "../koboplatformintegrationplugin/koboplatformfunctions.h"
#endif

DownloadManager::DownloadManager(QObject *parent)
    : QObject(parent),
      networkmanager(new QNetworkAccessManager(this)),
      cookies(),
      fileDownloads()
{
    networkmanager->setCookieJar(&cookies);
#ifdef KOBO
    loadCertificates("/mnt/onboard/.adds/qt-5.14.1-kobo/lib/ssl_certs");
#endif
}

QNetworkAccessManager *DownloadManager::networkAccessManager()
{
    return this->networkmanager;
}

bool DownloadManager::connect()
{
#ifdef KOBO
    KoboPlatformFunctions::enableWiFiConnection();
    return KoboPlatformFunctions::testInternetConnection();
#endif

    return true;
}

bool DownloadManager::connected()
{
#ifdef KOBO
//    return Platform::get()->isNetworkActive();
#endif

    return true;
}

QSharedPointer<DownloadStringJob> DownloadManager::downloadAsString(
    const QString &url, int timeout)
{
    qDebug() << "downloading:" << url;

    auto job = QSharedPointer<DownloadStringJob>(
        new DownloadStringJob(this, networkmanager, url, timeout),
        &QObject::deleteLater);

    job->start();
    return job;
}

QSharedPointer<DownloadStringJob> DownloadManager::downloadAsStringPost(
    const QString &url, QByteArray *postdata, int timeout)
{
    qDebug() << "downloading:" << url;

    auto job = QSharedPointer<DownloadStringJob>(
        new DownloadStringJob(this, networkmanager, url, timeout, postdata),
        &QObject::deleteLater);

    job->start();
    return job;
}

QSharedPointer<DownloadFileJob> DownloadManager::downloadAsFile(
    const QString &url, const QString &localPath)
{
    if (fileDownloads.contains(url))
    {
        auto job = fileDownloads.value(url).toStrongRef();
        if (job)
            return job;
        else
            fileDownloads.remove(url);
    }

    auto job = QSharedPointer<DownloadFileJob>(
        new DownloadFileJob(this, networkmanager, url, localPath),
        [this](DownloadFileJob *j) {
            this->fileDownloads.remove(j->originalUrl);
            j->deleteLater();
        });

    job->start();

    fileDownloads.insert(url, job.toWeakRef());

    return job;
}

QSharedPointer<DownloadFileJob> DownloadManager::downloadAsScaledImage(
    const QString &url, const QString &localPath)
{
    if (fileDownloads.contains(url))
    {
        auto job = fileDownloads.value(url).toStrongRef();
        if (job)
            return job;
        else
            fileDownloads.remove(url);
    }
    auto job = QSharedPointer<DownloadFileJob>(
        new DownloadScaledImageJob(this, networkmanager, url, localPath,
                                   imageRescaleSize),
        [this](DownloadFileJob *j) {
            this->fileDownloads.remove(j->originalUrl);
            j->deleteLater();
        });

    job->start();

    fileDownloads.insert(url, job.toWeakRef());

    return job;
}

void DownloadManager::setImageRescaleSize(QSize size)
{
    imageRescaleSize = size;
}

void DownloadManager::addCookie(const QString &domain, const char *key,
                                const char *value)
{
    for (int i = 0; i < cookies.cookies.count(); i++)
        if (cookies.cookies[i].name() == QByteArray(key) &&
            cookies.cookies[i].domain() == domain)
        {
            cookies.cookies.removeAt(i);
            break;
        }

    cookies.addCookie(domain, key, value);
}

void DownloadManager::loadCertificates(const QString &certsPath)
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
