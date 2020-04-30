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
    for (const Favorite &f : favorites)
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

void FavoritesManager::moveFavoriteToFront(int i)
{
    favorites.move(i, 0);
    favoriteinfos.move(i, 0);

    serialize();
}

void FavoritesManager::loadInfos()
{
    for (int i = 0; i < favorites.length(); i++)
    {
        auto &fav = favorites[i];
        if (mangasources.contains(fav.hostname) &&
            mangasources[fav.hostname]->mangaList.titles.contains(fav.title))
        {
            QTimer::singleShot(0, [this, &fav]() {
                auto mi = mangasources[fav.hostname]->loadMangaInfo(fav.mangaUrl, fav.title, false);

                if (mi.isOk())
                {
                    favoriteinfos.append(mi.unwrap());
                }
                else
                {
                    favoriteinfos.append(QSharedPointer<MangaInfo>(nullptr));
                }
            });
        }
        else
        {
            favorites.removeAt(i--);
        }
    }
}

void FavoritesManager::updateInfos()
{
    for (int i = 0; i < favoriteinfos.length(); i++)
    {
        if (!favoriteinfos[i].isNull())
        {
            mangasources[favoriteinfos[i]->hostname]->updateMangaInfoAsync(favoriteinfos[i]);
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

void FavoritesManager::clearFavorites()
{
    favorites.clear();
    favoriteinfos.clear();
}
