#include "favoritesmanager.h"

FavoritesManager::FavoritesManager(
    const QList<AbstractMangaSource *> &mangasources)
    : favoriteinfos(), favorites(), mangasources(mangasources)
{
    //    deserialize();
}

void FavoritesManager::deserialize()
{
    QFile file(QString(cachedir) + "favorites.dat");
    if (!file.open(QIODevice::ReadOnly)) return;

    QDataStream in(&file);
    in >> favorites;
    file.close();

    loadInfos();
}

void FavoritesManager::serialize()
{
    //    qDebug() << "serialize";

    QFile file(QString(cachedir) + "favorites.dat");
    if (!file.open(QIODevice::WriteOnly)) return;

    QDataStream out(&file);
    out << favorites;
    file.close();
}

bool FavoritesManager::isFavorite(MangaInfo *info)
{
    QString key = info->hostname + info->title;
    foreach (const Favorite &f, favorites)
    {
        if (f.hostname + f.title == key) return true;
    }
    return false;
}

bool FavoritesManager::toggleFavorite(QSharedPointer<MangaInfo> info)
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

            favoriteinfos.removeOne(info);

            return false;
        }
    }

    favorites.append(Favorite::fromMangaInfo(info.data()));
    favoriteinfos.append(info);
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
    favoriteinfos.clear();
    foreach (const Favorite &fav, favorites)
    {
        foreach (AbstractMangaSource *s, mangasources)
        {
            if (s->name != fav.hostname) continue;

            QSharedPointer<MangaInfo> mi =
                s->loadMangaInfo(fav.mangalink, fav.title, false);
            favoriteinfos.append(mi);
            break;
        }
    }
}

void FavoritesManager::updateInfos()
{
    foreach (QSharedPointer<MangaInfo> info, favoriteinfos)
        foreach (AbstractMangaSource *s, mangasources)
            if (s->name == info->hostname)
            {
                if (s->name != info->hostname) continue;

                s->updateMangaInfo(info);
                break;
            }

    //    serialize();
}

void FavoritesManager::clearFavorites()
{
    favorites.clear();
    favoriteinfos.clear();
}
