#include <nw4r/g3d.h>

namespace nw4r {
namespace g3d {
namespace detail {
namespace workmem {
namespace {

union {
    u8 mem[WORKMEM_SIZE]; // at 0x0
    struct {
        math::_VEC3 tmpScale[WORKMEM_NUMTMPSCALE]; // at 0x0
        u32 mtxID[WORKMEM_NUMMTXID];               // at 0x6000
    };
    u8 byteCode[WORKMEM_NUMBYTECODE];                 // at 0x0
    MdlZ mdlZ[WORKMEM_NUMMDLZ];                       // at 0x0
    math::_MTX34 skinningMtx[WORKMEM_NUMSKINNINGMTX]; // at 0x0
    math::_MTX34 bbMtx[WORKMEM_NUMBBMTX];             // at 0x0
    u8 shpAnmResultBuf[WORKMEM_NUMSHPANMRESULT];      // at 0x0
} sTemp ALIGN(128);

} // namespace

math::VEC3* GetScaleTemporary() {
#ifdef TARGET_PC
    // gcc does not let static_cast convert the array itself to a pointer to
    // the derived class; the pointer to its first element is the same thing.
    return static_cast<math::VEC3*>(&sTemp.tmpScale[0]);
#else
    return static_cast<math::VEC3*>(sTemp.tmpScale);
#endif
}

u32* GetMtxIDTemporary() {
    return sTemp.mtxID;
}

MdlZ* GetMdlZTemporary() {
    return sTemp.mdlZ;
}

math::MTX34* GetSkinningMtxTemporary() {
    return reinterpret_cast<math::MTX34*>(sTemp.skinningMtx);
}

math::MTX34* GetBillboardMtxTemporary() {
    return reinterpret_cast<math::MTX34*>(sTemp.bbMtx);
}

ShpAnmResultBuf* GetShpAnmResultBufTemporary() {
    return reinterpret_cast<ShpAnmResultBuf*>(sTemp.shpAnmResultBuf);
}

} // namespace workmem
} // namespace detail
} // namespace g3d
} // namespace nw4r
