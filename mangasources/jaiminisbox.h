#ifndef JAIMINISBOX_H
#define JAIMINISBOX_H

#include "abstractmangasource.h"
#include "mangainfo.h"

class JaiminisBox : public AbstractMangaSource
{
public:
    JaiminisBox(QObject *parent, DownloadManager *dm);

    MangaList getMangaList() override;

    Result<QSharedPointer<MangaInfo>, QString> getMangaInfo(
        const QString &mangalink) override;

    void updateMangaInfoFinishedLoading(
        QSharedPointer<DownloadStringJob> job,
        QSharedPointer<MangaInfo> info) override;
    Result<QStringList, QString> getPageList(
        const QString &chapterlink) override;

private:
    QByteArray postdatastr;
};

#endif  // JAIMINISBOX_H
