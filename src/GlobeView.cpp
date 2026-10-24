#include "SimpleGlobe.h"
#include "System.h"
#include "Vec3.h"
#include "SimpleModel.h"
#include "MtxUtil.h"
#include <nw4r/g3d/g3d_camera.h>
#include <nw4r/math.h>
#include <revolution/MTX.h>


Vec3 gGlobeRotation(0.0f, 0.0f, 0.0f);

GlobeView::GlobeView(nw4r::g3d::Camera camera) : mCamera(camera) {
    mUp.x = 0.0f;
    mUp.y = 1.0f;
    mUp.z = 0.0f;
    mTarget.x = 0.0f;
    mTarget.y = 0.0f;
    mTarget.z = 0.0f;
    mPosition.x = 0.0f;
    mPosition.y = 0.0f;
    mPosition.z = 0.0f;
    mOrientation.x = 0.0f;
    mOrientation.y = 0.0f;
    mOrientation.z = 0.0f;
    mCameraPos.x = 0.0f;
    mCameraPos.y = 0.0f;
    mCameraPos.z = 0.0f;
    mDirection.x = 0.0f;
    mDirection.y = 0.0f;
    mDirection.z = 0.0f;
    mResetting = FALSE;
    mFOVy = 5.0f;
    mAspect = (f32)(gWidescreen ? 832 : 608) / 456.0f;
    mNear = 0.1f;
    mFar = 200.0f;
    mZoom = 100.0f;
    mViewportX = 0.0f;
    mViewportY = 0.0f;
    mViewportW = 0.0f;
    mViewportH = 0.0f;
    mUnusedE8 = 0;
    PSMTXIdentity(mCameraMtx);
    nw4r::math::MTX44Identity((nw4r::math::MTX44*)mProjMtx);
}

Mtx34::~Mtx34() {}

GlobeView::~GlobeView() {}

void GlobeView::Setup(Vec* position) {
    mCamera.SetPerspective(mFOVy, mAspect, mNear, mFar);
    mCamera.SetScissor(0, 0, gRenderMode.fbWidth, gRenderMode.efbHeight);
    mCamera.SetViewport(0.0f, 0.0f, (f32)(s32)gRenderMode.fbWidth, (f32)(s32)gRenderMode.efbHeight);
    if (position != NULL) {
        mPosition.x = position->x;
        mPosition.y = position->y;
        mPosition.z = position->z;
    }
}

void GlobeView::CalcCamera() {
    Vec vec;
    vec.x = 0.0f;
    vec.y = 0.0f;
    vec.z = gModelRange;
    Vec zoomOffset;
    zoomOffset.x = 0.0f;
    zoomOffset.y = 0.0f;
    zoomOffset.z = mZoom;

    PSMTXTrans(gModelMtx.m, vec.x, vec.y, vec.z);
    RotateX(gModelMtx.m, -mPosition.x);
    RotateZ(gModelMtx.m, mPosition.z);
    RotateY(gModelMtx.m, mPosition.y);

    Mtx look;
    PSMTXCopy(gModelMtx.m, look);
    mTarget.x = look[0][3];
    mTarget.y = look[1][3];
    mTarget.z = look[2][3];

    // Read back to front, before the target stores
    f32 tz = zoomOffset.z;
    f32 ty = zoomOffset.y;
    f32 tx = zoomOffset.x;
    PSMTXTrans(gModelMtx.m, tx, ty, tz);
    RotateX(gModelMtx.m, mOrientation.x);
    RotateZ(gModelMtx.m, mOrientation.z);
    RotateY(gModelMtx.m, mOrientation.y);
    PSMTXConcat(look, gModelMtx.m, gModelMtx.m);
    PSMTXCopy(gModelMtx.m, look);

    mCameraPos.x = look[0][3];
    mCameraPos.y = look[1][3];
    mCameraPos.z = look[2][3];

    vec.z = 0.0f;
    vec.x = 0.0f;
    vec.y = 1.0f;
    PSMTXMultVec(gModelMtx.m, &vec, &mUp);

    mCamera.SetPosition(mCameraPos.x, mCameraPos.y, mCameraPos.z);

    mDirection.x = mTarget.x - mCameraPos.x;
    mDirection.y = mTarget.y - mCameraPos.y;
    mDirection.z = mTarget.z - mCameraPos.z;

    nw4r::g3d::Camera::PostureInfo posture;
    posture.tp = nw4r::g3d::Camera::POSTURE_LOOKAT;
    posture.cameraUp.x = mUp.x;
    posture.cameraUp.y = mUp.y;
    posture.cameraUp.z = mUp.z;
    posture.cameraTarget.x = mTarget.x;
    posture.cameraTarget.y = mTarget.y;
    posture.cameraTarget.z = mTarget.z;
    mCamera.SetPosture(posture);

    mCamera.GetCameraMtx((nw4r::math::MTX34*)mCameraMtx);
    mCamera.GetProjectionMtx((nw4r::math::MTX44*)mProjMtx);
    mCamera.GetViewport(&mViewportX, &mViewportY, &mViewportW, &mViewportH, NULL, NULL);

    mViewportW *= 0.5f;
    mViewportH *= 0.5f;
    mViewportX += mViewportW;
    mViewportY += mViewportH;

    mViewportX = mViewportX * (gWidescreen ? 1.3684211f : 1.0f);
    mViewportW = mViewportW * (gWidescreen ? 1.3684211f : 1.0f);
}

