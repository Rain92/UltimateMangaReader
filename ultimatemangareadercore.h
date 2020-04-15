#ifndef ULITIMATEMANGAREADERCORE_H
#define ULITIMATEMANGAREADERCORE_H

#include <QObject>

#include "downloadmanager.h"
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

public:
    void setImageSize(const QSize &size);

    void setCurrentMangaSource(AbstractMangaSource *mangaSource);
    void setCurrentManga(const QString &mangalink, const QString &mangatitle);

    long getCacheSize();
    long getFreeSpace();
    void clearDownloadCache(ClearDownloadCacheLevel level);
    void updateActiveScources();

signals:
    void currentMangaSourceChanged();
    void currentMangaChanged();
    void currentMangaIndexChanged();

    void activeMangaSourcesChanged();

    void error(const QString &error);

    void timeTick();

private:
    QTimer timer;
    void timerTick();

    void setupDirectories();
};

#endif  // ULITIMATEMANGAREADERCORE_H
