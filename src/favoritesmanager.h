#pragma once

#include <QElapsedTimer>
#include <QString>
#include <QTimer>
#include <QtConcurrent/QtConcurrent>

#include "abstractmangasource.h"
#include "favorite.h"
#include "networkmanager.h"

class FavoritesManager : public QObject
{
    Q_OBJECT
public:
    explicit FavoritesManager(const QMap<QString, AbstractMangaSource *> &mangasources,
                              NetworkManager *networkManager, QObject *parent = nullptr);

    bool isFavorite(QSharedPointer<MangaInfo> info);
    bool toggleFavorite(QSharedPointer<MangaInfo> info);

    void moveFavoriteToFront(int i);
    // Updates the infos from the sites in the background (new chapters).
    void updateInfos();
    // Loads the infos from the cache, without downloads, so it doesn't block. An info that isn't cached yet
    // only has its title and url until updateInfos() loaded it.
    void loadInfos();
    // The loaded info of favorite i: from the cache, else downloaded (blocking), e.g. when it gets opened
    // before updateInfos() loaded it. Null if it isn't available.
    QSharedPointer<MangaInfo> requireInfo(int i);
    void serialize();
    void clearFavorites();
    void clearInvalidFavorites();
    void resetUpdatedStatus();

    int findFavorite(const MangaInfo *info) const;

    // favoriteinfos[i] belongs to favorites[i] once loadInfos() was called. It is null when the source of the
    // favorite isn't enabled.
    QList<QSharedPointer<MangaInfo>> favoriteinfos;
    QList<Favorite> favorites;

signals:
    void error(const QString &error);

private:
    const QMap<QString, AbstractMangaSource *> &mangasources;
    NetworkManager *networkManager;
    bool infosLoaded;
    QHash<QString, QElapsedTimer> lastUpdates;
    bool updateAfterConnecting;

    void deserialize();
    QSharedPointer<MangaInfo> loadInfo(const Favorite &fav);
    int indexOf(const Favorite &fav) const;
};
