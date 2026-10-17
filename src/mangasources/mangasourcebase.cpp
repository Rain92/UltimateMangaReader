#include "mangasourcebase.h"

MangaSourceBase::MangaSourceBase(NetworkManager *networkManager)
    : networkManager(networkManager), headerRules()
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
