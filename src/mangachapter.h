#pragma once
#include <QtCore>

#include "imagelink.h"

struct MangaPage
{
    // from the page list of the chapter: the image, or a page of the site that
    // MangaSourceBase::resolveImage() resolves to it
    ImageLink link;
    // the resolved image, empty until then (or again when it expired)
    ImageLink image;
};

class MangaChapter
{
public:
    QString chapterTitle;
    QString chapterUrl;

    bool pagesLoaded;
    QList<MangaPage> pages;

    explicit MangaChapter(const QString &title, const QString &url);
    MangaChapter();
};

QDataStream &operator<<(QDataStream &str, const MangaPage &m);
QDataStream &operator>>(QDataStream &str, MangaPage &m);
QDataStream &operator<<(QDataStream &str, const MangaChapter &m);
QDataStream &operator>>(QDataStream &str, MangaChapter &m);
