#include <nw4r/ef.h>

namespace nw4r {
namespace ef {

// Older revision (News Channel). Written from the DOL: no reference
// decompilation has ef_animcurve.cpp.
//
// Work in progress (NonMatching): only AnimCurveExecuteAlpha (~94%) and
// AnimCurveExecuteTexture (~99%) are written. The other executors
// (Color, F32x1/x2/x3, F32 (count from the curve flags), Rotate, Child and
// createChild) follow the same scheme: frame/loop calculation, key search,
// per-key random values (seed hash, then the XOR-folded bytes), and
// linear/smooth/step interpolation.

inline f32 TurnTime(f32 time, u8 flag, u32 loop, int turnLen) {
    if (!(flag & AnimCurveHeader::PROC_FLAG_TURN)) {
        return time;
    }

    if (!(loop & 1)) {
        return time;
    }

    return turnLen - time;
}

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

inline u32 CalcRandomBase(u16 seed, u16 headerSeed) {
    return seed * 0x3F81F635 + headerSeed * 0x30A74193;
}

inline const AnimCurveRandomU8*
GetRandomU8(const AnimCurveKeyU8* pKey, u8* pRandom, u8* pRandomTable,
            u32 base, u32 loop, u32& rRandom) {
    bool useTable = pKey->random & AC_KEY_RANDOM_TABLE;

    AnimCurveRandomSeed rnd;
    rnd.value = base + loop * 0x7B929 + pKey->randomIdx * 0x371097E7 + 0x4BF53;
    rnd.bytes[2] ^= rnd.bytes[3];
    rnd.bytes[1] ^= rnd.bytes[2];
    rnd.bytes[0] ^= rnd.bytes[1];

    rRandom = rnd.value;

    if (!useTable) {
        return reinterpret_cast<AnimCurveRandomU8*>(pRandom + 4) +
               pKey->randomIdx;
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

struct AnimCurveKeyColor {
    u16 frame;  // at 0x0
    u16 interp; // at 0x2
    u8 PADDING_0x4[2];
    u8 random; // at 0x6
    u8 PADDING_0x7;
    union {
        u8 value[3];   // at 0x8
        u16 randomIdx; // at 0x8
    };
    u8 PADDING_0xB;
};

inline const AnimCurveRandomU8*
GetRandomColor(const AnimCurveKeyColor* pKey, u8* pRandom, u8* pRandomTable,
               u32 base, u32 loop, u32& rRandom) {
    bool useTable = pKey->random & AC_KEY_RANDOM_TABLE;

    AnimCurveRandomSeed rnd;
    rnd.value = base + loop * 0x7B929 + pKey->randomIdx * 0x371097E7 + 0x4BF53;
    rnd.bytes[2] ^= rnd.bytes[3];
    rnd.bytes[1] ^= rnd.bytes[2];
    rnd.bytes[0] ^= rnd.bytes[1];

    rRandom = rnd.value;

    if (!useTable) {
        return reinterpret_cast<AnimCurveRandomU8*>(pRandom + 4) +
               pKey->randomIdx * 3;
    }

    u16 num = *reinterpret_cast<u16*>(pRandomTable);
    const AnimCurveRandomU8* pEntry =
        reinterpret_cast<AnimCurveRandomU8*>(pRandomTable + 4) +
        (rRandom >> 16) % num * 3;
    rRandom = rRandom * 0x343FD + 0x269EC3;
    return pEntry;
}

void AnimCurveExecuteColor(u8* pCmdList, u8* pTarget, u32 tick, u16 seed,
                           u32 life) {
    AnimCurveHeader* pHeader = reinterpret_cast<AnimCurveHeader*>(pCmdList);

    u8* pKey = pCmdList + sizeof(AnimCurveHeader);
    u8* pRandom = pKey + pHeader->keyTable;
    u8* pRandomTable = pRandom + pHeader->rangeTable;

    u32 loop = 0;
    u32 nextLoop;
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

            time = TurnTime(time, flag, loop, turnLen);
            frame = time;
        }
    }

    int f = frame;
    AnimCurveKey* pKeyTable = reinterpret_cast<AnimCurveKey*>(pKey);
    AnimCurveKeyColor* pKeys;

    int idx = pKeyTable->count - 1;
    int mid = idx / 2;
    pKeys = reinterpret_cast<AnimCurveKeyColor*>(pKeyTable->datas);
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
    AnimCurveKeyColor* pTheKey;

    if (exact) {
        pTheKey = &pKeys[idx];
    } else {
    nextLoop = loop;
    u8 flag = pHeader->processFlag;

    if ((flag & AnimCurveHeader::PROC_FLAG_TURN) &&
        ((flag & AnimCurveHeader::PROC_FLAG_INFLOOP) ||
         pHeader->loopCount > 1)) {

        if (!(loop & 1) && idx + 1 >= *reinterpret_cast<u16*>(pKey) - 1 &&
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

    AnimCurveKeyColor* pKey0 = &pKeys[idx];
    AnimCurveKeyColor* pKey1 = &pKeys[idx + 1];
    u16 interp = pKey0->interp;
    bool isRandom0 = pKey0->random != 0;
    bool isRandom1 = pKey1->random != 0;

    if (!isRandom0 && !isRandom1) {
        u8* pValue0 = pKey0->value;
        u8* pValue1 = pKey1->value;

        for (int i = 0; i < 3; i++) {
            u8 v0 = *pValue0;
            u8 v1 = *pValue1;

            if (v0 == v1) {
                *pTarget++ = v0;
            } else {
                *pTarget++ = InterpolateU8(v0, v1, t, interp);
            }

            interp >>= 2;
            pValue0++;
            pValue1++;
        }
    } else if (isRandom0 && !isRandom1) {
        u32 r0;
        const AnimCurveRandomU8* pEntry0 = GetRandomColor(
            pKey0, pRandom, pRandomTable,
            CalcRandomBase(seed, pHeader->randomSeed), loop, r0);
        u8* pValue1 = pKey1->value;

        for (int i = 0; i < 3; i++) {
            int v0 = CalcRandomU8(pEntry0, r0);
            pEntry0++;
            r0 = r0 * 0x343FD + 0x269EC3;

            *pTarget++ = InterpolateU8(v0, *pValue1, t, interp);
            interp >>= 2;
            pValue1++;
        }
    } else if (!isRandom0 && isRandom1) {
        u32 r1;
        const AnimCurveRandomU8* pEntry1 = GetRandomColor(
            pKey1, pRandom, pRandomTable,
            CalcRandomBase(seed, pHeader->randomSeed), nextLoop, r1);
        u8* pValue0 = pKey0->value;

        for (int i = 0; i < 3; i++) {
            int v1 = CalcRandomU8(pEntry1, r1);
            pEntry1++;
            r1 = r1 * 0x343FD + 0x269EC3;

            *pTarget++ = InterpolateU8(*pValue0, v1, t, interp);
            interp >>= 2;
            pValue0++;
        }
    } else {
        u32 r0;
        const AnimCurveRandomU8* pEntry0 = GetRandomColor(
            pKey0, pRandom, pRandomTable,
            CalcRandomBase(seed, pHeader->randomSeed), loop, r0);
        u32 r1;
        const AnimCurveRandomU8* pEntry1 = GetRandomColor(
            pKey1, pRandom, pRandomTable,
            CalcRandomBase(seed, pHeader->randomSeed), nextLoop, r1);

        for (int i = 0; i < 3; i++) {
            int v0 = CalcRandomU8(pEntry0, r0);
            pEntry0++;
            int v1 = CalcRandomU8(pEntry1, r1);
            pEntry1++;
            r0 = r0 * 0x343FD + 0x269EC3;
            r1 = r1 * 0x343FD + 0x269EC3;

            *pTarget++ = InterpolateU8(v0, v1, t, interp);
            interp >>= 2;
        }
    }
        return;
    }

    if (pTheKey->random == 0) {
        pTarget[0] = pTheKey->value[0];
        pTarget[1] = pTheKey->value[1];
        pTarget[2] = pTheKey->value[2];
    } else {
        u32 r;
        const AnimCurveRandomU8* pEntry =
            GetRandomColor(pTheKey, pRandom, pRandomTable,
                           CalcRandomBase(seed, pHeader->randomSeed), loop, r);

        for (int i = 0; i < 3; i++) {
            pTarget[i] = CalcRandomU8(pEntry, r);
            pEntry++;
            r = r * 0x343FD + 0x269EC3;
        }
    }
}

void AnimCurveExecuteAlpha(u8* pCmdList, u8* pTarget, u32 tick, u16 seed,
                           u32 life) {
    AnimCurveHeader* pHeader = reinterpret_cast<AnimCurveHeader*>(pCmdList);

    u8* pKey = pCmdList + sizeof(AnimCurveHeader);
    u8* pRandom = pKey + pHeader->keyTable;
    u8* pRandomTable = pRandom + pHeader->rangeTable;

    u32 loop = 0;
    u32 nextLoop;
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

            time = TurnTime(time, flag, loop, turnLen);
            frame = time;
        }
    }

    int f = frame;
    AnimCurveKey* pKeyTable = reinterpret_cast<AnimCurveKey*>(pKey);
    AnimCurveKeyU8* pKeys;

    int idx = pKeyTable->count - 1;
    int mid = idx / 2;
    pKeys = reinterpret_cast<AnimCurveKeyU8*>(pKeyTable->datas);
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
    nextLoop = loop;
    u8 flag = pHeader->processFlag;

    if ((flag & AnimCurveHeader::PROC_FLAG_TURN) &&
        ((flag & AnimCurveHeader::PROC_FLAG_INFLOOP) ||
         pHeader->loopCount > 1)) {

        if (!(loop & 1) && idx + 1 >= *reinterpret_cast<u16*>(pKey) - 1 &&
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
    bool isRandom0 = pKey0->random != 0;
    bool isRandom1 = pKey1->random != 0;

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
            GetRandomU8(pKey0, pRandom, pRandomTable,
                        CalcRandomBase(seed, pHeader->randomSeed), loop, r0);
        int v0 = CalcRandomU8(pEntry0, r0);
        *pTarget = InterpolateU8(v0, pKey1->value, t, interp);
    } else if (!isRandom0 && isRandom1) {
        u32 r1;
        const AnimCurveRandomU8* pEntry1 =
            GetRandomU8(pKey1, pRandom, pRandomTable,
                        CalcRandomBase(seed, pHeader->randomSeed), nextLoop, r1);
        int v1 = CalcRandomU8(pEntry1, r1);
        *pTarget = InterpolateU8(pKey0->value, v1, t, interp);
    } else {
        u32 r0;
        const AnimCurveRandomU8* pEntry0 =
            GetRandomU8(pKey0, pRandom, pRandomTable,
                        CalcRandomBase(seed, pHeader->randomSeed), loop, r0);
        u32 r1;
        const AnimCurveRandomU8* pEntry1 =
            GetRandomU8(pKey1, pRandom, pRandomTable,
                        CalcRandomBase(seed, pHeader->randomSeed), nextLoop, r1);
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
            GetRandomU8(pTheKey, pRandom,
                        pRandomTable, CalcRandomBase(seed, pHeader->randomSeed),
                        loop, r);
        *pTarget = CalcRandomU8(pEntry, r);
    }
}

struct AnimCurveKeyF32x2 {
    u16 frame;  // at 0x0
    u16 interp; // at 0x2
    u8 PADDING_0x4[2];
    u8 random; // at 0x6
    u8 PADDING_0x7;
    union {
        f32 value[2];  // at 0x8
        u16 randomIdx; // at 0x8
    };
};

struct AnimCurveKeyF32x3 {
    u16 frame;  // at 0x0
    u16 interp; // at 0x2
    u8 PADDING_0x4[2];
    u8 random; // at 0x6
    u8 PADDING_0x7;
    union {
        f32 value[3];  // at 0x8
        u16 randomIdx; // at 0x8
    };
};

struct AnimCurveRandomF32 {
    f32 base;  // at 0x0
    f32 range; // at 0x4
};

struct AnimCurveRandomF32x2 {
    AnimCurveRandomF32 v[2];
};

struct AnimCurveRandomF32x3 {
    AnimCurveRandomF32 v[3];
};

// Optional output of the F32x1/F32x2 executors (every caller passes NULL)
struct AnimCurveCacheF32 {
    union {
        f32 value;     // at 0x0
        f32 values[2]; // at 0x0
    };
    u8 PADDING_0x8[0x11 - 0x8];
    bool dirty; // at 0x11
};

template <typename TEntry, typename TKey>
inline const TEntry* GetRandomEntry(const TKey* pKey, u8* pRandom,
                                    u8* pRandomTable, u32 base, u32 loop,
                                    u32& rRandom) {
    bool useTable = pKey->random & AC_KEY_RANDOM_TABLE;

    AnimCurveRandomSeed rnd;
    rnd.value = base + loop * 0x7B929 + pKey->randomIdx * 0x371097E7 + 0x4BF53;
    rnd.bytes[2] ^= rnd.bytes[3];
    rnd.bytes[1] ^= rnd.bytes[2];
    rnd.bytes[0] ^= rnd.bytes[1];

    rRandom = rnd.value;

    if (!useTable) {
        return reinterpret_cast<TEntry*>(pRandom + 4) + pKey->randomIdx;
    }

    u16 num = *reinterpret_cast<u16*>(pRandomTable);
    const TEntry* pEntry =
        reinterpret_cast<TEntry*>(pRandomTable + 4) + (rRandom >> 16) % num;
    rRandom = rRandom * 0x343FD + 0x269EC3;
    return pEntry;
}

inline f32 CalcRandomF32(const AnimCurveRandomF32* pEntry, u32 random) {
    return pEntry->base + pEntry->range * static_cast<u16>(random >> 16);
}

inline f32 InterpolateF32(f32 v0, f32 v1, f32 t, u16 interp) {
    switch (interp & 3) {
    case AC_INTERP_LINEAR: {
        return v0 + t * (v1 - v0);
    }
    case AC_INTERP_SMOOTH: {
        return v0 + t * (t * ((3.0f - 2.0f * t) * (v1 - v0)));
    }
    case AC_INTERP_STEP: {
        return v0;
    }
    default: {
        return 0.0f;
    }
    }
}

inline f32 InterpolateKeyF32(f32 v0, f32 v1, f32 t, u16 interp) {
    if (v0 == v1) {
        return v0;
    }

    return InterpolateF32(v0, v1, t, interp);
}

void AnimCurveExecuteF32x2(u8* pCmdList, Particle* pParticle, f32* pTarget,
                           u32 tick, u16 seed, u32 life) {
    AnimCurveCacheF32* pCache = reinterpret_cast<AnimCurveCacheF32*>(pParticle);

    AnimCurveHeader* pHeader = reinterpret_cast<AnimCurveHeader*>(pCmdList);

    u8* pKey = pCmdList + sizeof(AnimCurveHeader);
    u8* pRandom = pKey + pHeader->keyTable;
    u8* pRandomTable = pRandom + pHeader->rangeTable;

    u32 loop = 0;
    u32 nextLoop;
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

            time = TurnTime(time, flag, loop, turnLen);
            frame = time;
        }
    }

