
#include "imageprocessingnative.h"

QByteArray cropImage(QRect rect, const QByteArray &src, int srcwidth)
{
    QByteArray imgBuf2;
    imgBuf2.resize(rect.width() * rect.height());
    for (int c = 0, p1 = srcwidth * rect.y() + rect.x(), p2 = 0; c < rect.height();
         c++, p1 += srcwidth, p2 += rect.width())
    {
        std::memcpy(imgBuf2.data() + p2, src.data() + p1, rect.width());
    }
    return imgBuf2;
}

QImage loadJpegFast(const QByteArray &jpegBuf, tjhandle tjInstanceD)
{
    int pixelFormat = TJPF_GRAY;
    int flags = TJFLAG_FASTDCT | TJFLAG_FASTUPSAMPLE;
    int inSubsamp, inColorspace;
    int width, height;

    bool cleanup = false;
    if (!tjInstanceD)
    {
        tjInstanceD = tjInitDecompress();
        if (!tjInstanceD)
            return QImage();
        cleanup = true;
    }
    auto guard = qScopeGuard([&] {
        if (cleanup)
            tjDestroy(tjInstanceD);
    });

    if (tjDecompressHeader3(tjInstanceD, (uchar *)jpegBuf.data(), jpegBuf.size(), &width, &height, &inSubsamp,
                            &inColorspace) < 0)
        return QImage();

    QByteArray imgBuf;
    imgBuf.resize(width * height);

    if (tjDecompress2(tjInstanceD, (uchar *)jpegBuf.data(), jpegBuf.size(), (uchar *)imgBuf.data(), width, 0,
                      height, pixelFormat, flags) < 0)
        return QImage();

    QImage retimgT((uchar *)imgBuf.data(), width, height, width, QImage::Format_Grayscale8);

    // make copy to take ownership of data safely
    auto retimg = retimgT.copy();

    return retimg;
}

QImage loadJpegFast(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return QImage();

    auto array = file.readAll();
    file.close();

    return loadJpegFast(array);
}

bool saveJpegFast(const QByteArray &imgBuf, QString filename, int width, int height, tjhandle tjInstanceC)
{
    int outSubsamp = TJSAMP_GRAY, outQual = 85;
    int pixelFormat = TJPF_GRAY;
    int flags = TJFLAG_FASTDCT | TJFLAG_FASTUPSAMPLE;

    uchar *newJpegBuf = nullptr; /* Dynamically allocate the JPEG buffer */
    ulong newJpegSize = 0;

    if (tjCompress2(tjInstanceC, (uchar *)imgBuf.data(), width, 0, height, pixelFormat, &newJpegBuf,
                    &newJpegSize, outSubsamp, outQual, flags) < 0)
        return false;

    auto guard2 = qScopeGuard([&] {
        if (newJpegBuf)
            free(newJpegBuf);
    });

    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    file.write((char *)newJpegBuf, newJpegSize);
    file.close();

    return true;
}

QImage processImageN(QByteArray &jpegBuf, const QString &filepath, QSize screenSize,
                     bool doublePageFullscreen, bool trim, bool manhwaMode)
{
    //    QElapsedTimer t;
    //    t.start();

    int pixelFormat = TJPF_GRAY;
    int flags = TJFLAG_FASTDCT | TJFLAG_FASTUPSAMPLE;
    int inSubsamp, inColorspace;
    int width, height;

    tjhandle tjInstanceD = tjInitDecompress(), tjInstanceC = tjInitCompress(),
             tjInstanceT = tjInitTransform();

    auto handleguard = qScopeGuard([&] {
        if (tjInstanceD)
            tjDestroy(tjInstanceD);
        if (tjInstanceC)
            tjDestroy(tjInstanceC);
        if (tjInstanceT)
            tjDestroy(tjInstanceT);
    });

    if (tjDecompressHeader3(tjInstanceD, (uchar *)jpegBuf.data(), jpegBuf.size(), &width, &height, &inSubsamp,
                            &inColorspace) < 0)
        return QImage();

    auto rot90 = calcRotationInfo(QSize(width, height), screenSize, doublePageFullscreen);

    QByteArray imgBuf;

    QScopedPointer<uchar, QScopedPointerPodDeleter> transformDataGuard;
    if (rot90)
    {
        unsigned char *dstBuf = nullptr; /* Dynamically allocate the JPEG buffer */
        unsigned long dstSize = 0;

        tjtransform xform;
        memset(&xform, 0, sizeof(tjtransform));
        xform.options = TJXOPT_GRAY | TJXOPT_TRIM;
        xform.op = TJXOP_ROT90;

        if (tjTransform(tjInstanceT, (uchar *)jpegBuf.data(), jpegBuf.size(), 1, &dstBuf, &dstSize, &xform,
                        flags) < 0)
        {
            tjFree(dstBuf);
            return QImage();
        }
        jpegBuf = QByteArray::fromRawData((char *)dstBuf, dstSize);
        transformDataGuard.reset(dstBuf);
    }

    if (tjDecompressHeader3(tjInstanceD, (uchar *)jpegBuf.data(), jpegBuf.size(), &width, &height, &inSubsamp,
                            &inColorspace) < 0)
        return QImage();

    //    qDebug() << "Image processing start:" << t.elapsed();
    //    t.restart();

    imgBuf.resize(width * height);
    if (tjDecompress2(tjInstanceD, (uchar *)jpegBuf.data(), jpegBuf.size(), (uchar *)imgBuf.data(), width, 0,
                      height, pixelFormat, flags) < 0)
        return QImage();

    //    qDebug() << "Image processing decompress:" << t.elapsed();
    //    t.restart();

    if (trim)
    {
        auto trimRect = getTrimRect(imgBuf, width, height, width);

        imgBuf = cropImage(trimRect, imgBuf, width);
        width = trimRect.width();
        height = trimRect.height();
    }

    //    qDebug() << "Image processing trim:" << t.elapsed();
    //    t.restart();

    auto rescaleSize = calcRescaleSize(QSize(width, height), screenSize, rot90, manhwaMode);
    int newWidth = rescaleSize.width();
    int newHeight = rescaleSize.height();

    QByteArray imgBuf2;
    imgBuf2.resize(newWidth * newHeight);

#ifdef KOBO
    img_resize_bilinear_grey_neon((uchar *)imgBuf2.data(), newWidth, newHeight, (uchar *)imgBuf.data(), width,
                                  height, width);
#else
    img_resize_bilinear_grey_c((uchar *)imgBuf2.data(), newWidth, newHeight, (uchar *)imgBuf.data(), width,
                               height, width);
#endif

    //    qDebug() << "Image processing rescale:" << t.elapsed();
    //    t.restart();

    if (!saveJpegFast(imgBuf2, filepath, newWidth, newHeight, tjInstanceC))
        return QImage();

    //    qDebug() << "Image processing save:" << t.elapsed();
    //    t.restart();

    QImage retimgT((uchar *)imgBuf2.data(), newWidth, newHeight, newWidth, QImage::Format_Grayscale8);

    // make copy to take ownership of data safely
    auto retimg = retimgT.copy();

    return retimg;
}
