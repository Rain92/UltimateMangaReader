#pragma once

#include "mangasourcebase.h"

// What an update of the manga list got done so far, see UpdateProgressToken::resumeState().
struct MangaHereUpdateState
{
    bool popularDone = false;
    // title, url of each popular page
    QVector<QList<QPair<QString, QString>>> popular;
};

class MangaHere : public MangaSourceBase
{
public:
    explicit MangaHere(NetworkManager *networkManager);

    Task<MangaListUpdate> fetchMangaList(UpdateProgressToken *token, MangaList current) override;
    Task<Result<MangaDetails>> fetchMangaInfo(QString mangaUrl, MangaChapterCollection known) override;
    Task<Result<QList<ImageLink>>> fetchPageList(QString chapterUrl) override;
    Task<Result<ImageLink>> resolveImage(ImageLink page) override;

private:
    QString mobileUrl;
    QString fanfoxUrl;
    QString dictionaryUrl;
    QString mangaListUrl;

    Task<Result<QStringList>> fetchPageListDesktop(QString chapterUrl, int timeout);

};
