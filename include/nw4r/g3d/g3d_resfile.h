#ifndef NW4R_G3D_RESFILE_H
#define NW4R_G3D_RESFILE_H

#include <types.h>
#include <revolution/mem.h>
#include <nw4r/math/math_types.h>

namespace nw4r {
namespace g3d {

struct ResMdlData;
struct ResFileData;

class ResMdl;

template <typename T>
class ResCommon {
public:
    ResCommon(void* data) : mpData(static_cast<T*>(data)) {}

protected:
    T* mpData; // at 0x0
};

class ResFile : public ResCommon<ResFileData> {
public:
    ResFile(void* data = NULL) : ResCommon<ResFileData>(data) {}

    void Init();
    bool Bind(ResFile file);
    ResMdl GetResMdl(int idx) const;
};

class ResMdl : public ResCommon<ResMdlData> {
public:
    ResMdl(void* data = NULL) : ResCommon<ResMdlData>(data) {}
};

class G3dObj {
public:
    void Destroy();
};

class ScnObj : public G3dObj {
public:
    enum ScnObjMtxType {
        MTX_LOCAL,
        MTX_WORLD,
        MTX_VIEW,
    };

    bool SetMtx(ScnObjMtxType type, const math::MTX34* mtx);
};

class ScnMdlSimple : public ScnObj {
public:
    static ScnMdlSimple* Construct(MEMAllocator* allocator, u32* size, ResMdl mdl, int numView);
};

} // namespace g3d
} // namespace nw4r

#endif
