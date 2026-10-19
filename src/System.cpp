#include "revolution/CNT/cnt.h"
#include <channel/System.h>
#include <channel/Scene.h>

#include <revolution/BASE.h>
#include <revolution/CNT.h>
#include <revolution/OS.h>
#include <revolution/SC.h>
#include <revolution/VI.h>
#include <new>

// Not yet decompiled (channel)
extern "C" void InitDownloader(void);
extern "C" void UpdateDownloader(void);

// Not yet decompiled (libraries)
extern "C" void VFInit(void);
extern "C" void OSRestart(u32 arg0);
extern "C" u32 VIGetNextField(void);
extern "C" u32 MEMGetTotalFreeSizeForExpHeap(MEMiHeapHead* heap);
extern "C" u32 CXGetUncompressedSize(const void* src);
extern "C" void CXUncompressLZ(const void* src, void* dst);
extern "C" void CXUncompressHuffman(const void* src, void* dst);
extern "C" void KPADGetProjectionPos(Vec2* out, const Vec2* pos, const Vec2* rect, f32 ratio);
extern "C" void KPADEnableDPD(s32 chan);
extern "C" void TPLGetGXTexObjFromPalette(TPLPalette* palette, GXTexObj* texObj, u32 id);

namespace nw4r {
namespace g3d {
void G3dInit(bool enableLockedCache);
}
namespace lyt {
void LytInit();
}
namespace math {
f32 SinFIdx(f32 fidx);
}
}

static inline f32 CalcCursorRate(f32 diff) {
    f32 rate = __fabsf(diff);
    rate = 0.002f * rate;

    if (rate < 0.1f) {
        rate = 0.1f;
    }
    if (rate > 1.0f) {
        rate = 1.0f;
    }
    return rate;
}

static void* AllocWPAD(u32 size);
static BOOL FreeWPAD(void* block);
static void PowerCallback(void);
static void ResetCallback(void);

void SystemInit(void) {
    Mtx texMtx;

    OSInit();
    OSInitFastCast();
    OSSetPowerCallback(PowerCallback);
    OSSetResetCallback(ResetCallback);

    {
        void* arenaHi = OSGetMEM1ArenaHi();
        void* heapStart = OSAllocFromMEM1ArenaHi((u32)OSGetMEM1ArenaHi() - (u32)OSGetMEM1ArenaLo(), 32);

        gMEM1Heap = MEMCreateExpHeapEx(heapStart, (u32)arenaHi - (u32)heapStart, 3);
    }
    MEMInitAllocatorForExpHeap(&gMEM1Allocator, gMEM1Heap, 4);
    MEMInitAllocatorForExpHeap(&gMEM1Allocator32, gMEM1Heap, 32);
    gMEM1FreeSize = 0;

    {
        void* arenaHi = OSGetMEM2ArenaHi();
        void* heapStart = OSAllocFromMEM2ArenaHi((u32)OSGetMEM2ArenaHi() - (u32)OSGetMEM2ArenaLo(), 32);

        gMEM2Heap = MEMCreateExpHeapEx(heapStart, (u32)arenaHi - (u32)heapStart, 3);
    }
    MEMInitAllocatorForExpHeap(&gMEM2Allocator, gMEM2Heap, 4);
    MEMInitAllocatorForExpHeap(&gMEM2Allocator32, gMEM2Heap, 32);
    gMEM2FreeSize = 0;

    WPADRegisterAllocator(AllocWPAD, FreeWPAD);
    KPADInit();

    for (int i = 0; i < WPAD_MAX_CONTROLLERS; i++) {
        KPADEnableDPD(i);
        KPADSetPosParam(i, 0.05f, 1.0f);
        KPADSetDistParam(i, 0.03f, 1.0f);

        gCursorX[i] = GetScreenWidth() / 2;
        gCursorY[i] = 228.0f;
        gKPADLatest[i] = -1;
        gHold[i] = 0;
        gRepeatSlow[i] = FALSE;
        gRepeatFast[i] = FALSE;
        gPointerZoomBase[i] = FALSE;
        gTrig[i] = 0;
        gRelease[i] = 0;
        gHorizon[i].x = 0.0f;
        gHorizon[i].y = 0.0f;
        gHoldFrames[i] = 0;
        gPointerZoomActive[i] = FALSE;
        gPointerWasValid[i] = FALSE;
        gConnected[i] = FALSE;
        gMotorOn[i] = FALSE;
        gMotorTimer[i] = 0;
        gMotorCooldown[i] = 0;
        gMotorPattern[i] = NULL;
        gMotorPatternPos[i] = 0;
    }

    VIInit();
    gGXFifo = MEMAllocFromExpHeapEx(gMEM1Heap, 0x40000, 4);
    gGXFifoObj = GXInit(gGXFifo, 0x40000);

    PSMTXIdentity(texMtx);
    GXLoadTexMtxImm(texMtx, GX_IDENTITY, GX_MTX_3x4);

    gLanguage = GetLanguage();
    if (gLanguage != SCGetLanguage()) {
        SCSetLanguage(gLanguage);
        SCFlushSync();
    }

    switch (GetAreaGroup()) {
    case 0:
        gRegion = 0;
        break;
    case 2:
        gRegion = 2;
        break;
    case 1:
        gRegion = 1;
        break;
    }

    gCountryCode = SCGetSimpleAddressID();
    if (gCountryCode == 0xFFFFFFFF) {
        switch (gRegion) {
        case 0:
            gCountryCode = 0x01000000;
            break;
        case 1:
            gCountryCode = 0x31000000;
            break;
        case 2:
            gCountryCode = 0x4E000000;
            break;
        }
    } else {
        gCountryCode &= 0xFF000000;
    }

    if (VIGetDTVStatus() == 0 && SCGetProgressiveMode() == 1) {
        SetVideoMode(FALSE, SCGetAspectRatio() == 1, FALSE);
    } else {
        SetVideoMode(SCGetProgressiveMode() == 1, SCGetAspectRatio() == 1, FALSE);
    }

    VFInit();
    gUnk80330B40 = 0;
    InitDownloader();
    CNTInit();

    u32 content = 6;
    for (u32 i = 4; i < CONTENT_HANDLE_MAX; i++) {
        contentInitHandleNAND(content, &gContentHandles[i], &gMEM1Allocator32);
        content++;
    }

    gUnk80330B64 = 7;
    nw4r::g3d::G3dInit(true);
    PPCMthid4(PPCMfhid4() & ~0x60000000);
    nw4r::lyt::LytInit();
    gRandSeed = OSGetTime();

    gCaptureTexture = MEMAllocFromExpHeapEx(gMEM2Heap, 0xB4000, 32);
    gFadeType = FADE_NONE;

    gCursor = new (MEMAllocFromExpHeapEx(gMEM1Heap, sizeof(Cursor), 4)) Cursor;
    gCursor->Reset();

    gScene = SCENE_NONE;
    if (gCursor->mEffectReady) {
        ChangeScene(SCENE_WEATHER);
    } else {
        ChangeScene(SCENE_FATAL);
    }

    gFrameCount = 0;
}

