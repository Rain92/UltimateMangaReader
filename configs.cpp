#include "configs.h"


QString makePathLegal(QString filename)
{
    const QRegExp forbiddenchars("[\\/:*\"<>|]");
    return filename.remove(forbiddenchars).replace(' ', "-");
}

