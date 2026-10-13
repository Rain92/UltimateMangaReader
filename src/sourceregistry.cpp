#include "sourceregistry.h"

#include "comick.h"
#include "mangadex.h"
#include "mangahere.h"
#include "mangakakalot.h"
#include "mangaplus.h"
#include "mangatown.h"

SourceRegistry::SourceRegistry(Settings *settings, QList<QSharedPointer<MangaSourceBase>> sources,
                               QObject *parent)
    : QObject(parent), settings(settings), sources(std::move(sources)), currentName()
{
}

QList<QSharedPointer<MangaSourceBase>> SourceRegistry::builtInSources(NetworkManager *networkManager)
{
    return {QSharedPointer<MangaSourceBase>(new MangaDex(networkManager)),
            QSharedPointer<MangaSourceBase>(new Mangakakalot(networkManager)),
            QSharedPointer<MangaSourceBase>(new MangaTown(networkManager)),
            QSharedPointer<MangaSourceBase>(new MangaHere(networkManager)),
            QSharedPointer<MangaSourceBase>(new Comick(networkManager)),
            QSharedPointer<MangaSourceBase>(new MangaPlus(networkManager))};
}

QStringList SourceRegistry::names() const
{
    QStringList names;
    for (const auto &source : sources)
        names.append(source->name);
    return names;
}

QSharedPointer<MangaSourceBase> SourceRegistry::source(const QString &name) const
{
    for (const auto &source : sources)
        if (source->name == name)
            return source;
    return {};
}

bool SourceRegistry::isEnabled(const QString &name) const
{
    return settings->enabledMangaSources.value(name, true) && source(name);
}

QStringList SourceRegistry::enabledNames() const
{
    QStringList names;
    for (const auto &source : sources)
        if (settings->enabledMangaSources.value(source->name, true))
            names.append(source->name);
    return names;
}

void SourceRegistry::setEnabled(const QString &name, bool enabled)
{
    if (isEnabled(name) == enabled)
        return;

    settings->enabledMangaSources[name] = enabled;
    settings->scheduleSerialize();
    emit sourcesChanged();

    // the list of a source that was disabled isn't shown anymore
    if (!enabled && currentName == name)
        setCurrent({});
}

void SourceRegistry::setCurrent(const QString &name)
{
    currentName = name;
    emit currentChanged(name);
}
