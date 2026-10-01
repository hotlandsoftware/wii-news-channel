#include <nw4r/g3d.h>

namespace nw4r {
namespace g3d {

NW4R_G3D_RTTI_DEF(G3dObj);

G3dObj::~G3dObj() {
    Dealloc(mpHeap, this);
}

void G3dObj::Destroy() {
    G3dObj* pParent = GetParent();

    if (pParent != NULL) {
        pParent->G3dProc(G3DPROC_CHILD_DETACHED, 0, this);
    }

    delete this;
}

// Not in the DOL (dead-stripped at link time). As ogws's DECOMP_FORCEACTIVE:
// a late reference to G3dObj::IsDerivedFrom so that it is emitted before
// GetTypeName/GetTypeObj, as in the original object.
void g3d_obj_cpp_ForceActive(G3dObj* pObj, G3dObj::TypeObj type) {
    pObj->G3dObj::IsDerivedFrom(type);
}

} // namespace g3d
} // namespace nw4r
