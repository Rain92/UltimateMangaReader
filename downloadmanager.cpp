#include <QUrl>
#include <QDir>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QtCore>
#include <QList>
#include <QTime>

#include "downloadmanager.h"

#ifdef KOBO
#include "../koboplatformintegrationplugin/koboplatformfunctions.h"
#endif

DownloadManager::DownloadManager(QObject *parent)
    : QObject(parent)
{
    networkmanager = new QNetworkAccessManager(parent);
    fileDownloads = new QMap<QString, DownloadFileJob *>();
    cookies = new CCookieJar();
    networkmanager->setCookieJar(cookies);
#ifdef KOBO
    loadCertificates("/mnt/onboard/.adds/qt-5.14.1-kobo/ssl_certs");
#endif
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

DownloadStringJob *DownloadManager::downloadAsString(const QString &url, int timeout)
{
    if (!connected())
        connect();

    qDebug() << "downloading:" << url ;
    DownloadStringJob *job = new DownloadStringJob(this, networkmanager, url, timeout);

    job->start();
    return job;
}


DownloadFileJob *DownloadManager::downloadAsFile(const QString &url, const QString &localPath, bool usedownloadmap)
{
    if (!connected())
        connect();

    if (usedownloadmap && fileDownloads->contains(url))
    {
        return fileDownloads->value(url);
    }
    else
    {
        DownloadFileJob *job = new DownloadFileJob(this, networkmanager, url, localPath);

        if (usedownloadmap)
            fileDownloads->insert(url, job);

        return job;
    }
}

DownloadFileJob *DownloadManager::downloadAsScaledImage(const QString &url, const QString &localPath)
{
    if (!connected())
        connect();

    if (fileDownloads->contains(url))
    {
        return fileDownloads->value(url);
    }
    else
    {
        DownloadFileJob *job = new DownloadScaledImageJob(this, networkmanager, url, localPath, imagewidth, imageheight);

        fileDownloads->insert(url, job);

        return job;
    }
}


void DownloadManager::setImageSize(int width, int height)
{
    imagewidth = width;
    imageheight = height;
}


bool DownloadManager::awaitAllFileDownloads(int timeout)
{
    QElapsedTimer timer;
    timer.start();

    foreach (DownloadFileJob *job, *fileDownloads)
    {
        int remaining = timeout - timer.elapsed();
        if (remaining < 0 || !job->await(remaining))
            return false;

        delete job;
    }
    return true;
}


void DownloadManager::addCookie(const QString &domain, const char *key, const char *value)
{
    for (int i = 0; i < cookies->cookies.count(); i++)
        if (cookies->cookies[i].name() == QByteArray(key) && cookies->cookies[i].domain() == domain)
        {
            cookies->cookies.removeAt(i);
            break;
        }

    cookies->addCookie(domain, key, value);
}

void DownloadManager::loadCertificates(const QString &certsPath)
{

    auto sslConfig = QSslConfiguration::defaultConfiguration();
    sslConfig.setProtocol(QSsl::SecureProtocols);
    auto certs = sslConfig.caCertificates();

    QDir dir(certsPath);
    dir.setFilter(QDir::Files | QDir::NoDotAndDotDot | QDir::Hidden);
    dir.setNameFilters({"*.cer", "*.crt", "*.pem"});
    foreach( auto& entry, dir.entryInfoList() )
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
