#include "ultimatemangareadercore.h"

UltimateMangaReaderCore::UltimateMangaReaderCore(QObject* parent)
    : QObject(parent),
      mangaSources(),
      activeMangaSources(),
      currentMangaSource(nullptr),
      currentManga(),
      networkManager(new NetworkManager(this)),
      mangaController(new MangaController(networkManager, this)),
      favoritesManager(new FavoritesManager(activeMangaSources, this)),
      mangaChapterDownloadManager(new MangaChapterDownloadManager(networkManager, this)),
      suspendManager(new SuspendManager(networkManager, this)),
      settings(),
      timer(),
      autoSuspendTimer()
{
    setupDirectories();
    settings.deserialize();

    mangaSources.append(QSharedPointer<AbstractMangaSource>(new MangaPanda(networkManager)));
    mangaSources.append(QSharedPointer<AbstractMangaSource>(new MangaDex(networkManager)));
    mangaSources.append(QSharedPointer<AbstractMangaSource>(new MangaHub(networkManager)));
    mangaSources.append(QSharedPointer<AbstractMangaSource>(new MangaOwl(networkManager)));
    mangaSources.append(QSharedPointer<AbstractMangaSource>(new Mangakakalot(networkManager)));
    mangaSources.append(QSharedPointer<AbstractMangaSource>(new MangaTown(networkManager)));

    currentMangaSource = mangaSources.first().get();

    for (auto ms : mangaSources)
        ms->deserializeMangaList();

    updateActiveScources();

    favoritesManager->loadInfos();

    timer.setInterval(60 * 1000);
    connect(&timer, &QTimer::timeout, this, &UltimateMangaReaderCore::timerTick);

    // auto suspend
    connect(networkManager, &NetworkManager::activity, this, &UltimateMangaReaderCore::activity);
    connect(mangaController, &MangaController::activity, this, &UltimateMangaReaderCore::activity);

    autoSuspendTimer.setInterval(10 * 60 * 1000);
    connect(&autoSuspendTimer, &QTimer::timeout, [this]() {
        qDebug() << "Auto Suspend!";
        suspendManager->suspend();
    });
}

void UltimateMangaReaderCore::enableTimers(bool enabled)
{
    if (enabled == timer.isActive())
        return;

    if (enabled)
    {
        autoSuspendTimer.start();
        timerTick();
        QTimer::singleShot(1000 * 60 - QTime::currentTime().second() * 1000 - QTime::currentTime().msec(),
                           [this]() {
                               timer.start();
                               timerTick();
                           });
    }
    else
    {
        autoSuspendTimer.stop();
        timer.stop();
    }
}

void UltimateMangaReaderCore::activity()
{
    autoSuspendTimer.start();
}

void UltimateMangaReaderCore::timerTick()
{
    emit timeTick();
}

void UltimateMangaReaderCore::updateActiveScources()
{
    activeMangaSources.clear();
    QMap<QString, bool> enabledMangaSources;
    for (auto ms : mangaSources)
    {
        if (!settings.enabledMangaSources.contains(ms->name))
            enabledMangaSources.insert(ms->name, true);
        else
            enabledMangaSources.insert(ms->name, settings.enabledMangaSources[ms->name]);

        if (enabledMangaSources[ms->name])
            activeMangaSources.insert(ms->name, ms.get());
    }
    settings.enabledMangaSources = enabledMangaSources;

    this->currentMangaSource = nullptr;

    emit activeMangaSourcesChanged(activeMangaSources.values());
}

void UltimateMangaReaderCore::setImageSize(const QSize& size)
{
    networkManager->setImageRescaleSize(size);
}

void UltimateMangaReaderCore::setCurrentMangaSource(AbstractMangaSource* mangaSource)
{
    if (mangaSource && this->currentMangaSource != mangaSource)
    {
        this->currentMangaSource = mangaSource;
        emit currentMangaSourceChanged(mangaSource);
    }
    activity();
}

void UltimateMangaReaderCore::setCurrentManga(const QString& mangaUrl, const QString& mangatitle)
{
    auto res = currentMangaSource->loadMangaInfo(mangaUrl, mangatitle);
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

    if (!QDir(CONF.screensaverDir).exists())
        QDir().mkpath(CONF.screensaverDir);
}

void UltimateMangaReaderCore::clearDownloadCache(ClearDownloadCacheLevel level)
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
            for (auto ms : mangaSources)
                removeDir(CONF.cacheDir + ms->name);
            QFile::remove(CONF.cacheDir + "favorites.dat");
            favoritesManager->clearFavorites();
            break;

        default:
            break;
    }
}

void UltimateMangaReaderCore::updateMangaLists(QSharedPointer<UpdateProgressToken> progressToken)
{
    for (auto name : progressToken->sourcesProgress.keys())
    {
        if (progressToken->sourcesProgress[name] == 100)
            continue;

        progressToken->currentSourceName = name;
        auto ms = activeMangaSources[name];
        if (ms->uptareMangaList(progressToken.get()))
        {
            ms->mangaList.sortAndFilter();
            ms->serializeMangaList();
        }
        else
        {
            return;
        }
    }
    progressToken->sendFinished();
}
