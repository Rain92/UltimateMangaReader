#include "favorite.h"
#include <QFileInfo>


Favorite::Favorite():
    numchapters(0),
    currentindex(0, 0),
    updated(false)
{}

Favorite::Favorite(QString hostname, QString title, int numchapters, MangaIndex currentindex, QString coverpath, QString mangalink) :
    hostname(hostname),
    title(title),
    numchapters(numchapters),
    currentindex(currentindex),
    coverpath(coverpath),
    mangalink(mangalink),
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

QDataStream &operator<<(QDataStream &str, const Favorite &m)
{
    str << m.title << m.hostname << (qint32)m.numchapters << m.currentindex << m.coverpath << m.mangalink;

    return str;
}

QDataStream &operator>>(QDataStream &str, Favorite &m)
{
    str >> m.title >> m.hostname >> m.numchapters >> m.currentindex >> m.coverpath >> m.mangalink;

    return str;
}


