#pragma once

#include <QSize>
#include <QString>

// The hardware the app runs on. The build links one implementation: the Kobo (device_kobo.cpp, through the
// Kobo platform plugin and sysfs) or a desktop for development (device_desktop.cpp, it only pretends).
class Device
{
public:
    static Device &get();
    virtual ~Device() = default;

    // --- screen and input
    // In device pixels. Invalid when the app runs in a window of any size.
    virtual QSize screenSize() const = 0;
    // The e-ink refresh mode of the whole screen: fast (e.g. while scrolling) or in full quality.
    virtual void setFastRefresh(bool fast) = 0;
    virtual void setDithering(bool enabled) = 0;
    // Otherwise taps come as mouse clicks.
    virtual bool hasTouchScreen() const = 0;
    // Text is entered with the on-screen keyboard.
    virtual bool needsVirtualKeyboard() const = 0;
    virtual int powerKey() const = 0;
    virtual int sleepCoverKey() const = 0;

    // --- frontlight
    struct Frontlight
    {
        bool available = false;
        int min = 0;
        int max = 100;
        // the warmth
        int naturalMin = 0;
        int naturalMax = 100;
    };
    virtual Frontlight frontlight() const = 0;
    // In the ranges of frontlight().
    virtual void setFrontlight(int light, int naturalLight) = 0;
    virtual void frontlightOff() = 0;

    // --- cpu
    // While on, the cpu runs at full speed, e.g. from a tap to the page turn it starts. Otherwise the governor
    // raises the speed once it notices the load (the Kobo's: after up to 50 ms).
    virtual void setCpuBoost(bool on) = 0;

    // --- battery
    virtual int batteryLevel() const = 0;
    virtual bool isCharging() const = 0;

    // --- power
    // Suspends to RAM, returns when the device woke up again (after resumed()).
    virtual bool suspend() = 0;
    // After a suspend.
    virtual void resumed() = 0;
    // Standby: the WiFi chip stays associated, a tap wakes the device up (~0.3 s).
    virtual bool standbySupported() const = 0;
    // The rtc alarm that wakes the device up after seconds, 0 clears it.
    virtual bool setWakeAlarm(int seconds) = 0;
    // Returns when the device woke up again. errorString: why it failed.
    virtual bool standby(QString *errorString) = 0;

    // --- network (called on the WiFi thread of the network manager)
    virtual void enableWifi() = 0;
    virtual void disableWifi() = 0;
    virtual bool testInternetConnection(int timeoutSeconds) = 0;
    // Of the CA certificates for TLS, empty for the ones of the system.
    virtual QString caCertificatesPath() const = 0;
};
