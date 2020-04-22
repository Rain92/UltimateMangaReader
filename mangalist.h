#ifndef MANGALIST_H
#define MANGALIST_H

#include <QStringList>

struct MangaList
{
    QStringList titles;
    QStringList urls;
    bool absoluteUrls = false;
    int size = 0;

    void sortAndFilter()
    {
        Q_ASSERT(titles.size() == urls.size() && size == titles.size());

        for (int i = 0; i < size; ++i)
        {
            if (titles[i] == "")
            {
                titles.removeAt(i);
                urls.removeAt(i);
                size--;
            }
        }

        QVector<int> indices(size);
        QVector<int> indicesInv(size);
        for (int i = 0; i < size; ++i)
            indices[i] = i;

        std::sort(indices.begin(), indices.end(), [this](int a, int b) {
            return QString::compare(this->titles[a], this->titles[b], Qt::CaseInsensitive) < 0;
        });

        for (int i = 0; i < size; ++i)
            indicesInv[indices[i]] = i;

        for (int i = 0; i < size; i++)
            while (i != indicesInv[i])
            {
                int j = indicesInv[i];

                titles.swapItemsAt(i, j);
                urls.swapItemsAt(i, j);
                indicesInv.swapItemsAt(i, j);
            }
    }
};

#endif  // MANGALIST_H
