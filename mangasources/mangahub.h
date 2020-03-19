#ifndef MANGAHUB_H
#define MANGAHUB_H

#include "abstractmangasource.h"
#include "defines.h"
#include "downloadqueue.h"
#include "mangachapter.h"
#include "mangainfo.h"

class MangaHub : public AbstractMangaSource
{
public:
    MangaHub(QObject *parent, DownloadManager *dm);

    MangaList getMangaList() override;
    void updateMangaInfoFinishedLoading(
        QSharedPointer<DownloadStringJob> job,
        QSharedPointer<MangaInfo> info) override;
    QStringList getPageList(const QString &chapterlink) override;
    QString getImageLink(const QString &pagelink) override;

private:
    QString dicturl;
    int binarySearchNumPages(const QRegularExpressionMatch &imagerxmatch,
                             int upperBound, int lowerBound, bool upperChecked);

    inline QString buildImgUrl(const QRegularExpressionMatch &imagerxmatch,
                               int i)
    {
        return imagerxmatch.captured(1) + QString::number(i) +
               imagerxmatch.captured(2);
    }
};

#endif  // MANGAHUB_H
