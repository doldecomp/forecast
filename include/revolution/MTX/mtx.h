#ifndef RVL_SDK_MTX_MTX_H
#define RVL_SDK_MTX_MTX_H
#include <types.h>

#include <revolution/MTX/mtxtypes.h>
#ifdef __cplusplus
extern "C" {
#endif

void PSMTXIdentity(Mtx m);
void PSMTXCopy(const Mtx src, Mtx dst);
void PSMTXConcat(const Mtx a, const Mtx b, Mtx ab);
void PSMTXConcatArray(const Mtx a, const Mtx* srcBase, Mtx* dstBase, u32 count);
void PSMTXTranspose(const Mtx src, Mtx xPose);
u32 PSMTXInverse(const Mtx src, Mtx inv);
u32 PSMTXInvXpose(const Mtx src, Mtx invX);
void PSMTXRotRad(Mtx m, char axis, f32 rad);
void PSMTXRotTrig(Mtx m, char axis, f32 sinA, f32 cosA);
void PSMTXRotAxisRad(Mtx m, const Vec* axis, f32 rad);
void PSMTXTrans(Mtx m, f32 xT, f32 yT, f32 zT);
void PSMTXTransApply(const Mtx src, Mtx dst, f32 xT, f32 yT, f32 zT);
void PSMTXScale(Mtx m, f32 xS, f32 yS, f32 zS);
void PSMTXScaleApply(const Mtx src, Mtx dst, f32 xS, f32 yS, f32 zS);
void PSMTXQuat(Mtx m, const Quaternion* q);

void C_MTXIdentity(Mtx m);
void C_MTXCopy(const Mtx src, Mtx dst);
void C_MTXConcat(const Mtx a, const Mtx b, Mtx ab);
void C_MTXConcatArray(const Mtx a, const Mtx* srcBase, Mtx* dstBase, u32 count);
u32 C_MTXInverse(const Mtx src, Mtx inv);
u32 C_MTXInvXpose(const Mtx src, Mtx invX);
void C_MTXRotRad(Mtx m, char axis, f32 rad);
void C_MTXRotTrig(Mtx m, char axis, f32 sinA, f32 cosA);
void C_MTXRotAxisRad(Mtx m, const Vec* axis, f32 rad);
void C_MTXTrans(Mtx m, f32 xT, f32 yT, f32 zT);
void C_MTXTransApply(const Mtx src, Mtx dst, f32 xT, f32 yT, f32 zT);
void C_MTXScale(Mtx m, f32 xS, f32 yS, f32 zS);
void C_MTXScaleApply(const Mtx src, Mtx dst, f32 xS, f32 yS, f32 zS);
void C_MTXLookAt(Mtx m, const Point3d* camPos, const Vec* camUp, const Point3d* target);
void C_MTXLightFrustum(Mtx m, f32 t, f32 b, f32 l, f32 r, f32 n, f32 scaleS, f32 scaleT, f32 transS, f32 transT);
void C_MTXLightPerspective(Mtx m, f32 fovY, f32 aspect, f32 scaleS, f32 scaleT, f32 transS, f32 transT);
void C_MTXLightOrtho(Mtx m, f32 t, f32 b, f32 l, f32 r, f32 scaleS, f32 scaleT, f32 transS, f32 transT);

#ifdef DEBUG
#define MTXIdentity C_MTXIdentity
#define MTXCopy C_MTXCopy
#define MTXConcat C_MTXConcat
#define MTXConcatArray C_MTXConcatArray
#define MTXInverse C_MTXInverse
#define MTXInvXpose C_MTXInvXpose
#define MTXRotRad C_MTXRotRad
#define MTXRotTrig C_MTXRotTrig
#define MTXRotAxisRad C_MTXRotAxisRad
#define MTXTrans C_MTXTrans
#define MTXTransApply C_MTXTransApply
#define MTXScale C_MTXScale
#define MTXScaleApply C_MTXScaleApply
#else
#define MTXIdentity PSMTXIdentity
#define MTXCopy PSMTXCopy
#define MTXConcat PSMTXConcat
#define MTXConcatArray PSMTXConcatArray
#define MTXInverse PSMTXInverse
#define MTXInvXpose PSMTXInvXpose
#define MTXRotRad PSMTXRotRad
#define MTXRotTrig PSMTXRotTrig
#define MTXRotAxisRad PSMTXRotAxisRad
#define MTXTrans PSMTXTrans
#define MTXTransApply PSMTXTransApply
#define MTXScale PSMTXScale
#define MTXScaleApply PSMTXScaleApply
#endif // DEBUG
#define MTXLookAt C_MTXLookAt                     /* C only */
#define MTXLightFrustum C_MTXLightFrustum         /* C only */
#define MTXLightPerspective C_MTXLightPerspective /* C only */
#define MTXLightOrtho C_MTXLightOrtho             /* C only */

#ifdef __cplusplus
}
#endif
#endif
