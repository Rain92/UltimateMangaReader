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
    lastY = event->pos().y();
}

void MangaImageWidget::mouseReleaseEvent(QMouseEvent *event)
{
    lastY = event->pos().y();
#ifdef KOBO
    KoboPlatformFunctions::setFullScreenRefreshMode(WaveForm_GC16);
    this->update();
#endif
}

void MangaImageWidget::mouseMoveEvent(QMouseEvent *event)
{
    int deltay = lastY - event->pos().y();
    lastY = event->pos().y();
    setVOffset(vOffset + 2 * deltay);
#ifdef KOBO
    KoboPlatformFunctions::setFullScreenRefreshMode(WaveForm_A2);
#endif
}

void MangaImageWidget::paintEvent(QPaintEvent *)
{
    PerfTimer timer("paintEvent");

    QPainter painter(this);
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

    // The pages are already rendered at screen resolution, so they are placed in device pixels.
    // Cancelling the high dpi scaling of the painter lets the raster engine blit the image instead of
    // resampling every pixel through its generic (and for a fractional scale lossy) transform path.
    const qreal pixelRatio = painter.device()->devicePixelRatioF();
    const int deviceWidth = qRound(this->width() * pixelRatio);
    const int deviceHeight = qRound(this->height() * pixelRatio);

    int x = (deviceWidth - img->width()) / 2;
    int y = (deviceHeight - img->height()) / 2;

    if (img->height() > deviceHeight * 1.1)
        y = -qRound(vOffset * pixelRatio);

    painter.setWorldTransform(QTransform::fromScale(1.0 / pixelRatio, 1.0 / pixelRatio));
    painter.drawImage(QPoint(x, y), *img);
}
