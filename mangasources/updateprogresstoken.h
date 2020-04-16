#ifndef UPDATEPROGRESSTOKEN_H
#define UPDATEPROGRESSTOKEN_H

#include <QMap>
#include <QObject>

class UpdateProgressToken : public QObject
{
    Q_OBJECT

public:
    UpdateProgressToken();

    void sendProgress(int p);
    void sendError(const QString &message);
    void sendFinished();

    QString currentSourceName;
    QMap<QString, int> sourcesProgress;
    bool canceled;
signals:
    void updateProgress();
    void updateError(const QString &message);
    void updateFinished();
};

#endif  // UPDATEPROGRESSTOKEN_H
