#include "favoritesmanager.h"

FavoritesManager::FavoritesManager(const QMap<QString, AbstractMangaSource *> &mangasources, QObject *parent)
    : QObject(parent), favoriteinfos(), favorites(), mangasources(mangasources)
{
    deserialize();
}

void FavoritesManager::deserialize()
{
    QFile file(QString(CONF.cacheDir) + "favorites.dat");
    if (!file.open(QIODevice::ReadOnly))
        return;

    QDataStream in(&file);
    in >> favorites;
    file.close();
}

void FavoritesManager::serialize()
{
    QFile file(QString(CONF.cacheDir) + "favorites.dat");
    if (!file.open(QIODevice::WriteOnly))
        return;

    QDataStream out(&file);
    out << favorites;
    file.close();
}

bool FavoritesManager::isFavorite(QSharedPointer<MangaInfo> info)
{
    for (const Favorite &f : qAsConst(favorites))
        if (f.hostname == info->hostname && f.title == info->title)
            return true;

    return false;
}

bool FavoritesManager::toggleFavorite(QSharedPointer<MangaInfo> info)
{
    for (int i = 0; i < favorites.length(); i++)
    {
        if (favorites[i].hostname == info->hostname && favorites[i].title == info->title)
        {
            favorites.removeAt(i);
            favoriteinfos.removeAt(i);

            serialize();

            return false;
        }
    }

    // add
    favorites.prepend(Favorite::fromMangaInfo(info.get()));
    favoriteinfos.prepend(info);
    serialize();
    return true;
}

int FavoritesManager::findFavorite(const QString &title)
{
    for (int i = 0; i < favoriteinfos.size(); i++)
        if (!favoriteinfos.at(i).isNull() && favoriteinfos.at(i)->title == title)
            return i;

    return -1;
}

void FavoritesManager::moveFavoriteToFront(int i)
{
    favorites.move(i, 0);
    favoriteinfos.move(i, 0);

    serialize();
}

bool FavoritesManager::loadInfos()
{
    if (favorites.length() == favoriteinfos.length())
        return true;

    bool res = true;

    favoriteinfos.clear();
    for (int i = 0; i < favorites.length(); i++)
    {
        auto &fav = favorites[i];
        if (mangasources.contains(fav.hostname) &&
            mangasources[fav.hostname]->mangaList.titles.contains(fav.title))
        {
            auto mi = mangasources[fav.hostname]->loadMangaInfo(fav.mangaUrl, fav.title, false);

            if (mi.isOk())
            {
                favoriteinfos.append(mi.unwrap());
            }
            else
            {
                res = false;
                favoriteinfos.append(QSharedPointer<MangaInfo>(nullptr));
                emit error(mi.unwrapErr());
            }
        }
        else
        {
            favorites.removeAt(i--);
        }
    }
    return res;
}

void FavoritesManager::updateInfos()
{
    for (int i = 0; i < favoriteinfos.length(); i++)
    {
        if (!favoriteinfos[i].isNull())
        {
            mangasources[favoriteinfos[i]->hostname]->updateMangaInfoAsync(favoriteinfos[i], false);
        }
        else
        {
            auto &fav = favorites[i];
            if (mangasources.contains(fav.hostname))
            {
                auto mi = mangasources[fav.hostname]->loadMangaInfo(fav.mangaUrl, fav.title, false);

                if (mi.isOk())
                    favoriteinfos[i] = mi.unwrap();
            }
        }
    }
}

void FavoritesManager::resetUpdatedStatus()
{
    for (const auto &fav : qAsConst(favoriteinfos))
        fav->updated = false;
}

void FavoritesManager::clearFavorites()
{
    favorites.clear();
    favoriteinfos.clear();
}
