#pragma once

#include "utils.h"

// dirstructure (the mangas and their chapters are identified by their urls):
//   /cache/ -> favorites.dat settings.dat cacheversion
//   /cache/mangalists/ -> <source>_mangalist.dat
//   /cache/<source>/<hash of the manga url>/ -> mangainfo.dat progress.dat cover.<type> cover_scaled.<type>
//   /cache/<source>/<hash of the manga url>/images/ -> <hash of the chapter url>_<page>.jpg
//   /sources/ -> index.yaml <source>.yaml <source>.lua <source>.png common.lua (the sources, see sources/README.md)

#define CONF StaticSettings::get()

class StaticSettings
{
public:
    static StaticSettings &get()
    {
        static StaticSettings instance;
        return instance;
    }

    QString cacheDir;
    QString mangaListDir;
    QString screensaverDir;
    QString sourcesDir;
    const int imageCacheSize = 7;
    const int parallelDownloadsLow = 4;
    const int parallelDownloadsMid = 6;
    const int parallelDownloadsHigh = 8;
    const int forwardPreloads = 3;
    const int backwardPreloads = 1;
    const int autoSuspendIntervalMinutes = 15;
    // idle time before the device goes into standby (when enabled in the settings)
    const int standbyDelaySeconds = 3;
    const int globalTickIntervalSeconds = 60;
    // fixed, so the files don't change with an update of Qt
    const QDataStream::Version dataStreamVersion = QDataStream::Qt_6_0;
    // the layout of the directories above: a cache of another layout is dropped, except for the files that
    // check their own format (favorites, settings, manga lists)
    const int cacheLayoutVersion = 2;

    // paths in cache files are relative to the cache dir, so a cache can be moved (e.g. desktop -> device)
    inline QString relativeCachePath(const QString &path)
    {
        return path.isEmpty() ? path : QDir(cacheDir).relativeFilePath(path);
    }
    inline QString absoluteCachePath(const QString &path)
    {
        return path.isEmpty() || QDir::isAbsolutePath(path) ? path : cacheDir + path;
    }

    inline QString mangasourcedir(const QString &sourceName) { return cacheDir + sourceName + "/"; }
    inline QString mangainfodir(const QString &sourceName, const QString &mangaUrl)
    {
        return mangasourcedir(sourceName) + urlHash(mangaUrl, 16) + "/";
    }
    inline QString mangaimagesdir(const QString &sourceName, const QString &mangaUrl)
    {
        return mangainfodir(sourceName, mangaUrl) + "images/";
    }
    // the file of a page in the images directory
    inline QString pagefilename(const QString &chapterUrl, int page)
    {
        return QString("%1_%2.jpg").arg(urlHash(chapterUrl, 12)).arg(page);
    }

private:
    StaticSettings()
    {
        cacheDir = QCoreApplication::applicationDirPath() + "/cache/";
        screensaverDir = QCoreApplication::applicationDirPath() + "/screensavers/";
        sourcesDir = QCoreApplication::applicationDirPath() + "/sources/";
        mangaListDir = cacheDir + "mangalists/";
    };
};
