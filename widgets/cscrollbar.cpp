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
    extent(-1),
    sliderlength(scrollbarsliderminlength),
    mousedownonslider(false),
    clickoffset()
{
}
CScrollBar::CScrollBar(Qt::Orientation orientation, QWidget *parent, int extent):
    QScrollBar(orientation, parent),
    extent(extent),
    sliderlength(scrollbarsliderminlength),
    mousedownonslider(false),
    clickoffset()
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
        size = QSize(scrollBarExtent * 2 + scrollBarSliderMin, extent > 0 ? extent : scrollbarheight);
    else
        size = QSize(extent > 0 ? extent : scrollbarwidth, scrollBarExtent * 2 + scrollBarSliderMin);

    return size;
}

QSize CScrollBar::minimumSizeHint() const
{
    return sizeHint();
}



void CScrollBar::paintEvent(QPaintEvent *)
{
    //    QScrollBar::paintEvent(e);

    adjustLength();
    QPainter painter(this);
    QStyleOptionSlider opt;
    initStyleOption(&opt);
    opt.subControls = QStyle::SC_All;
    QWidget *widget = this;

    QPainter *p = &painter;

    if (const QStyleOptionSlider *scrollbar = &opt)
    {
        QStyleOptionSlider newScrollbar = *scrollbar;
        QStyle::State saveFlags = scrollbar->state;
        if (scrollbar->subControls & QStyle::SC_ScrollBarSlider)
        {

            newScrollbar.rect = scrollbar->rect;

            newScrollbar.state = saveFlags;
            QRect rect = subControlRect(QStyle::CC_ScrollBar, &newScrollbar, QStyle::SC_ScrollBarSlider, widget);
            newScrollbar.rect = QRect(rect.topLeft(), QSize(rect.width(), rect.height()));

            if (newScrollbar.rect.isValid())
            {
                style()->drawControl(QStyle::CE_ScrollBarSlider, &newScrollbar, p, widget);

            }
        }
    }

}


void CScrollBar::setSliderLength(int length)
{
    sliderlength = length;
//    qDebug() << length << minimum() << value() << maximum();
}

void CScrollBar::adjustLength()
{
    int maxlen = (orientation() == Qt::Horizontal) ?
                 width() : height();

//    qDebug() << maxlen << sliderlength << minimum() << value() << maximum();

    if (maxlen - sliderlength > maximum() - minimum())
        setSliderLength(-maximum() + minimum() + maxlen);
}

void CScrollBar::mousePressEvent(QMouseEvent *e)
{


    QPoint click = e->pos();

    int maxlen = (orientation() == Qt::Horizontal) ? width() : height();
    int cpos = (orientation() == Qt::Horizontal) ?  click.x() : click.y();

    int sliderstart = this->sliderPositionFromValue(minimum(),
                                                    maximum(),
                                                    value(),
                                                    maxlen - sliderlength,
                                                    false);


    if (cpos < sliderstart)
    {
        setValue(value() - pageStep());
    }
    else if (cpos > sliderstart + sliderlength)
    {
        setValue(value() + pageStep());
//        qDebug() << value() << maximum();
    }
    else
    {
        mousedownonslider = true;
        clickoffset = click;
//        qDebug() << "clicked";
    }
}

void CScrollBar::mouseMoveEvent(QMouseEvent *e)
{
    if (!mousedownonslider)
        return;

    QPoint click = e->pos();

    int delta = (orientation() == Qt::Horizontal) ? clickoffset.x() - click.x() : clickoffset.y() - click.y();

    clickoffset = e->pos();

    int maxlen = (orientation() == Qt::Horizontal) ?
                 width() : height();

    float valueperpixel = (float)(minimum() - maximum()) / (maxlen - sliderlength);
    setValue(value() + delta * valueperpixel);
}


void CScrollBar::mouseReleaseEvent(QMouseEvent *)
{
    mousedownonslider = false;
}


QRect CScrollBar::subControlRect(QStyle::ComplexControl cc, const QStyleOptionComplex *opt, QStyle::SubControl sc, /*const*/ QWidget *widget)
{
    QRect ret;
    switch (cc)
    {
    case QStyle::CC_ScrollBar:
        if (const QStyleOptionSlider *scrollbar = qstyleoption_cast<const QStyleOptionSlider *>(opt))
        {
            const QRect scrollBarRect = scrollbar->rect;

            int maxlen = ((scrollbar->orientation == Qt::Horizontal) ?
                          scrollBarRect.width() : scrollBarRect.height());

            int sliderlen = ((CScrollBar *)widget)->sliderlength;

            int sliderstart =  this->sliderPositionFromValue(scrollbar->minimum,
                                                             scrollbar->maximum,
                                                             scrollbar->sliderPosition,
                                                             maxlen - sliderlen,
                                                             scrollbar->upsideDown);

            switch (sc)
            {

            case QStyle::SC_ScrollBarSlider:
                if (scrollbar->orientation == Qt::Horizontal)
                    ret.setRect(sliderstart, 0, sliderlen, scrollBarRect.height());
                else
                    ret.setRect(0, sliderstart, scrollBarRect.width(), sliderlen);

                break;
            default:
                break;
            }
            ret = visualRect(scrollbar->direction, scrollBarRect, ret);
        }
    }
//    qDebug() << ret.width() << ret.height() ;
    return ret;
}


QRect CScrollBar::visualRect(Qt::LayoutDirection direction, const QRect &boundingRect, const QRect &logicalRect)
{
    if (direction == Qt::LeftToRight)
        return logicalRect;
    QRect rect = logicalRect;
    rect.translate(2 * (boundingRect.right() - logicalRect.right()) +
                   logicalRect.width() - boundingRect.width(), 0);
    return rect;
}

int CScrollBar::sliderPositionFromValue(int min, int max, int logicalValue, int span, bool upsideDown)
{
    if (span <= 0 || logicalValue < min || max <= min)
        return 0;
    if (logicalValue > max)
        return upsideDown ? span : min;

    uint range = max - min;
    uint p = upsideDown ? max - logicalValue : logicalValue - min;

    if (range > (uint)INT_MAX / 4096)
    {
        double dpos = (double(p)) / (double(range) / span);
        return int(dpos);
    }
    else if (range > (uint)span)
    {
        return (2 * p * span + range) / (2 * range);
    }
    else
    {
        uint div = span / range;
        uint mod = span % range;
        return p * div + (2 * p * mod + range) / (2 * range);
    }
    // equiv. to (p * span) / range + 0.5
    // no overflow because of this implicit assumption:
    // span <= 4096
}
