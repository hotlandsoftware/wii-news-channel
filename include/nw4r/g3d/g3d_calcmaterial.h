#ifndef NW4R_G3D_CALC_MATERIAL_H
#define NW4R_G3D_CALC_MATERIAL_H
#include <types.h>
#include <macros.h>

#include <nw4r/g3d/res/g3d_resmdl.h>

namespace nw4r {
namespace g3d {

// Forward declarations
class AnmObjTexPat;
class AnmObjTexSrt;
class AnmObjMatClr;

void CalcMaterialDirectly(ResMdl mdl, AnmObjTexPat* pAnmTexPat,
                          AnmObjTexSrt* pAnmTexSrt, AnmObjMatClr* pAnmMatClr);

} // namespace g3d
} // namespace nw4r

#endif
