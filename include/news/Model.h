#ifndef NEWS_MODEL_H
#define NEWS_MODEL_H

#include <types.h>
#include <nw4r/g3d/g3d_resfile.h>
#include <nw4r/math/math_types.h>

// The first model of a .brres, placed with a position and a rotation in degrees.
class Model {
public:
    Model(void* brres);
    virtual ~Model();

    void Update();
    void Calc();
    nw4r::math::MTX34 CalcMtx(nw4r::math::VEC3& rotate);
    void Draw();

    nw4r::g3d::ResMdl GetResMdl() const { return mResMdl; }
    nw4r::g3d::ScnMdlSimple* GetScnMdl() const { return mScnMdl; }
    nw4r::math::MTX34* GetMtx() { return &mMtx; }

private:
    u32 _04;
    nw4r::g3d::ResMdl mResMdl;            // at 0x08
    nw4r::g3d::ScnMdlSimple* mScnMdl;     // at 0x0C
    nw4r::math::MTX34 mMtx;               // at 0x10
    nw4r::math::VEC3 mPos;                // at 0x40
    nw4r::math::VEC3 mRotate;             // at 0x4C
    nw4r::math::VEC3 mScale;              // at 0x58
};

#endif
