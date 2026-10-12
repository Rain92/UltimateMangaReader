#ifndef MANGATOWN_H
#define MANGATOWN_H

#include "abstractmangasource.h"
#include "mangainfo.h"

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

class MangaTown : public AbstractMangaSource
{
public:
    MangaTown(NetworkManager *dm);
    virtual ~MangaTown() = default;

    bool updateMangaList(UpdateProgressToken *token) override;
    Result<MangaChapterCollection, QString> updateMangaInfoFinishedLoading(
        QSharedPointer<DownloadStringJob> job, QSharedPointer<MangaInfo> info) override;
    Result<QStringList, QString> getPageList(const QString &chapterUrl) override;
    Result<QString, QString> getImageUrl(const QString &pageUrl) override;

private:
    QString dictionaryUrl;

    QString licensedMessage(const QString &buffer);
};

#endif  // MANGATOWN_H
