#include "mangaimagecontainer.h"

#include <QApplication>
#include <QDebug>
#include <QFileInfo>
#include <QPainter>
#include <QScreen>

MangaImageContainer::MangaImageContainer(QWidget *parent)
    : QFrame(parent),
      lastY(0),
      vOffset(0),
      showError(false),
      errorPixmap(new QPixmap(":/images/icons/file-error.png"))
{
}

void MangaImageContainer::setImage(QSharedPointer<QPixmap> img)
{
    showError = false;
    vOffset = 0;
    pixmap = img;
    update();
}

void MangaImageContainer::clearImage()
{
    showError = false;
    vOffset = 0;
    pixmap.clear();
    update();
}

void MangaImageContainer::setVOffset(int y)
{
    vOffset = qMax(qMin(y, (int)(pixmap->height() / qApp->devicePixelRatio()) - this->height()), 0);
    this->update();
}

void MangaImageContainer::showErrorImage()
{
    showError = true;
    vOffset = 0;
    pixmap.clear();
    update();
}

void MangaImageContainer::mousePressEvent(QMouseEvent *event)
{
    lastY = event->y();
}

void MangaImageContainer::mouseMoveEvent(QMouseEvent *event)
{
    setVOffset(vOffset + lastY - event->y());
    lastY = event->y();
}

void MangaImageContainer::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    auto pixelRatio = qApp->devicePixelRatio();
    auto img = pixmap;

    if (!img || img.isNull())
    {
        if (showError)
        {
            img = errorPixmap;
        }
        else
        {
            painter.fillRect(this->rect(), QColor::fromRgb(255, 255, 255));
            return;
        }
    }

    int x = (this->size().width() - img->width() / pixelRatio) / 2;
    int y = (this->size().height() - img->height() / pixelRatio) / 2;

    if (pixmap->height() > this->height() * 1.1)
        y = -vOffset;

    painter.drawPixmap(x, y, img->width() / pixelRatio, img->height() / pixelRatio, *img);
}
