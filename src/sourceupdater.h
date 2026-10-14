#pragma once

#include <QObject>

#include "mangasourcebase.h"
#include "networkmanager.h"
#include "sourceindex.h"
#include "task.h"

// The installed sources (a directory of sources, see SourceIndex) and their updates from the repository.
//
// The installed directory gets the bundled sources of the app (at the first start, and the newer ones after an
// update of the app) and the updates from the repository: the index there is compared with the installed one, the
// files of the changed entries are downloaded (with verified TLS, checked against the hashes of the index). An
// update is installed as a whole, and only if all of its sources load.
class SourceUpdater : public QObject
{
    Q_OBJECT

public:
    // What the repository has that the installed sources don't.
    struct Changes
    {
        // the index to install
        SourceIndex index;
        // the names of the sources
        QStringList added, updated, removed;
        // also the libraries
        int changedEntries = 0;

        bool isEmpty() const { return changedEntries == 0 && removed.isEmpty(); }
        // e.g. "MangaDex updated, MangaTown removed"
        QString summary() const;
    };

    // repositoryUrl: the directory of the repository's index.yaml (with a / at the end)
    SourceUpdater(NetworkManager *networkManager, const QString &bundledDir, const QString &installedDir,
                  const QString &repositoryUrl, QObject *parent = nullptr);

    // Installs the bundled sources that are newer than the installed ones (all of them the first time, the ones of
    // an update of the app later). Returns the directory to load the sources from.
    QString installBundled();

    // Compares the index of the repository with the installed one.
    Task<Result<Changes>> check();
    // Downloads the changed files and installs the index with them. Returns the loaded sources.
    Task<Result<QList<QSharedPointer<MangaSourceBase>>>> install(Changes changes);

    QString installedDirectory() const { return installedDir; }

private:
    NetworkManager *networkManager;
    QString bundledDir;
    QString installedDir;
    QString repositoryUrl;

    // Writes index with its files (each from the first of the directories that has it with the hash of the
    // index) into the installed directory, through a new directory that replaces it. extraFiles: name → content
    // (e.g. the bundled index it came from).
    Result<> writeInstalled(const SourceIndex &index, const QStringList &fromDirs,
                            const QMap<QString, QByteArray> &extraFiles);
};
