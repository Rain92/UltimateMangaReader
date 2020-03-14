#ifndef UTILS_H
#define UTILS_H

#include <QRegularExpression>
#include <QSharedPointer>

#include "downloadmanager.h"

QList<QRegularExpressionMatch> getAllRxMatches(const QRegularExpression& rx,
                                               const QString& text,
                                               int spos = 0, int epos = -1);

QString makePathLegal(QString filename);

class BindingClass : public QObject
{
    Q_OBJECT
public:
    BindingClass(QSharedPointer<DownloadJobBase> job,
                 std::function<void()> lambda)
        : QObject(), job(job), lambda(lambda)
    {
        if (!job->isCompleted)
        {
            QObject::connect(job.get(), &DownloadJobBase::completed, this,
                             &BindingClass::action);
            QObject::connect(job.get(), &DownloadJobBase::downloadError, this,
                             &BindingClass::deleteLater);
        }
        else
        {
            action();
        }
    }
    ~BindingClass() = default;

private:
    QSharedPointer<DownloadJobBase> job;
    std::function<void()> lambda;

    void action()
    {
        lambda();
        job.clear();
        deleteLater();
    }
};

void executeOnJobCompletion(QSharedPointer<DownloadJobBase> job,
                            std::function<void()> lambda);

#endif  // UTILS_H
