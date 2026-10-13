#include "suspendmanager.h"

#include "device.h"

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

    // returns when the device woke up again. If it fails (e.g. EBUSY while USB is connected) the timer tries again.
    if (!Device::get().suspend())
        return false;

    sleeping = true;

    return true;
}

bool SuspendManager::resume()
{
    qDebug() << QTime::currentTime().toString("hh:mm:ss") << "Waking up...";

    Device::get().resumed();

    timer.stop();
    sleeping = false;

    emit resuming();

    return true;
}