    int f = frame;
    AnimCurveKey* pKeyTable = reinterpret_cast<AnimCurveKey*>(pKey);
    AnimCurveKeyF32x2* pKeys;

    int idx = pKeyTable->count - 1;
    int mid = idx / 2;
    pKeys = reinterpret_cast<AnimCurveKeyF32x2*>(pKeyTable->datas);
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
    AnimCurveKeyF32x2* pTheKey;

    if (exact) {
        pTheKey = &pKeys[idx];
    } else {
    nextLoop = loop;
    u8 flag = pHeader->processFlag;

    if ((flag & AnimCurveHeader::PROC_FLAG_TURN) &&
        ((flag & AnimCurveHeader::PROC_FLAG_INFLOOP) ||
         pHeader->loopCount > 1)) {

        if (!(loop & 1) && idx + 1 >= *reinterpret_cast<u16*>(pKey) - 1 &&
            ((flag & AnimCurveHeader::PROC_FLAG_INFLOOP) ||
             loop < pHeader->loopCount - 1)) {
            nextLoop = loop + 1;
        }

        if ((loop & 1) && idx == 0 && loop != 0) {
            loop++;
        }
    }

    f32 t = (time - static_cast<u16>(frame0)) /
            (static_cast<u16>(frame1) - static_cast<u16>(frame0));

    AnimCurveKeyF32x2* pKey0 = &pKeys[idx];
    AnimCurveKeyF32x2* pKey1 = &pKeys[idx + 1];
    u16 interp = pKey0->interp;
    bool isRandom0 = pKey0->random != 0;
    bool isRandom1 = pKey1->random != 0;

    if (!isRandom0 && !isRandom1) {
        if (pCache != NULL) {
            pCache->dirty = false;
        }

        pTarget[0] = InterpolateKeyF32(pKey0->value[0], pKey1->value[0], t, interp);
        if (pCache != NULL) {
            pCache->values[0] = pTarget[0];
        }
        interp >>= 2;
        pTarget[1] = InterpolateKeyF32(pKey0->value[1], pKey1->value[1], t, interp);
        if (pCache != NULL) {
            pCache->values[1] = pTarget[1];
        }
    } else if (isRandom0 && !isRandom1) {
        u32 r0;
        const AnimCurveRandomF32x2* pEntry0 = GetRandomEntry<AnimCurveRandomF32x2>(
            pKey0, pRandom, pRandomTable,
            CalcRandomBase(seed, pHeader->randomSeed), loop, r0);
        pTarget[0] = InterpolateF32(CalcRandomF32(&pEntry0->v[0], r0), pKey1->value[0], t, interp);
        r0 = r0 * 0x343FD + 0x269EC3;
        interp >>= 2;
        pTarget[1] = InterpolateF32(CalcRandomF32(&pEntry0->v[1], r0), pKey1->value[1], t, interp);
    } else if (!isRandom0 && isRandom1) {
        u32 r1;
        const AnimCurveRandomF32x2* pEntry1 = GetRandomEntry<AnimCurveRandomF32x2>(
            pKey1, pRandom, pRandomTable,
            CalcRandomBase(seed, pHeader->randomSeed), nextLoop, r1);
        pTarget[0] = InterpolateF32(pKey0->value[0], CalcRandomF32(&pEntry1->v[0], r1), t, interp);
        r1 = r1 * 0x343FD + 0x269EC3;
        interp >>= 2;
        pTarget[1] = InterpolateF32(pKey0->value[1], CalcRandomF32(&pEntry1->v[1], r1), t, interp);
    } else {
        u32 r0;
        const AnimCurveRandomF32x2* pEntry0 = GetRandomEntry<AnimCurveRandomF32x2>(
            pKey0, pRandom, pRandomTable,
            CalcRandomBase(seed, pHeader->randomSeed), loop, r0);
        u32 r1;
        const AnimCurveRandomF32x2* pEntry1 = GetRandomEntry<AnimCurveRandomF32x2>(
            pKey1, pRandom, pRandomTable,
            CalcRandomBase(seed, pHeader->randomSeed), nextLoop, r1);
        pTarget[0] = InterpolateF32(CalcRandomF32(&pEntry0->v[0], r0), CalcRandomF32(&pEntry1->v[0], r1), t, interp);
        r0 = r0 * 0x343FD + 0x269EC3;
        r1 = r1 * 0x343FD + 0x269EC3;
        interp >>= 2;
        pTarget[1] = InterpolateF32(CalcRandomF32(&pEntry0->v[1], r0), CalcRandomF32(&pEntry1->v[1], r1), t, interp);
    }
        return;
    }

