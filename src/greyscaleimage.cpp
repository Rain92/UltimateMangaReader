#include "greyscaleimage.h"

#include <webp/decode.h>

#ifdef __ARM_NEON__
#include <arm_neon.h>
#endif

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

bool isWebp(const QByteArray &buffer)
{
    return buffer.size() >= 12 && memcmp(buffer.constData(), "RIFF", 4) == 0 &&
           memcmp(buffer.constData() + 8, "WEBP", 4) == 0;
}

bool readWebpSize(const QByteArray &data, QSize &size)
{
    int width = 0, height = 0;
    if (!WebPGetInfo((const uint8_t *)data.constData(), data.size(), &width, &height))
        return false;
    size = QSize(width, height);
    return true;
}

namespace
{
struct TjHandle
{
    explicit TjHandle(int initType) : handle(tj3Init(initType)) {}
    ~TjHandle()
    {
        if (handle)
            tj3Destroy(handle);
    }
    operator tjhandle() const { return handle; }
    tjhandle handle;
};

// Reads the header and returns the image size, or an invalid size on error.
QSize jpegHeaderSize(tjhandle handle, const QByteArray &data)
{
    if (tj3DecompressHeader(handle, (const uchar *)data.constData(), data.size()) < 0)
        return QSize();

    return QSize(tj3Get(handle, TJPARAM_JPEGWIDTH), tj3Get(handle, TJPARAM_JPEGHEIGHT));
}

// The largest reduction (N/8) the decoder offers that still yields at least minSize.
tjscalingfactor chooseScalingFactor(QSize jpegSize, QSize minSize)
{
    tjscalingfactor best = TJUNSCALED;
    if (!minSize.isValid())
        return best;

    int count = 0;
    tjscalingfactor *factors = tj3GetScalingFactors(&count);
    if (!factors)
        return best;

    double bestRatio = 1.0;
    for (int i = 0; i < count; i++)
    {
        const tjscalingfactor factor = factors[i];
        // Measured on the Cortex-A9: entropy decoding dominates, so a reduced size IDCT only pays off
        // from 1/2 downwards (about 20% for 1/2, more for 3/8 and below). 5/8, 3/4 and 7/8 use the
        // scalar IDCTs with nearly full size output and are slower than a full decode.
        if (factor.num * 2 > factor.denom)
            continue;
        if (TJSCALED(jpegSize.width(), factor) < minSize.width() ||
            TJSCALED(jpegSize.height(), factor) < minSize.height())
            continue;
        double ratio = (double)factor.num / factor.denom;
        if (ratio < bestRatio)
        {
            bestRatio = ratio;
            best = factor;
        }
    }
    return best;
}

bool decodeJpeg(tjhandle handle, const QByteArray &data, uchar *dst, int pitch)
{
    tj3Set(handle, TJPARAM_FASTDCT, 1);
    tj3Set(handle, TJPARAM_FASTUPSAMPLE, 1);
    return tj3Decompress8(handle, (const uchar *)data.constData(), data.size(), dst, pitch, TJPF_GRAY) == 0;
}

bool beginPngRead(png_image &image, const QByteArray &data)
{
    memset(&image, 0, (sizeof image));
    image.version = PNG_IMAGE_VERSION;

    if (!png_image_begin_read_from_memory(&image, data.constData(), data.size()))
        return false;

    image.format = PNG_FORMAT_GRAY;
    return true;
}
}  // namespace

bool readJpegSize(const QByteArray &data, QSize &size)
{
    TjHandle handle(TJINIT_DECOMPRESS);
    if (!handle)
        return false;

    size = jpegHeaderSize(handle, data);
    return size.isValid();
}

QImage decodeToQImage(const QByteArray &data)
{
    if (isJpeg(data))
    {
        TjHandle handle(TJINIT_DECOMPRESS);
        if (!handle)
            return QImage();

        QSize size = jpegHeaderSize(handle, data);
        if (!size.isValid())
            return QImage();

        QImage image(size, QImage::Format_Grayscale8);
        if (image.isNull() || !decodeJpeg(handle, data, image.bits(), image.bytesPerLine()))
            return QImage();

        return image;
    }
    else if (isPng(data))
    {
        png_image pngImage;
        if (!beginPngRead(pngImage, data))
            return QImage();

        QImage image(pngImage.width, pngImage.height, QImage::Format_Grayscale8);
        if (image.isNull() ||
            !png_image_finish_read(&pngImage, NULL, image.bits(), image.bytesPerLine(), NULL))
            return QImage();

        return image;
    }

    return QImage();
}

