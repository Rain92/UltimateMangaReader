#ifndef DOWNLOADIMAGEDESCRIPTOR_H
#define DOWNLOADIMAGEDESCRIPTOR_H

#include <QtCore>

struct DownloadImageDescriptor
{
    DownloadImageDescriptor(const QString &imagelink, const QString &title,
                            int chapter, int page)
        : imagelink(imagelink), title(title), chapter(chapter), page(page)
    {
    }

    QString imagelink;
    QString title;
    int chapter;
    int page;
};

#endif  // DOWNLOADIMAGEDESCRIPTOR_H
