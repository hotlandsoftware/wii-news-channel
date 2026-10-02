#include <nw4r/ef.h>

namespace nw4r {
namespace ef {

// Older revision (News Channel). Written from the DOL: no reference
// decompilation has ef_animcurve.cpp.

union AnimCurveRandomSeed {
    u32 value;
    u8 bytes[4];
};

struct AnimCurveTextureKey {
    u16 frame;    // at 0x0
    u8 PADDING_0x2[4];
    u8 random;    // at 0x6
    u8 PADDING_0x7;
    union {
        struct {
            u8 wrap;    // at 0x8
            u8 reverse; // at 0x9
        };
        u16 randomIdx; // at 0x8
    };
    u16 texIdx; // at 0xA
};

struct AnimCurveTextureValue {
    u8 wrap;    // at 0x0
    u8 reverse; // at 0x1
    u16 texIdx; // at 0x2
    u8 type;    // at 0x4
    u8 PADDING_0x5[3];
};

struct AnimCurveTextureRandom {
    u8 wrap;    // at 0x0
    u8 reverse; // at 0x1
    u16 texIdx; // at 0x2
};

inline u16 CalcFrame(AnimCurveHeader* pHeader, u32 tick, u32 life,
                     u32& loop) {
    u8 flag = pHeader->processFlag;
    u16 frame;

    if (!(flag & AnimCurveHeader::PROC_FLAG_INFLOOP) &&
        pHeader->loopCount <= 1) {

        if (!(flag & AnimCurveHeader::PROC_FLAG_FITTING)) {
            if (tick >= pHeader->frameLength) {
                frame = pHeader->frameLength;
            } else {
                frame = tick;
            }
        } else {
            frame = pHeader->frameLength * tick / life;
        }
    } else if (!(flag & AnimCurveHeader::PROC_FLAG_FITTING)) {
        frame = pHeader->frameLength;

        if (frame <= 1 || !(flag & AnimCurveHeader::PROC_FLAG_TURN)) {
            loop = tick / frame;

            if (!(flag & AnimCurveHeader::PROC_FLAG_INFLOOP) &&
                loop >= pHeader->loopCount) {
                loop = static_cast<u8>(pHeader->loopCount - 1);
            } else {
                frame = tick - loop * frame;
            }
        } else {
            u32 turnLen = frame - 1;
            loop = tick / turnLen;

            if (!(flag & AnimCurveHeader::PROC_FLAG_INFLOOP) &&
                loop >= pHeader->loopCount) {
                frame = pHeader->loopCount % 2 == 0 ? 0 : static_cast<u8>(turnLen);
                loop = static_cast<u8>(pHeader->loopCount - 1);
            } else if (!(loop & 1)) {
                frame = tick - loop * turnLen;
            } else {
                frame = turnLen * (loop + 1) - tick;
            }
        }
    } else if (tick >= life) {
        if (!(flag & AnimCurveHeader::PROC_FLAG_TURN) ||
            pHeader->loopCount % 2 != 0) {
            frame = pHeader->frameLength;
        } else {
            frame = 0;
        }

        loop = static_cast<u8>(pHeader->loopCount - 1);
    } else {
        u32 len = pHeader->frameLength;

        if (len <= 1 || !(flag & AnimCurveHeader::PROC_FLAG_TURN)) {
            f32 ratio = pHeader->loopCount * (static_cast<f32>(len) / life);
            loop = tick * ratio / len;
            frame = tick * ratio - loop * len;
        } else {
            int turnLen = len - 1;
            u8 loopCount = pHeader->loopCount;
            f32 r = (1.0f + static_cast<f32>(turnLen) * loopCount) / life;
            u32 time = tick * r;
            loop = time / turnLen;

            if (loop >= loopCount) {
                frame = loopCount % 2 == 0 ? 0 : static_cast<u8>(turnLen);
                loop = static_cast<u8>(loopCount - 1);
            } else if (loop % 2 == 0) {
                frame = time - loop * turnLen;
            } else {
                frame = turnLen * (loop + 1) - time;
            }
        }
    }

    return frame;
}

template <typename T>
inline int SearchKey(const T* pKeys, u16 numKey, int frame) {
    int hi = numKey - 1;
    int lo = 0;
    int mid = hi / 2;

    if (frame < pKeys[0].frame) {
        return 0;
    }

    if (pKeys[hi].frame < frame || frame == pKeys[hi].frame) {
        return hi;
    }

    int val = pKeys[mid].frame;

    while (lo < mid) {
        if (frame == val) {
            lo = mid;
            break;
        }

        if (val < frame) {
            lo = mid;
        }
        if (val >= frame) {
            hi = mid;
        }

        mid = (lo + hi) / 2;
        val = pKeys[mid].frame;
    }

    return lo;
}

void AnimCurveExecuteTexture(u8* pCmdList, Particle* pParticle, u32 tick,
                             u16 seed, u32 life) {
    AnimCurveHeader* pHeader = reinterpret_cast<AnimCurveHeader*>(pCmdList);

    u8* pKey = pCmdList + sizeof(AnimCurveHeader);
    u8* pRandom = pKey + pHeader->keyTable;
    u8* pRandomTable = pRandom + pHeader->rangeTable;
    u8* pNameTable = pRandomTable + pHeader->randomTable;

    u32 loop = 0;
    u16 frame = CalcFrame(pHeader, tick, life, loop);

    TextureData** ppTarget;
    int layer;

    switch (pHeader->kindType) {
    case 14: {
        ppTarget = &pParticle->mParameter.mTexture[TEX_LAYER_1];
        layer = TEX_LAYER_1;
        break;
    }
    case 15: {
        ppTarget = &pParticle->mParameter.mTexture[TEX_LAYER_2];
        layer = TEX_LAYER_2;
        break;
    }
    case 16: {
        ppTarget = &pParticle->mParameter.mTexture[TEX_LAYER_IND];
        layer = TEX_LAYER_IND;
        break;
    }
    default: {
        return;
    }
    }

    u16 numKey = *reinterpret_cast<u16*>(pKey);
    AnimCurveTextureKey* pKeys = reinterpret_cast<AnimCurveTextureKey*>(pKey + 4);

    int idx = SearchKey(pKeys, numKey, frame);

    AnimCurveTextureKey* pTheKey = &pKeys[idx];

    u16 texIdx;
    u8 wrap;
    u8 reverse;

    if (pTheKey->random == 0) {
        texIdx = pTheKey->texIdx;
        wrap = pTheKey->wrap;
        reverse = pTheKey->reverse;
    } else {
        u16 randomIdx = pTheKey->randomIdx;

        AnimCurveRandomSeed rnd;
        rnd.value = seed * 0x3F81F635 + pHeader->randomSeed * 0x30A74193 +
                    (loop * 0x7B929 + randomIdx * 0x371097E7 + 0x4BF53);
        rnd.bytes[2] ^= rnd.bytes[3];
        rnd.bytes[1] ^= rnd.bytes[2];
        rnd.bytes[0] ^= rnd.bytes[1];

        if (!(pTheKey->random & 2)) {
            AnimCurveTextureValue* pValue =
                reinterpret_cast<AnimCurveTextureValue*>(pRandom + 4) +
                randomIdx;

            u8 type = pValue->type;
            texIdx = pValue->texIdx;
            wrap = pValue->wrap;
            reverse = pValue->reverse;

            switch (type) {
            case 1: {
                reverse = (reverse & 2) | (rnd.value >> 16 & 1);
                break;
            }
            case 2: {
                reverse = (reverse & 1) | (rnd.value >> 16 & 2);
                break;
            }
            case 3: {
                reverse = rnd.value >> 16 & 3;
                break;
            }
            }
        } else {
            u16 numRandom = *reinterpret_cast<u16*>(pRandomTable);
            AnimCurveTextureRandom* pTable =
                reinterpret_cast<AnimCurveTextureRandom*>(pRandomTable + 4) +
                (rnd.value >> 16) % numRandom;

            texIdx = pTable->texIdx;
            wrap = pTable->wrap;
            reverse = pTable->reverse;
        }
    }

    *ppTarget = reinterpret_cast<TextureData*>(
        reinterpret_cast<AnimCurveNameTable*>(pNameTable)->datas[texIdx].work);

    pParticle->mParameter.mTextureWrap =
        static_cast<u16>(pParticle->mParameter.mTextureWrap & ~(0xF << layer * 4)) |
        ((wrap & 0xF) << layer * 4);

    pParticle->mParameter.mTextureReverse =
        static_cast<u8>(pParticle->mParameter.mTextureReverse & ~(3 << layer * 2)) |
        ((reverse & 3) << layer * 2);
}

} // namespace ef
} // namespace nw4r
