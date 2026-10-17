// The check of the index of a directory of sources (index.yaml, see sources/README.md and SourceIndex).
#include <QDir>
#include <QFile>

#include "harness.h"
#include "luasource.h"
#include "sourceindex.h"

static QByteArray fileContent(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

// Whether the files of the entry differ from the ones of the entry of the base.
static bool changed(const SourceIndex::Entry &entry, const QString &directory, const SourceIndex::Entry &base,
                    const QString &baseDirectory)
{
    if (entry.files != base.files)
        return true;
    for (const auto &file : entry.files)
        if (!QFile::exists(baseDirectory + "/" + file) ||
            fileContent(directory + "/" + file) != fileContent(baseDirectory + "/" + file))
            return true;
    return false;
}

int checkIndex(NetworkManager *networkManager, const QString &directory, const QString &baseDirectory)
{
    auto index = SourceIndex::read(directory);
    if (!index)
    {
        out << index.error().message << "\n";
        return 1;
    }

    QDir dir(directory);
    int failures = 0;
    auto fail = [&failures](const QString &message)
    {
        out << message << "\n";
        failures++;
    };

    // the files of the index are there, and the index has all files
    QStringList listed{"index.yaml", "README.md"};
    QStringList libraries;
    for (const auto &entries : {index->libraries, index->sources})
        for (const auto &entry : entries)
        {
            if (!entry.isCompatible())
                fail(QString("%1 is version %2, the app runs %3.x.")
                         .arg(entry.id, entry.version.toString())
                         .arg(LuaSource::majorVersion));
            for (const auto &file : entry.files)
            {
                listed.append(file);
                if (!dir.exists(file))
                    fail(QString("%1 of %2 isn't there.").arg(file, entry.id));
            }
        }
    for (const auto &file : dir.entryList(QDir::Files))
        if (!listed.contains(file))
            fail(QString("%1 isn't in the index.").arg(file));

    // every source loads, with the name of the index
    for (const auto &library : index->libraries)
        libraries.append(library.files);
    for (const auto &entry : index->sources)
    {
        auto loaded = LuaSource::load(networkManager, directory, entry.id, libraries);
        if (!loaded)
            fail(QString("Couldn't load %1: %2").arg(entry.id, loaded.error().message));
        else if ((*loaded)->name != entry.name)
            fail(QString("%1 is %2 in the index, but %3 in %1.yaml.").arg(entry.id, entry.name, (*loaded)->name));
        else
            out << entry.id << ": " << entry.name << " version " << entry.version.toString() << "\n";
    }

    // The app installs an entry of the repository only when its version is newer than the installed one: an entry
    // that changed since the base (e.g. the sources that were published) needs a newer version than there.
    if (!baseDirectory.isEmpty())
    {
        auto base = SourceIndex::read(baseDirectory);
        if (!base)
            fail("The base: " + base.error().message);
        else
            for (bool isLibrary : {true, false})
                for (const auto &entry : isLibrary ? index->libraries : index->sources)
                {
                    auto old = isLibrary ? base->library(entry.id) : base->source(entry.id);
                    if (!old)
                        continue;
                    if (entry.version < old->version)
                        fail(QString("%1 is version %2, older than the %3 of the base.")
                                 .arg(entry.id, entry.version.toString(), old->version.toString()));
                    else if (entry.version == old->version && changed(entry, directory, *old, baseDirectory))
                        fail(QString("%1 changed, but its version is still %2: count up its minor version.")
                                 .arg(entry.id, entry.version.toString()));
                }
        out << "The versions were checked against " << baseDirectory << ".\n";
    }

    if (failures == 0)
        out << "The index is fine.\n";
    return failures;
}
