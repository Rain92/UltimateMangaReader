#ifndef MANGAINFO_H
#define MANGAINFO_H

#include <QMutex>

#include "abstractmangasource.h"
#include "defines.h"
#include "downloadimagedescriptor.h"
#include "mangachaptercollection.h"

class MangaIndex;

class MangaInfo : public QObject
{
    Q_OBJECT

public:
    explicit MangaInfo(AbstractMangaSource *mangaSource);
    ~MangaInfo();

    static QSharedPointer<MangaInfo> deserialize(
        AbstractMangaSource *mangaSource, const QString &path);

    void serialize();
    QString title;
    QString link;

    QString author;
    QString artist;
    QString releaseYear;
    QString genres;
    QString status;
    QString summary;
    QString coverPath;
    QString coverLink;

    QString hostname;

    bool updated;

    AbstractMangaSource *mangaSource;

    MangaChapterCollection chapters;

    bool updating;
    QScopedPointer<QMutex> updateMutex;

    void updateCompeted(bool newchapters);
    QString coverThumbnailPath() const;

signals:
    void updatedSignal(bool newchapters);
    void coverLoaded();

public slots:
    void sendCoverLoaded();
};

#endif  // MANGAINFO_H
