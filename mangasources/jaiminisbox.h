#ifndef JAIMINISBOX_H
#define JAIMINISBOX_H

#include "abstractmangasource.h"
#include "mangachapter.h"
#include "mangainfo.h"

class JaiminisBox : public AbstractMangaSource
{
public:
    JaiminisBox(QObject *parent, DownloadManager *dm);

    bool updateMangaList();
    MangaInfo *getMangaInfo(QString mangalink);
    QStringList getPageList(const QString &chapterlink);
    QString getImageLink(const QString &pagelink);

    void updateMangaInfoFinishedLoading(DownloadStringJob *job,
                                        MangaInfo *info);
};

#endif  // JAIMINISBOX_H
