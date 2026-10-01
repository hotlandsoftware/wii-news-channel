#ifndef REVOLUTION_SC_H
#define REVOLUTION_SC_H

#include <types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SC_AREA_JPN,
    SC_AREA_USA,
    SC_AREA_EUR,
    SC_AREA_AUS,
    SC_AREA_BRA,
    SC_AREA_TWN,
    SC_AREA_KOR,
    SC_AREA_HKG,
    SC_AREA_ASI,
    SC_AREA_LTN,
    SC_AREA_SAF,
} SCProductArea;

typedef enum {
    SC_LANG_JAPANESE,
    SC_LANG_ENGLISH,
    SC_LANG_GERMAN,
    SC_LANG_FRENCH,
    SC_LANG_SPANISH,
    SC_LANG_ITALIAN,
    SC_LANG_DUTCH,
    SC_LANG_SIMP_CHINESE,
    SC_LANG_TRAD_CHINESE,
    SC_LANG_KOREAN,
} SCLanguage;

s8 SCGetProductArea(void);
u8 SCGetLanguage(void);

#ifdef __cplusplus
}
#endif

#endif
