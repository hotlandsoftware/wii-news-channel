#ifndef NEWS_DRAW2D_H
#define NEWS_DRAW2D_H

#include <types.h>
#include <nw4r/ut/ut_Color.h>
#include <nw4r/ut/ut_Rect.h>
#include <nw4r/math/math_types.h>

struct TPLPalette;

void Draw2D_SetupGX();
void Draw2D_SetOrtho();
void Draw2D_FillQuad(const nw4r::math::VEC3* quad, const nw4r::ut::Color* color);
void Draw2D_TexRect(TPLPalette* tpl, u32 index, const nw4r::ut::Rect* rect, f32 z, u32 flags);
void Draw2D_TexPos(TPLPalette* tpl, u32 index, const nw4r::math::VEC3* pos, f32 scaleX, f32 scaleY,
                   u8 flags);

void Draw2D_Tex(TPLPalette* tpl, u32 index, const Vec* pos, f32 scaleX, f32 scaleY);

u32 TPL_GetWidth(TPLPalette* tpl, u32 index);
u32 TPL_GetHeight(TPLPalette* tpl, u32 index);

#endif
