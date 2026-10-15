#include "luasource.h"

#include <QDateTime>
#include <QDir>
#include <QPointer>
#include <algorithm>
#include <chrono>
#include <numeric>

#include "lualibs.h"
#include "staticsettings.h"
#include "updateprogresstoken.h"
#include "yamlreader.h"

using lua::push;
using lua::rawField;
using lua::stringField;
using lua::toQString;

static LuaSource *sourceOf(lua_State *L)
{
    return static_cast<LuaSource *>(LuaEngine::of(L)->host);
}

template <typename T>
static int collect(lua_State *L)
{
    static_cast<T *>(lua_touserdata(L, 1))->~T();
    return 0;
}

// --- http

// The download until its response was taken, then the response (the user value), so the buffer goes now and not
// when the collector gets to the request. The one of http.get() keeps no response: the request has a finalizer,
// which keeps it (and its user value) for another cycle of the collector, and the emergency collection near the
// memory limit runs no finalizers (a large body could fail the script).
struct Request
{
    QSharedPointer<DownloadBufferJob> job;
    bool keepResponse = true;
};
static const char *requestType = "umr.Request";

static bool integerField(lua_State *L, int index, const char *name, int &value)
{
    bool found = rawField(L, index, name) == LUA_TNUMBER;
    if (found)
        value = int(lua_tointeger(L, -1));
    lua_pop(L, 1);
    return found;
}

// http.get(url [, options]), http.start(url [, options])
static DownloadRequest requestFromArgs(lua_State *L)
{
    DownloadRequest request;
    request.url = QString::fromUtf8(luaL_checkstring(L, 1));
    if (lua_isnoneornil(L, 2))
        return request;

    luaL_checktype(L, 2, LUA_TTABLE);
    integerField(L, 2, "timeout", request.timeout);
    integerField(L, 2, "transferTimeout", request.transferTimeout);
    integerField(L, 2, "retries", request.maxRetries);
    integerField(L, 2, "retryDelay", request.retryDelay);
    integerField(L, 2, "maxRetryDelay", request.maxRetryDelay);
    int maxBytes = 0;
    if (integerField(L, 2, "maxBytes", maxBytes))
        request.maxBytes = maxBytes;
    if (rawField(L, 2, "retryStatuses") == LUA_TTABLE)
    {
        for (lua_Integer i = 1; lua_rawgeti(L, -1, i) == LUA_TNUMBER; i++)
        {
            request.retryStatuses.append(int(lua_tointeger(L, -1)));
            lua_pop(L, 1);
        }
        lua_pop(L, 1);
    }
    lua_pop(L, 1);
    request.headOnly = rawField(L, 2, "head") == LUA_TBOOLEAN && lua_toboolean(L, -1);
    lua_pop(L, 1);

    if (rawField(L, 2, "headers") == LUA_TTABLE)
    {
        lua_pushnil(L);
        while (lua_next(L, -2))
        {
            if (lua_type(L, -2) == LUA_TSTRING && lua_type(L, -1) == LUA_TSTRING)
                request.headers.append(HttpHeader{lua_tostring(L, -2), lua_tostring(L, -1)});
            lua_pop(L, 1);
        }
    }
    lua_pop(L, 1);
    return request;
}

// The response of the finished request at index, fails with its error.
static int pushResponse(lua_State *L, int index)
{
    auto request = static_cast<Request *>(lua_touserdata(L, index));
    if (!request->job)
    {
        lua_getiuservalue(L, index, 1);
        return 1;
    }
    auto job = request->job;
    if (!job->isCompleted)
        lua::raise(L, job->error);

    lua_createtable(L, 0, 3);
    push(L, job->url);
    lua_setfield(L, -2, "url");
    lua_pushinteger(L, job->httpStatus);
    lua_setfield(L, -2, "status");
    lua_pushlstring(L, job->buffer.constData(), size_t(job->buffer.size()));
    lua_setfield(L, -2, "body");

    if (request->keepResponse)
    {
        lua_pushvalue(L, -1);
        lua_setiuservalue(L, index, 1);
    }
    request->job.reset();
    return 1;
}

