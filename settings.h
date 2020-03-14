#ifndef SETTINGS_H
#define SETTINGS_H

#include <QTimer>
#include <QtCore>

#include "defines.h"

class Settings : QObject
{
    Q_OBJECT

public:
    Settings();

    void deserialize();
    void scheduleSerialize();

    int lightvalue;
    int comflightvalue;

public slots:
    void serialize();

private:
    QTimer timer;
};

QDataStream &operator<<(QDataStream &str, const Settings &m);

QDataStream &operator>>(QDataStream &str, Settings &m);

#endif  // SETTINGS_H
