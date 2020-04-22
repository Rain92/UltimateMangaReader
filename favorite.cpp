#include "favorite.h"

Favorite::Favorite() = default;

Favorite::Favorite(const QString &hostname, const QString &title,
                   const QString &mangaurl)
    : hostname(hostname), title(title), mangaUrl(mangaurl)
{
}

QString Favorite::mangaInfoPath() const
{
    return CONF.mangainfodir(hostname, title) + "mangainfo.dat";
}

QDataStream &operator<<(QDataStream &str, const Favorite &m)
{
    str << m.title << m.hostname << m.mangaUrl;

    return str;
}

QDataStream &operator>>(QDataStream &str, Favorite &m)
{
    str >> m.title >> m.hostname >> m.mangaUrl;

    return str;
}
