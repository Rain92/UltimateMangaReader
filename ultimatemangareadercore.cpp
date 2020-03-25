#include "ultimatemangareadercore.h"

UltimateMangaReaderCore::UltimateMangaReaderCore(QObject* parent)
    : QObject(parent),
      mangaSources(),
      activeMangaSources(),
      currentMangaSource(nullptr),
      currentManga(),
      downloadManager(new DownloadManager(this)),
      mangaController(new MangaController(downloadManager, this)),
      favoritesManager(new FavoritesManager(activeMangaSources_, this)),
      settings()
{
    setupDirectories();
    settings.deserialize();

    downloadManager->connect();

    mangaSources.append(QSharedPointer<AbstractMangaSource>(
        new MangaPanda(this, downloadManager)));
    mangaSources.append(QSharedPointer<AbstractMangaSource>(
        new JaiminisBox(this, downloadManager)));
    //    mangasources.append(QSharedPointer<AbstractMangaSource>(new
    //    MangaDex(this, downloadManager)));
    //        mangasources.append(QSharedPointer<AbstractMangaSource>(new
    //        Mangakakalot(this, downloadManager)));
    mangaSources.append(QSharedPointer<AbstractMangaSource>(
        new MangaHub(this, downloadManager)));
    mangaSources.append(QSharedPointer<AbstractMangaSource>(
        new MangaOwl(this, downloadManager)));

    currentMangaSource = mangaSources.first().get();

    favoritesManager->deserialize();

    for (auto ms : mangaSources)
        ms->deserializeMangaList();

    for (auto ms : mangaSources)
        activeMangaSources.insert(ms->name, ms.get());
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
    if (!QDir(cachedir).exists())
        QDir().mkpath(cachedir);

    if (!QDir(mangalistdir).exists())
        QDir().mkpath(mangalistdir);
}
