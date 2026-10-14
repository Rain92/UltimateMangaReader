// The index of a directory of sources (index.yaml, see sources/README.md and SourceIndex).
#include <QDir>
#include <QSaveFile>

#include "harness.h"
#include "luasource.h"
#include "sourceindex.h"

// The entry with the files, its version one up if they changed (and the api of this app then).
static SourceIndex::Entry updatedEntry(const SourceIndex::Entry *old, const QString &id, const QString &name,
                                       const QMap<QString, QString> &files)
{
    SourceIndex::Entry entry;
    entry.id = id;
    entry.name = name;
    entry.files = files;
    if (old && old->files == files)
    {
        entry.version = old->version;
        entry.api = old->api;
    }
    else
    {
        entry.version = old ? old->version + 1 : 1;
        entry.api = LuaSource::apiLevel;
    }
    return entry;
}

// In the order of the old index, the new ones after them.
static QStringList ordered(QStringList ids, const QList<SourceIndex::Entry> &oldEntries)
{
    QStringList order;
    for (const auto &entry : oldEntries)
        if (ids.removeOne(entry.id))
            order.append(entry.id);
    ids.sort();
    return order + ids;
}

int writeIndex(NetworkManager *networkManager, const QString &directory, bool check)
{
    QDir dir(directory);
    auto oldIndex = dir.exists("index.yaml") ? SourceIndex::read(directory) : SourceIndex();
    if (!oldIndex)
    {
        out << oldIndex.error().message << "\n";
        return 1;
    }

    // <id>.yaml: a source, the other .lua files: libraries
    QStringList sourceIds, libraryIds;
    for (const auto &file : dir.entryInfoList({"*.yaml"}, QDir::Files))
        if (file.fileName() != "index.yaml")
            sourceIds.append(file.completeBaseName());
    for (const auto &file : dir.entryInfoList({"*.lua"}, QDir::Files))
        if (!sourceIds.contains(file.completeBaseName()))
            libraryIds.append(file.completeBaseName());

    SourceIndex index;
    QStringList libraryFiles;
    for (const auto &id : ordered(libraryIds, oldIndex->libraries))
    {
        auto file = id + ".lua";
        libraryFiles.append(file);
        index.libraries.append(
            updatedEntry(oldIndex->library(id), id, {}, {{file, SourceIndex::fileHash(dir.filePath(file))}}));
    }

    int failures = 0;
    for (const auto &id : ordered(sourceIds, oldIndex->sources))
    {
        auto loaded = LuaSource::load(networkManager, directory, id, libraryFiles);
        if (!loaded)
        {
            out << "Couldn't load " << id << ": " << loaded.error().message << "\n";
            failures++;
            continue;
        }
        QMap<QString, QString> files;
        for (const auto &suffix : {".yaml", ".lua", ".png"})
            if (dir.exists(id + suffix))
                files.insert(id + suffix, SourceIndex::fileHash(dir.filePath(id + suffix)));
        index.sources.append(updatedEntry(oldIndex->source(id), id, (*loaded)->name, files));
        out << id << ": " << (*loaded)->name << " version " << index.sources.last().version << "\n";
    }
    if (failures > 0)
        return failures;

    auto yaml = index.toYaml();
    QFile current(dir.filePath("index.yaml"));
    bool upToDate = current.open(QIODevice::ReadOnly) && current.readAll() == yaml;
    if (check)
    {
        out << (upToDate ? "The index is up to date.\n" : "The index is out of date: run harness index.\n");
        return upToDate ? 0 : 1;
    }
    if (!upToDate)
    {
        QSaveFile file(dir.filePath("index.yaml"));
        if (!file.open(QIODevice::WriteOnly) || file.write(yaml) != yaml.size() || !file.commit())
        {
            out << "Couldn't write " << file.fileName() << "\n";
            return 1;
        }
        out << "Wrote " << file.fileName() << "\n";
    }
    return 0;
}