static int requestWaited(lua_State *L, int, lua_KContext index)
{
    // a wait with a limit that ended first
    auto request = static_cast<Request *>(lua_touserdata(L, int(index)));
    if (request->job && !request->job->isFinished())
        return 0;
    return pushResponse(L, int(index));
}

// Waits for the request at index (yields until it's finished, or for at most maxWait ms if it is > 0).
static int waitFor(lua_State *L, int index, int maxWait = 0)
{
    auto request = static_cast<Request *>(lua_touserdata(L, index));
    if (request->job && !request->job->isFinished())
        LuaEngine::wait(L, {{request->job}, maxWait});
    else if (LuaEngine::shouldYield(L))
        // e.g. the pages of a list update that were downloaded meanwhile, the GUI goes on between them
        LuaEngine::wait(L, {{}, 0});
    else
        return pushResponse(L, index);
    return lua_yieldk(L, 0, index, requestWaited);
}

// The index of the first request of the list (at 1) that is finished, 0 if none is. running: gets the others.
static lua_Integer finishedRequest(lua_State *L, QList<QSharedPointer<DownloadJobBase>> *running)
{
    for (lua_Integer i = 1; lua_rawgeti(L, 1, i) != LUA_TNIL; i++)
    {
        auto request = static_cast<Request *>(luaL_testudata(L, -1, requestType));
        lua_pop(L, 1);
        if (!request)
            luaL_argerror(L, 1, "a list of requests expected");
        if (!request->job || request->job->isFinished())
            return i;
        if (running)
            running->append(request->job);
    }
    lua_pop(L, 1);
    return 0;
}

static int anyWaited(lua_State *L, int, lua_KContext)
{
    auto i = finishedRequest(L, nullptr);
    if (i > 0)
        lua_pushinteger(L, i);
    else
        lua_pushnil(L);
    return 1;
}

// http.waitAny(requests [, ms]): the index of one of the requests that has its response (or failed), after
// waiting for one (for at most ms if it's > 0, then nil)
static int httpWaitAny(lua_State *L)
{
    luaL_checktype(L, 1, LUA_TTABLE);
    int maxWait = int(luaL_optinteger(L, 2, 0));
    lua_settop(L, 1);
    QList<QSharedPointer<DownloadJobBase>> running;
    auto i = finishedRequest(L, &running);
    if (i == 0 && running.isEmpty())
        return luaL_argerror(L, 1, "no requests");
    if (i > 0 && !LuaEngine::shouldYield(L))
    {
        lua_pushinteger(L, i);
        return 1;
    }
    // (one that is finished goes on after the GUI)
    LuaEngine::wait(L, {i > 0 ? QList<QSharedPointer<DownloadJobBase>>() : running, i > 0 ? 0 : maxWait});
    return lua_yieldk(L, 0, 0, anyWaited);
}

static int httpStart(lua_State *L)
{
    auto request = requestFromArgs(L);
    auto userdata = static_cast<Request *>(lua_newuserdatauv(L, sizeof(Request), 1));
    new (userdata) Request{sourceOf(L)->startDownload(request)};
    luaL_setmetatable(L, requestType);
    return 1;
}

static int httpGet(lua_State *L)
{
    httpStart(L);
    static_cast<Request *>(lua_touserdata(L, -1))->keepResponse = false;
    return waitFor(L, lua_gettop(L));
}

static int requestWait(lua_State *L)
{
    luaL_checkudata(L, 1, requestType);
    int maxWait = int(luaL_optinteger(L, 2, 0));
    lua_settop(L, 1);
    return waitFor(L, 1, maxWait);
}

static int requestFailed(lua_State *L)
{
    auto request = static_cast<Request *>(luaL_checkudata(L, 1, requestType));
    lua_pushboolean(L, request->job && request->job->hasFailed());
    return 1;
}

