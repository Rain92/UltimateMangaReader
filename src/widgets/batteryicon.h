#pragma once

#include <QLabel>
#include <QPair>
#include <QPixmap>

#include "sizes.h"

class BatteryIcon : public QLabel
{
    Q_OBJECT

public:
    BatteryIcon(QWidget *parent = nullptr);

    void updateIcon();

protected:
    void mousePressEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;

private:
    QPixmap fullIcon;
    QPixmap chargingIcon;
    QPixmap emptyIcon;
    QLabel *tooltipLabel;

    QPair<int, bool> getBatteryState();
};
