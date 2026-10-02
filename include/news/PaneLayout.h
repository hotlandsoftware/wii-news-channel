#ifndef NEWS_PANE_LAYOUT_H
#define NEWS_PANE_LAYOUT_H

#include <types.h>

class PaneButton;

// A layout whose panes are PaneButtons (code at 0x80047B50, not decompiled
// yet). Allocated with operator new(0x434).
struct Layout;

extern "C" {
Layout* fn_80047B50(void* mem, u32 arc, const char* name, void* resAccessor, u32 arg);
void fn_80047DE8(Layout* layout, s32 flags);
void fn_80047EFC(Layout* layout);
void fn_80047F70(Layout* layout);
void fn_80048154(Layout* layout);
PaneButton* fn_80048364(Layout* layout, const char* name);
void fn_8004BD60(Layout* layout, u32 arg);
void fn_8004BFE0();
s32 fn_8004C000(const char* name, u32 button);
s32 fn_8004C13C(const char* name, u32 button);
}

#endif
