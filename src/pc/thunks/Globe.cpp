// Thunks for the names under which d_scene.cpp calls Globe (src/news/Globe.cpp).
// See <pc/thunk.h> and docs/pc_port.md, rule R7.

#include <news/Globe.h>
#include <pc/thunk.h>

PC_THUNK_CTOR(__ct__5GlobeFv, Globe)
PC_THUNK_DTOR(__dt__5GlobeFv, Globe)
PC_THUNK_METHOD(void, CalcScene__5GlobeFv, Globe, CalcScene)
