#ifndef BATTERYICONLABEL_H
#define BATTERYICONLABEL_H

#include <QApplication>
#include <QDebug>
#include <QLabel>
#include <QPainter>
#include <QPair>
#include <QPixmap>
#include <QToolTip>

#include "sizes.h"

class BatteryIcon : public QLabel
{
    Q_OBJECT

public:
    BatteryIcon(QWidget *parent = nullptr);

    void updateIcon();

protected:
    void mousePressEvent(QMouseEvent *);
    void mouseReleaseEvent(QMouseEvent *);

private:
    QPixmap batteryicons[4];
    QLabel *tooltipLabel;

    void setBatteryIcon();
    QPair<int, bool> getBatteryState();
};

#endif  // BATTERYICONLABEL_H
