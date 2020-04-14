#include "settings.h"

Settings::Settings()
    : lightValue(0),
      comflightValue(0),
      hideErrorMessages(false),
      reverseSwipeDirection(false),
      reverseButtonDirection(false),
      doublePageFullscreen(false),
      timer()
{
    QObject::connect(&timer, &QTimer::timeout, [this]() { this->serialize(); });
}

void Settings::deserialize()
{
    QFile file(QString(CONF.cacheDir) + "/settings.dat");
    if (!file.open(QIODevice::ReadOnly))
        return;

    QDataStream in(&file);
    in >> *this;
    file.close();
}

void Settings::scheduleSerialize() { timer.start(1000); }

void Settings::serialize()
{
    timer.stop();
    QFile file(QString(CONF.cacheDir) + "/settings.dat");
    if (!file.open(QIODevice::WriteOnly))
        return;

    QDataStream out(&file);
    out << *this;
    file.close();
}

QDataStream &operator<<(QDataStream &str, const Settings &m)
{
    str << (qint32)m.lightValue << (qint32)m.comflightValue
        << m.hideErrorMessages << m.reverseSwipeDirection
        << m.reverseButtonDirection << m.doublePageFullscreen
        << m.enabledMangaSources;

    return str;
}

QDataStream &operator>>(QDataStream &str, Settings &m)
{
    m.enabledMangaSources.clear();
    str >> m.lightValue >> m.comflightValue >> m.hideErrorMessages >>
        m.reverseSwipeDirection >> m.reverseButtonDirection >>
        m.doublePageFullscreen >> m.enabledMangaSources;

    return str;
}
