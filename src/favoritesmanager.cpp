#include "favoritesmanager.h"

#include <QSaveFile>
#include <QtConcurrent/QtConcurrent>

#include "cachefile.h"
#include "imageprocessingnative.h"
#include "mangainfo.h"
#include "perftimer.h"
#include "sizes.h"

FavoritesManager::FavoritesManager(const QMap<QString, MangaSourceBase *> &mangasources,
                                   MangaRepository *repository, NetworkManager *networkManager, QObject *parent)
    : QObject(parent),
      mangasources(mangasources),
      repository(repository),
      networkManager(networkManager),
      entries(),
      infosLoaded(false),
      lastUpdates(),
      updateAfterConnecting(false),
      thumbnailWatcher(),
      thumbnailBatch(),
      queuedThumbnails(),
      thumbnailBatchTimer(),
      thumbnailBatchRunning(false),
      thumbnailPixelRatio(0)
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

    connect(&thumbnailWatcher, &QFutureWatcher<QImage>::resultReadyAt, this, &FavoritesManager::thumbnailReady);
    connect(&thumbnailWatcher, &QFutureWatcher<QImage>::finished, this,
            [this]()
            {
                thumbnailBatchRunning = false;
                if (perfLoggingEnabled())
                    qDebug().nospace() << "[perf] thumbnails: " << thumbnailBatch.size() << " in "
                                       << thumbnailBatchTimer.elapsed() << " ms";
                startThumbnailBatch();
            });
}

FavoritesManager::~FavoritesManager()
{
    // the batch keeps running until it is done, without reporting back
    thumbnailWatcher.disconnect(this);
}

void FavoritesManager::deserialize()
{
    QFile file(QString(CONF.cacheDir) + "favorites.dat");
    if (!file.open(QIODevice::ReadOnly))
        return;

    QList<Favorite> favorites;
    QDataStream in(&file);
    bool current = readCacheFileHeader(in, CacheFile::Favorites);
    if (current)
        in >> favorites;
    file.close();

    // the file would be overwritten with the empty list by the next save, it's kept aside
    if (!current || in.status() != QDataStream::Ok)
    {
        auto kept = file.fileName() + (current ? ".damaged" : ".old");
        qDebug() << "Couldn't read the favorites, keeping the file as" << kept;
        QFile::remove(kept);
        QFile::rename(file.fileName(), kept);
        return;
    }

    for (const auto &fav : std::as_const(favorites))
        entries.append({fav, {}, {}});
}

void FavoritesManager::serialize()
{
    QList<Favorite> favorites;
    for (const auto &entry : std::as_const(entries))
        favorites.append(entry.favorite);

    // the old file stays until the new one is complete
    QSaveFile file(QString(CONF.cacheDir) + "favorites.dat");
    if (!file.open(QIODevice::WriteOnly))
        return;

    QDataStream out(&file);
    writeCacheFileHeader(out, CacheFile::Favorites);
    out << favorites;
    if (out.status() != QDataStream::Ok || !file.commit())
        qDebug() << "Couldn't save the favorites";
}

int FavoritesManager::indexOf(const Favorite &fav) const
{
    for (int i = 0; i < entries.size(); i++)
        if (entries[i].favorite.hostname == fav.hostname && entries[i].favorite.mangaUrl == fav.mangaUrl)
            return i;

    return -1;
}

int FavoritesManager::indexOf(const MangaInfo *info) const
{
    return info ? indexOf(Favorite(info->hostname, info->title, info->url)) : -1;
}

bool FavoritesManager::isFavorite(QSharedPointer<MangaInfo> info) const
{
    return indexOf(info.get()) >= 0;
}

bool FavoritesManager::toggleFavorite(QSharedPointer<MangaInfo> info)
{
    int i = indexOf(info.get());
    if (i >= 0)
    {
        removeAt(i);
        serialize();
        return false;
    }

    Entry entry{Favorite::fromMangaInfo(info.get()), info, {}};
    entry.progress.deserialize(info->hostname, info->url);
    connectInfo(info);
    entries.prepend(entry);
    serialize();
    emit favoriteInserted(0);

    return true;
}

void FavoritesManager::removeAt(int i)
{
    entries.removeAt(i);
    emit favoriteRemoved(i);
}

void FavoritesManager::moveFavoriteToFront(int i)
{
    if (i <= 0 || i >= entries.count())
        return;

    entries.move(i, 0);
    serialize();
    emit favoriteMoved(i, 0);
}