static void* AllocWPAD(u32 size) {
    return MEMAllocFromExpHeapEx(gMEM2Heap, size, 32);
}

static BOOL FreeWPAD(void* block) {
    MEMFreeToExpHeap(gMEM2Heap, block);
    return FALSE;
}

void SystemCalc(void) {
    f32 pointerRatio = (f32)gRenderMode.fbWidth / (f32)gRenderMode.viWidth;
    f32 pointerScale = gWidescreen ? 1.1666666f : 1.0f;
    Vec2 screenRect[2];

    screenRect[0].x = 0.0f;
    screenRect[0].y = 0.0f;
    screenRect[1].x = GetScreenWidth();
    screenRect[1].y = 456.0f;

    for (int i = 0; i < WPAD_MAX_CONTROLLERS; i++) {
        u32 prevHold = gHold[i];
        u8 wasConnected = gConnected[i];
        s32 count;
        int j;

        gConnected[i] = FALSE;
        count = KPADRead(i, gKPADStatus[i], KPAD_READ_MAX);
        gKPADReadCount[i] = count;

        if (count > 0) {
            for (j = 0; j < count; j++) {
                if (gKPADStatus[i][j].wpad_err == WPAD_ERR_OK) {
                    gConnected[i] = TRUE;
                    break;
                }
            }
        }

        if (!gConnected[i] && wasConnected) {
            gTrig[i] = 0;
            for (j = 0; j < KPAD_READ_MAX; j++) {
                gKPADStatus[i][j].trig = 0;
            }
            continue;
        }

        gKPADLatest[i] = -1;
        gHold[i] = 0;
        gTrig[i] = 0;
        gRelease[i] = 0;

        gPointerWasValid[i] = FALSE;
        for (j = 0; j < 4; j++) {
            if (gPointerValid[i][j]) {
                gPointerWasValid[i] = TRUE;
                break;
            }
        }

        for (j = 0; j < gKPADReadCount[i]; j++) {
            KPADStatus* status = &gKPADStatus[i][j];

            if (status->wpad_err == WPAD_ERR_OK) {
                Vec2 pos;

                KPADGetProjectionPos(&pos, &status->pos, screenRect, pointerRatio);
                gPointerX[i][j] = pos.x * pointerScale + 0.5f * GetScreenWidth();
                gPointerY[i][j] = pos.y * pointerScale + 0.5f * GetScreenHeight();
                gPointerDist[i][j] = status->dist;
                gPointerValid[i][j] = status->dpd_valid_fg != 0;

                if (gKPADLatest[i] < 0) {
                    gHorizon[i] = status->horizon;
                    gKPADLatest[i] = j;
                    gHold[i] = status->hold;
                    gTrig[i] = status->trig;
                    gRelease[i] = status->release;
                }
            } else {
                gPointerValid[i][j] = FALSE;
            }
        }

        for (j = gKPADReadCount[i]; j < KPAD_READ_MAX; j++) {
            gPointerValid[i][j] = FALSE;
        }

        {
            f32 rate = CalcCursorRate(GetPointerX(i) - gCursorX[i]);
            gCursorX[i] = rate * GetPointerX(i) + (1.0f - rate) * gCursorX[i];

            rate = CalcCursorRate(GetPointerY(i) - gCursorY[i]);
            gCursorY[i] = rate * GetPointerY(i) + (1.0f - rate) * gCursorY[i];
        }

        {
            BOOL found = FALSE;
            BOOL zooming;

            for (j = 0; j < gKPADReadCount[i]; j++) {
                if (gKPADStatus[i][j].dpd_valid_fg == 2) {
                    found = TRUE;
                    break;
                }
            }

            if (!found) {
                for (j = 0; j < gKPADReadCount[i]; j++) {
                    if (gKPADStatus[i][j].dpd_valid_fg == 1) {
                        found = TRUE;
                        break;
                    }
                }
            }

            zooming = FALSE;
            if (found) {
                if (!gPointerZoomActive[i]) {
                    gPointerZoomActive[i] = TRUE;
                    if (!gPointerZoomBase[i]) {
                        gPointerZoomBase[i] = TRUE;
                        gZoomBaseDist[i] = gPointerDist[i][0];
                    } else {
                        zooming = TRUE;
                    }
                } else {
                    zooming = TRUE;
                }
            } else if (gKPADLatest[i] < 0) {
                gPointerZoomBase[i] = FALSE;
            }

            if (zooming) {
                f32 zoom = gZoomBaseDist[i] / gPointerDist[i][0];

                if (zoom < 1.0f) {
                    zoom *= 1.2f + 3.0f * (zoom - 1.0f);
                } else {
                    zoom = 1.2f + 10.0f * (zoom - 1.0f);
                }

                if (zoom < 0.5f) {
                    zoom = 0.5f;
                }
                if (zoom > 5.0f) {
                    zoom = 5.0f;
                }

                if (zoom < 1.0f) {
                    gZoom[i] = 2.0f * (zoom - 1.0f);
                } else {
                    gZoom[i] = 0.25f * (zoom - 1.0f);
                }
            } else {
                gZoom[i] = 0.0f;
            }
        }

        gRepeatSlow[i] = FALSE;
        gRepeatFast[i] = FALSE;

        if (prevHold != 0 && prevHold == gHold[i] && gKPADLatest[i] >= 0) {
            gHoldFrames[i]++;
            if (gHoldFrames[i] > 40) {
                if ((gHoldFrames[i] & 3) == 0) {
                    gRepeatFast[i] = TRUE;
                }
                if (gHoldFrames[i] % 10 == 0) {
                    gRepeatSlow[i] = TRUE;
                }
            }
        } else {
            gHoldFrames[i] = 0;
        }

        gRepeatSlowButtons[i] = gTrig[i] | (gRepeatSlow[i] ? gHold[i] : 0);
        gRepeatFastButtons[i] = gTrig[i] | (gRepeatFast[i] ? gHold[i] : 0);
    }

    gHoldAll = 0;
    gTrigAll = 0;
    gReleaseAll = 0;
    gRepeatSlowAll = 0;
    gRepeatFastAll = 0;

    for (int i = 0; i < WPAD_MAX_CONTROLLERS; i++) {
        if (gKPADLatest[i] >= 0) {
            gHoldAll |= gHold[i];
            gTrigAll |= gTrig[i];
            gReleaseAll |= gRelease[i];
            gRepeatSlowAll |= gRepeatSlowButtons[i];
            gRepeatFastAll |= gRepeatFastButtons[i];
        }
    }

    for (int i = 0; i < WPAD_MAX_CONTROLLERS; i++) {
        u32 rumble = FALSE;

        if (gMotorTimer[i] > 0) {
            rumble = TRUE;
        }

        if (gMotorPattern[i] != NULL) {
            char c = gMotorPattern[i][gMotorPatternPos[i]];
            gMotorPatternPos[i]++;

            if (c == '\0') {
                gMotorPattern[i] = NULL;
                gMotorPatternPos[i] = 0;
            } else if (c == '1') {
                rumble = TRUE;
            }
        }

        if (gMotorOn[i] != rumble && gKPADLatest[i] >= 0) {
            if (rumble) {
                WPADControlMotor(i, WPAD_MOTOR_RUMBLE);
            } else {
                WPADControlMotor(i, WPAD_MOTOR_STOP);
            }
            gMotorOn[i] = rumble;
        }

        if (gMotorTimer[i] > 0) {
            gMotorTimer[i]--;
        } else if (gMotorCooldown[i] > 0) {
            gMotorCooldown[i]--;
        }
    }

    gRandSeed = gRandSeed * 1664525 + 1013904223 + (u32)OSGetTime();

    if (!gFading) {
        gNextScene = gScene;
    }

    if (gFadeType != FADE_NONE && gFadeTimer == 0) {
        gFading = FALSE;

        if (gFadeNextType != FADE_NONE) {
            s32 duration = gFadeDuration;

            if (gFadeNextDuration > 0) {
                duration = gFadeNextDuration;
            }
            StartFade(gFadeNextType, duration, FADE_NONE, 0);
        } else {
            gFadeType = FADE_NONE;
        }
    }

    if (!gFading) {
        switch (gScene) {
        case SCENE_NONE:
            break;
        case SCENE_WEATHER:
            gWeatherScene->Calc();
            break;
        case SCENE_FATAL:
            gFatalScene->Calc();
            break;
        }
    }

    if (gFadeType != FADE_NONE && !gFadeCaptureRequest && gFadeTimer > 0) {
        gFadeTimer--;
    }

    gCursor->Calc();

    if (!gFading && gNextScene != gScene) {
        ChangeScene(gNextScene);
    }

    UpdateDownloader();

    if (gShutdownRequested) {
        VISetBlack(TRUE);
        VIFlush();
        VIWaitForRetrace();

        switch (gScene) {
        case SCENE_WEATHER:
            gWeatherScene->OnShutdown();
            break;
        }

        CNTShutdown();
        OSShutdownSystem();
    }

    gFrameCount++;
}

