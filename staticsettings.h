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
    int imageCacheSize = 10;
    int parallelDownloadsLow = 4;
    int parallelDownloadsHigh = 8;
    int forwardPreloads = 6;
    int backwardPreloads = 1;
    int autoSuspendIntervalMinutes = 15;
    int globalTickIntervalSeconds = 60;

    inline QString mangainfodir(const QString &hostname, const QString &title)
    {
        return cacheDir + hostname + "/" + makePathLegal(title) + "/";
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
