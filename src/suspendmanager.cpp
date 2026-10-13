#include "suspendmanager.h"

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#ifdef KOBO
// Writes value to a sysfs file, logs why if that failed.
static bool writeSysfsFile(const char *path, const char *value)
{
    int fd = open(path, O_RDWR);
    if (fd < 0)
    {
        qDebug() << "Could not open" << path << ":" << strerror(errno);
        return false;
    }

    ssize_t length = strlen(value);
    bool ok = write(fd, value, length) == length;
    if (!ok)
        qDebug() << "Could not write to" << path << ":" << strerror(errno);
    close(fd);
    return ok;
}
#endif

SuspendManager::SuspendManager(NetworkManager *networkManager, QObject *parent)
    : QObject(parent), sleeping(false), networkManager(networkManager), timer()
{
    timer.setInterval(60 * 5000);
    connect(&timer, &QTimer::timeout, this, &SuspendManager::suspendInternal);
}

bool SuspendManager::suspend()
{
    if (sleeping)
        return true;

    emit suspending();
    qApp->processEvents();

    if (sleeping == false)
        qDebug() << QTime::currentTime().toString("hh:mm:ss") << "Going to sleep...";

    sleeping = true;

    timer.start();

    return suspendInternal();
}

bool SuspendManager::suspendInternal()
{
    // also when it isn't connected: the WiFi might be on without a connection
    networkManager->disconnectWifi();

#ifdef KOBO
    if (!writeSysfsFile("/sys/power/state-extended", "1\n"))
        return false;

    QThread::sleep(2);
    QProcess::execute("sync", {});

    // returns when the device woke up again. If it fails (e.g. EBUSY while USB is connected) the timer tries again.
    if (!writeSysfsFile("/sys/power/state", "mem\n"))
        return false;
#endif

    sleeping = true;

    return true;
}

bool SuspendManager::resume()
{
    qDebug() << QTime::currentTime().toString("hh:mm:ss") << "Waking up...";

#ifdef KOBO
    // the app wakes up also if these fail
    writeSysfsFile("/sys/power/state-extended", "0\n");
    QThread::msleep(100);
    writeSysfsFile("/sys/devices/virtual/input/input1/neocmd", "a\n");
#endif

    timer.stop();
    sleeping = false;

    emit resuming();

    return true;
}
