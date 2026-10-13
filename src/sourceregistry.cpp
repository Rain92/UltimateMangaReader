#include "sourceregistry.h"

#include <QDir>

#include "comick.h"
#include "luasource.h"
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

QList<QSharedPointer<MangaSourceBase>> SourceRegistry::withScriptSources(QList<QSharedPointer<MangaSourceBase>> sources,
                                                                         NetworkManager *networkManager,
                                                                         const QString &directory, QStringList *errors)
{
    if (directory.isEmpty())
        return sources;

    for (const auto &entry : QDir(directory).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name))
    {
        auto loaded = LuaSource::load(networkManager, entry.absoluteFilePath());
        if (!loaded)
        {
            auto message = QString("Couldn't load the source in %1: %2")
                               .arg(entry.absoluteFilePath(), loaded.error().message);
            qWarning().noquote() << message;
            if (errors)
                errors->append(message);
            continue;
        }

        auto script = *loaded;
        auto same = std::find_if(sources.begin(), sources.end(),
                                 [&script](const auto &source) { return source->name == script->name; });
        if (same != sources.end())
            *same = script;
        else
            sources.append(script);
    }
    return sources;
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
