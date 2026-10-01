#ifndef NEWS_HEADLINE_LIST_H
#define NEWS_HEADLINE_LIST_H

#include <types.h>
#include <nw4r/math/math_types.h>
#include <nw4r/ut/ut_TextWriterBase.h>
#include <news/System.h>

class Ticker;
class IconTextButton;
class SmallTextButton;
struct Category;

// Scrolling page with the headlines of one news section, its header
// ("News at N o'clock" / "Updated hh:mm ago") and the footer buttons.
class HeadlineList {
public:
    enum Mode {
        MODE_TOP_EMPTY, // first page without headlines: only the section grid
        MODE_TOP,       // first page: headlines followed by the section grid
        MODE_SECTION,   // any other section
    };

    HeadlineList(Category* category, nw4r::ut::TextWriterBase<wchar_t>* writer,
                 nw4r::math::VEC2& pos, nw4r::math::VEC2& size, s32 pageIndex);
    ~HeadlineList();

    void SetScale(const f32& scale);
    void ResetItems();
    void Draw(f32 alpha, f32 headerAlpha, const f32& offsetX);
    s32 Update(const f32& offsetX, f32 scale, f32 scaleDelta, const bool* dragging);
    void ScrollUp();
    void ScrollDown();
    f32 GetOffset(s32 index);
    f32 GetTopButtonBottom();
    void Snap();
    void UpdateButtons(nw4r::math::VEC2 pos);
    s32 GetMaxIndex();

private:
    static f32 GetContentWidth() { return 10.0f + (GetScreenWidth() - 2 * GetSideMargin()); }

    void UpdatePosition();
    void LayoutItems();
    void UpdateTotalHeight();
    void RecalcTotalHeight();
    void Refresh();
    void JumpToIndex();
    bool IsTopPage() const { return mMode == MODE_TOP; }
    bool IsScrolledTo(s32 index);
    s32 FindIndex();
    void ResetItemsInline();
    void SnapInline();
    s32 GetMaxIndexInline();
    void CalcTotalHeight();
    void RelayoutItems();
    void ClearItemStates();

public:
    Category* mCategory;                         // at 0x00
    nw4r::ut::TextWriterBase<wchar_t>* mWriter;  // at 0x04
    Ticker* mItems;                              // at 0x08
    Ticker* mVisibleItems;                       // at 0x0C
    IconTextButton* mSectionButton;              // at 0x10
    IconTextButton* mTopButton;                  // at 0x14
    SmallTextButton* mLanguageButton;            // at 0x18
    nw4r::math::VEC2 mPos;                       // at 0x1C
    nw4r::math::VEC2 mSize;                      // at 0x24
    nw4r::math::VEC2 mOrigin;                    // at 0x2C
    nw4r::math::VEC2 mBasePos;                   // at 0x34
    f32 mScrollX;                                // at 0x3C
    f32 mScroll;                                 // at 0x40
    f32 mScrollXTarget;                          // at 0x44
    f32 mScrollTarget;                           // at 0x48
    f32 mScale;                                  // at 0x4C
    f32 mTotalHeight;                            // at 0x50
    f32 mScrollRate;                             // at 0x54
    f32 mScrollVel;                              // at 0x58
    s32 mMode;                                   // at 0x5C
    s32 mPageIndex;                              // at 0x60
    s32 mNumItems;                               // at 0x64
    s32 mNumSectionRows;                         // at 0x68
    s32 mSelected;                               // at 0x6C
    s32 mIndex;                                  // at 0x70
    s32 mNumIndices;                             // at 0x74
    bool mLanguagePressed;                       // at 0x78
    wchar_t mTitle[256];                         // at 0x7A
};

#endif
