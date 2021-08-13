#include "mangaimagewidget.h"

MangaImageWidget::MangaImageWidget(QWidget *parent)
    : QFrame(parent),
      lastY(0),
      vOffset(0),
      showError(false),
      errorImage(new QImage(":/images/icons/file-error.png"))
{
}

void MangaImageWidget::setImage(QSharedPointer<QImage> img)
{
    showError = false;
    vOffset = 0;
    image = img;
    update();
}

void MangaImageWidget::clearImage()
{
    showError = false;
    vOffset = 0;
    image.clear();
    update();
}

void MangaImageWidget::setVOffset(int y)
{
    if (!image || image.isNull())
        return;

    auto newvOffset = qMax(qMin(y, (int)(image->height() / qApp->devicePixelRatio()) - this->height()), 0);

    if (newvOffset != vOffset)
    {
        vOffset = newvOffset;
        this->update();
    }
}

void MangaImageWidget::showErrorImage()
{
    showError = true;
    vOffset = 0;
    image.clear();
    update();
}

void MangaImageWidget::mousePressEvent(QMouseEvent *event)
{
    lastY = event->y();
}

void MangaImageWidget::mouseReleaseEvent(QMouseEvent *event)
{
    lastY = event->y();
#ifdef KOBO
    KoboPlatformFunctions::setFullScreenRefreshMode(WaveForm_GC16);
    this->update();
#endif
}

void MangaImageWidget::mouseMoveEvent(QMouseEvent *event)
{
    int deltay = lastY - event->y();
    lastY = event->y();
    setVOffset(vOffset + 2 * deltay);
#ifdef KOBO
    KoboPlatformFunctions::setFullScreenRefreshMode(WaveForm_A2);
#endif
}

void MangaImageWidget::paintEvent(QPaintEvent *)
{
    //    QElapsedTimer t;
    //    t.start();

    QPainter painter(this);
    auto pixelRatio = qApp->devicePixelRatio();
    auto img = image;

    if (!img || img.isNull())
    {
        if (showError)
        {
            img = errorImage;
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

    if (img->height() > this->height() * pixelRatio * 1.1)
        y = -vOffset;

    painter.drawImage(QRect(x, y, img->width() / pixelRatio, img->height() / pixelRatio), *img);

    //    qDebug() << "Image Painting:" << t.elapsed();
}
