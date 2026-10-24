#include "SimpleGlobe.h"
#include "SceneBase.h"
#include "SimpleModel.h"
#include "System.h"
#include "DrawUtil.h"
#include "ColorWhite.h"
#include "MtxUtil.h"
#include "Sound.h"
#include "nw4r/snd.h"
#include "nw4r/g3d/g3d_light.h"
#include "nw4r/g3d/g3d_camera.h"
#include "nw4r/g3d/g3d_scnobj.h"
#include "nw4r/g3d/g3d_scnroot.h"
#include "nw4r/ut/ut_algorithm.h"
#include "revolution/MTX/mtxtypes.h"
#include <cstddef>
#include <cmath>
#include "nw4r/g3d/res/g3d_resmdl.h"
#include "nw4r/g3d/res/g3d_resmat.h"

// Camera distance from the globe per zoom level
const f32 sZoomDistances[10] = {
    1.0f, 2.0f, 5.0f, 8.0f, 12.0f,
    17.0f, 25.0f, 40.0f, 65.0f, 100.0f,
};

// Tilt angle per tilt level; extern because WeatherScene reads it too
extern const f32 gGlobeZooms[6] = {
    0.0f, 45.0f, 55.0f, 65.0f, 73.0f, 80.0f,
};

// Spin damping per zoom level
const f32 sSpinDamping[10] = {
    0.85f, 0.9f, 0.95f, 0.97f, 0.98f,
    0.98f, 0.98f, 0.98f, 0.98f, 0.98f,
};

// Unused in this file; extern keeps -ipa file from dropping it, as the original kept it
extern const f32 gUnusedGlobeTable[6] = {
    0.1f, 2.1f, 0.6f, 0.4f, 0.1f, 0.2f,
};

#pragma explicit_zero_data on
f32 speedX = 0.0f;
f32 speedY = 0.0f;
#pragma explicit_zero_data reset

SimpleGlobe::SimpleGlobe() :
mScnRoot(NULL), mView(NULL),
mRotation(0.0f, 0.0f, 0.0f),
mNorthPole(0.0f, gModelRange, 0.0f),
mSouthPole(0.0f, -gModelRange, 0.0f)
{
    u32 val;

    this->mSpeed.x = 0.0f;
    this->mSpeed.y = 0.0f;

    this->mUnused74 = 0.0f;
    this->mUnused78 = 0.0f;
    this->mNorthScreen[0] = 0.0f;
    this->mNorthScreen[1] = 0.0f;
    this->mSouthScreen[0] = 0.0f;
    this->mSouthScreen[1] = 0.0f;

    this->mSpinning = 0;
    this->mZoomIn = 0;
    this->mZoomOut = 0;
    this->mTiltUp = 0;
    this->mTiltDown = 0;
    this->mCentering = 0;
    this->mNorthFacing = 0;
    this->mNorthVisible = 0;
    this->mSouthFacing = 0;
    this->mSouthVisible = 0;
    this->mNorthBehind = 0;
    this->mSouthBehind = 0;

    this->mZoomLevel = 0;
    this->mTiltLevel = 0;

    this->mZoomDist = sZoomDistances[8];
    this->mZoomDistTarget = sZoomDistances[8];
    this->mTilt = 0.0f;
    this->mTiltTarget = 0.0f;
    this->mZoom = 1.0f;

    this->mScnRoot = nw4r::g3d::ScnRoot::Construct(&gMEM2Allocator, &val, 0x1F, 0x100, 0x80, 0x80);
    this->mScnRoot->SetCurrentCamera(0);

    this->mView = new GlobeView(this->mScnRoot->GetCurrentCamera());
    this->mView->setZoom(this->mZoomDist);

    this->mGrabbed[0] = 0;
    this->mGrabbed[1] = 0;
    this->mGrabbed[2] = 0;
    this->mGrabbed[3] = 0;
}

SimpleGlobe::~SimpleGlobe() {
    delete this->mView;
    this->mScnRoot->Destroy();
}

