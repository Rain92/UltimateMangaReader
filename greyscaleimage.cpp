#include "greyscaleimage.h"

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
    dither_auto(buffer, width, height);
}

GreyscaleImage GreyscaleImage::rotate90()
{
    QByteArray newBuffer;
    newBuffer.resize(width * height);
    int nw = height;
    int nh = width;
    for (int y = 0; y < nh; y++)
        for (int x = 0; x < nw; x++)
            //                p2[y * nw + x] = p1[x * width + (width - 1 - y)];
            newBuffer[y * nw + x] = buffer[(height - 1 - x) * width + y];

    return GreyscaleImage({nw, nh}, qMove(newBuffer));
}

GreyscaleImage GreyscaleImage::crop(const QRect &rect)
{
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