static int requestDone(lua_State *L)
{
    auto request = static_cast<Request *>(luaL_checkudata(L, 1, requestType));
    lua_pushboolean(L, !request->job || request->job->isFinished());
    return 1;
}

static int requestRestart(lua_State *L)
{
    auto request = static_cast<Request *>(luaL_checkudata(L, 1, requestType));
    if (!request->job)
        return luaL_error(L, "the request has its response");
    request->job->restart();
    return 0;
}

// --- sleep, log

static int sleepFor(lua_State *L)
{
    LuaEngine::wait(L, {{}, int(luaL_checkinteger(L, 1))});
    return lua_yield(L, 0);
}

static int wallClock(lua_State *L)
{
    lua_pushinteger(L, QDateTime::currentSecsSinceEpoch());
    return 1;
}

static int clockMs(lua_State *L)
{
    auto now = std::chrono::steady_clock::now().time_since_epoch();
    lua_pushinteger(L, std::chrono::duration_cast<std::chrono::milliseconds>(now).count());
    return 1;
}

static int logMessage(lua_State *L)
{
    QStringList parts;
    int count = lua_gettop(L);
    luaL_checkstack(L, 2, "too many values to log");
    for (int i = 1; i <= count; i++)
    {
        parts.append(QString::fromUtf8(luaL_tolstring(L, i, nullptr)));
        lua_pop(L, 1);
    }
    qDebug().noquote() << sourceOf(L)->name + ":" << parts.join(' ');
    return 0;
}

// --- list updates

// update.progress(p), update.cancelled(), ... of fetchMangaList(update), for as long as the update runs
struct UpdateHandle
{
    QPointer<UpdateProgressToken> token;
    // the last list, and its titles by the absolute urls (made at the first lookup)
    MangaList current;
    QString baseUrl;
    QHash<QString, QString> knownTitles;
};
static const char *updateHandleType = "umr.UpdateHandle";

static UpdateProgressToken *tokenOf(lua_State *L)
{
    auto handle = static_cast<UpdateHandle *>(lua_touserdata(L, lua_upvalueindex(1)));
    if (!handle->token)
        luaL_error(L, "the update is over");
    return handle->token;
}

static int updateProgress(lua_State *L)
{
    tokenOf(L)->sendProgress(int(luaL_checkinteger(L, 1)));
    // e.g. a long loop over the mangas, the GUI goes on meanwhile
    if (lua_isyieldable(L) && LuaEngine::shouldYield(L))
    {
        LuaEngine::wait(L, {{}, 0});
        return lua_yield(L, 0);
    }
    return 0;
}

static int updateCancelled(lua_State *L)
{
    lua_pushboolean(L, tokenOf(L)->canceled);
    return 1;
}

static int updateClearState(lua_State *L)
{
    tokenOf(L)->clearResumeState();
    return 0;
}

static int updateKnownTitle(lua_State *L)
{
    tokenOf(L);
    auto handle = static_cast<UpdateHandle *>(lua_touserdata(L, lua_upvalueindex(1)));
    auto absolute = [handle](const QString &url) { return url.startsWith("http") ? url : handle->baseUrl + url; };
    if (handle->knownTitles.isEmpty())
    {
        const auto &current = handle->current;
        handle->knownTitles.reserve(current.size);
        for (int i = 0; i < current.titles.size() && i < current.urls.size(); i++)
            handle->knownTitles.insert(current.absoluteUrls ? current.urls[i] : handle->baseUrl + current.urls[i],
                                       current.titles[i]);
    }
    auto title = handle->knownTitles.value(absolute(toQString(L, 1)));
    if (title.isEmpty())
        return 0;
    push(L, title);
    return 1;
}

// The table a list update keeps for a retry (UpdateProgressToken::resumeState()), in the registry of the script.
struct ScriptTable
{
    ScriptTable(std::shared_ptr<LuaEngine> engine, int ref) : engine(std::move(engine)), ref(ref) {}
    ScriptTable(const ScriptTable &) = delete;
    ~ScriptTable() { luaL_unref(engine->state(), LUA_REGISTRYINDEX, ref); }

