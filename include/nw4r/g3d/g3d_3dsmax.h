#ifndef NW4R_G3D_3DSMAX_H
#define NW4R_G3D_3DSMAX_H
#include <types.h>
#include <macros.h>

#include <nw4r/g3d/res/g3d_resanmtexsrt.h>

#include <nw4r/math/math_types.h>
#include <nw4r/math/math_arithmetic.h>
#include <nw4r/math/math_triangular.h>
#include <nw4r/math/math_geometry.h>

namespace nw4r {
namespace g3d {
namespace detail {
namespace dcc {

bool CalcTexMtx_3dsmax(math::MTX34* pMtx, bool set, const TexSrt& rSrt,
                       TexSrt::Flag flag);

} // namespace dcc
} // namespace detail
} // namespace g3d
} // namespace nw4r

#endif
