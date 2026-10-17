#pragma once

#include <QStringList>
#include <optional>

#include "result.h"

// The index of a directory of sources (index.yaml, see sources/README.md): the sources and the libraries of their
// scripts, with their versions and files.
struct SourceIndex
{
    // major.minor (e.g. 3.0): the major version is the one of the app that runs it (LuaSource::majorVersion), the
    // minor version goes up with every change of the files.
    struct Version
    {
        int major = 0;
        int minor = 0;

        auto operator<=>(const Version &) const = default;
        QString toString() const;
        static std::optional<Version> parse(const QString &text);
    };

    struct Entry
    {
        // the base name of its files (<id>.yaml, <id>.lua, <id>.png; <id>.lua of a library), unique in the index
        QString id;
        // of a source: how the app shows it, it identifies the source in the cache and the favorites
        QString name;
        Version version;
        // the file names
        QStringList files;

        // Whether this app runs it: the major version of the app.
        bool isCompatible() const;
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
};