    if (pTheKey->random == 0) {
        pTarget[0] = pTheKey->value[0];
        pTarget[1] = pTheKey->value[1];
    } else {
        u32 r;
        const AnimCurveRandomF32x2* pEntry = GetRandomEntry<AnimCurveRandomF32x2>(
            pTheKey, pRandom, pRandomTable,
            CalcRandomBase(seed, pHeader->randomSeed), loop, r);
        pTarget[0] = CalcRandomF32(&pEntry->v[0], r);
        r = r * 0x343FD + 0x269EC3;
        pTarget[1] = CalcRandomF32(&pEntry->v[1], r);
    }
}

void AnimCurveExecuteF32x3(u8* pCmdList, f32* pTarget, u32 tick, u16 seed,
                           u32 life) {
    AnimCurveHeader* pHeader = reinterpret_cast<AnimCurveHeader*>(pCmdList);

    u8* pKey = pCmdList + sizeof(AnimCurveHeader);
    u8* pRandom = pKey + pHeader->keyTable;
    u8* pRandomTable = pRandom + pHeader->rangeTable;

    u32 loop = 0;
    u32 nextLoop;
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

            time = TurnTime(time, flag, loop, turnLen);
            frame = time;
        }
    }

    int f = frame;
    AnimCurveKey* pKeyTable = reinterpret_cast<AnimCurveKey*>(pKey);
    AnimCurveKeyF32x3* pKeys;

    int idx = pKeyTable->count - 1;
    int mid = idx / 2;
    pKeys = reinterpret_cast<AnimCurveKeyF32x3*>(pKeyTable->datas);
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
    AnimCurveKeyF32x3* pTheKey;

    if (exact) {
        pTheKey = &pKeys[idx];
    } else {
    nextLoop = loop;
    u8 flag = pHeader->processFlag;

    if ((flag & AnimCurveHeader::PROC_FLAG_TURN) &&
        ((flag & AnimCurveHeader::PROC_FLAG_INFLOOP) ||
         pHeader->loopCount > 1)) {

        if (!(loop & 1) && idx + 1 >= *reinterpret_cast<u16*>(pKey) - 1 &&
            ((flag & AnimCurveHeader::PROC_FLAG_INFLOOP) ||
             loop < pHeader->loopCount - 1)) {
            nextLoop = loop + 1;
        }

        if ((loop & 1) && idx == 0 && loop != 0) {
            loop++;
        }
    }

    f32 t = (time - static_cast<u16>(frame0)) /
            (static_cast<u16>(frame1) - static_cast<u16>(frame0));

    AnimCurveKeyF32x3* pKey0 = &pKeys[idx];
    AnimCurveKeyF32x3* pKey1 = &pKeys[idx + 1];
    u16 interp = pKey0->interp;
    bool isRandom0 = pKey0->random != 0;
    bool isRandom1 = pKey1->random != 0;

    if (!isRandom0 && !isRandom1) {
        pTarget[0] = InterpolateKeyF32(pKey0->value[0], pKey1->value[0], t, interp);
        interp >>= 2;
        pTarget[1] = InterpolateKeyF32(pKey0->value[1], pKey1->value[1], t, interp);
        interp >>= 2;
        pTarget[2] = InterpolateKeyF32(pKey0->value[2], pKey1->value[2], t, interp);
    } else if (isRandom0 && !isRandom1) {
        u32 r0;
        const AnimCurveRandomF32x3* pEntry0 = GetRandomEntry<AnimCurveRandomF32x3>(
            pKey0, pRandom, pRandomTable,
            CalcRandomBase(seed, pHeader->randomSeed), loop, r0);
        pTarget[0] = InterpolateF32(CalcRandomF32(&pEntry0->v[0], r0), pKey1->value[0], t, interp);
        r0 = r0 * 0x343FD + 0x269EC3;
        interp >>= 2;
        pTarget[1] = InterpolateF32(CalcRandomF32(&pEntry0->v[1], r0), pKey1->value[1], t, interp);
        r0 = r0 * 0x343FD + 0x269EC3;
        interp >>= 2;
        pTarget[2] = InterpolateF32(CalcRandomF32(&pEntry0->v[2], r0), pKey1->value[2], t, interp);
    } else if (!isRandom0 && isRandom1) {
        u32 r1;
        const AnimCurveRandomF32x3* pEntry1 = GetRandomEntry<AnimCurveRandomF32x3>(
            pKey1, pRandom, pRandomTable,
            CalcRandomBase(seed, pHeader->randomSeed), nextLoop, r1);
        pTarget[0] = InterpolateF32(pKey0->value[0], CalcRandomF32(&pEntry1->v[0], r1), t, interp);
        r1 = r1 * 0x343FD + 0x269EC3;
        interp >>= 2;
        pTarget[1] = InterpolateF32(pKey0->value[1], CalcRandomF32(&pEntry1->v[1], r1), t, interp);
        r1 = r1 * 0x343FD + 0x269EC3;
        interp >>= 2;
        pTarget[2] = InterpolateF32(pKey0->value[2], CalcRandomF32(&pEntry1->v[2], r1), t, interp);
    } else {
        u32 r0;
        const AnimCurveRandomF32x3* pEntry0 = GetRandomEntry<AnimCurveRandomF32x3>(
            pKey0, pRandom, pRandomTable,
            CalcRandomBase(seed, pHeader->randomSeed), loop, r0);
        u32 r1;
        const AnimCurveRandomF32x3* pEntry1 = GetRandomEntry<AnimCurveRandomF32x3>(
            pKey1, pRandom, pRandomTable,
            CalcRandomBase(seed, pHeader->randomSeed), nextLoop, r1);
        pTarget[0] = InterpolateF32(CalcRandomF32(&pEntry0->v[0], r0), CalcRandomF32(&pEntry1->v[0], r1), t, interp);
        r0 = r0 * 0x343FD + 0x269EC3;
        r1 = r1 * 0x343FD + 0x269EC3;
        interp >>= 2;
        pTarget[1] = InterpolateF32(CalcRandomF32(&pEntry0->v[1], r0), CalcRandomF32(&pEntry1->v[1], r1), t, interp);
        r0 = r0 * 0x343FD + 0x269EC3;
        r1 = r1 * 0x343FD + 0x269EC3;
        interp >>= 2;
        pTarget[2] = InterpolateF32(CalcRandomF32(&pEntry0->v[2], r0), CalcRandomF32(&pEntry1->v[2], r1), t, interp);
    }
        return;
    }

    if (pTheKey->random == 0) {
        pTarget[0] = pTheKey->value[0];
        pTarget[1] = pTheKey->value[1];
        pTarget[2] = pTheKey->value[2];
    } else {
        u32 r;
        const AnimCurveRandomF32x3* pEntry = GetRandomEntry<AnimCurveRandomF32x3>(
            pTheKey, pRandom, pRandomTable,
            CalcRandomBase(seed, pHeader->randomSeed), loop, r);
        pTarget[0] = CalcRandomF32(&pEntry->v[0], r);
        r = r * 0x343FD + 0x269EC3;
        pTarget[1] = CalcRandomF32(&pEntry->v[1], r);
        r = r * 0x343FD + 0x269EC3;
        pTarget[2] = CalcRandomF32(&pEntry->v[2], r);
    }
}

