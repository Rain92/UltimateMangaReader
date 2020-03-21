#include "ulitimatemangareadercore.h"

UlitimateMangaReaderCore::UlitimateMangaReaderCore(QObject *parent)
    : QObject(parent),
      mangaSources(),
      activeMangaSources(),
      currentMangaSources(nullptr),
      currentManga(),
      downloadManager(this),
      favoritesManager(activeMangaSources),
      settings()
{
}
