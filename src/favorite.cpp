#include "favorite.h"

Favorite::Favorite() = default;

Favorite::Favorite(const QString &sourceName, const QString &title,
                   const QString &mangaurl)
    : sourceName(sourceName), title(title), mangaUrl(mangaurl)
{
}

QDataStream &operator<<(QDataStream &str, const Favorite &m)
{
    str << m.title << m.sourceName << m.mangaUrl;

    return str;
}

QDataStream &operator>>(QDataStream &str, Favorite &m)
{
    str >> m.title >> m.sourceName >> m.mangaUrl;

    return str;
}
