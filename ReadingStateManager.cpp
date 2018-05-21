#include "ReadingStateManager.h"

ReadingStateManager::ReadingStateManager():
    states()
{
//    deserialize();
}

void ReadingStateManager::deserialize()
{
    QDir directory(readingstatesdir);
    QStringList files = directory.entryList(QStringList() << "*.dat", QDir::Files);
    foreach (const QString &filename, files)
    {
//        qDebug() << readingstatesdir + "/" + filename;
        QFile file(readingstatesdir + "/" + filename);
        if (!file.open(QIODevice::ReadOnly))
            continue;

        QDataStream in(&file);
        ReadingState rs;
        in >> rs;
        file.close();
        states.insert(rs.getKey(), rs);
    }
}

void ReadingStateManager::serialize(const ReadingState &rs)
{
    QFile file(rs.getSerializationFilename());
    if (!file.open(QIODevice::WriteOnly))
        return;

    QDataStream out(&file);
    out << rs;
    file.close();
}


ReadingState *ReadingStateManager::findOrInsert(const MangaInfo &info)
{
    QString key = info.hostname + "_" + info.title;
    if (states.contains(key))
        return &states[key];

    states[key] = ReadingState::fromMangaInfo(info);

    return &states[key];
}


void ReadingStateManager::update(MangaInfo *info)
{
    ReadingState *state = findOrInsert(*info);

    state->currentindex = info->currentindex;
    if (state->maxindex.chapter < state->currentindex.chapter ||
            (state->maxindex.chapter == state->currentindex.chapter && state->maxindex.page < state->currentindex.page))
        state->maxindex = state->currentindex;

    serialize(*state);
}

void ReadingStateManager::toggleFavorite(MangaInfo *info)
{
    ReadingState *state = findOrInsert(*info);
    state->isfavorite = !state->isfavorite;

    serialize(*state);
}

QList<ReadingState> ReadingStateManager::getFavorites()
{
    QList<ReadingState> ret;

    foreach (const ReadingState &state, states)
    {
        if (state.isfavorite)
            ret.append(state);
    }

    return ret;
}
