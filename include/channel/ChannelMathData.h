#ifndef CHANNEL_CHANNEL_MATH_DATA_H
#define CHANNEL_CHANNEL_MATH_DATA_H

#include <types.h>

// These constants are defined in ChannelMathData.cpp. The code units must see
// them as external objects: defining them alongside the functions lets MWCC
// fold them into anonymous local constants, which does not match the target.
extern const f32 sDragScrollZero;
extern const f32 sDragScrollOffset;
extern const f32 sDragScrollThreshold;
extern const f32 sDragScrollNegThreshold;
extern const f32 sDragScrollMax;
extern const f32 sDragScrollAngleScale;
extern const f32 sDragScrollHalfPi;
extern const f32 sDragScrollTimerScale;

extern const f32 sSmoothEpsilon;
extern const f32 sSmoothEpsilonNeg;
extern const f32 sAngleZero;
extern const f32 sAngleFull;
extern const f32 sAngleHalfNeg;
extern const f32 sAngleHalf;
extern const f32 sIdxToFIdx;
extern const f32 sHalf;

extern const f32 sDegToRad;
extern const f32 sDegToFIdx;

#endif  // CHANNEL_CHANNEL_MATH_DATA_H
