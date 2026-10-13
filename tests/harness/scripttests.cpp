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
#include "sourceregistry.h"
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
    return table.concat({a, b, table.concat(words, "+"), whole,
                         re.compile([[(\d+)]]):replace("a1b22", [[<\1>]]),
                         tostring(none), tostring(limited), re.compile("A", "i"):match("xa")}, "|")
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

static bool writeSource(const QString &dir, const QByteArray &manifest, const QByteArray &script)
{
    QDir().mkpath(dir);
    QFile manifestFile(dir + "/manifest.json"), scriptFile(dir + "/source.lua");
    return manifestFile.open(QIODevice::WriteOnly) && manifestFile.write(manifest) == manifest.size() &&
           scriptFile.open(QIODevice::WriteOnly) && scriptFile.write(script) == script.size();
}

int scriptTests(const QString &server)
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
        check("captures, gmatch, replace, init and limit", captures && *captures == "|b|b+cd|bb|a<1>b<22>|nil|nil|a");
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
    auto manifest = QString(R"({"name": "Script", "baseUrl": "%1"})").arg(server).toUtf8();
    writeSource(dir.path() + "/Script", manifest, sourceCode);

    auto loaded = LuaSource::load(&nm, dir.path() + "/Script", {96 * 1024 * 1024, 300});
    check("a source is loaded from its directory", loaded.has_value());
    if (!loaded)
    {
        out << "  " << loaded.error().message << "\n";
        return failures;
    }
    auto source = *loaded;
    check("the manifest names the source", source->name == "Script" && source->baseUrl == server);

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
    check("the headers of a request", urls(pages("headers")).contains("X-Test: yes"));
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

    // --- loading

    {
        writeSource(dir.path() + "/Unnamed", "{}", "");
        writeSource(dir.path() + "/Broken", R"({"name": "Broken"})", "function (");
        writeSource(dir.path() + "/MangaTown", R"({"name": "MangaTown"})", "");
        QDir().mkpath(dir.path() + "/Empty");

        auto unnamed = LuaSource::load(&nm, dir.path() + "/Unnamed");
        check("a source needs a name", !unnamed && unnamed.error().message.contains("has no name"));
        auto broken = LuaSource::load(&nm, dir.path() + "/Broken");
        check("a script that doesn't compile isn't loaded",
              !broken && broken.error().message.contains("Broken/source.lua:1:"));
        auto empty = LuaSource::load(&nm, dir.path() + "/Empty");
        check("a source needs a manifest", !empty && empty.error().message.contains("manifest.json"));

        QStringList errors, builtInNames;
        auto builtIn = SourceRegistry::builtInSources(&nm);
        for (const auto &builtInSource : builtIn)
            builtInNames.append(builtInSource->name);
        SourceRegistry registry(&settings, SourceRegistry::withScriptSources(builtIn, &nm, dir.path(), &errors));
        check("a script replaces the built-in source of its name, in its place",
              dynamic_cast<LuaSource *>(registry.source("MangaTown").get()) != nullptr &&
                  registry.names().indexOf("MangaTown") == builtInNames.indexOf("MangaTown"));
        check("the other scripts are added", registry.names().endsWith("Script"));
        check("the ones that can't be loaded are left out", errors.size() == 3 && !registry.names().contains("Broken"));
    }

    return failures;
}