    std::shared_ptr<LuaEngine> engine;
    int ref;
};
struct LuaUpdateState
{
    std::shared_ptr<ScriptTable> table;
};

// --- conversions

static void pushImageLink(lua_State *L, const ImageLink &link)
{
    lua_createtable(L, 0, 5);
    push(L, link.url);
    lua_setfield(L, -2, "url");
    if (!link.xorKey.isEmpty())
    {
        lua_pushlstring(L, link.xorKey.constData(), size_t(link.xorKey.size()));
        lua_setfield(L, -2, "xorKey");
    }
    if (!link.appendUrl.isEmpty())
    {
        push(L, link.appendUrl);
        lua_setfield(L, -2, "appendUrl");
    }
    if (link.expires > 0)
    {
        lua_pushinteger(L, link.expires);
        lua_setfield(L, -2, "expires");
    }
    lua_createtable(L, 0, int(link.headers.size()));
    for (const auto &header : link.headers)
    {
        lua_pushlstring(L, header.name.constData(), size_t(header.name.size()));
        lua_pushlstring(L, header.value.constData(), size_t(header.value.size()));
        lua_rawset(L, -3);
    }
    lua_setfield(L, -2, "headers");
}

// a url or {url, headers, xorKey, appendUrl, expires}
static Result<ImageLink> toImageLink(lua_State *L, int index)
{
    if (lua_type(L, index) == LUA_TSTRING)
        return ImageLink::fromUrl(toQString(L, index));
    if (lua_type(L, index) != LUA_TTABLE)
        return Err(Error::Parse, "The script returned no image.");

    ImageLink link;
    link.url = stringField(L, index, "url");
    link.appendUrl = stringField(L, index, "appendUrl");
    if (rawField(L, index, "xorKey") == LUA_TSTRING)
    {
        size_t length = 0;
        const char *key = lua_tolstring(L, -1, &length);
        link.xorKey = QByteArray(key, qsizetype(length));
    }
    lua_pop(L, 1);
    if (rawField(L, index, "expires") == LUA_TNUMBER)
        link.expires = lua_tointeger(L, -1);
    lua_pop(L, 1);
    if (rawField(L, index, "headers") == LUA_TTABLE)
    {
        lua_pushnil(L);
        while (lua_next(L, -2))
        {
            if (lua_type(L, -2) == LUA_TSTRING && lua_type(L, -1) == LUA_TSTRING)
                link.headers.append(HttpHeader{lua_tostring(L, -2), lua_tostring(L, -1)});
            lua_pop(L, 1);
        }
    }
    lua_pop(L, 1);

    if (link.url.isEmpty())
        return Err(Error::Parse, "The script returned an image without url.");
    return link;
}

static QStringList stringList(lua_State *L, int index, const char *name)
{
    QStringList list;
    if (rawField(L, index, name) == LUA_TTABLE)
    {
        lua_Integer n = lua_Integer(lua_rawlen(L, -1));
        list.reserve(qsizetype(n));
        for (lua_Integer i = 1; i <= n; i++)
        {
            lua_rawgeti(L, -1, i);
            list.append(toQString(L, -1));
            lua_pop(L, 1);
        }
    }
    lua_pop(L, 1);
    return list;
}

static QList<double> numberList(lua_State *L, int index, const char *name)
{
    QList<double> list;
    if (rawField(L, index, name) == LUA_TTABLE)
    {
        lua_Integer n = lua_Integer(lua_rawlen(L, -1));
        list.reserve(qsizetype(n));
        for (lua_Integer i = 1; i <= n; i++)
        {
            lua_rawgeti(L, -1, i);
            list.append(lua_tonumber(L, -1));
            lua_pop(L, 1);
        }
    }
    lua_pop(L, 1);
    return list;
}

