#include "suspendmanager.h"

SuspendManager::SuspendManager(QObject *parent) : QObject(parent), sleeping(false), numUnexpectedWakups(0) {}

void printPowerStates()
{
    QFile file("/sys/power/state-extended");
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        qDebug() << "Could not open /sys/power/state-extended";
        return;
    }

    qDebug() << "Reading /sys/power/state-extended";
    QTextStream in(&file);
    QString line = in.readLine();
    qDebug() << line;
    file.close();

    QFile file2("/sys/power/state");
    if (!file2.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        qDebug() << "Could not open /sys/power/state";
        return;
    }

    qDebug() << "Reading /sys/power/state";
    QTextStream in2(&file2);
    QString line2 = in2.readLine();
    qDebug() << line2;
    file2.close();
}

bool SuspendManager::suspend()
{
    emit suspending();
    qApp->processEvents();

#ifdef KOBO
    qDebug() << QTime::currentTime().toString("hh:mm:ss") << "Going to sleep...";
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
    QProcess::execute("sync");

    qDebug() << QTime::currentTime().toString("hh:mm:ss") << "suspending to RAM";

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
    // UIManager:scheduleIn(15, check_unexpected_wakeup)
#endif

    sleeping = true;

    return true;
}

bool SuspendManager::resume()
{
    qDebug() << QTime::currentTime().toString("hh:mm:ss") << "Waking up...";

#ifdef KOBO
    numUnexpectedWakups = 0;
    //    require("ui/uimanager"):unschedule(check_unexpected_wakeup)

    int handleSE = open("/sys/power/state-extended", O_RDWR);
    if (!handleSE)
    {
        qDebug() << "Could not open /sys/power/state-extended";
        return false;
    }
    auto ret = write(handleSE, "0\n", 2);
    close(handleSE);

    QThread::msleep(100);

    int handleNC = open("/sys/devices/virtual/input/input1/neocmd", O_RDWR);
    if (!handleNC)
    {
        qDebug() << "Could not open /sys/power/state-extended";
    }
    else
    {
        ret = write(handleNC, "a\n", 2);
        close(handleNC);
    }
#endif

    sleeping = false;

    emit resuming();

    return true;
}