void FavoritesManager::loadEntry(Entry &entry)
{
    auto source = mangasources.value(entry.favorite.hostname);
    entry.info = source ? repository->cachedInfo(source, entry.favorite.mangaUrl, entry.favorite.title) : nullptr;
    entry.progress = ReadingProgress(entry.favorite.hostname, entry.favorite.mangaUrl);
    connectInfo(entry.info);
}

void FavoritesManager::loadInfos()
{
    if (infosLoaded)
        return;

    PerfTimer perf("load favorite infos");
    for (auto &entry : entries)
        loadEntry(entry);
    infosLoaded = true;

    emit favoritesReset();
}

void FavoritesManager::connectInfo(const QSharedPointer<MangaInfo> &info)
{
    if (!info)
        return;

    connect(info.get(), &MangaInfo::updatedSignal, this, &FavoritesManager::infoUpdated, Qt::UniqueConnection);
    connect(info.get(), &MangaInfo::chaptersMoved, this, &FavoritesManager::chaptersMoved, Qt::UniqueConnection);
    connect(info.get(), &MangaInfo::coverLoaded, this, &FavoritesManager::coverLoaded, Qt::UniqueConnection);
}

void FavoritesManager::chaptersMoved()
{
    int i = indexOf(qobject_cast<MangaInfo *>(sender()));
    if (i < 0)
        return;

    // the info located the chapter of the progress again (updatedSignal follows)
    entries[i].progress = ReadingProgress(entries[i].favorite.hostname, entries[i].favorite.mangaUrl);
}

void FavoritesManager::infoUpdated(bool newChapters)
{
    int i = indexOf(qobject_cast<MangaInfo *>(sender()));
    if (i < 0)
        return;

    if (newChapters && i > 0)
    {
        moveFavoriteToFront(i);
        i = 0;
    }
    emit favoriteChanged(i);
}

void FavoritesManager::coverLoaded()
{
    int i = indexOf(qobject_cast<MangaInfo *>(sender()));
    if (i < 0 || thumbnailPixelRatio <= 0)
        return;

    queueThumbnail(i);
    startThumbnailBatch();
}

QSharedPointer<MangaInfo> FavoritesManager::requireInfo(int i)
{
    loadInfos();

    if (i < 0 || i >= entries.count())
        return {};

    auto info = entries[i].info;
    if (info && !info->chapters.isEmpty())
        return info;

    // a copy: the download runs an event loop, the favorites can be reordered or removed meanwhile
    const Favorite fav = entries[i].favorite;
    auto source = mangasources.value(fav.hostname);
    if (!source)
        return {};

    auto res = repository->loadInfo(source, fav.mangaUrl, fav.title).wait();
    if (!res.has_value())
    {
        emit error(res.error().message);
        return {};
    }

    info = res.value();
    int now = indexOf(fav);
    if (now >= 0)
    {
        entries[now].info = info;
        connectInfo(info);
        emit favoriteChanged(now);
    }
    return info;
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

    // a copy: an update that is done right away (e.g. a failed request) can reorder the favorites
    QList<QSharedPointer<MangaInfo>> infos;
    for (const auto &entry : std::as_const(entries))
    {
        // the source might have been disabled since the infos were loaded
        const auto &info = entry.info;
        if (!info || !mangasources.value(info->hostname))
            continue;

        // also the ones without chapters (e.g. all of them removed at the source)
        auto &lastUpdate = lastUpdates[info->hostname + '/' + info->url];
        if (lastUpdate.isValid() && lastUpdate.elapsed() < minUpdateIntervalMs)
            continue;
        lastUpdate.start();

        infos.append(info);
    }

    if (!infos.isEmpty())
        update(infos);
}

Task<> FavoritesManager::update(QList<QSharedPointer<MangaInfo>> infos)
{
    std::vector<Task<Result<>>> updates;
    for (const auto &info : std::as_const(infos))
        updates.push_back(repository->updateInfo(info, false));

    int failed = 0;
    Error firstError;
    for (const auto &update : updates)
    {
        auto result = co_await update;
        if (!result && !result.error().isCancelled() && failed++ == 0)
            firstError = result.error();
    }

    // without a connection that's no news
    if (failed > 0 && networkManager->connected)
        emit error(failed == 1 ? "Couldn't update a favorite: " + firstError.message
                               : QString("Couldn't update %1 favorites: %2").arg(failed).arg(firstError.message));
}

void FavoritesManager::resetUpdatedStatus()
{
    for (int i = 0; i < entries.count(); i++)
    {
        auto &info = entries[i].info;
        if (info && info->updated)
        {
            info->updated = false;
            emit favoriteChanged(i);
        }
    }
}

