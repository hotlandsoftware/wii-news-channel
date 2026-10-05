// Sampling of nw4r::g3d animation resources: the functions of the library's
// g3d_resanm.cpp, g3d_resanmclr.cpp, g3d_resanmtexsrt.cpp and
// g3d_resanmvis.cpp.
//
// None of them is in the News Channel DOL. The channel plays no g3d animation
// (earth.brres has none), so CodeWarrior's linker removed these functions
// together with the virtual functions that call them (AnmObjMatClrRes::
// GetResult() and its relatives): config/HAGE/symbols.txt has no GetAnmResult,
// GetResKeyFrameAnmResult or GetResColorAnmResult, and there is no
// disassembly to derive them from. gcc keeps every virtual function, so the
// PC build needs the definitions (docs/pc_port.md, R9).
//
// They are written against this repository's own headers
// (include/nw4r/g3d/res/g3d_resanm*.h), with the decompilation of a later
// revision of the same library (ogws, src/nw4r/g3d/res) as the reference for
// the algorithm. If the main branch ever gets these files as decompiled
// sources, delete this file and add them to pc/ported/nw4r_g3d.txt.
//
// The resources themselves have no byte-order converter yet
// (src/pc/endian/fmt_g3d.cpp), so these functions expect host-order data,
// which is what the self-test (selftest_g3d_res.cpp) builds for them.

#include <cmath>

#include <nw4r/g3d.h>
#include <nw4r/math.h>
#include <nw4r/ut.h>

