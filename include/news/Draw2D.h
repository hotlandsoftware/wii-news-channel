#ifndef NEWS_DRAW2D_H
#define NEWS_DRAW2D_H

#include <types.h>
#include <nw4r/ut/ut_Color.h>
#include <nw4r/ut/ut_Rect.h>
#include <nw4r/math/math_types.h>

struct TPLPalette;

void Draw2D_SetupGX();
void Draw2D_SetOrtho();
void Draw2D_FillQuad(const nw4r::math::VEC3* quad, const GXColor* color);
void Draw2D_TexRect(TPLPalette* tpl, u32 index, const nw4r::ut::Rect* rect, f32 z, u32 flags);
void Draw2D_TexPos(TPLPalette* tpl, u32 index, const nw4r::math::VEC3* pos, f32 scaleX, f32 scaleY,
                   u8 flags);

void Draw2D_Tex(TPLPalette* tpl, u32 index, const Vec* pos, f32 scaleX, f32 scaleY);

void Draw2D_Line(const nw4r::math::VEC3& p0, const nw4r::math::VEC3& p1, u8 width,
                 nw4r::ut::Color c0, nw4r::ut::Color c1);
void Draw2D_SetScissor(u32 x, u32 y, u32 width, u32 height);

struct NewsTexture;
void Draw2D_Icon(u32 index, nw4r::math::VEC3* pos, f32 scaleX, f32 scaleY, u32 flags);
void Draw2D_Texture(NewsTexture* tex, const nw4r::math::VEC3* pos, f32 scale);

u32 TPL_GetWidth(TPLPalette* tpl, u32 index);
u32 TPL_GetHeight(TPLPalette* tpl, u32 index);

#endif
