#pragma once

#include <QObject>

#include "networkmanager.h"
#include "staticsettings.h"

class SuspendManager : public QObject
{
    Q_OBJECT

public:
    explicit SuspendManager(NetworkManager *networkManager, QObject *parent = nullptr);

    bool suspend();
    bool resume();

    bool sleeping;

signals:
    void suspending();
    void resuming();

private:
    NetworkManager *networkManager;
    QTimer timer;

    bool suspendInternal();
};