void SimpleGlobe::SetRotation(const Vec* rotation, s32 frames) {
    nw4r::g3d::ScnRoot *scnRoot = this->mScnRoot;
    this->mZoomIn = 0;
    this->mZoomOut = 0;
    this->mTiltUp = 0;
    this->mTiltDown = 0;

    while (scnRoot->Size() != 0) {
        u32 i = scnRoot->Size();
        if (i > 0) {
            scnRoot->Remove(--i);
        }
    }

    if (gEarthModel != 0) {
        gEarthModel->Calc();
        this->mScnRoot->Insert(this->mScnRoot->Size(), (nw4r::g3d::ScnObj *)gEarthModel->mScnMdl);
    }

    this->mGrabbed[0] = 0;
    this->mGrabbed[1] = 0;
    this->mGrabbed[2] = 0;
    this->mGrabbed[3] = 0;

    this->mRotation = *static_cast<const Vector3 *>(rotation);

    this->mSpinning = 0;

    this->mSpeed.x = speedX;
    this->mSpeed.y = speedY;

    this->mZoomLevel = frames;

    this->mZoomDist = sZoomDistances[frames];
    this->mZoomDistTarget = sZoomDistances[frames];

    this->mView->setZoom(sZoomDistances[frames]);

    this->mView->Setup(&this->mRotation);

    nw4r::g3d::Camera camera = this->mScnRoot->GetCamera(1);
    camera.Init(gRenderMode.fbWidth, gRenderMode.efbHeight, gRenderMode.fbWidth, gRenderMode.xfbHeight, (gWidescreen) ? 0x340 : 0x260, 0x1C8);
    camera.SetPerspective(this->mView->getFOVy(), this->mView->getAspect(), this->mView->getNear(), this->mView->getFar());
    camera.SetScissor(0, 0, gRenderMode.fbWidth, gRenderMode.efbHeight);
    camera.SetViewport(0.0f, 0.0f, (int)gRenderMode.fbWidth, (int)gRenderMode.efbHeight);
}

nw4r::snd::SoundHandle gRotateSoundHandle;
Color gGlobeLightColor(0xFFFFFFFF);
f32 SmoothApproach(f32* value, f32 target, f32 rate, f32 limit, f32 epsilon);
// SmoothApproach for an angle in degrees, going the short way around and wrapping into [0, 360)
f32 SmoothApproachAngle(f32* value, f32 target, f32 rate, f32 limit, f32 epsilon);

void SimpleGlobe::Setup(const Vec* rotation) {
    SetRotation(rotation, mZoomLevel);

    if (gEarthModel != NULL) {
        gEarthModel->Calc();
    }

    nw4r::g3d::LightSet lightSet = mScnRoot->GetLightSet(0);
    lightSet.SelectLightObj(0, 0);
    lightSet.SelectLightObj(1, -1);
    lightSet.SelectLightObj(2, -1);
    lightSet.SelectLightObj(3, -1);
    lightSet.SelectLightObj(4, -1);
    lightSet.SelectLightObj(5, -1);
    lightSet.SelectLightObj(6, -1);
    lightSet.SelectLightObj(7, -1);
    lightSet.SelectAmbLightObj(-1);

    nw4r::g3d::LightObj* light = lightSet.GetLightObj(0);
    light->Clear();
    light->InitLightColor((nw4r::ut::Color&)gGlobeLightColor);
    light->InitLightAttnA(1.0f, 0.0f, 0.0f);
    light->InitLightAttnK(1.0f, 0.0f, 0.0f);
    light->Enable();

    GXColor clearColor = {0, 0, 0, 255};
    GXSetCopyClear(clearColor, 0xFFFFFF);
}

void SimpleGlobe::DrawModel() {
    if (gEarthModel != NULL) {
        gEarthModel->Draw();
    }
}

void SimpleGlobe::Draw() {
    if (mScnRoot != NULL) {
        mScnRoot->DrawOpa();
        mScnRoot->DrawXlu();
    }

    if (gGlobeAlpha != 0) {
        SetDefaultGXState();
        SetOrthoProjection();

        nw4r::ut::Rect rect(0.0f, 0.0f, (f32)(gWidescreen ? 832 : 608), 456.0f);
        GXColor color;
        color.r = 0;
        color.g = 0;
        color.b = 0;
        color.a = gGlobeAlpha;
        DrawRect((Rect*)&rect, &color);
    }
}

void SimpleGlobe::ClearInput() {
    nw4r::g3d::ScnRoot* scnRoot = mScnRoot;
    mZoomIn = 0;
    mZoomOut = 0;
    mTiltUp = 0;
    mTiltDown = 0;

    while (scnRoot->Size() != 0) {
        u32 i = scnRoot->Size();
        if (i > 0) {
            scnRoot->Remove(--i);
        }
    }

    if (gEarthModel != NULL) {
        gEarthModel->Calc();
        mScnRoot->Insert(mScnRoot->Size(), (nw4r::g3d::ScnObj*)gEarthModel->mScnMdl);
    }
}

