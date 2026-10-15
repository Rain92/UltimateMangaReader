#include "cpuboost.h"

#include <QCoreApplication>
#include <QEvent>

#include "device.h"

// after a release: the page turn, its paint and the start of its preloads
static const int afterReleaseMs = 500;
// while pressed, in case the release goes elsewhere (e.g. a popup grabbed it)
static const int whilePressedMs = 3000;

CpuBoost::CpuBoost(QObject *parent) : QObject(parent), timer()
{
    timer.setSingleShot(true);
    connect(&timer, &QTimer::timeout, this, []() { Device::get().setCpuBoost(false); });

    qApp->installEventFilter(this);
}

CpuBoost::~CpuBoost()
{
    Device::get().setCpuBoost(false);
}

void CpuBoost::start(int ms)
{
    Device::get().setCpuBoost(true);
    timer.start(ms);
}

bool CpuBoost::eventFilter(QObject *obj, QEvent *event)
{
    switch (event->type())
    {
        case QEvent::MouseButtonPress:
        case QEvent::TouchBegin:
        case QEvent::KeyPress:
            start(whilePressedMs);
            break;
        case QEvent::MouseButtonRelease:
        case QEvent::TouchEnd:
        case QEvent::TouchCancel:
        case QEvent::KeyRelease:
            start(afterReleaseMs);
            break;
        default:
            break;
    }

    return QObject::eventFilter(obj, event);
}
