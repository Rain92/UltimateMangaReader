#pragma once

#include <QCoreApplication>
#include <QEventLoop>
#include <QPointer>
#include <QSharedPointer>
#include <QTimer>

#include <algorithm>
#include <concepts>
#include <coroutine>
#include <exception>
#include <functional>
#include <memory>
#include <optional>
#include <type_traits>

#include "downloadjobbase.h"
#include "result.h"

// Work that waits for downloads without blocking: a coroutine on the event loop of the gui thread. It starts
// right away and runs until it waits for something (co_await of a job, of another Task or of delay()), and goes
// on from the event loop when that finished. It finishes on its own, also when nobody keeps its Task.
//
// The parameters of a coroutine are taken by value: a reference could be gone when it goes on. For the same
// reason a coroutine is never a lambda (its captures live in the lambda, not in the coroutine).
//
//     Task<Result<QString>> Source::title(QString url)
//     {
//         auto job = download(url);
//         if (!co_await job)
//             co_return Err(job->error);
//         co_return job->text();
//     }
template <typename T = void>
class Task;

namespace detail
{
struct TaskStateBase
{
    bool done = false;
    std::exception_ptr exception;
    // run when it finished: the coroutines and the blocking waits that wait for it
    QList<std::function<void()>> waiters;

    void finish()
    {
        done = true;
        auto list = std::exchange(waiters, {});
        for (const auto &waiter : list)
            waiter();
    }
};

template <typename T>
struct TaskState : TaskStateBase
{
    std::optional<T> value;

    T result() const
    {
        if (exception)
            std::rethrow_exception(exception);
        return *value;
    }
};

template <>
struct TaskState<void> : TaskStateBase
{
    void result() const
    {
        if (exception)
            std::rethrow_exception(exception);
    }
};

template <typename T>
struct TaskPromiseBase
{
    std::shared_ptr<TaskState<T>> state = std::make_shared<TaskState<T>>();

    Task<T> get_return_object() { return Task<T>(state); }
    std::suspend_never initial_suspend() noexcept { return {}; }

    // The coroutine is gone when the ones that wait for it go on, the state (with the result) stays.
    struct FinalAwaiter
    {
        bool await_ready() noexcept { return false; }
        template <typename Promise>
        void await_suspend(std::coroutine_handle<Promise> handle) noexcept
        {
            auto state = handle.promise().state;
            handle.destroy();
            state->finish();
        }
        void await_resume() noexcept {}
    };
    FinalAwaiter final_suspend() noexcept { return {}; }

    void unhandled_exception() { state->exception = std::current_exception(); }
};

template <typename T>
struct TaskPromise : TaskPromiseBase<T>
{
    // by value: co_return {} and co_return of values that convert to T
    void return_value(T value) { this->state->value.emplace(std::move(value)); }

};

template <>
struct TaskPromise<void> : TaskPromiseBase<void>
{
    void return_void() {}
};
}  // namespace detail

template <typename T>
class Task
{
public:
    using promise_type = detail::TaskPromise<T>;

    explicit Task(std::shared_ptr<detail::TaskState<T>> state) : state(std::move(state)) {}

    bool isFinished() const { return state->done; }

    // co_await task: goes on with the result when the task finished.
    bool await_ready() const noexcept { return state->done; }
    void await_suspend(std::coroutine_handle<> handle) const
    {
        state->waiters.append([handle]() { handle.resume(); });
    }
    T await_resume() const { return state->result(); }

    // Calls f(result) when the task finished (right away if it is).
    template <typename F>
    void then(F f) const
    {
        if (state->done)
        {
            callWithResult(f);
            return;
        }
        state->waiters.append([state = state, f]() mutable { Task(state).callWithResult(f); });

    }

    // The result, after waiting in an event loop until the task finished: for the callers that aren't
    // coroutines. A task that can't finish because the application quits ends with Cancelled.
    T wait() const
    {
        if (!state->done)
        {
            auto hold = DownloadJobBase::holdActivity();
            QEventLoop loop;
            state->waiters.append(
                [loop = QPointer<QEventLoop>(&loop)]()
                {
                    if (loop)
                        loop->quit();
                });
            loop.exec();
        }

        if (!state->done)
        {
            if constexpr (std::is_constructible_v<T, std::unexpected<Error>>)
                return T(Err(Error::Cancelled, "Cancelled"));
            else if constexpr (std::is_void_v<T>)
                return;
            else
                std::terminate();
        }
        return state->result();
    }

private:
    std::shared_ptr<detail::TaskState<T>> state;

