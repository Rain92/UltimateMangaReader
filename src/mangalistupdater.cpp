#include "mangalistupdater.h"

MangaListUpdater::MangaListUpdater(SourceRegistry* sources, Settings* settings, StandbyManager* standbyManager,
                                   QObject* parent)
    : QObject(parent), sources(sources), settings(settings), standbyManager(standbyManager), updating(false)
{
}

void MangaListUpdater::updateMangaLists(QSharedPointer<UpdateProgressToken> progressToken)
{
    // the updates run event loops, a cancelled one might still be waiting for its last download
    if (updating)
    {
        progressToken->sendError("The previous update is still stopping, please try again.");
        return;
    }
    updating = true;
    standbyManager->setInhibited(true);

    bool ok = true;
    for (const auto& name : progressToken->sourcesProgress.keys())
    {
        if (progressToken->sourcesProgress[name] == 100)
            continue;

        // it might have been disabled meanwhile
        auto ms = sources->active().value(name);
        if (!ms)
            continue;

        progressToken->currentSourceName = name;
        // the updates compare with it and keep its titles
        ms->loadMangaList();
        int oldSize = ms->mangaList.size;
        if (!ms->updateMangaList(progressToken.get()))
        {
            ok = false;
            break;
        }

        // A list that shrank a lot is more likely a broken update (the site changed) than the titles being
        // gone: the old one (still on disk) stays, a retry starts over.
        int newSize = ms->mangaList.size;
        if (oldSize > 0 && newSize < oldSize / 2)
        {
            qDebug() << "Update of" << name << "found only" << newSize << "of" << oldSize << "titles";
            ms->deserializeMangaList();
            emit mangaListChanged(ms);
            progressToken->clearResumeState();
            progressToken->sourcesProgress[name] = 0;
            progressToken->sendError(QString("%1: the update found only %2 of %3 titles, the old list is kept.")
                                         .arg(name)
                                         .arg(newSize)
                                         .arg(oldSize));
            ok = false;
            break;
        }

        // only the updated lists, writing all of them takes a while on the device
        ms->mangaList.filter();
        ms->mangaList.sortBy(settings->mangaOrder);
        // the list on the home screen indexes into it (a cancelled update can go on for a while)
        emit mangaListChanged(ms);
        if (!ms->serializeMangaList())
        {
            progressToken->sendError(QString("Couldn't save the list of %1.").arg(name));
            ok = false;
            break;
        }
        progressToken->clearResumeState();
    }

    updating = false;
    standbyManager->setInhibited(false);

    if (ok)
        progressToken->sendFinished();
}

void MangaListUpdater::sortMangaLists()
{
    // the lists that aren't loaded yet are sorted when they are (loading all of them takes seconds)
    for (const auto& ms : sources->all())
    {
        ms->listOrder = settings->mangaOrder;
        if (!ms->mangaListLoaded)
            continue;
        ms->mangaList.sortBy(settings->mangaOrder);
        ms->serializeMangaList();
        emit mangaListChanged(ms.get());
    }
}
