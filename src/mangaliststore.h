#pragma once

#include <map>

#include "enums.h"
#include "mangalist.h"

// The manga lists of the sources, by the name of the source. A list is read from its file the first time it is
// needed: the lists of all sources have tens of MB (seconds to read on the device, more in memory) and most
// sessions don't need them.
class MangaListStore
{
public:
    // The list of the source, empty if there is none. The reference stays valid.
    MangaList &list(const QString &source);
    bool isLoaded(const QString &source) const;
    // Writes the list into its file.
    bool save(const QString &source);
    // Forgets the list and deletes its file (e.g. of a source that isn't there anymore).
    void remove(const QString &source);
    // Reads the list again from its file (empty if there is none).
    bool reload(const QString &source);

    // The order of the lists: a list that was saved in another one is sorted when it's loaded.
    MangaOrderMethod order() const { return listOrder; }
    // Sorts the lists that are loaded and saves them, returns their sources.
    QStringList setOrder(MangaOrderMethod order);

private:
    std::map<QString, MangaList> lists;
    MangaOrderMethod listOrder = OrderByPopularity;

    bool read(const QString &source);
};
