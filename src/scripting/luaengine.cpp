#include "luaengine.h"

#include <QDebug>

#include "lualib.h"

// after this the waits of a script go through the event loop (shouldYield())
static const int yieldSliceMs = 50;

static const char *codeNames[] = {"failed", "network", "timeout", "cancelled", "http", "parse", "notfound", "storage"};

static int runProtected(lua_State *L)
{
    auto f = static_cast<const std::function<void(lua_State *)> *>(lua_touserdata(L, -1));
    lua_pop(L, 1);
    (*f)(L);
    return lua_gettop(L);
}

Result<> LuaEngine::protect(lua_State *L, int nargs, const std::function<void(lua_State *)> &f, int nresults)
{
    if (!lua_checkstack(L, 2))
        return Err(Error::Failed, "The script is out of stack space.");

    lua_pushcfunction(L, runProtected);
    lua_insert(L, -(nargs + 1));
    lua_pushlightuserdata(L, const_cast<std::function<void(lua_State *)> *>(&f));
    if (lua_pcall(L, nargs + 1, nresults, 0) != LUA_OK)
    {
        auto error = toError(L, -1);
        lua_pop(L, 1);
        return Err(error);
    }
    return {};
}

LuaEngine::LuaEngine(LuaLimits limits) : limits(limits)
{
    L = lua_newstate(allocate, this);
    // copied into the threads
    *static_cast<LuaEngine **>(lua_getextraspace(L)) = this;

    auto opened = protect(L, 0, [](lua_State *state) {
        luaL_requiref(state, LUA_GNAME, luaopen_base, 1);
        luaL_requiref(state, LUA_STRLIBNAME, luaopen_string, 1);
        luaL_requiref(state, LUA_TABLIBNAME, luaopen_table, 1);
        luaL_requiref(state, LUA_MATHLIBNAME, luaopen_math, 1);
        luaL_requiref(state, LUA_UTF8LIBNAME, luaopen_utf8, 1);
        lua_settop(state, 0);

        // no code but the script's, no files, no control over the collector
        for (auto name : {"dofile", "loadfile", "load", "collectgarbage", "print"})
        {
            lua_pushnil(state);
            lua_setglobal(state, name);
        }
    });
    if (!opened)
        qWarning() << "Lua:" << opened.error().message;

    lua_sethook(L, hook, LUA_MASKCOUNT, 100000);
}

LuaEngine::~LuaEngine()
{
    lua_close(L);
}

LuaEngine *LuaEngine::of(lua_State *L)
{
    return *static_cast<LuaEngine **>(lua_getextraspace(L));
}

void *LuaEngine::allocate(void *ud, void *ptr, size_t oldSize, size_t newSize)
{
    auto engine = static_cast<LuaEngine *>(ud);
    // without a block oldSize is the type of the object
    if (!ptr)
        oldSize = 0;

    if (newSize == 0)
    {
        free(ptr);
        engine->memoryUsed -= oldSize;
        return nullptr;
    }

    // Lua raises "not enough memory"
    if (newSize > oldSize && engine->memoryUsed - oldSize + newSize > engine->limits.memory)
        return nullptr;

    void *block = realloc(ptr, newSize);
    if (block)
    {
        engine->memoryUsed = engine->memoryUsed - oldSize + newSize;
        engine->memoryMax = qMax(engine->memoryMax, engine->memoryUsed);
    }
    return block;
}

void LuaEngine::hook(lua_State *L, lua_Debug *)
{
    auto engine = of(L);
    if (engine->slice.isValid() && engine->slice.elapsed() > engine->limits.runMs)
        luaL_error(L, "The script ran for more than %d ms without waiting.", engine->limits.runMs);
}

Result<> LuaEngine::run(const QByteArray &code, const QString &name)
{
    auto chunk = ("@" + name).toUtf8();
    if (luaL_loadbufferx(L, code.constData(), code.size(), chunk.constData(), "t") != LUA_OK)
    {
        auto error = toError(L, -1);
        lua_pop(L, 1);
        return Err(error);
    }

    slice.start();
    int status = lua_pcall(L, 0, 0, 0);
    slice.invalidate();
    if (status != LUA_OK)
    {
        auto error = toError(L, -1);
        lua_pop(L, 1);
        return Err(error);
    }
    return {};
}

void LuaEngine::wait(lua_State *thread, Wait wait)
{
    of(thread)->waits.insert(thread, std::move(wait));
}

bool LuaEngine::shouldYield(lua_State *thread)
{
    auto engine = of(thread);
    return engine->slice.isValid() && engine->slice.elapsed() > yieldSliceMs;
}

Task<Result<int>> LuaEngine::resume(lua_State *thread, int nargs)
{
    while (true)
    {
        int nresults = 0;
        slice.start();
        int status = lua_resume(thread, L, nargs, &nresults);
        slice.invalidate();

        if (status == LUA_OK)
            co_return nresults;
        if (status != LUA_YIELD)
            co_return Err(toError(thread, -1));

        // a host function waits for something
        lua_pop(thread, nresults);
        nargs = 0;
        auto wait = waits.take(thread);
        if (wait.job)
            co_await waitFor(wait.job, wait.pause);
        else
            co_await delay(wait.pause);
    }
}

void LuaEngine::release(lua_State *thread, int ref)
{
    waits.remove(thread);
    lua_closethread(thread, L);
    luaL_unref(L, LUA_REGISTRYINDEX, ref);
}

Error LuaEngine::toError(lua_State *L, int index)
{
    index = lua_absindex(L, index);
    if (lua_type(L, index) == LUA_TTABLE)
    {
        Error error;
        lua_pushliteral(L, "code");
        if (lua_rawget(L, index) == LUA_TSTRING)
            error.code = codeFromName(QString::fromUtf8(lua_tostring(L, -1)));
        lua_pushliteral(L, "message");
        if (lua_rawget(L, index) == LUA_TSTRING)
            error.message = QString::fromUtf8(lua_tostring(L, -1));
        lua_pushliteral(L, "status");
        if (lua_rawget(L, index) == LUA_TNUMBER)
            error.httpStatus = int(lua_tointeger(L, -1));
        lua_pop(L, 3);
        return error;
    }

    if (lua_type(L, index) == LUA_TSTRING || lua_type(L, index) == LUA_TNUMBER)
        return {Error::Failed, QString::fromUtf8(lua_tostring(L, index)), 0};

    return {Error::Failed, "The script failed.", 0};
}

Error::Code LuaEngine::codeFromName(const QString &name)
{
    for (int code = 0; code < int(std::size(codeNames)); code++)
        if (name == codeNames[code])
            return Error::Code(code);
    return Error::Failed;
}

const char *LuaEngine::codeName(Error::Code code)
{
    return code >= 0 && code < int(std::size(codeNames)) ? codeNames[code] : codeNames[0];
}
