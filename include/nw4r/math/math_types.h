#ifndef NW4R_MATH_TYPES_H
#define NW4R_MATH_TYPES_H

#include <types.h>
#include <revolution/mtx.h>

namespace nw4r {
namespace math {

struct VEC2 : public Vec2 {
    VEC2() {}
    VEC2(f32 fx, f32 fy) {
        x = fx;
        y = fy;
    }
    ~VEC2() {}
};

struct VEC3 : public Vec {
    VEC3() {}
    VEC3(f32 fx, f32 fy, f32 fz) {
        x = fx;
        y = fy;
        z = fz;
    }
    // Mascot.cpp needs this destructor for its weak-destructor placement,
    // but PaneButton.cpp's temporaries show VEC3 has no destructor there.
#ifndef NW4R_MATH_VEC3_NO_DTOR
    ~VEC3() {}
#endif

    VEC3& operator+=(const VEC3& rhs);
    VEC3 operator+(const VEC3& rhs) const;
};

inline VEC3* VEC3Add(register VEC3* pOut, register const VEC3* p1, register const VEC3* p2) {
    register f32 a, b, c;
    asm {
        psq_l a, 0(p1), 0, 0
        psq_l b, 0(p2), 0, 0
        ps_add c, a, b
        psq_l a, 8(p1), 1, 0
        psq_l b, 8(p2), 1, 0
        psq_st c, 0(pOut), 0, 0
        ps_add c, a, b
        psq_st c, 8(pOut), 1, 0
    }
    return pOut;
}

inline VEC3 VEC3::operator+(const VEC3& rhs) const {
    VEC3 tmp;
    VEC3Add(&tmp, this, &rhs);
    return tmp;
}

inline VEC3& VEC3::operator+=(register const VEC3& rhs) {
    register VEC3* self = this;
    register f32 b, a;
    asm {
        psq_l a, 0(self), 0, 0
        psq_l b, 0(rhs), 0, 0
        ps_add b, a, b
        psq_l a, 8(self), 1, 0
        psq_st b, 0(self), 0, 0
        psq_l b, 8(rhs), 1, 0
        ps_add b, a, b
        psq_st b, 8(self), 1, 0
    }
    return *this;
}

struct MTX34 {
    union {
        struct {
            f32 _00, _01, _02, _03;
            f32 _10, _11, _12, _13;
            f32 _20, _21, _22, _23;
        };
        f32 m[3][4];
        f32 a[12];
        Mtx mtx;
    };

    MTX34() {}
    ~MTX34() {}

    operator f32*() { return a; }
    operator const f32*() const { return a; }
};

struct VEC4 {
    f32 x, y, z, w;
};

struct MTX44 {
    union {
        struct {
            f32 _00, _01, _02, _03;
            f32 _10, _11, _12, _13;
            f32 _20, _21, _22, _23;
            f32 _30, _31, _32, _33;
        };
        f32 m[4][4];
        f32 a[16];
        Mtx44 mtx;
    };

    operator f32*() { return a; }
    operator const f32*() const { return a; }
};

// Inline MTX34 helpers (added for lyt, Task 15; as in tp nw4hbm math/types.h)
inline MTX34* MTX34Mult(MTX34* pOut, const MTX34* p1, const MTX34* p2) {
    PSMTXConcat(p1->mtx, p2->mtx, pOut->mtx);
    return pOut;
}

inline MTX34* MTX34Copy(MTX34* pOut, const MTX34* p) {
    PSMTXCopy(p->mtx, pOut->mtx);
    return pOut;
}

inline MTX34* MTX34Identity(MTX34* pOut) {
    PSMTXIdentity(pOut->mtx);
    return pOut;
}

MTX44* MTX44Identity(MTX44* pOut);
VEC4* VEC3Transform(VEC4* pOut, const MTX44* pM, const VEC3* pV);

} // namespace math
} // namespace nw4r

#endif