static QList<int> integerList(lua_State *L, int index, const char *name)
{
    QList<int> list;
    if (rawField(L, index, name) == LUA_TTABLE)
    {
        lua_Integer n = lua_Integer(lua_rawlen(L, -1));
        list.reserve(qsizetype(n));
        for (lua_Integer i = 1; i <= n; i++)
        {
            lua_rawgeti(L, -1, i);
            list.append(int(lua_tointeger(L, -1)));
            lua_pop(L, 1);
        }
    }
    lua_pop(L, 1);
    return list;
}

// --- the source

LuaSource::LuaSource(NetworkManager *networkManager, LuaLimits limits)
    : MangaSourceBase(networkManager), engine(std::make_shared<LuaEngine>(limits)), hasResolveImage(false)
{
    engine->host = this;
}

// headers, cookies: {domain: {name: value}}
static Result<QList<std::pair<QString, HttpHeader>>> domainValues(const QVariantMap &definition, const QString &key,
                                                                  const QString &file)
{
    QList<std::pair<QString, HttpHeader>> values;
    if (!definition.contains(key))
        return values;
    if (definition[key].typeId() != QMetaType::QVariantMap)
        return Err(Error::Parse, QString("%1: %2: a mapping of domains is expected.").arg(file, key));
    auto domains = definition[key].toMap();
    for (auto domain = domains.cbegin(); domain != domains.cend(); ++domain)
    {
        if (domain.value().typeId() != QMetaType::QVariantMap)
            return Err(Error::Parse,
                       QString("%1: %2.%3: a mapping of names and values is expected.").arg(file, key, domain.key()));
        auto map = domain.value().toMap();
        for (auto it = map.cbegin(); it != map.cend(); ++it)
            values.append({domain.key(), HttpHeader{it.key().toUtf8(), it.value().toString().toUtf8()}});
    }
    return values;
}

// images: {retries, retryDelay, maxRetryDelay}, each optional
static Result<ImageRetries> readImageRetries(const QVariantMap &definition, const QString &file)
{
    ImageRetries retries;
    if (!definition.contains("images"))
        return retries;
    if (definition["images"].typeId() != QMetaType::QVariantMap)
        return Err(Error::Parse, QString("%1: images: a mapping is expected.").arg(file));
    auto images = definition["images"].toMap();
    for (auto it = images.cbegin(); it != images.cend(); ++it)
    {
        int *value = it.key() == "retries"         ? &retries.maxRetries
                     : it.key() == "retryDelay"    ? &retries.retryDelay
                     : it.key() == "maxRetryDelay" ? &retries.maxRetryDelay
                                                   : nullptr;
        if (!value)
            return Err(Error::Parse, QString("%1: images.%2: unknown key").arg(file, it.key()));
        auto type = it.value().typeId();
        bool ok = false;
        *value = it.value().toInt(&ok);
        if ((type != QMetaType::Int && type != QMetaType::LongLong) || !ok || *value < 0)
            return Err(Error::Parse, QString("%1: images.%2: a number (0 or more) is expected").arg(file, it.key()));
    }
    return retries;
}

