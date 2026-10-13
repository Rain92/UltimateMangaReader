#include "batteryicon.h"

#include <QPainter>

#include "device.h"

BatteryIcon::BatteryIcon(QWidget *parent)
    : QLabel(parent),
      fullIcon(":/images/icons/batteryfull.png"),
      chargingIcon(":/images/icons/batterycharging.png"),
      emptyIcon(":/images/icons/batteryempty.png")
{
    setScaledContents(true);
    setFixedSize(SIZES.batteryIconHeight * 2, SIZES.batteryIconHeight + 1);

    // the level is shown in a label in the parent instead of a QToolTip, which is a window of its own
    tooltipLabel = new QLabel(parent);
    tooltipLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    tooltipLabel->setStyleSheet("QLabel { background-color : black; color : white; }");
    tooltipLabel->hide();
}

void BatteryIcon::mousePressEvent(QMouseEvent *)
{
    int level = Device::get().batteryLevel();
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
        setPixmap(fullIcon);
    }
    else if (charging)
    {
        setPixmap(chargingIcon);
    }
    else
    {
        QPixmap icon = emptyIcon;

        QPainter painter(&icon);
        QBrush brush(Qt::black);

        if (bat > 90)
        {
            int w = (bat - 90) / 2;
            painter.fillRect(7 + (5 - w), 12, w, 8, brush);
        }

        int w = qMin(45, bat / 2);
        painter.fillRect(12 + (45 - w), 6, w, 20, brush);

        painter.end();
        setPixmap(icon);
    }
}

QPair<int, bool> BatteryIcon::getBatteryState()
{
    return QPair<int, bool>(Device::get().batteryLevel(), Device::get().isCharging());
}
