#ifndef SIZES_H
#define SIZES_H

#ifdef DESKTOP
#define screen_dpi 108
#else
#define screen_dpi 300
#endif

#define mm_to_px(mm) (mm * screen_dpi * 0.0393701)

#define listsourcesheight mm_to_px(22)
#define mangasourceiconsize mm_to_px(13)
#define mangasourceitemwidth mm_to_px(20)
#define mangasourceitemheight mm_to_px(20)
#define mangacourceiconspacing mm_to_px(2.5)

#define buttonsize mm_to_px(8)
#define buttonsizeaddfavorite mm_to_px(8)

#define numpadheight mm_to_px(35)

#define resourceiconsize mm_to_px(5)
#define batteryiconsize mm_to_px(3)
#define menuiconsize mm_to_px(10)

#define scrollbarsliderminlength mm_to_px(8)
#define scrollbarwidth mm_to_px(6)
#define scrollbarheight mm_to_px(4)
#define summaryscrollbarwidth mm_to_px(5)

#define coversize mm_to_px(50)

#define favoritesectonheight mm_to_px(20)
#define favoritesectionwidth mm_to_px(50)
#define favoritecoverheight mm_to_px(16)
#define favoritecoverwidth mm_to_px(16)

#define readerpreviouspagethreshold 0.3
#define readerbottommenuthreshold 0.1

#define frontlightslidergrooveheight mm_to_px(5)
#define frontlightsliderhandlewidth mm_to_px(7)
#define frontlightsliderhandleheight mm_to_px(6)

#define errormessagewidgetheight mm_to_px(8)

#endif  // SIZES_H