bool saveQImageAsJpeg(const QImage &image, const QString &path, int quality)
{
    if (image.format() != QImage::Format_Grayscale8)
        return false;

    TjHandle handle(TJINIT_COMPRESS);
    if (!handle)
        return false;

    tj3Set(handle, TJPARAM_QUALITY, quality);
    tj3Set(handle, TJPARAM_SUBSAMP, TJSAMP_GRAY);
    tj3Set(handle, TJPARAM_FASTDCT, 1);

    uchar *jpegBuf = nullptr; /* Dynamically allocate the JPEG buffer */
    size_t jpegSize = 0;

    if (tj3Compress8(handle, image.constBits(), image.width(), image.bytesPerLine(), image.height(),
                     TJPF_GRAY, &jpegBuf, &jpegSize) < 0)
        return false;

    auto bufferGuard = qScopeGuard([&] { tj3Free(jpegBuf); });

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    file.write((const char *)jpegBuf, jpegSize);
    file.close();

    return true;
}

GreyscaleImage::GreyscaleImage() : buffer(), width(0), height(0) {}

GreyscaleImage::GreyscaleImage(QSize size) : buffer(), width(size.width()), height(size.height())
{
    buffer.resize(width * height);
}

GreyscaleImage::GreyscaleImage(QSize size, QByteArray &&imageBuffer)
    : buffer(std::move(imageBuffer)), width(size.width()), height(size.height())
{
    Q_ASSERT(buffer.size() == width * height);
}

bool GreyscaleImage::isValid() const
{
    return width > 0 && height > 0 && buffer.size() == width * height;
}

bool GreyscaleImage::isNull() const
{
    return !isValid();
}

QSize GreyscaleImage::size() const
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

bool GreyscaleImage::loadFromJpeg(const QByteArray &data, QSize minSize)
{
    TjHandle handle(TJINIT_DECOMPRESS);
    if (!handle)
        return false;

    QSize jpegSize = jpegHeaderSize(handle, data);
    if (!jpegSize.isValid())
        return false;

    tjscalingfactor factor = chooseScalingFactor(jpegSize, minSize);
    if (tj3SetScalingFactor(handle, factor) < 0)
        return false;

    width = TJSCALED(jpegSize.width(), factor);
    height = TJSCALED(jpegSize.height(), factor);

    buffer.resize(width * height);
    return decodeJpeg(handle, data, (uchar *)buffer.data(), width);
}

bool GreyscaleImage::loadFromPng(const QByteArray &data)
{
    png_image image;
    if (!beginPngRead(image, data))
        return false;

    width = image.width;
    height = image.height;

    buffer.resize(width * height);

    if (!png_image_finish_read(&image, NULL, buffer.data(), 0, NULL))
        return false;

    return true;
}

GreyscaleImage GreyscaleImage::resize(QSize newSize) const
{
    QByteArray newBuffer;
    newBuffer.resize(newSize.width() * newSize.height());
#ifdef __ARM_NEON__
    Simd::Neon::ResizeBilinear((const uint8_t *)buffer.constData(), width, height, width,
                               (uint8_t *)newBuffer.data(), newSize.width(), newSize.height(),
                               newSize.width(), 1);
#else
    Simd::Base::ResizeBilinear((const uint8_t *)buffer.constData(), width, height, width,
                               (uint8_t *)newBuffer.data(), newSize.width(), newSize.height(),
                               newSize.width(), 1);
#endif
    return GreyscaleImage(newSize, std::move(newBuffer));
}

QImage GreyscaleImage::resizeToQImage(QSize newSize) const
{
    QImage image(newSize, QImage::Format_Grayscale8);
    if (image.isNull())
        return image;
#ifdef __ARM_NEON__
    Simd::Neon::ResizeBilinear((const uint8_t *)buffer.constData(), width, height, width, image.bits(),
                               image.width(), image.height(), image.bytesPerLine(), 1);
#else
    Simd::Base::ResizeBilinear((const uint8_t *)buffer.constData(), width, height, width, image.bits(),
                               image.width(), image.height(), image.bytesPerLine(), 1);
#endif
    return image;
}

GreyscaleImage GreyscaleImage::rotate(int rotation) const
{
    if (rotation % 90 != 0)
        return GreyscaleImage();

    if (rotation == 0)
        return GreyscaleImage(*this);

    int rot = ((rotation % 360) + 360) % 360;
    int nw = rot == 180 ? width : height;
    int nh = rot == 180 ? height : width;

    auto newBuffer = rotateBuffer(buffer, width, height, rot);

    return GreyscaleImage({nw, nh}, std::move(newBuffer));
}

