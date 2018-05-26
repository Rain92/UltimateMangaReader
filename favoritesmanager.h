#ifndef READINGSTATEMANAGER_H
#define READINGSTATEMANAGER_H

#include <QMap>

#include "abstractmangasource.h"
#include "configs.h"
#include "favorite.h"

class FavoritesManager
{
public:
    FavoritesManager();
    void update(MangaInfo *info);

    Favorite *findOrInsert(MangaInfo *info);
    QList<Favorite> getFavorites();


    bool isFavorite(MangaInfo *info);
    bool toggleFavorite(MangaInfo *info);

    void serialize();
    void deserialize();
private:
    QMap<QString, Favorite> favorites;
};

#endif // READINGSTATEMANAGER_H
