#pragma once

#include "abstractmangasource.h"
#include "mangainfo.h"

// What an update of the manga list got done so far, see UpdateProgressToken::resumeState().
struct MangaHereUpdateState
{
    bool popularDone = false;
    // title, url of each popular page
    QVector<QList<QPair<QString, QString>>> popular;
};

class MangaHere : public AbstractMangaSource
{
public:
    explicit MangaHere(NetworkManager *dm);
    virtual ~MangaHere() = default;

    bool updateMangaList(UpdateProgressToken *token) override;
    Result<MangaChapterCollection, QString> updateMangaInfoFinishedLoading(
        QSharedPointer<DownloadStringJob> job, QSharedPointer<MangaInfo> info) override;
    Result<QList<ImageLink>, QString> getPageList(const QString &chapterUrl) override;
    Result<ImageLink, QString> getImageLink(const ImageLink &page) override;

private:
    QString mobileUrl;
    QString fanfoxUrl;
    QString dictionaryUrl;
    QString mangaListUrl;

    Result<QStringList, QString> getPageListDesktop(const QString &chapterUrl, int timeout);
};
