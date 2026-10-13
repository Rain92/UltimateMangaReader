#include "standbymanager.h"

#include <time.h>

#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QEvent>

#include "device.h"
#include "mangachapterdownloadmanager.h"
#include "networkmanager.h"
#include "staticsettings.h"
#include "suspendmanager.h"

static qint64 clockMs(clockid_t clock)
{
    timespec ts;
    clock_gettime(clock, &ts);
    return qint64(ts.tv_sec) * 1000 + ts.tv_nsec / 1000000;
}

qint64 StandbyManager::bootTimeMs()
{
#ifdef CLOCK_BOOTTIME
    return clockMs(CLOCK_BOOTTIME);
#else
    return QDateTime::currentMSecsSinceEpoch();
#endif
}

StandbyManager::StandbyManager(Settings *settings, SuspendManager *suspendManager,
                               NetworkManager *networkManager,
                               MangaChapterDownloadManager *chapterDownloadManager, QObject *parent)
    : QObject(parent),
      settings(settings),
      suspendManager(suspendManager),
      networkManager(networkManager),
      chapterDownloadManager(chapterDownloadManager),
      msUntilDeadline(),
      supported(false),
      inhibited(false),
      failures(0),
      maxSleepSeconds(0),
      idleTimer()
{
    supported = Device::get().standbySupported();
    if (!supported)
        qDebug() << "Standby isn't supported here";

    // UMR_STANDBY_WAKE_SECONDS=<n> wakes the device at least every n seconds (for testing over ssh, where the
    // screen can't be tapped to wake it up)
    bool ok = false;
    int wake = qEnvironmentVariableIntValue("UMR_STANDBY_WAKE_SECONDS", &ok);
    if (ok && wake > 0)
        maxSleepSeconds = wake;

    idleTimer.setSingleShot(true);
    connect(&idleTimer, &QTimer::timeout, this, &StandbyManager::idleTimeout);

    if (supported)
        qApp->installEventFilter(this);
}

void StandbyManager::setWakeDeadline(std::function<qint64()> msUntilDeadline)
{
    this->msUntilDeadline = std::move(msUntilDeadline);
}

// Failed attempts (the display driver refuses while the panel is still powered) are retried with a growing
// delay, each one costs a suspend and resume of the WiFi chip.
int StandbyManager::delayMs() const
{
    return (CONF.standbyDelaySeconds * 1000) << qMin(failures, 4);
}

void StandbyManager::postpone()
{
    if (supported)
        idleTimer.start(delayMs());
}

void StandbyManager::setInhibited(bool inhibited)
{
    this->inhibited = inhibited;
    postpone();
}

bool StandbyManager::eventFilter(QObject *obj, QEvent *event)
{
    switch (event->type())
    {
        case QEvent::MouseButtonPress:
        case QEvent::MouseButtonRelease:
        case QEvent::MouseMove:
        case QEvent::TouchBegin:
        case QEvent::TouchUpdate:
        case QEvent::TouchEnd:
        case QEvent::KeyPress:
        case QEvent::KeyRelease:
        case QEvent::Wheel:
            postpone();
            break;
        default:
            break;
    }

    return QObject::eventFilter(obj, event);
}

void StandbyManager::idleTimeout()
{
    if (!settings->standbyWhenIdle || suspendManager->sleeping)
        return;

    // standby would stall downloads and preloads (or the WiFi scripts), check again later. The blocking waits
    // for downloads and the pauses between requests count as network activity.
    if (inhibited || networkManager->isBusy() || networkManager->isConnectingWifi() ||
        chapterDownloadManager->isRunning())
    {
        idleTimer.start(delayMs());
        return;
    }

    qint64 deadline = msUntilDeadline ? msUntilDeadline() : -1;
    // the auto suspend is due in a moment anyway
    if (deadline >= 0 && deadline < CONF.standbyDelaySeconds * 1000)
        return;

    int wakeAfterSeconds = deadline < 0 ? 0 : int((deadline + 999) / 1000);
    if (maxSleepSeconds > 0)
        wakeAfterSeconds = wakeAfterSeconds > 0 ? qMin(wakeAfterSeconds, maxSleepSeconds) : maxSleepSeconds;

    if (enterStandby(wakeAfterSeconds))
        failures = 0;
    else
        failures++;

    // after an rtc wake up nothing else restarts the countdown
    idleTimer.start(delayMs());
}

bool StandbyManager::enterStandby(int wakeAfterSeconds)
{
    auto &device = Device::get();
    if (!device.setWakeAlarm(wakeAfterSeconds))
        qDebug() << "Standby: couldn't set the wake alarm";

    const qint64 boot = bootTimeMs();
    const qint64 mono = clockMs(CLOCK_MONOTONIC);
    QString error;
    bool ok = device.standby(&error);
    const qint64 transitionMs = clockMs(CLOCK_MONOTONIC) - mono;
    const qint64 asleepMs = bootTimeMs() - boot - transitionMs;

    device.setWakeAlarm(0);

    if (!ok)
    {
        qDebug() << "Standby failed:" << error;
        return false;
    }

    qDebug() << "Standby: slept" << asleepMs << "ms, suspend + resume took" << transitionMs << "ms";
    emit resumed(asleepMs);
    return true;
}
