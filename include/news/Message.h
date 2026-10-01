#ifndef NEWS_MESSAGE_H
#define NEWS_MESSAGE_H

#include <types.h>

// Localized string tables, indexed by gLanguage.
extern const wchar_t* gMsgNewsChannel[7];
extern const wchar_t* gMsgChooseLanguage[8];
extern const wchar_t* gMsgWeekday[7][7];

const wchar_t* GetMsgSectionSelect();
const wchar_t* GetMsgToSectionSelect();
const wchar_t* GetMsgToTop();

s32 GetSectionRowCount();

// "<updated> hh:mm ago"-style strings, per language and message variant.
void FormatElapsedA_EN(s32 minutes, wchar_t* buf, u32 size);
void FormatElapsedB_EN(s32 minutes, wchar_t* buf, u32 size);
void FormatElapsedB_DE(s32 minutes, wchar_t* buf, u32 size);
void FormatElapsedA_FR(s32 minutes, wchar_t* buf, u32 size);
void FormatElapsedB_FR(s32 minutes, wchar_t* buf, u32 size);
void FormatElapsedA_ES(s32 minutes, wchar_t* buf, u32 size);
void FormatElapsedB_ES(s32 minutes, wchar_t* buf, u32 size);
void FormatElapsedB_IT(s32 minutes, wchar_t* buf, u32 size);
void FormatElapsedB_NL(s32 minutes, wchar_t* buf, u32 size);

#endif
