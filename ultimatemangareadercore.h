#ifndef ULITIMATEMANGAREADERCORE_H
#define ULITIMATEMANGAREADERCORE_H

#include <QObject>

#include "downloadmanager.h"
#include "favoritesmanager.h"
#include "jaiminisbox.h"
#include "mangacontroller.h"
#include "mangadex.h"
#include "mangahub.h"
#include "mangakakalot.h"
#include "mangaowl.h"
#include "mangapanda.h"
#include "settings.h"

class UltimateMangaReaderCore : public QObject
{
    Q_OBJECT
public:
    explicit UltimateMangaReaderCore(QObject *parent = nullptr);

    QList<QSharedPointer<AbstractMangaSource>> mangaSources;

    QMap<QString, AbstractMangaSource *> activeMangaSources;
    QList<AbstractMangaSource *> activeMangaSources_;
    AbstractMangaSource *currentMangaSource;

    QSharedPointer<MangaInfo> currentManga;

    DownloadManager *downloadManager;
    MangaController *mangaController;
    FavoritesManager *favoritesManager;

    Settings settings;

public slots:
    void setImageSize(const QSize &size);

    void setCurrentMangaSource(AbstractMangaSource *mangaSource);
    void setCurrentManga(const QString &mangalink, const QString &mangatitle);

signals:
    void currentMangaSourceChanged();
    void currentMangaChanged();
    void currentMangaIndexChanged();

private:
    void setupDirectories();
};

#endif  // ULITIMATEMANGAREADERCORE_H
