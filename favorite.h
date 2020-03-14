#ifndef READINGSTATE_H
#define READINGSTATE_H

#include "defines.h"
#include "mangaindex.h"
#include "mangainfo.h"

struct Favorite
{
    QString hostname;
    QString title;
    QString mangalink;

    Favorite();

    Favorite(const QString &hostname, const QString &title,
             const QString &mangalink);

    QString mangaInfoPath() const;
    QString mangaProgressPath() const;

    QString coverpathscaled() const;

    static Favorite fromMangaInfo(MangaInfo *info)
    {
        return Favorite(info->hostname, info->title, info->link);
    }
};

QDataStream &operator<<(QDataStream &str, const Favorite &m);

QDataStream &operator>>(QDataStream &str, Favorite &m);

#endif  // READINGSTATE_H