static inline f32 SinRad(f32 rad) {
    return nw4r::math::SinFIdx(rad * 40.743664f);
}

static inline void LoadFadeMatrices(void) {
    Mtx view;
    Mtx44 proj;

    PSMTXIdentity(view);
    GXLoadPosMtxImm(view, GX_PNMTX0);
    GXSetCurrentMtx(GX_PNMTX0);
    f32 width = GetScreenWidth();
    C_MTXOrtho(proj, 0.0f, 456.0f, 0.0f, width, -100.0f, 100.0f);
    GXSetProjection(proj, GX_ORTHOGRAPHIC);
}

static inline void SetFadeColor(u8 alpha) {
    GXColor fade = {0, 0, 0, 0};
    fade.a = alpha;
    GXSetTevColor(GX_TEVREG1, fade);
}

static inline void DrawFadeBrightness(GXTexObj* texObj, u8 alpha, f32 width, f32 height) {
    GXLoadTexObj(texObj, GX_TEXMAP0);

    GXColor brightness = {alpha, alpha, alpha, 255};
    GXSetTevColor(GX_TEVREG0, brightness);

    GXBegin(GX_QUADS, GX_VTXFMT0, 4);
    GXPosition3f32(0.0f, 0.0f, 0.0f);
    GXTexCoord2f32(0.0f, 0.0f);
    GXPosition3f32(width, 0.0f, 0.0f);
    GXTexCoord2f32(1.0f, 0.0f);
    GXPosition3f32(width, height, 0.0f);
    GXTexCoord2f32(1.0f, 1.0f);
    GXPosition3f32(0.0f, height, 0.0f);
    GXTexCoord2f32(0.0f, 1.0f);
    GXEnd();
}

static inline void SetZoomColor(u8 alpha) {
    GXColor brightness = {alpha, alpha, alpha, 255};
    GXSetTevColor(GX_TEVREG0, brightness);
}

