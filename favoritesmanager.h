#ifndef READINGSTATEMANAGER_H
#define READINGSTATEMANAGER_H

#include <QMap>

#include "abstractmangasource.h"
#include "configs.h"
#include "favorite.h"

class FavoritesManager : public QObject
{
    Q_OBJECT
public:
    FavoritesManager();
    void update(MangaInfo *info);

    Favorite *findOrInsert(MangaInfo *info);
    QList<Favorite> *getFavorites();


    bool isFavorite(MangaInfo *info);
    bool toggleFavorite(MangaInfo *info);

    void deserialize();
public slots:
    void serialize();
private:
    QList<Favorite> favorites;
};

#endif // READINGSTATEMANAGER_H
