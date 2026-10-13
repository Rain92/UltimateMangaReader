#ifndef UTILS_H
#define UTILS_H

#include <unistd.h>

#include <QRegularExpression>
#include <QScrollArea>
#include <QScroller>
#include <QtCore>

#include "enums.h"
#include "networkmanager.h"

#ifdef __ARM_NEON__
#include <arm_neon.h>
#endif

QList<QRegularExpressionMatch> getAllRxMatches(const QRegularExpression& rx, const QString& text,
                                               int spos = 0, int epos = -1);

QString makePathLegal(QString filename);

inline bool awaitSignal(QObject* object, const QVector<const char*>& completionSignals, int timeout)
{
    QEventLoop loop;

    for (const auto& signal : completionSignals)
        QObject::connect(object, signal, &loop, SLOT(quit()));

    QTimer timer;
    if (timeout > 0)
    {
        QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
        timer.start(timeout);
    }

    loop.exec();

    return timer.isActive();
}

class BindingClass : public QObject
{
    Q_OBJECT
public:
    BindingClass(QSharedPointer<DownloadJobBase> job, std::function<void()> lambda)
        : QObject(), job(job), lambda(lambda), lambda2(nullptr)
    {
        connect(job);
    }

    BindingClass(QSharedPointer<DownloadJobBase> job,
                 std::function<void(QSharedPointer<DownloadJobBase>)> lambda)
        : QObject(), job(job), lambda(nullptr), lambda2(lambda)
    {
        connect(job);
    }
    ~BindingClass() = default;

private:
    QSharedPointer<DownloadJobBase> job;
    std::function<void()> lambda;
    std::function<void(QSharedPointer<DownloadJobBase>)> lambda2;

    void connect(QSharedPointer<DownloadJobBase> job)
    {
        if (job->isCompleted)
        {
            action();
        }
        // a job that failed already doesn't signal anymore
        else if (job->hasFailed())
        {
            deleteLater();
        }
        else
        {
            QObject::connect(job.get(), &DownloadJobBase::completed, this, &BindingClass::action);
            QObject::connect(job.get(), &DownloadJobBase::downloadError, this, &BindingClass::deleteLater);
        }
    }

    void action()
    {
        if (lambda)
            lambda();
        if (lambda2)
            lambda2(job);
        job.get()->disconnect(this);
        job.clear();
        deleteLater();
    }
};

void executeOnJobCompletion(QSharedPointer<DownloadJobBase> job, std::function<void()> lambda);

void executeOnJobCompletion(QSharedPointer<DownloadJobBase> job,
                            std::function<void(QSharedPointer<DownloadJobBase>)> lambda);

PageTurnDirection conditionalReverse(PageTurnDirection dir, bool condition);

qint64 dirSize(const QString& path);

bool removeDir(const QString& path, const QString& ignore = "");
void activateScroller(QAbstractScrollArea* pArea);

// The device pixel ratio the main window is painted with. It can differ from qApp->devicePixelRatio(), on
// Wayland with fractional scaling that is the ratio of the screens rounded up (2 instead of 1.25).
qreal windowPixelRatio();
void setWindowPixelRatio(qreal ratio);

qint64 getCacheSize();
qint64 getFreeSpace();

unsigned long long getTotalSystemMemory();
// The memory that is free or only holds caches the kernel drops when it needs the memory.
unsigned long long getFreeSystemMemory();
bool enoughFreeSystemMemory();

// For background work that the gui thread shouldn't have to wait for (the device has a single core).
void lowerThreadPriority();
// The calling thread only runs while no other thread wants the cpu (or again normally).
void setIdleThreadPriority(bool idle);

void decryptXorInplace(QByteArray& data, const QByteArray& key);
#ifdef __ARM_NEON__
void decryptXorInplace_NEON(QByteArray& data, const QByteArray& key);
#endif

QByteArray hexstr2array(const QString& str);

#endif  // UTILS_H
