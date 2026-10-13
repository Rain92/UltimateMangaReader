#pragma once

#include <QStringList>

#include "httpheader.h"

// Where the image of a page is, and what downloading it needs besides the url.
struct ImageLink
{
    QString url;
    // the image is encrypted with it (XOR)
    QByteArray xorKey;
    // the rest of the page, which the site cut off into an image of its own: it is stitched below the page
    QString appendUrl;
    // sent besides the headers of the source (e.g. a token of the page list)
    HttpHeaders headers;
    // when the url stops working (seconds since the epoch), 0: it doesn't. Not stored: the images of the pages
    // are resolved again in every session (see MangaInfo::deserialize()).
    qint64 expires = 0;

    bool isEmpty() const { return url.isEmpty(); }

    static ImageLink fromUrl(const QString &url) { return {url, {}, {}, {}, 0}; }

    static QList<ImageLink> fromUrls(const QStringList &urls)
    {
        QList<ImageLink> links;
        links.reserve(urls.size());
        for (const auto &url : urls)
            links.append(fromUrl(url));
        return links;
    }
};

inline QDataStream &operator<<(QDataStream &str, const ImageLink &m)
{
    return str << m.url << m.xorKey << m.appendUrl << m.headers;
}

inline QDataStream &operator>>(QDataStream &str, ImageLink &m)
{
    return str >> m.url >> m.xorKey >> m.appendUrl >> m.headers;
}
