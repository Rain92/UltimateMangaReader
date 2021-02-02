#ifndef GREYSCALEIMAGE_H
#define GREYSCALEIMAGE_H

#include <png.h>
#include <turbojpeg.h>

#include <QDebug>
#include <QElapsedTimer>
#include <QFile>
#include <QImage>
#include <QScopeGuard>
#include <cstring>

#include "thirdparty/ne10_imgproc.h"

class GreyscaleImage
{
public:
    GreyscaleImage();
    GreyscaleImage(QSize size);
    GreyscaleImage(QSize size, QByteArray &&buffer);

    QByteArray buffer;

    int width;
    int height;

    QSize size();
    bool isValid();
    bool isNull();

    bool loadFromJpeg(const QByteArray &data);
    bool loadFromPng(const QByteArray &data);

    GreyscaleImage resize(QSize newSize);
    void dither();
    GreyscaleImage rotate90();
    GreyscaleImage crop(const QRect &rect);

    bool saveAsJpeg(const QString &path);

    QImage toQImage();
};

#endif  // GREYSCALEIMAGE_H