void SimpleGlobe::UpdateView() {
    if (mView != NULL) {
        UpdateWater();
        mView->CalcCamera();
    }
}

static inline f32 Dot(const nw4r::math::VEC3* a, const nw4r::math::VEC3* b) {
    return nw4r::math::VEC3Dot(a, b);
}

// The original copied this vector through an inlined local, which is why it gets its own
// stack slot below the operator temporaries.
static inline void CopyVec(nw4r::math::VEC3* out, const nw4r::math::VEC3& v) {
    nw4r::math::VEC3 copy;
    copy = v;
    *out = copy;
}

void SimpleGlobe::UpdateFacing() {
    if (gEarthModel != NULL) {
        gEarthModel->UpdateMtx();
    }

    GlobeView* view = mView;
    nw4r::math::VEC3 toTarget;
    toTarget = *(nw4r::math::VEC3*)&view->mTarget - *(nw4r::math::VEC3*)&view->mCameraPos;
    nw4r::math::VEC3 a = *(nw4r::math::VEC3*)&mNorthPole;
    nw4r::math::VEC3 b;
    b = *(nw4r::math::VEC3*)&mNorthPole - *(nw4r::math::VEC3*)&view->mCameraPos;
    nw4r::math::VEC3Normalize(&a, &a);
    nw4r::math::VEC3Normalize(&toTarget, &toTarget);
    nw4r::math::VEC3Normalize(&b, &b);
    mNorthFacing = Dot(&a, &toTarget) < 0.0f;
    mNorthVisible = Dot(&a, &b) < 0.0f;
    mNorthBehind = Dot(&toTarget, &b) < 0.0f;

    a = *(nw4r::math::VEC3*)&mSouthPole;
    CopyVec(&b, *(nw4r::math::VEC3*)&mSouthPole - *(nw4r::math::VEC3*)&view->mCameraPos);
    nw4r::math::VEC3Normalize(&a, &a);
    nw4r::math::VEC3Normalize(&b, &b);
    mSouthFacing = Dot(&a, &toTarget) < 0.0f;
    mSouthVisible = Dot(&a, &b) < 0.0f;
    mSouthBehind = Dot(&toTarget, &b) < 0.0f;

    mView->Project(mNorthScreen, &mNorthPole);
    mView->Project(mSouthScreen, &mSouthPole);
}

static inline u32 GetNumLights(nw4r::g3d::LightSet* lightSet) {
    return (*(nw4r::g3d::LightSetting**)lightSet)->GetNumLightObj();
}

void SimpleGlobe::UpdateLight() {
    GlobeView* view = mView;
    if (view != NULL && mScnRoot != NULL) {
        nw4r::g3d::LightSet lightSet = mScnRoot->GetLightSet(0);
        if (GetNumLights(&lightSet) != 0) {
            for (u32 i = 0; i < GetNumLights(&lightSet); i++) {
                nw4r::g3d::LightObj* light = lightSet.GetLightObj(i);
                if (light != NULL) {
                    light->InitLightPos(view->mCameraPos.x, view->mCameraPos.y, view->mCameraPos.z);
                    light->InitLightDir(view->mDirection.x, view->mDirection.y, view->mDirection.z);
                }
            }
        }
    }
}

void SimpleGlobe::Calc() {
    if (mScnRoot != NULL) {
        mScnRoot->UpdateFrame();
        mScnRoot->CalcWorld();
        mScnRoot->CalcMaterial();
        mScnRoot->CalcView();
        mScnRoot->GatherDrawScnObj();
        mScnRoot->ZSort();
    }
}

void SimpleGlobe::UpdateZoom(const s32* sounds) {
    s32 prev = mZoomLevel;
    if (mView != NULL) {
        if (mZoomIn) {
            mZoomLevel = prev + 1;
            if (mZoomLevel >= 10) {
                mZoomLevel = 9;
            }
            mZoomDistTarget = sZoomDistances[mZoomLevel];
            if (mZoomLevel != prev) {
                PlaySE(sounds[mZoomLevel]);
            }
        } else if (mZoomOut) {
            mZoomLevel = prev - 1;
            if (mZoomLevel < 0) {
                mZoomLevel = 0;
            }
            mZoomDistTarget = sZoomDistances[mZoomLevel];
            if (mZoomLevel != prev) {
                PlaySE(sounds[mZoomLevel]);
            }
        }

        mZoomDist = mView->mZoom;
        SmoothApproach(&mZoomDist, mZoomDistTarget, 0.1f, 100.0f, 0.001f);
        mView->mZoom = mZoomDist;
        if (mView != NULL) {
            mZoom = mView->mZoom;
        }
    }
}

