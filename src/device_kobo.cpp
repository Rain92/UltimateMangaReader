#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#include <QDebug>
#include <QFile>
#include <QLibraryInfo>
#include <QProcess>
#include <QThread>

#include "device.h"
#include "koboplatformfunctions.h"

// Writes value to a sysfs file. Writing to /sys/power/state only returns after the device woke up again.
static bool writeSysfs(const char *path, const QByteArray &value, int *error = nullptr)
{
    int fd = open(path, O_WRONLY);
    bool ok = fd >= 0 && write(fd, value.constData(), value.size()) == value.size();
    if (error)
        *error = ok ? 0 : errno;
    if (fd >= 0)
        close(fd);
    return ok;
}

// Like writeSysfs(), logs why it failed.
static bool writeSysfsLogged(const char *path, const QByteArray &value)
{
    int error = 0;
    bool ok = writeSysfs(path, value, &error);
    if (!ok)
        qDebug() << "Could not write to" << path << ":" << strerror(error);
    return ok;
}

static const char *powerStatePath = "/sys/power/state";
static const char *powerStateExtendedPath = "/sys/power/state-extended";
static const char *wakeAlarmPath = "/sys/class/rtc/rtc0/wakealarm";

// The Kobo through its platform plugin (the platform functions do nothing on another platform).
class KoboReader : public Device
{
public:
    QSize screenSize() const override { return {descriptor().width, descriptor().height}; }
    void setFastRefresh(bool fast) override
    {
        KoboPlatformFunctions::setFullScreenRefreshMode(fast ? WaveForm_A2 : WaveForm_GC16);
    }
    void setDithering(bool enabled) override { KoboPlatformFunctions::enableDithering(enabled, false); }
    bool hasTouchScreen() const override { return true; }
    bool needsVirtualKeyboard() const override { return true; }
    int powerKey() const override { return KoboKey::Key_Power; }
    int sleepCoverKey() const override { return KoboKey::Key_SleepCover; }

    Frontlight frontlight() const override
    {
        const auto &settings = descriptor().frontlightSettings;
        return {settings.hasFrontLight, settings.frontlightMin, settings.frontlightMax, settings.naturalLightMin,
                settings.naturalLightMax};
    }
    void setFrontlight(int light, int naturalLight) override
    {
        const auto &settings = descriptor().frontlightSettings;
        if (!settings.hasFrontLight)
            return;

        // the hardware of some devices takes the warmth inverted
        int hardwareNaturalLight =
            settings.naturalLightInverted ? settings.naturalLightMax - naturalLight : naturalLight;
        KoboPlatformFunctions::setFrontlightLevel(light, hardwareNaturalLight);
    }
    void frontlightOff() override { KoboPlatformFunctions::setFrontlightLevel(0, 0); }

    int batteryLevel() const override { return KoboPlatformFunctions::getBatteryLevel(); }
    bool isCharging() const override { return KoboPlatformFunctions::isBatteryCharging(); }

    bool suspend() override
    {
        if (!writeSysfsLogged(powerStateExtendedPath, "1\n"))
            return false;

        QThread::sleep(2);
        QProcess::execute("sync", {});

        // returns when the device woke up again, fails e.g. with EBUSY while USB is connected
        return writeSysfsLogged(powerStatePath, "mem\n");
    }
    void resumed() override
    {
        // the app wakes up also if these fail
        writeSysfsLogged(powerStateExtendedPath, "0\n");
        QThread::msleep(100);
        writeSysfsLogged("/sys/devices/virtual/input/input1/neocmd", "a\n");
    }
    bool standbySupported() const override
    {
        QFile states(powerStatePath);
        return states.open(QIODevice::ReadOnly) && states.readAll().contains("standby");
    }
    bool setWakeAlarm(int seconds) override
    {
        writeSysfs(wakeAlarmPath, "0");
        return seconds <= 0 || writeSysfs(wakeAlarmPath, "+" + QByteArray::number(seconds));
    }
    bool standby(QString *errorString) override
    {
        int error = 0;
        bool ok = writeSysfs(powerStatePath, "standby", &error);
        if (!ok && errorString)
            *errorString = strerror(error);
        return ok;
    }

    void enableWifi() override { KoboPlatformFunctions::enableWiFiConnection(); }
    void disableWifi() override { KoboPlatformFunctions::disableWiFiConnection(); }
    bool testInternetConnection(int timeoutSeconds) override
    {
        return KoboPlatformFunctions::testInternetConnection(timeoutSeconds);
    }
    QString caCertificatesPath() const override
    {
        // deployed with Qt (the launcher sets QTPATH)
        if (qEnvironmentVariableIsSet("QTPATH"))
            return qEnvironmentVariable("QTPATH") + "/lib/ssl_certs";
        return QLibraryInfo::path(QLibraryInfo::PrefixPath) + "/lib/ssl_certs";
    }

private:
    // from the platform plugin, once the application exists
    const KoboDeviceDescriptor &descriptor() const
    {
        static const KoboDeviceDescriptor descriptor = KoboPlatformFunctions::getKoboDeviceDescriptor();
        return descriptor;
    }
};

Device &Device::get()
{
    static KoboReader device;
    return device;
}
