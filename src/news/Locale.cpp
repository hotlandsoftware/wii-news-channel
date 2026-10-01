#include <news/Locale.h>
#include <revolution/sc.h>

s32 GetRegionGroup() {
    switch (SCGetProductArea()) {
    case SC_AREA_JPN:
    case SC_AREA_TWN:
        return 0;
    case SC_AREA_EUR:
    case SC_AREA_AUS:
    case SC_AREA_SAF:
        return 2;
    default:
        return 1;
    }
}

u8 GetSupportedLanguage() {
    u8 lang = SCGetLanguage();
    switch (lang) {
    case SC_LANG_JAPANESE:
    case SC_LANG_ENGLISH:
    case SC_LANG_GERMAN:
    case SC_LANG_FRENCH:
    case SC_LANG_SPANISH:
    case SC_LANG_ITALIAN:
    case SC_LANG_DUTCH:
        return lang;
    default:
        return SC_LANG_ENGLISH;
    }
}