    template <typename F>
    void callWithResult(F &f) const
    {
        if constexpr (std::is_void_v<T>)
        {
            state->result();
            f();
        }
        else
        {
            f(state->result());
        }
    }
};

// Lets the coroutine go on from the event loop: not from within a signal of what it waited for (it might
// destroy the sender, or start it again).
inline void resumeLater(std::coroutine_handle<> handle)
{
    QMetaObject::invokeMethod(QCoreApplication::instance(), [handle]() { handle.resume(); }, Qt::QueuedConnection);
}

// Goes on when one of the senders emits one of the signals, or after timeout ms if it is > 0. Once.
template <typename Sender, typename... Signals>
void resumeOnSignal(const QList<Sender *> &senders, std::coroutine_handle<> handle, int timeout,
                    Signals... signalList)
{
    auto resumed = std::make_shared<bool>(false);
    auto connections = std::make_shared<QList<QMetaObject::Connection>>();
    auto resume = [handle, resumed, connections]()
    {
        if (std::exchange(*resumed, true))
            return;
        for (const auto &connection : *connections)
            QObject::disconnect(connection);
        resumeLater(handle);
    };
    for (auto *sender : senders)
        (connections->append(QObject::connect(sender, signalList, sender, resume)), ...);
    if (timeout > 0)
        QTimer::singleShot(timeout, QCoreApplication::instance(), resume);
}

// co_await job: waits until the job finished, true if it completed.
template <typename Job>
    requires std::derived_from<Job, DownloadJobBase>
struct JobAwaiter
{
    QSharedPointer<Job> job;
    // at most this long (ms) if > 0, the download goes on
    int maxWait = 0;
    // no standby while a coroutine waits, the next request comes right after
    std::shared_ptr<void> hold;

    bool await_ready() const { return job->isFinished(); }
    void await_suspend(std::coroutine_handle<> handle)
    {
        hold = DownloadJobBase::holdActivity();
        resumeOnSignal(QList<DownloadJobBase *>{job.get()}, handle, maxWait, &DownloadJobBase::completed,
                       &DownloadJobBase::downloadError);
    }
    bool await_resume()
    {
        hold.reset();
        return job->isCompleted;
    }
};

template <typename Job>
    requires std::derived_from<Job, DownloadJobBase>
JobAwaiter<Job> operator co_await(QSharedPointer<Job> job)
{
    return {std::move(job), 0, {}};
}

// co_await waitFor(job, maxWait): like co_await job, but goes on after maxWait ms at the latest.
template <typename Job>
    requires std::derived_from<Job, DownloadJobBase>
JobAwaiter<Job> waitFor(QSharedPointer<Job> job, int maxWait)
{
    return {std::move(job), maxWait, {}};
}

// co_await anyFinished(jobs, maxWait): goes on when one of the jobs finished, after maxWait ms at the latest
// if it is > 0.
struct AnyJobAwaiter
{
    QList<DownloadJobBase *> jobs;
    int maxWait = 0;
    std::shared_ptr<void> hold;

    bool await_ready() const
    {
        return std::any_of(jobs.begin(), jobs.end(), [](DownloadJobBase *job) { return job->isFinished(); });
    }
    void await_suspend(std::coroutine_handle<> handle)
    {
        hold = DownloadJobBase::holdActivity();
        resumeOnSignal(jobs, handle, maxWait, &DownloadJobBase::completed, &DownloadJobBase::downloadError);
    }
    void await_resume() { hold.reset(); }
};

// The jobs have to be kept until it goes on.
inline AnyJobAwaiter anyFinished(QList<DownloadJobBase *> jobs, int maxWait)
{
    return {std::move(jobs), maxWait, {}};
}

// co_await delay(ms): goes on after ms (from the event loop, also for 0).

struct DelayAwaiter
{
    qint64 ms;
    std::shared_ptr<void> hold;

    bool await_ready() const { return false; }
    void await_suspend(std::coroutine_handle<> handle)
    {
        // no standby until then, it would stop the timer
        hold = DownloadJobBase::holdActivity();
        QTimer::singleShot(std::chrono::milliseconds(qMax<qint64>(0, ms)), [handle]() { handle.resume(); });
    }
    void await_resume() { hold.reset(); }
};

inline DelayAwaiter delay(qint64 ms)
{
    return {ms, {}};
}
