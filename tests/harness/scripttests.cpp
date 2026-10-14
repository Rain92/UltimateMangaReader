// The scripts of sources: the Lua engine, its libraries and LuaSource, against tests/harness/testserver.py.
#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QTemporaryDir>
#include <QTimer>

#include "harness.h"
#include "luaengine.h"
#include "lualibs.h"
#include "luasource.h"
#include "networkmanager.h"
#include "settings.h"
#include "sourceindex.h"
#include "sourceregistry.h"
#include "sourceupdater.h"
#include "updateprogresstoken.h"

// An engine with the libraries that ran code, null if that failed.
static std::unique_ptr<LuaEngine> engineWith(const QByteArray &code, LuaLimits limits = {})
{
    auto engine = std::make_unique<LuaEngine>(limits);
    auto opened = LuaEngine::protect(engine->state(), 0, [](lua_State *L) { lua::openLibraries(L); });
    auto ran = opened ? engine->run(code, "test.lua") : opened;
    if (!ran)
    {
        out << "  " << ran.error().message << "\n";
        return nullptr;
    }
    return engine;
}

// What the function returns (as text) or its error.
static Result<QString> callText(LuaEngine &engine, const QByteArray &function)
{
    return engine
        .call<QString>(
            function, [](lua_State *) { return 0; },
            [](lua_State *L, int count) -> Result<QString> { return count > 0 ? lua::toQString(L, -count) : ""; })
        .wait();
}

static const char *engineCode = R"lua(
function sandbox()
    local present = {}
    for _, name in ipairs({"io", "os", "require", "package", "debug", "dofile", "loadfile", "load", "loadstring",
                           "collectgarbage", "print", "coroutine"}) do
        if _G[name] ~= nil then
            present[#present + 1] = name
        end
    end
    return table.concat(present, " ")
end

function positions()
    local s = "äöü abc äöü abc €"
    local rx = re.compile("abc")
    local f1, l1 = rx:find(s)
    local f2, l2 = rx:find(s, l1 + 1)
    local e1 = string.find(s, "abc", 1, true)
    local e2 = string.find(s, "abc", e1 + 1, true)
    local all = rx:all(re.text(s))
    local before = rx:all(s, 1, e2)
    local euro = re.compile("€"):find(s)
    local emoji = re.compile("x"):find("😀x")
    return table.concat({f1, l1, f2, l2, e1, e2, #all, all[2].first, all[2].last, all[2][0], #before, euro, #s,
                         emoji}, ",")
end

function captures()
    local a, b = re.compile("(a)|(b)"):match("b")
    local words = {}
    for word in re.compile([[\w+]]):gmatch("ab cd ef", 2, 7) do
        words[#words + 1] = word
    end
    local whole = re.compile("b+"):match("abbc")
    local none = re.compile("a"):find("abc", 10)
    local limited = re.compile("a"):match("abc", 1, 0)
    local crossing = re.compile("cd"):match("abcd", 1, 4)
    local named = re.compile("(?<key>\\w)=(\\w)"):all("a=1 b=2")
    local groups = re.compile("(?<one>x)(y)(?<two>z)"):groups()
    return table.concat({a, b, table.concat(words, "+"), whole,
                         re.compile([[(\d+)]]):replace("a1b22", [[<\1>]]),
                         tostring(none), tostring(limited), re.compile("A", "i"):match("xa"), tostring(crossing),
                         named[2].key .. named[2][2], groups.one .. groups.two}, "|")
end

function libraries()
    local j = json.decode('{"a": [1, 2.5, null, true], "b": {"c": "d"}}')
    local checks = {
        text.trim("  a b  ") == "a b",
        text.simplify(" a \n  b ") == "a b",
        html.text("a &amp; b") == "a & b",
        html.genres("<a>Action</a>, <a>Drama</a>") == "Action Drama",
        math.type(j.a[1]) == "integer" and j.a[1] == 1,
        j.a[2] == 2.5,
        j.a[3] == json.null,
        j.a[4] == true,
        j.b.c == "d",
    }
    local failed = {}
    for i, ok in ipairs(checks) do
        if not ok then
            failed[#failed + 1] = i
        end
    end
    return table.concat(failed, ",")
end

function jsonErrors()
    local ok1, e1 = pcall(json.decode, "{")
    local ok2, e2 = pcall(json.decode, string.rep("[", 300) .. string.rep("]", 300))
    return table.concat({tostring(ok1), e1.code, tostring(ok2), e2.code, e2.message}, "|")
end

function raises()
    raise("notfound", "gone", 404)
end

function fails()
    error("boom")
end

function failsWithTable()
    error({code = "http", message = "teapot", status = 418})
end

function hog()
    local t = {}
    for i = 1, 10000000 do
        t[i] = i
    end
end

function spin()
    while true do
    end
end

function small()
    return "fine"
end
)lua";

static const char *sourceCode = R"lua(
local base = source.baseUrl

function fetchPageList(test)
    if test == "get" then
        local response = http.get(base .. "/redirect")
        return {response.url, tostring(response.status), response.body}
    elseif test == "parallel" then
        local a = http.start(base .. "/delay/1")
        local b = http.start(base .. "/delay/1")
        return {a:wait().body, b:wait().body}
    elseif test == "again" then
        local request = http.start(base .. "/final")
        local first = request:wait()
        local second = request:wait()
        local restarted = pcall(request.restart, request)
        return {tostring(first == second), tostring(request:failed()), tostring(restarted), second.body}
    elseif test == "httperror" then
        http.get(base .. "/status/404")
    elseif test == "caught" then
        local ok, err = pcall(http.get, base .. "/status/404")
        return {tostring(ok), err.code, tostring(err.status)}
    elseif test == "timeout" then
        http.get(base .. "/delay/3", {timeout = 300})
    elseif test == "headers" then
        return {http.get(base .. "/headers", {headers = {["X-Test"] = "yes"}}).body}
    elseif test == "sleep" then
        sleep(400)
        return {"slept"}
    elseif test == "images" then
        return {base .. "/image/1x1",
                {url = base .. "/image/2x2", headers = {["X-A"] = "b"}, xorKey = "\1\2", appendUrl = "?x"}}
    elseif test == "log" then
        log("several", 1, nil, true, {})
        return {"logged"}
    end
    return {}
end

function fetchMangaInfo(url, known)
    return {author = url, chapters = {{title = "new", url = "/new"}, known[1]}}
end

function resolveImage(page)
    return {url = page.url .. "#resolved", headers = page.headers}
end

function fetchMangaList(update)
    if update.cancelled() then
        raise("cancelled", "stopped")
    end
    update.progress(50)
    local state = update.state
    state.calls = (state.calls or 0) + 1
    if state.calls == 1 then
        raise("network", "first attempt")
    end
    update.clearState()
    return {titles = {"B", "A", "C"}, urls = {"/b", "/a", "/c"}, popularity = {5, 9, 5}}, "partial"
end
)lua";

static bool writeFile(const QString &path, const QByteArray &content)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(content) == content.size();
}

