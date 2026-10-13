#include "ultimatemangareadercore.h"

#include <QSaveFile>

UltimateMangaReaderCore::UltimateMangaReaderCore(QObject* parent)
    : QObject(parent),
      networkManager(new NetworkManager(this)),
      sources(new SourceRegistry(networkManager, this)),
      repository(new MangaRepository(networkManager, this)),
      mangaLists(),
      mangaController(new MangaController(networkManager, repository, this)),
      favoritesManager(new FavoritesManager(sources->active(), repository, networkManager, this)),
      mangaChapterDownloadManager(new MangaChapterDownloadManager(networkManager, repository, this)),
      suspendManager(new SuspendManager(networkManager, this)),
      settings(),
      standbyManager(new StandbyManager(&settings, suspendManager, networkManager,
                                        mangaChapterDownloadManager, this)),
      mangaListUpdater(new MangaListUpdater(sources, &mangaLists, &settings, standbyManager, this)),
      timer(),
      clockAlignTimer(),
      autoSuspendTimer(),
      autoSuspendStart(0),
      currentDay(QDate::currentDate().day()),
      mangaRequests(0)
{
    setupDirectories();
    settings.deserialize();

    // the manga lists are loaded when they are needed
    mangaLists.setOrder(settings.mangaOrder);

    updateActiveSources();

    timer.setInterval(CONF.globalTickIntervalSeconds * 1000);
    connect(&timer, &QTimer::timeout, this, &UltimateMangaReaderCore::timerTick);

    clockAlignTimer.setSingleShot(true);
    connect(&clockAlignTimer, &QTimer::timeout, this,
            [this]()
            {
                timer.start();
                timerTick();
            });

    // the favorites list shows the reading progress
    connect(mangaController, &MangaController::currentIndexChanged, favoritesManager,
            [this](const ReadingProgress &progress)
            { favoritesManager->setProgress(mangaController->currentManga.get(), progress); });

    // the chapters of the manga that is open might not be the current ones (without a connection that's no
    // news)
    connect(repository, &MangaRepository::infoUpdateFailed, this,
            [this](QSharedPointer<MangaInfo> info, const Error &error)
            {
                if (info == mangaController->currentManga && networkManager->connected)
                    emit this->error(QString("Couldn't update %1: %2").arg(info->title, error.message));
            });

    // auto suspend
    connect(networkManager, &NetworkManager::activity, this, &UltimateMangaReaderCore::activity);
    connect(mangaController, &MangaController::activity, this, &UltimateMangaReaderCore::activity);

    autoSuspendTimer.setSingleShot(true);
    connect(&autoSuspendTimer, &QTimer::timeout, this, [this]() { suspendManager->suspend(); });

    // standby
    standbyManager->setWakeDeadline([this]() { return msUntilAutoSuspend(); });
    connect(standbyManager, &StandbyManager::resumed, this, &UltimateMangaReaderCore::resumedFromStandby);
}

void UltimateMangaReaderCore::enableTimers(bool enabled)
{
    if (enabled == (timer.isActive() || clockAlignTimer.isActive()))
        return;

    if (enabled)
    {
        restartAutoSuspend();
        alignClock();
    }
    else
    {
        autoSuspendTimer.stop();
        clockAlignTimer.stop();
        timer.stop();
    }
}

// Ticks now and restarts the minute tick at the next full minute.
void UltimateMangaReaderCore::alignClock()
{
    timer.stop();
    timerTick();
    QTime now = QTime::currentTime();
    clockAlignTimer.start(1000 * 60 - now.second() * 1000 - now.msec());
}

void UltimateMangaReaderCore::restartAutoSuspend()
{
    autoSuspendStart = StandbyManager::bootTimeMs();
    autoSuspendTimer.start(CONF.autoSuspendIntervalMinutes * 60 * 1000);
}

