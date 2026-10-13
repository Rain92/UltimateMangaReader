#pragma once

#include <QObject>

#include "mangaliststore.h"
#include "settings.h"
#include "sourceregistry.h"
#include "standbymanager.h"
#include "task.h"
#include "updateprogresstoken.h"

// Updates the manga lists of the enabled sources from the sites, and sorts them.
class MangaListUpdater : public QObject
{
    Q_OBJECT

public:
    // No standby while an update runs (if there is a standbyManager).
    MangaListUpdater(SourceRegistry *sources, MangaListStore *lists, Settings *settings,
                     StandbyManager *standbyManager, QObject *parent = nullptr);

    // The sources of the token that aren't at 100 % yet, one after the other, in the background. The token gets
    // the progress, the errors and the end.
    void updateMangaLists(QSharedPointer<UpdateProgressToken> progressToken);
    // In the order of the settings (the lists that aren't loaded yet when they are loaded).
    void sortMangaLists();

signals:
    // it was updated or sorted
    void mangaListChanged(const QString &source);

private:
    SourceRegistry *sources;
    MangaListStore *lists;
    Settings *settings;
    StandbyManager *standbyManager;
    bool updating;

    Task<> update(QSharedPointer<UpdateProgressToken> progressToken);
};
