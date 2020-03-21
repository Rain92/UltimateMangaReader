#ifndef ULITIMATEMANGAREADERCORE_H
#define ULITIMATEMANGAREADERCORE_H

#include <QObject>

#include "downloadmanager.h"
#include "favoritesmanager.h"
#include "favoriteswidget.h"
#include "homewidget.h"
#include "jaiminisbox.h"
#include "mangadex.h"
#include "mangahub.h"
#include "mangakakalot.h"
#include "mangaowl.h"
#include "mangapanda.h"
#include "mangareaderwidget.h"
#include "settings.h"

class UlitimateMangaReaderCore : public QObject
{
    Q_OBJECT
public:
    explicit UlitimateMangaReaderCore(QObject *parent = nullptr);

public slots:
    //    void setCurrentSource(AbstractMangaSource *source);
    //    void setCurrentManga(AbstractMangaSource *source);
    //    void setImageSize(QSize);

    //    void advanceMangaPage(bool direction);

signals:

private:
    QList<QSharedPointer<AbstractMangaSource>> mangaSources;
    QList<AbstractMangaSource *> activeMangaSources;

    AbstractMangaSource *currentMangaSources;
    QSharedPointer<MangaInfo> currentManga;

    DownloadManager downloadManager;
    FavoritesManager favoritesManager;

    Settings settings;
};

#endif  // ULITIMATEMANGAREADERCORE_H
