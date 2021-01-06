#include "utils.h"

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
        QScrollerProperties prop = scroller->scrollerProperties();
        prop.setScrollMetric(QScrollerProperties::VerticalOvershootPolicy,
                             QScrollerProperties::OvershootAlwaysOff);
        prop.setScrollMetric(QScrollerProperties::HorizontalOvershootPolicy,
                             QScrollerProperties::OvershootAlwaysOff);
        prop.setScrollMetric(QScrollerProperties::QScrollerProperties::DragStartDistance, 0);
        prop.setScrollMetric(QScrollerProperties::QScrollerProperties::MousePressEventDelay, 0);
        prop.setScrollMetric(QScrollerProperties::QScrollerProperties::OvershootDragDistanceFactor, 0);

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
    return getFreeSystemMemory() / 1024 / 1024 > 150;
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

#ifdef KOBO
void decryptXorInplace_NEON(QByteArray& data, const QByteArray& key)
{
    //    QElapsedTimer t;
    //    t.start();

    QByteArray key2;
    key2.append(key);
    key2.append(key);

    uint8_t* datap = (uint8_t*)data.data();
    uint8_t* key2p = (uint8_t*)key2.data();

    uint8x16_t vdata, vkey, vres, vmask;
    int incr = qMin(16, key.length());
    int rounds = data.length() / incr;

    int koff = 0;
    int doff = 0;

    uint8_t mask[16];
    for (int i = 0; i < 16; i++)
        mask[i] = i < incr ? 255 : 0;

    vmask = vld1q_u8(mask);

    for (int i = 0; i < rounds; i++)
    {
        /* load */
        vkey = vld1q_u8(key2p + koff);
        vdata = vld1q_u8(datap + doff);

        /* res = data ^ key */
        vres = veorq_u8(vdata, vkey);

        // mask result if key is smaller than 16
        if (key.length() < 16)
            vres = vbslq_u8(vmask, vres, vdata);

        /* store */
        vst1q_u8(datap + doff, vres);

        doff += incr;
        koff += incr;
        if (koff >= key.length())
            koff -= key.length();
    }

    // take care of leftovers
    for (; doff < data.length(); doff++, koff++)
    {
        data[doff] = data[doff] ^ key2[koff];
    }

    //    qDebug() << "Xor NEON:" << t.elapsed();
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
