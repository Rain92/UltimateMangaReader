#include <QUrl>
#include <QDir>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QtCore>
#include <QList>
#include <QTime>

#include "downloadmanager.h"

#ifdef KOBO
#include "wifidialog.h"
#include "Platform.h"
#include "QtUtils.h"
#endif

DownloadManager::DownloadManager(QObject *parent)
    : QObject(parent)
{
    manager = new QNetworkAccessManager(parent);
    fileDownloads = new QMap<QString, DownloadFileJob *>();
    cookies = new CCookieJar();
    manager->setCookieJar(cookies);
}

bool DownloadManager::connect()
{
#ifndef WINDOWS
    return activateNetwork(static_cast<QWidget *>(this->parent()));
#endif

    return true;
}

bool DownloadManager::connected()
{
#ifndef WINDOWS
    return Platform::get()->isNetworkActive();
#endif

    return true;
}

void DownloadManager::onActivity()
{
#ifndef WINDOWS
    Platform::get()->networkActivity();
#endif
}

DownloadStringJob *DownloadManager::downloadAsString(QString url, int timeout)
{
    if (!connected())
        connect();

    DownloadStringJob *job = new DownloadStringJob(this, manager, url, timeout);

    job->start();
    return job;
}


DownloadFileJob *DownloadManager::downloadAsFile(QString url, QString path, bool usedownloadmap)
{
    if (!connected())
        connect();

    if (usedownloadmap && fileDownloads->contains(url))
    {
        return fileDownloads->value(url);
    }
    else
    {
        DownloadFileJob *job = new DownloadFileJob(this, manager, url, path);

        if (usedownloadmap)
            fileDownloads->insert(url, job);

        return job;
    }
}

DownloadFileJob *DownloadManager::downloadAsScaledImage(QString url, QString path)
{
    if (!connected())
        connect();

    if (fileDownloads->contains(url))
    {
        return fileDownloads->value(url);
    }
    else
    {
        DownloadFileJob *job = new DownloadScaledImageJob(this, manager, url, path, imagewidth, imageheight);

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
    int start = QTime::currentTime().elapsed();

    foreach (DownloadFileJob *job, *fileDownloads)
    {
        int remaining = timeout - (QTime::currentTime().elapsed() - start);
        if (remaining < 0 || !job->await(remaining))
            return false;

        delete job;
    }
    return true;
}


void DownloadManager::addCookie(QString domain, const char *key, const char *value)
{
    for (int i = 0; i < cookies->cookies.count(); i++)
        if (cookies->cookies[i].name() == QByteArray(key) && cookies->cookies[i].domain() == domain)
        {
            cookies->cookies.removeAt(i);
            break;
        }

    cookies->addCookie(domain, key, value);
}
