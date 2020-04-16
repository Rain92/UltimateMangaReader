#ifndef MANGAHUB_H
#define MANGAHUB_H

#include "abstractmangasource.h"
#include "mangainfo.h"

class MangaHub : public AbstractMangaSource
{
public:
    explicit MangaHub(DownloadManager *dm);

    bool uptareMangaList(UpdateProgressToken *token) override;
    void updateMangaInfoFinishedLoading(QSharedPointer<DownloadStringJob> job,
                                        QSharedPointer<MangaInfo> info) override;
    Result<QStringList, QString> getPageList(const QString &chapterlink) override;

private:
    QString dicturl;
    int binarySearchNumPages(const QRegularExpressionMatch &imagerxmatch, int lowerBound, int upperBound,
                             bool upperChecked);

    inline QString buildImgUrl(const QRegularExpressionMatch &imagerxmatch, int i)
    {
        return imagerxmatch.captured(1) + QString::number(i) + imagerxmatch.captured(2);
    }
};

#endif  // MANGAHUB_H