void SimpleGlobe::SyncZoom() {
    if (mView != NULL) {
        mZoom = mView->mZoom;
    }
}

Vec GetPosition(GlobeView* view);
Vec GetOrientation(GlobeView* view);

BOOL SimpleGlobe::UpdateGrab(s32 chan) {
    if (gTrig[chan] & WPAD_BUTTON_A) {
        mGrabbed[chan] = 1;
        for (s32 i = 0; i < 4; i++) {
            if (chan != i) {
                mGrabbed[i] = 0;
            }
        }

        mView->mResetting = FALSE;
        mCentering = 0;
        mSpinning = 0;
        f32 x = GetPosition(mView).x;
        mGrabPos[chan].x = x;
        f32 y = GetPosition(mView).y;
        mGrabPos[chan].y = y;
        mGrabCursor[chan].x = gCursorX[chan];
        mGrabCursor[chan].y = gCursorY[chan];
        mGrabRoll = GetOrientation(mView).z;

        // The original works on a local copy of the sample
        KPADStatus status;
        const KPADStatus& sample = gKPADStatus[chan][0];
        status = sample;
        mRoll[chan] = nw4r::math::Atan2Deg(status.horizon.x, -status.horizon.y);
        mDragSign = 1.0f;
        if (mNorthVisible) {
            if (!mNorthBehind) {
                mDragSign = mGrabCursor[chan].y < mNorthScreen[1] ? -1.0f : 1.0f;
            }
        } else if (mSouthVisible) {
            if (!mSouthBehind) {
                mDragSign = mGrabCursor[chan].y > mSouthScreen[1] ? -1.0f : 1.0f;
            }
        }
        return TRUE;
    }
    return FALSE;
}

Vec GetPosition(GlobeView *view) {
    return *view->getPosition();
}

Vec GetOrientation(GlobeView *view) {
    return *view->getOrientation();
}

s32 SimpleGlobe::UpdateDrag(s32 chan) {
    GlobeView* view = mView;
    if (view == NULL) {
        return 0;
    }

    f32 factor = 0.5f * (0.01f * mZoom);
    if (mGrabbed[chan]) {
        if (gHold[chan] & WPAD_BUTTON_A) {
            // The original works on a local copy of the sample
            KPADStatus status;
            const KPADStatus& sample = gKPADStatus[chan][0];
            status = sample;
            f32 roll = nw4r::math::Atan2Deg(status.horizon.x, -status.horizon.y);
            f32 rollTarget = roll - mRoll[chan];
            if (nw4r::math::FAbs(rollTarget) > 30.0f) {
                rollTarget += mGrabRoll;
                if (rollTarget < 0.0f) {
                    rollTarget += 360.0f;
                } else if (rollTarget >= 360.0f) {
                    rollTarget -= 360.0f;
                }
                mView->mResetting = FALSE;
                mSpeed.x = 0.0f;
                mSpeed.y = 0.0f;
            } else {
                Vec drag;
                Vec2 last;
                last.x = view->mPosition.x;
                last.y = view->mPosition.y;
                rollTarget = mGrabRoll;

                drag.x = factor * (gCursorY[chan] - mGrabCursor[chan].y);
                BOOL level = FALSE;
                if (view->mOrientation.z < 0.0008f && view->mOrientation.z > -0.0008f) {
                    level = TRUE;
                }
                if (level) {
                    drag.y = factor * (mDragSign * (gCursorX[chan] - mGrabCursor[chan].x));
                } else {
                    drag.y = factor * (gCursorX[chan] - mGrabCursor[chan].x);
                }
                drag.z = 0.0f;
                SetRotateZ(gModelMtx.m, view->mOrientation.z);
                PSMTXMultVec(gModelMtx.m, &drag, &drag);

                view->mPosition.x = mGrabPos[chan].x + drag.x;
                view->mPosition.y = mGrabPos[chan].y - drag.y;
                if (view->mPosition.x > 89.0f) {
                    view->mPosition.x = 89.0f;
                } else if (view->mPosition.x < -89.0f) {
                    view->mPosition.x = -89.0f;
                }
                while (view->mPosition.y < -180.0f) {
                    view->mPosition.y += 360.0f;
                }
                while (view->mPosition.y > 180.0f) {
                    view->mPosition.y -= 360.0f;
                }

                mSpeed.x = view->mPosition.x - last.x;
                f32 deltaY = view->mPosition.y - last.y;
                mSpeed.y = deltaY;
                if (deltaY < -180.0f) {
                    mSpeed.y += 360.0f;
                } else if (deltaY > 180.0f) {
                    mSpeed.y -= 360.0f;
                }
            }
            SmoothApproachAngle(&view->mOrientation.z, rollTarget, 0.1f, 180.0f, 1.0f);
            return 1;
        }
        mGrabbed[chan] = 0;
        mSpinning = 1;
        return 2;
    }
    return 0;
}

