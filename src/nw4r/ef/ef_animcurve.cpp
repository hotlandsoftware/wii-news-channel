#include <nw4r/ef.h>

namespace nw4r {
namespace ef {

// Older revision (News Channel). Written from the DOL: no reference
// decompilation has ef_animcurve.cpp.

struct AnimCurveKeyU8 {
    u16 frame;  // at 0x0
    u16 interp; // at 0x2
    u8 PADDING_0x4[2];
    u8 random; // at 0x6
    u8 PADDING_0x7;
    union {
        u8 value;      // at 0x8
        u16 randomIdx; // at 0x8
    };
};

struct AnimCurveRandomU8 {
    u8 base;  // at 0x0
    u8 range; // at 0x1
};

enum AnimCurveInterp {
    AC_INTERP_LINEAR,
    AC_INTERP_SMOOTH,
    AC_INTERP_STEP,
};

enum AnimCurveKeyRandom {
    AC_KEY_RANDOM_TABLE = (1 << 1),
};

inline u32 CalcRandomSeed(u16 seed, u16 headerSeed, u32 loop, u16 idx) {
    return seed * 0x3F81F635 + headerSeed * 0x30A74193 +
           (loop * 0x7B929 + idx * 0x371097E7 + 0x4BF53);
}

inline const AnimCurveRandomU8*
GetRandomU8(u8 type, u16 idx, u8* pRandom, u8* pRandomTable, u16 seed,
            u16 headerSeed, u32 loop, u32& rRandom) {
    AnimCurveRandomSeed rnd;
    rnd.value = CalcRandomSeed(seed, headerSeed, loop, idx);
    rnd.bytes[2] ^= rnd.bytes[3];
    rnd.bytes[1] ^= rnd.bytes[2];
    rnd.bytes[0] ^= rnd.bytes[1];

    rRandom = rnd.value;

    if (!(type & AC_KEY_RANDOM_TABLE)) {
        return reinterpret_cast<AnimCurveRandomU8*>(pRandom + 4) + idx;
    }

    u16 num = *reinterpret_cast<u16*>(pRandomTable);
    const AnimCurveRandomU8* pEntry =
        reinterpret_cast<AnimCurveRandomU8*>(pRandomTable + 4) +
        (rRandom >> 16) % num;
    rRandom = rRandom * 0x343FD + 0x269EC3;
    return pEntry;
}

inline int CalcRandomU8(const AnimCurveRandomU8* pEntry, u32 random) {
    int value =
        pEntry->base + pEntry->range * static_cast<s16>(random >> 16) / 32768;

    if (value < 0) {
        value = 0;
    }
    if (value > 255) {
        value = 255;
    }

    return value;
}

inline u8 InterpolateU8(u8 v0, u8 v1, u32 t, u16 interp) {
    switch (interp & 3) {
    case AC_INTERP_LINEAR: {
        return v0 + (t * (v1 - v0) >> 16);
    }
    case AC_INTERP_SMOOTH: {
        f32 ft = t / 65536.0f;
        return v0 + ft * (ft * ((3.0f - 2.0f * ft) * (v1 - v0)));
    }
    case AC_INTERP_STEP: {
        return v0;
    }
    default: {
        return 0;
    }
    }
}

template <typename T>
inline int SearchKeyF(AnimCurveKey* pKeyTable, T* pKeys, int frame, f32 time,
                      bool& exact, int& frame0, int& frame1) {
    int idx = pKeyTable->count - 1;
    int mid = idx / 2;
    int lo = 0;
    exact = static_cast<f32>(__fabs(frame - time)) < NW4R_MATH_FLT_EPSILON;

    frame0 = pKeys[0].frame;

    if (frame < frame0) {
        idx = 0;
        exact = true;
    } else if (frame == frame0) {
        if (idx == 0) {
            exact = true;
        } else if (!exact) {
            frame1 = pKeys[1].frame;
        }

        idx = 0;
    } else {
        frame1 = pKeys[idx].frame;

        if (frame1 <= frame) {
            exact = true;
        } else {
            int val = pKeys[mid].frame;

            while (lo < mid) {
                if (frame == val) {
                    idx = mid;

                    if (!exact) {
                        frame0 = val;
                        frame1 = pKeys[mid + 1].frame;
                    }
                    return idx;
                }

                if (val < frame) {
                    lo = mid;
                    frame0 = val;
                } else {
                    idx = mid;
                    frame1 = val;
                }

                mid = (lo + idx) / 2;
                val = pKeys[mid].frame;
            }

            idx = lo;
            exact = false;
        }
    }

    return idx;
}

void AnimCurveExecuteAlpha(u8* pCmdList, u8* pTarget, u32 tick, u16 seed,
                           u32 life) {
    AnimCurveHeader* pHeader = reinterpret_cast<AnimCurveHeader*>(pCmdList);

    u8* pKey = pCmdList + sizeof(AnimCurveHeader);
    u8* pRandom = pKey + pHeader->keyTable;
    u8* pRandomTable = pRandom + pHeader->rangeTable;

    u32 loop = 0;
    u16 len = pHeader->frameLength;
    u16 frame;
    f32 time;

    if (len <= 1) {
        time = 0.0f;
        loop = tick;
        frame = 0;
    } else {
        u8 flag = pHeader->processFlag;

        if (!(flag & AnimCurveHeader::PROC_FLAG_INFLOOP) &&
            pHeader->loopCount <= 1) {

            if (!(flag & AnimCurveHeader::PROC_FLAG_FITTING)) {
                frame = tick;
                if (tick >= len - 1) {
                    frame = len - 1;
                }

                time = frame;
            } else {
                time = tick * (static_cast<f32>(len - 1) / (life - 1));

                if (time > len - 1) {
                    time = len - 1;
                }

                frame = time;
            }
        } else if (!(flag & AnimCurveHeader::PROC_FLAG_FITTING)) {
            u32 turnLen = len - 1;
            loop = tick / turnLen;

            if (!(flag & AnimCurveHeader::PROC_FLAG_TURN)) {
                if (!(flag & AnimCurveHeader::PROC_FLAG_INFLOOP) &&
                    loop >= pHeader->loopCount) {
                    frame = turnLen;
                    loop = static_cast<u8>(pHeader->loopCount - 1);
                } else {
                    frame = tick - loop * turnLen;
                }
            } else if (!(flag & AnimCurveHeader::PROC_FLAG_INFLOOP) &&
                       loop >= pHeader->loopCount) {
                frame = pHeader->loopCount % 2 == 0 ? static_cast<u16>(0) : static_cast<u16>(turnLen);
                loop = static_cast<u8>(pHeader->loopCount - 1);
            } else if (loop % 2 == 0) {
                frame = tick - loop * turnLen;
            } else {
                frame = turnLen * (loop + 1) - tick;
            }

            time = frame;
        } else if (tick >= life - 1) {
            if (!(flag & AnimCurveHeader::PROC_FLAG_TURN) ||
                pHeader->loopCount % 2 != 0) {
                frame = static_cast<u8>(len - 1);
            } else {
                frame = 0;
            }

            time = frame;
            loop = static_cast<u8>(pHeader->loopCount - 1);
        } else {
            int turnLen = len - 1;
            f32 ratio = pHeader->loopCount * (static_cast<f32>(turnLen) / (life - 1));
            loop = tick * ratio / turnLen;
            time = tick * ratio - loop * turnLen;

            if ((flag & AnimCurveHeader::PROC_FLAG_TURN) && (loop & 1)) {
                time = turnLen - time;
            }

            frame = time;
        }
    }

    int f = frame;
    AnimCurveKey* pKeyTable = reinterpret_cast<AnimCurveKey*>(pKey);

    int idx = pKeyTable->count - 1;
    int mid = idx / 2;
    AnimCurveKeyU8* pKeys = reinterpret_cast<AnimCurveKeyU8*>(pKeyTable->datas);
    int lo = 0;
    bool exact = static_cast<f32>(__fabs(f - time)) < NW4R_MATH_FLT_EPSILON;

    int frame0 = pKeys[0].frame;
    int frame1;

    if (f < frame0) {
        idx = 0;
        exact = true;
    } else if (f == frame0) {
        if (idx == 0) {
            exact = true;
        } else if (!exact) {
            frame1 = pKeys[1].frame;
        }

        idx = 0;
    } else {
        frame1 = pKeys[idx].frame;

        if (frame1 <= f) {
            exact = true;
        } else {
            int val = pKeys[mid].frame;

            while (lo < mid) {
                if (f == val) {
                    idx = mid;

                    if (!exact) {
                        frame0 = val;
                        frame1 = pKeys[mid + 1].frame;
                    }
                    goto found;
                }

                if (val < f) {
                    lo = mid;
                    frame0 = val;
                } else {
                    idx = mid;
                    frame1 = val;
                }

                mid = (lo + idx) / 2;
                val = pKeys[mid].frame;
            }

            idx = lo;
            exact = false;
        }
    }
found:
    AnimCurveKeyU8* pTheKey;

    if (exact) {
        pTheKey = &pKeys[idx];
    } else {
    u32 nextLoop = loop;
    u8 flag = pHeader->processFlag;

    if ((flag & AnimCurveHeader::PROC_FLAG_TURN) &&
        ((flag & AnimCurveHeader::PROC_FLAG_INFLOOP) ||
         pHeader->loopCount > 1)) {

        if (!(loop & 1) && idx + 1 >= pKeyTable->count - 1 &&
            ((flag & AnimCurveHeader::PROC_FLAG_INFLOOP) ||
             loop < pHeader->loopCount - 1)) {
            nextLoop = loop + 1;
        }

        if ((loop & 1) && idx == 0 && loop != 0) {
            loop++;
        }
    }

    u32 t = static_cast<u32>(65536.0f * (time - static_cast<u16>(frame0))) /
            (static_cast<u16>(frame1) - static_cast<u16>(frame0));

    AnimCurveKeyU8* pKey0 = &pKeys[idx];
    AnimCurveKeyU8* pKey1 = &pKeys[idx + 1];
    u16 interp = pKey0->interp;
    u8 random0 = pKey0->random;
    u8 random1 = pKey1->random;
    bool isRandom0 = random0 != 0;
    bool isRandom1 = random1 != 0;

    if (!isRandom0 && !isRandom1) {
        u8 v0 = pKey0->value;
        u8 v1 = pKey1->value;

        if (v0 == v1) {
            *pTarget = v0;
        } else {
            *pTarget = InterpolateU8(v0, v1, t, interp);
        }
    } else if (isRandom0 && !isRandom1) {
        u32 r0;
        const AnimCurveRandomU8* pEntry0 =
            GetRandomU8(random0, pKey0->randomIdx, pRandom, pRandomTable, seed,
                        pHeader->randomSeed, loop, r0);
        int v0 = CalcRandomU8(pEntry0, r0);
        *pTarget = InterpolateU8(v0, pKey1->value, t, interp);
    } else if (!isRandom0 && isRandom1) {
        u32 r1;
        const AnimCurveRandomU8* pEntry1 =
            GetRandomU8(random1, pKey1->randomIdx, pRandom, pRandomTable, seed,
                        pHeader->randomSeed, nextLoop, r1);
        int v1 = CalcRandomU8(pEntry1, r1);
        *pTarget = InterpolateU8(pKey0->value, v1, t, interp);
    } else {
        u32 r0;
        const AnimCurveRandomU8* pEntry0 =
            GetRandomU8(random0, pKey0->randomIdx, pRandom, pRandomTable, seed,
                        pHeader->randomSeed, loop, r0);
        u32 r1;
        const AnimCurveRandomU8* pEntry1 =
            GetRandomU8(random1, pKey1->randomIdx, pRandom, pRandomTable, seed,
                        pHeader->randomSeed, nextLoop, r1);
        int v0 = CalcRandomU8(pEntry0, r0);
        int v1 = CalcRandomU8(pEntry1, r1);
        *pTarget = InterpolateU8(v0, v1, t, interp);
    }
        return;
    }

    if (pTheKey->random == 0) {
        *pTarget = pTheKey->value;
    } else {
        u32 r;
        const AnimCurveRandomU8* pEntry =
            GetRandomU8(pTheKey->random, pTheKey->randomIdx, pRandom,
                        pRandomTable, seed, pHeader->randomSeed, loop, r);
        *pTarget = CalcRandomU8(pEntry, r);
    }
}

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
                frame = pHeader->loopCount % 2 == 0 ? static_cast<u16>(0) : static_cast<u16>(static_cast<u8>(turnLen));
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
                frame = loopCount % 2 == 0 ? static_cast<u16>(0) : static_cast<u16>(static_cast<u8>(turnLen));
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
