#pragma once

#include "abstractmangasource.h"
#include "mangachaptercollection.h"
#include "readingprogress.h"
#include "staticsettings.h"

class MangaInfo : public QObject
{
    Q_OBJECT

public:
    explicit MangaInfo(AbstractMangaSource *mangaSource);
    ~MangaInfo();

    // Null if the file is missing or can't be read.
    static QSharedPointer<MangaInfo> deserialize(AbstractMangaSource *mangaSource, const QString &path);

    void serialize();
    // for display
    QString title;
    // the identity of the manga (with hostname, the source): the url of its entry in the manga list
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

    // After the chapters were merged.
    void updateCompleted(const MangaChapterCollection::MergeResult &merge);
    // Takes over everything an info download gets (not the identity: source and url, and the title).
    void takeOverDetails(const MangaInfo &other);
    QString coverThumbnailPath() const;

    // The directory of the manga in the cache.
    QString directory() const;
    // The file of a downloaded page.
    QString pagePath(const QString &chapterUrl, int page) const;
    // Empty when there's no such chapter.
    QString pagePath(int chapter, int page) const;

    void sendCoverLoaded();

signals:
    void updatedSignal(bool newchapters);
    // the chapters that were there before have other indices now (or are gone)
    void chaptersMoved();
    void coverLoaded();
};
