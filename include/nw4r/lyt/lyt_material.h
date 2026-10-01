#ifndef NW4R_LYT_MATERIAL_H
#define NW4R_LYT_MATERIAL_H

#include <types.h>

namespace nw4r {
namespace lyt {

// Colour element indices for Material::SetColorElement.
enum {
    ANIMTARGET_MATCOLOR_MATR,
    ANIMTARGET_MATCOLOR_MATG,
    ANIMTARGET_MATCOLOR_MATB,
    ANIMTARGET_MATCOLOR_MATA,
    ANIMTARGET_MATCOLOR_TEV0R,
    ANIMTARGET_MATCOLOR_TEV0G,
    ANIMTARGET_MATCOLOR_TEV0B,
    ANIMTARGET_MATCOLOR_TEV0A,
    ANIMTARGET_MATCOLOR_TEV1R,
    ANIMTARGET_MATCOLOR_TEV1G,
    ANIMTARGET_MATCOLOR_TEV1B,
    ANIMTARGET_MATCOLOR_TEV1A,
};

class Material {
public:
    void SetColorElement(u32 colorType, s16 value);
};

} // namespace lyt
} // namespace nw4r

#endif
