#include "sourceindex.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QRegularExpression>

#include "yamlreader.h"

static Result<QList<SourceIndex::Entry>> readEntries(const QVariant &value, const QString &path, bool isSource)
{
    QList<SourceIndex::Entry> entries;
    // missing or empty (null)
    if (!value.isValid() || (value.typeId() == QMetaType::QString && value.toString().isEmpty()))
        return entries;
    if (value.typeId() != QMetaType::QVariantList)
        return Err(Error::Parse, path + " is no list.");

    static const QRegularExpression idrx(R"(^[a-z0-9_-]+$)");
    static const QRegularExpression filerx(R"(^[A-Za-z0-9_.-]+$)");
    for (const auto &item : value.toList())
    {
        auto map = item.toMap();
        SourceIndex::Entry entry;
        entry.id = map.value("id").toString();
        entry.name = map.value("name").toString();
        entry.version = map.value("version").toInt();
        entry.api = map.value("api", 1).toInt();
        if (!idrx.match(entry.id).hasMatch())
            return Err(Error::Parse, QString("%1: invalid id \"%2\".").arg(path, entry.id));
        if (isSource && entry.name.isEmpty())
            return Err(Error::Parse, QString("%1: %2 has no name.").arg(path, entry.id));

        auto files = map.value("files").toMap();
        for (auto it = files.cbegin(); it != files.cend(); ++it)
        {
            // only files next to the index
            if (!filerx.match(it.key()).hasMatch() || it.key().startsWith('.'))
                return Err(Error::Parse, QString("%1: invalid file name \"%2\".").arg(path, it.key()));
            entry.files.insert(it.key(), it.value().toString());
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
    auto libraries = readEntries(map.value("libraries"), name + ": libraries", false);
    if (!libraries)
        return Err(libraries.error());
    auto sources = readEntries(map.value("sources"), name + ": sources", true);
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
    QString yaml = "# The sources and the libraries of their scripts, see README.md. Written by `harness index`: change\n"
                   "# the files, then run it.\n";
    auto writeEntries = [&yaml](const char *key, const QList<Entry> &entries, bool isSource)
    {
        yaml += QString(entries.isEmpty() ? "%1: []\n" : "%1:\n").arg(key);
        for (const auto &entry : entries)
        {
            yaml += QString("  - id: %1\n").arg(entry.id);
            if (isSource)
                yaml += QString("    name: %1\n").arg(quoted(entry.name));
            yaml += QString("    version: %1\n    api: %2\n    files:\n").arg(entry.version).arg(entry.api);
            for (auto it = entry.files.cbegin(); it != entry.files.cend(); ++it)
                yaml += QString("      %1: %2\n").arg(it.key(), it.value());
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

QString SourceIndex::fileHash(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(&file);
    return QString::fromLatin1(hash.result().toHex());
}
