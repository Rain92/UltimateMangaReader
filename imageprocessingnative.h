#ifndef IMAGEPROCESSINGNATIVE_H
#define IMAGEPROCESSINGNATIVE_H

#include <png.h>
#include <turbojpeg.h>

#include <QDebug>
#include <QElapsedTimer>
#include <QFile>
#include <QImage>
#include <QScopeGuard>
#include <cstring>

#include "dithering.h"
#include "greyscaleimage.h"
#include "imageprocessingqt.h"
#include "thirdparty/ne10_imgproc.h"

bool isJpeg(const QByteArray &buffer);
bool isPng(const QByteArray &buffer);

QImage loadQImageFast(const QString &path, bool useSWDithering = false);

GreyscaleImage loadFromJpegAndRotate(const QByteArray &buffer, QSize screenSize, bool doublePageFullscreen);

QImage processImageN(const QByteArray &buffer, const QString &filepath, QSize screenSize,
                     bool doublePageFullscreen, bool trim, bool manhwaMode, bool useSWDither);

#endif  // IMAGEPROCESSINGNATIVE_H
