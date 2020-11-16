#ifndef SETTINGS_H
#define SETTINGS_H

#include <QDataStream>
#include <QFile>
#include <QMap>
#include <QTimer>

#include "enums.h"

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

    MangaOrderMethod mangaOrder;

    bool doublePageFullscreen;
    bool trimPages;
    bool manhwaMode;

    QMap<QString, bool> enabledMangaSources;

private:
    QTimer timer;
};

QDataStream &operator<<(QDataStream &str, const Settings &m);

QDataStream &operator>>(QDataStream &str, Settings &m);

#endif  // SETTINGS_H
