#include "configs.h"

QString makePathLegal(QString filename)
{
    const QRegExp forbiddenchars("[\\/:*\"<>|]");
    QString ret = filename.remove(forbiddenchars).replace(' ', "-");
    if (ret.endsWith('.'))
        ret.append("_");
    return ret;
}
