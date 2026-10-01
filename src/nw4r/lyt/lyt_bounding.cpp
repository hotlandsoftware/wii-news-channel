#include <nw4r/lyt/lyt_bounding.h>

#include <nw4r/lyt/lyt_common.h>
#include <nw4r/lyt/lyt_drawInfo.h>

namespace nw4r {
namespace lyt {

NW4R_UT_GET_DERIVED_RUNTIME_TYPEINFO(Bounding, Pane);

Bounding::Bounding(const res::Bounding* pBlock, const ResBlockSet&) : Pane(pBlock) {}

Bounding::~Bounding() {}

// No debug drawing in this revision.
void Bounding::DrawSelf(const DrawInfo&) {}

} // namespace lyt
} // namespace nw4r
