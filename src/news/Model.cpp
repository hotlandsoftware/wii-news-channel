#include <news/Model.h>
#include <news/Common.h>
#include <nw4r/math/math_triangular.h>

using namespace nw4r;

extern MEMAllocator gMdlAllocator;
extern math::MTX34 gWorkMtx;

void Mtx_RotateDeg(math::MTX34* mtx, f32 x, f32 y, f32 z);
void Mtx_Translate(math::MTX34* mtx, f32 x, f32 y, f32 z);

// Referenced by other files; defined here.
extern const f32 gModelDepth = 3.0f;

Model::Model(void* brres)
    : mPos(0.0f, 0.0f, 0.0f), mRotate(0.0f, 0.0f, 0.0f), mScale(21.0f, 21.0f, 21.0f) {
    g3d::ResFile file(brres);
    file.Init();
    file.Bind();
    mResMdl = file.GetResMdl(0);
    u32 size;
    mScnMdl = g3d::ScnMdlSimple::Construct(&gMdlAllocator, &size, mResMdl, 1);
}

Model::~Model() {
    mScnMdl->Destroy();
}

void Model::Update() {}

void Model::Calc() {
    mMtx = CalcMtx(mRotate);
    mScnMdl->SetMtx(g3d::ScnObj::MTX_LOCAL, &mMtx);
}

inline void Mtx_RotateDegXZ(math::MTX34* mtx, f32 x, f32 z) {
    Mtx_RotateDeg(mtx, x, 0.0f, z);
}

math::MTX34 Model::CalcMtx(const math::VEC3& rotate) {
    math::MTX34RotXYZFIdx(&gWorkMtx, 0.0f, (256.0f / 360.0f) * rotate.y, 0.0f);
    Mtx_RotateDegXZ(&gWorkMtx, rotate.x, rotate.z);
    Mtx_Translate(&gWorkMtx, mPos.x, mPos.y, mPos.z);
    return gWorkMtx;
}

void Model::Draw() {}