// Key with a variable number of components (AnimCurveExecuteF32)
struct AnimCurveKeyF32N {
    u16 frame;  // at 0x0
    u16 interp; // at 0x2
    u8 PADDING_0x4[2];
    u8 random; // at 0x6
    u8 PADDING_0x7;
    union {
        f32 value[1];  // at 0x8
        u16 randomIdx; // at 0x8
    };
};

inline AnimCurveKeyF32N* GetKeyN(u8* pKeys, int idx, int keySize) {
    return reinterpret_cast<AnimCurveKeyF32N*>(pKeys + idx * keySize);
}

inline const AnimCurveRandomF32*
GetRandomEntryN(const AnimCurveKeyF32N* pKey, u8* pRandom, u8* pRandomTable,
                u32 base, u32 loop, u32& rRandom, int randomSize) {
    bool useTable = pKey->random & AC_KEY_RANDOM_TABLE;

    AnimCurveRandomSeed rnd;
    rnd.value = base + loop * 0x7B929 + pKey->randomIdx * 0x371097E7 + 0x4BF53;
    rnd.bytes[2] ^= rnd.bytes[3];
    rnd.bytes[1] ^= rnd.bytes[2];
    rnd.bytes[0] ^= rnd.bytes[1];

    rRandom = rnd.value;

    if (!useTable) {
        return reinterpret_cast<AnimCurveRandomF32*>(
            pRandom + 4 + pKey->randomIdx * randomSize);
    }

    u16 num = *reinterpret_cast<u16*>(pRandomTable);
    const AnimCurveRandomF32* pEntry = reinterpret_cast<AnimCurveRandomF32*>(
        pRandomTable + 4 + randomSize * ((rRandom >> 16) % num));
    rRandom = rRandom * 0x343FD + 0x269EC3;
    return pEntry;
}

