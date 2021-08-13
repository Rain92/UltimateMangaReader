#ifndef MANGAINFO_H
#define MANGAINFO_H

#include <QMutex>

#include "abstractmangasource.h"
#include "downloadimagedescriptor.h"
#include "mangachaptercollection.h"
#include "readingprogress.h"
#include "staticsettings.h"

class MangaInfo : public QObject
{
    Q_OBJECT

public:
    explicit MangaInfo(AbstractMangaSource *mangaSource);
    ~MangaInfo();

    static QSharedPointer<MangaInfo> deserialize(AbstractMangaSource *mangaSource, const QString &path);

    void serialize();
    QString title;
    QString url;

    QString author;
    QString artist;
    QString releaseYear;
    QString genres;
    QString status;
    QString summary;
    QString coverPath;
    QString coverUrl;

    QString hostname;

    bool updated;

    AbstractMangaSource *mangaSource;

    MangaChapterCollection chapters;

    QScopedPointer<QMutex> updateMutex;

    void updateCompeted(bool updated, const QList<QPair<int, int>> &moveMap);
    QString coverThumbnailPath() const;

    void sendCoverLoaded();

signals:
    void updatedSignal(bool newchapters);
    void chaptersMoved(QList<QPair<int, int>> moveMap);
    void coverLoaded();
};

#endif  // MANGAINFO_H
