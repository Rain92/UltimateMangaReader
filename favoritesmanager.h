#ifndef READINGSTATEMANAGER_H
#define READINGSTATEMANAGER_H

#include "abstractmangasource.h"
#include "defines.h"
#include "downloadmanager.h"
#include "favorite.h"

class FavoritesManager : public QObject
{
    Q_OBJECT
public:
    explicit FavoritesManager(const QList<AbstractMangaSource *> &mangasources,
                              QObject *parent = nullptr);
    void update(MangaInfo *info);

    Favorite *findOrInsert(MangaInfo *info);
    QList<Favorite> *getFavorites();

    bool isFavorite(MangaInfo *info);
    bool toggleFavorite(QSharedPointer<MangaInfo> info);

    void deserialize();

    void moveFavoriteToFront(int i);

    void updateInfos();

    void loadInfos();

    QList<QSharedPointer<MangaInfo> > favoriteinfos;
public slots:
    void serialize();
    void clearFavorites();

private:
    QList<Favorite> favorites;
    const QList<AbstractMangaSource *> &mangasources;
};

#endif  // READINGSTATEMANAGER_H
