#pragma once

#include <QObject>

#include "settings.h"
#include "sourceregistry.h"
#include "standbymanager.h"
#include "updateprogresstoken.h"

// Updates the manga lists of the enabled sources from the sites, and sorts them.
class MangaListUpdater : public QObject
{
    Q_OBJECT

public:
    MangaListUpdater(SourceRegistry *sources, Settings *settings, StandbyManager *standbyManager,
                     QObject *parent = nullptr);

    // The sources of the token that aren't at 100 % yet, one after the other.
    void updateMangaLists(QSharedPointer<UpdateProgressToken> progressToken);
    // In the order of the settings (the lists that aren't loaded yet when they are loaded).
    void sortMangaLists();

signals:
    // it was updated or sorted
    void mangaListChanged(AbstractMangaSource *source);

private:
    SourceRegistry *sources;
    Settings *settings;
    StandbyManager *standbyManager;
    bool updating;
};
