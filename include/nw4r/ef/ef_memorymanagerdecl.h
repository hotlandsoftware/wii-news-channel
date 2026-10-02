#ifndef NW4R_EF_MEMORY_MANAGER_DECL_H
#define NW4R_EF_MEMORY_MANAGER_DECL_H
#include <nw4r/types_nw4r.h>

// The default memory manager, for users of the library (the News Channel game
// code). In this older revision MemoryManager is a regular library class
// (ef_memorymanager.cpp; its constructor is at 0x800AC4C4), so the library
// header only declares its members.
#include <nw4r/ef/ef_memorymanagerconfig.h>
#include <nw4r/ef/ef_memorymanagerimpl.h>
#include <nw4r/ef/ef_memorymanagertmp.h>

#endif
