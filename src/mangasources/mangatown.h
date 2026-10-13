#pragma once

#include "mangasourcebase.h"

// What an update of the manga list got done so far, see UpdateProgressToken::resumeState().
struct MangaTownUpdateState
{
    MangaList mangas;
    QSet<QString> urls;
    QList<qint64> views;
    int nextPage = 1;
    // requested but not processed yet, in order
    QStringList queuedPages;
    // short pages are the end of the directory, unless a full page follows them
    QStringList shortPages;
    QSet<QString> refetchedPages;
    int processedPages = 0;
};

class MangaTown : public MangaSourceBase
{
public:
    explicit MangaTown(NetworkManager *networkManager);

    Task<MangaListUpdate> fetchMangaList(UpdateProgressToken *token, MangaList current) override;
    Task<Result<MangaDetails>> fetchMangaInfo(QString mangaUrl, MangaChapterCollection known) override;
    Task<Result<QList<ImageLink>>> fetchPageList(QString chapterUrl) override;
    Task<Result<ImageLink>> resolveImage(ImageLink page) override;

private:
    QString dictionaryUrl;

    QString licensedMessage(const QString &buffer);
};
