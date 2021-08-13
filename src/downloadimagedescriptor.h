#ifndef DOWNLOADIMAGEDESCRIPTOR_H
#define DOWNLOADIMAGEDESCRIPTOR_H

#include <QtCore>

struct DownloadImageDescriptor
{
    DownloadImageDescriptor(const QString &imageUrl, const QString &title,
                            int chapter, int page)
        : imageUrl(imageUrl), title(title), chapter(chapter), page(page)
    {
    }

    QString imageUrl;
    QString title;
    int chapter;
    int page;
};

#endif  // DOWNLOADIMAGEDESCRIPTOR_H
