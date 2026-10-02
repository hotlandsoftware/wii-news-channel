#ifndef OSFONT_H
#define OSFONT_H

#include <types.h>
#include <macros.h>

typedef struct OSFontHeader {
    u16 fontType;
    u16 firstChar;
    u16 lastChar;
    u16 invalChar;
    u16 ascent;
    u16 descent;
    u16 width;
    u16 leading;
    u16 cellWidth;
    u16 cellHeight;
    u32 sheetSize;
    u16 sheetFormat;
    u16 sheetColumn;
    u16 sheetRow;
    u16 sheetWidth;
    u16 sheetHeight;
    u16 widthTable;
    u32 sheetImage;
    u32 sheetFullSize;
    u8 c0;
    u8 c1;
    u8 c2;
    u8 c3;
} OSFontHeader;

// Added for nw4r::ut::RomFont (Task 14), from ogws OSFont.h
typedef enum {
    OS_FONT_ENCODE_ANSI,
    OS_FONT_ENCODE_SJIS,
    OS_FONT_ENCODE_2,
    OS_FONT_ENCODE_UTF8,
    OS_FONT_ENCODE_UTF16,
    OS_FONT_ENCODE_UTF32,
    OS_FONT_ENCODE_MAX
} OSFontEncode;

u16 OSGetFontEncode(void);
BOOL OSInitFont(OSFontHeader* font);
const char* OSGetFontTexture(const char* str, void** texOut, u32* xOut, u32* yOut, u32* widthOut);
const char* OSGetFontWidth(const char* str, u32* widthOut);

u32 OSLoadFont(OSFontHeader *, void *);
char *OSGetFontTexel(const char *, void *, s32, s32, s32 *);

#endif  // OSFONT_H
