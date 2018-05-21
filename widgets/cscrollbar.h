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
    CScrollBar(Qt::Orientation orientation, QWidget *parent, int extend, bool engargeslider);


protected:
    QSize sizeHint() const;
    QSize minimumSizeHint() const;
    void paintEvent(QPaintEvent *event);

private:
    int extend;
    bool enlargeslider;
};


#endif // CSCROLLBAR_H
