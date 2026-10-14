#include "mangaliststore.h"

#include <QSaveFile>

#include "cachefile.h"
#include "perftimer.h"
#include "staticsettings.h"

static QString listPath(const QString &source)
{
    return CONF.mangaListDir + source + "_mangalist.dat";
}

MangaList &MangaListStore::list(const QString &source)
{
    auto it = lists.find(source);
    if (it != lists.end())
        return it->second;

    PerfTimer timer("loadMangaList");
    if (read(source) && !lists[source].isSortedBy(listOrder))
    {
        lists[source].sortBy(listOrder);
        save(source);
    }
    return lists[source];
}

void MangaListStore::remove(const QString &source)
{
    lists.erase(source);
    QFile::remove(listPath(source));
}

bool MangaListStore::isLoaded(const QString &source) const
{
    return lists.contains(source);
}

bool MangaListStore::save(const QString &source)
{
    // the old list stays until the new one is complete (the lists have megabytes)
    QSaveFile file(listPath(source));
    if (!file.open(QIODevice::WriteOnly))
        return false;

    QDataStream out(&file);
    writeCacheFileHeader(out, CacheFile::MangaList);
    out << lists[source];
    return out.status() == QDataStream::Ok && file.commit();
}

bool MangaListStore::reload(const QString &source)
{
    return read(source);
}

bool MangaListStore::read(const QString &source)
{
    auto &list = lists[source];
    list = MangaList();

    QFile file(listPath(source));
    if (!file.open(QIODevice::ReadOnly))
        return false;

    QDataStream in(&file);
    if (readCacheFileHeader(in, CacheFile::MangaList))
        in >> list;

    bool consistent = list.urls.size() == list.titles.size() &&
                      list.popularityRanks.size() == list.titles.size() && list.size == list.titles.size();
    if (in.status() != QDataStream::Ok || !consistent)
    {
        qDebug() << "Couldn't read the manga list of" << source;
        list = MangaList();
        return false;
    }

    return true;
}

QStringList MangaListStore::setOrder(MangaOrderMethod order)
{
    listOrder = order;

    // the lists that aren't loaded yet are sorted when they are (loading all of them takes seconds)
    QStringList sorted;
    for (auto &[source, list] : lists)
    {
        list.sortBy(order);
        save(source);
        sorted.append(source);
    }
    return sorted;
}
