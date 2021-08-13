#ifndef READINGSTATEMANAGER_H
#define READINGSTATEMANAGER_H

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
                              QObject *parent = nullptr);

    bool isFavorite(QSharedPointer<MangaInfo> info);
    bool toggleFavorite(QSharedPointer<MangaInfo> info);

    void moveFavoriteToFront(int i);
    void updateInfos();
    bool loadInfos();
    void serialize();
    void clearFavorites();
    void resetUpdatedStatus();

    int findFavorite(const QString &title);

    QList<QSharedPointer<MangaInfo>> favoriteinfos;
    QList<Favorite> favorites;

signals:
    void error(const QString &error);

private:
    const QMap<QString, AbstractMangaSource *> &mangasources;
    void deserialize();
};

#endif  // READINGSTATEMANAGER_H