namespace nw4r {
namespace g3d {
namespace detail {
namespace {

// The cubic f with f(0) = v0, f(d) = v1, f'(0) = t0, f'(d) = t1, at p.
inline f32 HermiteInterpolation(f32 v0, f32 t0, f32 v1, f32 t1, f32 p, f32 d) {
    f32 invd = math::FInv(d);

    f32 s = p * invd;   // p / d
    f32 s_1 = s - 1.0f; // (p - d) / d

    return v0 + s * (s * ((2.0f * s - 3.0f) * (v0 - v1))) + p * s_1 * (s_1 * t0 + s * t1);
}

// a at ratio 0, b at ratio 0x8000.
inline u8 LinearInterpColorElem(u8 a, u8 b, s16 ratio) {
    return a + ((b - a) * ratio >> 15);
}

} // namespace

f32 GetResKeyFrameAnmResult(const ResKeyFrameAnmData* pData, f32 frame) {
    const ResKeyFrameData& rFirst = pData->keyFrames[0];
    const ResKeyFrameData& rLast = pData->keyFrames[pData->numKeyFrame - 1];

    if (frame <= rFirst.frame) {
        return rFirst.value;
    }

    if (rLast.frame <= frame) {
        return rLast.value;
    }

    // Guess the key from the frame, then walk to the right one
    f32 frameOffset = frame - rFirst.frame;
    f32 numKeyFrame = math::U16ToF32(pData->numKeyFrame);

    f32 fEstimate = frameOffset * numKeyFrame * pData->invKeyFrameRange;
    u16 iEstimate = math::F32ToU16(fEstimate);

    const ResKeyFrameData* pLeft = &pData->keyFrames[iEstimate];

    if (frame < pLeft->frame) {
        do {
            pLeft--;
        } while (frame < pLeft->frame);
    } else {
        do {
            pLeft++;
        } while (pLeft->frame <= frame);

        pLeft--;
    }

    if (pLeft->frame == frame) {
        return pLeft->value;
    }

    const ResKeyFrameData* pRight = pLeft + 1;
    f32 curFrameDelta = frame - pLeft->frame;
    f32 keyFrameDelta = pRight->frame - pLeft->frame;

    return HermiteInterpolation(pLeft->value, pLeft->slope, pRight->value, pRight->slope, curFrameDelta,
                                keyFrameDelta);
}

u32 GetResColorAnmResult(const ResColorAnmFramesData* pData, f32 frame) {
    const u32* pColorArray = pData->frameColors;

    f32 intPart;
    f32 fracPart = std::modf(frame, &intPart);
    int intFrame = static_cast<int>(intPart);

    if (fracPart == 0.0f) {
        return pColorArray[intFrame];
    }

    // ut::Color and u32 convert by value on PC: 0xRRGGBBAA either way
    ut::Color left(pColorArray[intFrame]);
    ut::Color right(pColorArray[intFrame + 1]);

    f32 biasedRatio = 32768 * fracPart;
    s16 fpRatio = math::F32ToS16(biasedRatio);

    return ut::Color(LinearInterpColorElem(left.r, right.r, fpRatio), LinearInterpColorElem(left.g, right.g, fpRatio),
                     LinearInterpColorElem(left.b, right.b, fpRatio),
                     LinearInterpColorElem(left.a, right.a, fpRatio));
}

} // namespace detail

/******************************************************************************
 *
 * ResAnmClr
 *
 ******************************************************************************/
void ResAnmClr::GetAnmResult(ClrAnmResult* pResult, u32 idx, f32 frame) const {
    const ResAnmClrMatData* pMatData = GetMatAnm(idx);
    const ResAnmClrAnmData* pAnmData = pMatData->anms;
    const ResAnmClrInfoData& rInfoData = ref().info;

    u32 flags = pMatData->flags;
    pResult->bRgbaExist = 0;

    if (flags == 0) {
        return;
    }

    f32 clippedFrame = detail::ClipFrame(rInfoData, frame);

    for (int i = 0; i < ClrAnmResult::CLA_MAX; flags >>= ResAnmClrMatData::NUM_OF_FLAGS, i++) {
        if (!(flags & ResAnmClrMatData::FLAG_ANM_EXISTS)) {
            continue;
        }

        bool constant = flags & ResAnmClrMatData::FLAG_ANM_CONSTANT;

        pResult->bRgbaExist |= 1 << i;
        pResult->rgbaMask[i] = pAnmData->mask;
        pResult->rgba[i] = detail::GetResColorAnmResult(&pAnmData->color, clippedFrame, constant);

        pAnmData++;
    }
}

/******************************************************************************
 *
 * ResAnmTexSrt
 *
 ******************************************************************************/
namespace {

inline u32 MakeResult(TexSrt* pSrt, const ResAnmTexSrtTexData* pTexData, f32 frame) {
    int anmIdx = 0;
    u32 flags = pTexData->flags;
    TexSrt& rSrt = *pSrt;

    if (!(flags & ResAnmTexSrtTexData::FLAG_SCALE_ONE)) {
        bool suConstant = flags & ResAnmTexSrtTexData::FLAG_SCALE_U_CONST;

        rSrt.Su = detail::GetResAnmResult(&pTexData->anms[anmIdx++], frame, suConstant);

        if (flags & ResAnmTexSrtTexData::FLAG_SCALE_UNIFORM) {
            rSrt.Sv = rSrt.Su;
        } else {
            bool svConstant = flags & ResAnmTexSrtTexData::FLAG_SCALE_V_CONST;

            rSrt.Sv = detail::GetResAnmResult(&pTexData->anms[anmIdx++], frame, svConstant);
        }
    } else {
        rSrt.Su = 1.0f;
        rSrt.Sv = 1.0f;
    }

    if (!(flags & ResAnmTexSrtTexData::FLAG_ROT_ZERO)) {
        bool rConstant = flags & ResAnmTexSrtTexData::FLAG_ROT_CONST;

        rSrt.R = detail::GetResAnmResult(&pTexData->anms[anmIdx++], frame, rConstant);
    } else {
        rSrt.R = 0.0f;
    }

    if (!(flags & ResAnmTexSrtTexData::FLAG_TRANS_ZERO)) {
        bool tuConstant = flags & ResAnmTexSrtTexData::FLAG_TRANS_U_CONST;
        bool tvConstant = flags & ResAnmTexSrtTexData::FLAG_TRANS_V_CONST;

        rSrt.Tu = detail::GetResAnmResult(&pTexData->anms[anmIdx++], frame, tuConstant);
        rSrt.Tv = detail::GetResAnmResult(&pTexData->anms[anmIdx++], frame, tvConstant);
    } else {
        rSrt.Tu = 0.0f;
        rSrt.Tv = 0.0f;
    }

    return flags & (ResAnmTexSrtTexData::FLAG_ANM_EXISTS | ResAnmTexSrtTexData::FLAG_SCALE_ONE |
                    ResAnmTexSrtTexData::FLAG_ROT_ZERO | ResAnmTexSrtTexData::FLAG_TRANS_ZERO);
}

} // namespace

void ResAnmTexSrt::GetAnmResult(TexSrtAnmResult* pResult, u32 idx, f32 frame) const {
    const ResAnmTexSrtMatData* pMatData = GetMatAnm(idx);
    const s32* pToTexData = pMatData->toResAnmTexSrtTexData;
    u32 flags = pMatData->flags;
    u32 indFlags = pMatData->indFlags;

    pResult->flags = 0;
    pResult->indFlags = 0;
    pResult->texMtxMode = ref().info.texMtxMode;

    int index;

    // Texture matrices, then the indirect matrices; one offset per animated one
    for (index = 0; flags != 0; flags >>= ResAnmTexSrtMatData::NUM_OF_FLAGS, index++) {
        if (!(flags & ResAnmTexSrtMatData::FLAG_ANM_EXISTS)) {
            continue;
        }

        const ResAnmTexSrtTexData* pTexData =
            reinterpret_cast<const ResAnmTexSrtTexData*>(ut::AddOffsetToPtr(pMatData, *pToTexData++));

        u32 result = MakeResult(&pResult->srt[index], pTexData, frame);
        pResult->flags |= result << (index * TexSrtAnmResult::NUM_OF_FLAGS);
    }

    for (index = 0; indFlags != 0; indFlags >>= ResAnmTexSrtMatData::NUM_OF_FLAGS, index++) {
        if (!(indFlags & ResAnmTexSrtMatData::FLAG_ANM_EXISTS)) {
            continue;
        }

        int srtIndex = index + TexSrtAnmResult::NUM_OF_MAT_TEX_MTX;

        const ResAnmTexSrtTexData* pTexData =
            reinterpret_cast<const ResAnmTexSrtTexData*>(ut::AddOffsetToPtr(pMatData, *pToTexData++));

        u32 result = MakeResult(&pResult->srt[srtIndex], pTexData, frame);
        pResult->indFlags |= result << (index * TexSrtAnmResult::NUM_OF_FLAGS);
    }
}

/******************************************************************************
 *
 * ResAnmVis
 *
 ******************************************************************************/
bool ResAnmVis::GetAnmResult(u32 idx, f32 frame) const {
    const ResAnmVisAnmData* pAnmData = GetNodeAnm(idx);
    const ResAnmVisInfoData& rInfoData = ref().info;

    if (pAnmData->flags & ResAnmVisAnmData::FLAG_CONST) {
        return pAnmData->flags & ResAnmVisAnmData::FLAG_ENABLE;
    }

    f32 fClippedFrame = detail::ClipFrame(rInfoData, frame);
    int iClippedFrame = static_cast<int>(math::FFloor(fClippedFrame));

    return detail::GetResBoolAnmFramesResult(&pAnmData->visibility, iClippedFrame);
}

} // namespace g3d
} // namespace nw4r
