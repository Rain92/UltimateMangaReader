#include "utils.h"

QList<QRegularExpressionMatch> getAllRxMatches(const QRegularExpression& rx,
                                               const QString& text, int spos,
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

void executeOnJobCompletion(QSharedPointer<DownloadJobBase> job,
                            std::function<void()> lambda)
{
    new BindingClass(job, lambda);
}

void executeOnJobCompletion(
    QSharedPointer<DownloadJobBase> job,
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

long dirSize(const QString& path)
{
    long size = 0;
    QDir dir(path);
    // calculate total size of current directories' files
    QDir::Filters fileFilters = QDir::Files | QDir::System | QDir::Hidden;
    for (const auto& filePath : dir.entryList(fileFilters))
    {
        QFileInfo fi(dir, filePath);
        size += fi.size();
    }
    // add size of child directories recursively
    QDir::Filters dirFilters =
        QDir::Dirs | QDir::NoDotAndDotDot | QDir::System | QDir::Hidden;

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
        foreach (
            QFileInfo info,
            dir.entryInfoList(QDir::NoDotAndDotDot | QDir::System |
                                  QDir::Hidden | QDir::AllDirs | QDir::Files,
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
