#pragma once

#include <QHash>
#include <QMap>
#include <QObject>
#include <any>

class UpdateProgressToken : public QObject
{
    Q_OBJECT

public:
    UpdateProgressToken();

    void sendProgress(int p);
    void sendError(const QString &message);
    void sendFinished();

    // What the current source got done so far: a retry with the same token continues from there instead of
    // starting over. It is cleared when the source finished.
    template <typename T>
    T &resumeState()
    {
        auto &state = resumeStates[currentSourceName];
        if (!std::any_cast<T>(&state))
            state = T();
        return *std::any_cast<T>(&state);
    }
    void clearResumeState();

    QString currentSourceName;
    QMap<QString, int> sourcesProgress;
    bool canceled;
signals:
    void updateProgress();
    void updateError(const QString &message);
    void updateFinished();

private:
    QHash<QString, std::any> resumeStates;
};
