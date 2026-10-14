// The check of the index of a directory of sources (index.yaml, see sources/README.md and SourceIndex).
#include <QDir>

#include "harness.h"
#include "luasource.h"
#include "sourceindex.h"

int checkIndex(NetworkManager *networkManager, const QString &directory)
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
            if (entry.api > LuaSource::apiLevel)
                fail(QString("%1 needs the api %2, the app has %3.").arg(entry.id).arg(entry.api).arg(LuaSource::apiLevel));
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
            out << entry.id << ": " << entry.name << " version " << entry.version << "\n";
    }

    if (failures == 0)
        out << "The index is fine.\n";
    return failures;
}
