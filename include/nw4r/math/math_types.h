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
    operator f32*() { return reinterpret_cast<f32*>(this); }
    operator const f32*() const { return reinterpret_cast<const f32*>(this); }

    // NW4R library code (lyt) is built without the VEC2/MTX34 destructors:
    // there an 8-byte VEC2 is returned in r3/r4 (Pane::GetVtxPos).
#ifndef NW4R_MATH_VEC2_NO_DTOR
    ~VEC2() {}
#endif
};

struct VEC3 : public Vec {
    VEC3() {}
    VEC3(f32 fx, f32 fy, f32 fz) {
        x = fx;
        y = fy;
        z = fz;
    }
    // Added for g3d (Task 10), as in ogws
    VEC3(const Vec& rVec) {
        x = rVec.x;
        y = rVec.y;
        z = rVec.z;
    }
    VEC3(const f32* pData) {
        x = pData[0];
        y = pData[1];
        z = pData[2];
    }
    operator Vec*() { return this; }
    operator const Vec*() const { return this; }
    // Mascot.cpp needs this destructor for its weak-destructor placement,
    // but PaneButton.cpp's temporaries show VEC3 has no destructor there.
#ifndef NW4R_MATH_VEC3_NO_DTOR
    ~VEC3() {}
#endif

    VEC3& operator+=(const VEC3& rhs);
    VEC3 operator+(const VEC3& rhs) const;

    // Added for g3d (Task 10), as in ogws (defined in math_types_g3d.h)
    f32 LenSq() const { return x * x + y * y + z * z; }
    VEC3 operator-() const { return VEC3(-x, -y, -z); }
    VEC3 operator-(const VEC3& rhs) const;
    VEC3 operator*(f32 s) const;
    VEC3 operator/(f32 s) const;
    VEC3& operator-=(const VEC3& rhs);
    VEC3& operator*=(f32 s);
    VEC3& operator/=(f32 s);
    bool operator==(const VEC3& rhs) const { return x == rhs.x && y == rhs.y && z == rhs.z; }
    bool operator!=(const VEC3& rhs) const { return x != rhs.x || y != rhs.y || z != rhs.z; }
};

inline VEC3* VEC3Add(register VEC3* pOut, register const VEC3* p1, register const VEC3* p2) {
#ifdef TARGET_PC
    pOut->x = p1->x + p2->x;
    pOut->y = p1->y + p2->y;
    pOut->z = p1->z + p2->z;
#else
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
#endif
    return pOut;
}

inline VEC3 VEC3::operator+(const VEC3& rhs) const {
    VEC3 tmp;
    VEC3Add(&tmp, this, &rhs);
    return tmp;
}

inline VEC3& VEC3::operator+=(register const VEC3& rhs) {
#ifdef TARGET_PC
    x += rhs.x;
    y += rhs.y;
    z += rhs.z;
#else
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
#endif
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
#ifndef NW4R_MATH_MTX34_NO_DTOR
    ~MTX34() {}
#endif

    operator f32*() { return a; }
    operator const f32*() const { return a; }

    // Added for g3d (Task 10), as in ogws
    typedef f32 (*MtxRef)[4];
    typedef const f32 (*MtxRefConst)[4];

    // clang-format off
    MTX34(f32 f00, f32 f01, f32 f02, f32 f03,
          f32 f10, f32 f11, f32 f12, f32 f13,
          f32 f20, f32 f21, f32 f22, f32 f23) {
        _00 = f00; _01 = f01; _02 = f02; _03 = f03;
        _10 = f10; _11 = f11; _12 = f12; _13 = f13;
        _20 = f20; _21 = f21; _22 = f22; _23 = f23;
    }
    // clang-format on

    operator MtxRef() { return mtx; }
    operator MtxRefConst() const { return mtx; }
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

    // Added for g3d (Task 10), as in ogws
    typedef f32 (*Mtx44Ref)[4];
    typedef const f32 (*Mtx44RefConst)[4];

    operator Mtx44Ref() { return mtx; }
    operator Mtx44RefConst() const { return mtx; }
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

#include <nw4r/math/math_types_g3d.h>

#endif
