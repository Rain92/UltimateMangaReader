#pragma once

#include "mangainfo.h"

// A favorite is identified by the name of its source and its url, the title is shown.
struct Favorite
{
    QString sourceName;
    QString title;
    QString mangaUrl;

    Favorite();

    Favorite(const QString &sourceName, const QString &title, const QString &mangaUrl);

    static Favorite fromMangaInfo(MangaInfo *info)
    {
        return Favorite(info->sourceName, info->title, info->url);
    }

    // unique among the favorites
    QString key() const { return sourceName + '/' + mangaUrl; }
};

QDataStream &operator<<(QDataStream &str, const Favorite &m);

QDataStream &operator>>(QDataStream &str, Favorite &m);
