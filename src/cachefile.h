#ifndef CACHEFILE_H
#define CACHEFILE_H

#include <QDataStream>

// The files of the app start with a header: the kind of file and the version of its format. The files of
// another version (e.g. of an older app) aren't read.
enum class CacheFile : quint32
{
    MangaInfo = 1,
    ReadingProgress = 2,
    Favorites = 3,
    Settings = 4,
    MangaList = 5
};

// Both also set the version of the stream.
void writeCacheFileHeader(QDataStream &out, CacheFile type);
// Whether the stream has the header of type in the current version, the status of the stream is an error if
// not.
bool readCacheFileHeader(QDataStream &in, CacheFile type);

#endif  // CACHEFILE_H
