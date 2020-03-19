#ifndef MANGADEX_H
#define MANGADEX_H

#include "abstractmangasource.h"
#include "defines.h"
#include "downloadqueue.h"
#include "mangachapter.h"
#include "mangainfo.h"

class MangaDex : public AbstractMangaSource
{
public:
    MangaDex(QObject *parent, DownloadManager *dm);

    MangaList getMangaList() override;
    void updateMangaInfoFinishedLoading(
        QSharedPointer<DownloadStringJob> job,
        QSharedPointer<MangaInfo> info) override;
    QStringList getPageList(const QString &chapterlink) override;
    QString getImageLink(const QString &pagelink) override;

private:
    void login();
};

#endif  // MANGADEX_H