void AnimCurveExecuteF32(u8* pCmdList, f32* pTarget, u32 tick, u16 seed,
                         u32 life, u8 ctrl) {
    AnimCurveHeader* pHeader = reinterpret_cast<AnimCurveHeader*>(pCmdList);

    u8* pKey = pCmdList + sizeof(AnimCurveHeader);
    u8* pRandom = pKey + pHeader->keyTable;
    u8* pRandomTable = pRandom + pHeader->rangeTable;

    u32 loop = 0;
    u32 nextLoop;
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

            time = TurnTime(time, flag, loop, turnLen);
            frame = time;
        }
    }

    int numComp = 8;
    u8 mask = 0x80;

    while (mask != 0) {
        if (ctrl & mask) {
            break;
        }

        mask >>= 1;
        numComp--;
    }

    if (numComp == 0) {
        return;
    }

    int keySize = numComp * sizeof(f32) + 8;
    int randomSize = numComp * sizeof(AnimCurveRandomF32);

    int f = frame;
    AnimCurveKey* pKeyTable = reinterpret_cast<AnimCurveKey*>(pKey);
    u8* pKeys;

    int idx = pKeyTable->count - 1;
    int mid = idx / 2;
    pKeys = pKeyTable->datas;
    int lo = 0;
    bool exact = static_cast<f32>(__fabs(f - time)) < NW4R_MATH_FLT_EPSILON;

    int frame0 = GetKeyN(pKeys, 0, keySize)->frame;
    int frame1;

    if (f < frame0) {
        idx = 0;
        exact = true;
    } else if (f == frame0) {
        if (idx == 0) {
            exact = true;
        } else if (!exact) {
            frame1 = GetKeyN(pKeys, 1, keySize)->frame;
        }

        idx = 0;
    } else {
        frame1 = GetKeyN(pKeys, idx, keySize)->frame;

        if (frame1 <= f) {
            exact = true;
        } else {
            int val = GetKeyN(pKeys, mid, keySize)->frame;

            while (lo < mid) {
                if (f == val) {
                    idx = mid;

                    if (!exact) {
                        frame0 = val;
                        frame1 = GetKeyN(pKeys, mid + 1, keySize)->frame;
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
                val = GetKeyN(pKeys, mid, keySize)->frame;
            }

            idx = lo;
            exact = false;
        }
    }
found:
    AnimCurveKeyF32N* pTheKey;

    if (exact) {
        pTheKey = GetKeyN(pKeys, idx, keySize);
    } else {
    nextLoop = loop;
    u8 flag = pHeader->processFlag;

    if ((flag & AnimCurveHeader::PROC_FLAG_TURN) &&
        ((flag & AnimCurveHeader::PROC_FLAG_INFLOOP) ||
         pHeader->loopCount > 1)) {

        if (!(loop & 1) && idx + 1 >= *reinterpret_cast<u16*>(pKey) - 1 &&
            ((flag & AnimCurveHeader::PROC_FLAG_INFLOOP) ||
             loop < pHeader->loopCount - 1)) {
            nextLoop = loop + 1;
        }

        if ((loop & 1) && idx == 0 && loop != 0) {
            loop++;
        }
    }

    f32 t = (time - static_cast<u16>(frame0)) /
            (static_cast<u16>(frame1) - static_cast<u16>(frame0));

    AnimCurveKeyF32N* pKey0 = GetKeyN(pKeys, idx, keySize);
    AnimCurveKeyF32N* pKey1 = GetKeyN(pKeys, idx + 1, keySize);
    u16 interp = pKey0->interp;
    bool isRandom0 = pKey0->random != 0;
    bool isRandom1 = pKey1->random != 0;
    f32* pValue0 = pKey0->value;
    f32* pValue1 = pKey1->value;

    if (!isRandom0 && !isRandom1) {
        for (; mask != 0; interp >>= 2, mask >>= 1, pTarget++) {
            if (ctrl & mask) {
                *pTarget = InterpolateKeyF32(*pValue0, *pValue1, t, interp);
                pValue0++;
                pValue1++;
            }
        }
    } else if (isRandom0 && !isRandom1) {
        u32 r0;
        const AnimCurveRandomF32* pEntry0 = GetRandomEntryN(
            pKey0, pRandom, pRandomTable,
            CalcRandomBase(seed, pHeader->randomSeed), loop, r0, randomSize);

        for (; mask != 0; interp >>= 2, mask >>= 1, pTarget++) {
            if (ctrl & mask) {
                f32 v0 = CalcRandomF32(pEntry0, r0);
                r0 = r0 * 0x343FD + 0x269EC3;
                pEntry0++;
                *pTarget = InterpolateF32(v0, *pValue1, t, interp);
                pValue1++;
            }
        }
    } else if (!isRandom0 && isRandom1) {
        u32 r1;
        const AnimCurveRandomF32* pEntry1 = GetRandomEntryN(
            pKey1, pRandom, pRandomTable,
            CalcRandomBase(seed, pHeader->randomSeed), nextLoop, r1,
            randomSize);

        for (; mask != 0; interp >>= 2, mask >>= 1, pTarget++) {
            if (ctrl & mask) {
                f32 v1 = CalcRandomF32(pEntry1, r1);
                r1 = r1 * 0x343FD + 0x269EC3;
                pEntry1++;
                *pTarget = InterpolateF32(*pValue0, v1, t, interp);
                pValue0++;
            }
        }
    } else {
        u32 r0;
        const AnimCurveRandomF32* pEntry0 = GetRandomEntryN(
            pKey0, pRandom, pRandomTable,
            CalcRandomBase(seed, pHeader->randomSeed), loop, r0, randomSize);
        u32 r1;
        const AnimCurveRandomF32* pEntry1 = GetRandomEntryN(
            pKey1, pRandom, pRandomTable,
            CalcRandomBase(seed, pHeader->randomSeed), nextLoop, r1,
            randomSize);

        for (; mask != 0; interp >>= 2, mask >>= 1, pTarget++) {
            if (ctrl & mask) {
                f32 v0 = CalcRandomF32(pEntry0, r0);
                f32 v1 = CalcRandomF32(pEntry1, r1);
                r0 = r0 * 0x343FD + 0x269EC3;
                r1 = r1 * 0x343FD + 0x269EC3;
                pEntry0++;
                pEntry1++;
                *pTarget = InterpolateF32(v0, v1, t, interp);
            }
        }
    }
        return;
    }

    f32* pValue = pTheKey->value;

    if (pTheKey->random == 0) {
        for (; mask != 0; mask >>= 1, pValue++, pTarget++) {
            if (ctrl & mask) {
                *pTarget = *pValue;
            }
        }
    } else {
        u32 r;
        const AnimCurveRandomF32* pEntry = GetRandomEntryN(
            pTheKey, pRandom, pRandomTable,
            CalcRandomBase(seed, pHeader->randomSeed), loop, r, randomSize);

        for (; mask != 0; mask >>= 1, pTarget++) {
            if (ctrl & mask) {
                *pTarget = CalcRandomF32(pEntry, r);
                r = r * 0x343FD + 0x269EC3;
                pEntry++;
            }
        }
    }
}

struct AnimCurveRandomRotate {
    AnimCurveRandomF32 v[3]; // at 0x0
    u8 randomSign;           // at 0x18
    u8 PADDING_0x19[3];
};

inline bool GetRandomSign(const AnimCurveRandomRotate* pEntry, u32& rRandom) {
    bool neg = false;

    if (pEntry->randomSign) {
        if (!(rRandom & 0x10000)) {
            neg = true;
        }

        rRandom = rRandom * 0x343FD + 0x269EC3;
    }

    return neg;
}

inline f32 CalcRandomRotate(const AnimCurveRandomF32* pEntry, u32 random,
                            bool neg) {
    f32 value = CalcRandomF32(pEntry, random);

    if (neg) {
        value = -value;
    }

    return value;
}

void AnimCurveExecuteRotate(u8* pCmdList, f32* pTarget, u32 tick, u16 seed,
                            u32 life) {
    AnimCurveHeader* pHeader = reinterpret_cast<AnimCurveHeader*>(pCmdList);

    u8* pKey = pCmdList + sizeof(AnimCurveHeader);
    u8* pRandom = pKey + pHeader->keyTable;
    u8* pRandomTable = pRandom + pHeader->rangeTable;

    u32 loop = 0;
    u32 nextLoop;
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

            time = TurnTime(time, flag, loop, turnLen);
            frame = time;
        }
    }

    int f = frame;
    AnimCurveKey* pKeyTable = reinterpret_cast<AnimCurveKey*>(pKey);
    AnimCurveKeyF32x3* pKeys;

    int idx = pKeyTable->count - 1;
    int mid = idx / 2;
    pKeys = reinterpret_cast<AnimCurveKeyF32x3*>(pKeyTable->datas);
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
    AnimCurveKeyF32x3* pTheKey;

    if (exact) {
        pTheKey = &pKeys[idx];
    } else {
    nextLoop = loop;
    u8 flag = pHeader->processFlag;

    if ((flag & AnimCurveHeader::PROC_FLAG_TURN) &&
        ((flag & AnimCurveHeader::PROC_FLAG_INFLOOP) ||
         pHeader->loopCount > 1)) {

        if (!(loop & 1) && idx + 1 >= *reinterpret_cast<u16*>(pKey) - 1 &&
            ((flag & AnimCurveHeader::PROC_FLAG_INFLOOP) ||
             loop < pHeader->loopCount - 1)) {
            nextLoop = loop + 1;
        }

        if ((loop & 1) && idx == 0 && loop != 0) {
            loop++;
        }
    }

    f32 t = (time - static_cast<u16>(frame0)) /
            (static_cast<u16>(frame1) - static_cast<u16>(frame0));

    AnimCurveKeyF32x3* pKey0 = &pKeys[idx];
    AnimCurveKeyF32x3* pKey1 = &pKeys[idx + 1];
    u16 interp = pKey0->interp;
    bool isRandom0 = pKey0->random != 0;
    bool isRandom1 = pKey1->random != 0;

    if (!isRandom0 && !isRandom1) {
        pTarget[0] = InterpolateKeyF32(pKey0->value[0], pKey1->value[0], t, interp);
        interp >>= 2;
        pTarget[1] = InterpolateKeyF32(pKey0->value[1], pKey1->value[1], t, interp);
        interp >>= 2;
        pTarget[2] = InterpolateKeyF32(pKey0->value[2], pKey1->value[2], t, interp);
    } else if (isRandom0 && !isRandom1) {
        u32 r0;
        const AnimCurveRandomRotate* pEntry0 = GetRandomEntry<AnimCurveRandomRotate>(
            pKey0, pRandom, pRandomTable,
            CalcRandomBase(seed, pHeader->randomSeed), loop, r0);
        bool neg0 = GetRandomSign(pEntry0, r0);
        pTarget[0] = InterpolateF32(CalcRandomRotate(&pEntry0->v[0], r0, neg0), pKey1->value[0], t, interp);
        r0 = r0 * 0x343FD + 0x269EC3;
        interp >>= 2;
        pTarget[1] = InterpolateF32(CalcRandomRotate(&pEntry0->v[1], r0, neg0), pKey1->value[1], t, interp);
        r0 = r0 * 0x343FD + 0x269EC3;
        interp >>= 2;
        pTarget[2] = InterpolateF32(CalcRandomRotate(&pEntry0->v[2], r0, neg0), pKey1->value[2], t, interp);
    } else if (!isRandom0 && isRandom1) {
        u32 r1;
        const AnimCurveRandomRotate* pEntry1 = GetRandomEntry<AnimCurveRandomRotate>(
            pKey1, pRandom, pRandomTable,
            CalcRandomBase(seed, pHeader->randomSeed), nextLoop, r1);
        bool neg1 = GetRandomSign(pEntry1, r1);
        pTarget[0] = InterpolateF32(pKey0->value[0], CalcRandomRotate(&pEntry1->v[0], r1, neg1), t, interp);
        r1 = r1 * 0x343FD + 0x269EC3;
        interp >>= 2;
        pTarget[1] = InterpolateF32(pKey0->value[1], CalcRandomRotate(&pEntry1->v[1], r1, neg1), t, interp);
        r1 = r1 * 0x343FD + 0x269EC3;
        interp >>= 2;
        pTarget[2] = InterpolateF32(pKey0->value[2], CalcRandomRotate(&pEntry1->v[2], r1, neg1), t, interp);
    } else {
        u32 r0;
        const AnimCurveRandomRotate* pEntry0 = GetRandomEntry<AnimCurveRandomRotate>(
            pKey0, pRandom, pRandomTable,
            CalcRandomBase(seed, pHeader->randomSeed), loop, r0);
        bool neg0 = GetRandomSign(pEntry0, r0);
        u32 r1;
        const AnimCurveRandomRotate* pEntry1 = GetRandomEntry<AnimCurveRandomRotate>(
            pKey1, pRandom, pRandomTable,
            CalcRandomBase(seed, pHeader->randomSeed), nextLoop, r1);
        bool neg1 = GetRandomSign(pEntry1, r1);
        pTarget[0] = InterpolateF32(CalcRandomRotate(&pEntry0->v[0], r0, neg0), CalcRandomRotate(&pEntry1->v[0], r1, neg1), t, interp);
        r0 = r0 * 0x343FD + 0x269EC3;
        r1 = r1 * 0x343FD + 0x269EC3;
        interp >>= 2;
        pTarget[1] = InterpolateF32(CalcRandomRotate(&pEntry0->v[1], r0, neg0), CalcRandomRotate(&pEntry1->v[1], r1, neg1), t, interp);
        r0 = r0 * 0x343FD + 0x269EC3;
        r1 = r1 * 0x343FD + 0x269EC3;
        interp >>= 2;
        pTarget[2] = InterpolateF32(CalcRandomRotate(&pEntry0->v[2], r0, neg0), CalcRandomRotate(&pEntry1->v[2], r1, neg1), t, interp);
    }
        return;
    }

    if (pTheKey->random == 0) {
        pTarget[0] = pTheKey->value[0];
        pTarget[1] = pTheKey->value[1];
        pTarget[2] = pTheKey->value[2];
    } else {
        u32 r;
        const AnimCurveRandomRotate* pEntry = GetRandomEntry<AnimCurveRandomRotate>(
            pTheKey, pRandom, pRandomTable,
            CalcRandomBase(seed, pHeader->randomSeed), loop, r);
        bool neg = GetRandomSign(pEntry, r);
        pTarget[0] = CalcRandomRotate(&pEntry->v[0], r, neg);
        r = r * 0x343FD + 0x269EC3;
        pTarget[1] = CalcRandomRotate(&pEntry->v[1], r, neg);
        r = r * 0x343FD + 0x269EC3;
        pTarget[2] = CalcRandomRotate(&pEntry->v[2], r, neg);
    }
}

