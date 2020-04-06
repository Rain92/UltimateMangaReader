#ifndef JAIMINISBOX_H
#define JAIMINISBOX_H

#include "abstractmangasource.h"
#include "mangainfo.h"

class JaiminisBox : public AbstractMangaSource
{
public:
    JaiminisBox(QObject *parent, DownloadManager *dm);

    MangaList getMangaList() override;

    void updateMangaInfoFinishedLoading(
        QSharedPointer<DownloadStringJob> job,
        QSharedPointer<MangaInfo> info) override;
    Result<QStringList, QString> getPageList(
        const QString &chapterlink) override;
};

#endif  // JAIMINISBOX_H
