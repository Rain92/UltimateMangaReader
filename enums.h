#ifndef ENUMS_H
#define ENUMS_H

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

enum MangaOrderMethod
{
    OrderByTitle,
    OrderByPopularity

};

enum DitheringMode
{
    NoDithering,
    HWDithering,
    SWHWDithering

};

#endif  // ENUMS_H
