#include "ultimatemangareadercore.h"

#include <QSaveFile>

UltimateMangaReaderCore::UltimateMangaReaderCore(QObject* parent)
    : QObject(parent),
      mangaSources(),
      activeMangaSources(),
      currentMangaSource(nullptr),
      networkManager(new NetworkManager(this)),
      mangaController(new MangaController(networkManager, this)),
      favoritesManager(new FavoritesManager(activeMangaSources, networkManager, this)),
      mangaChapterDownloadManager(new MangaChapterDownloadManager(networkManager, this)),
      suspendManager(new SuspendManager(networkManager, this)),
      settings(),
      standbyManager(new StandbyManager(&settings, suspendManager, networkManager,
                                        mangaChapterDownloadManager, this)),
      timer(),
      clockAlignTimer(),
      autoSuspendTimer(),
      autoSuspendStart(0),
      currentDay(QDate::currentDate().day()),
      updatingMangaLists(false),
      mangaRequests(0)
{
    setupDirectories();
    settings.deserialize();

    mangaSources.append(QSharedPointer<AbstractMangaSource>(new MangaDex(networkManager)));
    mangaSources.append(QSharedPointer<AbstractMangaSource>(new Mangakakalot(networkManager)));
    mangaSources.append(QSharedPointer<AbstractMangaSource>(new MangaTown(networkManager)));
    mangaSources.append(QSharedPointer<AbstractMangaSource>(new MangaHere(networkManager)));
    mangaSources.append(QSharedPointer<AbstractMangaSource>(new Comick(networkManager)));
    mangaSources.append(QSharedPointer<AbstractMangaSource>(new MangaPlus(networkManager)));

    // the manga lists are loaded when they are needed (AbstractMangaSource::loadMangaList())
    for (const auto& ms : std::as_const(mangaSources))
        ms->listOrder = settings.mangaOrder;

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
    activeMangaSources.clear();
    QMap<QString, bool> enabledMangaSources;
    for (const auto& ms : std::as_const(mangaSources))
    {
        if (!settings.enabledMangaSources.contains(ms->name))
            enabledMangaSources.insert(ms->name, true);
        else
            enabledMangaSources.insert(ms->name, settings.enabledMangaSources[ms->name]);

        if (enabledMangaSources[ms->name])
            activeMangaSources.insert(ms->name, ms.get());
    }
    settings.enabledMangaSources = enabledMangaSources;

    emit activeMangaSourcesChanged(activeMangaSources.values());

    // the list of a source that was disabled isn't shown anymore
    if (currentMangaSource && !activeMangaSources.values().contains(currentMangaSource))
    {
        currentMangaSource = nullptr;
        emit currentMangaSourceChanged(nullptr);
    }
}

void UltimateMangaReaderCore::setCurrentMangaSource(AbstractMangaSource* mangaSource)
{
    this->currentMangaSource = mangaSource;
    emit currentMangaSourceChanged(mangaSource);

    activity();
}

void UltimateMangaReaderCore::setCurrentManga(const QString& mangaUrl, const QString& mangatitle)
{
    if (!currentMangaSource)
        return;

    // the latest choice wins, also over a favorite that was opened meanwhile
    int request = ++mangaRequests;
    auto shownManga = mangaController->currentManga;

    auto res = currentMangaSource->loadMangaInfo(mangaUrl, mangatitle);
    if (request != mangaRequests || mangaController->currentManga != shownManga)
        return;

    if (res.isOk())
        mangaController->setCurrentManga(res.unwrap());
    else
        emit error(res.unwrapErr());
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
            for (const auto& ms : std::as_const(mangaSources))
            {
                for (auto& info :
                     QDir(CONF.cacheDir + ms->name)
                         .entryInfoList(QDir::NoDotAndDotDot | QDir::System | QDir::Hidden | QDir::AllDirs))
                    removeDir(info.absoluteFilePath() + "/images");
            }
            break;

        case ClearInfos:
            for (const auto& ms : std::as_const(mangaSources))
                removeDir(CONF.cacheDir + ms->name, "progress.dat");

            break;

        case ClearAll:
            for (const auto& ms : std::as_const(mangaSources))
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

void UltimateMangaReaderCore::updateMangaLists(QSharedPointer<UpdateProgressToken> progressToken)
{
    // the updates run event loops, a cancelled one might still be waiting for its last download
    if (updatingMangaLists)
    {
        progressToken->sendError("The previous update is still stopping, please try again.");
        return;
    }
    updatingMangaLists = true;
    standbyManager->setInhibited(true);

    bool ok = true;
    for (const auto& name : progressToken->sourcesProgress.keys())
    {
        if (progressToken->sourcesProgress[name] == 100)
            continue;

        // it might have been disabled meanwhile
        auto ms = activeMangaSources.value(name);
        if (!ms)
            continue;

        progressToken->currentSourceName = name;
        // the updates compare with it and keep its titles
        ms->loadMangaList();
        int oldSize = ms->mangaList.size;
        if (!ms->updateMangaList(progressToken.get()))
        {
            ok = false;
            break;
        }

        // A list that shrank a lot is more likely a broken update (the site changed) than the titles being
        // gone: the old one (still on disk) stays, a retry starts over.
        int newSize = ms->mangaList.size;
        if (oldSize > 0 && newSize < oldSize / 2)
        {
            qDebug() << "Update of" << name << "found only" << newSize << "of" << oldSize << "titles";
            ms->deserializeMangaList();
            if (ms == currentMangaSource)
                emit currentMangaSourceChanged(ms);
            progressToken->clearResumeState();
            progressToken->sourcesProgress[name] = 0;
            progressToken->sendError(QString("%1: the update found only %2 of %3 titles, the old list is kept.")
                                         .arg(name)
                                         .arg(newSize)
                                         .arg(oldSize));
            ok = false;
            break;
        }

        // only the updated lists, writing all of them takes a while on the device
        ms->mangaList.filter();
        ms->mangaList.sortBy(settings.mangaOrder);
        // the list on the home screen indexes into it (a cancelled update can go on for a while)
        if (ms == currentMangaSource)
            emit currentMangaSourceChanged(ms);
        if (!ms->serializeMangaList())
        {
            progressToken->sendError(QString("Couldn't save the list of %1.").arg(name));
            ok = false;
            break;
        }
        progressToken->clearResumeState();
    }

    updatingMangaLists = false;
    standbyManager->setInhibited(false);

    if (ok)
        progressToken->sendFinished();

    emit currentMangaSourceChanged(this->currentMangaSource);
}

void UltimateMangaReaderCore::sortMangaLists()
{
    QElapsedTimer timer;
    timer.start();

    // the lists that aren't loaded yet are sorted when they are (loading all of them takes seconds)
    for (const auto& ms : std::as_const(mangaSources))
    {
        ms->listOrder = settings.mangaOrder;
        if (!ms->mangaListLoaded)
            continue;
        ms->mangaList.sortBy(settings.mangaOrder);
        ms->serializeMangaList();
    }

    emit currentMangaSourceChanged(this->currentMangaSource);
}
