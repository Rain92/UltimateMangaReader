#ifndef MANGAINFO_H
#define MANGAINFO_H

#include "downloadimagedescriptor.h"
#include "mangachapter.h"
#include "mangaindex.h"
#include "preloadqueue.h"

class MangaInfo : public QObject
{
    Q_OBJECT

public:
    explicit MangaInfo(AbstractMangaSource *mangaSource);
    ~MangaInfo();

    static QSharedPointer<MangaInfo> deserialize(
        AbstractMangaSource *mangaSource, const QString &path);

    void serialize();
    void serializeProgress();
    void deserializeProgress();

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

    MangaIndex currentIndex;

    int numChapters;

    AbstractMangaSource *mangaSource;

    QStringList chaperTitleListDescending;
    QList<MangaChapter> chapters;

    bool updating;

    QString getCoverpathScaled() const;

    QString getImageLink(MangaIndex index);

    QString getCurrentImage();
    QString goNextPage();
    QString goPrevPage();
    QString goChapterPage(MangaIndex index);
    QString goNextChapter();
    QString goPrevChapter();
    QString goLastChapter();
    QString goFirstChapter();

    void preloadNeighbours(int forward, int backward);
    void preloadChapter();
    void preloadImage(MangaIndex index);

    void preloadPopular();
    void cancelAllPreloads();

    void updateCompeted(bool changed);

signals:
    void completedImagePreloadSignal(const QString &path);
    void updatedSignal();
    void coverLoaded();

public slots:
    void sendCoverLoaded();

private slots:
    void completedImagePreload(const QString &path);

private:
    PreloadQueue preloadQueue;
};

#endif  // MANGAINFO_H