Result<QSharedPointer<LuaSource>> LuaSource::load(NetworkManager *networkManager, const QString &directory,
                                                  const QString &id, const QStringList &libraries, LuaLimits limits)
{
    QDir dir(directory);
    auto definitionName = id + ".yaml";
    QFile definitionFile(dir.filePath(definitionName));
    if (!definitionFile.open(QIODevice::ReadOnly))
        return Err(Error::Storage, QString("Couldn't read %1.").arg(definitionFile.fileName()));
    auto read = readYaml(definitionFile.readAll(), definitionName);
    if (!read)
        return Err(read.error());
    if (read->typeId() != QMetaType::QVariantMap)
        return Err(Error::Parse, QString("%1: a mapping is expected.").arg(definitionName));
    auto definition = read->toMap();

    QSharedPointer<LuaSource> source(new LuaSource(networkManager, limits));
    source->id = id;
    source->name = definition.value("name").toString();
    source->baseUrl = definition.value("baseUrl").toString();
    if (source->name.isEmpty() || source->baseUrl.isEmpty())
        return Err(Error::Parse, QString("%1: name and baseUrl are required.").arg(definitionName));
    if (dir.exists(id + ".png"))
        source->iconPath = dir.filePath(id + ".png");

    auto headers = domainValues(definition, "headers", definitionName);
    if (!headers)
        return Err(headers.error());
    for (const auto &[domain, header] : *headers)
        source->headerRules.add(domain, header.name, header.value);
    auto cookies = domainValues(definition, "cookies", definitionName);
    if (!cookies)
        return Err(cookies.error());
    for (const auto &[domain, cookie] : *cookies)
        networkManager->addCookie("." + domain, cookie.name, cookie.value);
    auto images = readImageRetries(definition, definitionName);
    if (!images)
        return Err(images.error());
    source->imageRetries = *images;

    auto opened = LuaEngine::protect(source->engine->state(), 0, [&source, &definition](lua_State *L) {
        lua::openLibraries(L);

        luaL_newmetatable(L, requestType);
        lua_pushcfunction(L, collect<Request>);
        lua_setfield(L, -2, "__gc");
        static const luaL_Reg requestMethods[] = {
            {"wait", requestWait}, {"failed", requestFailed}, {"done", requestDone},
            {"restart", requestRestart}, {nullptr, nullptr}};
        luaL_newlib(L, requestMethods);
        lua_setfield(L, -2, "__index");
        lua_pop(L, 1);

        luaL_newmetatable(L, updateHandleType);
        lua_pushcfunction(L, collect<UpdateHandle>);
        lua_setfield(L, -2, "__gc");
        lua_pop(L, 1);

        static const luaL_Reg httpFunctions[] = {{"get", httpGet}, {"start", httpStart}, {"waitAny", httpWaitAny}, {nullptr, nullptr}};
        luaL_newlib(L, httpFunctions);
        lua_pushinteger(L, CONF.parallelDownloadsHigh);
        lua_setfield(L, -2, "parallel");
        lua_setglobal(L, "http");

        lua_register(L, "sleep", sleepFor);
        lua_register(L, "clock", clockMs);
        lua_register(L, "time", wallClock);
        lua_register(L, "log", logMessage);

        lua_createtable(L, 0, 4);
        push(L, source->id);
        lua_setfield(L, -2, "id");
        push(L, source->name);
        lua_setfield(L, -2, "name");
        push(L, source->baseUrl);
        lua_setfield(L, -2, "baseUrl");
        lua::pushVariant(L, definition);
        lua_setfield(L, -2, "definition");
        lua_setglobal(L, "source");
    });
    if (!opened)
        return Err(opened.error());

    // the libraries, then the script of the source
    auto scripts = libraries;
    if (dir.exists(id + ".lua"))
        scripts.append(id + ".lua");
    for (const auto &script : std::as_const(scripts))
    {
        QFile file(dir.filePath(script));
        if (!file.open(QIODevice::ReadOnly))
            return Err(Error::Storage, QString("Couldn't read %1.").arg(file.fileName()));
        auto loaded = source->engine->run(file.readAll(), script);
        if (!loaded)
            return Err(loaded.error());
    }

    auto L = source->engine->state();
    auto prepared = LuaEngine::protect(L, 0, [](lua_State *state) {
        if (lua_getglobal(state, "prepare") == LUA_TFUNCTION)
            lua_call(state, 0, 0);
        else
            lua_pop(state, 1);
        for (auto function : {"fetchMangaList", "fetchMangaInfo", "fetchPageList"})
        {
            if (lua_getglobal(state, function) != LUA_TFUNCTION)
                luaL_error(state, "The source has no %s.", function);
            lua_pop(state, 1);
        }
    });
    if (!prepared)
        return Err(prepared.error());

    source->hasResolveImage = lua_getglobal(L, "resolveImage") == LUA_TFUNCTION;
    lua_pop(L, 1);

    return source;
}

