#pragma once

#include <QByteArray>
#include <QElapsedTimer>
#include <QHash>
#include <QSharedPointer>
#include <functional>

#include "downloadjobbase.h"
#include "lauxlib.h"
#include "lua.h"
#include "result.h"
#include "task.h"

struct LuaLimits
{
    // for everything the script allocates, in bytes
    size_t memory = 96 * 1024 * 1024;
    // a script that runs this long without waiting is stopped (e.g. an endless loop)
    int runMs = 10000;
};

// A Lua state for the script of a source: with the libraries a script may use (no files, no os, no loading of
// code) and limits on its memory and on how long it runs without waiting.
//
// The script's functions are called as Lua threads (call()). A host function that has to wait for something
// (a download, a pause) records it with wait() and yields, call() resumes the thread when it's there. So several
// calls can run at the same time, and the event loop goes on meanwhile.
class LuaEngine
{
public:
    explicit LuaEngine(LuaLimits limits = {});
    ~LuaEngine();
    LuaEngine(const LuaEngine &) = delete;
    LuaEngine &operator=(const LuaEngine &) = delete;

    lua_State *state() const { return L; }
    static LuaEngine *of(lua_State *L);

    // what the script has allocated now and at most (bytes)
    size_t memoryInUse() const { return memoryUsed; }
    size_t memoryPeak() const { return memoryMax; }
    // Frees the garbage at once (e.g. of a large call, it would wait for the next calls), and gives the freed memory
    // back to the system. Not while a script runs.
    void collectGarbage();

    // e.g. the source of the script, for its host functions
    void *host = nullptr;

    // Compiles and runs code (text, never bytecode), name is the chunk name for the error messages.
    Result<> run(const QByteArray &code, const QString &name);

    // What a host function waits for before its thread goes on, before it yields (lua_yieldk).
    struct Wait
    {
        // the wait ends when one of them finished
        QList<QSharedPointer<DownloadJobBase>> jobs;
        // ms without jobs, else the longest wait for them (0: until one finished)
        int pause = 0;
    };
    static void wait(lua_State *thread, Wait wait);
    // The running script has kept the event loop waiting for a while: a host function should yield (wait for a
    // pause of 0) even if it has what it waits for, so the GUI goes on meanwhile.
    static bool shouldYield(lua_State *thread);

    // Calls the global function with the arguments pushArgs pushes (it returns their number), in a thread of its
    // own. convert gets the thread with the results on top of its stack (their number), it may only read them.
    template <typename T>
    Task<Result<T>> call(QByteArray function, std::function<int(lua_State *)> pushArgs,
                         std::function<Result<T>(lua_State *, int)> convert);

    // The error a raised Lua value stands for: a table {code=, message=, status=} or a message.
    static Error toError(lua_State *L, int index);
    // The names of the codes in the scripts, e.g. "notfound".
    static Error::Code codeFromName(const QString &name);
    static const char *codeName(Error::Code code);

    // Runs f in protected mode, a Lua error in it (e.g. out of memory) becomes the result instead of ending the
    // app. f gets the nargs values on top of the stack as its own, nresults of what it leaves stay there.
    static Result<> protect(lua_State *L, int nargs, const std::function<void(lua_State *)> &f, int nresults = 0);

private:
    lua_State *L;
    LuaLimits limits;
    size_t memoryUsed = 0;
    size_t memoryMax = 0;
    QHash<lua_State *, Wait> waits;
    // since the running thread was resumed
    QElapsedTimer slice;

    static void *allocate(void *ud, void *ptr, size_t oldSize, size_t newSize);
    static void hook(lua_State *L, lua_Debug *ar);

    // Runs the thread until it returned (the number of its results) or failed.
    Task<Result<int>> resume(lua_State *thread, int nargs);
    void release(lua_State *thread, int ref);
};

template <typename T>
Task<Result<T>> LuaEngine::call(QByteArray function, std::function<int(lua_State *)> pushArgs,
                                std::function<Result<T>(lua_State *, int)> convert)
{
    lua_State *thread = nullptr;
    int ref = LUA_NOREF;
    auto started = protect(L, 0, [&](lua_State *state) {
        thread = lua_newthread(state);
        // kept from the garbage collector while it runs
        ref = luaL_ref(state, LUA_REGISTRYINDEX);
    });
    if (!started)
        co_return Err(started.error());

    int nargs = 0;
    auto pushed = protect(thread, 0, [&](lua_State *state) {
        if (lua_getglobal(state, function.constData()) != LUA_TFUNCTION)
            luaL_error(state, "The script has no function %s.", function.constData());
        nargs = pushArgs(state);
    }, LUA_MULTRET);

    Result<int> results = Err(pushed ? Error() : pushed.error());
    if (pushed)
        results = co_await resume(thread, nargs);

    Result<T> result = Err(results ? Error() : results.error());
    if (results)
    {
        // the results are the arguments of the conversion
        int count = *results;
        auto converted = protect(thread, count, [&](lua_State *state) { result = convert(state, count); });
        if (!converted)
            result = Err(converted.error());
    }

    release(thread, ref);
    co_return result;
}
