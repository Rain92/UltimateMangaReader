#ifndef ENUMS_H
#define ENUMS_H

enum PageTurnDirection
{
    Forward = 0,
    Backward
};

enum WidgetTab
{
    HomeTab = 0,
    MangaInfoTab = 1,
    FavoritesTab = 2,
    MangaReaderTab = 3,
    SettingsTab = 4
};

enum ClearCacheLevel
{
    ClearImages = 3,
    ClearInfos,
    ClearAll
};

#endif  // ENUMS_H
