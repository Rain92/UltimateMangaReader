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
    const QRegularExpression forbiddenchars(R"([\/:*"<>|])");
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
    {
        QFileInfo fi(dir, filePath);
        size += fi.size();
    }
    // add size of child directories recursively
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
        foreach (QFileInfo info, dir.entryInfoList(QDir::NoDotAndDotDot | QDir::System | QDir::Hidden |
                                                       QDir::AllDirs | QDir::Files,
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
        prop.setScrollMetric(QScrollerProperties::MaximumClickThroughVelocity, 0);
        prop.setScrollMetric(QScrollerProperties::MousePressEventDelay, 0.5);
        scroller->setScrollerProperties(prop);
        scroller->grabGesture(pArea, QScroller::LeftMouseButtonGesture);
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
