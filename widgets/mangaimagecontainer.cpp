#include "mangaimagecontainer.h"
#include <QDebug>
#include <QFileInfo>
#include <QPainter>
#include <QDebug>


MangaImageContainer::MangaImageContainer(QWidget *parent) :
    QFrame(parent)
{

}

void MangaImageContainer::mousePressEvent(QMouseEvent *event)
{
    emit clicked(event->pos());
}


void MangaImageContainer::setImage(QString path)
{
    QFileInfo fi(path);
    if (fi.exists())
    {
        pixmap.load(path);
//        pixmap= pixmap.scaled(imgwidth, imgheight, Qt::KeepAspectRatio, Qt::FastTransformation); //Qt::SmoothTransformation
        update();
    }
}

void MangaImageContainer::setImage(const QPixmap &img)
{
    pixmap = img;
    update();
}

void MangaImageContainer::paintEvent(QPaintEvent *)
{
    QPainter qp(this);

    int x = (this->size().width() - pixmap.width()) / 2;
    int y = (this->size().height() - pixmap.height()) / 2;

//   qDebug() << "this w h" << imgwidth << imgheight;
//   qDebug() << "pixmap w h" << pixmap.width() << pixmap.height();
//   qDebug() << "x y" << x << y;
    qp.drawPixmap(x, y, pixmap.width(), pixmap.height(), pixmap);
}