void SimpleGlobe::SetZoom(s32 level, s32 flag) {
    if (mZoomLevel >= 8) {
        mCentering = flag;
    }
    mTiltLevel = level;
    mTiltTarget = gGlobeZooms[level];
    mView->StartReset();
}

BOOL SimpleGlobe::IsDefaultView() {
    if (mZoomLevel >= 8) {
        BOOL result = FALSE;
        f32 x = mView->mPosition.x;
        BOOL near = FALSE;
        if (x < 0.0008f && x > -0.0008f) {
            near = TRUE;
        }
        if (near && mView->IsSettled()) {
            result = TRUE;
        }
        return !result;
    }
    return !mView->IsSettled();
}

void SimpleGlobe::SetMode(s32 mode) {
    mTiltLevel = mode;
    mTiltTarget = gGlobeZooms[mode];
    mTilt = mTiltTarget;
    mView->mOrientation.x = mTiltTarget;
    mView->mResetting = FALSE;
}

void SimpleGlobe::UpdateTilt(u32 arg, const s32* sounds) {
    if (mView != NULL) {
        BOOL moving = mView->UpdateReset();
        s32 prev = mTiltLevel;
        if (mTiltUp == 1) {
            mTiltLevel = prev + 1;
            if (mTiltLevel >= 6) {
                mTiltLevel = 5;
            }
            mTiltTarget = gGlobeZooms[mTiltLevel];
            mView->mResetting = FALSE;
            moving = TRUE;
            if (mTiltLevel != prev) {
                PlaySE(sounds[mTiltLevel]);
            }
        } else if (mTiltDown == 1) {
            mTiltLevel = prev - 1;
            if (mTiltLevel < 0) {
                mTiltLevel = 0;
            }
            mTiltTarget = gGlobeZooms[mTiltLevel];
            mView->mResetting = FALSE;
            moving = TRUE;
            if (mTiltLevel != prev) {
                PlaySE(sounds[mTiltLevel]);
            }
        }

        if (!moving) {
            mTilt = mView->mOrientation.x;
            SmoothApproach(&mTilt, mTiltTarget, 0.1f, 100.0f, 0.001f);
            mView->mOrientation.x = mTilt;
        }

        if (mCentering) {
            f32* x = &mView->mPosition.x;
            BOOL near = FALSE;
            if (*x < 0.0008f && *x > -0.0008f) {
                near = TRUE;
            }
            if (near) {
                mCentering = 0;
                mView->mPosition.x = 0.0f;
            } else if (*x < 0.0f) {
                if (!SmoothApproach(x, 0.0f, 0.1f, 100.0f, 0.001f)) {
                    mCentering = 0;
                }
            } else if (*x > 0.0f) {
                if (!SmoothApproach(x, 0.0f, 0.1f, 100.0f, 0.001f)) {
                    mCentering = 0;
                }
            }
        }
    }
}

static inline BOOL IsNearZero(f32 x) {
    return x < 0.0008f && x > -0.0008f;
}

