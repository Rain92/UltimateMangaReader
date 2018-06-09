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

void FavoritesManager::deserializeProgresses()
{
    QMutableListIterator<Favorite> iterator(favorites);
    while (iterator.hasNext())
    {
        Favorite &fav = iterator.next();
        QFile file(mangainfodir(fav.hostname, fav.title) + "progress.dat");
        if (!file.open(QIODevice::ReadOnly))
            continue;

        QDataStream in(&file);
        in >> fav.currentindex;
        file.close();
    }
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
    QString key = info->hostname + info->title;

    QMutableListIterator<Favorite> iterator(favorites);
    while (iterator.hasNext())
    {
        Favorite &f = iterator.next();
        if (f.hostname + f.title == key)
            return &f;
    }

    favorites.append(Favorite::fromMangaInfo(info));

    return &favorites.last();
}


bool FavoritesManager::isFavorite(MangaInfo *info)
{
    QString key = info->hostname + info->title;
    foreach (const Favorite &f, favorites)
    {
        if (f.hostname + f.title == key)
            return true;
    }
    return false;
}

bool FavoritesManager::toggleFavorite(MangaInfo *info)
{
    QString key = info->hostname + info->title;

//    qDebug() << key;

    QMutableListIterator<Favorite> iterator(favorites);
    while (iterator.hasNext())
    {
        Favorite &f = iterator.next();
        if (f.hostname + f.title == key)
        {
            iterator.remove();
            serialize();
            return false;
        }

    }

    favorites.append(Favorite::fromMangaInfo(info));
    serialize();
    return true;


}

QList<Favorite> *FavoritesManager::getFavorites()
{
    deserializeProgresses();
    return &favorites;
}
