#ifndef NW4R_LYT_BOUNDING_H
#define NW4R_LYT_BOUNDING_H

#include <types.h>
#include <nw4r/lyt/lyt_pane.h>

namespace nw4r {
namespace lyt {

class Bounding : public Pane {
public:
    Bounding(const res::Bounding* pBlock, const ResBlockSet& resBlockSet);

    virtual ~Bounding();                              // at 0x08
    NW4R_UT_RUNTIME_TYPEINFO;                         // at 0x0C
    virtual void DrawSelf(const DrawInfo& drawInfo);  // at 0x18
};

} // namespace lyt
} // namespace nw4r

#endif
