#include "favorite.h"
#include <QFileInfo>
#include <QImage>
#include "configs.h"


Favorite::Favorite()
{}

Favorite::Favorite(QString hostname, QString title, QString mangalink) :
    hostname(hostname),
    title(title),
    mangalink(mangalink)
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
    str << m.title << m.hostname << m.mangalink;

    return str;
}

QDataStream &operator>>(QDataStream &str, Favorite &m)
{
    str >> m.title >> m.hostname >> m.mangalink;

    return str;
}


