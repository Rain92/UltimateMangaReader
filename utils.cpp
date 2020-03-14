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
