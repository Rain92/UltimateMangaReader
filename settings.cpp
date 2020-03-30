#include "settings.h"

Settings::Settings() : timer()
{
    timer.setSingleShot(true);
    QObject::connect(&timer, SIGNAL(timeout()), this, SLOT(serialize()));

    lightvalue = 30;
    comflightvalue = 6400;
}

void Settings::deserialize()
{
    QFile file(QString(CONF.cachedir) + "/settings.dat");
    if (!file.open(QIODevice::ReadOnly))
        return;

    QDataStream in(&file);
    in >> *this;
    file.close();
}

void Settings::scheduleSerialize() { timer.start(500); }

void Settings::serialize()
{
    QFile file(QString(CONF.cachedir) + "/settings.dat");
    if (!file.open(QIODevice::WriteOnly))
        return;

    QDataStream out(&file);
    out << *this;
    file.close();
}

QDataStream &operator<<(QDataStream &str, const Settings &m)
{
    str << (qint32)m.lightvalue << (qint32)m.comflightvalue;

    return str;
}

QDataStream &operator>>(QDataStream &str, Settings &m)
{
    str >> m.lightvalue >> m.comflightvalue;

    return str;
}
