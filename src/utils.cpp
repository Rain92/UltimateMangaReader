#include "utils.h"

#include <QGuiApplication>
#include <atomic>

#include "staticsettings.h"

// read by the threads that generate cover thumbnails
static std::atomic<double> currentWindowPixelRatio{0};

qreal windowPixelRatio()
{
    double ratio = currentWindowPixelRatio.load(std::memory_order_relaxed);
    return ratio > 0 ? ratio : qGuiApp->devicePixelRatio();
}

void setWindowPixelRatio(qreal ratio)
{
    currentWindowPixelRatio.store(ratio, std::memory_order_relaxed);
}

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

QString makePathLegal(QString filename)
{
    const QRegularExpression forbiddenchars(R"([\/:*"<>|?])");
    QString ret = filename.remove(forbiddenchars).replace(' ', "-");
    if (ret.endsWith('.'))
        ret.append("_");
    return ret;
}

void executeOnJobCompletion(QSharedPointer<DownloadJobBase> job, std::function<void()> lambda)
{
    new BindingClass(job, lambda);
}

void executeOnJobCompletion(QSharedPointer<DownloadJobBase> job,
                            std::function<void(QSharedPointer<DownloadJobBase>)> lambda)
{
    new BindingClass(job, lambda);
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
    bool result = true;
    QDir dir(path);

    if (dir.exists())
    {
        for (const auto& info : dir.entryInfoList(
                 QDir::NoDotAndDotDot | QDir::System | QDir::Hidden | QDir::AllDirs | QDir::Files,
                 QDir::DirsFirst))
        {
            if (info.isDir())
            {
                result = removeDir(info.absoluteFilePath(), ignore);
            }
            else
            {
                if (ignore == "" || !info.absoluteFilePath().endsWith(ignore))
                    result = QFile::remove(info.absoluteFilePath());
            }

            if (!result)
            {
                return result;
            }
        }
        // result = dir.rmdir(dirName);
    }
    return result;
}

void activateScroller(QAbstractScrollArea* pArea)
{
    if (pArea)
    {
        QScroller* scroller = QScroller::scroller(pArea);
        // properties won't be set for some reason; has to be hardfixed in the qt source
        QScrollerProperties prop;
        prop.setScrollMetric(QScrollerProperties::QScrollerProperties::MousePressEventDelay, 0);
        prop.setScrollMetric(QScrollerProperties::VerticalOvershootPolicy,
                             QScrollerProperties::OvershootAlwaysOff);
        prop.setScrollMetric(QScrollerProperties::HorizontalOvershootPolicy,
                             QScrollerProperties::OvershootAlwaysOff);

        prop.setScrollMetric(QScrollerProperties::OvershootDragResistanceFactor, 0);
        prop.setScrollMetric(QScrollerProperties::OvershootDragDistanceFactor, 0);

        scroller->setScrollerProperties(prop);
        scroller->grabGesture(pArea->viewport(), QScroller::LeftMouseButtonGesture);
    }
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
    long pages = sysconf(_SC_AVPHYS_PAGES);
    long page_size = sysconf(_SC_PAGE_SIZE);
    return pages * page_size;
}

bool enoughFreeSystemMemory()
{
    return getFreeSystemMemory() / 1024 / 1024 > 100;
}

void decryptXorInplace(QByteArray& data, const QByteArray& key)
{
    //    QElapsedTimer t;
    //    t.start();

    // loops inflated for better performance due to compiler optimizations
    int r = data.length() / key.length();
    int i, c, p;
    for (i = 0, c = 0; c < r; c++)
        for (p = 0; p < key.length(); i++, p++)
        {
            data[i] = data[i] ^ key[p];
        }

    for (p = 0; i < data.length(); i++, p++)
    {
        data[i] = data[i] ^ key[p];
    }

    //    for (int i = 0, p = 0; i < data.length(); i++, p++)
    //    {
    //        if (p >= key.length())
    //            p = 0;
    //        data[i] = data[i] ^ key[p];
    //    }

    //    qDebug() << "Xor:" << t.elapsed();
}

#ifdef __ARM_NEON__
void decryptXorInplace_NEON(QByteArray& data, const QByteArray& key)
{
    const int keyLength = key.length();
    if (keyLength <= 0 || data.isEmpty())
        return;

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
