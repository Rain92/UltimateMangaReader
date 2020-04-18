#ifndef SUSPENDMANAGER_H
#define SUSPENDMANAGER_H

#include <fcntl.h>
#include <unistd.h>

#include <QObject>

#include "staticsettings.h"

class SuspendManager : public QObject
{
    Q_OBJECT

public:
    SuspendManager(QObject* parent = nullptr);

    bool suspend();
    bool resume();
    //    bool checkUnexpectedWakeup();

    bool sleeping;

signals:
    void suspending();
    void resuming();

private:
    int numUnexpectedWakups;
};

#endif  // SUSPENDMANAGER_H
