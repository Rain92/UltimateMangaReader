#pragma once

#include <QObject>

#include "mangasourcebase.h"
#include "networkmanager.h"
#include "sourceindex.h"
#include "task.h"

// The installed sources (a directory of sources, see SourceIndex) and their updates from the repository.
//
// The installed directory gets the bundled sources of the app (at the first start, and the newer ones after an
// update of the app) and the updates from a repository: the index there is compared with the installed one, the
// files of the entries with a newer version are downloaded (with verified TLS). An update is installed as a whole,
// and only if all of its sources load.
class SourceUpdater : public QObject
{
    Q_OBJECT

public:
    // What the repository has that the installed sources don't.
    struct Changes
    {
        // the repository they are from
        QString repositoryUrl;
        // the index to install
        SourceIndex index;
        // the names of the sources
        QStringList added, updated, removed;
        // the ids of the entries whose files are downloaded (also libraries)
        QStringList downloads;
        int removedLibraries = 0;

        bool isEmpty() const { return downloads.isEmpty() && removed.isEmpty() && removedLibraries == 0; }
        // e.g. "MangaDex updated, MangaTown removed"
        QString summary() const;
    };

    SourceUpdater(NetworkManager *networkManager, const QString &bundledDir, const QString &installedDir,
                  QObject *parent = nullptr);

    // Installs the bundled sources that are newer than the installed ones (all of them the first time, the ones of
    // an update of the app later). Returns the directory to load the sources from.
    QString installBundled();

    // Compares the index of the repository with the installed one. repositoryUrl: the directory of its index.yaml.
    Task<Result<Changes>> check(const QString &repositoryUrl);
    // Downloads the files of the changed entries and installs the index with them. Returns the loaded sources.
    Task<Result<QList<QSharedPointer<MangaSourceBase>>>> install(Changes changes);

    QString installedDirectory() const { return installedDir; }

private:
    NetworkManager *networkManager;
    QString bundledDir;
    QString installedDir;

    // Writes index with its files into the installed directory, through a new directory that replaces it.
    // entryDirs: the directory of the files of an entry (by id), the installed directory for the others.
    // extraFiles: name → content (e.g. the bundled index it came from).
    Result<> writeInstalled(const SourceIndex &index, const QHash<QString, QString> &entryDirs,
                            const QMap<QString, QByteArray> &extraFiles);
};
