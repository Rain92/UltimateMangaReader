#include "mangalist.h"

void MangaList::filter()
{
    Q_ASSERT(titles.size() == urls.size() && size == titles.size() && size == popularityRanks.size());

    for (int i = 0; i < size; ++i)
    {
        if (titles[i].trimmed() == "")
        {
            titles.removeAt(i);
            urls.removeAt(i);
            popularityRanks.removeAt(i);
            size--;
            i--;
        }
    }
}

void MangaList::sortBy(MangaOrderMethod method)
{
    Q_ASSERT(titles.size() == urls.size() && size == titles.size() && size == popularityRanks.size());

    QVector<int> indices(size);
    QVector<int> indicesInv(size);
    for (int i = 0; i < size; ++i)
        indices[i] = i;

    if (method == OrderByTitle)
        std::sort(indices.begin(), indices.end(), [this](int a, int b) {
            return QString::compare(this->titles[a], this->titles[b], Qt::CaseInsensitive) < 0;
        });
    else  // if (method == OrderByPopularity)
        std::sort(indices.begin(), indices.end(),
                  [this](int a, int b) { return this->popularityRanks[a] < this->popularityRanks[b]; });

    for (int i = 0; i < size; ++i)
        indicesInv[indices[i]] = i;

    for (int i = 0; i < size; i++)
        while (i != indicesInv[i])
        {
            int j = indicesInv[i];

            titles.swapItemsAt(i, j);
            urls.swapItemsAt(i, j);
            popularityRanks.swapItemsAt(i, j);
            indicesInv.swapItemsAt(i, j);
        }
}

void MangaList::append(const QString &title, const QString &url)
{
    titles.append(title);
    urls.append(url);
    popularityRanks.append(size++);
}

QDataStream &operator<<(QDataStream &str, const MangaList &m)
{
    str << m.titles << m.urls << m.popularityRanks << m.absoluteUrls << m.size;

    return str;
}

QDataStream &operator>>(QDataStream &str, MangaList &m)
{
    m.titles.clear();
    m.urls.clear();
    m.popularityRanks.clear();

    str >> m.titles >> m.urls >> m.popularityRanks >> m.absoluteUrls >> m.size;

    return str;
}
