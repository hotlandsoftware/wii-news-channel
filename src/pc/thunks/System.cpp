// Thunk for the name under which Resource.cpp and NewsArticle.cpp call
// operator new(size_t, MEMAllocator*) (src/news/System.cpp).
// See <pc/thunk.h> and docs/pc_port.md, rule R7.

#include <revolution/mem.h>
#include <pc/thunk.h>

void* operator new(size_t size, MEMAllocator* allocator);

extern "C" void* __nw__FUlP12MEMAllocator(size_t size, MEMAllocator* allocator) {
    return operator new(size, allocator);
}
