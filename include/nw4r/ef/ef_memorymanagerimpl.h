#ifndef NW4R_EF_MEMORY_MANAGER_IMPL_H
#define NW4R_EF_MEMORY_MANAGER_IMPL_H
#include <nw4r/types_nw4r.h>

#include <nw4r/ef/ef_effect.h>
#include <nw4r/ef/ef_effectsystem.h>
#include <nw4r/ef/ef_emitter.h>
#include <nw4r/ef/ef_memorymanager.h>
#include <nw4r/ef/ef_memorymanagerconfig.h>
#include <nw4r/ef/ef_memorymanagertmp.h>
#include <nw4r/ef/ef_particle.h>
#include <nw4r/ef/ef_particlemanager.h>
#include <nw4r/ut.h>

NW4R_EF_MEMORY_MANAGER_NAMESPACE_OPEN;

class NW4R_EF_MEMORY_MANAGER_CLASS : public ::nw4r::ef::MemoryManagerBase {
    NW4R_EF_MEMORY_MANAGER_MEMBER_ACCESS;
    struct MemInfo {
        MemInfo* prev;      // at 0x0
        MemInfo* next;      // at 0x4
        MemInfo* chainPrev; // at 0x8
        MemInfo* chainNext; // at 0xC
        u32 size;           // at 0x10
        bool active;        // at 0x14
        u8 PADDING_0x15[3]; // at 0x15
    };

    NW4R_EF_MEMORY_MANAGER_MEMBER_ACCESS;
    int mLeastEffect;     // at 0x4
    int mMaxEffect;       // at 0x8
    TEffectOM* mEffectOM; // at 0xC

    int mLeastEmitter;      // at 0x10
    int mMaxEmitter;        // at 0x14
    TEmitterOM* mEmitterOM; // at 0x18

    int mLeastParticleManager;              // at 0x1C
    int mMaxParticleManager;                // at 0x20
    TParticleManagerOM* mParticleManagerOM; // at 0x24

    int mLeastParticle;       // at 0x28
    int mMaxParticle;         // at 0x2C
    TParticleOM* mParticleOM; // at 0x30

    void* mHeapStartAddr; // at 0x34
    void* mHeapEndAddr;   // at 0x38

    MemInfo* mActiveMem;   // at 0x3C
    MemInfo* mFreeMem;     // at 0x40
    MemInfo* mFreeMemTail; // at 0x44
    MemInfo* mAllChain;    // at 0x48

public:
    static u32 CalcMemorySize(u16 maxEffect, u16 maxEmitter,
                              u16 maxParticleManager, u16 maxParticle,
                              u32 maxGroupID) {
        // clang-format off
        return ROUND_UP(sizeof(MemInfo), 32) + ::nw4r::ut::RoundUp<u32>(maxEffect * sizeof(TEffect) + 32, 32) +
               ROUND_UP(sizeof(MemInfo), 32) + ::nw4r::ut::RoundUp<u32>(maxEmitter * sizeof(TEmitter) + 32, 32) +
               ROUND_UP(sizeof(MemInfo), 32) + ::nw4r::ut::RoundUp<u32>(maxParticleManager * sizeof(TParticleManager) + 32, 32) +
               ROUND_UP(sizeof(MemInfo), 32) + ::nw4r::ut::RoundUp<u32>(maxParticle * sizeof(TParticle) + 32, 32) +
               ROUND_UP(sizeof(MemInfo), 32) + ::nw4r::ut::RoundUp<u32>(maxGroupID * sizeof(::nw4r::ef::ActivityList) + 32, 32) +
               ROUND_UP(sizeof(MemInfo), 32) + ROUND_UP(sizeof(TEffectOM), 32) +
               ROUND_UP(sizeof(MemInfo), 32) + ROUND_UP(sizeof(TEmitterOM), 32) +
               ROUND_UP(sizeof(MemInfo), 32) + ROUND_UP(sizeof(TParticleManagerOM), 32) +
               ROUND_UP(sizeof(MemInfo), 32) + ROUND_UP(sizeof(TParticleOM), 32);
        // clang-format on
    }

    // Older revision (News Channel): the memory manager is a regular library
    // class (ef_memorymanager.cpp), and its constructor takes u32 counts.
    NW4R_EF_MEMORY_MANAGER_CLASS(void* pStartAddr, u32 size, u32 maxEffect,
                                 u32 maxEmitter, u32 maxParticleManager,
                                 u32 maxParticle);

    virtual ~NW4R_EF_MEMORY_MANAGER_CLASS(); // at 0x8

    virtual void GarbageCollection(); // at 0xC

    virtual TEffect* AllocEffect();            // at 0x10
    virtual void FreeEffect(void* pObj);       // at 0x14
    virtual u32 GetNumAllocEffect() const;     // at 0x18
    virtual u32 GetNumActiveEffect() const;    // at 0x1C
    virtual u32 GetNumFreeEffect() const;      // at 0x20

    virtual TEmitter* AllocEmitter();          // at 0x24
    virtual void FreeEmitter(void* pObj);      // at 0x28
    virtual u32 GetNumAllocEmitter() const;    // at 0x2C
    virtual u32 GetNumActiveEmitter() const;   // at 0x30
    virtual u32 GetNumFreeEmitter() const;     // at 0x34

    virtual TParticleManager* AllocParticleManager();  // at 0x38
    virtual void FreeParticleManager(void* pObj);      // at 0x3C
    virtual u32 GetNumAllocParticleManager() const;    // at 0x40
    virtual u32 GetNumActiveParticleManager() const;   // at 0x44
    virtual u32 GetNumFreeParticleManager() const;     // at 0x48

    virtual TParticle* AllocParticle();        // at 0x4C
    virtual void FreeParticle(void* pObj);     // at 0x50
    virtual u32 GetNumAllocParticle() const;   // at 0x54
    virtual u32 GetNumActiveParticle() const;  // at 0x58
    virtual u32 GetNumFreeParticle() const;    // at 0x5C

    virtual void* AllocHeap(u32 size); // at 0x60
    virtual void FreeHeap(void* pPtr); // at 0x64
};

NW4R_EF_MEMORY_MANAGER_NAMESPACE_CLOSE;

#endif