void SimpleGlobe::UpdateRotation(u32 stop) {
    GlobeView* view = mView;
    if (view != NULL) {
        if (stop == 1) {
            mSpinning = 0;
        }

        if (mSpinning) {
            view->mPosition.x += mSpeed.x;
            if (view->mPosition.x > 89.0f) {
                view->mPosition.x = 89.0f;
                mSpeed.x = 0.0f;
                f32 spinY = mSpeed.y;
                BOOL slow = FALSE;
                if (spinY < 1.0f && spinY > -1.0f) {
                    slow = TRUE;
                }
                if (slow && IsSoundPlaying(&gRotateSoundHandle)) {
                    StopSound(&gRotateSoundHandle, 0);
                }
            } else if (view->mPosition.x < -89.0f) {
                view->mPosition.x = -89.0f;
                mSpeed.x = 0.0f;
                f32 spinY = mSpeed.y;
                BOOL slow = FALSE;
                if (spinY < 1.0f && spinY > -1.0f) {
                    slow = TRUE;
                }
                if (slow && IsSoundPlaying(&gRotateSoundHandle)) {
                    StopSound(&gRotateSoundHandle, 0);
                }
            }

            view->mPosition.y += mSpeed.y;
            if (view->mPosition.y < -180.0f) {
                view->mPosition.y += 360.0f;
            } else if (view->mPosition.y > 180.0f) {
                view->mPosition.y -= 360.0f;
            }

            f32 loss = mSpeed.y * (1.0f - sSpinDamping[mZoomLevel]);
            mSpeed.y -= loss;
            if (IsNearZero(mSpeed.y)) {
                mSpeed.y = 0.0f;
            }

            f32 absLoss = nw4r::math::FAbs(loss);
            if (absLoss < 0.05f) {
                mSpeed.x = mSpeed.x * sSpinDamping[mZoomLevel];
                if (IsNearZero(mSpeed.x)) {
                    mSpeed.x = 0.0f;
                }
            } else if (!IsNearZero(mSpeed.x)) {
                if (mSpeed.x < 0.0f) {
                    mSpeed.x += absLoss;
                    if (mSpeed.x > 0.0f) {
                        mSpeed.x = 0.0f;
                    }
                } else if (mSpeed.x > 0.0f) {
                    mSpeed.x -= absLoss;
                    if (mSpeed.x < 0.0f) {
                        mSpeed.x = 0.0f;
                    }
                }
            }

            f32 total = nw4r::math::FAbs(nw4r::math::FSqrt(mSpeed.x * mSpeed.x + mSpeed.y * mSpeed.y));
            BOOL stopped = FALSE;
            if (total < 0.001f && total > -0.001f) {
                stopped = TRUE;
            }
            if (stopped) {
                mSpinning = 0;
                mSpeed.y = 0.0f;
                mSpeed.x = 0.0f;
            }
        }
    }
}

void SimpleGlobe::SetZoomLevel(s32 level) {
    mZoomLevel = level;
    mZoomDistTarget = sZoomDistances[level];
}

void SimpleGlobe::SetSpeed(f32 speed) {
    if (mView != NULL) {
        mView->mOrientation.z = speed;
    }
}

void SimpleGlobe::PlayRotateSound(u32 id) {
    f32 speed = nw4r::math::FAbs(nw4r::math::FSqrt(mSpeed.x * mSpeed.x + mSpeed.y * mSpeed.y));
    BOOL still = FALSE;
    if (speed < 1.0f && speed > -1.0f) {
        still = TRUE;
    }
    if (!still) {
        f32 pitch;
        f32 volume;
        volume = speed / 5.0f;
        if (volume > 1.0f) {
            volume = 1.0f;
        }
        pitch = speed / 90.0f;
        if (pitch > 1.0f) {
            pitch = 1.0f;
        }
        pitch += 0.5f;
        StartSound(&gRotateSoundHandle, id);
        SetSoundVolume(&gRotateSoundHandle, volume);
        SetSoundPitch(&gRotateSoundHandle, pitch);
        SetSoundPan(&gRotateSoundHandle, 0.0f);
    }
}

static inline f32 Clamp01(f32 x) {
    return x > 1.0f ? 1.0f : (x < 0.0f ? 0.0f : x);
}

// Unbiased binary exponent of x, read from its IEEE bits
static inline s32 GetExponent(f32 x) {
    return ((*(u32*)&x >> 23) & 0xFF) - 127;
}