qint64 UltimateMangaReaderCore::msUntilAutoSuspend() const
{
    if (!autoSuspendTimer.isActive())
        return -1;

    qint64 elapsed = StandbyManager::bootTimeMs() - autoSuspendStart;
    return qMax<qint64>(0, CONF.autoSuspendIntervalMinutes * 60 * 1000 - elapsed);
}

// QTimers stood still while suspended, catch them up with the time that passed.
void UltimateMangaReaderCore::resumedFromStandby()
{
    if (!autoSuspendTimer.isActive())
        return;

    qint64 remaining = msUntilAutoSuspend();
    if (remaining > 0)
    {
        autoSuspendTimer.start(remaining);
        alignClock();
    }
    else
    {
        autoSuspendTimer.stop();
        QTimer::singleShot(0, suspendManager, [this]() { suspendManager->suspend(); });
    }
}

void UltimateMangaReaderCore::activity()
{
    restartAutoSuspend();
    standbyManager->postpone();
}

void UltimateMangaReaderCore::timerTick()
{
    if (currentDay != QDate::currentDate().day())
    {
        currentDay = QDate::currentDate().day();
        favoritesManager->resetUpdatedStatus();
    }

    emit timeTick();
}

void UltimateMangaReaderCore::updateActiveSources()
{
    sources->updateActive(settings);
}

void UltimateMangaReaderCore::setCurrentManga(const QString& mangaUrl, const QString& mangatitle)
{
    auto currentMangaSource = sources->current();
    if (!currentMangaSource)
        return;

    // the latest choice wins, also over a favorite that was opened meanwhile
    int request = ++mangaRequests;
    auto shownManga = mangaController->currentManga;

    auto res = repository->loadInfo(currentMangaSource, mangaUrl, mangatitle).wait();

    if (request != mangaRequests || mangaController->currentManga != shownManga)
        return;

    if (res.has_value())
        mangaController->setCurrentManga(res.value());
    else
        emit error(res.error().message);
}

void UltimateMangaReaderCore::setupDirectories()
{
    if (!QDir(CONF.cacheDir).exists())
        QDir().mkpath(CONF.cacheDir);

    // The cache of another version of the app is laid out differently, its mangas (infos, pages, reading
    // progress) are dropped. The files in the cache directory itself and the manga lists check their format.
    QFile versionFile(CONF.cacheDir + "cacheversion");
    int layoutVersion = versionFile.open(QIODevice::ReadOnly) ? versionFile.readAll().trimmed().toInt() : 0;
    versionFile.close();
    if (layoutVersion != CONF.cacheLayoutVersion)
    {
        qDebug() << "Dropping the cache of layout version" << layoutVersion;
        QDir cache(CONF.cacheDir);
        for (const auto &dir : cache.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden))
            if (dir.fileName() != "mangalists")
                removeDir(dir.absoluteFilePath());

        QSaveFile file(versionFile.fileName());
        if (!file.open(QIODevice::WriteOnly) || file.write(QByteArray::number(CONF.cacheLayoutVersion)) < 0 ||
            !file.commit())
            qDebug() << "Couldn't write" << file.fileName();
    }

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
            for (const auto& ms : sources->all())
            {
                for (auto& info :
                     QDir(CONF.cacheDir + ms->name)
                         .entryInfoList(QDir::NoDotAndDotDot | QDir::System | QDir::Hidden | QDir::AllDirs))
                    removeDir(info.absoluteFilePath() + "/images");
            }
            break;

        case ClearInfos:
            for (const auto& ms : sources->all())
                removeDir(CONF.cacheDir + ms->name, "progress.dat");

            break;

        case ClearAll:
            for (const auto& ms : sources->all())
                removeDir(CONF.cacheDir + ms->name);
            QFile::remove(CONF.cacheDir + "favorites.dat");
            favoritesManager->clearFavorites();
            break;

        case ClearInvalidFavs:
            favoritesManager->clearInvalidFavorites();
            break;

        default:
            break;
    }
    emit downloadCacheCleared(level);
}
