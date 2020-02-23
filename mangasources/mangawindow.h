#ifndef MANGAWINDOW_H
#define MANGAWINDOW_H

#include "abstractmangasource.h"
#include "mangachapter.h"
#include "mangainfo.h"

class MangaWindow : public AbstractMangaSource
{
public:
    MangaWindow(QObject *parent, DownloadManager *dm);

    bool updateMangaList() override;
    QSharedPointer<MangaInfo> getMangaInfo(const QString &mangalink) override;
    void updateMangaInfoFinishedLoading(
        QSharedPointer<DownloadStringJob> job,
        QSharedPointer<MangaInfo> info) override;
    QStringList getPageList(const QString &chapterlink) override;
    QString getImageLink(const QString &pagelink) override;
};

#endif  // MANGAWINDOW_H
