#include "mangaimagewidget.h"

#include <QMouseEvent>
#include <QPainter>

#include "perftimer.h"

#ifdef KOBO
#include "koboplatformfunctions.h"
#endif

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

    int height = displaySize(*image, deviceSize()).height();
    auto newvOffset = qMax(qMin(y, (int)(height / devicePixelRatioF()) - this->height()), 0);

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
    const QSize device(qRound(this->width() * pixelRatio), qRound(this->height() * pixelRatio));

    bool tall = false;
    const QSize size = displaySize(*img, device, &tall);

    int x = (device.width() - size.width()) / 2;
    int y = tall ? -qRound(vOffset * pixelRatio) : (device.height() - size.height()) / 2;

    painter.setWorldTransform(QTransform::fromScale(1.0 / pixelRatio, 1.0 / pixelRatio));
    if (size == img->size())
    {
        painter.drawImage(QPoint(x, y), *img);
    }
    else
    {
        painter.setRenderHint(QPainter::SmoothPixmapTransform);
        painter.drawImage(QRect(QPoint(x, y), size), *img);
    }
}

QSize MangaImageWidget::deviceSize() const
{
    return QSize(qRound(this->width() * devicePixelRatioF()), qRound(this->height() * devicePixelRatioF()));
}

// The size a page is shown at, in device pixels. Pages that were rendered for a larger window (on the desktop,
// e.g. before the window was resized) are scaled down to fit. tall: the page is scrolled vertically.
QSize MangaImageWidget::displaySize(const QImage &img, QSize device, bool *tall) const
{
    // pages cached by older versions can be up to 4 px wider (calcRescaleSize() rounded the width up to a
    // multiple of 4), those are still blitted
    const int tolerance = 4;

    QSize size = img.size();
    bool isTall = size.height() > device.height() * 1.1;

    if (size.width() > device.width() + tolerance || (!isTall && size.height() > device.height() + tolerance))
    {
        // like the manhwa mode of calcRescaleSize()
        isTall = (double)size.height() / size.width() > 1.6 * device.height() / device.width();
        size = isTall ? size.scaled(device.width(), INT_MAX, Qt::KeepAspectRatio)
                      : size.scaled(device, Qt::KeepAspectRatio);
    }

    if (tall)
        *tall = isTall;
    return size;
}
