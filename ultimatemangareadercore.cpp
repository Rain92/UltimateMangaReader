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
      settings()
{
    setupDirectories();
    settings.deserialize();

    downloadManager->connect();

    mangaSources.append(QSharedPointer<AbstractMangaSource>(
        new MangaPanda(this, downloadManager)));
    mangaSources.append(QSharedPointer<AbstractMangaSource>(
        new JaiminisBox(this, downloadManager)));
    mangaSources.append(QSharedPointer<AbstractMangaSource>(
        new MangaDex(this, downloadManager)));
    mangaSources.append(QSharedPointer<AbstractMangaSource>(
        new MangaHub(this, downloadManager)));
    mangaSources.append(QSharedPointer<AbstractMangaSource>(
        new MangaOwl(this, downloadManager)));

    currentMangaSource = mangaSources.first().get();

    for (auto ms : mangaSources)
        ms->deserializeMangaList();

    for (auto ms : mangaSources)
        activeMangaSources.insert(ms->name, ms.get());

    favoritesManager->loadInfos();
}

void UltimateMangaReaderCore::setImageSize(const QSize& size)
{
    downloadManager->setImageRescaleSize(size);
}

void UltimateMangaReaderCore::setCurrentMangaSource(
    AbstractMangaSource* mangaSource)
{
    if (mangaSource && this->currentMangaSource != mangaSource)
    {
        this->currentMangaSource = mangaSource;
        emit currentMangaSourceChanged();
    }
}

void UltimateMangaReaderCore::setCurrentManga(const QString& mangalink,
                                              const QString& mangatitle)
{
    mangaController->setCurrentManga(QSharedPointer<MangaInfo>(
        currentMangaSource->loadMangaInfo(mangalink, mangatitle)));
}

void UltimateMangaReaderCore::setupDirectories()
{
    if (!QDir(CONF.cacheDir).exists())
        QDir().mkpath(CONF.cacheDir);

    if (!QDir(CONF.mangaListDir).exists())
        QDir().mkpath(CONF.mangaListDir);
}
