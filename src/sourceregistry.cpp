#include "sourceregistry.h"

#include "comick.h"
#include "mangadex.h"
#include "mangahere.h"
#include "mangakakalot.h"
#include "mangaplus.h"
#include "mangatown.h"

SourceRegistry::SourceRegistry(NetworkManager *networkManager, QObject *parent)
    : QObject(parent), sources(), activeSources(), currentSource(nullptr)
{
    sources.append(QSharedPointer<MangaSourceBase>(new MangaDex(networkManager)));
    sources.append(QSharedPointer<MangaSourceBase>(new Mangakakalot(networkManager)));
    sources.append(QSharedPointer<MangaSourceBase>(new MangaTown(networkManager)));
    sources.append(QSharedPointer<MangaSourceBase>(new MangaHere(networkManager)));
    sources.append(QSharedPointer<MangaSourceBase>(new Comick(networkManager)));
    sources.append(QSharedPointer<MangaSourceBase>(new MangaPlus(networkManager)));
}

MangaSourceBase *SourceRegistry::source(const QString &name) const
{
    for (const auto &source : sources)
        if (source->name == name)
            return source.get();
    return nullptr;
}

void SourceRegistry::updateActive(Settings &settings)
{
    activeSources.clear();
    QMap<QString, bool> enabledMangaSources;
    for (const auto &ms : std::as_const(sources))
    {
        bool enabled = settings.enabledMangaSources.value(ms->name, true);
        enabledMangaSources.insert(ms->name, enabled);
        if (enabled)
            activeSources.insert(ms->name, ms.get());
    }
    settings.enabledMangaSources = enabledMangaSources;

    emit activeSourcesChanged(activeSources.values());

    // the list of a source that was disabled isn't shown anymore
    if (currentSource && !activeSources.values().contains(currentSource))
        setCurrent(nullptr);
}

void SourceRegistry::setCurrent(MangaSourceBase *source)
{
    currentSource = source;
    emit currentSourceChanged(source);
}
