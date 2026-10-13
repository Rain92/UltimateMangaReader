#pragma once

#include <QObject>

#include "mangasourcebase.h"
#include "settings.h"

// The manga sources by name: which ones there are, which ones are enabled (in the settings) and the one whose
// manga list is shown. The rest of the app keeps the names. The sources themselves are shared: one that is
// replaced or removed (e.g. a script that was updated) stays alive for the work that still uses it.
class SourceRegistry : public QObject
{
    Q_OBJECT

public:
    SourceRegistry(Settings *settings, QList<QSharedPointer<MangaSourceBase>> sources,
                   QObject *parent = nullptr);
    // The sources that are built into the app.
    static QList<QSharedPointer<MangaSourceBase>> builtInSources(NetworkManager *networkManager);

    // All of them, in the order they are shown.
    QStringList names() const;
    // Null if there is none with the name.
    QSharedPointer<MangaSourceBase> source(const QString &name) const;

    // A source that isn't in the settings yet is enabled, one that isn't there (anymore) isn't.
    bool isEnabled(const QString &name) const;
    QStringList enabledNames() const;
    void setEnabled(const QString &name, bool enabled);

    // Empty if none is chosen.
    QString current() const { return currentName; }
    void setCurrent(const QString &name);

signals:
    // Sources were added, removed or replaced, or enabled or disabled.
    void sourcesChanged();
    void currentChanged(const QString &name);

private:
    Settings *settings;
    QList<QSharedPointer<MangaSourceBase>> sources;
    QString currentName;
};
