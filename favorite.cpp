#include "favorite.h"
#include <QFileInfo>
#include <QImage>
#include "configs.h"


Favorite::Favorite():
    numchapters(0),
    currentindex(0, 0),
    updated(false)
{}

Favorite::Favorite(QString hostname, QString title, int numchapters, MangaIndex currentindex, QString coverpath, QString mangalink, QString status) :
    hostname(hostname),
    title(title),
    numchapters(numchapters),
    currentindex(currentindex),
    coverpath(coverpath),
    mangalink(mangalink),
    status(status),
    updated(false)
{}

QString Favorite::mangaInfoPath() const
{
    return mangainfodir(hostname, title) + "mangainfo.dat";
}

QString Favorite::mangaProgressPath() const
{
    return mangainfodir(hostname, title) + "progress.dat";
}


QString Favorite::coverpathscaled() const
{
    if(coverpath == "")
        return "";

    QString scpath = coverpath;
    scpath.insert(scpath.length() - 4, "_scaled");

    QFileInfo scaledcover(scpath);
    if(!scaledcover.exists())
    {
        QImage img;
        img.load(coverpath);
        img = img.scaled(favoritecoverwidth, favoritecoverheight, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        img.save(scpath);
    }

    return scpath;
}

QDataStream &operator<<(QDataStream &str, const Favorite &m)
{
    str << m.title << m.hostname << (qint32)m.numchapters << m.currentindex << m.coverpath << m.mangalink << m.status;

    return str;
}

QDataStream &operator>>(QDataStream &str, Favorite &m)
{
    str >> m.title >> m.hostname >> m.numchapters >> m.currentindex >> m.coverpath >> m.mangalink >> m.status;

    return str;
}


