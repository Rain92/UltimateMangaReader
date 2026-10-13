#pragma once

#include <QDataStream>
#include <QFile>
#include <QMap>
#include <QTimer>

#include "enums.h"

class Settings
{
public:
    Settings();
    // saves a change that is still waiting for the timer
    ~Settings();

    void deserialize();
    void serialize();
    void scheduleSerialize();
    void resetToDefaults();

    int lightValue;
    int comflightValue;

    bool hideErrorMessages;

    AdvancePageGestureDirection tabAdvance;
    AdvancePageGestureDirection swipeAdvance;
    AdvancePageHWButton buttonAdvance;

    MangaOrderMethod mangaOrder;

    DoublePageMode doublePageMode;
    bool trimPages;
    bool manhwaMode;
    DitheringMode ditheringMode;
    bool standbyWhenIdle;

    // by the name of the source, the ones that aren't in it are enabled (see SourceRegistry)
    QMap<QString, bool> enabledMangaSources;

private:
    QTimer timer;
};

QDataStream &operator<<(QDataStream &str, const Settings &m);

QDataStream &operator>>(QDataStream &str, Settings &m);