void SystemDraw(void) {
    if (gRenderMode.field_rendering) {
        GXSetViewportJitter(0.0f, 0.0f, gRenderMode.fbWidth, gRenderMode.efbHeight, 0.0f, 1.0f,
                            VIGetNextField());
    } else {
        GXSetViewport(0.0f, 0.0f, gRenderMode.fbWidth, gRenderMode.efbHeight, 0.0f, 1.0f);
    }

    GXInvalidateVtxCache();
    GXInvalidateTexAll();

    if (!gFading || gFadeCaptured || gFadeCaptureRequest) {
        switch (gScene) {
        case SCENE_NONE:
            break;
        case SCENE_WEATHER:
            gWeatherScene->Draw();
            break;
        case SCENE_FATAL:
            gFatalScene->Draw();
            break;
        }
    }

    gCursor->Draw();

    if (!gFading || gFadeCaptured || gFadeCaptureRequest) {
        switch (gScene) {
        case SCENE_WEATHER:
            gWeatherScene->DrawOverlay();
            break;
        }
    }

    if (gFadeType != FADE_NONE) {
        f32 progress = SinRad(1.5708f * gFadeTimer / gFadeDuration);
        u8 alpha = 255.0f * progress;
        f32 width = GetScreenWidth();
        f32 height = 456.0f;
        GXTexObj texObj;

        if (gFadeCaptured || gFadeCaptureRequest) {
            GXDrawDone();
            GXSetTexCopySrc(0, 0, gRenderMode.fbWidth, gRenderMode.efbHeight);
            GXSetTexCopyDst(gRenderMode.fbWidth, gRenderMode.efbHeight, GX_TF_RGB565, GX_FALSE);
            GXCopyTex(gCaptureTexture, GX_TRUE);
            GXPixModeSync();

            if (gFadeCaptureRequest) {
                gFadeCaptureRequest = FALSE;
            }
        }

        LoadFadeMatrices();

        SetDefaultGXState();
        GXSetZMode(GX_FALSE, GX_LEQUAL, GX_FALSE);
        GXInitTexObj(&texObj, gCaptureTexture, gRenderMode.fbWidth, gRenderMode.efbHeight,
                     GX_TF_RGB565, GX_CLAMP, GX_CLAMP, GX_FALSE);

        switch (gFadeType) {
        case FADE_TO_BLACK: {
            GXColor clear = {0, 0, 0, 0};
            GXSetTevColor(GX_TEVREG0, clear);

            SetFadeColor(alpha);

            GXBegin(GX_QUADS, GX_VTXFMT0, 4);
            GXPosition3f32(0.0f, 0.0f, 0.0f);
            GXTexCoord2f32(0.0f, 0.0f);
            GXPosition3f32(width, 0.0f, 0.0f);
            GXTexCoord2f32(1.0f, 0.0f);
            GXPosition3f32(width, height, 0.0f);
            GXTexCoord2f32(1.0f, 1.0f);
            GXPosition3f32(0.0f, height, 0.0f);
            GXTexCoord2f32(0.0f, 1.0f);
            GXEnd();
            break;
        }
        case FADE_CAPTURE_BRIGHTNESS:
            DrawFadeBrightness(&texObj, alpha, width, height);
            break;
        case FADE_ZOOM_IN:
        case FADE_ZOOM_OUT:
            {
            if (gFadeType == FADE_ZOOM_IN) {
                alpha = 255 - alpha;
            }
            if (gFadeType != FADE_ZOOM_IN) {
                progress = 1.0f - progress;
            }
            progress *= 0.1f;
            GXLoadTexObj(&texObj, GX_TEXMAP0);
            SetZoomColor(alpha);
            GXBegin(GX_QUADS, GX_VTXFMT0, 4);
            GXPosition3f32(0.0f, 0.0f, 0.0f);
            GXTexCoord2f32(progress, progress);
            GXPosition3f32(width, 0.0f, 0.0f);
            GXTexCoord2f32(1.0f - progress, progress);
            GXPosition3f32(width, height, 0.0f);
            GXTexCoord2f32(1.0f - progress, 1.0f - progress);
            GXPosition3f32(0.0f, height, 0.0f);
            GXTexCoord2f32(progress, 1.0f - progress);
            GXEnd();
            break;
        }
        }
    }

    GXSetZMode(GX_TRUE, GX_LEQUAL, GX_TRUE);
    GXSetColorUpdate(GX_TRUE);
    GXCopyDisp(gCurrentXfb, GX_TRUE);
    GXDrawDone();
    VISetNextFrameBuffer(gCurrentXfb);

    if (gUnblackNextFrame) {
        VISetBlack(FALSE);
        gUnblackNextFrame = FALSE;
    }

    VIFlush();
    VIWaitForRetrace();

    gCurrentXfb = gCurrentXfb == gXfb1 ? gXfb2 : gXfb1;
}

void ChangeScene(u32 scene) {
    BOOL leaked;
    u32 freeSize;

    gTrigAll = 0;
    gHoldAll = 0;
    gReleaseAll = 0;
    gRepeatSlowAll = 0;
    gRepeatFastAll = 0;

    for (int i = 0; i < WPAD_MAX_CONTROLLERS; i++) {
        gHold[i] = 0;
        gTrig[i] = 0;
        gRelease[i] = 0;

        for (int j = 0; j < KPAD_READ_MAX; j++) {
            gKPADStatus[i][j].hold = 0;
            gKPADStatus[i][j].trig = 0;
            gKPADStatus[i][j].release = 0;
        }
    }

    switch (gScene) {
    case SCENE_NONE:
        break;
    case SCENE_WEATHER:
        delete gWeatherScene;
        gWeatherScene = NULL;
        break;
    case SCENE_FATAL:
        delete gFatalScene;
        gFatalScene = NULL;
        break;
    }

    leaked = FALSE;
    freeSize = MEMGetTotalFreeSizeForExpHeap(gMEM1Heap);
    if (gMEM1FreeSize != freeSize && gMEM1FreeSize != 0) {
        leaked = TRUE;
    }
    freeSize = MEMGetTotalFreeSizeForExpHeap(gMEM2Heap);
    if (gMEM2FreeSize != freeSize && gMEM2FreeSize != 0) {
        leaked = TRUE;
    }

    if (scene != SCENE_FATAL && leaked) {
        OSPanic("System.cpp", 1325, "!!! MEMORY LEAK FOUND !!!\n");
    }

    gScene = scene;
    gMEM1FreeSize = MEMGetTotalFreeSizeForExpHeap(gMEM1Heap);
    gMEM2FreeSize = MEMGetTotalFreeSizeForExpHeap(gMEM2Heap);

    switch (gScene) {
    case SCENE_NONE:
        break;
    case SCENE_WEATHER:
        gWeatherScene =
            new (MEMAllocFromExpHeapEx(gMEM1Heap, sizeof(WeatherScene), 4)) WeatherScene;
        gWeatherScene->Init();
        break;
    case SCENE_FATAL:
        gFatalScene = new (MEMAllocFromExpHeapEx(gMEM1Heap, sizeof(FatalScene), 4)) FatalScene;
        gFatalScene->Init();
        break;
    }
}

