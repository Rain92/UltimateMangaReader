#ifndef MANGADEX_H
#define MANGADEX_H

#include "abstractmangasource.h"
#include "mangachapter.h"
#include "mangainfo.h"

class MangaDex : public AbstractMangaSource
{
public:
    MangaDex(QObject *parent, DownloadManager *dm);

    void initialize();

    bool updateMangaList();
    MangaInfo *getMangaInfo(QString mangalink);
    QStringList getPageList(const QString &chapterlink);
    QString getImageLink(const QString &pagelink);

    void updateMangaInfoFinishedLoading(DownloadStringJob *job,
                                        MangaInfo *info);

private:
    void login();
};

#endif  // MANGADEX_H
