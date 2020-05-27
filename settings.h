#ifndef SETTINGS_H
#define SETTINGS_H

#include <QDataStream>
#include <QFile>
#include <QTimer>

#include "enums.h"
#include "staticsettings.h"

class Settings
{
public:
    Settings();

    void deserialize();
    void serialize();
    void scheduleSerialize();

    int lightValue;
    int comflightValue;

    bool hideErrorMessages;

    AdvancePageGestureDirection tabAdvance;
    AdvancePageGestureDirection swipeAdvance;
    AdvancePageHWButton buttonAdvance;

    bool doublePageFullscreen;

    QMap<QString, bool> enabledMangaSources;

private:
    QTimer timer;
};

QDataStream &operator<<(QDataStream &str, const Settings &m);

QDataStream &operator>>(QDataStream &str, Settings &m);

#endif  // SETTINGS_H
