#pragma once

#include <QByteArray>
#include <QImage>
#include <QRect>
#include <QSize>

// Whether an image of that size may be decoded (memory, and the int sizes of the buffers).
bool decodedSizeAllowed(qint64 width, qint64 height);

bool isJpeg(const QByteArray &buffer);
bool isPng(const QByteArray &buffer);
bool isWebp(const QByteArray &buffer);

// Reads the image size from the WebP header only.
bool readWebpSize(const QByteArray &data, QSize &size);

// Reads the image size from the JPEG header only.
bool readJpegSize(const QByteArray &data, QSize &size);

// Reads the size of a JPEG, PNG or WebP from the start of the file (a few KB are enough unless a JPEG
// has large metadata before its frame header).
bool readImageSize(const QByteArray &header, QSize &size);

// Decodes a JPEG or PNG straight into a Format_Grayscale8 QImage (no intermediate buffer).
QImage decodeToQImage(const QByteArray &data);

bool saveQImageAsJpeg(const QImage &image, const QString &path, int quality = 85);

class GreyscaleImage
{
public:
    GreyscaleImage();
    GreyscaleImage(QSize size);
    GreyscaleImage(QSize size, QByteArray &&buffer);

    QByteArray buffer;

    int width;
    int height;

    QSize size() const;
    bool isValid() const;
    bool isNull() const;

    bool loadFromEncoded(const QByteArray &data);
    // With a valid minSize the JPEG is decoded with the decoder's DCT scaling at the smallest
    // size that is still at least minSize in both dimensions.
    bool loadFromJpeg(const QByteArray &data, QSize minSize = QSize());
    bool loadFromPng(const QByteArray &data);
    // Takes the luma plane of the WebP (lossy or lossless, no alpha). libwebp's rescaling while
    // decoding isn't used: on the Cortex-A9 it is slower than a full decode plus the NEON resize.
    bool loadFromWebp(const QByteArray &data);

    GreyscaleImage resize(QSize newSize) const;
    // Resizes straight into a Format_Grayscale8 QImage
    QImage resizeToQImage(QSize newSize) const;
    GreyscaleImage rotate(int rotation) const;
    GreyscaleImage crop(QRect rect) const;

    bool saveAsJpeg(const QString &path) const;

    QImage toQImage() const;
};