void GlobeView::StartReset() {
    mResetting = TRUE;
    BOOL x = Approach(gGlobeRotation.x, &mOrientation.x);
    BOOL y = Approach(gGlobeRotation.y, &mOrientation.y);
    BOOL z = Approach(gGlobeRotation.z, &mOrientation.z);
    if (x && y && z) {
        mResetting = FALSE;
    }
}

// Not called anywhere: the linker strips it from the DOL, but its constants stay in this
// file's .sdata2 pool, ahead of IsSettled's. The original pool order (180, 360, -180 before
// 0.0008) needs it here.
static f32 WrapAngle(f32 angle) {
    if (angle > 180.0f) {
        angle -= 360.0f;
    } else if (angle < -180.0f) {
        angle += 360.0f;
    }
    return angle;
}

static inline BOOL IsNear(f32 diff) {
    return diff < 0.0008f && diff > -0.0008f;
}

BOOL GlobeView::IsSettled() {
    BOOL x = IsNear(mOrientation.x - gGlobeRotation.x);
    BOOL y = IsNear(mOrientation.y - gGlobeRotation.y);
    f32 dz = mOrientation.z - gGlobeRotation.z;
    BOOL result = FALSE;
    if (x) {
        if (y) {
            if (dz < 0.0008f && dz > -0.0008f) {
                result = TRUE;
            }
        }
    }
    return result;
}

BOOL GlobeView::UpdateReset() {
    if (mResetting == TRUE) {
        BOOL x = Approach(gGlobeRotation.x, &mOrientation.x);
        BOOL y = Approach(gGlobeRotation.y, &mOrientation.y);
        BOOL z = Approach(gGlobeRotation.z, &mOrientation.z);
        if (x && y && z) {
            mResetting = FALSE;
        }
    }
    return mResetting;
}

BOOL GlobeView::Approach(f32 target, f32* value) {
    f32 diff = target - *value;
    if (diff < -180.0f) {
        diff += 360.0f;
    } else if (diff > 180.0f) {
        diff -= 360.0f;
    }

    BOOL reached = FALSE;
    if (diff < 0.008f && diff > -0.008f) {
        reached = TRUE;
    }
    if (reached) {
        *value = target;
        return reached;
    }

    reached = FALSE;
    diff *= 0.05f;
    if (diff < 0.008f && diff > -0.008f) {
        reached = TRUE;
    }
    if (reached) {
        *value = target;
        return reached;
    }

    *value = *value + diff;
    if (*value < 0.0f) {
        *value += 360.0f;
    } else if (*value >= 360.0f) {
        *value -= 360.0f;
    }
    return reached;
}

void GlobeView::Project(f32* screen, const Vec* world) {
    Mtx cameraMtx;
    Mtx44 projMtx;
    Vec view;
    f32 clip[4];
    f32 vx, vy, vw, vh;

    mCamera.GetCameraMtx((nw4r::math::MTX34*)cameraMtx);
    mCamera.GetProjectionMtx((nw4r::math::MTX44*)projMtx);
    mCamera.GetViewport(&vx, &vy, &vw, &vh, NULL, NULL);

    // Both globals are read before the viewport math
    s32 fbWidth = gRenderMode.fbWidth;
    BOOL widescreen = gWidescreen;
    f32 halfW = vw / 2.0f;
    f32 halfH = vh / 2.0f;
    vx = vx + halfW;
    vy = vy + halfH;
    vh = halfH;
    vx = vx * ((f32)(widescreen ? 832 : 608) / (f32)fbWidth);
    vw = halfW * ((f32)(widescreen ? 832 : 608) / (f32)fbWidth);

    PSMTXMultVec(cameraMtx, world, &view);
    MultVec4(clip, projMtx, &view);
    f32 invW = 1.0f / clip[3];
    clip[3] = invW;
    screen[0] = vx + vw * (clip[0] * invW);
    screen[1] = vy - vh * (clip[1] * clip[3]);
}
