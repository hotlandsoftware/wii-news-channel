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

struct Link {
    void* prevObject; // at 0x0
    void* nextObject; // at 0x4
};

void List_Init(List* list, u16 offset);
void List_Append(List* list, void* object);
void List_Prepend(List* list, void* object);
void List_Insert(List* list, void* target, void* object);
void List_Remove(List* list, void* object);
void* List_GetNext(const List* list, const void* object);
void* List_GetPrev(const List* list, const void* object);
void* List_GetNth(const List* list, u16 index);

inline void* List_GetFirst(const List* list) { return List_GetNext(list, NULL); }
inline void* List_GetLast(const List* list) { return List_GetPrev(list, NULL); }
inline u16 List_GetSize(const List* list) { return list->numObjects; }

#define NW4R_UT_LIST_GET_LINK(LIST, OBJ) reinterpret_cast<nw4r::ut::Link*>((u8*)(OBJ) + (LIST).offset)

} // namespace ut
} // namespace nw4r

#endif
