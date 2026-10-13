#include "cachefile.h"

#include "staticsettings.h"

static const quint32 magic = 0x554d5243;  // "UMRC"

static quint32 formatVersion(CacheFile type)
{
    // a change of a format increments its version
    switch (type)
    {
        case CacheFile::MangaInfo:
        case CacheFile::ReadingProgress:
        case CacheFile::Favorites:
        case CacheFile::Settings:
        case CacheFile::MangaList:
            return 1;
    }
    return 0;
}

void writeCacheFileHeader(QDataStream &out, CacheFile type)
{
    out.setVersion(CONF.dataStreamVersion);
    out << magic << static_cast<quint32>(type) << formatVersion(type);
}

bool readCacheFileHeader(QDataStream &in, CacheFile type)
{
    in.setVersion(CONF.dataStreamVersion);

    quint32 fileMagic = 0, fileType = 0, fileVersion = 0;
    in >> fileMagic >> fileType >> fileVersion;

    if (fileMagic != magic || fileType != static_cast<quint32>(type) || fileVersion != formatVersion(type))
        in.setStatus(QDataStream::ReadCorruptData);

    return in.status() == QDataStream::Ok;
}
