#ifndef ULITIMATEMANGAREADERCORE_H
#define ULITIMATEMANGAREADERCORE_H

#include <QObject>

#include "favoritesmanager.h"
#include "mangachapterdownloadmanager.h"
#include "mangacontroller.h"
#include "mangadex.h"
#include "mangakakalot.h"
#include "mangaplus.h"
#include "mangatown.h"
#include "readmanga.h"
#include "settings.h"
#include "suspendmanager.h"
#include "utils.h"

class UltimateMangaReaderCore : public QObject
{
    Q_OBJECT
public:
    explicit UltimateMangaReaderCore(QObject *parent = nullptr);

    QList<QSharedPointer<AbstractMangaSource>> mangaSources;

    QMap<QString, AbstractMangaSource *> activeMangaSources;

    AbstractMangaSource *currentMangaSource;

    QSharedPointer<MangaInfo> currentManga;

    NetworkManager *networkManager;
    MangaController *mangaController;
    FavoritesManager *favoritesManager;
    MangaChapterDownloadManager *mangaChapterDownloadManager;
    SuspendManager *suspendManager;

    Settings settings;

public:
    void setCurrentMangaSource(AbstractMangaSource *mangaSource);
    void setCurrentManga(const QString &mangaUrl, const QString &mangatitle);

    void clearDownloadCache(ClearDownloadCacheLevel level);
    void updateActiveScources();

    void updateMangaLists(QSharedPointer<UpdateProgressToken> progressToken);
    void sortMangaLists();

    void enableTimers(bool enabled);

    void activity();

signals:
    void currentMangaSourceChanged(AbstractMangaSource *source);
    void currentMangaChanged();
    void currentMangaIndexChanged();

    void activeMangaSourcesChanged(const QList<AbstractMangaSource *> &sources);

    void downloadCacheCleared(ClearDownloadCacheLevel level);

    void error(const QString &error);

    void timeTick();

private:
    QTimer timer;
    QTimer autoSuspendTimer;
    int currentDay;

    void timerTick();
    void setupDirectories();
};

#endif  // ULITIMATEMANGAREADERCORE_H
