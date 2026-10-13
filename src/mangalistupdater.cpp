#include "mangalistupdater.h"

MangaListUpdater::MangaListUpdater(SourceRegistry* sources, MangaListStore* lists, Settings* settings,
                                   StandbyManager* standbyManager, QObject* parent)
    : QObject(parent),
      sources(sources),
      lists(lists),
      settings(settings),
      standbyManager(standbyManager),
      updating(false)
{
}

void MangaListUpdater::updateMangaLists(QSharedPointer<UpdateProgressToken> progressToken)
{
    // a cancelled update might still be waiting for its last download
    if (updating)
    {
        progressToken->sendError("The previous update is still stopping, please try again.");
        return;
    }

    update(progressToken);
}

Task<> MangaListUpdater::update(QSharedPointer<UpdateProgressToken> progressToken)
{
    updating = true;
    standbyManager->setInhibited(true);

    bool ok = true;
    QStringList warnings;
    for (const auto& name : progressToken->sourcesProgress.keys())
    {
        if (progressToken->sourcesProgress[name] == 100)
            continue;

        // it might have been disabled meanwhile
        auto ms = sources->active().value(name);
        if (!ms)
            continue;

        progressToken->currentSourceName = name;
        // the updates compare with the list and keep its titles
        auto update = co_await ms->fetchMangaList(progressToken.get(), lists->list(name));
        if (update.status == MangaListUpdate::Failed)
        {
            // the dialog is gone after a cancel
            if (!update.error.isCancelled())
                progressToken->sendError(update.error.message);
            ok = false;
            break;
        }

        // A list that shrank a lot is more likely a broken update (the site changed) than the titles being
        // gone: the old one stays, a retry starts over.
        auto& list = lists->list(name);
        int oldSize = list.size;
        int newSize = update.list.size;
        if (oldSize > 0 && newSize < oldSize / 2)
        {
            qDebug() << "Update of" << name << "found only" << newSize << "of" << oldSize << "titles";
            progressToken->clearResumeState();
            progressToken->sourcesProgress[name] = 0;
            progressToken->sendError(QString("%1: the update found only %2 of %3 titles, the old list is kept.")
                                         .arg(name)
                                         .arg(newSize)
                                         .arg(oldSize));
            ok = false;
            break;
        }

        update.list.filter();
        update.list.sortBy(settings->mangaOrder);
        list = update.list;
        // the list on the home screen indexes into it
        emit mangaListChanged(ms);
        // only the updated lists, writing all of them takes a while on the device
        if (!lists->save(name))
        {
            progressToken->sendError(QString("Couldn't save the list of %1.").arg(name));
            ok = false;
            break;
        }
        if (update.status == MangaListUpdate::Partial)
            warnings.append(name + ": " + update.error.message);
        progressToken->clearResumeState();
    }

    updating = false;
    standbyManager->setInhibited(false);

    if (ok)
        progressToken->sendFinished(warnings.join('\n'));
}

void MangaListUpdater::sortMangaLists()
{
    for (const auto& name : lists->setOrder(settings->mangaOrder))
        if (auto ms = sources->source(name))
            emit mangaListChanged(ms);
}
