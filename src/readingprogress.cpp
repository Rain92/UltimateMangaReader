#include "readingprogress.h"

#include <QSaveFile>

ReadingProgress::ReadingProgress(const MangaIndex &index, int numChapters, int numPages)
    : index(index), numChapters(numChapters), numPages(numPages)
{
}

ReadingProgress::ReadingProgress(const QString &hostname, const QString &title)
    : numChapters(0), numPages(0)
{
    deserialize(hostname, title);
}

void ReadingProgress::serialize(const QString &hostname, const QString &title)
{
    QSaveFile file(CONF.mangainfodir(hostname, title) + "progress.dat");
    if (!file.open(QIODevice::WriteOnly))
        return;

    QDataStream out(&file);
    out.setVersion(CONF.dataStreamVersion);
    out << *this;
    if (out.status() != QDataStream::Ok || !file.commit())
        qDebug() << "Couldn't save the reading progress of" << title;
}

bool ReadingProgress::deserialize(const QString &hostname, const QString &title)
{
    QFile file(CONF.mangainfodir(hostname, title) + "progress.dat");

    if (!file.open(QIODevice::ReadOnly))
        return false;

    QDataStream in(&file);
    in.setVersion(CONF.dataStreamVersion);
    in >> *this;

    if (in.status() != QDataStream::Ok)
    {
        qDebug() << "Couldn't read the reading progress of" << title;
        *this = ReadingProgress(MangaIndex(0, 0), 0, 0);
        return false;
    }
    return true;
}

QDataStream &operator<<(QDataStream &str, const ReadingProgress &m)
{
    return str << m.index << m.numChapters << m.numPages;
}

QDataStream &operator>>(QDataStream &str, ReadingProgress &m)
{
    return str >> m.index >> m.numChapters >> m.numPages;
}
