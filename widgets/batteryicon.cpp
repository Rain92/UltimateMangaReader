#include "batteryicon.h"

#ifdef KOBO
#include "koboplatformfunctions.h"
#endif

BatteryIcon::BatteryIcon(QWidget *parent) : QLabel(parent)
{
    batteryicons[0] = QPixmap(":/images/icons/batteryfull.png");
    batteryicons[1] = QPixmap(":/images/icons/batterycharging.png");
    batteryicons[2] = QPixmap(":/images/icons/batteryempty.png");

    setScaledContents(true);
    setFixedSize(SIZES.batteryIconHeight * 2, SIZES.batteryIconHeight + 1);

    // workaround
    tooltipLabel = new QLabel(parent);
    tooltipLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    tooltipLabel->setStyleSheet("QLabel { background-color : black; color : white; }");
    tooltipLabel->hide();
}

void BatteryIcon::mousePressEvent(QMouseEvent *)
{
    int level = 100;
#ifdef KOBO
    level = KoboPlatformFunctions::getBatteryLevel();
#endif
    // In Qt 5.15 this seems to crash so we need a workaround
    //    QToolTip::showText(this->mapToGlobal(QPoint(0, 0)), QString::number(level) + "%");

    tooltipLabel->setText(QString::number(level) + "%");
    tooltipLabel->move(this->mapToParent(QPoint(-10, this->width() / 2 + 6)));
    tooltipLabel->adjustSize();
    tooltipLabel->show();
}

void BatteryIcon::mouseReleaseEvent(QMouseEvent *)
{
    tooltipLabel->hide();
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
        batteryicons[3] = QPixmap(":/images/icons/batteryempty.png");

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