void FavoritesManager::setProgress(const MangaInfo *info, const ReadingProgress &progress)
{
    int i = indexOf(info);
    if (i < 0 || !infosLoaded)
        return;

    entries[i].progress = progress;
    emit favoriteChanged(i);
}

void FavoritesManager::clearFavorites()
{
    entries.clear();
    infosLoaded = true;
    emit favoritesReset();
}

void FavoritesManager::clearInvalidFavorites()
{
    loadInfos();

    // the downloads run event loops, the favorites can be reordered meanwhile: they are found again afterwards
    QList<Favorite> all;
    for (const auto &entry : std::as_const(entries))
        all.append(entry.favorite);

    for (const auto &fav : std::as_const(all))
    {
        int i = indexOf(fav);
        // the ones of disabled sources have no info, they come back when the source is enabled again
        if (i < 0 || !mangasources.value(fav.hostname))
            continue;

        // the infos that aren't cached are downloaded first, only the ones that fail or have nothing to read
        // (e.g. MangaDex titles whose chapters are all external links) are invalid
        auto info = entries[i].info;
        bool valid = info && !info->chapters.isEmpty();
        if (!valid && info)
        {
            auto res = repository->loadInfo(info->mangaSource, fav.mangaUrl, fav.title).wait();

            i = indexOf(fav);
            if (i < 0)
                continue;
            if (res.has_value())
            {
                entries[i].info = res.value();
                connectInfo(entries[i].info);
                valid = !entries[i].info->chapters.isEmpty();
            }
        }

        if (!valid)
        {
            qDebug() << "Removing invalid favorite:" << fav.hostname << fav.title << fav.mangaUrl;
            removeAt(i);
        }
    }
    serialize();
}

// Runs on a worker thread.
static QImage loadThumbnail(const ThumbnailRequest &request)
{
    if (QFile::exists(request.thumbnailPath))
    {
        QImage image = loadQImageFast(request.thumbnailPath);
        if (!image.isNull())
            return image;
    }

    // the cover is still being downloaded, its coverLoaded() signal comes later
    if (request.coverPath.isEmpty() || !QFile::exists(request.coverPath))
        return QImage();

    QImage image = loadQImageFast(request.coverPath);
    if (image.isNull())
        return image;

    int size = qRound(SIZES.favoriteCoverSize * request.pixelRatio);
    image = image.scaled(size, size, Qt::KeepAspectRatio, Qt::SmoothTransformation);

    // the list might load it again while it is written
    QSaveFile file(request.thumbnailPath);
    const char *format = request.thumbnailPath.endsWith(".png") ? "PNG" : "JPG";
    if (!file.open(QIODevice::WriteOnly) || !image.save(&file, format) || !file.commit())
        qDebug() << "Failed to save thumbnail:" << request.thumbnailPath;

    return image;
}

void FavoritesManager::loadThumbnails(const QList<int> &indices, qreal pixelRatio)
{
    thumbnailPixelRatio = pixelRatio;
    for (int i : indices)
        queueThumbnail(i);

    startThumbnailBatch();
}

void FavoritesManager::queueThumbnail(int i)
{
    auto info = entries.value(i).info;
    if (!info)
        return;

    ThumbnailRequest request{entries[i].favorite.key(), info->coverPath, info->coverThumbnailPath(),
                             thumbnailPixelRatio};
    if (request.thumbnailPath.isEmpty())
        return;

    auto sameKey = [&request](const ThumbnailRequest &r) { return r.key == request.key; };
    if (std::none_of(queuedThumbnails.begin(), queuedThumbnails.end(), sameKey))
        queuedThumbnails.append(request);
}

void FavoritesManager::startThumbnailBatch()
{
    // not isRunning(): that is false before the last results of the batch were delivered, a new future would
    // drop them
    if (thumbnailBatchRunning || queuedThumbnails.isEmpty())
        return;

    thumbnailBatchRunning = true;
    thumbnailBatchTimer.start();

    thumbnailBatch = queuedThumbnails;
    queuedThumbnails.clear();
    thumbnailWatcher.setFuture(QtConcurrent::mapped(thumbnailBatch, loadThumbnail));
}

void FavoritesManager::thumbnailReady(int index)
{
    if (index < 0 || index >= thumbnailBatch.size())
        return;

    QImage image = thumbnailWatcher.resultAt(index);
    if (!image.isNull())
        emit thumbnailLoaded(thumbnailBatch[index].key, image);
}
