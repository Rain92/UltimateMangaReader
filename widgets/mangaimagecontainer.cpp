#include "mangaimagecontainer.h"

#include <QApplication>
#include <QDebug>
#include <QFileInfo>
#include <QPainter>
#include <QScreen>

MangaImageContainer::MangaImageContainer(QWidget *parent)
    : QFrame(parent), errorPixmap(new QPixmap(":/images/icons/file-error.png"))
{
}

void MangaImageContainer::setImage(QSharedPointer<QPixmap> img)
{
    showError = false;
    pixmap = img;
    update();
}

void MangaImageContainer::clearImage()
{
    showError = false;
    pixmap.clear();
    update();
}

void MangaImageContainer::showErrorImage()
{
    showError = true;
    pixmap.clear();
    update();
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

    painter.drawPixmap(x, y, img->width() / pixelRatio, img->height() / pixelRatio, *img);
}
