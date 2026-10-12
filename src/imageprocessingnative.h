#ifndef IMAGEPROCESSINGNATIVE_H
#define IMAGEPROCESSINGNATIVE_H

#include <png.h>
#include <turbojpeg.h>

#include <QImage>

#include "greyscaleimage.h"
#include "imageprocessingqt.h"

QImage loadQImageFast(const QString &path, bool useSWDithering = true);

QImage processImageN(const QByteArray &buffer, const QString &filepath, QSize screenSize,
                     DoublePageMode doublePageMode, bool trim, bool manhwaMode, bool useSWDither);

// Rotates, trims, rescales and saves an image decoded by Qt (any format) with the native pipeline.
QImage processDecodedImage(const QImage &decoded, const QString &filepath, QSize screenSize,
                           DoublePageMode doublePageMode, bool trim, bool manhwaMode);

#endif  // IMAGEPROCESSINGNATIVE_H
