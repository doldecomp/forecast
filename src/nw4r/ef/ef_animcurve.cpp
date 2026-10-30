#include <nw4r/ef.h>

#include <cmath>

namespace nw4r {
namespace ef {

namespace {

// Key of a float curve with N components
template <int N> struct AnimCurveKeyF32 {
    u16 frame;       // at 0x0
    u16 interp;      // at 0x2, two bits per component
    u16 PADDING_0x4; // at 0x4
    u8 random;       // at 0x6
    u8 PADDING_0x7;  // at 0x7
    union {
        f32 value[N]; // at 0x8
        u16 rangeIdx; // at 0x8
    };
};

// Random range of a float curve with N components
template <int N> struct AnimCurveRangeF32 {
    struct {
        f32 base;  // at 0x0
        f32 width; // at 0x4
    } comp[N];
};

// Random range of a rotation curve
struct AnimCurveRangeRotate {
    struct {
        f32 base;  // at 0x0
        f32 width; // at 0x4
    } comp[3];
    u8 randomSign;     // at 0x18
    u8 PADDING_0x19[3]; // at 0x19
};

// Key of a byte curve with N components
template <int N> struct AnimCurveKeyU8 {
    u16 frame;       // at 0x0
    u16 interp;      // at 0x2, two bits per component
    u16 PADDING_0x4; // at 0x4
    u8 random;       // at 0x6
    u8 PADDING_0x7;  // at 0x7
    union {
        u8 value[N];  // at 0x8
        u16 rangeIdx; // at 0x8
    };
};

struct AnimCurveRangeCompU8 {
    u8 base;  // at 0x0
    u8 width; // at 0x1, scaled by a signed random in [-1, 1)
};

// Random range of a byte curve with N components
template <int N> struct AnimCurveRangeU8 {
    AnimCurveRangeCompU8 comp[N];
};

// Key of a texture curve
struct AnimCurveKeyTexture {
    u16 frame;       // at 0x0
    u16 PADDING_0x2; // at 0x2
    u16 PADDING_0x4; // at 0x4
    u8 random;       // at 0x6
    u8 PADDING_0x7;  // at 0x7
    union {
        struct {
            u8 wrap;    // at 0x8
            u8 reverse; // at 0x9
        };
        u16 rangeIdx; // at 0x8
    };
    u16 name; // at 0xA
};

struct AnimCurveRangeTexture {
    u8 wrap;          // at 0x0
    u8 reverse;       // at 0x1
    u16 name;         // at 0x2
    u8 randomReverse; // at 0x4
    u8 PADDING_0x5[3]; // at 0x5
};

struct AnimCurveRandomTexture {
    u8 wrap;    // at 0x0
    u8 reverse; // at 0x1
    u16 name;   // at 0x2
};

// Child created by a child curve
struct AnimCurveChild {
    EmitterInheritSetting setting; // at 0x0
    u16 resIdx;                    // at 0x8, index into the name table
};

// Key of a child curve
struct AnimCurveKeyChild {
    u16 frame;       // at 0x0
    u16 PADDING_0x2; // at 0x2
    u16 PADDING_0x4; // at 0x4
    u8 random;       // at 0x6
    u8 PADDING_0x7;  // at 0x7
    union {
        AnimCurveChild child; // at 0x8
        u16 rangeIdx;         // at 0x8
    };
};

template <typename T> struct AnimCurveTable {
    u16 count;       // at 0x0
    u16 PADDING_0x2; // at 0x2
    T datas[1];      // at 0x4
};

enum InterpType {
    INTERP_LINEAR,
    INTERP_SMOOTH,
    INTERP_STEP,
};

enum KeyRandomFlag {
    KEY_RANDOM_FLAG_TABLE = (1 << 1),
};

} // namespace

// Finds the key at or before the frame. Returns whether the key value can be
// used as is (no interpolation).
template <typename TKey>
static inline bool SearchKey(AnimCurveTable<TKey>* pKeyTbl, int frame,
                             f32 time, int& rIdx, u16& rPrevFrame,
                             u16& rNextFrame) {
    bool exact;
    int lo;
    int hi;
    int mid;
    TKey* pKeys;

    pKeys = pKeyTbl->datas;

    lo = 0;
    hi = pKeyTbl->count - 1;
    mid = hi / 2;

    exact =
        std::fabs(static_cast<f32>(frame) - time) < NW4R_MATH_FLT_EPSILON;

    rPrevFrame = pKeys[0].frame;

    if (frame < pKeys[0].frame) {
        rIdx = 0;
        return true;
    }

    if (frame == pKeys[0].frame) {
        rIdx = 0;

        if (hi == 0) {
            return true;
        }

        if (!exact) {
            rNextFrame = pKeys[1].frame;
        }
        return exact;
    }

    rNextFrame = pKeys[hi].frame;
    if (rNextFrame <= frame) {
        rIdx = hi;
        return true;
    }

    int midFrame = pKeys[mid].frame;

    while (lo < mid) {
        if (frame == midFrame) {
            rIdx = mid;

            if (!exact) {
                rPrevFrame = midFrame;
                rNextFrame = pKeys[mid + 1].frame;
            }
            return exact;
        }

        if (midFrame < frame) {
            lo = mid;
            rPrevFrame = midFrame;
        } else {
            hi = mid;
            rNextFrame = midFrame;
        }

        mid = (lo + hi) / 2;
        midFrame = pKeys[mid].frame;
    }

    rIdx = lo;
    return false;
}

// Frame of a key in a table with variable sized keys
static inline u16 KeyFrame(u8* pKeys, int idx, int keySize) {
    return *reinterpret_cast<u16*>(pKeys + idx * keySize);
}

// Time of a looping curve stretched over the particle life
static inline f32 CalcFittingLoopTime(AnimCurveHeader* pHeader, u8 flag,
                                      u32 tick, u32 life, u32& rLoop) {
    u32 period = pHeader->frameLength - 1;
    f32 scale = pHeader->loopCount *
                (static_cast<f32>(static_cast<int>(period)) / (life - 1));

    u32 loop = tick * scale / static_cast<int>(period);
    rLoop = loop;

    f32 time = tick * scale - loop * period;

    if (!(flag & AnimCurveHeader::PROC_FLAG_TURN)) {
        return time;
    }

    if (loop % 2 == 0) {
        return time;
    }

    return static_cast<f32>(static_cast<int>(period)) - time;
}

static inline f32 CalcFittingLoopTimeP(AnimCurveHeader* pHeader, u8 flag,
                                       u32 tick, u32 life, u32 period,
                                       u32& rLoop) {
    f32 scale = pHeader->loopCount *
                (static_cast<f32>(static_cast<int>(period)) / (life - 1));

    u32 loop = tick * scale / static_cast<int>(period);
    rLoop = loop;

    f32 time = tick * scale - loop * period;

    if (!(flag & AnimCurveHeader::PROC_FLAG_TURN)) {
        return time;
    }

    if (loop % 2 == 0) {
        return time;
    }

    return static_cast<f32>(static_cast<int>(period)) - time;
}

// Fitting curves: the last tick holds the end frame, the ticks before it
// spread the loops over the particle life (CalcFittingLoopTime)
static inline void CalcFittingTime(AnimCurveHeader* pHeader, u8 flag,
                                   u16 frameLength, u32 tick, u32 life,
                                   u16& rFrame, u32& rLoop, f32& rTime) {
    u32 last = life - 1;

    if (tick >= last) {
        if (!(flag & AnimCurveHeader::PROC_FLAG_TURN) ||
            pHeader->loopCount % 2 != 0) {
            rFrame = static_cast<u8>(frameLength - 1);
        } else {
            rFrame = 0;
        }

        rTime = rFrame;
        rLoop = static_cast<u8>(pHeader->loopCount - 1);
    } else {
        u32 period = pHeader->frameLength - 1;
        f32 scale = pHeader->loopCount *
                    (static_cast<f32>(static_cast<int>(period)) / last);

        u32 loop = tick * scale / static_cast<int>(period);
        rLoop = loop;

        f32 time = tick * scale - loop * period;

        if (!(flag & AnimCurveHeader::PROC_FLAG_TURN)) {
            rTime = time;
        } else if (loop % 2 == 0) {
            rTime = time;
        } else {
            rTime = static_cast<f32>(static_cast<int>(period)) - time;
        }

        rFrame = rTime;
    }
}

// CalcTime for curves longer than one frame, with the process flag passed in
static inline void CalcTimeWithFlag(AnimCurveHeader* pHeader, u8 flag,
                                    u16 frameLength, u32 tick, u32 life,
                                    u16& rFrame, u32& rLoop, f32& rTime) {
    u8 loopCount;
    u32 period;

    if (!(flag & AnimCurveHeader::PROC_FLAG_INFLOOP) &&
        pHeader->loopCount <= 1) {

        if (!(flag & AnimCurveHeader::PROC_FLAG_FITTING)) {
            rFrame = tick;
            if (tick >= frameLength - 1) {
                rFrame = frameLength - 1;
            }
            rTime = rFrame;
        } else {
            rTime = tick * (static_cast<f32>(frameLength - 1) / (life - 1));

            if (rTime > static_cast<f32>(frameLength - 1)) {
                rTime = static_cast<f32>(frameLength - 1);
            }

            rFrame = rTime;
        }
    } else if (!(flag & AnimCurveHeader::PROC_FLAG_FITTING)) {
        period = frameLength - 1;
        rLoop = tick / period;

        if (!(flag & AnimCurveHeader::PROC_FLAG_TURN)) {
            if (!(flag & AnimCurveHeader::PROC_FLAG_INFLOOP) &&
                rLoop >= pHeader->loopCount) {
                rFrame = period;
                rLoop = static_cast<u8>(pHeader->loopCount - 1);
            } else {
                rFrame = tick - rLoop * period;
            }
        } else {
            if (!(flag & AnimCurveHeader::PROC_FLAG_INFLOOP) &&
                rLoop >= (loopCount = pHeader->loopCount)) {
                if (loopCount % 2 == 0) {
                    rFrame = 0;
                } else {
                    rFrame = period;
                }
                rLoop = static_cast<u8>(loopCount - 1);
            } else if (rLoop % 2 == 0) {
                rFrame = tick - rLoop * period;
            } else {
                rFrame = period * (rLoop + 1) - tick;
            }
        }

        rTime = rFrame;
    } else {
        CalcFittingTime(pHeader, flag, frameLength, tick, life, rFrame, rLoop,
                        rTime);
    }
}

// Converts the particle tick to a curve time (loop, turn and fitting modes)
static inline f32 CalcTime(AnimCurveHeader* pHeader, u32 tick, u32 life,
                           u16& rFrame, u32& rLoop) {
    u16 frameLength = pHeader->frameLength;
    rLoop = 0;
    f32 time;

    if (frameLength <= 1) {
        time = 0.0f;
        rLoop = tick;
        rFrame = 0;
    } else {
        u8 flag = pHeader->processFlag;

        if (!(flag & AnimCurveHeader::PROC_FLAG_INFLOOP) &&
            pHeader->loopCount <= 1) {

            if (!(flag & AnimCurveHeader::PROC_FLAG_FITTING)) {
                rFrame = tick;
                if (tick >= frameLength - 1) {
                    rFrame = frameLength - 1;
                }
                time = rFrame;
            } else {
                time = tick * (static_cast<f32>(frameLength - 1) / (life - 1));

                if (time > static_cast<f32>(frameLength - 1)) {
                    time = static_cast<f32>(frameLength - 1);
                }

                rFrame = time;
            }
        } else if (!(flag & AnimCurveHeader::PROC_FLAG_FITTING)) {
            u32 period = frameLength - 1;
            rLoop = tick / period;

            if (!(flag & AnimCurveHeader::PROC_FLAG_TURN)) {
                if (!(flag & AnimCurveHeader::PROC_FLAG_INFLOOP) &&
                    rLoop >= pHeader->loopCount) {
                    rFrame = period;
                    rLoop = static_cast<u8>(pHeader->loopCount - 1);
                } else {
                    rFrame = tick - rLoop * period;
                }
            } else {
                if (!(flag & AnimCurveHeader::PROC_FLAG_INFLOOP) &&
                    rLoop >= pHeader->loopCount) {
                    if (pHeader->loopCount % 2 == 0) {
                        rFrame = 0;
                    } else {
                        rFrame = period;
                    }
                    rLoop = static_cast<u8>(pHeader->loopCount - 1);
                } else if (rLoop % 2 == 0) {
                    rFrame = tick - rLoop * period;
                } else {
                    rFrame = period * (rLoop + 1) - tick;
                }
            }

            time = rFrame;
        } else if (tick >= life - 1) {
            if (!(flag & AnimCurveHeader::PROC_FLAG_TURN) ||
                pHeader->loopCount % 2 != 0) {
                rFrame = static_cast<u8>(frameLength - 1);
            } else {
                rFrame = 0;
            }

            time = rFrame;
            rLoop = static_cast<u8>(pHeader->loopCount - 1);
        } else {
            time = CalcFittingLoopTime(pHeader, flag, tick, life, rLoop);
            rFrame = time;
        }
    }


    return time;
}

// Turning curves play the keys backwards on odd loops, so the random seed of
// the key on the far side belongs to the neighbouring loop.
static inline void AdjustTurnLoop(u8* pCmdList, AnimCurveKey* pKeyTbl, int idx,
                                  u32 loop, u32& rLoop0, u32& rLoop1) {
    AnimCurveHeader* pHeader = reinterpret_cast<AnimCurveHeader*>(pCmdList);
    u8 flag = pHeader->processFlag;

    if (flag & AnimCurveHeader::PROC_FLAG_TURN) {
        if ((flag & AnimCurveHeader::PROC_FLAG_INFLOOP) ||
            pHeader->loopCount > 1) {

            if (!(loop & 1) && idx + 1 >= pKeyTbl->count - 1) {
                if ((flag & AnimCurveHeader::PROC_FLAG_INFLOOP) ||
                    loop < pHeader->loopCount - 1) {
                    rLoop1 = loop + 1;
                }
            }

            if ((loop & 1) && idx == 0 && loop != 0) {
                rLoop0 = loop + 1;
            }
        }
    }
}

// Integer frame of the step curves (texture and child), returned as a time
// like CalcTime does. rEnd is set on the last tick of an even number of turns.
static inline f32 CalcFrameStep(u8* pCmdList, u32 tick, u32 life,
                                u16& rFrame, u32& rLoop, bool& rEnd) {
    AnimCurveHeader* pHeader = reinterpret_cast<AnimCurveHeader*>(pCmdList);
    u8 flag = pHeader->processFlag;
    rLoop = 0;
    rEnd = false;

    if (!(flag & AnimCurveHeader::PROC_FLAG_INFLOOP) &&
        pHeader->loopCount <= 1) {

        if (!(flag & AnimCurveHeader::PROC_FLAG_FITTING)) {
            if (tick >= pHeader->frameLength) {
                rFrame = pHeader->frameLength;
            } else {
                rFrame = tick;
            }
        } else {
            rFrame = pHeader->frameLength * tick / life;
        }
    } else if (!(flag & AnimCurveHeader::PROC_FLAG_FITTING)) {
        u16 frameLength = pHeader->frameLength;

        if (frameLength <= 1 || !(flag & AnimCurveHeader::PROC_FLAG_TURN)) {
            rLoop = tick / frameLength;

            if (!(flag & AnimCurveHeader::PROC_FLAG_INFLOOP) &&
                rLoop >= pHeader->loopCount) {
                rLoop = static_cast<u8>(pHeader->loopCount - 1);
                rFrame = frameLength;
            } else {
                rFrame = tick - rLoop * frameLength;
            }
        } else {
            rLoop = tick / (frameLength - 1);

            if (!(flag & AnimCurveHeader::PROC_FLAG_INFLOOP) &&
                rLoop >= pHeader->loopCount) {
                if (pHeader->loopCount % 2 == 0) {
                    if (rLoop == pHeader->loopCount &&
                        tick - rLoop * (frameLength - 1) == 0) {
                        rEnd = true;
                    }
                    rFrame = 0;
                } else {
                    rFrame = static_cast<u8>(frameLength - 1);
                }
                rLoop = static_cast<u8>(pHeader->loopCount - 1);
            } else if (rLoop % 2 == 0) {
                rFrame = tick - rLoop * (frameLength - 1);
            } else {
                rFrame = (frameLength - 1) - (tick - rLoop * (frameLength - 1));
            }
        }
    } else if (tick >= life) {
        if (!(flag & AnimCurveHeader::PROC_FLAG_TURN) ||
            pHeader->loopCount % 2 != 0) {
            rFrame = pHeader->frameLength;
        } else {
            rFrame = 0;
        }
        rLoop = static_cast<u8>(pHeader->loopCount - 1);
    } else {
        u16 frameLength = pHeader->frameLength;

        if (frameLength <= 1 || !(flag & AnimCurveHeader::PROC_FLAG_TURN)) {
            f32 scale =
                pHeader->loopCount *
                (static_cast<f32>(frameLength) / static_cast<f32>(life));

            u32 loop = tick * scale / frameLength;
            rLoop = loop;
            rFrame = tick * scale - loop * frameLength;
        } else {
            u8 loopCount = pHeader->loopCount;
            u32 period = frameLength - 1;

            f32 scale =
                (1.0f + static_cast<f32>(static_cast<int>(period)) * loopCount) /
                life;
            u32 pos = tick * scale;
            rLoop = pos / period;

            if (rLoop >= loopCount) {
                if (loopCount % 2 == 0) {
                    if (tick == life - 1) {
                        rEnd = true;
                    }
                    rFrame = 0;
                } else {
                    rFrame = static_cast<u8>(period);
                }
                rLoop = static_cast<u8>(loopCount - 1);
            } else if (rLoop % 2 == 0) {
                rFrame = pos - rLoop * period;
            } else {
                rFrame = period * (rLoop + 1) - pos;
            }
        }
    }

    return rFrame;
}

// Step curves: binary search for the frame. With lowerBound set this finds
// the first key at or after the frame, otherwise the last key at or before it.
template <typename TKey>
static inline int SearchKeyStep(TKey* pKeys, u16 numKey, int frame,
                                bool lowerBound) {
    int hi = numKey - 1;
    int lo = 0;
    int mid = hi / 2;
    int firstFrame = pKeys[0].frame;

    if (frame < firstFrame || (frame == firstFrame && lowerBound)) {
        return 0;
    }

    if (pKeys[hi].frame < frame ||
        (frame == pKeys[hi].frame && !lowerBound)) {
        return hi;
    }

    int midFrame = pKeys[mid].frame;

    while (lo < mid) {
        if (frame == midFrame) {
            if (lowerBound) {
                hi = mid;
            } else {
                lo = mid;
            }
        } else {
            if (midFrame < frame) {
                lo = mid;
            }
            if (midFrame >= frame) {
                hi = mid;
            }
        }

        mid = (lo + hi) / 2;
        midFrame = pKeys[mid].frame;
    }

    return lo;
}

// Seed of a key's random range
static inline u32 CalcRandomSeed(u16 seed, u16 randomSeed, u32 rangeIdx,
                                 u32 loop) {
    return seed * 0x3F81F635 + randomSeed * 0x30A74193 +
           rangeIdx * 0x371097E7 + loop * 0x7B929 + 0x4BF53;
}

static inline u32 MixRandom(u32 rnd) {
    return rnd * 0x343FD + 0x269EC3;
}

// Picks the random range of a key. rRnd receives the random value.
template <typename TRange, typename TKey>
static inline TRange* GetRandomRange(AnimCurveHeader* pHeader,
                                     AnimCurveTable<TRange>* pRangeTbl,
                                     AnimCurveTable<TRange>* pRandomTbl,
                                     TKey* pKey, u32 loop, u16 seed,
                                     u32& rRnd) {
    union {
        u32 value;
        u8 bytes[4];
    } rndSeed;

    u32 rangeIdx = pKey->rangeIdx;
    rndSeed.value = CalcRandomSeed(seed, pHeader->randomSeed, rangeIdx, loop);

    rndSeed.bytes[2] ^= rndSeed.bytes[3];
    rndSeed.bytes[1] ^= rndSeed.bytes[2];
    rndSeed.bytes[0] ^= rndSeed.bytes[1];

    u32 rnd = rndSeed.value;
    TRange* pRange;

    if (!(pKey->random & KEY_RANDOM_FLAG_TABLE)) {
        pRange = &pRangeTbl->datas[pKey->rangeIdx];
    } else {
        pRange = &pRandomTbl->datas[(rnd >> 16) % pRandomTbl->count];
        rnd = MixRandom(rnd);
    }

    rRnd = rnd;
    return pRange;
}

// Picks the random range of a key. rRnd receives the random value.
template <typename TRange, typename TKey>
static inline TRange* GetRandomRangeW(AnimCurveHeader* pHeader,
                                     AnimCurveTable<TRange>* pRangeTbl,
                                     AnimCurveTable<TRange>* pRandomTbl,
                                     TKey* pKey, u32 loop, u16 seed,
                                     u32& rRnd) {
    union {
        u32 value;
        u8 bytes[4];
    } rndSeed;

    u32 rangeIdx = pKey->rangeIdx;
    rndSeed.value = CalcRandomSeed(seed, pHeader->randomSeed, rangeIdx, loop);

    rndSeed.bytes[2] ^= rndSeed.bytes[3];
    rndSeed.bytes[1] ^= rndSeed.bytes[2];
    rndSeed.bytes[0] ^= rndSeed.bytes[1];

    rRnd = rndSeed.value;
    TRange* pRange;

    if (!(pKey->random & KEY_RANDOM_FLAG_TABLE)) {
        pRange = &pRangeTbl->datas[pKey->rangeIdx];
    } else {
        pRange = &pRandomTbl->datas[(rRnd >> 16) % pRandomTbl->count];
        rRnd = MixRandom(rRnd);
    }

    return pRange;
}

// GetRandomRange with the range index read through the key values
template <typename TRange>
static inline TRange* GetRandomRangeV(AnimCurveHeader* pHeader,
                                      AnimCurveTable<TRange>* pRangeTbl,
                                      AnimCurveTable<TRange>* pRandomTbl,
                                      u8 random, u16* pRangeIdx, u32 loop,
                                      u16 seed, u32& rRnd) {
    union {
        u32 value;
        u8 bytes[4];
    } rndSeed;

    u32 rangeIdx = *pRangeIdx;
    rndSeed.value = CalcRandomSeed(seed, pHeader->randomSeed, rangeIdx, loop);

    rndSeed.bytes[2] ^= rndSeed.bytes[3];
    rndSeed.bytes[1] ^= rndSeed.bytes[2];
    rndSeed.bytes[0] ^= rndSeed.bytes[1];

    u32 rnd = rndSeed.value;
    TRange* pRange;

    if (!(random & KEY_RANDOM_FLAG_TABLE)) {
        pRange = &pRangeTbl->datas[*pRangeIdx];
    } else {
        pRange = &pRandomTbl->datas[(rnd >> 16) % pRandomTbl->count];
        rnd = MixRandom(rnd);
    }

    rRnd = rnd;
    return pRange;
}

// GetRandomRangeV mixing rRnd in place, as GetRandomRangeW does
template <typename TRange>
static inline TRange* GetRandomRangeVW(AnimCurveHeader* pHeader,
                                        AnimCurveTable<TRange>* pRangeTbl,
                                        AnimCurveTable<TRange>* pRandomTbl,
                                        u8 random, u16* pRangeIdx, u32 loop,
                                        u16 seed, u32& rRnd) {
    union {
        u32 value;
        u8 bytes[4];
    } rndSeed;

    u32 rangeIdx = *pRangeIdx;
    rndSeed.value = CalcRandomSeed(seed, pHeader->randomSeed, rangeIdx, loop);

    rndSeed.bytes[2] ^= rndSeed.bytes[3];
    rndSeed.bytes[1] ^= rndSeed.bytes[2];
    rndSeed.bytes[0] ^= rndSeed.bytes[1];

    rRnd = rndSeed.value;
    TRange* pRange;

    if (!(random & KEY_RANDOM_FLAG_TABLE)) {
        pRange = &pRangeTbl->datas[*pRangeIdx];
    } else {
        pRange = &pRandomTbl->datas[(rRnd >> 16) % pRandomTbl->count];
        rRnd = MixRandom(rRnd);
    }

    return pRange;
}

// GetRandomRange for ranges of variable size
static inline AnimCurveRangeF32<1>*
GetRandomRangeStride(AnimCurveHeader* pHeader, AnimCurveTable<u8>* pRangeTbl,
                     AnimCurveTable<u8>* pRandomTbl, int rangeSize, u8 random,
                     u16* pRangeIdx, u32 loop, u16 seed, u32& rRnd) {
    union {
        u32 value;
        u8 bytes[4];
    } rndSeed;

    u32 rangeIdx = *pRangeIdx;
    rndSeed.value = CalcRandomSeed(seed, pHeader->randomSeed, rangeIdx, loop);

    rndSeed.bytes[2] ^= rndSeed.bytes[3];
    rndSeed.bytes[1] ^= rndSeed.bytes[2];
    rndSeed.bytes[0] ^= rndSeed.bytes[1];

    u32 rnd = rndSeed.value;
    u8* pRange;

    if (!(random & KEY_RANDOM_FLAG_TABLE)) {
        pRange = pRangeTbl->datas + *pRangeIdx * rangeSize;
    } else {
        pRange = pRandomTbl->datas + (rnd >> 16) % pRandomTbl->count * rangeSize;
        rnd = MixRandom(rnd);
    }

    rRnd = rnd;
    return reinterpret_cast<AnimCurveRangeF32<1>*>(pRange);
}

// GetRandomRangeStride taking the range index already read
static inline AnimCurveRangeF32<1>*
GetRandomRangeStrideIdx(AnimCurveHeader* pHeader,
                        AnimCurveTable<u8>* pRangeTbl,
                        AnimCurveTable<u8>* pRandomTbl, int rangeSize,
                        u8 random, u32 rangeIdx, u32 loop, u16 seed,
                        u32& rRnd) {
    union {
        u32 value;
        u8 bytes[4];
    } rndSeed;

    rndSeed.value = CalcRandomSeed(seed, pHeader->randomSeed, rangeIdx, loop);

    rndSeed.bytes[2] ^= rndSeed.bytes[3];
    rndSeed.bytes[1] ^= rndSeed.bytes[2];
    rndSeed.bytes[0] ^= rndSeed.bytes[1];

    u32 rnd = rndSeed.value;
    u8* pRange;

    if (!(random & KEY_RANDOM_FLAG_TABLE)) {
        pRange = pRangeTbl->datas + rangeIdx * rangeSize;
    } else {
        pRange = pRandomTbl->datas + (rnd >> 16) % pRandomTbl->count * rangeSize;
        rnd = MixRandom(rnd);
    }

    rRnd = rnd;
    return reinterpret_cast<AnimCurveRangeF32<1>*>(pRange);
}

// Rotation ranges can pick a random sign. rRnd advances when they do.
static inline bool GetRandomSign(AnimCurveRangeRotate* pRange, u32& rRnd) {
    bool negate = false;

    if (pRange->randomSign) {
        if (!(rRnd & 0x10000)) {
            negate = true;
        }
        rRnd = MixRandom(rRnd);
    }

    return negate;
}

// Random part of a range component
static inline f32 ScaleRnd(f32 width, u32 rnd) {
    return width * (rnd >> 16);
}

template <typename TRange>
static inline f32 GetRangeValueF32(TRange* pRange, int i, u32 rnd) {
    return pRange->comp[i].base + pRange->comp[i].width * (rnd >> 16);
}

template <typename TRange, typename TKey>
static inline f32 CalcRandomF32(AnimCurveHeader* pHeader,
                                AnimCurveTable<TRange>* pRangeTbl,
                                AnimCurveTable<TRange>* pRandomTbl,
                                TKey* pKey, u32 loop, u16 seed) {
    u32 rnd;
    TRange* pRange = GetRandomRange(pHeader, pRangeTbl, pRandomTbl, pKey,
                                    loop, seed, rnd);
    return GetRangeValueF32(pRange, 0, rnd);
}

// Value of a range component (read into locals first); rRnd advances
template <typename TRange>
static inline f32 NextRangeValueF32N(TRange* pRange, int i, u32& rRnd) {
    f32 base = pRange->comp[i].base;
    f32 width = pRange->comp[i].width;
    f32 value = base + width * (rRnd >> 16);
    rRnd = MixRandom(rRnd);
    return value;
}

static inline void MixRandomRef(u32& rRnd) {
    rRnd = rRnd * 0x343FD + 0x269EC3;
}

// Byte curves use a 16.16 fixed point t
static inline u8 InterpolateU8(u16 interp, u32 t, u8 v0, u8 v1) {
    switch (interp & 3) {
    case INTERP_LINEAR: {
        return v0 + ((t * (v1 - v0)) >> 16);
    }
    case INTERP_SMOOTH: {
        f32 ft = t / 65536.0f;
        return v0 + ft * (ft * ((3.0f - 2.0f * ft) * (v1 - v0)));
    }
    case INTERP_STEP: {
        return v0;
    }
    default: {
        return 0;
    }
    }
}

static inline int GetRangeValueU8(AnimCurveRangeCompU8* pComp, u32 rnd) {
    int value =
        pComp->base + pComp->width * static_cast<s16>(rnd >> 16) / 32768;

    if (value < 0) {
        value = 0;
    }
    if (value > 255) {
        value = 255;
    }

    return value;
}

static inline f32 InterpolateF32(u16 interp, f32 t, f32 v0, f32 v1) {
    switch (interp & 3) {
    case INTERP_LINEAR: {
        return v0 + t * (v1 - v0);
    }
    case INTERP_SMOOTH: {
        return v0 + t * (t * ((3.0f - 2.0f * t) * (v1 - v0)));
    }
    case INTERP_STEP: {
        return v0;
    }
    default: {
        return 0.0f;
    }
    }
}

static inline f32 InterpolateConstF32(u16 interp, f32 t, f32 v0, f32 v1) {
    if (v0 == v1) {
        return v0;
    }

    return InterpolateF32(interp, t, v0, v1);
}

// InterpolateConstF32 reading both values through pointers first
static inline f32 InterpolateConstPtrF32(u16 interp, f32 t, f32* pValue0,
                                         f32* pValue1) {
    f32 v0 = *pValue0;
    f32 v1 = *pValue1;
    if (v0 == v1) {
        return v0;
    }
    return InterpolateF32(interp, t, v0, v1);
}

// InterpolateConstF32 reading both key values first
template <int N>
static inline f32 InterpolateConstKeyF32(u16 interp, f32 t,
                                         AnimCurveKeyF32<N>* pKey0,
                                         AnimCurveKeyF32<N>* pKey1, int i) {
    f32 v0 = pKey0->value[i];
    f32 v1 = pKey1->value[i];
    if (v0 == v1) {
        return v0;
    }
    return InterpolateF32(interp, t, v0, v1);
}

// Finds the key at or before the frame. Returns whether the key value can be
// used as is (no interpolation).
template <typename TKey>
static inline bool SearchKeyU8(AnimCurveTable<TKey>* pKeyTbl, int frame,
                             f32 time, int& rIdx, u16& rPrevFrame,
                             u16& rNextFrame) {
    bool exact;
    int lo;
    int hi;
    int mid;
    TKey* pKeys;

    pKeys = pKeyTbl->datas;

    lo = 0;
    hi = pKeyTbl->count - 1;
    mid = hi / 2;

    exact =
        std::fabs(static_cast<f32>(frame) - time) < NW4R_MATH_FLT_EPSILON;

    rPrevFrame = pKeys[0].frame;

    if (frame < pKeys[0].frame) {
        rIdx = 0;
        return true;
    }

    if (frame == pKeys[0].frame) {
        rIdx = 0;

        if (hi == 0) {
            return true;
        }

        if (!exact) {
            rNextFrame = pKeys[1].frame;
        }
        return exact;
    }

    rNextFrame = pKeys[hi].frame;
    if (rNextFrame <= frame) {
        rIdx = hi;
        return true;
    }

    int midFrame = pKeys[mid].frame;

    while (lo < mid) {
        if (frame == midFrame) {
            rIdx = mid;

            if (!exact) {
                rPrevFrame = midFrame;
                rNextFrame = pKeys[mid + 1].frame;
            }
            return exact;
        }

        if (midFrame < frame) {
            lo = mid;
            rPrevFrame = midFrame;
        } else {
            hi = mid;
            rNextFrame = midFrame;
        }

        mid = (lo + hi) / 2;
        midFrame = pKeys[mid].frame;
    }

    rIdx = lo;
    return false;
}

static inline bool SearchKeyStride(AnimCurveTable<u8>* pKeyTbl, int keySize,
                                   int frame,
                             f32 time, int& rIdx, u16& rPrevFrame,
                             u16& rNextFrame) {
    int lo;
    bool exact;
    int hi;
    int mid;
    u8* pKeys;

    pKeys = pKeyTbl->datas;

    lo = 0;
    hi = pKeyTbl->count - 1;
    mid = hi / 2;

    exact =
        std::fabs(static_cast<f32>(frame) - time) < NW4R_MATH_FLT_EPSILON;

    rPrevFrame = KeyFrame(pKeys, 0, keySize);

    if (frame < KeyFrame(pKeys, 0, keySize)) {
        rIdx = 0;
        return true;
    }

    if (frame == KeyFrame(pKeys, 0, keySize)) {
        rIdx = 0;

        if (hi == 0) {
            return true;
        }

        if (!exact) {
            rNextFrame = KeyFrame(pKeys, 1, keySize);
        }
        return exact;
    }

    rNextFrame = KeyFrame(pKeys, hi, keySize);
    if (rNextFrame <= frame) {
        rIdx = hi;
        return true;
    }

    int midFrame = KeyFrame(pKeys, mid, keySize);

    while (lo < mid) {
        if (frame == midFrame) {
            rIdx = mid;

            if (!exact) {
                rPrevFrame = midFrame;
                rNextFrame = KeyFrame(pKeys, mid + 1, keySize);
            }
            return exact;
        }

        if (midFrame < frame) {
            lo = mid;
            rPrevFrame = midFrame;
        } else {
            hi = mid;
            rNextFrame = midFrame;
        }

        mid = (lo + hi) / 2;
        midFrame = KeyFrame(pKeys, mid, keySize);
    }

    rIdx = lo;
    return false;
}

// Time of a looping curve stretched over the particle life
static inline f32 CalcFittingLoopTimeU8(AnimCurveHeader* pHeader, u8 flag,
                                      u32 tick, u32 life, u32 period,
                                       u32& rLoop) {
    f32 scale = pHeader->loopCount *
                (static_cast<f32>(static_cast<int>(period)) / (life - 1));

    u32 loop = tick * scale / static_cast<int>(period);
    rLoop = loop;

    f32 time = tick * scale - loop * period;

    if (!(flag & AnimCurveHeader::PROC_FLAG_TURN)) {
        return time;
    }

    if (loop % 2 == 0) {
        return time;
    }

    return static_cast<f32>(static_cast<int>(period)) - time;
}

// Picks the random range of a key. rRnd receives the random value.
template <typename TRange, typename TKey>
static inline TRange* GetRandomRangeU8(AnimCurveHeader* pHeader,
                                     AnimCurveTable<TRange>* pRangeTbl,
                                     AnimCurveTable<TRange>* pRandomTbl,
                                     TKey* pKey, u32 loop, u16 seed,
                                     u32& rRnd) {
    union {
        u32 value;
        u8 bytes[4];
    } rndSeed;

    u32 rangeIdx = pKey->rangeIdx;
    rndSeed.value = CalcRandomSeed(seed, pHeader->randomSeed, rangeIdx, loop);

    rndSeed.bytes[2] ^= rndSeed.bytes[3];
    rndSeed.bytes[1] ^= rndSeed.bytes[2];
    rndSeed.bytes[0] ^= rndSeed.bytes[1];

    rRnd = rndSeed.value;
    TRange* pRange;

    if (!(pKey->random & KEY_RANDOM_FLAG_TABLE)) {
        pRange = &pRangeTbl->datas[pKey->rangeIdx];
    } else {
        pRange = &pRandomTbl->datas[(rRnd >> 16) % pRandomTbl->count];
        rRnd = MixRandom(rRnd);
    }

    return pRange;
}

// Not referenced by the channel; the linker strips them. They come first in
// the file and give .sdata2 its constant order.
f32 AnimCurveCalcTime(u8* pCmdList, u32 tick, u32 life) {
    u16 frame;
    u32 loop;
    return CalcTime(reinterpret_cast<AnimCurveHeader*>(pCmdList), tick, life,
                    frame, loop);
}

u16 AnimCurveCalcFrameStep(u8* pCmdList, u32 tick, u32 life) {
    u16 frame;
    u32 loop;
    bool end;
    CalcFrameStep(pCmdList, tick, life, frame, loop, end);
    return frame;
}

void AnimCurveExecuteU8Vec3(u8* pCmdList, u8* pTarget, u32 tick, u16 seed,
                        u32 life) {
    u32 period;
    AnimCurveTable<AnimCurveKeyU8<3> >* pKeyTbl;
    AnimCurveTable<AnimCurveRangeU8<3> >* pRangeTbl;
    AnimCurveTable<AnimCurveRangeU8<3> >* pRandomTbl;
    u32 loop0;
    bool random1;
    AnimCurveRangeU8<3>* pRange0;
    bool found;
    u32 rnd1;
    u32 t;
    u32 loop;
    AnimCurveHeader* pHeader = reinterpret_cast<AnimCurveHeader*>(pCmdList);
    u32 loop1;

    pKeyTbl =
        reinterpret_cast<AnimCurveTable<AnimCurveKeyU8<3> >*>(
            pCmdList + sizeof(AnimCurveHeader));
    pRangeTbl =
        reinterpret_cast<AnimCurveTable<AnimCurveRangeU8<3> >*>(
            reinterpret_cast<u8*>(pKeyTbl) + pHeader->keyTable);
    pRandomTbl =
        reinterpret_cast<AnimCurveTable<AnimCurveRangeU8<3> >*>(
            reinterpret_cast<u8*>(pRangeTbl) + pHeader->rangeTable);
    u16 frame;
    f32 time;
    {
        u16 frameLength = pHeader->frameLength;
        loop = 0;

        if (frameLength <= 1) {
            time = 0.0f;
            loop = tick;
            frame = 0;
        } else {

            if (!(pHeader->processFlag & AnimCurveHeader::PROC_FLAG_INFLOOP) &&
                pHeader->loopCount <= 1) {

                if (!(pHeader->processFlag & AnimCurveHeader::PROC_FLAG_FITTING)) {
                    frame = tick;
                    if (tick >= frameLength - 1) {
                        frame = frameLength - 1;
                    }
                    time = frame;
                } else {
                    time = tick * (static_cast<f32>(frameLength - 1) / (life - 1));

                    if (time > static_cast<f32>(frameLength - 1)) {
                        time = static_cast<f32>(frameLength - 1);
                    }

                    frame = time;
                }
            } else if (!(pHeader->processFlag & AnimCurveHeader::PROC_FLAG_FITTING)) {
                loop = tick / (frameLength - 1);

                if (!(pHeader->processFlag & AnimCurveHeader::PROC_FLAG_TURN)) {
                    if (!(pHeader->processFlag & AnimCurveHeader::PROC_FLAG_INFLOOP) &&
                        loop >= pHeader->loopCount) {
                        frame = frameLength - 1;
                        loop = static_cast<u8>(pHeader->loopCount - 1);
                    } else {
                        frame = tick - loop * (frameLength - 1);
                    }
                } else {
                    if (!(pHeader->processFlag & AnimCurveHeader::PROC_FLAG_INFLOOP) &&
                        loop >= pHeader->loopCount) {
                        if (pHeader->loopCount % 2 == 0) {
                            frame = 0;
                        } else {
                            frame = frameLength - 1;
                        }
                        loop = static_cast<u8>(pHeader->loopCount - 1);
                    } else if (loop % 2 == 0) {
                        frame = tick - loop * (frameLength - 1);
                    } else {
                        frame = (frameLength - 1) * (loop + 1) - tick;
                    }
                }

                time = frame;
            } else if (tick >= life - 1) {
                if (!(pHeader->processFlag & AnimCurveHeader::PROC_FLAG_TURN) ||
                    pHeader->loopCount % 2 != 0) {
                    frame = static_cast<u8>(frameLength - 1);
                } else {
                    frame = 0;
                }

                time = frame;
                loop = static_cast<u8>(pHeader->loopCount - 1);
            } else {
                period = pHeader->frameLength - 1;
                time = CalcFittingLoopTimeU8(pHeader, pHeader->processFlag, tick, life, period, loop);
                frame = time;
            }
        }
    }

    AnimCurveKeyU8<3>* pKeys = pKeyTbl->datas;

    int idx;
    u16 prevFrame;
    u16 nextFrame;
    found = SearchKeyU8(pKeyTbl, frame, time, idx, prevFrame, nextFrame);

    AnimCurveKeyU8<3>* pKey;

    if (found) {
        pKey = &pKeys[idx];
    } else {
        loop0 = loop;
        loop1 = loop;
        AdjustTurnLoop(reinterpret_cast<u8*>(pHeader),
                       reinterpret_cast<AnimCurveKey*>(pKeyTbl), idx, loop,
                       loop0, loop1);

        t = static_cast<u32>(65536.0f * (time - prevFrame)) /
                (nextFrame - prevFrame);

        // The random flags are read through the value arrays
        pKey = &pKeys[idx];
        u8* pValue0 = pKey->value;
        AnimCurveKeyU8<3>* pKey0 =
            reinterpret_cast<AnimCurveKeyU8<3>*>(pValue0 - 8);
        u8* pValue1 = pKeys[idx + 1].value;
        AnimCurveKeyU8<3>* pKey1 =
            reinterpret_cast<AnimCurveKeyU8<3>*>(pValue1 - 8);
        u16 interp = pKey->interp;
        bool random0 = pKey0->random != 0;
        random1 = pKey1->random != 0;

        if (!random0 && !random1) {
            for (int i = 0; i < 3; i++, interp >>= 2, pValue0++, pValue1++) {
                if (*pValue0 == *pValue1) {
                    *pTarget++ = *pValue0;
                } else {
                    *pTarget++ = InterpolateU8(interp, t, *pValue0, *pValue1);
                }
            }
        } else if (random0 && !random1) {
            u32 rnd;
            AnimCurveRangeU8<3>* pRange = GetRandomRangeV(
                pHeader, pRangeTbl, pRandomTbl, pKey0->random,
                reinterpret_cast<u16*>(pValue0), loop0, seed, rnd);
            AnimCurveRangeCompU8* pComp = pRange->comp;

            for (int i = 0; i < 3; i++, interp >>= 2, pValue1++) {
                int value = GetRangeValueU8(pComp++, rnd);
                rnd = MixRandom(rnd);
                *pTarget++ = InterpolateU8(interp, t, value, *pValue1);
            }
        } else if (!random0 && random1) {
            u32 rnd;
            AnimCurveRangeU8<3>* pRange = GetRandomRangeV(
                pHeader, pRangeTbl, pRandomTbl, pKey1->random,
                reinterpret_cast<u16*>(pValue1), loop1, seed, rnd);
            AnimCurveRangeCompU8* pComp = pRange->comp;

            for (int i = 0; i < 3; i++, interp >>= 2, pValue0++) {
                // GetRangeValueU8 written out, stepping pComp before the clamp
                int value = pComp->base +
                            pComp->width * static_cast<s16>(rnd >> 16) / 32768;
                pComp++;
                if (value < 0) {
                    value = 0;
                }
                if (value > 255) {
                    value = 255;
                }
                rnd = MixRandom(rnd);
                *pTarget++ = InterpolateU8(interp, t, *pValue0, value);
            }
        } else {
            u32 rnd0;
            pRange0 = GetRandomRangeV(
                pHeader, pRangeTbl, pRandomTbl, pKey0->random,
                reinterpret_cast<u16*>(pValue0), loop0, seed, rnd0);
            AnimCurveRangeCompU8* pComp0 = pRange0->comp;
            AnimCurveRangeU8<3>* pRange1 = GetRandomRangeVW(
                pHeader, pRangeTbl, pRandomTbl, pKey1->random,
                reinterpret_cast<u16*>(pValue1), loop1, seed, rnd1);
            AnimCurveRangeCompU8* pComp1 = pRange1->comp;

            for (int i = 0; i < 3; i++, interp >>= 2) {
                int value0 = GetRangeValueU8(pComp0++, rnd0);
                rnd0 = MixRandom(rnd0);
                int value1 = GetRangeValueU8(pComp1++, rnd1);
                rnd1 = MixRandom(rnd1);
                *pTarget++ = InterpolateU8(interp, t, value0, value1);
            }
        }
        return;
    }

    if (pKey->random == 0) {
        for (int i = 0; i < 3; i++) {
            pTarget[i] = pKey->value[i];
        }
    } else {
        u32 rnd;
        AnimCurveRangeU8<3>* pRange = GetRandomRangeU8(
            pHeader, pRangeTbl, pRandomTbl, pKey, loop, seed, rnd);

        pTarget[0] = GetRangeValueU8(&pRange->comp[0], rnd);
        rnd = MixRandom(rnd);
        pTarget[1] = GetRangeValueU8(&pRange->comp[1], rnd);
        rnd = MixRandom(rnd);
        pTarget[2] = GetRangeValueU8(&pRange->comp[2], rnd);
    }
}

void AnimCurveExecuteU8(u8* pCmdList, u8* pTarget, u32 tick, u16 seed,
                        u32 life) {
    u32 period;
    AnimCurveTable<AnimCurveKeyU8<1> >* pKeyTbl;
    AnimCurveTable<AnimCurveRangeU8<1> >* pRangeTbl;
    AnimCurveTable<AnimCurveRangeU8<1> >* pRandomTbl;
    u32 loop0;
    bool random1;
    AnimCurveRangeU8<1>* pRange0;
    bool found;
    u32 rnd1;
    u32 t;
    u32 loop;
    AnimCurveHeader* pHeader = reinterpret_cast<AnimCurveHeader*>(pCmdList);
    u32 loop1;

    pKeyTbl =
        reinterpret_cast<AnimCurveTable<AnimCurveKeyU8<1> >*>(
            pCmdList + sizeof(AnimCurveHeader));
    pRangeTbl =
        reinterpret_cast<AnimCurveTable<AnimCurveRangeU8<1> >*>(
            reinterpret_cast<u8*>(pKeyTbl) + pHeader->keyTable);
    pRandomTbl =
        reinterpret_cast<AnimCurveTable<AnimCurveRangeU8<1> >*>(
            reinterpret_cast<u8*>(pRangeTbl) + pHeader->rangeTable);
    u16 frame;
    f32 time;
    {
        u16 frameLength = pHeader->frameLength;
        loop = 0;

        if (frameLength <= 1) {
            time = 0.0f;
            loop = tick;
            frame = 0;
        } else {

            if (!(pHeader->processFlag & AnimCurveHeader::PROC_FLAG_INFLOOP) &&
                pHeader->loopCount <= 1) {

                if (!(pHeader->processFlag & AnimCurveHeader::PROC_FLAG_FITTING)) {
                    frame = tick;
                    if (tick >= frameLength - 1) {
                        frame = frameLength - 1;
                    }
                    time = frame;
                } else {
                    time = tick * (static_cast<f32>(frameLength - 1) / (life - 1));

                    if (time > static_cast<f32>(frameLength - 1)) {
                        time = static_cast<f32>(frameLength - 1);
                    }

                    frame = time;
                }
            } else if (!(pHeader->processFlag & AnimCurveHeader::PROC_FLAG_FITTING)) {
                loop = tick / (frameLength - 1);

                if (!(pHeader->processFlag & AnimCurveHeader::PROC_FLAG_TURN)) {
                    if (!(pHeader->processFlag & AnimCurveHeader::PROC_FLAG_INFLOOP) &&
                        loop >= pHeader->loopCount) {
                        frame = frameLength - 1;
                        loop = static_cast<u8>(pHeader->loopCount - 1);
                    } else {
                        frame = tick - loop * (frameLength - 1);
                    }
                } else {
                    if (!(pHeader->processFlag & AnimCurveHeader::PROC_FLAG_INFLOOP) &&
                        loop >= pHeader->loopCount) {
                        if (pHeader->loopCount % 2 == 0) {
                            frame = 0;
                        } else {
                            frame = frameLength - 1;
                        }
                        loop = static_cast<u8>(pHeader->loopCount - 1);
                    } else if (loop % 2 == 0) {
                        frame = tick - loop * (frameLength - 1);
                    } else {
                        frame = (frameLength - 1) * (loop + 1) - tick;
                    }
                }

                time = frame;
            } else if (tick >= life - 1) {
                if (!(pHeader->processFlag & AnimCurveHeader::PROC_FLAG_TURN) ||
                    pHeader->loopCount % 2 != 0) {
                    frame = static_cast<u8>(frameLength - 1);
                } else {
                    frame = 0;
                }

                time = frame;
                loop = static_cast<u8>(pHeader->loopCount - 1);
            } else {
                period = pHeader->frameLength - 1;
                time = CalcFittingLoopTimeU8(pHeader, pHeader->processFlag, tick, life, period, loop);
                frame = time;
            }
        }
    }

    AnimCurveKeyU8<1>* pKeys = pKeyTbl->datas;

    int idx;
    u16 prevFrame;
    u16 nextFrame;
    found = SearchKeyU8(pKeyTbl, frame, time, idx, prevFrame, nextFrame);

    AnimCurveKeyU8<1>* pKey;

    if (found) {
        pKey = &pKeys[idx];
    } else {
        loop0 = loop;
        loop1 = loop;
        AdjustTurnLoop(reinterpret_cast<u8*>(pHeader),
                       reinterpret_cast<AnimCurveKey*>(pKeyTbl), idx, loop,
                       loop0, loop1);

        AnimCurveKeyU8<1>* pKey1;
        t = static_cast<u32>(65536.0f * (time - prevFrame)) /
                (nextFrame - prevFrame);

        AnimCurveKeyU8<1>* pKey0 = &pKeys[idx];
        u16 interp;
        pKey1 = &pKeys[idx + 1];

        bool random0 = pKey0->random != 0;
        random1 = pKey1->random != 0;
        interp = pKey0->interp;

        if (!random0 && !random1) {
            u8 value1 = pKey1->value[0];
            u8 value0 = pKey0->value[0];

            if (value0 == value1) {
                *pTarget = value0;
            } else {
                *pTarget = InterpolateU8(interp, t, value0, value1);
            }
        } else if (random0 && !random1) {
            u32 rnd;
            AnimCurveRangeU8<1>* pRange = GetRandomRangeU8(
                pHeader, pRangeTbl, pRandomTbl, pKey0, loop0, seed, rnd);

            int value = GetRangeValueU8(pRange->comp, rnd);
            *pTarget = InterpolateU8(interp, t, value, pKey1->value[0]);
        } else if (!random0 && random1) {
            u32 rnd;
            AnimCurveRangeU8<1>* pRange = GetRandomRangeU8(
                pHeader, pRangeTbl, pRandomTbl, pKey1, loop1, seed, rnd);

            int value = GetRangeValueU8(pRange->comp, rnd);
            *pTarget = InterpolateU8(interp, t, pKey0->value[0], value);
        } else {
            u32 rnd0;
            pRange0 = GetRandomRangeU8(
                pHeader, pRangeTbl, pRandomTbl, pKey0, loop0, seed, rnd0);
            AnimCurveRangeU8<1>* pRange1 = GetRandomRangeU8(
                pHeader, pRangeTbl, pRandomTbl, pKey1, loop1, seed, rnd1);

            int value0 = GetRangeValueU8(pRange0->comp, rnd0);
            int value1 = GetRangeValueU8(pRange1->comp, rnd1);
            *pTarget = InterpolateU8(interp, t, value0, value1);
        }
        return;
    }

    if (pKey->random == 0) {
        *pTarget = pKey->value[0];
    } else {
        u32 rnd;
        AnimCurveRangeU8<1>* pRange = GetRandomRangeU8(
            pHeader, pRangeTbl, pRandomTbl, pKey, loop, seed, rnd);

        *pTarget = GetRangeValueU8(pRange->comp, rnd);
    }
}

void AnimCurveExecuteF32Vec2(u8* pCmdList, f32* pBase, f32* pTarget, u32 tick,
                             u16 seed, u32 life) {
    AnimCurveHeader* pHeader = reinterpret_cast<AnimCurveHeader*>(pCmdList);
    AnimCurveTable<AnimCurveKeyF32<2> >* pKeyTbl;
    AnimCurveTable<AnimCurveRangeF32<2> >* pRangeTbl;
    AnimCurveTable<AnimCurveRangeF32<2> >* pRandomTbl;
    // The time calculation works on its own copy of tick
    const u32 curTick = tick;
    f32 t;
    u16 frameLength;
    u32 loop;
    bool found;
    f32 value;
    AnimCurveKeyF32<2>* pKey0;
    f32 value1;
    u32 loop0;
    f32 time;
    bool random1;
    AnimCurveKeyF32<2>* pKey;
    u32 loop1;
    AnimCurveKeyF32<2>* pKeys;
    int idx;

    pKeyTbl =
        reinterpret_cast<AnimCurveTable<AnimCurveKeyF32<2> >*>(pHeader + 1);
    pRangeTbl =
        reinterpret_cast<AnimCurveTable<AnimCurveRangeF32<2> >*>(
            reinterpret_cast<u8*>(pKeyTbl) + pHeader->keyTable);
    pRandomTbl =
        reinterpret_cast<AnimCurveTable<AnimCurveRangeF32<2> >*>(
            reinterpret_cast<u8*>(pRangeTbl) + pHeader->rangeTable);

    u16 frame;
    {
        frameLength = pHeader->frameLength;
        loop = 0;

        if (frameLength <= 1) {
            time = 0.0f;
            loop = curTick;
            frame = 0;
        } else {
            CalcTimeWithFlag(pHeader, pHeader->processFlag, frameLength,
                             curTick, life, frame, loop, time);
        }
    }

    pKeys = pKeyTbl->datas;

    u16 prevFrame;
    u16 nextFrame;
    found = SearchKey(pKeyTbl, frame, time, idx, prevFrame, nextFrame);

    if (found) {
        pKey = &pKeys[idx];
    } else {
        loop0 = loop;
        loop1 = loop;
        AdjustTurnLoop(reinterpret_cast<u8*>(pHeader),
                       reinterpret_cast<AnimCurveKey*>(pKeyTbl), idx, loop,
                       loop0, loop1);

        t = (time - prevFrame) / static_cast<int>(nextFrame - prevFrame);

        pKey0 = &pKeys[idx];
        AnimCurveKeyF32<2>* pKey1 = &pKeys[idx + 1];

        bool random0 = pKey0->random != 0;
        random1 = pKey1->random != 0;
        u16 interp = pKey0->interp;

        if (!random0 && !random1) {
            if (pBase != NULL) {
                reinterpret_cast<u8*>(pBase)[0x11] = 0;
            }

            value = InterpolateConstKeyF32(interp, t, pKey0, pKey1, 0);
            pTarget[0] = value;
            if (pBase != NULL) {
                pBase[0] = value;
            }

            value = InterpolateConstKeyF32(interp >> 2, t, pKey0, pKey1, 1);
            pTarget[1] = value;
            if (pBase != NULL) {
                pBase[1] = value;
            }

        } else if (random0 && !random1) {
            u32 rnd;
            AnimCurveRangeF32<2>* pRange = GetRandomRangeW(pHeader, pRangeTbl, pRandomTbl,
                                         pKey0, loop0, seed, rnd);

            value = pRange->comp[0].base;
            value = value + ScaleRnd(pRange->comp[0].width, rnd);
            rnd = MixRandom(rnd);
            pTarget[0] = InterpolateF32(interp, t, value, pKey1->value[0]);
            value = NextRangeValueF32N(pRange, 1, rnd);
            pTarget[1] = InterpolateF32(interp >> 2, t, value, pKey1->value[1]);

        } else if (!random0 && random1) {
            u32 rnd;
            AnimCurveRangeF32<2>* pRange = GetRandomRangeW(pHeader, pRangeTbl, pRandomTbl,
                                         pKey1, loop1, seed, rnd);

            value = NextRangeValueF32N(pRange, 0, rnd);
            pTarget[0] = InterpolateF32(interp, t, pKey0->value[0], value);
            value = pRange->comp[1].base;
            value = value + ScaleRnd(pRange->comp[1].width, rnd);
            rnd = MixRandom(rnd);
            pTarget[1] = InterpolateF32(interp >> 2, t, pKey0->value[1], value);

        } else {
            u32 rnd0;
            AnimCurveRangeF32<2>* pRange0 = GetRandomRangeW(pHeader, pRangeTbl, pRandomTbl,
                                          pKey0, loop0, seed, rnd0);
            u32 rnd1;
            AnimCurveRangeF32<2>* pRange1 = GetRandomRangeW(pHeader, pRangeTbl, pRandomTbl,
                                          pKey1, loop1, seed, rnd1);

            f32 value0;

            value0 = pRange0->comp[0].base;
            value0 = value0 + ScaleRnd(pRange0->comp[0].width, rnd0);
            rnd0 = MixRandom(rnd0);
            // The second range is read through a pointer to its components
            f32* pComp1 = &pRange1->comp[0].base;
            value1 = *pComp1++;
            value1 = value1 + *pComp1++ * (rnd1 >> 16);
            rnd1 = MixRandom(rnd1);
            pTarget[0] = InterpolateF32(interp, t, value0, value1);
            value0 = pRange0->comp[1].base;
            value0 = value0 + ScaleRnd(pRange0->comp[1].width, rnd0);
            value1 = *pComp1++;
            value1 = value1 + *pComp1++ * (rnd1 >> 16);
            pTarget[1] = InterpolateF32(interp >> 2, t, value0, value1);

        }
        return;
    }

    if (pKey->random == 0) {
        pTarget[0] = pKey->value[0];
        pTarget[1] = pKey->value[1];

    } else {
        u32 rnd;
        AnimCurveRangeF32<2>* pRange = GetRandomRangeW(pHeader, pRangeTbl, pRandomTbl,
                                     pKey, loop, seed, rnd);


        value = pRange->comp[0].base;
        pTarget[0] = value + ScaleRnd(pRange->comp[0].width, rnd);
        rnd = MixRandom(rnd);
        pTarget[1] = NextRangeValueF32N(pRange, 1, rnd);

    }
}



// GetRandomRangeW writing the range through a reference
template <typename TRange, typename TKey>
static inline void GetRandomRangeR(AnimCurveHeader* pHeader,
                                   AnimCurveTable<TRange>* pRangeTbl,
                                   AnimCurveTable<TRange>* pRandomTbl,
                                   TKey* pKey, u32 loop, u16 seed,
                                   TRange*& rRange, u32& rRnd) {
    union {
        u32 value;
        u8 bytes[4];
    } rndSeed;

    u32 rangeIdx = pKey->rangeIdx;
    rndSeed.value = CalcRandomSeed(seed, pHeader->randomSeed, rangeIdx, loop);

    rndSeed.bytes[2] ^= rndSeed.bytes[3];
    rndSeed.bytes[1] ^= rndSeed.bytes[2];
    rndSeed.bytes[0] ^= rndSeed.bytes[1];

    rRnd = rndSeed.value;

    if (!(pKey->random & KEY_RANDOM_FLAG_TABLE)) {
        rRange = &pRangeTbl->datas[pKey->rangeIdx];
    } else {
        rRange = &pRandomTbl->datas[(rRnd >> 16) % pRandomTbl->count];
        rRnd = MixRandom(rRnd);
    }
}

void AnimCurveExecuteF32Vec3(u8* pCmdList, f32* pTarget, u32 tick, u16 seed,
                            u32 life) {
    int idx;
    AnimCurveHeader* pHeader = reinterpret_cast<AnimCurveHeader*>(pCmdList);

    AnimCurveTable<AnimCurveKeyF32<3> >* pKeyTbl =
        reinterpret_cast<AnimCurveTable<AnimCurveKeyF32<3> >*>(pCmdList +
                                                sizeof(AnimCurveHeader));
    AnimCurveTable<AnimCurveRangeF32<3> >* pRangeTbl =
        reinterpret_cast<AnimCurveTable<AnimCurveRangeF32<3> >*>(
            reinterpret_cast<u8*>(pKeyTbl) + pHeader->keyTable);
    AnimCurveTable<AnimCurveRangeF32<3> >* pRandomTbl =
        reinterpret_cast<AnimCurveTable<AnimCurveRangeF32<3> >*>(
            reinterpret_cast<u8*>(pRangeTbl) + pHeader->rangeTable);

    u16 frame;
    u32 loop;
    f32 time;
    {
        u16 frameLength = pHeader->frameLength;
        loop = 0;

        if (frameLength <= 1) {
            time = 0.0f;
            loop = tick;
            frame = 0;
        } else {

            if (!(pHeader->processFlag & AnimCurveHeader::PROC_FLAG_INFLOOP) &&
                pHeader->loopCount <= 1) {

                if (!(pHeader->processFlag & AnimCurveHeader::PROC_FLAG_FITTING)) {
                    frame = tick;
                    if (tick >= frameLength - 1) {
                        frame = frameLength - 1;
                    }
                    time = frame;
                } else {
                    time = tick * (static_cast<f32>(frameLength - 1) / (life - 1));

                    if (time > static_cast<f32>(frameLength - 1)) {
                        time = static_cast<f32>(frameLength - 1);
                    }

                    frame = time;
                }
            } else if (!(pHeader->processFlag & AnimCurveHeader::PROC_FLAG_FITTING)) {
                loop = tick / (frameLength - 1);

                if (!(pHeader->processFlag & AnimCurveHeader::PROC_FLAG_TURN)) {
                    if (!(pHeader->processFlag & AnimCurveHeader::PROC_FLAG_INFLOOP) &&
                        loop >= pHeader->loopCount) {
                        frame = frameLength - 1;
                        loop = static_cast<u8>(pHeader->loopCount - 1);
                    } else {
                        frame = tick - loop * (frameLength - 1);
                    }
                } else {
                    if (!(pHeader->processFlag & AnimCurveHeader::PROC_FLAG_INFLOOP) &&
                        loop >= pHeader->loopCount) {
                        if (pHeader->loopCount % 2 == 0) {
                            frame = 0;
                        } else {
                            frame = frameLength - 1;
                        }
                        loop = static_cast<u8>(pHeader->loopCount - 1);
                    } else if (loop % 2 == 0) {
                        frame = tick - loop * (frameLength - 1);
                    } else {
                        frame = (frameLength - 1) * (loop + 1) - tick;
                    }
                }

                time = frame;
            } else if (tick >= life - 1) {
                if (!(pHeader->processFlag & AnimCurveHeader::PROC_FLAG_TURN) ||
                    pHeader->loopCount % 2 != 0) {
                    frame = static_cast<u8>(frameLength - 1);
                } else {
                    frame = 0;
                }

                time = frame;
                loop = static_cast<u8>(pHeader->loopCount - 1);
            } else {
                u32 period = pHeader->frameLength - 1;
                time = CalcFittingLoopTimeP(pHeader, pHeader->processFlag, tick, life, period, loop);
                frame = time;
            }
        }
    }

    AnimCurveKeyF32<3>* pKeys = pKeyTbl->datas;
    u16 prevFrame;
    u16 nextFrame;
    bool found = SearchKey(pKeyTbl, frame, time, idx, prevFrame, nextFrame);

    AnimCurveKeyF32<3>* pKey;

    if (found) {
        pKey = &pKeys[idx];
    } else {
        u32 loop0 = loop;
        u32 loop1 = loop;
        AdjustTurnLoop(reinterpret_cast<u8*>(pHeader),
                       reinterpret_cast<AnimCurveKey*>(pKeyTbl), idx, loop,
                       loop0, loop1);

        f32 t = (time - prevFrame) / static_cast<int>(nextFrame - prevFrame);

        AnimCurveKeyF32<3>* pKey0 = &pKeys[idx];
        AnimCurveKeyF32<3>* pKey1 = &pKeys[idx + 1];

        bool random0 = pKey0->random != 0;
        bool random1 = pKey1->random != 0;
        u16 interp = pKey0->interp;

        f32 value;

        if (!random0 && !random1) {
            value = InterpolateConstKeyF32(interp, t, pKey0, pKey1, 0);
            pTarget[0] = value;

            interp >>= 2;

            value = InterpolateConstKeyF32(interp, t, pKey0, pKey1, 1);
            pTarget[1] = value;

            interp >>= 2;

            value = InterpolateConstKeyF32(interp, t, pKey0, pKey1, 2);
            pTarget[2] = value;

        } else if (random0 && !random1) {
            u32 rnd;
            AnimCurveRangeF32<3>* pRange;
            GetRandomRangeR(pHeader, pRangeTbl, pRandomTbl, pKey0, loop0, seed, pRange, rnd);
            f32* pComp;

            pComp = &pRange->comp[0].base;
            value = *pComp++;
            value = value + *pComp++ * (rnd >> 16);
            rnd = MixRandom(rnd);
            pTarget[0] = InterpolateF32(interp, t, value, pKey1->value[0]);

            interp >>= 2;

            pComp = &pRange->comp[1].base;
            value = *pComp++;
            value = value + *pComp++ * (rnd >> 16);
            rnd = MixRandom(rnd);
            pTarget[1] = InterpolateF32(interp, t, value, pKey1->value[1]);

            interp >>= 2;

            pComp = &pRange->comp[2].base;
            value = *pComp++;
            value = value + *pComp++ * (rnd >> 16);
            pTarget[2] = InterpolateF32(interp, t, value, pKey1->value[2]);

        } else if (!random0 && random1) {
            u32 rnd;
            AnimCurveRangeF32<3>* pRange;
            GetRandomRangeR(pHeader, pRangeTbl, pRandomTbl, pKey1, loop1, seed, pRange, rnd);

            value = pRange->comp[0].base;
            value = value + ScaleRnd(pRange->comp[0].width, rnd);
            rnd = MixRandom(rnd);
            pTarget[0] = InterpolateF32(interp, t, pKey0->value[0], value);

            interp >>= 2;

            value = pRange->comp[1].base;
            value = value + ScaleRnd(pRange->comp[1].width, rnd);
            rnd = MixRandom(rnd);
            pTarget[1] = InterpolateF32(interp, t, pKey0->value[1], value);

            interp >>= 2;

            value = pRange->comp[2].base;
            value = value + ScaleRnd(pRange->comp[2].width, rnd);
            pTarget[2] = InterpolateF32(interp, t, pKey0->value[2], value);

        } else {
            u32 rnd0;
            AnimCurveRangeF32<3>* pRange0 = GetRandomRangeW(pHeader, pRangeTbl, pRandomTbl, pKey0, loop0, seed, rnd0);
            u32 rnd1;
            AnimCurveRangeF32<3>* pRange1;
            GetRandomRangeR(pHeader, pRangeTbl, pRandomTbl, pKey1, loop1, seed, pRange1, rnd1);

            f32 value0;
            f32 value1;
            // The second range is read through a pointer to its components
            f32* pComp1;

            value0 = pRange0->comp[0].base;
            value0 = value0 + ScaleRnd(pRange0->comp[0].width, rnd0);
            rnd0 = MixRandom(rnd0);
            pComp1 = &pRange1->comp[0].base;
            value1 = *pComp1++;
            value1 = value1 + *pComp1++ * (rnd1 >> 16);
            rnd1 = MixRandom(rnd1);
            pTarget[0] = InterpolateF32(interp, t, value0, value1);

            interp >>= 2;

            value0 = pRange0->comp[1].base;
            value0 = value0 + ScaleRnd(pRange0->comp[1].width, rnd0);
            rnd0 = MixRandom(rnd0);
            pComp1 = &pRange1->comp[1].base;
            value1 = *pComp1++;
            value1 = value1 + *pComp1++ * (rnd1 >> 16);
            rnd1 = MixRandom(rnd1);
            pTarget[1] = InterpolateF32(interp, t, value0, value1);

            interp >>= 2;

            value0 = pRange0->comp[2].base;
            value0 = value0 + ScaleRnd(pRange0->comp[2].width, rnd0);
            pComp1 = &pRange1->comp[2].base;
            value1 = *pComp1++;
            value1 = value1 + *pComp1++ * (rnd1 >> 16);
            pTarget[2] = InterpolateF32(interp, t, value0, value1);

        }
        return;
    }

    if (pKey->random == 0) {
        pTarget[0] = pKey->value[0];
        pTarget[1] = pKey->value[1];
        pTarget[2] = pKey->value[2];

    } else {
        u32 rnd;
        AnimCurveRangeF32<3>* pRange;
        GetRandomRangeR(pHeader, pRangeTbl, pRandomTbl, pKey, loop, seed, pRange, rnd);

        f32 value;
        value = pRange->comp[0].base;
        value = value + ScaleRnd(pRange->comp[0].width, rnd);
        pTarget[0] = value;
        rnd = MixRandom(rnd);
        f32* pComp = &pRange->comp[1].base;
        value = *pComp++;
        value = value + *pComp++ * (rnd >> 16);
        pTarget[1] = value;
        rnd = MixRandom(rnd);
        value = pRange->comp[2].base;
        value = value + ScaleRnd(pRange->comp[2].width, rnd);
        // @bug The third component is written over the second one
        pTarget[1] = value;

    }
}

void AnimCurveExecuteF32N(u8* pCmdList, f32* pTarget, u32 tick, u16 seed,
                          u32 life, u8 enable) {
    AnimCurveHeader* pHeader = reinterpret_cast<AnimCurveHeader*>(pCmdList);
    AnimCurveTable<u8>* pKeyTbl =
        reinterpret_cast<AnimCurveTable<u8>*>(pHeader + 1);
    AnimCurveTable<u8>* pRangeTbl = reinterpret_cast<AnimCurveTable<u8>*>(
        reinterpret_cast<u8*>(pKeyTbl) + pHeader->keyTable);
    AnimCurveTable<u8>* pRandomTbl = reinterpret_cast<AnimCurveTable<u8>*>(
        reinterpret_cast<u8*>(pRangeTbl) + pHeader->rangeTable);
    // The time calculation works on its own copy of tick
    const u32 curTick = tick;
    bool random0;
    u32 loop;
    int num;
    AnimCurveRangeF32<1>* pRange1;
    u32 loop1;
    u32 loop0;
    bool random1;
    f32 t;
    AnimCurveRangeF32<1>* pRange0;
    f32* pValue0;
    f32* pValue1;
    u16 interp;
    u8* pKeys;
    int keySize;
    u16 frameLength;
    u32 rnd1;
    AnimCurveKeyF32<1>* pKey0;
    int idx;
    u16 rangeIdx0;
    u16 keyRangeIdx1;
    u16 keyRangeIdx0;
    u16 prevFrame;
    f32 value0;
    u16 nextFrame;
    f32 time;
    f32* pValue;
    AnimCurveKeyF32<1>* pKey;

    u16 frame;
    {
        frameLength = pHeader->frameLength;
        loop = 0;

        if (frameLength <= 1) {
            time = 0.0f;
            loop = curTick;
            frame = 0;
        } else {
            CalcTimeWithFlag(pHeader, pHeader->processFlag, frameLength,
                             curTick, life, frame, loop, time);
        }
    }

    // The highest enabled bit gives the number of components
    num = 8;
    u8 bit;
    for (bit = 0x80; bit != 0; bit >>= 1, num--) {
        if (enable & bit) {
            break;
        }
    }

    if (num == 0) {
        return;
    }

    keySize = num * sizeof(f32) + 8;
    int rangeSize = num * sizeof(AnimCurveRangeF32<1>);
    pKeys = pKeyTbl->datas;
    bool found = SearchKeyStride(pKeyTbl, keySize, frame, time, idx,
                                 prevFrame, nextFrame);


    if (found) {
        pValue = reinterpret_cast<AnimCurveKeyF32<1>*>(pKeys + idx * keySize)
                     ->value;
    } else {
        loop0 = loop;
        loop1 = loop;
        AdjustTurnLoop(reinterpret_cast<u8*>(pHeader),
                       reinterpret_cast<AnimCurveKey*>(pKeyTbl), idx, loop,
                       loop0, loop1);

        t = (time - prevFrame) / static_cast<int>(nextFrame - prevFrame);

        pKey0 =
            reinterpret_cast<AnimCurveKeyF32<1>*>(pKeys + idx * keySize);
        pValue0 = pKey0->value;
        pValue1 =
            reinterpret_cast<AnimCurveKeyF32<1>*>(pKeys + (idx + 1) * keySize)
                ->value;
        AnimCurveKeyF32<1>* pKey1 = reinterpret_cast<AnimCurveKeyF32<1>*>(
            reinterpret_cast<u8*>(pValue1) - 8);

        random0 = pKey0->random != 0;
        random1 = pKey1->random != 0;
        interp = pKey0->interp;

        if (!random0 && !random1) {
            for (; bit != 0; interp >>= 2, bit >>= 1, pTarget++) {
                if (enable & bit) {
                    *pTarget =
                        InterpolateConstPtrF32(interp, t, pValue0, pValue1);
                    pValue0++;
                    pValue1++;
                }
            }
        } else if (random0 && !random1) {
            u32 rnd;
            keyRangeIdx0 = *reinterpret_cast<u16*>(pValue0);
            AnimCurveRangeF32<1>* pRange = GetRandomRangeStrideIdx(
                pHeader, pRangeTbl, pRandomTbl, rangeSize, pKey0->random,
                keyRangeIdx0, loop0, seed, rnd);

            f32* pComp = &pRange->comp[0].base;

            for (; bit != 0; interp >>= 2, bit >>= 1, pTarget++) {
                if (enable & bit) {
                    f32 value = *pComp++;
                    value = value + *pComp++ * (rnd >> 16);
                    rnd = MixRandom(rnd);
                    *pTarget = InterpolateF32(interp, t, value, *pValue1);
                    pValue1++;
                }
            }
        } else if (!random0 && random1) {
            u32 rnd;
            keyRangeIdx1 = *reinterpret_cast<u16*>(pValue1);
            AnimCurveRangeF32<1>* pRange = GetRandomRangeStrideIdx(
                pHeader, pRangeTbl, pRandomTbl, rangeSize, pKey1->random,
                keyRangeIdx1, loop1, seed, rnd);

            f32* pComp = &pRange->comp[0].base;

            for (; bit != 0; interp >>= 2, bit >>= 1, pTarget++) {
                if (enable & bit) {
                    f32 value = *pComp++;
                    value = value + *pComp++ * (rnd >> 16);
                    rnd = MixRandom(rnd);
                    *pTarget = InterpolateF32(interp, t, *pValue0, value);
                    pValue0++;
                }
            }
        } else {
            u32 rnd0;
            rangeIdx0 = *reinterpret_cast<u16*>(pValue0);
            pRange0 = GetRandomRangeStrideIdx(
                pHeader, pRangeTbl, pRandomTbl, rangeSize, pKey0->random,
                rangeIdx0, loop0, seed, rnd0);
            pRange1 = GetRandomRangeStride(
                pHeader, pRangeTbl, pRandomTbl, rangeSize, pKey1->random,
                reinterpret_cast<u16*>(pValue1), loop1, seed, rnd1);

            f32* pComp1 = &pRange1->comp[0].base;

            for (; bit != 0; interp >>= 2, bit >>= 1, pTarget++) {
                if (enable & bit) {
                    value0 = pRange0->comp[0].base;
                    value0 = value0 + ScaleRnd(pRange0->comp[0].width, rnd0);
                    pRange0++;
                    rnd0 = MixRandom(rnd0);
                    f32 value1 = *pComp1++;
                    value1 = value1 + *pComp1++ * (rnd1 >> 16);
                    rnd1 = MixRandom(rnd1);
                    *pTarget = InterpolateF32(interp, t, value0, value1);
                }
            }
        }
        return;
    }

    pKey = reinterpret_cast<AnimCurveKeyF32<1>*>(
        reinterpret_cast<u8*>(pValue) - 8);

    if (pKey->random == 0) {
        for (; bit != 0; bit >>= 1, pValue++, pTarget++) {
            if (enable & bit) {
                *pTarget = *pValue;
            }
        }
    } else {
        u32 rnd;
        AnimCurveRangeF32<1>* pRange =
            GetRandomRangeStride(pHeader, pRangeTbl, pRandomTbl, rangeSize,
                                 pKey->random, reinterpret_cast<u16*>(pValue),
                                 loop, seed, rnd);

        f32* pComp = &pRange->comp[0].base;

        for (; bit != 0; bit >>= 1, pTarget++) {
            if (enable & bit) {
                f32 value = *pComp++;
                *pTarget = value + *pComp++ * (rnd >> 16);
                rnd = MixRandom(rnd);
            }
        }
    }
}



void AnimCurveExecuteRotate(u8* pCmdList, f32* pTarget, u32 tick, u16 seed,
                            u32 life) {
    int idx;
    AnimCurveHeader* pHeader = reinterpret_cast<AnimCurveHeader*>(pCmdList);

    AnimCurveTable<AnimCurveKeyF32<3> >* pKeyTbl =
        reinterpret_cast<AnimCurveTable<AnimCurveKeyF32<3> >*>(pCmdList +
                                                sizeof(AnimCurveHeader));
    AnimCurveTable<AnimCurveRangeRotate >* pRangeTbl =
        reinterpret_cast<AnimCurveTable<AnimCurveRangeRotate >*>(
            reinterpret_cast<u8*>(pKeyTbl) + pHeader->keyTable);
    AnimCurveTable<AnimCurveRangeRotate >* pRandomTbl =
        reinterpret_cast<AnimCurveTable<AnimCurveRangeRotate >*>(
            reinterpret_cast<u8*>(pRangeTbl) + pHeader->rangeTable);

    u16 frame;
    u32 loop;
    f32 time;
    {
        u16 frameLength = pHeader->frameLength;
        loop = 0;

        if (frameLength <= 1) {
            time = 0.0f;
            loop = tick;
            frame = 0;
        } else {

            if (!(pHeader->processFlag & AnimCurveHeader::PROC_FLAG_INFLOOP) &&
                pHeader->loopCount <= 1) {

                if (!(pHeader->processFlag & AnimCurveHeader::PROC_FLAG_FITTING)) {
                    frame = tick;
                    if (tick >= frameLength - 1) {
                        frame = frameLength - 1;
                    }
                    time = frame;
                } else {
                    time = tick * (static_cast<f32>(frameLength - 1) / (life - 1));

                    if (time > static_cast<f32>(frameLength - 1)) {
                        time = static_cast<f32>(frameLength - 1);
                    }

                    frame = time;
                }
            } else if (!(pHeader->processFlag & AnimCurveHeader::PROC_FLAG_FITTING)) {
                loop = tick / (frameLength - 1);

                if (!(pHeader->processFlag & AnimCurveHeader::PROC_FLAG_TURN)) {
                    if (!(pHeader->processFlag & AnimCurveHeader::PROC_FLAG_INFLOOP) &&
                        loop >= pHeader->loopCount) {
                        frame = frameLength - 1;
                        loop = static_cast<u8>(pHeader->loopCount - 1);
                    } else {
                        frame = tick - loop * (frameLength - 1);
                    }
                } else {
                    if (!(pHeader->processFlag & AnimCurveHeader::PROC_FLAG_INFLOOP) &&
                        loop >= pHeader->loopCount) {
                        if (pHeader->loopCount % 2 == 0) {
                            frame = 0;
                        } else {
                            frame = frameLength - 1;
                        }
                        loop = static_cast<u8>(pHeader->loopCount - 1);
                    } else if (loop % 2 == 0) {
                        frame = tick - loop * (frameLength - 1);
                    } else {
                        frame = (frameLength - 1) * (loop + 1) - tick;
                    }
                }

                time = frame;
            } else if (tick >= life - 1) {
                if (!(pHeader->processFlag & AnimCurveHeader::PROC_FLAG_TURN) ||
                    pHeader->loopCount % 2 != 0) {
                    frame = static_cast<u8>(frameLength - 1);
                } else {
                    frame = 0;
                }

                time = frame;
                loop = static_cast<u8>(pHeader->loopCount - 1);
            } else {
                u32 period = pHeader->frameLength - 1;
                time = CalcFittingLoopTimeP(pHeader, pHeader->processFlag, tick, life, period, loop);
                frame = time;
            }
        }
    }

    AnimCurveKeyF32<3>* pKeys = pKeyTbl->datas;
    u16 prevFrame;
    u16 nextFrame;
    bool found = SearchKey(pKeyTbl, frame, time, idx, prevFrame, nextFrame);

    AnimCurveKeyF32<3>* pKey;

    if (found) {
        pKey = &pKeys[idx];
    } else {
        u32 loop0 = loop;
        u32 loop1 = loop;
        AdjustTurnLoop(reinterpret_cast<u8*>(pHeader),
                       reinterpret_cast<AnimCurveKey*>(pKeyTbl), idx, loop,
                       loop0, loop1);

        f32 t = (time - prevFrame) / static_cast<int>(nextFrame - prevFrame);

        AnimCurveKeyF32<3>* pKey0 = &pKeys[idx];
        AnimCurveKeyF32<3>* pKey1 = &pKeys[idx + 1];

        bool random0 = pKey0->random != 0;
        bool random1 = pKey1->random != 0;
        u16 interp = pKey0->interp;

        f32 value;

        if (!random0 && !random1) {
            value = InterpolateConstKeyF32(interp, t, pKey0, pKey1, 0);
            pTarget[0] = value;

            interp >>= 2;

            value = InterpolateConstKeyF32(interp, t, pKey0, pKey1, 1);
            pTarget[1] = value;

            interp >>= 2;

            value = InterpolateConstKeyF32(interp, t, pKey0, pKey1, 2);
            pTarget[2] = value;

        } else if (random0 && !random1) {
            u32 rnd;
            AnimCurveRangeRotate* pRange;
            GetRandomRangeR(pHeader, pRangeTbl, pRandomTbl, pKey0, loop0, seed, pRange, rnd);
            bool negate = false;
            if (pRange->randomSign) {
                if (!(rnd & 0x10000)) {
                    negate = true;
                }
                rnd = MixRandom(rnd);
            }

            value = pRange->comp[0].base;
            value = value + ScaleRnd(pRange->comp[0].width, rnd);
            if (negate) {
                value = -value;
            }
            rnd = MixRandom(rnd);
            pTarget[0] = InterpolateF32(interp, t, value, pKey1->value[0]);

            interp >>= 2;

            value = pRange->comp[1].base;
            value = value + ScaleRnd(pRange->comp[1].width, rnd);
            if (negate) {
                value = -value;
            }
            rnd = MixRandom(rnd);
            pTarget[1] = InterpolateF32(interp, t, value, pKey1->value[1]);

            interp >>= 2;

            value = pRange->comp[2].base;
            value = value + ScaleRnd(pRange->comp[2].width, rnd);
            if (negate) {
                value = -value;
            }
            pTarget[2] = InterpolateF32(interp, t, value, pKey1->value[2]);

        } else if (!random0 && random1) {
            u32 rnd;
            AnimCurveRangeRotate* pRange;
            GetRandomRangeR(pHeader, pRangeTbl, pRandomTbl, pKey1, loop1, seed, pRange, rnd);
            bool negate = false;
            if (pRange->randomSign) {
                if (!(rnd & 0x10000)) {
                    negate = true;
                }
                rnd = MixRandom(rnd);
            }

            value = pRange->comp[0].base;
            value = value + ScaleRnd(pRange->comp[0].width, rnd);
            if (negate) {
                value = -value;
            }
            pTarget[0] = InterpolateF32(interp, t, pKey0->value[0], value);
            rnd = MixRandom(rnd);

            interp >>= 2;

            value = pRange->comp[1].base;
            value = value + ScaleRnd(pRange->comp[1].width, rnd);
            if (negate) {
                value = -value;
            }
            pTarget[1] = InterpolateF32(interp, t, pKey0->value[1], value);
            rnd = MixRandom(rnd);

            interp >>= 2;

            value = pRange->comp[2].base;
            value = value + ScaleRnd(pRange->comp[2].width, rnd);
            if (negate) {
                value = -value;
            }
            pTarget[2] = InterpolateF32(interp, t, pKey0->value[2], value);

        } else {
            u32 rnd0;
            AnimCurveRangeRotate* pRange0 = GetRandomRangeW(pHeader, pRangeTbl, pRandomTbl, pKey0, loop0, seed, rnd0);
            u32 rnd1;
            AnimCurveRangeRotate* pRange1;
            GetRandomRangeR(pHeader, pRangeTbl, pRandomTbl, pKey1, loop1, seed, pRange1, rnd1);
            bool negate0 = false;
            if (pRange0->randomSign) {
                if (!(rnd0 & 0x10000)) {
                    negate0 = true;
                }
                rnd0 = MixRandom(rnd0);
            }
            bool negate1 = false;
            if (pRange1->randomSign) {
                if (!(rnd1 & 0x10000)) {
                    negate1 = true;
                }
                rnd1 = MixRandom(rnd1);
            }

            f32 value0;
            f32 value1;

            value0 = pRange0->comp[0].base;
            value0 = value0 + ScaleRnd(pRange0->comp[0].width, rnd0);
            if (negate0) {
                value0 = -value0;
            }
            value1 = pRange1->comp[0].base;
            value1 = value1 + ScaleRnd(pRange1->comp[0].width, rnd1);
            if (negate1) {
                value1 = -value1;
            }
            pTarget[0] = InterpolateF32(interp, t, value0, value1);
            rnd0 = MixRandom(rnd0);
            rnd1 = MixRandom(rnd1);

            interp >>= 2;

            value0 = pRange0->comp[1].base;
            value0 = value0 + ScaleRnd(pRange0->comp[1].width, rnd0);
            if (negate0) {
                value0 = -value0;
            }
            value1 = pRange1->comp[1].base;
            value1 = value1 + ScaleRnd(pRange1->comp[1].width, rnd1);
            if (negate1) {
                value1 = -value1;
            }
            pTarget[1] = InterpolateF32(interp, t, value0, value1);
            rnd0 = MixRandom(rnd0);
            rnd1 = MixRandom(rnd1);

            interp >>= 2;

            value0 = pRange0->comp[2].base;
            value0 = value0 + ScaleRnd(pRange0->comp[2].width, rnd0);
            if (negate0) {
                value0 = -value0;
            }
            value1 = pRange1->comp[2].base;
            value1 = value1 + ScaleRnd(pRange1->comp[2].width, rnd1);
            if (negate1) {
                value1 = -value1;
            }
            pTarget[2] = InterpolateF32(interp, t, value0, value1);

        }
        return;
    }

    if (pKey->random == 0) {
        pTarget[0] = pKey->value[0];
        pTarget[1] = pKey->value[1];
        pTarget[2] = pKey->value[2];

    } else {
        u32 rnd;
        AnimCurveRangeRotate* pRange;
        GetRandomRangeR(pHeader, pRangeTbl, pRandomTbl, pKey, loop, seed, pRange, rnd);
        bool negate = false;
        if (pRange->randomSign) {
            if (!(rnd & 0x10000)) {
                negate = true;
            }
            rnd = MixRandom(rnd);
        }

        f32 value;
        value = pRange->comp[0].base;
        value = value + ScaleRnd(pRange->comp[0].width, rnd);
        if (negate) {
            value = -value;
        }
        pTarget[0] = value;
        rnd = MixRandom(rnd);
        value = pRange->comp[1].base;
        value = value + ScaleRnd(pRange->comp[1].width, rnd);
        if (negate) {
            value = -value;
        }
        pTarget[1] = value;
        rnd = MixRandom(rnd);
        value = pRange->comp[2].base;
        value = value + ScaleRnd(pRange->comp[2].width, rnd);
        if (negate) {
            value = -value;
        }
        pTarget[2] = value;

    }
}

void AnimCurveExecuteF32(u8* pCmdList, f32* pBase, f32* pTarget, u32 tick,
                         u16 seed, u32 life) {
    AnimCurveHeader* pHeader = reinterpret_cast<AnimCurveHeader*>(pCmdList);
    AnimCurveTable<AnimCurveKeyF32<1> >* pKeyTbl;
    AnimCurveTable<AnimCurveRangeF32<1> >* pRangeTbl;
    AnimCurveTable<AnimCurveRangeF32<1> >* pRandomTbl;
    // The time calculation works on its own copy of tick
    const u32 curTick = tick;
    f32 t;
    u16 frameLength;
    u32 loop;
    bool found;
    f32 value;
    AnimCurveKeyF32<1>* pKey0;
    f32 value1;
    u32 loop0;
    f32 time;
    bool random1;
    AnimCurveKeyF32<1>* pKey;
    u32 loop1;
    AnimCurveKeyF32<1>* pKeys;
    int idx;

    pKeyTbl =
        reinterpret_cast<AnimCurveTable<AnimCurveKeyF32<1> >*>(pHeader + 1);
    pRangeTbl =
        reinterpret_cast<AnimCurveTable<AnimCurveRangeF32<1> >*>(
            reinterpret_cast<u8*>(pKeyTbl) + pHeader->keyTable);
    pRandomTbl =
        reinterpret_cast<AnimCurveTable<AnimCurveRangeF32<1> >*>(
            reinterpret_cast<u8*>(pRangeTbl) + pHeader->rangeTable);

    u16 frame;
    {
        frameLength = pHeader->frameLength;
        loop = 0;

        if (frameLength <= 1) {
            time = 0.0f;
            loop = curTick;
            frame = 0;
        } else {
            CalcTimeWithFlag(pHeader, pHeader->processFlag, frameLength,
                             curTick, life, frame, loop, time);
        }
    }

    pKeys = pKeyTbl->datas;
    if (pKeyTbl->count == 0) {
        return;
    }

    u16 prevFrame;
    u16 nextFrame;
    found = SearchKey(pKeyTbl, frame, time, idx, prevFrame, nextFrame);

    if (found) {
        pKey = &pKeys[idx];
    } else {
        loop0 = loop;
        loop1 = loop;
        AdjustTurnLoop(reinterpret_cast<u8*>(pHeader),
                       reinterpret_cast<AnimCurveKey*>(pKeyTbl), idx, loop,
                       loop0, loop1);

        t = (time - prevFrame) / static_cast<int>(nextFrame - prevFrame);

        pKey0 = &pKeys[idx];
        AnimCurveKeyF32<1>* pKey1 = &pKeys[idx + 1];

        bool random0 = pKey0->random != 0;
        random1 = pKey1->random != 0;
        u16 interp = pKey0->interp;

        if (!random0 && !random1) {
            if (pBase != NULL) {
                reinterpret_cast<u8*>(pBase)[0x11] = 0;
            }

            value = InterpolateConstKeyF32(interp, t, pKey0, pKey1, 0);
            pTarget[0] = value;
            if (pBase != NULL) {
                pBase[0] = value;
            }

        } else if (random0 && !random1) {
            u32 rnd;
            AnimCurveRangeF32<1>* pRange = GetRandomRangeW(pHeader, pRangeTbl, pRandomTbl,
                                         pKey0, loop0, seed, rnd);

            value = pRange->comp[0].base;
            value = value + ScaleRnd(pRange->comp[0].width, rnd);
            pTarget[0] = InterpolateF32(interp, t, value, pKey1->value[0]);

        } else if (!random0 && random1) {
            u32 rnd;
            AnimCurveRangeF32<1>* pRange = GetRandomRangeW(pHeader, pRangeTbl, pRandomTbl,
                                         pKey1, loop1, seed, rnd);

            value = NextRangeValueF32N(pRange, 0, rnd);
            pTarget[0] = InterpolateF32(interp, t, pKey0->value[0], value);

        } else {
            u32 rnd0;
            AnimCurveRangeF32<1>* pRange0 = GetRandomRangeW(pHeader, pRangeTbl, pRandomTbl,
                                          pKey0, loop0, seed, rnd0);
            u32 rnd1;
            AnimCurveRangeF32<1>* pRange1 = GetRandomRangeW(pHeader, pRangeTbl, pRandomTbl,
                                          pKey1, loop1, seed, rnd1);

            f32 value0;

            value0 = pRange0->comp[0].base;
            value0 = value0 + ScaleRnd(pRange0->comp[0].width, rnd0);
            // The second range is read through a pointer to its components
            f32* pComp1 = &pRange1->comp[0].base;
            value1 = *pComp1++;
            value1 = value1 + *pComp1++ * (rnd1 >> 16);
            pTarget[0] = InterpolateF32(interp, t, value0, value1);

        }
        return;
    }

    if (pKey->random == 0) {
        pTarget[0] = pKey->value[0];

    } else {
        u32 rnd;
        AnimCurveRangeF32<1>* pRange = GetRandomRangeW(pHeader, pRangeTbl, pRandomTbl,
                                     pKey, loop, seed, rnd);


        pTarget[0] = NextRangeValueF32N(pRange, 0, rnd);

    }
}



void AnimCurveExecuteTexture(u8* pCmdList, Particle* pParticle, u32 tick,
                             u16 seed, u32 life) {
    AnimCurveHeader* pHeader = reinterpret_cast<AnimCurveHeader*>(pCmdList);
    AnimCurveTable<AnimCurveKeyTexture>* pKeyTbl =
        reinterpret_cast<AnimCurveTable<AnimCurveKeyTexture>*>(pHeader + 1);
    AnimCurveTable<AnimCurveRangeTexture>* pRangeTbl =
        reinterpret_cast<AnimCurveTable<AnimCurveRangeTexture>*>(
            reinterpret_cast<u8*>(pKeyTbl) + pHeader->keyTable);
    AnimCurveTable<AnimCurveRandomTexture>* pRandomTbl =
        reinterpret_cast<AnimCurveTable<AnimCurveRandomTexture>*>(
            reinterpret_cast<u8*>(pRangeTbl) + pHeader->rangeTable);
    AnimCurveTable<TextureData*>* pNameTbl =
        reinterpret_cast<AnimCurveTable<TextureData*>*>(
            reinterpret_cast<u8*>(pRandomTbl) + pHeader->randomTable);

    u32 loop;
    bool end;
    u16 frame;
    CalcFrameStep(reinterpret_cast<u8*>(pHeader), tick, life, frame, loop,
                  end);

    TextureData** ppTexture;
    int layer;

    switch (pHeader->kindType) {
    case AC_TARGET_TEXTURE1: {
        ppTexture = &pParticle->mParameter.mTexture[TEX_LAYER_1];
        layer = TEX_LAYER_1;
        break;
    }
    case AC_TARGET_TEXTURE2: {
        ppTexture = &pParticle->mParameter.mTexture[TEX_LAYER_2];
        layer = TEX_LAYER_2;
        break;
    }
    case AC_TARGET_TEXTUREIND: {
        ppTexture = &pParticle->mParameter.mTexture[TEX_LAYER_IND];
        layer = TEX_LAYER_IND;
        break;
    }
    default: {
        return;
    }
    }

    AnimCurveKeyTexture* pKeys = pKeyTbl->datas;
    AnimCurveKeyTexture* pKey = &pKeys[SearchKeyStep(pKeys, pKeyTbl->count, frame, false)];

    u16 name;
    u8 wrap;
    u8 reverse;

    if (pKey->random == 0) {
        name = pKey->name;
        wrap = pKey->wrap;
        reverse = pKey->reverse;
    } else {
        union {
            u32 value;
            u8 bytes[4];
        } rndSeed;

        u16 rangeIdx = pKey->rangeIdx;
        rndSeed.value = CalcRandomSeed(seed, pHeader->randomSeed, rangeIdx, loop);

        rndSeed.bytes[2] ^= rndSeed.bytes[3];
        rndSeed.bytes[1] ^= rndSeed.bytes[2];
        rndSeed.bytes[0] ^= rndSeed.bytes[1];

        if (!(pKey->random & KEY_RANDOM_FLAG_TABLE)) {
            AnimCurveRangeTexture* pRange = &pRangeTbl->datas[rangeIdx];

            name = pRange->name;
            wrap = pRange->wrap;
            reverse = pRange->reverse;

            switch (pRange->randomReverse) {
            case 1: {
                reverse = (reverse & 2) | ((rndSeed.value >> 16) & 1);
                break;
            }
            case 2: {
                reverse = (reverse & 1) | ((rndSeed.value >> 16) & 2);
                break;
            }
            case 3: {
                reverse = (rndSeed.value >> 16) & 3;
                break;
            }
            }
        } else {
            AnimCurveRandomTexture* pRandom =
                &pRandomTbl->datas[(rndSeed.value >> 16) % pRandomTbl->count];

            name = pRandom->name;
            wrap = pRandom->wrap;
            reverse = pRandom->reverse;
        }
    }

    *ppTexture = pNameTbl->datas[name];

    pParticle->mParameter.mTextureWrap &= ~(0xF << (layer * 4));
    pParticle->mParameter.mTextureWrap |= (wrap & 0xF) << (layer * 4);
    pParticle->mParameter.mTextureReverse &= ~(3 << (layer * 2));
    pParticle->mParameter.mTextureReverse |= (reverse & 3) << (layer * 2);
}

void createChild(u8* pKeyData, u16 seed, AnimCurveHeader* pHeader,
                 AnimCurveNameTable* pNameTbl,
                 AnimCurveRandomTable* pRandomTbl, Particle* pParticle,
                 u32 loop) {
    AnimCurveKeyChild* pKey = reinterpret_cast<AnimCurveKeyChild*>(pKeyData);
    AnimCurveChild* pChild;

    if (pKey->random == 0) {
        pChild = &pKey->child;
    } else {
        union {
            u32 value;
            u8 bytes[4];
        } rndSeed;

        u32 rangeIdx = pKey->rangeIdx;
        rndSeed.value = CalcRandomSeed(seed, pHeader->randomSeed, rangeIdx, loop);

        rndSeed.bytes[2] ^= rndSeed.bytes[3];
        rndSeed.bytes[1] ^= rndSeed.bytes[2];
        rndSeed.bytes[0] ^= rndSeed.bytes[1];

        // @bug pChild is left unset for keys with a random range
        if (pKey->random & KEY_RANDOM_FLAG_TABLE) {
            u16 count = pRandomTbl->count;
            if (count == 0) {
                return;
            }

            pChild = &reinterpret_cast<AnimCurveChild*>(
                pRandomTbl->datas)[(rndSeed.value >> 16) % count];
        }
    }

    EmitterResource* pResource = reinterpret_cast<EmitterResource*>(
        pNameTbl->datas[pChild->resIdx].work);

    if (pResource == NULL) {
        return;
    }

    if (pChild->setting.type == 0) {
        pParticle->mParticleManager->mManagerEM->mManagerEF->mManagerES
            ->mCreationQueue.AddParticleCreation(&pChild->setting, pParticle,
                                                 pResource,
                                                 pParticle->mCalcRemain);
    } else {
        pParticle->mParticleManager->mManagerEM->mManagerEF->mManagerES
            ->mCreationQueue.AddEmitterCreation(&pChild->setting, pParticle,
                                                pResource,
                                                pParticle->mCalcRemain);
    }
}

void AnimCurveExecuteChild(u8* pCmdList, Particle* pParticle, u32 tick,
                           u16 seed, u32 life) {
    AnimCurveHeader* pHeader = reinterpret_cast<AnimCurveHeader*>(pCmdList);
    AnimCurveTable<AnimCurveKeyChild>* pKeyTbl =
        reinterpret_cast<AnimCurveTable<AnimCurveKeyChild>*>(pHeader + 1);
    u8* pRangeTbl = reinterpret_cast<u8*>(pKeyTbl) + pHeader->keyTable;
    AnimCurveRandomTable* pRandomTbl =
        reinterpret_cast<AnimCurveRandomTable*>(pRangeTbl + pHeader->rangeTable);
    AnimCurveNameTable* pNameTbl = reinterpret_cast<AnimCurveNameTable*>(
        reinterpret_cast<u8*>(pRandomTbl) + pHeader->randomTable);

    // Children are created for the keys passed between this tick and the
    // next one
    u16 frame0;
    u32 loop0;
    bool end;
    bool end1;
    u16 frame1;
    u32 loop1;
    int i;
    AnimCurveKeyChild* pKey;
    u32 loop;
    CalcFrameStep(reinterpret_cast<u8*>(pHeader), tick, life, frame0, loop0,
                  end);
    CalcFrameStep(reinterpret_cast<u8*>(pHeader), tick + 1, life, frame1, loop1,
                  end1);

    AnimCurveKeyChild* pKeys = pKeyTbl->datas;

    if (loop0 == loop1) {
        if (frame0 == frame1) {
            if (!end) {
                return;
            }
            frame1 = 1;
        }

        if (frame0 < frame1) {
            i = SearchKeyStep(pKeys, pKeyTbl->count, frame0, true);
            pKey = &pKeys[i];

            for (; i < pKeyTbl->count; pKey++, i++) {
                if (pKey->frame >= frame0) {
                    if (frame1 <= pKey->frame) {
                        return;
                    }

                    createChild(reinterpret_cast<u8*>(pKey), seed, pHeader,
                                pNameTbl, pRandomTbl, pParticle, loop0);
                }
            }
        } else {
            i = SearchKeyStep(pKeys, pKeyTbl->count, frame0, false);
            pKey = &pKeys[i];

            for (; i >= 0; pKey--, i--) {
                if (frame0 >= pKey->frame) {
                    if (pKey->frame <= frame1) {
                        return;
                    }

                    createChild(reinterpret_cast<u8*>(pKey), seed, pHeader,
                                pNameTbl, pRandomTbl, pParticle, loop0);
                }
            }
        }
        return;
    }

    if (pHeader->processFlag & AnimCurveHeader::PROC_FLAG_TURN) {
        // Rest of the current loop
        if (!(loop0 & 1)) {
            i = SearchKeyStep(pKeys, pKeyTbl->count, frame0, true);
            pKey = &pKeys[i];

            for (; i < pKeyTbl->count; pKey++, i++) {
                if (pKey->frame >= frame0) {
                    if (pKey->frame == pHeader->frameLength - 1) {
                        break;
                    }

                    createChild(reinterpret_cast<u8*>(pKey), seed, pHeader,
                                pNameTbl, pRandomTbl, pParticle, loop0);
                }
            }
        } else {
            i = SearchKeyStep(pKeys, pKeyTbl->count, frame0, false);
            pKey = &pKeys[i];

            for (; i >= 0; pKey--, i--) {
                if (frame0 >= pKey->frame) {
                    if (pKey->frame == 0) {
                        break;
                    }

                    createChild(reinterpret_cast<u8*>(pKey), seed, pHeader,
                                pNameTbl, pRandomTbl, pParticle, loop0);
                }
            }
        }

        // Whole loops in between
        for (loop = loop0 + 1; loop < loop1; loop++) {
            if (!(loop0 & 1)) {
                pKey = pKeys;

                for (i = 0; i < pKeyTbl->count; pKey++, i++) {
                    if (pKey->frame == pHeader->frameLength - 1) {
                        break;
                    }

                    createChild(reinterpret_cast<u8*>(pKey), seed, pHeader,
                                pNameTbl, pRandomTbl, pParticle, loop0);
                }
            } else {
                i = pKeyTbl->count - 1;
                pKey = &pKeys[i];

                for (; i >= 0; pKey--, i--) {
                    if (pKey->frame == 0) {
                        break;
                    }

                    createChild(reinterpret_cast<u8*>(pKey), seed, pHeader,
                                pNameTbl, pRandomTbl, pParticle, loop0);
                }
            }
        }

        // Start of the next loop
        if (!(loop1 & 1)) {
            pKey = pKeys;

            for (i = 0; i < pKeyTbl->count; pKey++, i++) {
                if (frame1 <= pKey->frame) {
                    return;
                }

                createChild(reinterpret_cast<u8*>(pKey), seed, pHeader,
                            pNameTbl, pRandomTbl, pParticle, loop0);
            }
        } else {
            i = pKeyTbl->count - 1;
            pKey = &pKeys[i];

            for (; i >= 0; pKey--, i--) {
                if (frame1 >= pKey->frame) {
                    return;
                }

                createChild(reinterpret_cast<u8*>(pKey), seed, pHeader,
                            pNameTbl, pRandomTbl, pParticle, loop0);
            }
        }
    } else {
        i = SearchKeyStep(pKeys, pKeyTbl->count, frame0, true);
        pKey = &pKeys[i];

        for (; i < pKeyTbl->count; pKey++, i++) {
            if (pKey->frame >= frame0) {
                createChild(reinterpret_cast<u8*>(pKey), seed, pHeader,
                            pNameTbl, pRandomTbl, pParticle, loop0);
            }
        }

        for (loop = loop0 + 1; loop < loop1; loop++) {
            pKey = pKeys;

            for (i = 0; i < pKeyTbl->count; pKey++, i++) {
                createChild(reinterpret_cast<u8*>(pKey), seed, pHeader,
                            pNameTbl, pRandomTbl, pParticle, loop0);
            }
        }

        pKey = pKeys;

        for (i = 0; i < pKeyTbl->count; pKey++, i++) {
            if (frame1 <= pKey->frame) {
                return;
            }

            createChild(reinterpret_cast<u8*>(pKey), seed, pHeader, pNameTbl,
                        pRandomTbl, pParticle, loop0);
        }
    }
}

} // namespace ef
} // namespace nw4r
