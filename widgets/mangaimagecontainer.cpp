#include "mangaimagecontainer.h"

#include <QDebug>
#include <QFileInfo>
#include <QPainter>

MangaImageContainer::MangaImageContainer(QWidget *parent) : QFrame(parent) {}

// void MangaImageContainer::mousePressEvent(QMouseEvent *event)
//{
//        emit clicked(event->pos());
//}

void MangaImageContainer::setImage(const QString &path)
{
    if (QFile::exists(path))
    {
        pixmap.reset(new QPixmap(path));
        update();
    }
}

void MangaImageContainer::setImage(QSharedPointer<QPixmap> img)
{
    pixmap = img;

    update();
}

void MangaImageContainer::paintEvent(QPaintEvent *)
{
    if (pixmap)
    {
        QPainter painter(this);

        int x = (this->size().width() - pixmap->width()) / 2;
        int y = (this->size().height() - pixmap->height()) / 2;

        painter.drawPixmap(x, y, pixmap->width(), pixmap->height(), *pixmap);
    }
}
