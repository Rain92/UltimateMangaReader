#ifndef ULITIMATEMANGAREADERCORE_H
#define ULITIMATEMANGAREADERCORE_H

#include <QObject>

#include "downloadmanager.h"
#include "downloadqueue.h"
#include "favoritesmanager.h"
#include "jaiminisbox.h"
#include "mangacontroller.h"
#include "mangadex.h"
#include "mangahub.h"
#include "mangaowl.h"
#include "mangapanda.h"
#include "settings.h"
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

    DownloadManager *downloadManager;
    MangaController *mangaController;
    FavoritesManager *favoritesManager;

    Settings settings;
    DownloadQueue backgroundDownloader;

public:
    void setImageSize(const QSize &size);

    void setCurrentMangaSource(AbstractMangaSource *mangaSource);
    void setCurrentManga(const QString &mangalink, const QString &mangatitle);

    long getCacheSize();
    long getFreeSpace();
    void clearDownloadCache(ClearDownloadCacheLevel level);
    void updateActiveScources();

    void updateMangaLists(QSharedPointer<UpdateProgressToken> progressToken);

    void enableTimer(bool enabled);

    void downloadMangaChapters(QSharedPointer<MangaInfo> mangaInfo, int fromChapter, int toChapter);

signals:
    void currentMangaSourceChanged(AbstractMangaSource *source);
    void currentMangaChanged();
    void currentMangaIndexChanged();

    void activeMangaSourcesChanged(const QList<AbstractMangaSource *> &sources);

    void error(const QString &error);

    void timeTick();

private:
    QTimer timer;
    void timerTick();

    void setupDirectories();
};

#endif  // ULITIMATEMANGAREADERCORE_H
