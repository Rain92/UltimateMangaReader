#include "sourceindex.h"

#include <QDir>
#include <QFile>
#include <QRegularExpression>

#include "luasource.h"
#include "yamlreader.h"

QString SourceIndex::Version::toString() const
{
    return QString("%1.%2").arg(major).arg(minor);
}

std::optional<SourceIndex::Version> SourceIndex::Version::parse(const QString &text)
{
    static const QRegularExpression rx(R"(^(\d{1,4})\.(\d{1,6})$)");
    auto match = rx.match(text);
    if (!match.hasMatch())
        return std::nullopt;
    return Version{match.captured(1).toInt(), match.captured(2).toInt()};
}

bool SourceIndex::Entry::isCompatible() const
{
    return version.major == LuaSource::majorVersion;
}

// ids: the ones of the index so far (they are unique, the files of the entries are named after them)
using Version = SourceIndex::Version;

static Result<QList<SourceIndex::Entry>> readEntries(const QVariant &value, const QString &path, bool isSource,
                                                     QStringList &ids)
{
    QList<SourceIndex::Entry> entries;
    // missing or empty (null)
    if (!value.isValid() || (value.typeId() == QMetaType::QString && value.toString().isEmpty()))
        return entries;
    if (value.typeId() != QMetaType::QVariantList)
        return Err(Error::Parse, path + " is no list.");

    static const QRegularExpression idrx(R"(^[a-z0-9_-]+$)");
    for (const auto &item : value.toList())
    {
        auto map = item.toMap();
        SourceIndex::Entry entry;
        entry.id = map.value("id").toString();
        entry.name = map.value("name").toString();
        if (!idrx.match(entry.id).hasMatch())
            return Err(Error::Parse, QString("%1: invalid id \"%2\".").arg(path, entry.id));
        if (ids.contains(entry.id))
            return Err(Error::Parse, QString("%1: %2 is there twice.").arg(path, entry.id));
        ids.append(entry.id);
        auto version = Version::parse(map.value("version").toString());
        if (!version)
            return Err(Error::Parse, QString("%1: %2 has no version (major.minor).").arg(path, entry.id));
        entry.version = *version;
        if (isSource && entry.name.isEmpty())
            return Err(Error::Parse, QString("%1: %2 has no name.").arg(path, entry.id));

        if (!isSource)
        {
            entry.files = {entry.id + ".lua"};
        }
        else
        {
            // only its own files: the definition, a script, a logo
            for (const auto &file : map.value("files").toList())
            {
                auto name = file.toString();
                if (name != entry.id + ".yaml" && name != entry.id + ".lua" && name != entry.id + ".png")
                    return Err(Error::Parse, QString("%1: %2 can't have the file \"%3\".").arg(path, entry.id, name));
                entry.files.append(name);
            }
            if (!entry.files.contains(entry.id + ".yaml"))
                return Err(Error::Parse, QString("%1: %2 has no %2.yaml.").arg(path, entry.id));
        }
        entries.append(entry);
    }
    return entries;
}

Result<SourceIndex> SourceIndex::parse(const QByteArray &yaml, const QString &name)
{
    auto document = readYaml(yaml, name);
    if (!document)
        return Err(document.error());
    auto map = document->toMap();

    SourceIndex index;
    QStringList ids;
    auto libraries = readEntries(map.value("libraries"), name + ": libraries", false, ids);
    if (!libraries)
        return Err(libraries.error());
    auto sources = readEntries(map.value("sources"), name + ": sources", true, ids);
    if (!sources)
        return Err(sources.error());
    index.libraries = *libraries;
    index.sources = *sources;
    return index;
}

Result<SourceIndex> SourceIndex::read(const QString &directory)
{
    QFile file(QDir(directory).filePath("index.yaml"));
    if (!file.open(QIODevice::ReadOnly))
        return Err(Error::Storage, QString("Couldn't read %1.").arg(file.fileName()));
    return parse(file.readAll(), file.fileName());
}

// single quoted, the YAML way
static QString quoted(const QString &s)
{
    return "'" + QString(s).replace("'", "''") + "'";
}

QByteArray SourceIndex::toYaml() const
{
    QString yaml;
    auto writeEntries = [&yaml](const char *key, const QList<Entry> &entries, bool isSource)
    {
        yaml += QString(entries.isEmpty() ? "%1: []\n" : "%1:\n").arg(key);
        for (const auto &entry : entries)
        {
            yaml += QString("  - id: %1\n").arg(entry.id);
            if (isSource)
                yaml += QString("    name: %1\n").arg(quoted(entry.name));
            yaml += QString("    version: %1\n").arg(entry.version.toString());
            if (isSource)
                yaml += QString("    files: [%1]\n").arg(entry.files.join(", "));
        }
    };
    writeEntries("libraries", libraries, false);
    writeEntries("sources", sources, true);
    return yaml.toUtf8();
}

const SourceIndex::Entry *SourceIndex::source(const QString &id) const
{
    for (const auto &entry : sources)
        if (entry.id == id)
            return &entry;
    return nullptr;
}

const SourceIndex::Entry *SourceIndex::library(const QString &id) const
{
    for (const auto &entry : libraries)
        if (entry.id == id)
            return &entry;
    return nullptr;
}