void SetDefaultGXState(void) {
    GXClearVtxDesc();
    GXSetVtxDesc(GX_VA_POS, GX_DIRECT);
    GXSetVtxDesc(GX_VA_TEX0, GX_DIRECT);
    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);

    GXSetChanCtrl(GX_COLOR0A0, GX_FALSE, GX_SRC_REG, GX_SRC_REG, GX_LIGHT_NULL, GX_DF_NONE,
                  GX_AF_NONE);
    GXSetNumChans(1);
    GXSetTexCoordGen2(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY, GX_FALSE,
                      GX_PTIDENTITY);
    GXSetNumTexGens(1);

    GXColor white = {255, 255, 255, 255};
    GXSetTevColor(GX_TEVREG0, white);
    GXColor clear = {0, 0, 0, 0};
    GXSetTevColor(GX_TEVREG1, clear);

    GXSetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR_NULL);
    GXSetTevColorIn(GX_TEVSTAGE0, GX_CC_ZERO, GX_CC_TEXC, GX_CC_C0, GX_CC_C1);
    GXSetTevAlphaIn(GX_TEVSTAGE0, GX_CA_ZERO, GX_CA_TEXA, GX_CA_A0, GX_CA_A1);
    GXSetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
    GXSetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
    GXSetTevSwapMode(GX_TEVSTAGE0, GX_TEV_SWAP0, GX_TEV_SWAP0);
    GXSetTevDirect(GX_TEVSTAGE0);
    GXSetNumTevStages(1);
    GXSetNumIndStages(0);
    GXSetTevSwapModeTable(GX_TEV_SWAP0, GX_CH_RED, GX_CH_GREEN, GX_CH_BLUE, GX_CH_ALPHA);

    GXSetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_SET);
    GXSetZMode(GX_TRUE, GX_LEQUAL, GX_TRUE);
    GXSetZCompLoc(GX_FALSE);
    GXSetAlphaCompare(GX_GREATER, 0, GX_AOP_AND, GX_ALWAYS, 0);
    GXSetColorUpdate(GX_TRUE);
    GXSetAlphaUpdate(GX_FALSE);
    GXSetCullMode(GX_CULL_NONE);
    GXSetClipMode(GX_CLIP_ENABLE);

    GXSetScissor(0, 0, gRenderMode.fbWidth, gRenderMode.efbHeight);
    GXSetViewport(0.0f, 0.0f, (s32)gRenderMode.fbWidth, (s32)gRenderMode.efbHeight, 0.0f, 1.0f);
}

void SetOrthoProjection(void) {
    Mtx view;
    Mtx44 proj;

    PSMTXIdentity(view);
    GXLoadPosMtxImm(view, GX_PNMTX0);
    GXSetCurrentMtx(GX_PNMTX0);
    f32 width = GetScreenWidth();
    C_MTXOrtho(proj, 0.0f, 456.0f, 0.0f, width, -100.0f, 100.0f);
    GXSetProjection(proj, GX_ORTHOGRAPHIC);
}

void MakeTransformMtx(const Vec* scale, const Vec2* rotation, const Vec* trans, Mtx out) {
    Mtx mtx;

    PSMTXIdentity(mtx);
    mtx[0][0] = rotation->x * scale->x;
    mtx[0][1] = -rotation->y * scale->y;
    mtx[1][0] = rotation->y * scale->x;
    mtx[1][1] = rotation->x * scale->y;
    mtx[2][2] = scale->z;
    PSMTXTransApply(mtx, out, trans->x, trans->y, trans->z);
}

static const char* sLanguageCodes[] = {"JP", "US", "GE", "FR", "SP", "IT", "DU"};

const char* GetLanguageCode(void) {
    return sLanguageCodes[gLanguage];
}

void* LoadContentFile(u32 content, const char* path, s32 align, u32* sizeOut, MEMiHeapHead* heap) {
    u32 length;
    void* result;
    u32 size;
    void* buf;
    s32 read;
    CNTFileInfoNAND file;

    result = NULL;
    size = 0;

    if (contentOpenNAND(&gContentHandles[content], path, &file) == 0) {
        length = (contentGetLengthNAND(&file) + 31) & ~31;
        buf = MEMAllocFromExpHeapEx(heap, length, align);

        if (buf != NULL) {
            read = contentReadNAND(&file, buf, length, 0);

            contentCloseNAND(&file);
            if (read == 0) {
                MEMFreeToExpHeap(heap, buf);
            } else {
                result = buf;
                size = length;
            }
        }
    }

    if (sizeOut != NULL) {
        *sizeOut = size;
    }

    return result;
}

void* LoadCompressedContentFile(u32 content, const char* path, s32 align, u32* sizeOut,
                                MEMiHeapHead* heap) {
    void* result;
    void* compressed;
    void* buf;
    u32 size;

    result = NULL;
    size = 0;
    compressed = LoadContentFile(content, path, -align, NULL, heap);

    if (compressed != NULL) {
        size = CXGetUncompressedSize(compressed);
        buf = MEMAllocFromExpHeapEx(heap, size, align);

        if (buf != NULL) {
            switch (*(u8*)compressed & 0xF0) {
            case 0x10:
                CXUncompressLZ(compressed, buf);
                break;
            case 0x20:
                CXUncompressHuffman(compressed, buf);
                break;
            default:
                OSPanic("System.cpp", 2247, "CXCompressionType %d unsupported.");
                break;
            }

            MEMFreeToExpHeap(heap, compressed);
            result = buf;
        }
    }

    if (sizeOut != NULL) {
        *sizeOut = size;
    }

    return result;
}

