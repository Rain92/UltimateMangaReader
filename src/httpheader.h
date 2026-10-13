#ifndef HTTPHEADER_H
#define HTTPHEADER_H

#include <QByteArray>
#include <QDataStream>
#include <QList>

struct HttpHeader
{
    QByteArray name;
    QByteArray value;
};
using HttpHeaders = QList<HttpHeader>;

inline QDataStream &operator<<(QDataStream &str, const HttpHeader &m)
{
    return str << m.name << m.value;
}

inline QDataStream &operator>>(QDataStream &str, HttpHeader &m)
{
    return str >> m.name >> m.value;
}

#endif  // HTTPHEADER_H