struct AnimCurveKeyF32 {
    u16 frame;  // at 0x0
    u16 interp; // at 0x2
    u8 PADDING_0x4[2];
    u8 random; // at 0x6
    u8 PADDING_0x7;
    union {
        f32 value;     // at 0x8
        u16 randomIdx; // at 0x8
    };
};

void AnimCurveExecuteF32x1(u8* pCmdList, Particle* pParticle, f32* pTarget,
                           u32 tick, u16 seed, u32 life) {
    AnimCurveCacheF32* pCache = reinterpret_cast<AnimCurveCacheF32*>(pParticle);

    AnimCurveHeader* pHeader = reinterpret_cast<AnimCurveHeader*>(pCmdList);

    u8* pKey = pCmdList + sizeof(AnimCurveHeader);
    u8* pRandom = pKey + pHeader->keyTable;
    u8* pRandomTable = pRandom + pHeader->rangeTable;

    u32 loop = 0;
    u32 nextLoop;
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

            time = TurnTime(time, flag, loop, turnLen);
            frame = time;
        }
    }

    int f = frame;
    AnimCurveKey* pKeyTable = reinterpret_cast<AnimCurveKey*>(pKey);
    AnimCurveKeyF32* pKeys;

    if (pKeyTable->count == 0) {
        return;
    }

    int idx = pKeyTable->count - 1;
    int mid = idx / 2;
    pKeys = reinterpret_cast<AnimCurveKeyF32*>(pKeyTable->datas);
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
    AnimCurveKeyF32* pTheKey;

    if (exact) {
        pTheKey = &pKeys[idx];
    } else {
    nextLoop = loop;
    u8 flag = pHeader->processFlag;

    if ((flag & AnimCurveHeader::PROC_FLAG_TURN) &&
        ((flag & AnimCurveHeader::PROC_FLAG_INFLOOP) ||
         pHeader->loopCount > 1)) {

        if (!(loop & 1) && idx + 1 >= *reinterpret_cast<u16*>(pKey) - 1 &&
            ((flag & AnimCurveHeader::PROC_FLAG_INFLOOP) ||
             loop < pHeader->loopCount - 1)) {
            nextLoop = loop + 1;
        }

        if ((loop & 1) && idx == 0 && loop != 0) {
            loop++;
        }
    }

    f32 t = (time - static_cast<u16>(frame0)) /
            (static_cast<u16>(frame1) - static_cast<u16>(frame0));

    AnimCurveKeyF32* pKey0 = &pKeys[idx];
    AnimCurveKeyF32* pKey1 = &pKeys[idx + 1];
    u16 interp = pKey0->interp;
    bool isRandom0 = pKey0->random != 0;
    bool isRandom1 = pKey1->random != 0;

    if (!isRandom0 && !isRandom1) {
        if (pCache != NULL) {
            pCache->dirty = false;
        }

        f32 v0 = pKey0->value;
        f32 v1 = pKey1->value;
        f32 value;

        if (v0 == v1) {
            value = v0;
        } else {
            value = InterpolateF32(v0, v1, t, interp);
        }

        *pTarget = value;

        if (pCache != NULL) {
            pCache->value = value;
        }
    } else if (isRandom0 && !isRandom1) {
        u32 r0;
        const AnimCurveRandomF32* pEntry0 = GetRandomEntry<AnimCurveRandomF32>(
            pKey0, pRandom, pRandomTable,
            CalcRandomBase(seed, pHeader->randomSeed), loop, r0);
        f32 v0 = CalcRandomF32(pEntry0, r0);
        *pTarget = InterpolateF32(v0, pKey1->value, t, interp);
    } else if (!isRandom0 && isRandom1) {
        u32 r1;
        const AnimCurveRandomF32* pEntry1 = GetRandomEntry<AnimCurveRandomF32>(
            pKey1, pRandom, pRandomTable,
            CalcRandomBase(seed, pHeader->randomSeed), nextLoop, r1);
        f32 v1 = CalcRandomF32(pEntry1, r1);
        *pTarget = InterpolateF32(pKey0->value, v1, t, interp);
    } else {
        u32 r0;
        const AnimCurveRandomF32* pEntry0 = GetRandomEntry<AnimCurveRandomF32>(
            pKey0, pRandom, pRandomTable,
            CalcRandomBase(seed, pHeader->randomSeed), loop, r0);
        u32 r1;
        const AnimCurveRandomF32* pEntry1 = GetRandomEntry<AnimCurveRandomF32>(
            pKey1, pRandom, pRandomTable,
            CalcRandomBase(seed, pHeader->randomSeed), nextLoop, r1);
        f32 v0 = CalcRandomF32(pEntry0, r0);
        f32 v1 = CalcRandomF32(pEntry1, r1);
        *pTarget = InterpolateF32(v0, v1, t, interp);
    }
        return;
    }

    if (pTheKey->random == 0) {
        *pTarget = pTheKey->value;
    } else {
        u32 r;
        const AnimCurveRandomF32* pEntry = GetRandomEntry<AnimCurveRandomF32>(
            pTheKey, pRandom, pRandomTable,
            CalcRandomBase(seed, pHeader->randomSeed), loop, r);
        *pTarget = CalcRandomF32(pEntry, r);
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

struct AnimCurveChildData {
    EmitterInheritSetting setting; // at 0x0
    u16 nameIdx;                   // at 0x8
};

struct AnimCurveKeyChild {
    u16 frame; // at 0x0
    u8 PADDING_0x2[4];
    u8 random; // at 0x6
    u8 PADDING_0x7;
    union {
        AnimCurveChildData data; // at 0x8
        u16 randomIdx;           // at 0x8
    };
};

void createChild(u8* pKey, u16 seed, AnimCurveHeader* pHeader,
                 AnimCurveNameTable* pNameTable,
                 AnimCurveRandomTable* pRandomTable, Particle* pParticle,
                 u32 loop) {
    AnimCurveKeyChild* pChildKey = reinterpret_cast<AnimCurveKeyChild*>(pKey);
    AnimCurveChildData* pData;

    if (pChildKey->random == 0) {
        pData = &pChildKey->data;
    } else {
        AnimCurveRandomSeed rnd;
        rnd.value = seed * 0x3F81F635 + pHeader->randomSeed * 0x30A74193 +
                    0x4BF53 +
                    (loop * 0x7B929 + pChildKey->randomIdx * 0x371097E7);
        rnd.bytes[2] ^= rnd.bytes[3];
        rnd.bytes[1] ^= rnd.bytes[2];
        rnd.bytes[0] ^= rnd.bytes[1];

        if (pChildKey->random & AC_KEY_RANDOM_TABLE) {
            if (pRandomTable->count == 0) {
                return;
            }

            pData = reinterpret_cast<AnimCurveChildData*>(pRandomTable->datas) +
                    (rnd.value >> 16) % pRandomTable->count;
        }
    }

    EmitterResource* pResource = reinterpret_cast<EmitterResource*>(
        pNameTable->datas[pData->nameIdx].work);

    if (pResource == NULL) {
        return;
    }

    if (pData->setting.type == 0) {
        pParticle->mParticleManager->mManagerEM->mManagerEF->mManagerES
            ->mCreationQueue.AddParticleCreation(&pData->setting, pParticle,
                                                 pResource,
                                                 pParticle->mCalcRemain);
    } else {
        pParticle->mParticleManager->mManagerEM->mManagerEF->mManagerES
            ->mCreationQueue.AddEmitterCreation(&pData->setting, pParticle,
                                                pResource,
                                                pParticle->mCalcRemain);
    }
}

inline u16 CalcFrameChild(AnimCurveHeader* pHeader, u32 tick, u32 life,
                          u32& loop, bool& rEnd) {
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
                if (pHeader->loopCount % 2 == 0) {
                    if (loop == pHeader->loopCount &&
                        tick - loop * turnLen == 0) {
                        rEnd = true;
                    }

                    frame = 0;
                } else {
                    frame = static_cast<u8>(turnLen);
                }

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
                if (loopCount % 2 == 0) {
                    if (tick == life - 1) {
                        rEnd = true;
                    }

                    frame = 0;
                } else {
                    frame = static_cast<u8>(turnLen);
                }

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

// Index of the first key at or after the frame
inline int SearchKeyChildFwd(const AnimCurveKeyChild* pKeys, u16 numKey,
                             int frame) {
    int hi = numKey - 1;
    int lo = 0;
    int mid = hi / 2;

    if (frame < pKeys[0].frame || frame == pKeys[0].frame) {
        return 0;
    }

    if (pKeys[hi].frame < frame) {
        return hi;
    }

    int val = pKeys[mid].frame;

    while (lo < mid) {
        if (frame == val) {
            hi = mid;
        } else {
            if (val < frame) {
                lo = mid;
            }
            if (val >= frame) {
                hi = mid;
            }
        }

        mid = (lo + hi) / 2;
        val = pKeys[mid].frame;
    }

    return lo;
}

// Index of the last key at or before the frame
inline int SearchKeyChildRev(const AnimCurveKeyChild* pKeys, u16 numKey,
                             int frame) {
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
        } else {
            if (val < frame) {
                lo = mid;
            }
            if (val >= frame) {
                hi = mid;
            }
        }

        mid = (lo + hi) / 2;
        val = pKeys[mid].frame;
    }

    return lo;
}

