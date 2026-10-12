#include "imageprocessor.h"

#ifdef Q_OS_LINUX
#include <sys/resource.h>
#include <sys/syscall.h>
#include <unistd.h>
#endif

#include "imageprocessingnative.h"
#include "imageprocessingqt.h"
#include "perftimer.h"
#include "utils.h"

QImage processDownloadedImage(const ImageProcessingRequest &request)
{
    PerfTimer timer("processImage");

    QByteArray array = request.data;

    if (request.encryption.type == XorEncryption)
    {
#ifdef __ARM_NEON__
        decryptXorInplace_NEON(array, request.encryption.key);
#else
        decryptXorInplace(array, request.encryption.key);
#endif
        timer.lap("decrypt");
    }

    QImage pimg;

    if (isJpeg(array) || isPng(array) || isWebp(array))
        pimg = processImageN(array, request.filepath, request.screenSize, request.doublePageMode,
                             request.trim, request.manhwaMode, request.swDither);

    if (pimg.isNull())
    {
        qDebug() << "Fast decoding failed, using fallback!";

        pimg = processImageQt(array, request.filepath, request.screenSize, request.doublePageMode,
                              request.trim, request.manhwaMode, request.swDither);
    }

    return pimg;
}

ImageProcessor::ImageProcessor(QObject *parent) : QObject(parent), thread(), worker(), nextId(0)
{
    thread.setObjectName("imageprocessor");
    worker.moveToThread(&thread);
    thread.start();

#ifdef Q_OS_LINUX
    // QThread priorities don't apply to SCHED_OTHER on Linux, so lower the nice value of the thread
    // instead: page turns on the gui thread shouldn't have to wait for preloads.
    QMetaObject::invokeMethod(
        &worker, [] { setpriority(PRIO_PROCESS, syscall(SYS_gettid), 5); }, Qt::QueuedConnection);
#endif
}

ImageProcessor::~ImageProcessor()
{
    // pending requests are dropped, the running one finishes first
    thread.quit();
    thread.wait();
}

quint64 ImageProcessor::process(const ImageProcessingRequest &request)
{
    const quint64 id = ++nextId;

    QMetaObject::invokeMethod(
        &worker,
        [this, id, request]
        {
            QImage image = processDownloadedImage(request);
            emit finished(id, image);
        },
        Qt::QueuedConnection);

    return id;
}
