#ifndef NW4R_UT_RUNTIME_TYPE_INFO_H
#define NW4R_UT_RUNTIME_TYPE_INFO_H

#include <types.h>

// Declares an inline GetRuntimeTypeInfo override and the static typeInfo member.
#define NW4R_UT_RUNTIME_TYPEINFO                                                                   \
    virtual const nw4r::ut::detail::RuntimeTypeInfo* GetRuntimeTypeInfo() const {                 \
        return &typeInfo;                                                                          \
    }                                                                                              \
    static const nw4r::ut::detail::RuntimeTypeInfo typeInfo

#define NW4R_UT_GET_RUNTIME_TYPEINFO(T) const nw4r::ut::detail::RuntimeTypeInfo T::typeInfo(NULL);

#define NW4R_UT_GET_DERIVED_RUNTIME_TYPEINFO(T, D)                                                 \
    const nw4r::ut::detail::RuntimeTypeInfo T::typeInfo(&D::typeInfo);

// ogws-style macros (added for the ut streams, Task 14)
#define NW4R_UT_RTTI_DECL(T)                                                                       \
    virtual const nw4r::ut::detail::RuntimeTypeInfo* GetRuntimeTypeInfo() const {                 \
        return &typeInfo;                                                                          \
    }                                                                                              \
                                                                                                   \
    static nw4r::ut::detail::RuntimeTypeInfo typeInfo;

#define NW4R_UT_RTTI_DEF_BASE(T) nw4r::ut::detail::RuntimeTypeInfo T::typeInfo(NULL)

#define NW4R_UT_RTTI_DEF_DERIVED(T, BASE)                                                          \
    nw4r::ut::detail::RuntimeTypeInfo T::typeInfo(&BASE::typeInfo)

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
