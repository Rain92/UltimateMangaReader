#include "greyscaleimage.h"

#define JPEG_MAGIC_NUMBER_0 (char)0xFF
#define JPEG_MAGIC_NUMBER_1 (char)0xD8

#define PNG_MAGIC_NUMBER_0 (char)0x89
#define PNG_MAGIC_NUMBER_1 (char)0x50

bool isJpeg(const QByteArray &buffer)
{
    return buffer.size() > 2 && buffer.data()[0] == JPEG_MAGIC_NUMBER_0 &&
           buffer.data()[1] == JPEG_MAGIC_NUMBER_1;
}

bool isPng(const QByteArray &buffer)
{
    return buffer.size() > 2 && buffer.data()[0] == PNG_MAGIC_NUMBER_0 &&
           buffer.data()[1] == PNG_MAGIC_NUMBER_1;
}

GreyscaleImage::GreyscaleImage() : buffer(), width(0), height(0) {}

GreyscaleImage::GreyscaleImage(QSize size) : buffer(), width(size.width()), height(size.height())
{
    buffer.resize(width * height);
}

GreyscaleImage::GreyscaleImage(QSize size, QByteArray &&buffer)
    : buffer(buffer), width(size.width()), height(size.height())
{
    Q_ASSERT(buffer.size() == width * height);
}

bool GreyscaleImage::isValid()
{
    return width > 0 && height > 0 && buffer.size() == width * height;
}

bool GreyscaleImage::isNull()
{
    return !isValid();
}

QSize GreyscaleImage::size()
{
    return QSize(width, height);
}

bool GreyscaleImage::loadFromEncoded(const QByteArray &data)
{
    if (isPng(data))
    {
        return loadFromPng(data);
    }
    else if (isJpeg(data))
    {
        return loadFromJpeg(data);
    }
    else
    {
        return false;
    }
}

bool GreyscaleImage::loadFromJpeg(const QByteArray &data)
{
    tjhandle tjInstanceD = tjInitDecompress();
    int pixelFormat = TJPF_GRAY;
    int flags = TJFLAG_FASTDCT | TJFLAG_FASTUPSAMPLE;
    int inSubsamp, inColorspace;

    auto handleguard = qScopeGuard([&] {
        if (tjInstanceD)
            tjDestroy(tjInstanceD);
    });

    if (tjDecompressHeader3(tjInstanceD, (uchar *)data.data(), data.size(), &width, &height, &inSubsamp,
                            &inColorspace) < 0)
        return false;

    buffer.resize(width * height);
    if (tjDecompress2(tjInstanceD, (uchar *)data.data(), data.size(), (uchar *)buffer.data(), width, 0,
                      height, pixelFormat, flags) < 0)
        return false;

    return true;
}

bool GreyscaleImage::loadFromPng(const QByteArray &data)
{
    png_image image;

    memset(&image, 0, (sizeof image));
    image.version = PNG_IMAGE_VERSION;

    if (!png_image_begin_read_from_memory(&image, data.data(), data.size()))
        return false;

    image.format = PNG_FORMAT_GRAY;

    width = image.width;
    height = image.height;

    buffer.resize(width * height);

    if (!png_image_finish_read(&image, NULL, buffer.data(), 0, NULL))
        return false;

    return true;
}

GreyscaleImage GreyscaleImage::resize(QSize newSize)
{
    QByteArray newBuffer;
    newBuffer.resize(newSize.width() * newSize.height());
#ifdef __ARM_NEON__
    Simd::Neon::ResizeBilinear((uint8_t *)buffer.data(), width, height, width, (uint8_t *)newBuffer.data(),
                               newSize.width(), newSize.height(), newSize.width(), 1);
#else
    Simd::Base::ResizeBilinear((uint8_t *)buffer.data(), width, height, width, (uint8_t *)newBuffer.data(),
                               newSize.width(), newSize.height(), newSize.width(), 1);
#endif
    return GreyscaleImage(newSize, qMove(newBuffer));
}

void GreyscaleImage::dither()
{
    ditherBuffer(buffer, width, height);
}

GreyscaleImage GreyscaleImage::rotate(int rotation)
{
    if (rotation % 90 != 0)
        return GreyscaleImage();

    if (rotation == 0)
        return GreyscaleImage(*this);

    int rot = ((rotation % 360) + 360) % 360;
    int nw = rotation == 180 ? width : height;
    int nh = rotation == 180 ? height : width;

    auto newBuffer = rotateBuffer(buffer, width, height, rot);

    return GreyscaleImage({nw, nh}, qMove(newBuffer));
}

GreyscaleImage GreyscaleImage::crop(QRect rect)
{
    if (!rect.isValid() || rect.right() > this->width || rect.bottom() > this->height)
        return *this;

    QByteArray newBuffer;
    newBuffer.resize(rect.width() * rect.height());
    for (int c = 0, p1 = width * rect.y() + rect.x(), p2 = 0; c < rect.height();
         c++, p1 += width, p2 += rect.width())
    {
        std::memcpy(newBuffer.data() + p2, buffer.data() + p1, rect.width());
    }

    return GreyscaleImage(rect.size(), qMove(newBuffer));
}

bool GreyscaleImage::saveAsJpeg(const QString &path)
{
    int outSubsamp = TJSAMP_GRAY, outQual = 85;
    int pixelFormat = TJPF_GRAY;
    int flags = TJFLAG_FASTDCT | TJFLAG_FASTUPSAMPLE;

    tjhandle tjInstanceC = tjInitCompress();

    auto handleguard = qScopeGuard([&] {
        if (tjInstanceC)
            tjDestroy(tjInstanceC);
    });

    uchar *newJpegBuf = nullptr; /* Dynamically allocate the JPEG buffer */
    ulong newJpegSize = 0;

    if (tjCompress2(tjInstanceC, (uchar *)buffer.data(), width, 0, height, pixelFormat, &newJpegBuf,
                    &newJpegSize, outSubsamp, outQual, flags) < 0)
        return false;

    auto guard2 = qScopeGuard([&] {
        if (newJpegBuf)
            free(newJpegBuf);
    });

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    file.write((char *)newJpegBuf, newJpegSize);
    file.close();

    return true;
}

QImage GreyscaleImage::toQImage()
{
    QImage retimgT((uchar *)buffer.data(), width, height, width, QImage::Format_Grayscale8);

    // make copy to take ownership of data safely
    auto retimg = retimgT.copy();

    return retimg;
}
