#ifndef UTILS_H
#define UTILS_H

#include <QEventLoop>
#include <QRegularExpression>
#include <QTimer>
#include <QtCore>

#include "enums.h"

QList<QRegularExpressionMatch> getAllRxMatches(const QRegularExpression& rx, const QString& text,
                                               int spos = 0, int epos = -1);

QString makePathLegal(QString filename);

// Runs an event loop until object emits one of the signals, is destroyed or timeout ms passed (no limit when
// timeout <= 0). Returns whether a signal was emitted.
template <typename Object, typename... Signals>
bool awaitSignal(Object* object, int timeout, Signals... completionSignals)
{
    QEventLoop loop;
    bool signalled = false;

    (QObject::connect(object, completionSignals, &loop,
                      [&]()
                      {
                          signalled = true;
                          loop.quit();
                      }),
     ...);
    QObject::connect(object, &QObject::destroyed, &loop, &QEventLoop::quit);

    QTimer timer;
    if (timeout > 0)
    {
        timer.setSingleShot(true);
        QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
        timer.start(timeout);
    }

    loop.exec();

    return signalled;
}

PageTurnDirection conditionalReverse(PageTurnDirection dir, bool condition);

qint64 dirSize(const QString& path);

bool removeDir(const QString& path, const QString& ignore = "");

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

// Xors data with the key repeated (an empty key leaves the data as it is).
void decryptXorInplace(QByteArray& data, const QByteArray& key);

QByteArray hexstr2array(const QString& str);

#endif  // UTILS_H
