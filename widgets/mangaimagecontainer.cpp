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
    if (!pixmap || pixmap.isNull())
        return;

    auto newvOffset = qMax(qMin(y, (int)(pixmap->height() / qApp->devicePixelRatio()) - this->height()), 0);

    if (newvOffset != vOffset)
    {
        vOffset = newvOffset;
        this->update();
    }
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
    int deltay = lastY - event->y();
    lastY = event->y();
    setVOffset(vOffset + 2 * deltay);
}

void MangaImageContainer::paintEvent(QPaintEvent *)
{
    //    QElapsedTimer t;
    //    t.start();

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

    auto scaledSize = img->size();

    int x = (this->size().width() - scaledSize.width() / pixelRatio) / 2;
    int y = (this->size().height() - scaledSize.height() / pixelRatio) / 2;

    if (img->height() > this->height() * 1.1)
        y = -vOffset;

    painter.drawPixmap(x, y, scaledSize.width() / pixelRatio, scaledSize.height() / pixelRatio, *img);

    //    qDebug() << "Image Painting:" << t.elapsed();
}
