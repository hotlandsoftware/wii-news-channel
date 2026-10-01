#ifndef NW4R_UT_TAG_PROCESSOR_BASE_H
#define NW4R_UT_TAG_PROCESSOR_BASE_H

#include <types.h>
#include <nw4r/ut/ut_Rect.h>

namespace nw4r {
namespace ut {

template <typename T> class TextWriterBase;

enum Operation {
    OPERATION_DEFAULT,
    OPERATION_NO_CHAR_SPACE,
    OPERATION_CHAR_SPACE,
    OPERATION_NEXT_LINE,
    OPERATION_END_DRAW,
};

template <typename T> struct PrintContext {
    TextWriterBase<T>* writer; // at 0x00
    const T* str;              // at 0x04
    f32 xOrigin;               // at 0x08
    f32 yOrigin;               // at 0x0C
    u32 flags;                 // at 0x10
};

template <typename T> class TagProcessorBase {
public:
    TagProcessorBase();
    virtual ~TagProcessorBase();                                                 // at 0x08
    virtual Operation Process(u16 code, PrintContext<T>* context);               // at 0x0C
    virtual Operation CalcRect(Rect* pRect, u16 code, PrintContext<T>* context); // at 0x10
};

} // namespace ut
} // namespace nw4r

#endif
