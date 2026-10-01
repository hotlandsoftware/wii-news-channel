#ifndef NEWS_LOCALE_H
#define NEWS_LOCALE_H

#include <types.h>

// Returns 0 for Japan/Taiwan, 2 for Europe/Australia/South Africa, 1 otherwise.
s32 GetRegionGroup();
// SCGetLanguage(), with languages the channel does not support mapped to English.
u8 GetSupportedLanguage();

#endif