// <id>.yaml and <id>.lua (if there is a script) in dir
static bool writeSource(const QString &dir, const QString &id, const QByteArray &definition,
                        const QByteArray &script = {})
{
    return writeFile(dir + "/" + id + ".yaml", definition) &&
           (script.isNull() || writeFile(dir + "/" + id + ".lua", script));
}

int scriptTests(const QString &server, const QString &sourcesDir)
{
    int failures = 0;
    auto check = [&](const char *what, bool ok)
    {
        out << (ok ? "PASS " : "FAIL ") << what << "\n";
        failures += !ok;
    };
    auto show = [](const Result<QString> &result)
    {
        if (!result)
            out << "  error " << result.error().code << " " << result.error().httpStatus << ": "
                << result.error().message << "\n";
        else
            out << "  " << *result << "\n";
    };

    // --- the engine and the libraries

    {
        auto engine = engineWith(engineCode);
        check("the engine runs code", engine != nullptr);
        if (!engine)
            return failures + 1;

        auto sandbox = callText(*engine, "sandbox");
        check("no files, os, loading of code or collector control", sandbox && sandbox->isEmpty());
        show(sandbox);

        auto positions = callText(*engine, "positions");
        check("regex positions are those of the UTF-8 bytes",
              positions && *positions == "8,10,19,21,8,19,2,19,21,abc,1,23,25,5");
        show(positions);

        auto captures = callText(*engine, "captures");
        check("captures, gmatch, replace, init and limit (the text ends before it), named groups",
              captures && *captures == "|b|b+cd|bb|a<1>b<22>|nil|nil|a|nil|b2|13");
        show(captures);

        auto libraries = callText(*engine, "libraries");
        check("text, html and json", libraries && libraries->isEmpty());
        show(libraries);

        auto jsonErrors = callText(*engine, "jsonErrors");
        check("invalid and too deep json fail with parse errors",
              jsonErrors && *jsonErrors == "false|parse|false|parse|The json is nested too deeply.");
        show(jsonErrors);

        auto raised = callText(*engine, "raises");
        check("raise() gives the error of the app", !raised && raised.error().code == Error::NotFound &&
                                                        raised.error().message == "gone" &&
                                                        raised.error().httpStatus == 404);
        auto failed = callText(*engine, "fails");
        check("a Lua error fails with its message", !failed && failed.error().code == Error::Failed &&
                                                        failed.error().message.endsWith("boom") &&
                                                        failed.error().message.contains("test.lua:"));
        show(failed);
        auto table = callText(*engine, "failsWithTable");
        check("an error table is an error of the app",
              !table && table.error().code == Error::Http && table.error().httpStatus == 418);
        auto missing = callText(*engine, "nothing");
        check("a missing function fails", !missing && missing.error().message.contains("no function nothing"));
    }
    {
        auto engine = engineWith(engineCode, {8 * 1024 * 1024, 300});
        auto hog = callText(*engine, "hog");
        check("the memory limit stops a script", !hog && hog.error().message.contains("not enough memory"));
        show(hog);
        auto small = callText(*engine, "small");
        check("the engine works after running out of memory", small && *small == "fine");

        QElapsedTimer timer;
        timer.start();
        auto spin = callText(*engine, "spin");
        check("the time limit stops a script that doesn't wait",
              !spin && spin.error().message.contains("300 ms without waiting") && timer.elapsed() < 3000);
        show(spin);
        check("the engine works after it stopped a script", callText(*engine, "small").value_or("") == "fine");
    }
    {
        LuaEngine engine;
        auto binary = engine.run(QByteArray("\x1bLuaT\x00\x19\x93", 8), "binary");
        check("bytecode isn't loaded", !binary && binary.error().message.contains("binary"));
        auto syntax = engine.run("function (", "broken.lua");
        check("a syntax error names the chunk and line", !syntax && syntax.error().message.contains("broken.lua:1:"));
    }

    // --- sources of scripts

    NetworkManager nm;
    Settings settings;
    nm.setDownloadSettings(QSize(1404, 1872), &settings);
    QTemporaryDir dir;
    auto definition = QString("name: Script\nbaseUrl: %1\nheaders:\n  127.0.0.1: {X-Rule: ruled}\n").arg(server);
    writeSource(dir.path(), "script", definition.toUtf8(), sourceCode);

    auto loaded = LuaSource::load(&nm, dir.path(), "script", {}, {96 * 1024 * 1024, 300});
    check("a source is loaded from its definition and script", loaded.has_value());
    if (!loaded)
    {
        out << "  " << loaded.error().message << "\n";
        return failures;
    }
    auto source = *loaded;
    check("the definition names the source", source->name == "Script" && source->baseUrl == server &&
                                                 source->id == "script" && source->iconPath.isEmpty());

    auto pages = [&](const QString &test) { return source->fetchPageList(test).wait(); };
    auto urls = [](const Result<QList<ImageLink>> &result)
    {
        QStringList list;
        for (const auto &page : result.value_or(QList<ImageLink>()))
            list.append(page.url);
        return list.join("|");
    };

    check("http.get follows redirects", urls(pages("get")) == server + "/final|200|final");
    {
        QElapsedTimer timer;
        timer.start();
        auto parallel = pages("parallel");
        check("http.start runs requests at the same time", urls(parallel) == "late|late" && timer.elapsed() < 1800);
    }
    check("a request keeps its response", urls(pages("again")) == "true|false|false|final");
    {
        auto error = pages("httperror");
        check("a failed download fails the script with its error",
              !error && error.error().code == Error::Http && error.error().httpStatus == 404);
    }
    check("pcall gets the error of a download as a table", urls(pages("caught")) == "false|http|404");
    {
        auto timeout = pages("timeout");
        check("the timeout of a request", !timeout && timeout.error().code == Error::Timeout);
    }
    check("the headers of a request and of the definition",
          urls(pages("headers")).contains("X-Test: yes") && urls(pages("headers")).contains("X-Rule: ruled"));
    {
        int ticks = 0;
        QTimer timer;
        QObject::connect(&timer, &QTimer::timeout, [&ticks]() { ticks++; });
        timer.start(50);
        QElapsedTimer elapsed;
        elapsed.start();
        auto first = source->fetchPageList("sleep");
        auto second = source->fetchPageList("sleep");
        bool both = urls(first.wait()) == "slept" && urls(second.wait()) == "slept";
        check("sleep() waits longer than the time limit, the event loop goes on", both && ticks >= 4);
        check("calls run at the same time", elapsed.elapsed() < 750);
    }
    {
        auto images = pages("images");
        bool ok = images && images->size() == 2 && images->at(0).url == server + "/image/1x1" &&
                  images->at(1).headers.size() == 1 && images->at(1).headers[0].name == "X-A" &&
                  images->at(1).headers[0].value == "b" && images->at(1).xorKey == QByteArray("\1\2", 2) &&
                  images->at(1).appendUrl == "?x";
        check("a page list of urls and images", ok);

        auto resolved = source->resolveImage(images.value_or(QList<ImageLink>{{}, {}}).at(1)).wait();
        check("resolveImage() gets and returns an image",
              resolved && resolved->url == server + "/image/2x2#resolved" && resolved->headers.size() == 1);
    }
    check("log() with any values", urls(pages("log")) == "logged");
    {
        MangaChapterCollection known;
        known.append(MangaChapter("known", "/known"));
        auto info = source->fetchMangaInfo("/manga", known).wait();
        check("fetchMangaInfo() gets the chapters the app has", info && info->author == "/manga" &&
                                                                    info->chapters.size() == 2 &&
                                                                    info->chapters[1].chapterTitle == "known");
    }
    {
        UpdateProgressToken token;
        token.currentSourceName = source->name;
        auto first = source->fetchMangaList(&token, {}).wait();
        check("a failed list update fails", first.status == MangaListUpdate::Failed &&
                                                first.error.code == Error::Network &&
                                                first.error.message == "first attempt");
        check("the progress of the update", token.sourcesProgress.value(source->name) == 50);

        auto second = source->fetchMangaList(&token, {}).wait();
        check("a retry gets the state of the failed update",
              second.status == MangaListUpdate::Partial && second.error.message == "partial");
        check("the popularity ranks the mangas, the order of the list for the same popularity",
              second.list.titles == QStringList({"B", "A", "C"}) &&
                  second.list.popularityRanks == QList<int>({1, 0, 2}) && !second.list.absoluteUrls);

        auto third = source->fetchMangaList(&token, {}).wait();
        check("clearState() starts over", third.status == MangaListUpdate::Failed &&
                                              third.error.message == "first attempt");

        token.canceled = true;
        auto cancelled = source->fetchMangaList(&token, {}).wait();
        check("the script sees the cancel", cancelled.error.code == Error::Cancelled);
    }

    // --- definitions (common.lua of the sources directory)

    if (sourcesDir.isEmpty())
    {
        check("the definitions need --sources (for common.lua)", false);
        return failures;
    }
    {
        QTemporaryDir defDir;
        auto path = defDir.path();
        QFile::copy(sourcesDir + "/common.lua", path + "/common.lua");
        auto site = QString(R"yaml(name: Site
baseUrl: %1
list:
  url: /site/dir/{page}
  pageSize: 2
  estimatedPages: 4
  mangas:
    each: '<a class="m" href="(?<url>[^"]*)" title="(?<title>[^"]*)">'
    popularity: 'Views: (\d+)'
info:
  expect: 'class="info"'
  notFound: '<p class="msg">([^<]*)</p>'
  author: {match: '<b>Author:</b>(.*?)</li>', remove: '^\[.*\]$'}
  genres: '<b>Genres:</b>(.*?)</li>'
  summary: {match: '<p class="sum">(.*?)</p>', remove: '\[.*?\]'}
  cover: '<div class="info"><img src="([^"]*)"'
  chapters:
    within: ['<ul class="chapters">', '</ul>']
    each: '<a href="(?<url>[^"]*)">(?<title>[^<]*)</a>'
    title: {lua: chapterTitle}
    order: newestFirst
pages:
  expect: '<div id="viewer"'
  images:
    within: ['<div id="viewer"', '<div class="end">|\z']
    each: '<img src="(?<url>[^"]*)" class="strip"'
  count: ['var none = (\d+)', 'var total = (\d+)']
  page: '{chapter}/{n}.html'
image:
  when: '\.html$'
  within: ['<div id="viewer"']
  url: ['<img src="([^"]*)" id="image"', '<img src="([^"]*)"']
)yaml")
                        .arg(server);
        writeSource(path, "site", site.toUtf8(), R"lua(
local spanrx = re.compile([[<span>([^<]*)</span>]])
function chapterTitle(chapter)
    local span = spanrx:match(chapter.text)
    return span and chapter.title .. ": " .. span or chapter.title
end
)lua");
        auto stubs = QByteArray("function fetchMangaInfo() end function fetchPageList() end");
        writeSource(path, "single", QString(R"yaml(name: Single
baseUrl: %1
list:
  url: /site/all
  complete: '<footer>'
  mangas:
    each: '<li><a href="(?<url>[^"]*)">(?<title>[^<]*)</a>'
  popular:
    url: /site/popular/{page}
    pages: 3
    mangas:
      each: '<b><a href="(?<url>[^"]*)">(?<title>[^<]*)</a>'
)yaml")
                                            .arg(server)
                                            .toUtf8(),
                    stubs);

        auto loadedSite = LuaSource::load(&nm, path, "site", {"common.lua"});
        check("a definition is loaded", loadedSite.has_value());
        if (!loadedSite)
        {
            out << "  " << loadedSite.error().message << "\n";
            return failures + 1;
        }
        auto siteSource = *loadedSite;

        {
            UpdateProgressToken token;
            token.currentSourceName = siteSource->name;
            auto update = siteSource->fetchMangaList(&token, {}).wait();
            QStringList ranked(update.list.size);
            for (int i = 0; i < update.list.size; i++)
                ranked[update.list.popularityRanks.value(i)] = update.list.titles[i];
            check("a paged list: until a page brings nothing new, relative urls, ranked by popularity",
                  update.status == MangaListUpdate::Complete && update.list.size == 7 &&
                      update.list.urls.value(0) == "/site/manga/1" && !update.list.absoluteUrls &&
                      ranked.first() == "Manga 7" && ranked.last() == "Manga 1");
            out << "  " << update.error.message << " " << update.list.titles.join(",") << "\n";
        }
        {
            auto single = LuaSource::load(&nm, path, "single", {"common.lua"});
            UpdateProgressToken token;
            token.currentSourceName = "Single";
            auto update = single ? (*single)->fetchMangaList(&token, {}).wait() : MangaListUpdate();
            check("a list on one page (cut off once), the popular pages first, each manga once",
                  update.status == MangaListUpdate::Complete &&
                      update.list.titles == QStringList({"Manga 3", "Manga 5", "Manga 1", "Manga 2", "Manga 4"}) &&
                      update.list.popularityRanks == QList<int>({0, 1, 2, 3, 4}));
            out << "  " << update.error.message << " " << update.list.titles.join(",") << "\n";
        }
        {
            auto info = siteSource->fetchMangaInfo(server + "/site/manga/1", {}).wait();
            QStringList chapters;
            for (const auto &chapter : info.value_or(MangaDetails()).chapters)
                chapters.append(chapter.chapterTitle + " " + chapter.chapterUrl.mid(server.size()));
            check("the fields of the info: names without placeholders, genres, removed parts, the cover url",
                  info && info->author == "A & B" && info->genres == "Action Drama" &&
                      info->summary == "Text more" && info->coverUrl == "https://example.com/cover.jpg");
            check("the chapters: oldest first, each once, the hook's titles, resolved urls",
                  chapters == QStringList({"Ch 1 /site/ch/1", "Ch 2 /site/ch/2", "Ch 3: Finale /site/ch/3"}));
            out << "  " << (info ? info->author + " | " + info->summary + " | " + chapters.join(", ")
                                 : info.error().message)
                << "\n";

            auto gone = siteSource->fetchMangaInfo(server + "/site/manga/gone", {}).wait();
            check("no chapter list and the notFound message: not found",
                  !gone && gone.error().code == Error::NotFound && gone.error().message == "It has been licensed");
            auto wrong = siteSource->fetchMangaInfo(server + "/site/ch/1", {}).wait();
            check("a page that isn't what's expected", !wrong && wrong.error().code == Error::Parse);
        }
        {
            auto strip = siteSource->fetchPageList(server + "/site/ch/3").wait();
            check("the images of a long strip, within the part of the page",
                  urls(strip) == server + "/image/1x1|" + server + "/image/2x2");
            auto paged = siteSource->fetchPageList(server + "/site/ch/1").wait();
            check("else the pages of the site, from the first count above 0",
                  urls(paged) == server + "/site/ch/1/1.html|" + server + "/site/ch/1/2.html");
            QStringList images;
            for (const auto &page : paged.value_or(QList<ImageLink>()))
            {
                auto image = siteSource->resolveImage(page).wait();
                images.append(image ? image->url.mid(server.size()) : image.error().message);
            }
            auto direct = siteSource->resolveImage(ImageLink::fromUrl(server + "/image/1x1")).wait();
            check("the images of the pages: the first pattern that matches, within the part of the page",
                  images == QStringList({"/image/1x1", "/image/2x2"}) && direct &&
                      direct->url == server + "/image/1x1");
            out << "  " << images.join(", ") << "\n";
        }

        // checked when loaded
        auto invalid = [&](const QByteArray &definition, const QByteArray &script = {}) -> QString
        {
            writeSource(path, "invalid", "name: Invalid\nbaseUrl: x\n" + definition,
                        script + " function fetchMangaList() end function fetchMangaInfo() end"
                                 " function fetchPageList() end");
            auto loaded = LuaSource::load(&nm, path, "invalid", {"common.lua"});
            return loaded ? QString() : loaded.error().message;
        };
        auto message = invalid("info:\n  autor: 'x'\n");
        check("an unknown key", message == "invalid.yaml: info.autor: unknown key");
        out << "  " << message << "\n";
        message = invalid("info:\n  author: '(x'\n");
        check("an invalid regex", message.startsWith("invalid.yaml: info.author: invalid regex"));
        out << "  " << message << "\n";
        message = invalid("list:\n  url: /\n  mangas:\n    each: '(?<name>x)'\n");
        check("a group that isn't a field", message.contains("list.mangas.each: the group name isn't a field"));
        message = invalid("info:\n  chapters:\n    each: 'x'\n    title: {lua: titleOf}\n");
        check("a hook that isn't there", message.contains("info.chapters.title.lua: invalid.lua has no function titleOf"));
        message = invalid("pages:\n  timeout: soon\n  count: 'x'\n  page: '{n}'\n");
        check("a value of the wrong kind", message == "invalid.yaml: pages.timeout: a number is expected");
        message = invalid("pages:\n  count: 'x'\n");
        check("a missing key", message == "invalid.yaml: pages.page: missing (the url of a page, with {n})");
    }

    // --- updates of the sources (SourceUpdater, with a repository of files)

    {
        QTemporaryDir updateDir;
        auto base = updateDir.path();
        auto bundled = base + "/bundled", repo = base + "/repo", installed = base + "/cache/sources";
        QDir().mkpath(bundled);
        QDir().mkpath(repo);
        auto functions =
            QByteArray("function fetchMangaList() end function fetchMangaInfo() end function fetchPageList() end");
        auto nameOf = [](const QString &id) { return id.left(1).toUpper() + id.mid(1); };
        // the sources (id, version, script) with common.lua (of the version of the first one) and their index
        auto writeSet = [&](const QString &dir, const QList<std::tuple<QString, QString, QByteArray>> &sources)
        {
            QDir().mkpath(dir);
            QFile::remove(dir + "/common.lua");
            QFile::copy(sourcesDir + "/common.lua", dir + "/common.lua");
            SourceIndex index;
            auto major = SourceIndex::Version::parse(std::get<1>(sources.first()))->major;
            index.libraries.append({"common", {}, {major, 0}, {"common.lua"}});
            for (const auto &[id, version, script] : sources)
            {
                writeSource(dir, id, QString("name: %1\nbaseUrl: x\n").arg(nameOf(id)).toUtf8(), script);
                index.sources.append({id, nameOf(id), *SourceIndex::Version::parse(version), {id + ".yaml", id + ".lua"}});
            }
            writeFile(dir + "/index.yaml", index.toYaml());
        };
        auto installedVersions = [](const QString &dir)
        {
            QStringList versions;
            for (const auto &entry : SourceIndex::read(dir).value_or(SourceIndex()).sources)
                versions.append(QString("%1 %2").arg(entry.id, entry.version.toString()));
            return versions.join(", ");
        };
        auto readText = [](const QString &path)
        {
            QFile file(path);
            return file.open(QIODevice::ReadOnly) ? QString::fromUtf8(file.readAll()) : QString();
        };

        writeSet(bundled, {{"alpha", "1.0", functions + " -- v1"}, {"gamma", "1.0", functions}});
        SourceUpdater updater(&nm, bundled, installed);
        auto repoUrl = QUrl::fromLocalFile(repo).toString();
        auto dir = updater.installBundled();
        check("the bundled sources are installed the first time",
              dir == installed && installedVersions(installed) == "alpha 1.0, gamma 1.0" &&
                  QFile::exists(installed + "/bundled.yaml"));

        writeSet(repo, {{"alpha", "1.1", functions + " -- v2"}, {"beta", "1.0", functions}});
        auto changes = updater.check(repoUrl).wait();
        check("the changes of the repository",
              changes && changes->updated == QStringList{"Alpha"} && changes->added == QStringList{"Beta"} &&
                  changes->removed == QStringList{"Gamma"} &&
                  changes->summary() == "Alpha updated, Beta new, Gamma removed");
        if (!changes)
            out << "  " << changes.error().message << "\n";

        auto updated = updater.install(changes.value_or(SourceUpdater::Changes())).wait();
        QStringList names;
        for (const auto &updatedSource : updated.value_or(QList<QSharedPointer<MangaSourceBase>>()))
            names.append(updatedSource->name);
        check("an update is installed and its sources loaded",
              updated && names == QStringList({"Alpha", "Beta"}) && installedVersions(installed) == "alpha 1.1, beta 1.0" &&
                  readText(installed + "/alpha.lua").endsWith("v2") && !QFile::exists(installed + "/gamma.yaml"));
        if (!updated)
            out << "  " << updated.error().message << "\n";
        auto again = updater.check(repoUrl + "/index.yaml").wait();
        check("then there are no changes (also with the url of the index)", again && again->isEmpty());

        writeSet(repo, {{"alpha", "1.1", functions + " -- v2"}, {"beta", "1.1", "function ("}});
        auto broken = updater.install(updater.check(repoUrl).wait().value_or(SourceUpdater::Changes())).wait();
        check("an update whose sources don't load isn't installed",
              !broken && broken.error().message.contains("beta.lua:1:") &&
                  installedVersions(installed) == "alpha 1.1, beta 1.0");
        writeSet(repo, {{"alpha", "1.1", functions + " -- changed"}, {"beta", "1.10", functions + " -- v2"}});
        auto newer = updater.install(updater.check(repoUrl).wait().value_or(SourceUpdater::Changes())).wait();
        check("only the entries with a newer version are downloaded",
              newer && installedVersions(installed) == "alpha 1.1, beta 1.10" &&
                  readText(installed + "/alpha.lua").endsWith("v2") &&
                  readText(installed + "/beta.lua").endsWith("v2"));

        writeSet(bundled, {{"alpha", "1.2", functions + " -- v3"}, {"beta", "1.9", functions}, {"gamma", "1.0", functions},
                           {"delta", "1.0", functions}});
        updater.installBundled();
        check("an update of the app brings its newer and new sources, not the removed ones",
              installedVersions(installed) == "alpha 1.2, beta 1.10, delta 1.0" &&
                  readText(installed + "/alpha.lua").endsWith("v3"));
        out << "  " << installedVersions(installed) << "\n";

        writeSet(repo, {{"alpha", "2.0", functions + " -- v4"}, {"beta", "2.0", functions}, {"delta", "1.0", functions}});
        auto otherMajor = updater.check(repoUrl).wait();
        check("the repository's sources of another major version aren't installed, and they aren't removed",
              otherMajor && otherMajor->isEmpty());

        // an update of the app to a new major version: from 0.x to this app's 1.x
        auto oldInstalled = base + "/old/sources", newBundled = base + "/newbundled";
        writeSet(oldInstalled, {{"alpha", "0.5", functions + " -- v0"}, {"beta", "0.3", functions}});
        writeSet(newBundled, {{"alpha", "1.0", functions + " -- v1"}});
        SourceUpdater newApp(&nm, newBundled, oldInstalled);
        newApp.installBundled();
        QStringList errors, loaded;
        for (const auto &source : SourceRegistry::loadDirectory(&nm, oldInstalled, &errors))
            loaded.append(source->name);
        check("a new major version of the app brings its sources, also older ones",
              installedVersions(oldInstalled) == "alpha 1.0, beta 0.3" && readText(oldInstalled + "/alpha.lua").endsWith("v1"));
        check("the sources of the old major version stay, but aren't loaded",
              loaded == QStringList{"Alpha"} && errors.isEmpty());
        writeSet(repo, {{"alpha", "0.6", functions}, {"beta", "0.4", functions}});
        auto oldRepository = newApp.check(repoUrl).wait();
        check("a repository of the old major version brings no updates", oldRepository && oldRepository->isEmpty());
        writeSet(repo, {{"alpha", "1.0", functions + " -- v1"}, {"beta", "1.0", functions}});
        auto newRepository = newApp.check(repoUrl).wait();
        check("then its sources of the new major version update the old ones",
              newRepository && newRepository->updated == QStringList{"Beta"} && newRepository->downloads == QStringList{"beta"});

        auto failed = updater.check(QUrl::fromLocalFile(base + "/none").toString()).wait();
        check("a repository that isn't there", !failed);
    }

    // --- loading

    {
        QTemporaryDir loadDir;
        auto path = loadDir.path();
        auto functions = QByteArray("function fetchMangaList() end function fetchMangaInfo() end "
                                    "function fetchPageList() end");
        writeSource(path, "unnamed", "{}");
        writeSource(path, "broken", "name: Broken\nbaseUrl: x", "function (");
        writeSource(path, "invalid", "name: Invalid\nbaseUrl: x\nname: Twice\n");
        writeSource(path, "incomplete", "name: Incomplete\nbaseUrl: x", "function fetchMangaList() end");
        writeSource(path, "mangatown", "name: MangaTown\nbaseUrl: x", functions);
        writeSource(path, "future", "name: Future\nbaseUrl: x", functions);
        writeSource(path, "other", "name: Other\nbaseUrl: x\ncookies:\n  example.com: {a: b}", functions);
        writeFile(path + "/other.png", "not a png");
        writeFile(path + "/lib.lua", "library = 'loaded'");
        writeSource(path, "uses", "name: Uses\nbaseUrl: x", functions + " assert(library == 'loaded')");

        auto unnamed = LuaSource::load(&nm, path, "unnamed");
        check("a source needs a name and a base url",
              !unnamed && unnamed.error().message.contains("name and baseUrl are required"));
        auto broken = LuaSource::load(&nm, path, "broken");
        check("a script that doesn't compile isn't loaded",
              !broken && broken.error().message.contains("broken.lua:1:"));
        auto invalid = LuaSource::load(&nm, path, "invalid");
        check("a definition with a key twice isn't loaded",
              !invalid && invalid.error().message.contains("invalid.yaml:3:"));
        if (!invalid)
            out << "  " << invalid.error().message << "\n";
        QByteArray bomb = "name: Bomb\nbaseUrl: x\na0: &a0 [x, x, x, x, x, x, x, x, x, x]\n";
        for (int i = 1; i <= 6; i++)
            bomb += QString("a%1: &a%1 [%2]\n").arg(i).arg(QString("*a%1, ").arg(i - 1).repeated(10)).toUtf8();
        writeSource(path, "bomb", bomb);
        auto exploded = LuaSource::load(&nm, path, "bomb");
        check("a definition that aliases make huge isn't read",
              !exploded && exploded.error().message.contains("too large"));
        auto missing = LuaSource::load(&nm, path, "missing");
        check("a source needs a definition", !missing && missing.error().message.contains("missing.yaml"));
        auto incomplete = LuaSource::load(&nm, path, "incomplete");
        check("a source needs the functions of the app",
              !incomplete && incomplete.error().message.contains("no fetchMangaInfo"));
        auto uses = LuaSource::load(&nm, path, "uses", {"lib.lua"});
        check("the libraries run before the script", uses.has_value());

        SourceIndex index;
        for (const auto &id : {"broken", "mangatown", "future", "other", "uses"})
            index.sources.append({id, id, {QString(id) == "future" ? LuaSource::majorVersion + 1 : LuaSource::majorVersion, 0},
                                  {id + QString(".yaml")}});
        index.libraries.append({"lib", {}, {LuaSource::majorVersion, 0}, {"lib.lua"}});
        writeFile(path + "/index.yaml", index.toYaml());
        auto reread = SourceIndex::read(path);
        check("the index is read back", reread && reread->sources.size() == 5 && reread->libraries.size() == 1 &&
                                            reread->sources[2].version.major == LuaSource::majorVersion + 1 &&
                                            reread->libraries[0].files.contains("lib.lua"));
        auto parsed = [](const char *yaml) { return SourceIndex::parse(yaml, "index.yaml").has_value(); };
        check("an index is read without the file of a library",
              parsed("libraries: [{id: lib, version: 1.0}]\nsources:\n  - {id: a, name: A, version: 1.0, files: [a.yaml]}"));
        check("an index with an id twice isn't read",
              !parsed("libraries: [{id: a, version: 1.0}]\nsources: [{id: a, name: A, version: 1.0, files: [a.yaml]}]"));
        check("an index whose source has the file of another one isn't read",
              !parsed("sources: [{id: a, name: A, version: 1.0, files: [a.yaml, b.lua]}]"));
        check("an index whose source has no definition isn't read",
              !parsed("sources: [{id: a, name: A, version: 1.0, files: [a.lua]}]"));
        check("an index whose entry has no version (major.minor) isn't read",
              !parsed("libraries: [{id: lib}]") && !parsed("libraries: [{id: lib, version: 1}]") &&
                  !parsed("libraries: [{id: lib, version: 1.0.0}]"));
        auto version = [](const char *text) { return *SourceIndex::Version::parse(text); };
        check("versions are compared number by number",
              version("1.10") > version("1.9") && version("2.0") > version("1.10") && version("1.0") == version("1.0"));

        QStringList errors, referenceNames;
        auto sources = harnessSources(&nm, path, &errors);
        for (const auto &reference : harnessSources(&nm, {}))
            referenceNames.append(reference->name);
        SourceRegistry registry(&settings, sources);
        check("a script replaces the C++ source of its name, in its place",
              dynamic_cast<LuaSource *>(registry.source("MangaTown").get()) != nullptr &&
                  registry.names().indexOf("MangaTown") == referenceNames.indexOf("MangaTown"));
        check("the other scripts are added in the order of the index", registry.names().mid(registry.names().size() - 2) == QStringList({"Other", "Uses"}));
        check("the ones that can't be loaded or are for another version of the app are left out",
              errors.size() == 1 && !registry.names().contains("Broken") && !registry.names().contains("Future"));
        check("the logo of a source is its png", registry.source("Other")->iconPath == path + "/other.png");

        int changes = 0;
        QObject::connect(&registry, &SourceRegistry::sourcesChanged, [&changes]() { changes++; });
        registry.setCurrent("Other");
        registry.setSources(SourceRegistry::loadDirectory(&nm, path));
        check("the sources can be replaced", changes == 1 && registry.names() == QStringList({"MangaTown", "Other", "Uses"}) &&
                                                 registry.current() == "Other");
        registry.setSources({});
        check("the current source goes when it isn't there anymore", registry.current().isEmpty());
    }

    return failures;
}
