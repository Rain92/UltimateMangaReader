#pragma once

#include <QRegularExpression>
#include <QTextDocument>

#include "downloadqueue.h"
#include "imagelink.h"
#include "mangachaptercollection.h"
#include "mangalist.h"
#include "networkmanager.h"
#include "result.h"
#include "staticsettings.h"
#include "task.h"
#include "updateprogresstoken.h"

// What a source knows about a manga, besides its identity (the source, the url and the title).
struct MangaDetails
{
    QString author;
    QString artist;
    QString releaseYear;
    QString genres;
    QString status;
    QString summary;
    QString coverUrl;
    // tried in turn when the cover can't be downloaded from coverUrl (e.g. the same file on other hosts)
    QStringList coverFallbackUrls;
    // oldest first
    MangaChapterCollection chapters;
};

// What an update of a manga list got.
struct MangaListUpdate
{
    enum Status
    {
        Complete,
        // usable, but something is missing (the error says what), e.g. the popularity order
        Partial,
        Failed
    };

    Status status = Failed;
    MangaList list;
    Error error;

    static MangaListUpdate complete(const MangaList &list) { return {Complete, list, {}}; }
    static MangaListUpdate partial(const MangaList &list, const Error &error) { return {Partial, list, error}; }
    static MangaListUpdate failed(const Error &error) { return {Failed, {}, error}; }
};

// A source of mangas: it fetches the list of its mangas, the info of a manga with its chapters, the pages of a
// chapter and the images of the pages, and returns them. What the app does with them (the cache, the merges of
// the chapters, the covers, the downloads of the images) is in the core: MangaRepository and MangaListStore.
//
// The calls are coroutines (see Task): they wait for their downloads without blocking, several of them can run
// at the same time.
class MangaSourceBase
{
public:
    explicit MangaSourceBase(NetworkManager *networkManager);
    virtual ~MangaSourceBase() = default;

    QString name;
    // the urls of a manga list without absolute urls are relative to it
    QString baseUrl;

    // All mangas of the source. current: the list of the last update (e.g. for the titles an update doesn't
    // get again). The token takes the progress (0 to 100) and says when the update is cancelled (it fails with
    // Cancelled then). An update that failed can keep what it got done in token->resumeState(): an update with
    // the same token goes on from there.
    virtual Task<MangaListUpdate> fetchMangaList(UpdateProgressToken *token, MangaList current) = 0;

    // The info of a manga with its chapters. known: the chapters that the app has (e.g. an update can keep the
    // uploads whose pages it loaded).
    virtual Task<Result<MangaDetails>> fetchMangaInfo(QString mangaUrl, MangaChapterCollection known) = 0;

    // The pages of a chapter: their images, or pages of the site that resolveImage() resolves.
    virtual Task<Result<QList<ImageLink>>> fetchPageList(QString chapterUrl) = 0;

    // The image of a page of a page list, by default the page itself. It is resolved again when it expired.
    virtual Task<Result<ImageLink>> resolveImage(ImageLink page);

    // The headers of the source's requests to url (e.g. the Referer an image host wants).
    HttpHeaders requestHeaders(const QString &url) const;

protected:
    NetworkManager *networkManager;
    // e.g. the Referer the image hosts want
    HeaderRules headerRules;

    // The downloads of the source, with its headers (before the request's own ones).
    QSharedPointer<DownloadBufferJob> download(DownloadRequest request);
    QSharedPointer<DownloadBufferJob> download(const QString &url, int timeout = 6000,
                                               int maxRetries = DownloadRequest().maxRetries);
    Task<NetworkManager::ImageSizes> probeImageSizes(QStringList urls, int timeout);
    // for a DownloadQueue
    std::function<HttpHeaders(const QString &)> requestHeadersFunction() const;

    QString htmlToPlainText(const QString &str);

    // The first group of the first match of rx in text, empty without a match (or for an empty pattern).
    static QString capture(const QRegularExpression &rx, const QString &text);
    // Genres from the text of a page, separated by spaces.
    QString genresFromHtml(const QString &html);

private:
    QTextDocument htmlConverter;
};