void SimpleGlobe::UpdateWater() {
    nw4r::math::MTX34 indMtx;
    nw4r::g3d::Camera::PostureInfo posture;
    Mtx look;
    Vec first, second, up, position, target, cameraUp;
    f32 fade = nw4r::ut::Min(Clamp01((mZoomDist - 2.0f) / 15.0f), Clamp01((100.0f - mZoomDist) / 35.0f));

    f32 ripple = Clamp01((mZoomDist - 40.0f) / 20.0f);
    f32 waveScale = 0.85f + 0.15f * ripple;
    f32 alphaScale = Clamp01((mZoomDist - 2.0f) / 15.0f);

    s8 exponent = 0;
    f32 scale = 0.0f;
    if (fade > 1e-18f) {
        exponent = GetExponent(1.0f + fade);
        scale = (f32)ldexp(1.0, -exponent);
    }

    indMtx._00 = fade * scale;
    indMtx._01 = 0.0f;
    indMtx._02 = 0.0f;
    indMtx._10 = 0.0f;
    indMtx._11 = fade * scale;
    indMtx._12 = 0.0f;

    nw4r::g3d::Camera camera = mScnRoot->GetCamera(1);

    f32 range = gModelRange;
    f32 zoom = mView->mZoom;
    first.x = 0.25f * -range;
    first.y = 0.25f * range;
    first.z = range;
    second.x = 0.25f * -zoom;
    second.y = 0.25f * zoom;
    second.z = zoom;
    up.x = 0.0f;
    up.y = 1.0f;
    up.z = 0.0f;

    PSMTXTrans(gModelMtx.m, first.x, first.y, first.z);
    RotateX(gModelMtx.m, -mView->mPosition.x);
    RotateZ(gModelMtx.m, mView->mPosition.z);
    RotateY(gModelMtx.m, mView->mPosition.y);

    PSMTXCopy(gModelMtx.m, look);
    target.x = look[0][3];
    target.y = look[1][3];
    target.z = look[2][3];

    // Read back to front, before the target stores
    f32 z = second.z;
    f32 y = second.y;
    f32 x = second.x;
    PSMTXTrans(gModelMtx.m, x, y, z);
    RotateX(gModelMtx.m, mView->mOrientation.x);
    RotateZ(gModelMtx.m, mView->mOrientation.z);
    RotateY(gModelMtx.m, mView->mOrientation.y);
    PSMTXConcat(look, gModelMtx.m, gModelMtx.m);
    PSMTXCopy(gModelMtx.m, look);

    position.x = look[0][3];
    position.y = look[1][3];
    position.z = look[2][3];
    gModelMtx.m[0][3] = 0.0f;
    gModelMtx.m[1][3] = 0.0f;
    gModelMtx.m[2][3] = 0.0f;

    PSMTXMultVec(gModelMtx.m, &up, &cameraUp);
    camera.SetPosition(*(nw4r::math::VEC3*)&position);

    posture.tp = nw4r::g3d::Camera::POSTURE_LOOKAT;
    posture.cameraUp.x = cameraUp.x;
    posture.cameraUp.y = cameraUp.y;
    posture.cameraUp.z = cameraUp.z;
    posture.cameraTarget.x = target.x;
    posture.cameraTarget.y = target.y;
    posture.cameraTarget.z = target.z;
    camera.SetPosture(posture);

    if (gEarthModel != NULL) {
        nw4r::g3d::ResMdl mdl = gEarthModel->mResMdl;
        u32 count = mdl.GetResMatNumEntries();
        u32 luminous = mdl.GetResMat("luminous_mat").GetID();
        for (u32 i = 0; i < count; i++) {
            nw4r::g3d::ResMat mat = mdl.GetResMat(i);
            if (i != luminous) {
                nw4r::g3d::ResMatTevColor tevColor = mat.GetResMatTevColor();
                GXColor color;
                if (tevColor.GXGetTevKColor(GX_KCOLOR0, &color)) {
                    color.a = (u8)(77.0f * alphaScale);
                    tevColor.GXSetTevKColor(GX_KCOLOR0, color);
                    tevColor.DCStore(false);
                }

                nw4r::g3d::ResTexSrt texSrt = mat.GetResTexSrt();
                texSrt.SetMapMode(1, 1, 1, -1);
                nw4r::g3d::ResTexSrtData& srt = texSrt.ref();
                srt.texSrt[3].Su = waveScale;
                srt.texSrt[3].Sv = waveScale;
                srt.flag &= ~0x2000;

                nw4r::g3d::ResMatIndMtxAndScale ind = mat.GetResMatIndMtxAndScale();
                ind.GXSetIndTexMtx(GX_ITM_0, indMtx, exponent);
                ind.DCStore(false);
            }
        }
    }
}
