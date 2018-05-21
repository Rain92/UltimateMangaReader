#ifndef READINGSTATEMANAGER_H
#define READINGSTATEMANAGER_H

#include <QMap>

#include "mangasources/abstractmangasource.h"
#include "configs.h"
#include "readingstate.h"

class ReadingStateManager
{
public:
    ReadingStateManager();
    void update(MangaInfo *info);

    ReadingState *findOrInsert(const MangaInfo &info);
    QList<ReadingState> getFavorites();
    void toggleFavorite(MangaInfo *info);

    void serialize(const ReadingState &rs);
    void deserialize();
private:
    QMap<QString, ReadingState> states;
};

#endif // READINGSTATEMANAGER_H
