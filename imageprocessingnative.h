#ifndef IMAGEPROCESSINGNATIVE_H
#define IMAGEPROCESSINGNATIVE_H

#include <turbojpeg.h>

#include <QDebug>
#include <QElapsedTimer>
#include <QFile>
#include <QImage>
#include <QScopeGuard>
#include <cstring>

#include "imageprocessingqt.h"
#include "thirdparty/ne10_imgproc.h"

bool isJpeg(const QByteArray &buffer);

QImage loadJpegFast(const QByteArray &jpegBuf, tjhandle tjInstanceD = nullptr);
QImage loadJpegFast(const QString &path);

bool saveJpegFast(const QByteArray &imgBuf, QString filename, int width, int height,
                  tjhandle tjInstanceC = nullptr);

QImage processImageN(QByteArray &jpegBuf, const QString &filepath, QSize screenSize,
                     bool doublePageFullscreen, bool trim, bool manhwaMode);

#endif  // IMAGEPROCESSINGNATIVE_H
