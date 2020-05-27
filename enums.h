#ifndef ENUMS_H
#define ENUMS_H

#include <QMetaEnum>

enum PageTurnDirection
{
    Forward,
    Backward
};

enum AdvancePageGestureDirection
{
    Left,
    Right
};

enum AdvancePageHWButton
{
    Down,
    Up
};

enum WidgetTab
{
    HomeTab,
    MangaInfoTab,
    FavoritesTab,
    MangaReaderTab
};

enum ClearDownloadCacheLevel
{
    ClearImages = 3,
    ClearInfos,
    ClearAll
};

#endif  // ENUMS_H
