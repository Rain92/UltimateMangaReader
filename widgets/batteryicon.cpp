#include "batteryicon.h"

#ifdef KOBO
#include "../koboplatformintegrationplugin/koboplatformfunctions.h"
#endif

BatteryIcon::BatteryIcon(QWidget *parent) : QLabel(parent)
{
    batteryicons[0] =
        QPixmap(":/resources/images/icons/batteryfull.png")
            .scaledToHeight(batteryiconsize, Qt::SmoothTransformation);
    batteryicons[1] =
        QPixmap(":/resources/images/icons/batterycharging.png")
            .scaledToHeight(batteryiconsize, Qt::SmoothTransformation);
    batteryicons[2] =
        QPixmap(":/resources/images/icons/batteryempty.png")
            .scaledToHeight(batteryiconsize, Qt::SmoothTransformation);
}

void BatteryIcon::updateIcon()
{
    QPair<int, bool> batterystate = getBatteryState();
    int bat = batterystate.first;
    bool charging = batterystate.second;

    if (bat >= 98)
    {
        setPixmap(batteryicons[0]);
    }
    else if (charging)
    {
        setPixmap(batteryicons[1]);
    }
    else
    {
        batteryicons[3] = QPixmap(":/resources/images/icons/batteryempty.png");

        QPainter painter(&batteryicons[3]);
        QBrush brush(Qt::black);

        if (bat > 90)
        {
            int w = (bat - 90) / 2;
            painter.fillRect(7 + (5 - w), 12, w, 8, brush);
        }

        int w = qMin(45, bat / 2);
        painter.fillRect(12 + (45 - w), 6, w, 20, brush);

        painter.end();
        setPixmap(batteryicons[3].scaledToHeight(batteryiconsize,
                                                 Qt::SmoothTransformation));
    }
}

QPair<int, bool> BatteryIcon::getBatteryState()
{
#ifdef KOBO
    return QPair<int, bool>(KoboPlatformFunctions::getBatteryLevel(),
                            KoboPlatformFunctions::isBatteryCharging());
#endif

    return QPair<int, bool>(100, false);
}
