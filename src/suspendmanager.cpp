#include "suspendmanager.h"

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
    if (networkManager->connected)
        networkManager->disconnectWifi();

#ifdef KOBO
    int handleSE = open("/sys/power/state-extended", O_RDWR);
    if (!handleSE)
    {
        qDebug() << "Could not open /sys/power/state-extended";
        return false;
    }
    auto ret = write(handleSE, "1\n", 2);
    close(handleSE);
    if (!ret)
    {
        qDebug() << "Could not write to /sys/power/state-extended:" << ret;
        return false;
    }

    QThread::sleep(2);
    QProcess::execute("sync", {});

    int handleS = open("/sys/power/state", O_RDWR);
    if (!handleS)
    {
        qDebug() << "Could not open /sys/power/state";
        int handleSE2 = open("/sys/power/state-extended", O_RDWR);
        if (!handleSE2)
        {
            qDebug() << "Could not open /sys/power/state-extended";
        }
        else
        {
            write(handleSE2, "0\n", 2);
            close(handleSE2);
        }

        return false;
    }
    ret = write(handleS, "mem\n", 4);
    if (!ret)
    {
        qDebug() << "Could not write to /sys/power/state:" << ret;
        return false;
    }

    close(handleS);
#endif

    sleeping = true;

    return true;
}

bool SuspendManager::resume()
{
    qDebug() << QTime::currentTime().toString("hh:mm:ss") << "Waking up...";

#ifdef KOBO

    int handleSE = open("/sys/power/state-extended", O_RDWR);
    if (!handleSE)
    {
        qDebug() << "Could not open /sys/power/state-extended";
        return false;
    }
    auto ret = write(handleSE, "0\n", 2);
    if (!ret)
    {
        qDebug() << "Could not write to /sys/power/state-extended:" << ret;
        return false;
    }
    close(handleSE);

    QThread::msleep(100);

    int handleNC = open("/sys/devices/virtual/input/input1/neocmd", O_RDWR);
    if (!handleNC)
    {
        qDebug() << "Could not open /sys/devices/virtual/input/input1/neocmd";
    }
    else
    {
        ret = write(handleNC, "a\n", 2);
        if (!ret)
        {
            qDebug() << "Could not write to /sys/devices/virtual/input/input1/neocmd:" << ret;
            return false;
        }
        close(handleNC);
    }
#endif

    timer.stop();
    sleeping = false;

    emit resuming();

    return true;
}
