// Sized deallocation.
//
// The game replaces the global operator new/delete with versions that use its
// own heaps (src/news/System.cpp). It was written for C++98 and so defines
// only `operator delete(void*)`. gcc in C++14 and later calls the sized form,
// `operator delete(void*, size_t)`, wherever it knows the size of the object
// (every `delete p` of a complete type, every deleting destructor). Without
// the definitions below that call goes to libstdc++, which hands a pointer
// from the game's heap to free(): "free(): invalid size" on the first delete.
//
// These forward to the game's unsized operators. Like those, they are kept
// local to the executable by pc/cmake/private_symbols.ver.

#include <cstddef>

void operator delete(void* ptr, std::size_t) noexcept {
    ::operator delete(ptr);
}

void operator delete[](void* ptr, std::size_t) noexcept {
    ::operator delete[](ptr);
}
