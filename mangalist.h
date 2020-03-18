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

    void sortAndFilter()
    {
        Q_ASSERT(titles.size() == links.size() && actualSize == titles.size());

        for (int i = 0; i < actualSize; ++i)
        {
            if (titles[i] == "")
            {
                titles.removeAt(i);
                links.removeAt(i);
                nominalSize--;
                actualSize--;
            }
        }

        QVector<int> indices(actualSize);
        QVector<int> indicesInv(actualSize);
        for (int i = 0; i < actualSize; ++i)
            indices[i] = i;

        std::sort(indices.begin(), indices.end(), [this](int a, int b) {
            return QString::compare(this->titles[a], this->titles[b],
                                    Qt::CaseInsensitive) < 0;
        });

        for (int i = 0; i < actualSize; ++i)
            indicesInv[indices[i]] = i;

        for (int i = 0; i < actualSize; i++)
            while (i != indicesInv[i])
            {
                int j = indicesInv[i];

                titles.swapItemsAt(i, j);
                links.swapItemsAt(i, j);
                indicesInv.swapItemsAt(i, j);
            }
    }
};

#endif  // MANGALIST_H
