#include "ultimatemangareadercore.h"

UltimateMangaReaderCore::UltimateMangaReaderCore(QObject* parent)
    : QObject(parent),
      mangaSources(),
      activeMangaSources(),
      currentMangaSource(nullptr),
      currentManga(),
      downloadManager(new DownloadManager(this)),
      mangaController(new MangaController(downloadManager, this)),
      favoritesManager(new FavoritesManager(activeMangaSources, this)),
      settings(),
      timer()
{
    setupDirectories();
    settings.deserialize();

    downloadManager->connect();

    mangaSources.append(QSharedPointer<AbstractMangaSource>(new MangaPanda(this, downloadManager)));
    mangaSources.append(QSharedPointer<AbstractMangaSource>(new JaiminisBox(this, downloadManager)));
    mangaSources.append(QSharedPointer<AbstractMangaSource>(new MangaDex(this, downloadManager)));
    mangaSources.append(QSharedPointer<AbstractMangaSource>(new MangaHub(this, downloadManager)));
    mangaSources.append(QSharedPointer<AbstractMangaSource>(new MangaOwl(this, downloadManager)));

    currentMangaSource = mangaSources.first().get();

    for (auto ms : mangaSources)
        ms->deserializeMangaList();

    updateActiveScources();

    favoritesManager->loadInfos();

    timer.setInterval(1000 * 60);
    connect(&timer, &QTimer::timeout, this, &UltimateMangaReaderCore::timerTick);
    QTimer::singleShot(1000 * 60 - QTime::currentTime().second() * 1000 - QTime::currentTime().msec(), [this]() {
        timer.start();
        timerTick();
    });
}

void UltimateMangaReaderCore::timerTick()
{
    emit timeTick();
}

void UltimateMangaReaderCore::updateActiveScources()
{
    activeMangaSources.clear();
    for (auto ms : mangaSources)
    {
        if (!settings.enabledMangaSources.contains(ms->name))
            settings.enabledMangaSources.insert(ms->name, true);

        if (settings.enabledMangaSources[ms->name])
            activeMangaSources.insert(ms->name, ms.get());
    }

    this->currentMangaSource = nullptr;

    emit activeMangaSourcesChanged();
}

void UltimateMangaReaderCore::setImageSize(const QSize& size)
{
    downloadManager->setImageRescaleSize(size);
}

void UltimateMangaReaderCore::setCurrentMangaSource(AbstractMangaSource* mangaSource)
{
    if (mangaSource && this->currentMangaSource != mangaSource)
    {
        this->currentMangaSource = mangaSource;
        emit currentMangaSourceChanged();
    }
}

void UltimateMangaReaderCore::setCurrentManga(const QString& mangalink, const QString& mangatitle)
{
    auto res = currentMangaSource->loadMangaInfo(mangalink, mangatitle);
    if (res.isOk())
        mangaController->setCurrentManga(res.unwrap());
    else
        emit error(res.unwrapErr());
}

void UltimateMangaReaderCore::setupDirectories()
{
    if (!QDir(CONF.cacheDir).exists())
        QDir().mkpath(CONF.cacheDir);

    if (!QDir(CONF.mangaListDir).exists())
        QDir().mkpath(CONF.mangaListDir);
}

long UltimateMangaReaderCore::getCacheSize()
{
    long size = dirSize(CONF.cacheDir) / 1024 / 1024;

    return size;
}

long UltimateMangaReaderCore::getFreeSpace()
{
    QStorageInfo info(CONF.cacheDir);

    long space = info.bytesAvailable() / 1024 / 1024;
    return space;
}

void UltimateMangaReaderCore::clearCache(ClearCacheLevel level)
{
    switch (level)
    {
        case ClearImages:
            for (auto ms : mangaSources)
            {
                for (auto& info :
                     QDir(CONF.cacheDir + ms->name)
                         .entryInfoList(QDir::NoDotAndDotDot | QDir::System | QDir::Hidden | QDir::AllDirs))
                    removeDir(info.absoluteFilePath() + "/images");
            }
            break;

        case ClearInfos:
            for (auto ms : mangaSources)
                removeDir(CONF.cacheDir + ms->name, "progress.dat");

            break;

        case ClearAll:
            removeDir(CONF.cacheDir, "mangaList.dat");
            favoritesManager->clearFavorites();
            break;

        default:
            break;
    }
}
