#ifndef JAIMINISBOX_H
#define JAIMINISBOX_H

#include "abstractmangasource.h"
#include "mangachapter.h"
#include "mangainfo.h"

class JaiminisBox : public AbstractMangaSource
{
public:
    JaiminisBox(QObject *parent, DownloadManager *dm);

    MangaList getMangaList() override;

    QSharedPointer<MangaInfo> getMangaInfo(const QString &mangalink) override;

    void updateMangaInfoFinishedLoading(
        QSharedPointer<DownloadStringJob> job,
        QSharedPointer<MangaInfo> info) override;
    QStringList getPageList(const QString &chapterlink) override;
    QString getImageLink(const QString &pagelink) override;

private:
    QByteArray postdatastr;
};

#endif  // JAIMINISBOX_H
