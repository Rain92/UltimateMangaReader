#include "device.h"

// A desktop for development: a window, always connected, no power management.
class DesktopDevice : public Device
{
public:
    QSize screenSize() const override { return {}; }
    void setFastRefresh(bool) override {}
    void setDithering(bool) override {}
    bool hasTouchScreen() const override { return false; }
    bool needsVirtualKeyboard() const override { return false; }
    int powerKey() const override { return Qt::Key_F1; }
    int sleepCoverKey() const override { return Qt::Key_F2; }

    // the sliders work (and are saved), there's nothing to light
    Frontlight frontlight() const override { return {true, 0, 100, 0, 100}; }
    void setFrontlight(int, int) override {}
    void frontlightOff() override {}

    void setCpuBoost(bool) override {}

    int batteryLevel() const override { return 100; }
    bool isCharging() const override { return false; }

    bool suspend() override { return true; }
    void resumed() override {}
    bool standbySupported() const override { return false; }
    bool setWakeAlarm(int) override { return false; }
    bool standby(QString *errorString) override
    {
        if (errorString)
            *errorString = "not supported";
        return false;
    }

    void enableWifi() override {}
    void disableWifi() override {}
    bool testInternetConnection(int) override { return true; }
    QString caCertificatesPath() const override { return {}; }
};

Device &Device::get()
{
    static DesktopDevice device;
    return device;
}
