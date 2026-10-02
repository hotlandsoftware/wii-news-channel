#ifndef NW4R_EF_MEMORY_MANAGER_DECL_H
#define NW4R_EF_MEMORY_MANAGER_DECL_H
#include <nw4r/types_nw4r.h>

#include <nw4r/ef/ef_memorymanager.h>

// Declaration-only view of the default memory manager for users of the
// library (the News Channel game code). In this DOL its constructor is out of
// line (0x800AC4C4). The library itself defines the class in
// ef_memorymanagerimpl.h; never include both headers in one file.

namespace nw4r {
namespace ef {

class MemoryManager : public MemoryManagerBase {
public:
    // The game's symbol is __ct__Q34nw4r2ef13MemoryManagerFPvUlUlUlUlUl
    MemoryManager(void* pStartAddr, u32 size, u32 maxEffect, u32 maxEmitter,
                  u32 maxParticleManager, u32 maxParticle);

    virtual ~MemoryManager();

    virtual void GarbageCollection();

    virtual Effect* AllocEffect();
    virtual void FreeEffect(void* pObject);
    virtual u32 GetNumAllocEffect() const;
    virtual u32 GetNumActiveEffect() const;
    virtual u32 GetNumFreeEffect() const;

    virtual Emitter* AllocEmitter();
    virtual void FreeEmitter(void* pObject);
    virtual u32 GetNumAllocEmitter() const;
    virtual u32 GetNumActiveEmitter() const;
    virtual u32 GetNumFreeEmitter() const;

    virtual ParticleManager* AllocParticleManager();
    virtual void FreeParticleManager(void* pObject);
    virtual u32 GetNumAllocParticleManager() const;
    virtual u32 GetNumActiveParticleManager() const;
    virtual u32 GetNumFreeParticleManager() const;

    virtual Particle* AllocParticle();
    virtual void FreeParticle(void* pObject);
    virtual u32 GetNumAllocParticle() const;
    virtual u32 GetNumActiveParticle() const;
    virtual u32 GetNumFreeParticle() const;

    virtual void* AllocHeap(u32 size);
    virtual void FreeHeap(void* pPtr);

private:
    u8 mImpl[0x4C - 0x4]; // at 0x4
};

} // namespace ef
} // namespace nw4r

#endif
