#include "sourceregistry.h"

#include <QDir>

#include "luasource.h"
#include "sourceindex.h"

SourceRegistry::SourceRegistry(Settings *settings, QList<QSharedPointer<MangaSourceBase>> sources,
                               QObject *parent)
    : QObject(parent), settings(settings), sources(std::move(sources)), currentName()
{
}

QList<QSharedPointer<MangaSourceBase>> SourceRegistry::loadDirectory(NetworkManager *networkManager,
                                                                     const QString &directory, QStringList *errors)
{
    QList<QSharedPointer<MangaSourceBase>> sources;
    if (directory.isEmpty())
        return sources;
    auto report = [errors](const QString &message)
    {
        qWarning().noquote() << message;
        if (errors)
            errors->append(message);
    };

    auto index = SourceIndex::read(directory);
    if (!index)
    {
        report(index.error().message);
        return sources;
    }

    QStringList libraries;
    for (const auto &library : std::as_const(index->libraries))
    {
        // not an error: it waits for its update to the version of the app
        if (!library.isCompatible())
            qWarning().noquote() << QString("The library %1 is version %2, this app runs %3.x.")
                                        .arg(library.id, library.version.toString())
                                        .arg(LuaSource::majorVersion);
        else
            libraries.append(library.files);
    }

    for (const auto &entry : std::as_const(index->sources))
    {
        if (!entry.isCompatible())
        {
            qWarning().noquote() << QString("The source %1 is version %2, this app runs %3.x.")
                                        .arg(entry.name, entry.version.toString())
                                        .arg(LuaSource::majorVersion);
            continue;
        }
        auto loaded = LuaSource::load(networkManager, directory, entry.id, libraries);
        if (!loaded)
        {
            report(QString("Couldn't load the source %1: %2").arg(entry.name, loaded.error().message));
            continue;
        }
        sources.append(*loaded);
    }
    return sources;
}

void SourceRegistry::setSources(QList<QSharedPointer<MangaSourceBase>> newSources)
{
    sources = std::move(newSources);
    emit sourcesChanged();
    if (!currentName.isEmpty() && !source(currentName))
        setCurrent({});
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
