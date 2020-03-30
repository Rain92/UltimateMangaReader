#include "favorite.h"

Favorite::Favorite() = default;

Favorite::Favorite(const QString &hostname, const QString &title,
                   const QString &mangalink)
    : hostname(hostname), title(title), mangalink(mangalink)
{
}

QString Favorite::mangaInfoPath() const
{
    return CONF.mangainfodir(hostname, title) + "mangainfo.dat";
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
