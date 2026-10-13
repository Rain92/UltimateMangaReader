#pragma once

#include <memory>

#include "luaengine.h"
#include "mangasourcebase.h"

// A source whose work a Lua script does. Its directory has:
// - manifest.json: {"name": ..., "baseUrl": ..., "headers": [{"domain": ..., "name": ..., "value": ...}]}
// - source.lua: the functions the app calls, like the ones of MangaSourceBase. They get and return tables, and
//   fail with raise() (or any Lua error):
//   - fetchMangaList(update): update = {state (a table that a retry after a failure gets again), currentSize (the
//     titles of the last update), lastProgress, progress(percent), cancelled(), clearState()}. Returns {titles,
//     urls, absoluteUrls (else relative to baseUrl), popularity (a number per manga, the higher the more popular,
//     e.g. its views) or ranks (from 1), else the order of the list} and a message if something is missing.
//   - fetchMangaInfo(url, known): known = the chapters the app has, {{title, url}}. Returns {author, artist,
//     releaseYear, genres, status, summary, cover, coverFallbacks, chapters = {{title, url}} (oldest first)}.
//   - fetchPageList(chapterUrl): the pages, urls or images {url, headers = {name = value}, xorKey, appendUrl,
//     expires}.
//   - resolveImage(page), optional: the image of a page of the page list (when the page list has pages of the
//     site), the page if it is an image already.
//
// Besides lualibs.h the script has:
// - http.get(url [, options]): waits for the download and returns {url (the final one), status, body}, fails
//   with the error of the download. options: timeout (ms, for all attempts), transferTimeout, retries, headers
//   ({name = value}), head (only the headers), maxBytes.
// - http.start(url [, options]): a request that runs meanwhile, request:wait() returns what http.get() does (the
//   same response again when it's called again). request:failed(), request:restart() (before the response).
//   http.parallel: how many downloads should run at the same time.
// - sleep(ms), log(...)
// - source: {name, baseUrl}
class LuaSource : public MangaSourceBase
{
public:
    static Result<QSharedPointer<LuaSource>> load(NetworkManager *networkManager, const QString &directory,
                                                  LuaLimits limits = {});

    Task<MangaListUpdate> fetchMangaList(UpdateProgressToken *token, MangaList current) override;
    Task<Result<MangaDetails>> fetchMangaInfo(QString mangaUrl, MangaChapterCollection known) override;
    Task<Result<QList<ImageLink>>> fetchPageList(QString chapterUrl) override;
    Task<Result<ImageLink>> resolveImage(ImageLink page) override;

    // for the host functions of the script
    QSharedPointer<DownloadBufferJob> startDownload(const DownloadRequest &request) { return download(request); }

private:
    LuaSource(NetworkManager *networkManager, LuaLimits limits);

    // shared with the state of unfinished list updates (they keep a table of the script)
    std::shared_ptr<LuaEngine> engine;
    bool hasResolveImage;
};
