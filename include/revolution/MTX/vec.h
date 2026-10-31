#ifndef RVL_SDK_MTX_VEC_H
#define RVL_SDK_MTX_VEC_H
#include <types.h>

#include <revolution/MTX/mtxtypes.h>
#ifdef __cplusplus
extern "C" {
#endif

void PSVECAdd(const Vec* a, const Vec* b, Vec* sum);
void PSVECScale(const Vec* in, Vec* out, f32 scale);
void PSVECNormalize(const Vec* in, Vec* out);
f32 PSVECMag(const Vec* v);
f32 PSVECDotProduct(const Vec* a, const Vec* b);
void PSVECCrossProduct(const Vec* a, const Vec* b, Vec* prod);
f32 PSVECSquareDistance(const Vec* a, const Vec* b);
void C_VECHalfAngle(const Vec* a, const Vec* b, Vec* half);

void C_VECNormalize(const Vec* src, Vec* dst);
void C_VECCrossProduct(const Vec* a, const Vec* b, Vec* axb);

#ifdef DEBUG
#define VECNormalize C_VECNormalize
#define VECCrossProduct C_VECCrossProduct
#else
#define VECNormalize PSVECNormalize
#define VECCrossProduct PSVECCrossProduct
#endif // DEBUG

#ifdef __cplusplus
}
#endif
#endif
