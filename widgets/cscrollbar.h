#ifndef CSCROLLBAR_H
#define CSCROLLBAR_H

#include <QScrollBar>
#include "qstyle.h"
#include "qstyleoption.h"
#include <QWindowsStyle>

class CScrollBar : public QScrollBar
{
    Q_OBJECT

public:
    CScrollBar(Qt::Orientation orientation, QWidget *parent);
    CScrollBar(Qt::Orientation orientation, QWidget *parent, int extent);

    void setSliderLength(int length);

protected:
    QSize sizeHint() const;
    QSize minimumSizeHint() const;
    void paintEvent(QPaintEvent *event);
    void mousePressEvent(QMouseEvent *e);
    void mouseMoveEvent(QMouseEvent *e);
    void mouseReleaseEvent(QMouseEvent *e);

    void adjustLength();

private:
    int extent;

    QRect subControlRect(QStyle::ComplexControl cc, const QStyleOptionComplex *opt, QStyle::SubControl sc, /*const*/ QWidget *widget);

    QRect visualRect(Qt::LayoutDirection direction, const QRect &boundingRect, const QRect &logicalRect);

    int sliderPositionFromValue(int min, int max, int logicalValue, int span, bool upsideDown);

private:
    int sliderlength;
    bool mousedownonslider;
    QPoint clickoffset;
};


#endif // CSCROLLBAR_H
