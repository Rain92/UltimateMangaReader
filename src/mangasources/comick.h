#ifndef COMICK_H
#define COMICK_H

#include <QSet>

#include "abstractmangasource.h"
#include "mangainfo.h"

#define RAPIDJSON_ASSERT(x) \
    if (!(x))               \
        throw QException();

#include "thirdparty/rapidjson.h"

class Comick : public AbstractMangaSource
{
public:
    struct CacheWarmUp;

    explicit Comick(NetworkManager *dm);
    virtual ~Comick();

    bool updateMangaList(UpdateProgressToken *token) override;
    Result<QSharedPointer<MangaInfo>, QString> getMangaInfo(const QString &mangaUrl,
                                                            const QString &mangaTitle) override;
    void updateMangaInfoAsync(QSharedPointer<MangaInfo> info, bool updateCover = true) override;
    Result<MangaChapterCollection, QString> updateMangaInfoFinishedLoading(
        QSharedPointer<DownloadStringJob> job, QSharedPointer<MangaInfo> info) override;
    Result<QStringList, QString> getPageList(const QString &chapterUrl) override;
    Result<QString, QString> getImageUrl(const QString &pageUrl) override;

private:
    QString apiUrl;
    QVector<QString> statuses;
    QHash<QString, QString> nextPages;
    QHash<QString, QSharedPointer<CacheWarmUp>> pageWarmUps;

    QString normalizeUrl(const QString &url) const;
    bool fetchSearch(const QString &order, int pages, MangaList &mangas, QSet<QString> &urls,
                     UpdateProgressToken *token, int progressFrom, int progressTo);
    bool fetchSitemaps(MangaList &mangas, QSet<QString> &urls, UpdateProgressToken *token);
    Result<MangaChapterCollection, QString> fetchChapters(const QString &slug,
                                                          const MangaChapterCollection &current);
};

#endif  // COMICK_H
