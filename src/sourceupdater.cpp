#include "sourceupdater.h"

#include <QCryptographicHash>
#include <QDir>
#include <QSaveFile>

#include "luasource.h"
#include "sourceregistry.h"
#include "utils.h"

// the bundled index that the installed sources got their bundled ones from
static const char *bundledIndexFile = "bundled.yaml";

using Entry = SourceIndex::Entry;

static const Entry *find(const QList<Entry> &entries, const QString &id)
{
    for (const auto &entry : entries)
        if (entry.id == id)
            return &entry;
    return nullptr;
}

static QByteArray readFile(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

static bool writeFile(const QString &path, const QByteArray &content)
{
    QSaveFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(content) == content.size() && file.commit();
}

QString SourceUpdater::Changes::summary() const
{
    QStringList parts;
    for (const auto &name : updated)
        parts.append(name + " updated");
    for (const auto &name : added)
        parts.append(name + " new");
    for (const auto &name : removed)
        parts.append(name + " removed");
    if (parts.isEmpty())
        parts.append("The shared scripts updated");
    return parts.join(", ");
}

SourceUpdater::SourceUpdater(NetworkManager *networkManager, const QString &bundledDir, const QString &installedDir,
                             const QString &repositoryUrl, QObject *parent)
    : QObject(parent),
      networkManager(networkManager),
      bundledDir(QDir::cleanPath(bundledDir)),
      installedDir(QDir::cleanPath(installedDir)),
      repositoryUrl(repositoryUrl)
{
}

QString SourceUpdater::installBundled()
{
    // an install that was interrupted between replacing the directories
    if (!QDir(installedDir).exists() && QDir(installedDir + ".old").exists())
        QDir().rename(installedDir + ".old", installedDir);

    // e.g. the tests
    if (!QFile::exists(bundledDir + "/index.yaml"))
        return installedDir;
    auto bundledYaml = readFile(bundledDir + "/index.yaml");
    auto bundled = SourceIndex::parse(bundledYaml, bundledDir + "/index.yaml");
    auto installed = SourceIndex::read(installedDir);
    if (!bundled)
    {
        qWarning().noquote() << bundled.error().message;
        return installedDir;
    }
    auto previousYaml = readFile(installedDir + "/" + bundledIndexFile);
    if (installed && previousYaml == bundledYaml)
        return installedDir;

    // the entries of the bundled index that are newer, and the ones that are new in it (not the ones the
    // repository removed)
    auto previous = SourceIndex::parse(previousYaml, bundledIndexFile).value_or(SourceIndex());
    SourceIndex merged = installed.value_or(SourceIndex());
    bool changed = !installed;
    auto merge = [&](QList<Entry> &into, const QList<Entry> &from, const QList<Entry> &previousEntries)
    {
        for (const auto &entry : from)
        {
            if (entry.api > LuaSource::apiLevel)
                continue;
            auto it = std::find_if(into.begin(), into.end(), [&entry](const Entry &e) { return e.id == entry.id; });
            if (it != into.end())
            {
                if (entry.version > it->version)
                {
                    *it = entry;
                    changed = true;
                }
            }
            else if (!installed || !find(previousEntries, entry.id))
            {
                into.append(entry);
                changed = true;
            }
        }
    };
    merge(merged.libraries, bundled->libraries, previous.libraries);
    merge(merged.sources, bundled->sources, previous.sources);

    if (!changed)
    {
        writeFile(installedDir + "/" + bundledIndexFile, bundledYaml);
        return installedDir;
    }
    auto written = writeInstalled(merged, {bundledDir, installedDir}, {{bundledIndexFile, bundledYaml}});
    if (!written)
    {
        qWarning().noquote() << "Couldn't install the bundled sources:" << written.error().message;
        // the app has its sources anyway
        return installed ? installedDir : bundledDir;
    }
    qDebug() << "Installed the bundled sources";
    return installedDir;
}

Result<> SourceUpdater::writeInstalled(const SourceIndex &index, const QStringList &fromDirs,
                                       const QMap<QString, QByteArray> &extraFiles)
{
    auto staging = installedDir + ".new";
    removeDir(staging);
    if (!QDir().mkpath(staging))
        return Err(Error::Storage, QString("Couldn't create %1.").arg(staging));

    for (const auto &entries : {index.libraries, index.sources})
        for (const auto &entry : entries)
            for (auto file = entry.files.cbegin(); file != entry.files.cend(); ++file)
            {
                bool copied = false;
                for (const auto &dir : fromDirs)
                {
                    auto path = dir + "/" + file.key();
                    if (SourceIndex::fileHash(path) == file.value())
                    {
                        copied = QFile::copy(path, staging + "/" + file.key());
                        break;
                    }
                }
                if (!copied)
                {
                    removeDir(staging);
                    return Err(Error::Storage, QString("The file %1 of %2 isn't there.").arg(file.key(), entry.id));
                }
            }

    auto extra = extraFiles;
    if (!extra.contains(bundledIndexFile))
        extra.insert(bundledIndexFile, readFile(installedDir + "/" + bundledIndexFile));
    extra.insert("index.yaml", index.toYaml());
    for (auto it = extra.cbegin(); it != extra.cend(); ++it)
        if (!writeFile(staging + "/" + it.key(), it.value()))
        {
            removeDir(staging);
            return Err(Error::Storage, QString("Couldn't write %1.").arg(it.key()));
        }

    // all of its sources have to load
    QStringList errors;
    SourceRegistry::loadDirectory(networkManager, staging, &errors);
    if (!errors.isEmpty())
    {
        removeDir(staging);
        return Err(Error::Failed, errors.first());
    }

    auto old = installedDir + ".old";
    removeDir(old);
    if (QDir(installedDir).exists() && !QDir().rename(installedDir, old))
    {
        removeDir(staging);
        return Err(Error::Storage, QString("Couldn't replace %1.").arg(installedDir));
    }
    if (!QDir().rename(staging, installedDir))
    {
        QDir().rename(old, installedDir);
        removeDir(staging);
        return Err(Error::Storage, QString("Couldn't replace %1.").arg(installedDir));
    }
    removeDir(old);
    return {};
}

Task<Result<SourceUpdater::Changes>> SourceUpdater::check()
{
    DownloadRequest request;
    request.url = repositoryUrl + "index.yaml";
    request.timeout = 20000;
    request.verifyTls = true;
    auto job = networkManager->download(request);
    if (!co_await job)
        co_return Err(job->error);
    auto remote = SourceIndex::parse(job->buffer, request.url);
    if (!remote)
        co_return Err(remote.error());
    auto installed = SourceIndex::read(installedDir).value_or(SourceIndex());

    // the entries of the repository (or the installed ones if they are newer, or the app is too old for the
    // repository's), without the installed ones the repository doesn't have
    Changes changes;
    auto compare = [&changes](const QList<Entry> &remoteEntries, const QList<Entry> &installedEntries,
                              QList<Entry> &into, bool isSource)
    {
        for (const auto &entry : remoteEntries)
        {
            auto current = find(installedEntries, entry.id);
            if (entry.api > LuaSource::apiLevel || (current && current->version >= entry.version))
            {
                if (current)
                    into.append(*current);
                continue;
            }
            into.append(entry);
            changes.changedEntries++;
            if (isSource)
                (current ? changes.updated : changes.added).append(entry.name);
        }
        for (const auto &entry : installedEntries)
            if (!find(remoteEntries, entry.id))
            {
                if (isSource)
                    changes.removed.append(entry.name);
                else
                    changes.changedEntries++;
            }
    };
    compare(remote->libraries, installed.libraries, changes.index.libraries, false);
    compare(remote->sources, installed.sources, changes.index.sources, true);
    co_return changes;
}

Task<Result<QList<QSharedPointer<MangaSourceBase>>>> SourceUpdater::install(Changes changes)
{
    auto downloads = installedDir + ".download";
    removeDir(downloads);
    QDir().mkpath(downloads);

    // the files that aren't installed, all at once
    QList<std::pair<QString, QString>> files;
    QList<QSharedPointer<DownloadBufferJob>> jobs;
    for (const auto &entries : {changes.index.libraries, changes.index.sources})
        for (const auto &entry : entries)
            for (auto file = entry.files.cbegin(); file != entry.files.cend(); ++file)
                if (SourceIndex::fileHash(installedDir + "/" + file.key()) != file.value())
                {
                    DownloadRequest request;
                    request.url = repositoryUrl + file.key();
                    request.timeout = 30000;
                    request.verifyTls = true;
                    files.append({file.key(), file.value()});
                    jobs.append(networkManager->download(request));
                }

    Result<> result;
    for (int i = 0; i < jobs.size() && result; i++)
    {
        auto job = jobs[i];
        const auto &[name, hash] = files[i];
        if (!co_await job)
            result = Err(job->error.code, QString("Couldn't download %1: %2").arg(name, job->error.message));
        else if (QCryptographicHash::hash(job->buffer, QCryptographicHash::Sha256).toHex() != hash.toLatin1())
            result = Err(Error::Parse, QString("%1 isn't the file of the index.").arg(name));
        else if (!writeFile(downloads + "/" + name, job->buffer))
            result = Err(Error::Storage, QString("Couldn't write %1.").arg(name));
    }
    if (result)
        result = writeInstalled(changes.index, {downloads, installedDir}, {});
    removeDir(downloads);
    if (!result)
        co_return Err(result.error());

    QStringList errors;
    auto sources = SourceRegistry::loadDirectory(networkManager, installedDir, &errors);
    co_return sources;
}
