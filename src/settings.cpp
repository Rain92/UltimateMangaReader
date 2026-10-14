#include "settings.h"

#include <QSaveFile>

#include "cachefile.h"
#include "staticsettings.h"

Settings::Settings() : timer()
{
    resetToDefaults();
    QObject::connect(&timer, &QTimer::timeout, &timer, [this]() { this->serialize(); });
}

void Settings::resetToDefaults()
{
    lightValue = 0;
    comflightValue = 0;
    hideErrorMessages = false;
    tabAdvance = Right;
    swipeAdvance = Left;
    buttonAdvance = Down;
    mangaOrder = OrderByPopularity;
    doublePageMode = DoublePage90CW;
    trimPages = true;
    manhwaMode = true;
    ditheringMode = SWDithering;
    standbyWhenIdle = false;
    enabledMangaSources.clear();
    sourcesRepository.clear();
}

QString Settings::sourcesRepositoryUrl() const
{
    return sourcesRepository.isEmpty() ? CONF.defaultSourcesRepository : sourcesRepository;
}

Settings::~Settings()
{
    if (timer.isActive())
        serialize();
}

void Settings::deserialize()
{
    QFile file(QString(CONF.cacheDir) + "/settings.dat");
    if (!file.open(QIODevice::ReadOnly))
        return;

    QDataStream in(&file);
    if (readCacheFileHeader(in, CacheFile::Settings))
        in >> *this;

    if (in.status() != QDataStream::Ok)
    {
        qDebug() << "Couldn't read the settings, using the defaults";
        resetToDefaults();
    }
}

void Settings::scheduleSerialize()
{
    timer.start(1000);
}

void Settings::serialize()
{
    timer.stop();
    QSaveFile file(QString(CONF.cacheDir) + "/settings.dat");
    if (!file.open(QIODevice::WriteOnly))
        return;

    QDataStream out(&file);
    writeCacheFileHeader(out, CacheFile::Settings);
    out << *this;
    if (out.status() != QDataStream::Ok || !file.commit())
        qDebug() << "Couldn't save the settings";
}

QDataStream &operator<<(QDataStream &str, const Settings &m)
{
    str << m.lightValue << m.comflightValue << m.hideErrorMessages << m.tabAdvance << m.swipeAdvance
        << m.buttonAdvance << m.mangaOrder << m.doublePageMode << m.trimPages << m.manhwaMode
        << m.enabledMangaSources << m.ditheringMode << m.standbyWhenIdle << m.sourcesRepository;

    return str;
}

QDataStream &operator>>(QDataStream &str, Settings &m)
{
    m.enabledMangaSources.clear();
    str >> m.lightValue >> m.comflightValue >> m.hideErrorMessages >> m.tabAdvance >> m.swipeAdvance >>
        m.buttonAdvance >> m.mangaOrder >> m.doublePageMode >> m.trimPages >> m.manhwaMode >>
        m.enabledMangaSources >> m.ditheringMode >> m.standbyWhenIdle;
    // added later, the settings of before keep the default
    m.sourcesRepository.clear();
    if (!str.atEnd())
        str >> m.sourcesRepository;

    return str;
}
