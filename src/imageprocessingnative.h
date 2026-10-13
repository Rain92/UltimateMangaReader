#pragma once

#include <QImage>

#include "enums.h"

QImage loadQImageFast(const QString &path);

// decoded: whether the image could be decoded (a null result then failed later, e.g. saving it)
QImage processImageN(const QByteArray &buffer, const QString &filepath, QSize screenSize,
                     DoublePageMode doublePageMode, bool trim, bool manhwaMode, bool *decoded = nullptr);

// Decodes both images and processes them as one page, the second one below the first (they need the same
// width). Returns a null image if that isn't possible.
QImage processStitchedImageN(const QByteArray &top, const QByteArray &bottom, const QString &filepath,
                             QSize screenSize, DoublePageMode doublePageMode, bool trim, bool manhwaMode);

// Rotates, trims, rescales and saves an image decoded by Qt (any format) with the native pipeline.
QImage processDecodedImage(const QImage &decoded, const QString &filepath, QSize screenSize,
                           DoublePageMode doublePageMode, bool trim, bool manhwaMode);
