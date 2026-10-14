#pragma once

#include <memory>

#include "luaengine.h"
#include "mangasourcebase.h"

// A source whose work Lua scripts do. Its files are in a directory of sources (see sources/README.md):
// - <id>.yaml: its definition, {name, baseUrl, headers = {domain: {name: value}} for the requests to the domain and
//   its subdomains, cookies = {domain: {name: value}}, ...}. The app reads these, the scripts all of it.
// - the libraries of the directory (e.g. common.lua, which runs the rest of the definition), then <id>.lua if
//   there is one. After them prepare() is called if it's there (e.g. to check the definition), a failure fails the
//   load.
// Then these functions must be there, the ones that the app calls, like the ones of MangaSourceBase. They get and
// return tables, and fail with raise() (or any Lua error):
// - fetchMangaList(update): update = {state (a table that a retry after a failure gets again), currentSize (the
//   titles of the last update), lastProgress, progress(percent), cancelled(), clearState()}. Returns {titles,
//   urls, absoluteUrls (else relative to baseUrl), popularity (a number per manga, the higher the more popular,
//   e.g. its views) or ranks (from 1), else the order of the list} and a message if something is missing.
// - fetchMangaInfo(url, known): known = the chapters the app has, {{title, url}}. Returns {author, artist,
//   releaseYear, genres, status, summary, cover, coverFallbacks, chapters = {{title, url}} (oldest first)}.
// - fetchPageList(chapterUrl): the pages, urls or images {url, headers = {name = value}, xorKey, appendUrl,
//   expires}.
// - resolveImage(page), optional: the image of a page of the page list (when the page list has pages of the
//   site), the page if it is an image already.
//
// Besides lualibs.h the scripts have:
// - http.get(url [, options]): waits for the download and returns {url (the final one), status, body}, fails
//   with the error of the download. options: timeout (ms, for all attempts), transferTimeout, retries, headers
//   ({name = value}), head (only the headers), maxBytes.
// - http.start(url [, options]): a request that runs meanwhile, request:wait([ms]) returns what http.get() does
//   (the same response again when it's called again), with ms nil if the response isn't there after ms.
//   request:failed(), request:restart() (before the response). http.parallel: how many downloads should run at
//   the same time.
// - sleep(ms), log(...), clock() (milliseconds of a monotonic clock, e.g. for the time that attempts may take)
// - source: {id, name, baseUrl, definition (the YAML as tables)}
class LuaSource : public MangaSourceBase
{
public:
    // The major version of the sources that this app runs (see SourceIndex::Version): it goes up with a change of
    // the interface above that breaks the sources.
    static const int majorVersion = 1;

    // The source <id> of the directory, with the libraries (files of the directory, run before its script).
    static Result<QSharedPointer<LuaSource>> load(NetworkManager *networkManager, const QString &directory,
                                                  const QString &id, const QStringList &libraries = {},
                                                  LuaLimits limits = {});

    Task<MangaListUpdate> fetchMangaList(UpdateProgressToken *token, MangaList current) override;
    Task<Result<MangaDetails>> fetchMangaInfo(QString mangaUrl, MangaChapterCollection known) override;
    Task<Result<QList<ImageLink>>> fetchPageList(QString chapterUrl) override;
    Task<Result<ImageLink>> resolveImage(ImageLink page) override;

    // for the host functions of the script
    QSharedPointer<DownloadBufferJob> startDownload(const DownloadRequest &request) { return download(request); }

    // the base name of its files
    QString id;

private:
    LuaSource(NetworkManager *networkManager, LuaLimits limits);

    // shared with the state of unfinished list updates (they keep a table of the script)
    std::shared_ptr<LuaEngine> engine;
    bool hasResolveImage;
};