void AnimCurveExecuteChild(u8* pCmdList, Particle* pParticle, u32 tick,
                           u16 seed, u32 life) {
    AnimCurveHeader* pHeader = reinterpret_cast<AnimCurveHeader*>(pCmdList);

    u8* pKey = pCmdList + sizeof(AnimCurveHeader);
    u8* pRandom = pKey + pHeader->keyTable;
    AnimCurveRandomTable* pRandomTable =
        reinterpret_cast<AnimCurveRandomTable*>(pRandom + pHeader->rangeTable);
    AnimCurveNameTable* pNameTable = reinterpret_cast<AnimCurveNameTable*>(
        reinterpret_cast<u8*>(pRandomTable) + pHeader->randomTable);

    u32 loop0 = 0;
    bool end = false;
    u16 frame0 = CalcFrameChild(pHeader, tick, life, loop0, end);

    u32 loop1 = 0;
    bool end1 = false;
    u16 frame1 = CalcFrameChild(pHeader, tick + 1, life, loop1, end1);

    AnimCurveKeyChild* pKeys = reinterpret_cast<AnimCurveKeyChild*>(pKey + 4);
    AnimCurveKeyChild* pIt;
    int i;

    if (loop0 == loop1) {
        if (frame0 == frame1) {
            if (!end) {
                return;
            }

            frame1 = 1;
        }

        if (frame0 < frame1) {
            i = SearchKeyChildFwd(pKeys, *reinterpret_cast<u16*>(pKey), frame0);

            for (pIt = &pKeys[i]; i < *reinterpret_cast<u16*>(pKey);
                 i++, pIt++) {
                if (pIt->frame >= frame0) {
                    if (pIt->frame >= frame1) {
                        return;
                    }

                    createChild(reinterpret_cast<u8*>(pIt), seed, pHeader,
                                pNameTable, pRandomTable, pParticle, loop0);
                }
            }
        } else {
            i = SearchKeyChildRev(pKeys, *reinterpret_cast<u16*>(pKey), frame0);

            for (pIt = &pKeys[i]; i >= 0; i--, pIt--) {
                if (frame0 >= pIt->frame) {
                    if (pIt->frame <= frame1) {
                        return;
                    }

                    createChild(reinterpret_cast<u8*>(pIt), seed, pHeader,
                                pNameTable, pRandomTable, pParticle, loop0);
                }
            }
        }

        return;
    }

    if (pHeader->processFlag & AnimCurveHeader::PROC_FLAG_TURN) {
        bool odd = loop0 & 1;

        if (!odd) {
            i = SearchKeyChildFwd(pKeys, *reinterpret_cast<u16*>(pKey), frame0);

            for (pIt = &pKeys[i]; i < *reinterpret_cast<u16*>(pKey);
                 i++, pIt++) {
                if (pIt->frame >= frame0) {
                    if (pIt->frame == pHeader->frameLength - 1) {
                        break;
                    }

                    createChild(reinterpret_cast<u8*>(pIt), seed, pHeader,
                                pNameTable, pRandomTable, pParticle, loop0);
                }
            }
        } else {
            i = SearchKeyChildRev(pKeys, *reinterpret_cast<u16*>(pKey), frame0);

            for (pIt = &pKeys[i]; i >= 0; i--, pIt--) {
                if (frame0 >= pIt->frame) {
                    if (pIt->frame == 0) {
                        break;
                    }

                    createChild(reinterpret_cast<u8*>(pIt), seed, pHeader,
                                pNameTable, pRandomTable, pParticle, loop0);
                }
            }
        }

        for (u32 l = loop0 + 1; l < loop1; l++) {
            if (!odd) {
                for (pIt = pKeys, i = 0; i < *reinterpret_cast<u16*>(pKey);
                     i++, pIt++) {
                    if (pIt->frame == pHeader->frameLength - 1) {
                        break;
                    }

                    createChild(reinterpret_cast<u8*>(pIt), seed, pHeader,
                                pNameTable, pRandomTable, pParticle, loop0);
                }
            } else {
                i = *reinterpret_cast<u16*>(pKey) - 1;

                for (pIt = &pKeys[i]; i >= 0; i--, pIt--) {
                    if (pIt->frame == 0) {
                        break;
                    }

                    createChild(reinterpret_cast<u8*>(pIt), seed, pHeader,
                                pNameTable, pRandomTable, pParticle, loop0);
                }
            }
        }

        if (!(loop1 & 1)) {
            for (pIt = pKeys, i = 0; i < *reinterpret_cast<u16*>(pKey);
                 i++, pIt++) {
                if (pIt->frame >= frame1) {
                    return;
                }

                createChild(reinterpret_cast<u8*>(pIt), seed, pHeader,
                            pNameTable, pRandomTable, pParticle, loop0);
            }
        } else {
            i = *reinterpret_cast<u16*>(pKey) - 1;

            for (pIt = &pKeys[i]; i >= 0; i--, pIt--) {
                if (pIt->frame <= frame1) {
                    return;
                }

                createChild(reinterpret_cast<u8*>(pIt), seed, pHeader,
                            pNameTable, pRandomTable, pParticle, loop0);
            }
        }
    } else {
        i = SearchKeyChildFwd(pKeys, *reinterpret_cast<u16*>(pKey), frame0);

        for (pIt = &pKeys[i]; i < *reinterpret_cast<u16*>(pKey); i++, pIt++) {
            if (pIt->frame >= frame0) {
                createChild(reinterpret_cast<u8*>(pIt), seed, pHeader,
                            pNameTable, pRandomTable, pParticle, loop0);
            }
        }

        for (u32 l = loop0 + 1; l < loop1; l++) {
            for (pIt = pKeys, i = 0; i < *reinterpret_cast<u16*>(pKey);
                 i++, pIt++) {
                createChild(reinterpret_cast<u8*>(pIt), seed, pHeader,
                            pNameTable, pRandomTable, pParticle, loop0);
            }
        }

        for (pIt = pKeys, i = 0; i < *reinterpret_cast<u16*>(pKey); i++, pIt++) {
            if (pIt->frame >= frame1) {
                return;
            }

            createChild(reinterpret_cast<u8*>(pIt), seed, pHeader, pNameTable,
                        pRandomTable, pParticle, loop0);
        }
    }
}

} // namespace ef
} // namespace nw4r
