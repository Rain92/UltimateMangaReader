#include "mangasourcebase.h"

MangaSourceBase::MangaSourceBase(NetworkManager *networkManager)
    : networkManager(networkManager), headerRules(), htmlConverter()
{
}

Task<Result<ImageLink>> MangaSourceBase::resolveImage(ImageLink page)
{
    // by default the page list has the images
    co_return page;
}

HttpHeaders MangaSourceBase::requestHeaders(const QString &url) const
{
    return headerRules.forUrl(url);
}

QSharedPointer<DownloadBufferJob> MangaSourceBase::download(DownloadRequest request)
{
    request.headers = requestHeaders(request.url) + request.headers;
    return networkManager->download(request);
}

QSharedPointer<DownloadBufferJob> MangaSourceBase::download(const QString &url, int timeout, int maxRetries)
{
    DownloadRequest request;
    request.url = url;
    request.timeout = timeout;
    request.maxRetries = maxRetries;
    return download(request);
}

Task<NetworkManager::ImageSizes> MangaSourceBase::probeImageSizes(QStringList urls, int timeout)
{
    return networkManager->probeImageSizes(urls, timeout, requestHeadersFunction());
}

std::function<HttpHeaders(const QString &)> MangaSourceBase::requestHeadersFunction() const
{
    return [this](const QString &url) { return requestHeaders(url); };
}

QString MangaSourceBase::htmlToPlainText(const QString &str)
{
    htmlConverter.setHtml(str);
    return htmlConverter.toPlainText();
}

QString MangaSourceBase::capture(const QRegularExpression &rx, const QString &text)
{
    // an empty pattern would match anywhere
    if (rx.pattern().isEmpty())
        return {};

    auto match = rx.match(text);
    return match.hasMatch() ? match.captured(1) : QString();
}

QString MangaSourceBase::genresFromHtml(const QString &html)
{
    return htmlToPlainText(html)
        .trimmed()
        .remove('\n')
        .replace(", ", " ")
        .replace("/ ", " ")
        .replace(",", " ");
}
