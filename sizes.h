#ifndef SIZES_H
#define SIZES_H

#define SIZES DPIAwareSizes::get()
class DPIAwareSizes
{
public:
    static DPIAwareSizes &get()
    {
        static DPIAwareSizes instance;
        return instance;
    }

    const int screenDPI = 108;
    constexpr int mmToPx(float mm) { return (int)(mm * screenDPI * 0.0393701); }

    const int listSourcesHeight = mmToPx(22);
    const int mangasourceIconSize = mmToPx(13);
    const int mangasourceItemWidth = mmToPx(19);
    const int mangasourceItemHeight = mmToPx(19);
    const int mangasourceIconSpacing = mmToPx(2);

    const int buttonSize = mmToPx(7);
    const int buttonSizeToggleFavorite = mmToPx(8);

    const int numpadHeight = mmToPx(35);

    const int resourceIconSize = mmToPx(5);
    const int batteryIconHeight = mmToPx(3);
    const int wifiIconSize = mmToPx(6);
    const int menuIconSize = mmToPx(10);

    const int coverHeight = mmToPx(50);
    const int coverWidth = coverHeight * 0.7;

    const int favoriteSectonHeight = mmToPx(20);
    const int favoriteCoverSize = mmToPx(16);

    const int frontlightSliderHandleHeight = mmToPx(8);

    const int errormessageWidgetHeight = mmToPx(8);

    const int downloadStatusDialogWidth = mmToPx(80);
    const int downloadStatusDialogHeight = mmToPx(60);

    const float readerPageSideThreshold = 0.4;
    const float readerBottomMenuThreshold = 0.1;
};

#endif  // SIZES_H
