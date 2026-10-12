#ifndef STATICSETTINGS_H
#define STATICSETTINGS_H

#include "utils.h"

// dirstructure:
//               /cache/      -> favorites.dat
//               /cache/mangalists/ -> hostname_mangalist.dat
//               /cache/hostname/manganame/ -> mangainfo.dat progress.dat
//               /cache/hostname/manganame/images/ ->
//                                         manganame_chapter_page.jpg|png

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
    // every cache file uses the same format, so Qt 5 and Qt 6 builds can share a cache
    const QDataStream::Version dataStreamVersion = QDataStream::Qt_5_15;

    // paths in cache files are relative to the cache dir, so a cache can be moved (e.g. desktop -> device)
    inline QString relativeCachePath(const QString &path)
    {
        return path.isEmpty() ? path : QDir(cacheDir).relativeFilePath(path);
    }
    inline QString absoluteCachePath(const QString &path)
    {
        return path.isEmpty() || QDir::isAbsolutePath(path) ? path : cacheDir + path;
    }

    inline QString mangasourcedir(const QString &hostname) { return cacheDir + hostname + "/"; }
    inline QString mangainfodir(const QString &hostname, const QString &title)
    {
        return mangasourcedir(hostname) + makePathLegal(title) + "/";
    }
    inline QString mangaimagesdir(const QString &hostname, const QString &title)
    {
        return mangainfodir(hostname, title) + "images/";
    }

private:
    StaticSettings()
    {
        cacheDir = QCoreApplication::applicationDirPath() + "/cache/";
        screensaverDir = QCoreApplication::applicationDirPath() + "/screensavers/";
        mangaListDir = cacheDir + "mangalists/";
    };
};

#endif  // STATICSETTINGS_H
