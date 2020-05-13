#include "mangaimagecontainer.h"

#include <QApplication>
#include <QDebug>
#include <QFileInfo>
#include <QPainter>
#include <QScreen>

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
    else
    {
        clearImage();
    }
}

void MangaImageContainer::setImage(QSharedPointer<QPixmap> img)
{
    pixmap = img;

    update();
}

void MangaImageContainer::clearImage()
{
    pixmap.clear();
    update();
}

void MangaImageContainer::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    qreal pixelRatio = qApp->devicePixelRatio();
    if (pixmap)
    {
        int x = (this->size().width() - pixmap->width() / qApp->devicePixelRatio()) / 2;
        int y = (this->size().height() - pixmap->height() / qApp->devicePixelRatio()) / 2;
        qDebug() << this->size() * pixelRatio << pixmap->size();

        painter.drawPixmap(x, y, pixmap->width() / qApp->devicePixelRatio(),
                           pixmap->height() / qApp->devicePixelRatio(), *pixmap);
    }
    else
    {
        painter.fillRect(this->rect(), QColor::fromRgb(255, 255, 255));
    }
}
