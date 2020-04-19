#ifndef MANGAPANDA_H
#define MANGAPANDA_H

#include "abstractmangasource.h"
#include "mangainfo.h"

class MangaPanda : public AbstractMangaSource
{
public:
    explicit MangaPanda(NetworkManager *dm);

    bool uptareMangaList(UpdateProgressToken *token) override;
    void updateMangaInfoFinishedLoading(QSharedPointer<DownloadStringJob> job,
                                        QSharedPointer<MangaInfo> info) override;
    Result<QStringList, QString> getPageList(const QString &chapterlink) override;
    Result<QString, QString> getImageLink(const QString &pagelink) override;
};

#endif  // MANGAPANDA_H
