#ifndef STATICSETTINGS_H
#define STATICSETTINGS_H

#include "utils.h"

// dirstructure:
//               /cache/      -> favorites.dat
//               /cache/mangalists/ -> hostname_mangalist.dat
//               /cache/hostname/manganame/ -> mangainfo.dat progress.dat
//               /cache/hostname/manganame/images/ ->
//               manganame_chapter_page.jpg|png

#define CONF StaticSettings::get()

class StaticSettings
{
public:
    static StaticSettings &get()
    {
        static StaticSettings instance;
        return instance;
    }

    QString cachedir;
    QString mangalistdir;
    int imagecachesize;
    int maxparalleldownloads;
    int maxparalleldownloads2;
    int forwardpreloads;
    int backwardpreloads;

    inline QString mangainfodir(const QString &hostname, const QString &title)
    {
        return cachedir + hostname + "/" + makePathLegal(title) + "/";
    }
    inline QString mangaimagesdir(const QString &hostname, const QString &title)
    {
        return mangainfodir(hostname, title) + "images/";
    }

private:
    StaticSettings()
    {
        cachedir = QCoreApplication::applicationDirPath() + "/cache/";
        mangalistdir = cachedir + "mangalists/";
        imagecachesize = 10;
        maxparalleldownloads = 4;
        maxparalleldownloads2 = 8;
        forwardpreloads = 6;
        backwardpreloads = 1;
    };
};

#endif  // STATICSETTINGS_H