Task<MangaListUpdate> LuaSource::fetchMangaList(UpdateProgressToken *token, MangaList current)
{
    // a table of the script that a retry gets again
    auto &state = token->resumeState<LuaUpdateState>();
    auto engine = this->engine;

    auto pushArgs = [this, &state, token, engine, &current](lua_State *L) {
        lua_createtable(L, 0, 7);

        if (!state.table)
        {
            lua_newtable(L);
            state.table = std::make_shared<ScriptTable>(engine, luaL_ref(L, LUA_REGISTRYINDEX));
        }
        lua_rawgeti(L, LUA_REGISTRYINDEX, state.table->ref);
        lua_setfield(L, -2, "state");

        lua_pushinteger(L, current.size);
        lua_setfield(L, -2, "currentSize");
        lua_pushinteger(L, token->sourcesProgress.value(token->currentSourceName));
        lua_setfield(L, -2, "lastProgress");

        auto handle = static_cast<UpdateHandle *>(lua_newuserdatauv(L, sizeof(UpdateHandle), 0));
        new (handle) UpdateHandle{token, current, baseUrl, {}};
        luaL_setmetatable(L, updateHandleType);
        for (auto [name, function] :
             {std::pair{"progress", updateProgress}, std::pair{"cancelled", updateCancelled},
              std::pair{"clearState", updateClearState}, std::pair{"knownTitle", updateKnownTitle}})
        {
            lua_pushvalue(L, -1);
            lua_pushcclosure(L, function, 1);
            lua_setfield(L, -3, name);
        }
        lua_pop(L, 1);
        return 1;
    };

    // {titles, urls [, popularity | ranks] [, absoluteUrls]} and a message if something is missing (Partial)
    auto convert = [](lua_State *L, int count) -> Result<MangaListUpdate> {
        int index = lua_gettop(L) - count + 1;
        if (count < 1 || lua_type(L, index) != LUA_TTABLE)
            return Err(Error::Parse, "The script returned no manga list.");

        MangaList list;
        list.titles = stringList(L, index, "titles");
        list.urls = stringList(L, index, "urls");
        if (list.titles.size() != list.urls.size())
            return Err(Error::Parse, "The script returned more titles than urls or the other way round.");
        list.size = int(list.titles.size());
        list.absoluteUrls = rawField(L, index, "absoluteUrls") == LUA_TBOOLEAN && lua_toboolean(L, -1);
        lua_pop(L, 1);

        // the most popular first, the order of the list for the same popularity (a sort in the script takes
        // ~0.6 s on the device for 24000 mangas)
        auto popularity = numberList(L, index, "popularity");
        if (!popularity.isEmpty())
        {
            if (popularity.size() != list.size)
                return Err(Error::Parse, "The script returned a popularity for some of the mangas only.");
            QList<int> order(list.size);
            std::iota(order.begin(), order.end(), 0);
            std::stable_sort(order.begin(), order.end(),
                             [&popularity](int a, int b) { return popularity[a] > popularity[b]; });
            list.popularityRanks.resize(list.size);
            for (int rank = 0; rank < list.size; rank++)
                list.popularityRanks[order[rank]] = rank;
        }
        else
        {
            auto ranks = integerList(L, index, "ranks");
            list.popularityRanks.reserve(list.size);
            for (int i = 0; i < list.size; i++)
                list.popularityRanks.append(i < ranks.size() ? ranks[i] - 1 : i);
        }

        if (count >= 2 && lua_type(L, index + 1) == LUA_TSTRING)
            return MangaListUpdate::partial(list, {Error::Failed, toQString(L, index + 1), 0});
        return MangaListUpdate::complete(list);
    };

    QElapsedTimer timer;
    timer.start();
    auto result = co_await engine->call<MangaListUpdate>("fetchMangaList", pushArgs, convert);
    // the list (the pages, all of its mangas) is garbage now, and so is the state of an update that didn't fail
    // (the updater would drop it a bit later)
    if (result && result->status != MangaListUpdate::Failed)
        token->clearResumeState();
    engine->collectGarbage();
    qDebug() << name << "list update:" << timer.elapsed() << "ms, script memory" << engine->memoryInUse() / 1024
             << "KB after it, at most" << engine->memoryPeak() / 1024 << "KB";
    co_return result ? *result : MangaListUpdate::failed(result.error());
}

