#ifndef NW4R_UT_RUNTIME_TYPE_INFO_H
#define NW4R_UT_RUNTIME_TYPE_INFO_H

#include <types.h>

namespace nw4r {
namespace ut {
namespace detail {

struct RuntimeTypeInfo {
    explicit RuntimeTypeInfo(const RuntimeTypeInfo* parent) : mParentTypeInfo(parent) {}

    bool IsDerivedFrom(const RuntimeTypeInfo* typeInfo) const {
        for (const RuntimeTypeInfo* self = this; self != NULL; self = self->mParentTypeInfo) {
            if (self == typeInfo) {
                return true;
            }
        }
        return false;
    }

    const RuntimeTypeInfo* mParentTypeInfo; // at 0x0
};

template <typename T> inline const RuntimeTypeInfo* GetTypeInfoFromPtr_(T* ptr) {
    return &ptr->typeInfo;
}

} // namespace detail

template <typename TDerived, typename TBase> inline TDerived DynamicCast(TBase* ptr) {
    const detail::RuntimeTypeInfo* typeInfo = detail::GetTypeInfoFromPtr_(static_cast<TDerived>(NULL));
    if (ptr->GetRuntimeTypeInfo()->IsDerivedFrom(typeInfo)) {
        return static_cast<TDerived>(ptr);
    }
    return NULL;
}

} // namespace ut
} // namespace nw4r

#endif
