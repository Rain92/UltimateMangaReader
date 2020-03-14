#ifndef MANGALIST_H
#define MANGALIST_H

#include <QStringList>

struct MangaList
{
    QStringList titles;
    QStringList links;
    bool absoluteUrls = false;
    int nominalSize = 0;
    int actualSize = 0;

    void sortByTitle()
    {
        QVector<int> indices(actualSize);
        for (int i = 0; i < actualSize; ++i) indices[i] = i;

        std::sort(indices.begin(), indices.end(), [&](auto a, auto b) {
            return QString::compare(titles[a], titles[b], Qt::CaseInsensitive);
        });

        for (int i = 0; i < actualSize; i++)
            while (indices[i] != i)
            {
                titles.swapItemsAt(i, indices[i]);
                links.swapItemsAt(i, indices[i]);
                indices.swapItemsAt(i, indices[i]);
            }
    }
};

#endif  // MANGALIST_H
