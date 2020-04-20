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
    void setDPI(int dpi)
    {
        if (dpi > 0 && dpi != screenDPI)
        {
            screenDPI = dpi;
            adjustSizes();
        }
    }

    inline int mmToPx(float mm) { return (int)(mm * screenDPI * 0.0393701); }

    int listSourcesHeight;
    int mangasourceIconSize;
    int mangasourceItemWidth;
    int mangasourceItemHeight;
    int mangasourceIconSpacing;

    int buttonSize;
    int buttonSizeToggleFavorite;

    int numpadHeight;

    int resourceIconSize;
    int batteryIconHeight;
    int wifiIconSize;
    int menuIconSize;

    int coverSize;

    int favoriteSectonHeight;
    int favoriteCoverHeight;
    int favoriteCoverWidth;

    int frontlightSliderGrooveHeight;
    int frontlightSliderHandleWidth;
    int frontlightSliderHandleHeight;

    int errormessageWidgetHeight;

    int downloadStatusDialogWidth;
    int downloadStatusDialogHeight;

    float readerPreviousPageThreshold;
    float readerBottomMenuThreshold;

private:
    int screenDPI;
    DPIAwareSizes()
    {
#ifdef DESKTOP
        screenDPI = 108;
#else
        screenDPI = 300;
#endif
        readerPreviousPageThreshold = 0.3;
        readerBottomMenuThreshold = 0.1;

        adjustSizes();
    };

    void adjustSizes()
    {
        listSourcesHeight = mmToPx(22);
        mangasourceIconSize = mmToPx(13);
        mangasourceItemWidth = mmToPx(20);
        mangasourceItemHeight = mmToPx(20);
        mangasourceIconSpacing = mmToPx(2.5);

        buttonSize = mmToPx(8);
        buttonSizeToggleFavorite = mmToPx(8);

        numpadHeight = mmToPx(35);

        resourceIconSize = mmToPx(5);
        batteryIconHeight = mmToPx(3);
        wifiIconSize = mmToPx(6);
        menuIconSize = mmToPx(10);

        coverSize = mmToPx(50);

        favoriteSectonHeight = mmToPx(20);
        favoriteCoverHeight = mmToPx(16);
        favoriteCoverWidth = mmToPx(16);

        frontlightSliderGrooveHeight = mmToPx(5);
        frontlightSliderHandleWidth = mmToPx(7);
        frontlightSliderHandleHeight = mmToPx(6);

        errormessageWidgetHeight = mmToPx(8);

        downloadStatusDialogWidth = mmToPx(80);
        downloadStatusDialogHeight = mmToPx(60);
    }
};

#endif  // SIZES_H
