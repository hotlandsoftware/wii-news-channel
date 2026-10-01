#ifndef NW4R_UT_LIST_H
#define NW4R_UT_LIST_H

#include <types.h>

namespace nw4r {
namespace ut {

struct List {
    void* headObject; // at 0x0
    void* tailObject; // at 0x4
    u16 numObjects;   // at 0x8
    u16 offset;       // at 0xA
};

void* List_GetNext(const List* list, const void* object);
void* List_GetNth(const List* list, u16 index);

} // namespace ut
} // namespace nw4r

#endif
