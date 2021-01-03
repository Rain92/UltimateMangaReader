#ifndef UTILS_H
#define UTILS_H

#include <unistd.h>

#include <QRegularExpression>
#include <QScrollArea>
#include <QScroller>
#include <QtCore>

#include "enums.h"
#include "networkmanager.h"

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
        if (!job->isCompleted)
        {
            QObject::connect(job.get(), &DownloadJobBase::completed, this, &BindingClass::action);
            QObject::connect(job.get(), &DownloadJobBase::downloadError, this, &BindingClass::deleteLater);
        }
        else
        {
            action();
        }
    }

    void action()
    {
        if (lambda)
            lambda();
        if (lambda2)
            lambda2(job);
        job.get()->disconnect();
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

qint64 getCacheSize();
qint64 getFreeSpace();

unsigned long long getTotalSystemMemory();
unsigned long long getFreeSystemMemory();
bool enoughFreeSystemMemory();

void decryptXorInplace(QByteArray& data, const QByteArray& key);
QByteArray hexstr2array(QString str);

#endif  // UTILS_H
