#ifndef IMAGEPROCESSINGNATIVE_H
#define IMAGEPROCESSINGNATIVE_H

#include <png.h>
#include <turbojpeg.h>

#include <QImage>

#include "dither.h"
#include "greyscaleimage.h"
#include "imageprocessingqt.h"

bool isJpeg(const QByteArray &buffer);
bool isPng(const QByteArray &buffer);

QImage loadQImageFast(const QString &path, bool useSWDithering = true);

GreyscaleImage loadFromJpegAndRotate(const QByteArray &buffer, QSize screenSize,
                                     DoublePageMode doublePageMode, int &rot90);

QImage processImageN(const QByteArray &buffer, const QString &filepath, QSize screenSize,
                     DoublePageMode doublePageMode, bool trim, bool manhwaMode, bool useSWDither);

#endif  // IMAGEPROCESSINGNATIVE_H
