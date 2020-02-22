#ifndef MANGAIMAGECONTAINER2_H
#define MANGAIMAGECONTAINER2_H

#include <qframe.h>

#include <QMouseEvent>
#include <QPainter>

class MangaImageContainer : public QFrame
{
    Q_OBJECT

public:
    MangaImageContainer(QWidget *parent);

    void setImage(const QString &path);
    void setImage(QSharedPointer<QPixmap> img);

protected:
    //    void mousePressEvent(QMouseEvent *event);

    virtual void paintEvent(QPaintEvent *);

private:
    QSharedPointer<QPixmap> pixmap;

signals:
    void clicked(QPoint);
};

#endif  // MANGAIMAGECONTAINER2_H