Task<Result<MangaDetails>> LuaSource::fetchMangaInfo(QString mangaUrl, MangaChapterCollection known)
{
    auto pushArgs = [&mangaUrl, &known](lua_State *L) {
        push(L, mangaUrl);
        lua_createtable(L, int(known.size()), 0);
        lua_Integer i = 0;
        for (const auto &chapter : std::as_const(known))
        {
            lua_createtable(L, 0, 3);
            push(L, chapter.chapterTitle);
            lua_setfield(L, -2, "title");
            push(L, chapter.chapterUrl);
            lua_setfield(L, -2, "url");
            lua_pushboolean(L, chapter.pagesLoaded);
            lua_setfield(L, -2, "pagesLoaded");
            lua_rawseti(L, -2, ++i);
        }
        return 2;
    };

    // {author, artist, releaseYear, genres, status, summary, cover, coverFallbacks, chapters = {{title, url}}}
    auto convert = [](lua_State *L, int count) -> Result<MangaDetails> {
        int index = lua_gettop(L) - count + 1;
        if (count < 1 || lua_type(L, index) != LUA_TTABLE)
            return Err(Error::Parse, "The script returned no manga info.");

        MangaDetails details;
        details.author = stringField(L, index, "author");
        details.artist = stringField(L, index, "artist");
        details.releaseYear = stringField(L, index, "releaseYear");
        details.genres = stringField(L, index, "genres");
        details.status = stringField(L, index, "status");
        details.summary = stringField(L, index, "summary");
        details.coverUrl = stringField(L, index, "cover");
        details.coverFallbackUrls = stringList(L, index, "coverFallbacks");

        if (rawField(L, index, "chapters") == LUA_TTABLE)
        {
            lua_Integer n = lua_Integer(lua_rawlen(L, -1));
            for (lua_Integer i = 1; i <= n; i++)
            {
                if (lua_rawgeti(L, -1, i) == LUA_TTABLE)
                    details.chapters.append(MangaChapter(stringField(L, -1, "title"), stringField(L, -1, "url")));
                lua_pop(L, 1);
            }
        }
        lua_pop(L, 1);
        return details;
    };

    co_return co_await engine->call<MangaDetails>("fetchMangaInfo", pushArgs, convert);
}

Task<Result<QList<ImageLink>>> LuaSource::fetchPageList(QString chapterUrl)
{
    auto pushArgs = [&chapterUrl](lua_State *L) {
        push(L, chapterUrl);
        return 1;
    };

    // the pages: urls or images (see toImageLink())
    auto convert = [](lua_State *L, int count) -> Result<QList<ImageLink>> {
        int index = lua_gettop(L) - count + 1;
        if (count < 1 || lua_type(L, index) != LUA_TTABLE)
            return Err(Error::Parse, "The script returned no page list.");

        QList<ImageLink> pages;
        lua_Integer n = lua_Integer(lua_rawlen(L, index));
        for (lua_Integer i = 1; i <= n; i++)
        {
            lua_rawgeti(L, index, i);
            auto page = toImageLink(L, -1);
            lua_pop(L, 1);
            if (!page)
                return Err(page.error());
            pages.append(*page);
        }
        return pages;
    };

    co_return co_await engine->call<QList<ImageLink>>("fetchPageList", pushArgs, convert);
}

Task<Result<ImageLink>> LuaSource::resolveImage(ImageLink page)
{
    if (!hasResolveImage)
        co_return page;

    auto pushArgs = [&page](lua_State *L) {
        pushImageLink(L, page);
        return 1;
    };
    auto convert = [](lua_State *L, int count) -> Result<ImageLink> {
        if (count < 1)
            return Err(Error::Parse, "The script returned no image.");
        return toImageLink(L, lua_gettop(L) - count + 1);
    };

    co_return co_await engine->call<ImageLink>("resolveImage", pushArgs, convert);
}
