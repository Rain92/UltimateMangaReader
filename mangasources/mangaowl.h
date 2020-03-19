#ifndef MANGAOWL_H
#define MANGAOWL_H

#include "abstractmangasource.h"
#include "defines.h"
#include "downloadqueue.h"
#include "mangachapter.h"
#include "mangainfo.h"

class MangaOwl : public AbstractMangaSource
{
public:
    MangaOwl(QObject *parent, DownloadManager *dm);

    MangaList getMangaList() override;
    void updateMangaInfoFinishedLoading(
        QSharedPointer<DownloadStringJob> job,
        QSharedPointer<MangaInfo> info) override;
    QStringList getPageList(const QString &chapterlink) override;
    QString getImageLink(const QString &pagelink) override;

private:
    QString dicturl;
};

#endif  // MANGAOWL_H
