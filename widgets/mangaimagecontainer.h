#ifndef MANGAIMAGECONTAINER2_H
#define MANGAIMAGECONTAINER2_H

#include <qframe.h>
#include <QMouseEvent>

class MangaImageContainer : public QFrame
{
    Q_OBJECT

public:
    MangaImageContainer(QWidget *parent);

    void setImage(QString path);
    void setImage(const QPixmap &img);

protected:
    void mousePressEvent(QMouseEvent *event);

    virtual void paintEvent(QPaintEvent *);

private:
    QPixmap pixmap;
    int imgwidth, imgheight;

signals:
    void clicked(QPoint);
};

#endif // MANGAIMAGECONTAINER2_H
