#ifndef MANGALIST_H
#define MANGALIST_H

#include <QDataStream>
#include <QStringList>

#include "enums.h"

struct MangaList
{
    QStringList titles;
    QStringList urls;
    QList<int> popularityRanks;
    bool absoluteUrls = false;
    int size = 0;

    void filter();

    void sortBy(MangaOrderMethod method = OrderByTitle);

    void append(const QString &title, const QString &url);
};

QDataStream &operator<<(QDataStream &str, const MangaList &m);

QDataStream &operator>>(QDataStream &str, MangaList &m);

#endif  // MANGALIST_H