GreyscaleImage GreyscaleImage::crop(QRect rect) const
{
    if (!rect.isValid() || rect.right() > this->width || rect.bottom() > this->height)
        return *this;

    QByteArray newBuffer;
    newBuffer.resize(rect.width() * rect.height());
    for (int c = 0, p1 = width * rect.y() + rect.x(), p2 = 0; c < rect.height();
         c++, p1 += width, p2 += rect.width())
    {
        std::memcpy(newBuffer.data() + p2, buffer.constData() + p1, rect.width());
    }

    return GreyscaleImage(rect.size(), std::move(newBuffer));
}

bool GreyscaleImage::saveAsJpeg(const QString &path) const
{
    return saveQImageAsJpeg(toQImage(), path);
}

QImage GreyscaleImage::toQImage() const
{
    QImage image(width, height, QImage::Format_Grayscale8);
    if (image.isNull())
        return image;

    if (image.bytesPerLine() == width)
        std::memcpy(image.bits(), buffer.constData(), width * height);
    else
        for (int y = 0; y < height; y++)
            std::memcpy(image.scanLine(y), buffer.constData() + y * width, width);

    return image;
}

namespace
{
// VP8 luma is BT.601 limited range (16..235). Stretch it to 0..255 like the decoder's own YUV->RGB
// conversion does: y' = (y - 16) * 255 / 219, computed as ((y - 16) * 149 + 64) >> 7 (149/128 = 1.164).
void expandLumaRange(uchar *data, int count)
{
    int i = 0;
#ifdef __ARM_NEON__
    const uint8x16_t offset = vdupq_n_u8(16);
    const uint8x8_t scale = vdup_n_u8(149);
    const uint16x8_t rounding = vdupq_n_u16(64);
    for (; i + 16 <= count; i += 16)
    {
        const uint8x16_t y = vqsubq_u8(vld1q_u8(data + i), offset);
        const uint16x8_t lo = vaddq_u16(vmull_u8(vget_low_u8(y), scale), rounding);
        const uint16x8_t hi = vaddq_u16(vmull_u8(vget_high_u8(y), scale), rounding);
        vst1q_u8(data + i, vcombine_u8(vqshrn_n_u16(lo, 7), vqshrn_n_u16(hi, 7)));
    }
#endif
    for (; i < count; i++)
    {
        const int v = (qMax(data[i] - 16, 0) * 149 + 64) >> 7;
        data[i] = (uchar)qMin(v, 255);
    }
}
}  // namespace

bool GreyscaleImage::loadFromWebp(const QByteArray &data)
{
    WebPDecoderConfig config;
    if (!WebPInitDecoderConfig(&config))
        return false;

    const uint8_t *bytes = (const uint8_t *)data.constData();
    if (WebPGetFeatures(bytes, data.size(), &config.input) != VP8_STATUS_OK)
        return false;

    // transparent or animated pages are rare, leave them to the generic decoder
    if (config.input.has_alpha || config.input.has_animation)
        return false;

    const int outWidth = config.input.width;
    const int outHeight = config.input.height;

    // Only the luma plane is wanted (the same BT.601 Y the JPEG path yields), but the chroma planes
    // still need somewhere to go.
    const int uvWidth = (outWidth + 1) / 2;
    const int uvHeight = (outHeight + 1) / 2;
    QByteArray chroma(2 * uvWidth * uvHeight, Qt::Uninitialized);

    buffer.resize(outWidth * outHeight);

    config.output.colorspace = MODE_YUV;
    config.output.is_external_memory = 1;
    config.output.width = outWidth;
    config.output.height = outHeight;
    WebPYUVABuffer &yuv = config.output.u.YUVA;
    yuv.y = (uint8_t *)buffer.data();
    yuv.y_stride = outWidth;
    yuv.y_size = outWidth * outHeight;
    yuv.u = (uint8_t *)chroma.data();
    yuv.u_stride = uvWidth;
    yuv.u_size = uvWidth * uvHeight;
    yuv.v = yuv.u + yuv.u_size;
    yuv.v_stride = uvWidth;
    yuv.v_size = uvWidth * uvHeight;

    const VP8StatusCode status = WebPDecode(bytes, data.size(), &config);
    WebPFreeDecBuffer(&config.output);  // no-op for external memory
    if (status != VP8_STATUS_OK)
        return false;

    width = outWidth;
    height = outHeight;
    expandLumaRange((uchar *)buffer.data(), width * height);
    return true;
}
