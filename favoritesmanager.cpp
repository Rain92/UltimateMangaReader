#include "favoritesmanager.h"

FavoritesManager::FavoritesManager():
    favorites()
{
//    deserialize();
}

void FavoritesManager::deserialize()
{
    QFile file(QString(cachedir) + "favorites.dat");
    if (!file.open(QIODevice::ReadOnly))
        return;

    QDataStream in(&file);
    in >> favorites;
    file.close();
}

void FavoritesManager::serialize()
{
    QFile file(QString(cachedir) + "favorites.dat");
    if (!file.open(QIODevice::WriteOnly))
        return;

    QDataStream out(&file);
    out << favorites;
    file.close();
}


Favorite *FavoritesManager::findOrInsert(MangaInfo *info)
{
    QString key = info->hostname + "_" + info->title;
    if (favorites.contains(key))
        return &favorites[key];

    favorites[key] = Favorite::fromMangaInfo(info);

    return &favorites[key];
}


bool FavoritesManager::isFavorite(MangaInfo *info)
{
    QString key = info->hostname + "_" + info->title;
    return favorites.contains(key);
}

bool FavoritesManager::toggleFavorite(MangaInfo *info)
{
    QString key = info->hostname + "_" + info->title;
    if (favorites.contains(key))
    {
        favorites.remove(key);
        serialize();
        return false;
    }
    else
    {
        findOrInsert(info);
        serialize();
        return true;
    }
}

QList<Favorite> FavoritesManager::getFavorites()
{
    QList<Favorite> ret;

    foreach (const Favorite &fav, favorites)
    {
        ret.append(fav);

        QFile file(fav.mangaProgressPath());
        if (file.open(QIODevice::ReadOnly))
        {
            QDataStream in(&file);
            in >> ret.last().currentindex >> ret.last().numchapters;
            file.close();
        }
    }

    return ret;
}
