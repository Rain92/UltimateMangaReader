#pragma once

#include "mangainfo.h"

// A favorite is identified by its source (hostname) and url, the title is shown.
struct Favorite
{
    QString hostname;
    QString title;
    QString mangaUrl;

    Favorite();

    Favorite(const QString &hostname, const QString &title, const QString &mangaUrl);

    static Favorite fromMangaInfo(MangaInfo *info)
    {
        return Favorite(info->hostname, info->title, info->url);
    }

    // unique among the favorites
    QString key() const { return hostname + '/' + mangaUrl; }
};

QDataStream &operator<<(QDataStream &str, const Favorite &m);

QDataStream &operator>>(QDataStream &str, Favorite &m);
