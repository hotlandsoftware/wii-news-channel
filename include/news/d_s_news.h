#ifndef NEWS_D_S_NEWS_H
#define NEWS_D_S_NEWS_H

#include <types.h>
#include <nw4r/math/math_types.h>
#include <nw4r/ut/ut_Rect.h>

class GlobePin;
class NewsArticle;

// Functions of the news scene (d_s_news.cpp) used by the main and slideshow
// screens: background music, the headline list, the globe pins and the
// article view.
void Bgm_MuteMain();
void Bgm_PlayArticle();
void Bgm_PlayMain(BOOL restart);
void HeadlineList_SetScale(const f32& scale);
void HeadlineList_SetIndexAndScale(const f32& scale, s32 index);
s32 HeadlineList_Update(const f32& offsetX, const bool* dragging, f32 scale, f32 scaleDelta);
void HeadlineList_ScrollUp();
void HeadlineList_ScrollDown();
void HeadlineList_SetX(f32 x);
BOOL HeadlineList_IsAtTop();
BOOL HeadlineList_IsAtBottom();
void HeadlineList_SetScrollVel(f32 vel);
void HeadlineList_Snap();
bool HeadlineList_IsLanguagePressed();
s32* HeadlineList_GetIndexPtr();
void HeadlineList_SetIndex(const s32& index);
void GetArticleLocation(nw4r::math::VEC2* out, NewsArticle* article);
void Globe_FocusArticle(NewsArticle* article, s32 arg, f32 x, f32 y);
void Globe_ResetFocus();
void Globe_SetZoom(s32 level);
void Pins_UpdateFade();
void Pins_SetStateAll(u8 state);
void Pins_SetState(s32 category, s32 index, u8 state);
void Pins_Select(s32 category, s32 index, u8 state);
GlobePin* Pins_GetPointed(s32* category, s32* index);
BOOL Pins_IsHovered(s32 chan);
BOOL Article_Set(NewsArticle* article, const wchar_t* title, BOOL withPicture,
                 const nw4r::math::VEC2* start, const nw4r::math::VEC2* picPos, const f32* picScale,
                 const nw4r::math::VEC2& size, bool indent, f32 x, bool latest);
void Article_Layout(const nw4r::math::VEC2* pos, f32 y);
void Article_Update(f32 arg);
void Article_Arrange(f32 scale);
void Article_Reset();
void Article_SetSelection(const nw4r::math::VEC2* pos, const nw4r::math::VEC2* from, const nw4r::math::VEC2* picPos,
                          const f32* picScale);
void Article_Draw(const nw4r::math::VEC2& pos, s32 type, BOOL drawHeadline, f32 alpha, f32 bodyAlpha);
bool Article_IsBodyScrolling();
f32 Article_GetScrollOffset();
BOOL Article_IsShort();
void Article_PageUp(s32 size, const f32& offset);
void Article_PageDown(s32 size, const f32& offset);
BOOL Article_IsAtTop();
BOOL Article_IsAtBottom();
void Article_SetX(f32 x);
f32 Article_GetMaxScrollOffset();
void Article_ClampScroll();
f32 Article_ScrollTo(f32 offset, f32 dir);
BOOL Article_HitTest(const nw4r::ut::Rect* rect);
void Article_ClearHit();
f32 GetTextScale(s32 size);
bool UpdateTextSize(BOOL up, BOOL down);
BOOL Article_GetPictureRect(nw4r::ut::Rect* rect, f32 x, f32 y, f32 scale);
BOOL Article_GetZoomedPictureRect(nw4r::ut::Rect* rect);
void Article_DrawZoomedPicture(const nw4r::ut::Rect& from, const nw4r::ut::Rect& to, f32 t);
void Bgm_SetSlideshowVolume(f32 volume);
void HeadlineList_Draw(const f32& offsetX, f32 alpha, f32 headerAlpha);
void Article_SetHeight(f32 height);
void Article_ResetScroll();
void Article_ResetHeadline();
void Article_DrawHeadline(const nw4r::math::VEC2* pos, bool clip, f32 alpha);
void Article_UpdateHeadline(f32 arg);
void Article_LayoutHeadline(const nw4r::math::VEC2* pos, bool clip, f32 scroll);
void Article_ArrangeHeadline(f32 scale);
f32 Article_GetHeadlineY();
void DrawTabRect(const nw4r::ut::Rect& rect, u8 alpha, f32 z);
void DrawPointerEffect(u8 alpha, u16 height);
void SetDPDAll(s32 value);

#endif
