#include "cscrollbar.h"

#include "qapplication.h"
#include "qcursor.h"
#include "qevent.h"
#include "qpainter.h"
#include "qscrollbar.h"
#include "qstyle.h"
#include "qstyleoption.h"
#include "qmenu.h"
#include <QtCore/qelapsedtimer.h>

#include <limits.h>
#include <QPainter>
#include <QScrollBar>
#include <QPaintEvent>
#include <QDebug>

#include "configs.h"


CScrollBar::CScrollBar(Qt::Orientation orientation, QWidget *parent):
    QScrollBar(orientation, parent),
    extend(-1),
    enlargeslider(true)
{
}
CScrollBar::CScrollBar(Qt::Orientation orientation, QWidget *parent, int extend, bool engargeslider):
    QScrollBar(orientation, parent),
    extend(extend),
    enlargeslider(engargeslider)
{
}


QSize CScrollBar::sizeHint() const
{
    ensurePolished();
    QStyleOptionSlider opt;
    initStyleOption(&opt);

    int scrollBarExtent = style()->pixelMetric(QStyle::PM_ScrollBarExtent, &opt, this);
    int scrollBarSliderMin = style()->pixelMetric(QStyle::PM_ScrollBarSliderMin, &opt, this);
    QSize size;
    if (opt.orientation == Qt::Horizontal)
        size = QSize(scrollBarExtent * 2 + scrollBarSliderMin, extend > 0 ? extend : scrollbarheight);
    else
        size = QSize(extend > 0 ? extend : scrollbarwidth, scrollBarExtent * 2 + scrollBarSliderMin);

    return size;
}

QSize CScrollBar::minimumSizeHint() const
{
    return sizeHint();
}


void CScrollBar::paintEvent(QPaintEvent *event)
{
    if (enlargeslider)
        setPageStep(maximum() / 8);
    QScrollBar::paintEvent(event);
}

