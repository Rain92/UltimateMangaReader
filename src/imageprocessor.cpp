#include "imageprocessor.h"

#include "greyscaleimage.h"
#include "imageprocessingnative.h"
#include "imageprocessingqt.h"
#include "perftimer.h"
#include "utils.h"

QImage processDownloadedImage(const ImageProcessingRequest &request)
{
    PerfTimer timer("processImage");

    QByteArray array = request.data;
    QByteArray appendArray = request.appendData;

    if (request.encryption.type == XorEncryption)
    {
        decryptXorInplace(array, request.encryption.key);
        decryptXorInplace(appendArray, request.encryption.key);
        timer.lap("decrypt");
    }

    QImage pimg;

    if (!appendArray.isEmpty())
    {
        pimg = processStitchedImageN(array, appendArray, request.screenSize, request.doublePageMode, request.trim,
                                     request.manhwaMode);
        if (pimg.isNull())
            qDebug() << "Stitching failed, using the first part only:" << request.filepath;
    }

    bool decoded = false;
    if (pimg.isNull() && (isJpeg(array) || isPng(array) || isWebp(array)))
        pimg = processImageN(array, request.screenSize, request.doublePageMode, request.trim, request.manhwaMode,
                             &decoded);

    // Qt's decoders are for what the native ones can't decode, they wouldn't do better with the rest
    if (pimg.isNull() && !decoded)
    {
        qDebug() << "Fast decoding failed, using fallback!";

        pimg = processImageQt(array, request.screenSize, request.doublePageMode, request.trim,
                              request.manhwaMode);
    }

    return pimg;
}

ImageProcessor::ImageProcessor(QObject *parent)
    : QObject(parent), thread(), worker(), nextId(0), pending(0)
{
    thread.setObjectName("imageprocessor");
    worker.moveToThread(&thread);
    thread.start();

    // page turns on the gui thread shouldn't have to wait for preloads
    QMetaObject::invokeMethod(&worker, [] { lowerThreadPriority(); }, Qt::QueuedConnection);
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
    pending.ref();

    QMetaObject::invokeMethod(
        &worker,
        [this, id, request]
        {
            QImage image;
            try
            {
                image = processDownloadedImage(request);
                // e.g. the page the reader waits for shows up while it is saved
                emit processed(id, image);
                if (!image.isNull() && !saveQImageAsJpeg(image, request.filepath))
                {
                    qDebug() << "Couldn't save" << request.filepath;
                    image = QImage();
                }
            }
            catch (const std::bad_alloc &)
            {
                // an exception would end the thread's event loop and the app with it
                qDebug() << "Out of memory processing" << request.filepath;
            }
            emit finished(id, image);
            pending.deref();
        },
        Qt::QueuedConnection);

    return id;
}

bool ImageProcessor::isBusy() const
{
    return pending.loadRelaxed() > 0;
}
