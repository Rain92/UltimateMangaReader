#include "favoritesmanager.h"

#include <QSaveFile>

#include "mangainfo.h"

FavoritesManager::FavoritesManager(const QMap<QString, AbstractMangaSource *> &mangasources,
                                   NetworkManager *networkManager, QObject *parent)
    : QObject(parent),
      favoriteinfos(),
      favorites(),
      mangasources(mangasources),
      networkManager(networkManager),
      infosLoaded(false),
      lastUpdates(),
      updateAfterConnecting(false)
{
    deserialize();

    // the checks that failed without a connection are worth repeating right away
    connect(networkManager, &NetworkManager::wifiConnectFinished, this,
            [this](bool connected)
            {
                if (connected)
                    lastUpdates.clear();
                if (updateAfterConnecting)
                {
                    updateAfterConnecting = false;
                    updateInfos();
                }
            });
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

    // a damaged file would be overwritten with the empty list by the next save, it's kept aside
    if (in.status() != QDataStream::Ok)
    {
        qDebug() << "Couldn't read the favorites, keeping the file as favorites.dat.damaged";
        favorites.clear();
        QFile::remove(file.fileName() + ".damaged");
        QFile::rename(file.fileName(), file.fileName() + ".damaged");
    }
}

void FavoritesManager::serialize()
{
    // the old file stays until the new one is complete
    QSaveFile file(QString(CONF.cacheDir) + "favorites.dat");
    if (!file.open(QIODevice::WriteOnly))
        return;

    QDataStream out(&file);
    out.setVersion(CONF.dataStreamVersion);
    out << favorites;
    if (out.status() != QDataStream::Ok || !file.commit())
        qDebug() << "Couldn't save the favorites";
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

    auto info = favoriteinfos[i];
    if (info && !info->chapters.isEmpty())
        return info;

    // a copy: the download runs an event loop, the favorites can be reordered or removed meanwhile
    const Favorite fav = favorites[i];
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
    int now = indexOf(fav);
    if (now >= 0 && now < favoriteinfos.size())
        favoriteinfos[now] = info;
    return info;
}

int FavoritesManager::indexOf(const Favorite &fav) const
{
    for (int i = 0; i < favorites.size(); i++)
        if (favorites[i].hostname == fav.hostname && favorites[i].title == fav.title)
            return i;

    return -1;
}

void FavoritesManager::updateInfos()
{
    loadInfos();

    // the downloads would fail while the WiFi is still connecting (e.g. right after the start), and not be
    // repeated for a while
    if (networkManager->isConnectingWifi())
    {
        updateAfterConnecting = true;
        return;
    }

    // tab switches would pile up downloads otherwise
    const qint64 minUpdateIntervalMs = 60 * 1000;

    for (const auto &info : qAsConst(favoriteinfos))
    {
        // the source might have been disabled since the infos were loaded
        auto source = info ? mangasources.value(info->hostname) : nullptr;
        if (!source)
            continue;

        // also the ones without chapters (e.g. all of them removed at the source)
        auto &lastUpdate = lastUpdates[info->hostname + '/' + info->title];
        if (lastUpdate.isValid() && lastUpdate.elapsed() < minUpdateIntervalMs)
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

    // the downloads run event loops, the favorites can be reordered meanwhile: they are found again afterwards
    const QList<Favorite> all = favorites;
    for (const auto &fav : all)
    {
        int i = indexOf(fav);
        // the ones of disabled sources have no info, they come back when the source is enabled again
        if (i < 0 || !mangasources.value(fav.hostname))
            continue;

        // the infos that aren't cached are downloaded first, only the ones that fail or have nothing to read
        // (e.g. MangaDex titles whose chapters are all external links) are invalid
        auto info = favoriteinfos[i];
        bool valid = info && !info->chapters.isEmpty();
        if (!valid && info)
        {
            auto res = info->mangaSource->loadMangaInfo(fav.mangaUrl, fav.title);
            i = indexOf(fav);
            if (i < 0)
                continue;
            if (res.isOk())
            {
                favoriteinfos[i] = res.unwrap();
                valid = !favoriteinfos[i]->chapters.isEmpty();
            }
        }

        if (!valid)
        {
            qDebug() << "Removing invalid favorite:" << fav.hostname << fav.title;
            favorites.removeAt(i);
            favoriteinfos.removeAt(i);
        }
    }
    serialize();
}
