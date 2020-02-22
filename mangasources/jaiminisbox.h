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
    QSharedPointer<MangaInfo> getMangaInfo(const QString &mangalink);
    QStringList getPageList(const QString &chapterlink);
    QString getImageLink(const QString &pagelink);

    void updateMangaInfoFinishedLoading(QSharedPointer<DownloadStringJob> job,
                                        QSharedPointer<MangaInfo> info);

private:
    QByteArray postdatastr;
};

#endif  // JAIMINISBOX_H
