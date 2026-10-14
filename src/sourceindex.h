#pragma once

#include <QMap>
#include <QStringList>

#include "result.h"

// The index of a directory of sources (index.yaml, see sources/README.md): the sources and the libraries of their
// scripts, with their versions and files. `harness index` writes it.
struct SourceIndex
{
    struct Entry
    {
        // the base name of its files (<id>.yaml, <id>.lua, <id>.png)
        QString id;
        // of a source: how the app shows it, it identifies the source in the cache and the favorites
        QString name;
        // goes up with every change of the files
        int version = 0;
        // the scripting interface of the app that it needs (LuaSource::apiLevel)
        int api = 1;
        // the file names and their SHA-256 (hex)
        QMap<QString, QString> files;
    };

    QList<Entry> libraries;
    // in the order the app shows them
    QList<Entry> sources;

    static Result<SourceIndex> parse(const QByteArray &yaml, const QString &name);
    // <directory>/index.yaml
    static Result<SourceIndex> read(const QString &directory);
    QByteArray toYaml() const;

    const Entry *source(const QString &id) const;
    const Entry *library(const QString &id) const;

    // The SHA-256 of a file (hex), empty if it can't be read.
    static QString fileHash(const QString &path);
};
