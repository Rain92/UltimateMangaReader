#ifndef PERFTIMER_H
#define PERFTIMER_H

#include <QDebug>
#include <QElapsedTimer>
#include <QTimer>
#include <QtGlobal>

#include <memory>

// Timing logs for the image pipeline, enabled with the environment variable UMR_PROFILE=1.
inline bool perfLoggingEnabled()
{
    static const bool enabled = qEnvironmentVariableIsSet("UMR_PROFILE");
    return enabled;
}

// Logs the time between construction and destruction (or between laps) as "[perf] name: x ms".
class PerfTimer
{
public:
    explicit PerfTimer(const char *name) : name(name), enabled(perfLoggingEnabled())
    {
        if (enabled)
            timer.start();
    }

    ~PerfTimer()
    {
        if (enabled)
            qDebug().nospace() << "[perf] " << name << ": " << timer.nsecsElapsed() / 1e6 << " ms";
    }

    // logs the time since the last lap (or the start) and restarts the clock
    void lap(const char *stage)
    {
        if (!enabled)
            return;
        qDebug().nospace() << "[perf] " << name << " " << stage << ": " << timer.nsecsElapsed() / 1e6
                           << " ms";
        timer.restart();
    }

private:
    const char *name;
    bool enabled;
    QElapsedTimer timer;
};

// Logs "[perf] GUI stall: x ms" whenever the thread of \a parent didn't get back to its event loop for
// longer than \a thresholdMs (only with UMR_PROFILE=1).
inline void startStallWatchdog(QObject *parent, int thresholdMs = 150)
{
    if (!perfLoggingEnabled())
        return;

    const int intervalMs = 50;
    auto clock = std::make_shared<QElapsedTimer>();
    clock->start();

    auto timer = new QTimer(parent);
    timer->setTimerType(Qt::PreciseTimer);
    QObject::connect(timer, &QTimer::timeout, parent,
                     [clock, intervalMs, thresholdMs]()
                     {
                         qint64 gap = clock->restart();
                         if (gap > intervalMs + thresholdMs)
                             qDebug().nospace() << "[perf] GUI stall: " << gap - intervalMs << " ms";
                     });
    timer->start(intervalMs);
}

#endif  // PERFTIMER_H
