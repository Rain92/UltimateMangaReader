#include "utils.h"

#include <unistd.h>

#ifdef __ARM_NEON__
#include <arm_neon.h>
#endif

#ifdef Q_OS_LINUX
#include <sched.h>
#include <sys/resource.h>
#include <sys/syscall.h>
#endif

#include "staticsettings.h"

QList<QRegularExpressionMatch> getAllRxMatches(const QRegularExpression& rx, const QString& text, int spos,
                                               int epos)
{
    QList<QRegularExpressionMatch> rxlist;

    QRegularExpressionMatchIterator rxit = rx.globalMatch(text, spos);

    while (rxit.hasNext())
    {
        auto rxmatch = rxit.next();
        if (epos >= 0 && rxmatch.capturedStart() >= epos)
            break;

        rxlist.append(rxmatch);
    }

    return rxlist;
}

QString urlHash(const QString& url, int length)
{
    return QCryptographicHash::hash(url.toUtf8(), QCryptographicHash::Sha1).toHex().left(length);
}

PageTurnDirection conditionalReverse(PageTurnDirection dir, bool condition)
{
    if (!condition)
    {
        return dir;
    }
    else
    {
        if (dir == Forward)
            return Backward;
        else
            return Forward;
    }
}

qint64 dirSize(const QString& path)
{
    qint64 size = 0;
    QDir dir(path);
    // calculate total size of current directories' files
    QDir::Filters fileFilters = QDir::Files | QDir::System | QDir::Hidden;
    for (const auto& filePath : dir.entryList(fileFilters))
        size += QFileInfo(dir, filePath).size();

    QDir::Filters dirFilters = QDir::Dirs | QDir::NoDotAndDotDot | QDir::System | QDir::Hidden;

    for (const auto& childDirPath : dir.entryList(dirFilters))
        size += dirSize(path + QDir::separator() + childDirPath);
    return size;
}

bool removeDir(const QString& path, const QString& ignore)
{
    // links are removed, not followed
    if (ignore.isEmpty())
        return QDir(path).removeRecursively();

    bool result = true;
    QDir dir(path);
    for (const auto& info :
         dir.entryInfoList(QDir::NoDotAndDotDot | QDir::System | QDir::Hidden | QDir::AllDirs | QDir::Files))
    {
        if (info.isDir() && !info.isSymLink())
            result = removeDir(info.absoluteFilePath(), ignore) && result;
        else if (!info.fileName().endsWith(ignore))
            result = QFile::remove(info.absoluteFilePath()) && result;
    }
    // it stays when it keeps files
    dir.rmdir(dir.absolutePath());

    return result;
}

qint64 getCacheSize()
{
    qint64 size = dirSize(CONF.cacheDir) / 1024 / 1024;

    return size;
}

qint64 getFreeSpace()
{
    QStorageInfo info(CONF.cacheDir);

    qint64 space = info.bytesAvailable() / 1024 / 1024;
    return space;
}

unsigned long long getTotalSystemMemory()
{
    long pages = sysconf(_SC_PHYS_PAGES);
    long page_size = sysconf(_SC_PAGE_SIZE);
    return pages * page_size;
}

unsigned long long getFreeSystemMemory()
{
#ifdef Q_OS_LINUX
    // MemFree alone is soon taken up by the page cache (every page that was downloaded or read), the image
    // cache of the reader was always cleared then. The Kobo's kernel (3.0) has no MemAvailable yet.
    QFile file("/proc/meminfo");
    if (file.open(QIODevice::ReadOnly))
    {
        QHash<QByteArray, qint64> values;
        for (const QByteArray& line : file.readAll().split('\n'))
        {
            int colon = line.indexOf(':');
            if (colon > 0)
                values.insert(line.left(colon), line.mid(colon + 1).trimmed().split(' ').first().toLongLong());
        }

        if (values.contains("MemAvailable"))
            return values["MemAvailable"] * 1024;
        if (values.contains("MemFree"))
            return qMax<qint64>(0, values["MemFree"] + values["Buffers"] + values["Cached"] - values["Shmem"]) *
                   1024;
    }
#endif

    long pages = sysconf(_SC_AVPHYS_PAGES);
    long page_size = sysconf(_SC_PAGE_SIZE);
    return pages * page_size;
}

bool enoughFreeSystemMemory()
{
    return getFreeSystemMemory() / 1024 / 1024 > 100;
}

void lowerThreadPriority()
{
#ifdef Q_OS_LINUX
    // QThread priorities don't apply to SCHED_OTHER on Linux, the nice value of the thread does
    setpriority(PRIO_PROCESS, syscall(SYS_gettid), 5);
#endif
}

void setIdleThreadPriority(bool idle)
{
#ifdef Q_OS_LINUX
    // pid 0 is the calling thread
    sched_param param = {};
    sched_setscheduler(0, idle ? SCHED_IDLE : SCHED_OTHER, &param);
#else
    Q_UNUSED(idle);
#endif
}

#ifdef __ARM_NEON__
static void decryptXorInplace_NEON(QByteArray& data, const QByteArray& key)
{
    const int keyLength = key.length();

    // The key repeated until any 16 byte window starting inside the first period is valid,
    // so every iteration processes 16 bytes regardless of the key length.
    QByteArray keyStream;
    keyStream.reserve(keyLength + 16 + keyLength);
    while (keyStream.length() < keyLength + 16)
        keyStream.append(key);

    uint8_t* datap = (uint8_t*)data.data();
    const uint8_t* keyp = (const uint8_t*)keyStream.constData();
    const qsizetype length = data.length();

    qsizetype doff = 0;
    int koff = 0;
    for (; doff + 16 <= length; doff += 16)
    {
        uint8x16_t vkey = vld1q_u8(keyp + koff);
        uint8x16_t vdata = vld1q_u8(datap + doff);
        vst1q_u8(datap + doff, veorq_u8(vdata, vkey));

        koff += 16;
        if (koff >= keyLength)
        {
            koff -= keyLength;
            if (koff >= keyLength)  // keys shorter than 16 bytes
                koff %= keyLength;
        }
    }

    for (; doff < length; doff++)
    {
        datap[doff] ^= keyp[koff];
        if (++koff == keyLength)
            koff = 0;
    }
}
#endif

void decryptXorInplace(QByteArray& data, const QByteArray& key)
{
    if (key.isEmpty() || data.isEmpty())
        return;

#ifdef __ARM_NEON__
    decryptXorInplace_NEON(data, key);
#else
    char* datap = data.data();
    const char* keyp = key.constData();
    const qsizetype length = data.length();
    const qsizetype keyLength = key.length();

    for (qsizetype i = 0, p = 0; i < length; i++)
    {
        datap[i] ^= keyp[p];
        if (++p == keyLength)
            p = 0;
    }
#endif
}

QByteArray hexstr2array(const QString& str)
{
    QByteArray key;
    for (int i = 0; i < str.length(); i += 2)
    {
        auto sub = str.mid(i, 2);
        key.append(sub.toUInt(nullptr, 16));
    }
    return key;
}
