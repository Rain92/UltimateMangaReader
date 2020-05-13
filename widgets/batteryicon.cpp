#include "batteryicon.h"

#ifdef KOBO
#include "../koboplatformintegrationplugin/koboplatformfunctions.h"
#endif

BatteryIcon::BatteryIcon(QWidget *parent) : QLabel(parent)
{
    //    setMaximumHeight(SIZES.batteryIconHeight);

    batteryicons[0] = QPixmap(":/images/icons/batteryfull.png")
                          .scaledToHeight(SIZES.batteryIconHeight, Qt::SmoothTransformation);
    batteryicons[1] =
        QPixmap(":/images/icons/batterycharging.png")
            .scaledToHeight(SIZES.batteryIconHeight * qApp->devicePixelRatio(), Qt::SmoothTransformation);
    batteryicons[2] =
        QPixmap(":/images/icons/batteryempty.png")
            .scaledToHeight(SIZES.batteryIconHeight * qApp->devicePixelRatio(), Qt::SmoothTransformation);
}

void BatteryIcon::mousePressEvent(QMouseEvent *)
{
    int level = 100;
#ifdef KOBO
    level = KoboPlatformFunctions::getBatteryLevel();
#endif
    QToolTip::showText(this->mapToGlobal(QPoint(0, 0)), QString::number(level) + "%");
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
        batteryicons[3] =
            QPixmap(":/images/icons/batteryempty.png")
                .scaledToHeight(SIZES.batteryIconHeight * qApp->devicePixelRatio(), Qt::SmoothTransformation);

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
        setPixmap(batteryicons[3]);
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
