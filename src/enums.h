#pragma once

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
    ClearAll,
    ClearInvalidFavs
};

enum MangaOrderMethod
{
    OrderByTitle,
    OrderByPopularity

};

enum DitheringMode
{
    // the values are stored in the settings
    NoDithering = 0,
    SWDithering = 2
};

enum DoublePageMode
{
    DoublePageNoRotation,
    DoublePage90CW,
    DoublePage90CCW

};
