#include "favoritesmanager.h"

#include "mangainfo.h"

FavoritesManager::FavoritesManager(const QMap<QString, AbstractMangaSource *> &mangasources, QObject *parent)
    : QObject(parent),
      favoriteinfos(),
      favorites(),
      mangasources(mangasources),
      infosLoaded(false),
      lastUpdates()
{
    deserialize();
}

void FavoritesManager::deserialize()
{
    QFile file(QString(CONF.cacheDir) + "favorites.dat");
    if (!file.open(QIODevice::ReadOnly))
        return;

    QDataStream in(&file);
    in.setVersion(CONF.dataStreamVersion);
    in >> favorites;
    file.close();
}

void FavoritesManager::serialize()
{
    QFile file(QString(CONF.cacheDir) + "favorites.dat");
    if (!file.open(QIODevice::WriteOnly))
        return;

    QDataStream out(&file);
    out.setVersion(CONF.dataStreamVersion);
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
            // the infos are only there once the favorites were shown
            if (infosLoaded)
                favoriteinfos.removeAt(i);

            serialize();

            return false;
        }
    }

    // add
    favorites.prepend(Favorite::fromMangaInfo(info.get()));
    if (infosLoaded)
        favoriteinfos.prepend(info);
    serialize();
    return true;
}

int FavoritesManager::findFavorite(const MangaInfo *info) const
{
    for (int i = 0; i < favoriteinfos.size(); i++)
        if (favoriteinfos.at(i).get() == info)
            return i;

    return -1;
}

void FavoritesManager::moveFavoriteToFront(int i)
{
    if (i <= 0 || i >= favorites.length())
        return;

    favorites.move(i, 0);
    if (infosLoaded)
        favoriteinfos.move(i, 0);

    serialize();
}

QSharedPointer<MangaInfo> FavoritesManager::loadInfo(const Favorite &fav)
{
    auto source = mangasources.value(fav.hostname);
    if (!source)
        return {};

    return source->loadCachedMangaInfo(fav.mangaUrl, fav.title);
}

void FavoritesManager::loadInfos()
{
    if (infosLoaded)
        return;

    favoriteinfos.clear();
    for (const auto &fav : qAsConst(favorites))
        favoriteinfos.append(loadInfo(fav));

    infosLoaded = true;
}

QSharedPointer<MangaInfo> FavoritesManager::requireInfo(int i)
{
    loadInfos();

    if (i < 0 || i >= favorites.length())
        return {};

    auto &info = favoriteinfos[i];
    if (info && !info->chapters.isEmpty())
        return info;

    const auto &fav = favorites[i];
    auto source = mangasources.value(fav.hostname);
    if (!source)
        return {};

    auto res = source->loadMangaInfo(fav.mangaUrl, fav.title);
    if (res.isErr())
    {
        emit error(res.unwrapErr());
        return {};
    }

    info = res.unwrap();
    return info;
}

void FavoritesManager::updateInfos()
{
    loadInfos();

    // tab switches would pile up downloads otherwise
    const qint64 minUpdateIntervalMs = 60 * 1000;

    for (const auto &info : qAsConst(favoriteinfos))
    {
        // the source might have been disabled since the infos were loaded
        auto source = info ? mangasources.value(info->hostname) : nullptr;
        if (!source)
            continue;

        auto &lastUpdate = lastUpdates[info->hostname + '/' + info->title];
        if (lastUpdate.isValid() && lastUpdate.elapsed() < minUpdateIntervalMs && !info->chapters.isEmpty())
            continue;
        lastUpdate.start();

        source->updateMangaInfoAsync(info, false);
    }
}

void FavoritesManager::resetUpdatedStatus()
{
    for (auto &fav : favoriteinfos)
        if (!fav.isNull())
            fav->updated = false;
}

void FavoritesManager::clearFavorites()
{
    favorites.clear();
    favoriteinfos.clear();
    infosLoaded = true;
}

void FavoritesManager::clearInvalidFavorites()
{
    loadInfos();

    for (int i = favorites.length() - 1; i >= 0; i--)
    {
        // the infos that aren't cached are downloaded first, only the ones that fail are invalid
        bool valid = favoriteinfos[i] && !favoriteinfos[i]->chapters.isEmpty();
        if (!valid && favoriteinfos[i])
        {
            auto res = favoriteinfos[i]->mangaSource->loadMangaInfo(favorites[i].mangaUrl, favorites[i].title);
            if (res.isOk())
            {
                favoriteinfos[i] = res.unwrap();
                valid = true;
            }
        }

        if (!valid)
        {
            qDebug() << "Removing invalid favorite:" << favorites[i].hostname << favorites[i].title;
            favorites.removeAt(i);
            favoriteinfos.removeAt(i);
        }
    }
    serialize();
}