void DrawLine(const Vec* start, const Vec* end, u8 width, const GXColor& startColor, const GXColor& endColor) {
    GXSetLineWidth(width, GX_TO_ZERO);

    GXClearVtxDesc();
    GXSetVtxDesc(GX_VA_POS, GX_DIRECT);
    GXSetVtxDesc(GX_VA_CLR0, GX_DIRECT);
    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);

    GXSetChanCtrl(GX_COLOR0A0, GX_FALSE, GX_SRC_VTX, GX_SRC_VTX, GX_LIGHT_NULL, GX_DF_NONE,
                  GX_AF_NONE);
    GXSetNumChans(1);
    GXSetNumTexGens(0);

    GXSetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD_NULL, GX_TEXMAP_NULL, GX_COLOR0A0);
    GXSetTevColorIn(GX_TEVSTAGE0, GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO, GX_CC_RASC);
    GXSetTevAlphaIn(GX_TEVSTAGE0, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_RASA);
    GXSetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
    GXSetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
    GXSetTevSwapMode(GX_TEVSTAGE0, GX_TEV_SWAP0, GX_TEV_SWAP0);
    GXSetTevDirect(GX_TEVSTAGE0);
    GXSetNumTevStages(1);
    GXSetNumIndStages(0);

    GXBegin(GX_LINES, GX_VTXFMT0, 2);
    GXPosition3f32(start->x, start->y, start->z);
    GXColor1u32(*(const u32*)&startColor);
    GXPosition3f32(end->x, end->y, end->z);
    GXColor1u32(*(const u32*)&endColor);
    GXEnd();
}

void SetVideoMode(BOOL progressive, BOOL widescreen, BOOL narrow) {
    GXRenderModeObj rmode;
    BOOL narrowFb;
    u16 viWidthMax;
    u16 viHeightMax;

    gProgressive = progressive;
    gWidescreen = widescreen;

    narrowFb = FALSE;
    if (narrow && !widescreen) {
        narrowFb = TRUE;
    }

    rmode.fbWidth = narrowFb ? 608 : 640;
    rmode.efbHeight = SCREEN_HEIGHT;

    switch (VIGetTvFormat()) {
    case VI_TVFORMAT_NTSC:
    default:
        rmode.tvInfo = progressive ? VI_TVMODE_NTSC_PROG : VI_TVMODE_NTSC_INT;
        viWidthMax = 720;
        viHeightMax = 480;
        break;
    case VI_TVFORMAT_PAL:
    case VI_TVFORMAT_EURGB60:
        if (progressive || SCGetEuRgb60Mode() == 1) {
            rmode.tvInfo = progressive ? VI_TVMODE_EURGB60_PROG : VI_TVMODE_EURGB60_INT;
            viWidthMax = 720;
            viHeightMax = 480;
        } else {
            rmode.tvInfo = VI_TVMODE_PAL_INT;
            viWidthMax = 720;
            viHeightMax = 574;
        }
        break;
    }

    if ((VITVMode)rmode.tvInfo == VI_TVMODE_PAL_INT) {
        if (narrow) {
            rmode.xfbHeight = 542;
            rmode.viWidth = 640;
            rmode.viHeight = 542;
        } else {
            rmode.xfbHeight = 542;
            rmode.viWidth = widescreen ? 682 : 666;
            rmode.viHeight = rmode.xfbHeight;
        }
    } else {
        if (narrow) {
            rmode.xfbHeight = SCREEN_HEIGHT;
            rmode.viWidth = 640;
        } else {
            rmode.xfbHeight = SCREEN_HEIGHT;
            rmode.viWidth = widescreen ? 686 : 670;
        }
        rmode.viHeight = rmode.xfbHeight;
    }

    rmode.viXOrigin = (viWidthMax - rmode.viWidth) / 2;
    rmode.viYOrigin = (viHeightMax - rmode.viHeight) / 2;

    if (progressive) {
        rmode.xfbMode = VI_XFBMODE_SF;
        rmode.vfilter[0] = 0;
        rmode.vfilter[1] = 0;
        rmode.vfilter[2] = 21;
        rmode.vfilter[3] = 22;
        rmode.vfilter[4] = 21;
        rmode.vfilter[5] = 0;
        rmode.vfilter[6] = 0;
    } else {
        rmode.xfbMode = VI_XFBMODE_DF;
        rmode.vfilter[0] = 8;
        rmode.vfilter[1] = 8;
        rmode.vfilter[2] = 10;
        rmode.vfilter[3] = 12;
        rmode.vfilter[4] = 10;
        rmode.vfilter[5] = 8;
        rmode.vfilter[6] = 8;
    }

    rmode.field_rendering = GX_FALSE;
    rmode.aa = GX_FALSE;
    for (int i = 0; i < 12; i++) {
        rmode.sample_pattern[i][0] = 6;
    }
    for (int i = 0; i < 12; i++) {
        rmode.sample_pattern[i][1] = 6;
    }

    SetRenderMode(&rmode);
}

void GetTexObj(TPLPalette* palette, u32 id, GXTexObj* texObj) {
    TPLGetGXTexObjFromPalette(palette, texObj, id);
}

u32 GetTexWidth(TPLPalette* palette, u32 id) {
    return TPLGet(palette, id)->textureHeader->width;
}

u32 GetTexHeight(TPLPalette* palette, u32 id) {
    return TPLGet(palette, id)->textureHeader->height;
}

void DrawTextureAt(TPLPalette* palette, u32 id, f32 scaleX, f32 scaleY, const Vec* pos) {
    GXTexObj texObj;
    f32 x0, y0, x1, y1;

    TPLGetGXTexObjFromPalette(palette, &texObj, id);
    GXLoadTexObj(&texObj, GX_TEXMAP0);

    x0 = pos->x;
    y0 = pos->y;
    x1 = pos->x + scaleX * TPLGet(palette, id)->textureHeader->width;
    y1 = pos->y + scaleY * TPLGet(palette, id)->textureHeader->height;

    GXBegin(GX_QUADS, GX_VTXFMT0, 4);
    GXPosition3f32(x0, y0, pos->z);
    GXTexCoord2f32(0.0f, 0.0f);
    GXPosition3f32(x1, y0, pos->z);
    GXTexCoord2f32(1.0f, 0.0f);
    GXPosition3f32(x1, y1, pos->z);
    GXTexCoord2f32(1.0f, 1.0f);
    GXPosition3f32(x0, y1, pos->z);
    GXTexCoord2f32(0.0f, 1.0f);
    GXEnd();
}

