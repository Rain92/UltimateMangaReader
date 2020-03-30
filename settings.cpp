#include "settings.h"

Settings::Settings()
    : lightValue(0), comflightValue(0), hideErrorMessages(false), timer()
{
    timer.setSingleShot(true);
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

void Settings::scheduleSerialize() { timer.start(500); }

void Settings::serialize()
{
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
        << m.hideErrorMessages;

    return str;
}

QDataStream &operator>>(QDataStream &str, Settings &m)
{
    str >> m.lightValue >> m.comflightValue >> m.hideErrorMessages;

    return str;
}
