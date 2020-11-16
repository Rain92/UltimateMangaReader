#include "settings.h"

#include "staticsettings.h"

Settings::Settings()
    : lightValue(0),
      comflightValue(0),
      hideErrorMessages(false),
      tabAdvance(Right),
      swipeAdvance(Left),
      buttonAdvance(Down),
      mangaOrder(OrderByPopularity),
      doublePageFullscreen(true),
      trimPages(true),
      manhwaMode(true),
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

void Settings::scheduleSerialize()
{
    timer.start(1000);
}

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
    str << m.lightValue << m.comflightValue << m.hideErrorMessages << m.tabAdvance << m.swipeAdvance
        << m.buttonAdvance << m.mangaOrder << m.doublePageFullscreen << m.trimPages << m.manhwaMode
        << m.enabledMangaSources;

    return str;
}

QDataStream &operator>>(QDataStream &str, Settings &m)
{
    m.enabledMangaSources.clear();
    str >> m.lightValue >> m.comflightValue >> m.hideErrorMessages >> m.tabAdvance >> m.swipeAdvance >>
        m.buttonAdvance >> m.mangaOrder >> m.doublePageFullscreen >> m.trimPages >> m.manhwaMode >>
        m.enabledMangaSources;

    return str;
}