static inline BOOL IsEnteringProgressive(u32 current, u32 next) {
    return current != VI_SCANMODE_PROG && next == VI_SCANMODE_PROG;
}

static inline BOOL IsLeavingProgressive(u32 current, u32 next) {
    return current == VI_SCANMODE_PROG && next != VI_SCANMODE_PROG;
}

void SetRenderMode(GXRenderModeObj* rmode) {
    u32 current = VIGetScanMode();
    u32 next = VI_TVMODE_SCANMODE(rmode->tvInfo);
    BOOL modeChanged = TRUE;
    u16 xfbLines;

    if (!IsEnteringProgressive(current, next) && !IsLeavingProgressive(current, next)) {
        modeChanged = FALSE;
    }

    gRenderMode = *rmode;

    if (gXfb1 != NULL) {
        MEMFreeToExpHeap(gMEM2Heap, gXfb1);
        MEMFreeToExpHeap(gMEM2Heap, gXfb2);
        gUnblackNextFrame = TRUE;
    }

    gXfbSize = ((u16)(gRenderMode.fbWidth + 15) & ~15) * gRenderMode.xfbHeight * 2;

    gXfb1 = MEMAllocFromExpHeapEx(gMEM2Heap, gXfbSize, -32);
    if (gXfb1 == NULL) {
        OSPanic("System.cpp", 2735, "1");
    }
    DCInvalidateRange(gXfb1, gXfbSize);

    gXfb2 = MEMAllocFromExpHeapEx(gMEM2Heap, gXfbSize, -32);
    if (gXfb2 == NULL) {
        OSPanic("System.cpp", 2741, "2");
    }
    DCInvalidateRange(gXfb2, gXfbSize);

    gCurrentXfb = gXfb2;
    GXSetViewport(0.0f, 0.0f, gRenderMode.fbWidth, gRenderMode.efbHeight, 0.0f, 1.0f);
    GXSetScissor(0, 0, gRenderMode.fbWidth, gRenderMode.efbHeight);

    xfbLines = GXSetDispCopyYScale(GXGetYScaleFactor(gRenderMode.efbHeight, gRenderMode.xfbHeight));
    GXSetDispCopySrc(0, 0, gRenderMode.fbWidth, gRenderMode.efbHeight);
    GXSetDispCopyDst(gRenderMode.fbWidth, xfbLines);
    GXSetCopyFilter(gRenderMode.aa, gRenderMode.sample_pattern, GX_TRUE, gRenderMode.vfilter);
    GXSetDispCopyGamma(GX_GM_1_0);

    if (gRenderMode.aa) {
        GXSetPixelFmt(GX_PF_RGBA565_Z16, GX_ZC_LINEAR);
    } else {
        GXSetPixelFmt(GX_PF_RGB8_Z24, GX_ZC_LINEAR);
    }

    GXCopyDisp(gXfb1, GX_TRUE);
    GXCopyDisp(gXfb1, GX_FALSE);
    GXCopyDisp(gXfb2, GX_FALSE);
    GXDrawDone();

    if (modeChanged) {
        VISetBlack(TRUE);
    }

    VIConfigure(&gRenderMode);
    VISetNextFrameBuffer(gXfb1);
    gCurrentXfb = gXfb2;
    VIFlush();
    VIWaitForRetrace();
    VIWaitForRetrace();

    if (modeChanged) {
        for (int i = 0; i < 98; i++) {
            VIWaitForRetrace();
        }
        gUnblackNextFrame = TRUE;
    }
}

static inline f32 GetScissorScale(void) {
    s32 fbWidth = gRenderMode.fbWidth;
    return (f32)GetScreenWidth() / fbWidth;
}

void SetScaledScissor(u32 left, u32 top, u32 width, u32 height) {
    u32 scaledLeft = left / GetScissorScale();
    u32 scaledWidth = width / GetScissorScale();

    GXSetScissor(scaledLeft, top, scaledWidth, height);
}

void StartFade(s32 type, s32 duration, s32 nextType, s32 nextDuration) {
    gFadeType = type;
    gFadeTimer = duration;
    gFadeDuration = duration;
    gFadeNextType = nextType;
    gFadeNextDuration = nextDuration;

    switch (type) {
    case FADE_CAPTURE_BRIGHTNESS:
    case FADE_ZOOM_OUT:
        gFadeCaptured = FALSE;
        gFadeCaptureRequest = TRUE;
        gFading = TRUE;
        break;
    case FADE_ZOOM_IN:
        gFadeCaptured = TRUE;
        gFadeCaptureRequest = FALSE;
        gFading = FALSE;
        break;
    case FADE_TO_BLACK:
    default:
        gFadeCaptured = FALSE;
        gFadeCaptureRequest = FALSE;
        gFading = FALSE;
        break;
    }
}

void SetCursor(s32 chan, s32 type) {
    gCursor->Set(chan, type);
}

static inline BOOL IsControllerActive(s32 chan) {
    return gPointerWasValid[chan] && gKPADLatest[chan] >= 0;
}

void StartRumble(s32 chan, s32 frames, s32 cooldown) {
    if (WPADIsMotorEnabled()) {
        if (IsControllerActive(chan) && gMotorCooldown[chan] == 0) {
            gMotorCooldown[chan] = cooldown;
            gMotorTimer[chan] = frames;
        }
    }
}

void StopRumble(s32 chan, s32 cooldown) {
    for (int i = 0; i < WPAD_MAX_CONTROLLERS; i++) {
        if (chan < 0 || i == chan) {
            WPADControlMotor(i, WPAD_MOTOR_STOP);
            gMotorOn[i] = FALSE;
            gMotorTimer[i] = 0;
            if (gMotorCooldown[i] < cooldown) {
                gMotorCooldown[i] = cooldown;
            }
        }
    }
}

