#ifndef NW4R_UT_NON_COPYABLE_H
#define NW4R_UT_NON_COPYABLE_H

#include <types.h>

namespace nw4r {
namespace ut {

class NonCopyable {
protected:
    NonCopyable() {}
    ~NonCopyable() {}

private:
    NonCopyable(const NonCopyable&);
    const NonCopyable& operator=(const NonCopyable&);
};

} // namespace ut
} // namespace nw4r

#endif
