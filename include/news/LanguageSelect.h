#ifndef NEWS_LANGUAGE_SELECT_H
#define NEWS_LANGUAGE_SELECT_H

#include <types.h>
#include <nw4r/ut/ut_Rect.h>
#include <nw4r/ut/ut_TextWriterBase.h>

struct Layout;
struct LayoutButton;

// Screen that lets the user pick the language of the news when the
// downloaded data offers several.
class LanguageSelect {
public:
    struct Item {
        Item() : rect(0.0f, 0.0f, 0.0f, 0.0f) {}
        ~Item() {}

        u32 unk0;              // at 0x00
        u32 unk4;              // at 0x04
        const wchar_t* name;   // at 0x08
        f32 x;                 // at 0x0C
        f32 y;                 // at 0x10
        f32 width;             // at 0x14
        f32 height;            // at 0x18
        f32 scaleX;            // at 0x1C
        f32 scaleY;            // at 0x20
        nw4r::ut::Rect rect;   // at 0x24
        bool hover;            // at 0x34
        bool prevHover;        // at 0x35
        bool hidden;           // at 0x36
        u8 language;           // at 0x37
    };

    typedef void (LanguageSelect::*DrawFunc)();
    typedef void (LanguageSelect::*StateFunc)(Item* item);
    typedef void (LanguageSelect::*ScrollFunc)();

    LanguageSelect(u32 arc);
    ~LanguageSelect();
    void Start();
    void Draw();
    void Update(bool arg);

    void DrawList();
    void DrawConfirm();

    void StateIdle(Item* item);
    void StateList(Item* item);
    void StateConfirm(Item* item);

    void CheckInput();

    void ScrollIdle();
    void ScrollDrag();

    BOOL UpdateItems();
    s32 HitTest(Item* item);
    void SetBackEnabled(BOOL enabled);
    void Reset();

private:
    void ChangeState(StateFunc state, Item* item = NULL) {
        if (mState) {
            mStep = -1;
            (this->*mState)(item);
        }
        mState = state;
        mStep = 0;
        if (mState) {
            (this->*mState)(item);
        }
    }

    void ChangeScroll(ScrollFunc scroll) {
        if (mScroll) {
            mScrollStep = -1;
            (this->*mScroll)();
        }
        mScroll = scroll;
        mScrollStep = 0;
        if (mScroll) {
            (this->*mScroll)();
        }
    }

public:
    Layout* mLayout1;                            // at 0x00
    Layout* mLayout2;                            // at 0x04
    Layout* mActiveLayout;                       // at 0x08
    LayoutButton* mBackButton;                   // at 0x0C
    LayoutButton* mUpButton;                     // at 0x10
    LayoutButton* mDownButton;                   // at 0x14
    Item* mItems;                                // at 0x18
    Item* mSelected;                             // at 0x1C
    u32 unk20;                                   // at 0x20
    DrawFunc mDraw;                              // at 0x24
    StateFunc mState;                            // at 0x30
    ScrollFunc mScroll;                          // at 0x3C
    nw4r::ut::TextWriterBase<wchar_t> mWriter;   // at 0x48
    f32 mListX;                                  // at 0xA8
    f32 mScrollY;                                // at 0xAC
    f32 unkB0;                                   // at 0xB0
    f32 unkB4;                                   // at 0xB4
    f32 unkB8;                                   // at 0xB8
    f32 unkBC;                                   // at 0xBC
    f32 unkC0;                                   // at 0xC0
    f32 unkC4;                                   // at 0xC4
    f32 mListTop;                                // at 0xC8
    f32 mScrollTarget;                           // at 0xCC
    f32 mRowHeight;                              // at 0xD0
    f32 mDragMax;                                // at 0xD4
    f32 mDragMin;                                // at 0xD8
    f32 mDragVelocity;                           // at 0xDC
    f32 mViewTop;                                // at 0xE0
    f32 mViewBottom;                             // at 0xE4
    s32 mMaxScroll;                              // at 0xE8
    s32 mScrollIdx;                              // at 0xEC
    s32 mStep;                                   // at 0xF0
    s32 mScrollStep;                             // at 0xF4
    s32 mNumItems;                               // at 0xF8
    s32 mTimer;                                  // at 0xFC
    bool mUpPressed;                             // at 0x100
    bool mDownPressed;                           // at 0x101
    bool mDragHeld;                              // at 0x102
    bool mYesPressed;                            // at 0x103
    bool mNoPressed;                             // at 0x104
    bool mDragging;                              // at 0x105
    bool unk106;                                 // at 0x106
    bool mBackPressed;                           // at 0x107
};

#endif