static void PowerCallback(void) {
    gShutdownRequested = TRUE;

    switch (gScene) {
    case SCENE_WEATHER:
        if (gWeatherScene != NULL) {
            gWeatherScene->OnPowerButton();
        }
        break;
    }
}

static void ResetCallback(void) {
    switch (gScene) {
    case SCENE_WEATHER:
        if (gWeatherScene != NULL) {
            gWeatherScene->OnResetButton();
        }
        break;
    }
}

void ReturnToMenu(void) {
    VISetBlack(TRUE);
    VIFlush();
    VIWaitForRetrace();
    OSReturnToMenu();
}

void Restart(void) {
    VISetBlack(TRUE);
    VIFlush();
    VIWaitForRetrace();
    OSRestart(0);
}

void* MEM1Alloc(u32 size, s32 align) {
    if (align == 0) {
        return MEMAllocFromExpHeapEx(gMEM1Heap, size, 4);
    }
    return MEMAllocFromExpHeapEx(gMEM1Heap, size, align);
}

void* MEM2Alloc(u32 size, s32 align) {
    if (align == 0) {
        return MEMAllocFromExpHeapEx(gMEM2Heap, size, 4);
    }
    return MEMAllocFromExpHeapEx(gMEM2Heap, size, align);
}

void MEM1Free(void* block) {
    MEMFreeToExpHeap(gMEM1Heap, block);
}

void MEM2Free(void* block) {
    MEMFreeToExpHeap(gMEM2Heap, block);
}

void* operator new(size_t size) {
    return MEMAllocFromExpHeapEx(gMEM1Heap, size, 4);
}

void* operator new(size_t size, s32 align) {
    return MEMAllocFromExpHeapEx(gMEM1Heap, size, align);
}

void* operator new[](size_t size) {
    return MEMAllocFromExpHeapEx(gMEM1Heap, size, 4);
}

void operator delete(void* block) {
    MEMFreeToExpHeap(gMEM1Heap, block);
}

void operator delete[](void* block) {
    MEMFreeToExpHeap(gMEM1Heap, block);
}

MEMiHeapHead* gMEM1Heap;
u32 gMEM1FreeSize;
MEMiHeapHead* gMEM2Heap;
u32 gMEM2FreeSize;
void* gGXFifo;
GXFifoObj* gGXFifoObj;
u32 gXfbSize;
void* gXfb1;
void* gXfb2;
void* gCurrentXfb;
s32 gScene;
s32 gNextScene;
u8 gPointerZoomBase[WPAD_MAX_CONTROLLERS];
u8 gPointerZoomActive[WPAD_MAX_CONTROLLERS];
u8 gPointerWasValid[WPAD_MAX_CONTROLLERS];
u8 gConnected[WPAD_MAX_CONTROLLERS];
u8 gRepeatSlow[WPAD_MAX_CONTROLLERS];
u8 gRepeatFast[WPAD_MAX_CONTROLLERS];
u32 gHoldAll;
u32 gTrigAll;
u32 gReleaseAll;
u32 gRepeatSlowAll;
u32 gRepeatFastAll;
u8 gMotorOn[WPAD_MAX_CONTROLLERS];
u32 gRandSeed;
u8 gWidescreen;
u8 gProgressive;
u8 gLanguage;
u32 gCountryCode;
s32 gRegion;
u8 gUnk80330B40;
void* gCaptureTexture;
s32 gFadeType;
s32 gFadeTimer;
s32 gFadeDuration;
s32 gFadeNextType;
s32 gFadeNextDuration;
u8 gFadeCaptured;
u8 gFadeCaptureRequest;
u8 gFading;
u8 gShutdownRequested;
u32 gFrameCount;
s32 gUnk80330B64;
Cursor* gCursor;
WeatherScene* gWeatherScene;
FatalScene* gFatalScene;
u32 gUnk80330B74;

MEMAllocator gMEM1Allocator;
MEMAllocator gMEM1Allocator32;
MEMAllocator gMEM2Allocator;
MEMAllocator gMEM2Allocator32;
GXRenderModeObj gRenderMode;
s32 gKPADLatest[WPAD_MAX_CONTROLLERS];
KPADStatus gKPADStatus[WPAD_MAX_CONTROLLERS][KPAD_READ_MAX];
s32 gKPADReadCount[WPAD_MAX_CONTROLLERS];
f32 gPointerX[WPAD_MAX_CONTROLLERS][KPAD_READ_MAX];
f32 gPointerY[WPAD_MAX_CONTROLLERS][KPAD_READ_MAX];
f32 gPointerDist[WPAD_MAX_CONTROLLERS][KPAD_READ_MAX];
f32 gCursorX[WPAD_MAX_CONTROLLERS];
f32 gCursorY[WPAD_MAX_CONTROLLERS];
f32 gZoom[WPAD_MAX_CONTROLLERS];
f32 gZoomBaseDist[WPAD_MAX_CONTROLLERS];
u8 gPointerValid[WPAD_MAX_CONTROLLERS][KPAD_READ_MAX];
u32 gHold[WPAD_MAX_CONTROLLERS];
u32 gTrig[WPAD_MAX_CONTROLLERS];
u32 gRelease[WPAD_MAX_CONTROLLERS];
Vec2 gHorizon[WPAD_MAX_CONTROLLERS];
u32 gRepeatSlowButtons[WPAD_MAX_CONTROLLERS];
u32 gRepeatFastButtons[WPAD_MAX_CONTROLLERS];
s32 gHoldFrames[WPAD_MAX_CONTROLLERS];
s32 gMotorTimer[WPAD_MAX_CONTROLLERS];
s32 gMotorCooldown[WPAD_MAX_CONTROLLERS];
const char* gMotorPattern[WPAD_MAX_CONTROLLERS];
s32 gMotorPatternPos[WPAD_MAX_CONTROLLERS];
CNTHandleNAND gContentHandles[CONTENT_HANDLE_MAX];

u8 gUnblackNextFrame = TRUE;
