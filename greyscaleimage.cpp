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
#ifdef KOBO
    img_resize_bilinear_grey_neon((uchar *)newBuffer.data(), newSize.width(), newSize.height(),
                                  (uchar *)buffer.data(), width, height, width);
#else
    img_resize_bilinear_grey_c((uchar *)newBuffer.data(), newWidth, newHeight, (uchar *)buffer.data(), width,
                               height, width);
#endif

    return GreyscaleImage(newSize, qMove(newBuffer));
}

#define DIV255(V)                  \
    ({                             \
        auto _v = (V) + 128;       \
        (((_v >> 8U) + _v) >> 8U); \
    })

static uint8_t dither_o8x8(unsigned short int x, unsigned short int y, uint8_t v)

{
    // c.f.,
    // https://github.com/ImageMagick/ImageMagick/blob/ecfeac404e75f304004f0566557848c53030bad6/config/thresholds.xml#L107
    static const uint8_t threshold_map_o8x8[] = {
        1,  49, 13, 61, 4,  52, 16, 64, 33, 17, 45, 29, 36, 20, 48, 32, 9,  57, 5,  53, 12, 60,
        8,  56, 41, 25, 37, 21, 44, 28, 40, 24, 3,  51, 15, 63, 2,  50, 14, 62, 35, 19, 47, 31,
        34, 18, 46, 30, 11, 59, 7,  55, 10, 58, 6,  54, 43, 27, 39, 23, 42, 26, 38, 22};

    // Constants:
    // Quantum = 8; Levels = 16; map Divisor = 65
    // QuantumRange = 0xFF
    // QuantumScale = 1.0 / QuantumRange
    //
    // threshold = QuantumScale * v * ((L-1) * (D-1) + 1)
    int16_t t = (int16_t)DIV255(v * ((15U << 6) + 1U));
    // level = t / (D-1);
    int16_t l = (t >> 6);
    // t -= l * (D-1);
    t = (int16_t)(t - (l << 6));

    // map width & height = 8
    // c = ClampToQuantum((l+(t >= map[(x % mw) + mw * (y % mh)])) * QuantumRange / (L-1));
    int16_t q = (int16_t)((l + (t >= threshold_map_o8x8[(x & 7U) + 8U * (y & 7U)])) * 17);
    uint8_t p;
    if (q > 0xFF)
    {
        p = 0xFF;
    }
    else if (q < 0)
    {
        p = 0;
    }
    else
    {
        p = (uint8_t)q;
    }

    return p;
}

void GreyscaleImage::dither()
{
    for (int y = 0, p = 0; y < height; y++)
        for (int x = 0; x < width; x++, p++)
            buffer[p] = dither_o8x8(x, y, buffer[p]);
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
