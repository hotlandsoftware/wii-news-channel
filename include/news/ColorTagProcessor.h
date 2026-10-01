#ifndef NEWS_COLOR_TAG_PROCESSOR_H
#define NEWS_COLOR_TAG_PROCESSOR_H

#include <types.h>
#include <nw4r/ut/ut_Color.h>
#include <nw4r/ut/ut_TagProcessorBase.h>

// Handles the colour control codes embedded in layout strings:
// 0x01 starts red text, 0x02 starts blue text, 0x09 restores the previous colour.
class ColorTagProcessor : public nw4r::ut::TagProcessorBase<wchar_t> {
public:
    ColorTagProcessor();
    virtual ~ColorTagProcessor();

    virtual nw4r::ut::Operation Process(u16 code, nw4r::ut::PrintContext<wchar_t>* context);
    virtual nw4r::ut::Operation CalcRect(nw4r::ut::Rect* pRect, u16 code,
                                         nw4r::ut::PrintContext<wchar_t>* context);

private:
    nw4r::ut::Color mSavedColor; // at 0x4
};

#endif
