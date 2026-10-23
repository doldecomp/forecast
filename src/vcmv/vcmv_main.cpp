#include <types.h>
#include <revolution/GX.h>
#include <revolution/KPAD.h>
#include <revolution/MEM.h>
#include <revolution/OS.h>
#include <revolution/VI.h>
#include <revolution/WPAD.h>
#include <cstring>
#include "vcmv.h"
#include "www.h"
#include "vcmv_www.h"

static s32 sRenderModeId;
static bool sIsBrowserActive;
static bool sNeedsInitialReload;
volatile bool sExitRequested = true;
bool sManualActive = true;
volatile bool sTriggerRumble;
bool wwwStarted;
bool wwwLibLoaded;
u8 sInitDelayCounter;
s32 sBrowserHandle;
MEMAllocator* wwwMem1Allocator;
MEMAllocator* wwwMem2Allocator;
static Callback_8025D2D0 sExitCallback;
u8 sManualActiveCursor;
volatile s32 sManualFrameCount;
bool sMotorActive;
s32 sMotorStartTime;
static s32 sScrollRepeatFrame;
s32 sScrollDragFrame;
static s32 sScrollDragY;
static bool sIsScrollDragging;
static u8 sRequestReload;
static u8 sRequestPrevPage;
static s32 sLastMouseEvent;
static s32 sLastMouseX;
s32 sLastMouseY;
static BOOL sIsMouseDown;
static s32 sLastMouseButton;

HBMControllerData sControllers;
VCMVControllerState sManualCursorArray[4];
static KPADStatus sKpad[4][16];
#define sManualCursors ((VCMV_VOLATILE VCMVControllerState*)sManualCursorArray)
u8 sManualThread[0x320];

bool sManualInitialized = true;

static inline void vcmvFreeBlock(void* block) {
    if (!((uintptr_t)block & 0x30000000)) {
        MEMFreeToAllocator(wwwMem1Allocator, block);
    } else {
        MEMFreeToAllocator(wwwMem2Allocator, block);
    }
}

static inline bool vcmvTryAllocBlock(void** param1, u32 size, MEMAllocator* allocator1, MEMAllocator* allocator2) {
    if (*param1 == NULL) {
        *param1 = MEMAllocFromAllocator(allocator1, size);

        if (*param1 == NULL) {
            *param1 = MEMAllocFromAllocator(allocator2, size);
        }

        if (*param1 == NULL) {
            return false;
        }
    }

    return true;
}

static inline bool vcmvAllocBlock(void** param1, u32 size, MEMAllocator* allocator1, MEMAllocator* allocator2) {
    if (!vcmvTryAllocBlock(param1, size, allocator1, allocator2)) {
        OSReport("AllocIfNecessary size=%p failed\n ", size);
        return false;
    }

    return true;
}

static inline f32 vcmvInterpolateSpline(f32 param1, f32 param2, UNKWORD param3, UNKWORD param4, f32 param5) {
    if (param3 >= param4) {
        return param2;
    }

    f32 temp = (param2 - param1) * 3.0f / param5;
    return param1 + (temp * param3 * param3 * (param4 - param3 * 0.6666667f));
}

void vcmvInitControllerState(VCMV_VOLATILE VCMVControllerState* param1) {
    s32 var_r6;
    s32 var_r5;

    // fixes .sdata2 ordering
    (void)3.0f;
    (void)0.6666667f;
    (void)4503601774854144.0;

    var_r6 = sManualFrameCount - param1->holdStartFrame;
    var_r5 = sManualFrameCount - param1->releaseStartFrame;

    if (param1->trig & 0x02) {
        param1->holdStartFrame = sManualFrameCount;

        if (var_r5 < 7) {
            param1->holdStartFrame -= 7 - var_r5;
        }

        var_r6 = sManualFrameCount - param1->holdStartFrame;
    } else if (param1->release & 2) {
        param1->releaseStartFrame = sManualFrameCount;

        if (var_r6 < 7) {
            param1->releaseStartFrame -= 7 - var_r6;
        }

        var_r5 = sManualFrameCount - param1->releaseStartFrame;
    }

    param1->cursorY = param1->posY;

    if (param1->hold & 0x02) {
        UNKWORD unused1 = param1->holdStartFrame;
        UNKWORD unused2 = sManualFrameCount;
        param1->cursorX = vcmvInterpolateSpline(param1->posX, sLeftMargin, var_r6, 7, 343.0f);
    } else {
        param1->cursorX = vcmvInterpolateSpline(sLeftMargin, param1->posX, var_r5, 7, 343.0f);
    }
}

void vcmvProcessDpadInput(VCMV_VOLATILE VCMVControllerState* param1, KPADStatus* param2) {
    param1->posX = sWidescreenScale * (param2->pos.x * sHalfScreenWidth);
    param1->posY = param2->pos.y * sHalfScreenHeight;
    param1->screenX = param1->posX + sHalfScreenWidth;
    param1->screenY = param1->posY + sHalfScreenHeight;
    param1->cosAngle = param2->horizon.x;
    param1->sinAngle = param2->horizon.y;

    param1->hold = 0x00;

    if (param2->hold & 0x800) {
        param1->hold |= 0x01;
    }

    if (param2->hold & 0x08) {
        param1->hold |= 0x08;
    }

    if (param2->hold & 0x04) {
        param1->hold |= 0x04;
    }

    if (param2->hold & 0x1000) {
        param1->hold |= 0x10;
    }

    if (param2->hold & 0x10) {
        param1->hold |= 0x20;
    }

    if (param2->hold & 0x8000) {
        param1->hold |= 0x40;
    }
}

#pragma push
#pragma gen_fsel on // it won't generate the fsel otherwise
void vcmvProcessStickInput(VCMV_VOLATILE VCMVControllerState* param1, KPADStatus* param2) {
    f32 temp_f0;
    f32 temp_f2;
    f32 temp_f2_2;
    f32 var_f5;

    temp_f2 = param1->stickX * param2->ex_status.cl.lstick.x + param1->stickY * -param2->ex_status.cl.lstick.y;

    var_f5 = 1.0f;
    if (temp_f2 > 0.0f) {
        var_f5 += 0.02f * temp_f2 + var_f5;
        var_f5 = 2.5f - var_f5 >= 0.0f ? var_f5 : 2.5f;
    }

    param1->stickX = param2->ex_status.cl.lstick.x * var_f5;
    param1->stickY = -param2->ex_status.cl.lstick.y * var_f5;

    //! TODO: volatile probably makes it more funny to match
    temp_f2_2 = param1->stickX * 4.0f + param1->posX;
    temp_f0 = param1->stickY * 4.0f + param1->posY;

    param1->cursorX = temp_f2_2;
    param1->posX = temp_f2_2;
    param1->posY = temp_f0;
    param1->cursorY = temp_f0;
    param1->screenX = temp_f2_2 + sHalfScreenWidth;

    param1->screenY = param1->posY + sHalfScreenHeight;
    param1->hold = 0;
    param1->sinAngle = -0.2f;
    param1->cosAngle = 0.97899997f;

    if (param2->ex_status.cl.hold & 0x10) {
        param1->hold |= 0x01;
    }

    if (param2->ex_status.cl.hold & 0x01) {
        param1->hold |= 0x08;
    }

    if (param2->ex_status.cl.hold & 0x4000) {
        param1->hold |= 0x04;
    }

    if (param2->ex_status.cl.hold & 0x1000) {
        param1->hold |= 0x10;
    }

    if (param2->ex_status.cl.hold & 0x400) {
        param1->hold |= 0x20;
    }

    if (param2->ex_status.cl.hold & 0x800) {
        param1->hold |= 0x40;
    }
}
#pragma pop

void vcmvUpdateController(register int index) {
    s32 limit;
    VCMV_VOLATILE VCMVControllerState* cursor;
    KPADStatus* kpad;
    u8 prevDevType;
    s32 probe;
    s32 devType;

    cursor = &sManualCursors[index];
    kpad = sKpad[index];
    cursor->pad3C = kpad->speed;
    probe = WPADProbe(index, &devType);
    prevDevType = kpad->dev_type;
    KPADRead(index, kpad, 16);

    switch (probe) {
        case 0:
        case -2:
        case -3:
            if (kpad->dev_type == 0xFF || kpad->dev_type == 0xFC) {
                kpad->dev_type = prevDevType;
            } else {
                kpad->dev_type = devType;
            }
            break;
        default:
            sControllers.wiiCon[index].kpad = NULL;
            cursor->active = 0;
            return;
    }

    if (index == sManualActiveCursor) {
        limit = 1500;
    } else {
        limit = 500;
    }

    switch (kpad->dev_type) {
        case 0xFD:
            sControllers.wiiCon[index].kpad = NULL;
            cursor->active = 0;
            return;
        case 1: {
            vcmvProcessDpadInput(cursor, kpad);

            if (index == 0 && !sExitRequested) {
                u32 hold = kpad->hold;

                if ((hold & 0x2000) && (hold & 0x800)) {
                    if (sManualActive) {
                        if ((hold & 0x200) && !(hold & 0x100)) {
                            if (*wwwAspectPtr == 0 && vcmvNimsGuiFlag != 0) {
                                sManualActive = false;
                                VCMVPlaySound(1);
                                *wwwAspectPtr = 1;
                                wwwSaveSettings();
                            }
                        } else if ((hold & 0x100) && !(hold & 0x200)) {
                            if (*wwwAspectPtr != 0 && vcmvNimsGuiFlag != 0) {
                                sManualActive = false;
                                VCMVPlaySound(1);
                                *wwwAspectPtr = 0;
                                wwwSaveSettings();
                            }
                        } else if (wwwAspectRatio != 0 && vcmvPageCurrentWidth != vcmvPageTargetWidth && (hold & 0x400)) {
                            sManualActive = false;
                            VCMVPlaySound(1);
                            wwwSettingsBuf.flag5 ^= 1;
                            wwwSaveSettings();
                        }
                    }
                } else if (!(hold & 0x2800)) {
                    sManualActive = true;
                }
            }

            goto cursorMoved;
        }
        case 0:
        default:
            vcmvProcessDpadInput(cursor, kpad);
        cursorMoved:
            if (kpad->hold != 0 || (kpad->speed != cursor->pad3C && kpad->speed > 0.007f)) {
                cursor->active += 50;
            }

            cursor->isConnected = true;
            sControllers.wiiCon[index].use_devtype = 0;
            break;
        case 2: {
            s32 sinceStick = sManualFrameCount - cursor->timer1;
            s32 sinceDpad = sManualFrameCount - cursor->timer2;

            if (kpad->ex_status.cl.hold != 0 || kpad->ex_status.cl.lstick.x || kpad->ex_status.cl.lstick.y) {
                vcmvProcessStickInput(cursor, kpad);
                cursor->isConnected = false;
                sControllers.wiiCon[index].use_devtype = 2;
                cursor->timer1 = sManualFrameCount;
                cursor->active += 50;
            } else if (kpad->hold != 0 ||
                       (kpad->speed != cursor->pad3C &&
                        (kpad->speed > 0.015f || (sinceStick > 10 && kpad->speed > 0.005f)))) {
                vcmvProcessDpadInput(cursor, kpad);
                sControllers.wiiCon[index].use_devtype = 0;
                cursor->timer2 = sManualFrameCount;
                cursor->active += 50;
            } else if (sinceStick > sinceDpad) {
                vcmvProcessDpadInput(cursor, kpad);
                cursor->isConnected = true;
                sControllers.wiiCon[index].use_devtype = 0;
            } else {
                vcmvProcessStickInput(cursor, kpad);
                cursor->isConnected = false;
                sControllers.wiiCon[index].use_devtype = 2;
            }
            break;
        }
    }

    if (cursor->posX < -sHalfScreenWidth) {
        cursor->posX = -sHalfScreenWidth;
    } else if (cursor->posX > sHalfScreenWidth) {
        cursor->posX = sHalfScreenWidth;
    }

    if (cursor->posY < -sHalfScreenHeight) {
        cursor->posY = -sHalfScreenHeight;
    } else if (cursor->posY > sHalfScreenHeight) {
        cursor->posY = sHalfScreenHeight;
    }

    sControllers.wiiCon[index].kpad = kpad;
    sControllers.wiiCon[index].pos.x = cursor->posX / (sHalfScreenWidth * sWidescreenScale);
    sControllers.wiiCon[index].pos.y = cursor->posY / sHalfScreenHeight;

    if (cursor->active != 0) {
        cursor->active--;

        if (cursor->active > limit) {
            cursor->active = limit;
        }
    }

    (void)cursor->trig;
    (void)cursor->release;
    (void)cursor->pad10;

    if (sManualFrameCount <= 0x65 && sManualFrameCount >= 0x64) {
        cursor->pad10 = cursor->hold;
    }

    BOOL enabled = OSDisableInterrupts();
    cursor->trig = cursor->trig | (cursor->hold & ~cursor->pad10);
    cursor->release = cursor->release | (cursor->pad10 & ~cursor->hold);
    cursor->pad10 = cursor->hold;
    OSRestoreInterrupts(enabled);

    vcmvInitControllerState(cursor);
}

void vcmvUpdateCursorPositions(void) {
    u32 var_r4;

    if (sManualCursors[sManualActiveCursor].active == 0) {
        var_r4 = 0;

        for (int i = 0; i < 4; i++) {
            if (var_r4 < sManualCursors[i].active) {
                var_r4 = sManualCursors[i].active;
                sManualActiveCursor = i;
            }
        }
    }

    for (int var_r31 = 0; var_r31 < 4; var_r31++) {
        if (var_r31 != sManualActiveCursor) {
            vcmvUpdateController(var_r31);
        }
    }

    vcmvUpdateController(sManualActiveCursor);
}

void vcmvSendMouseEvent(s32 param1, s32 param2, s32 param3, s32 param4, VCMV_VOLATILE VCMVControllerState* param5) {
    s32 temp_r0;
    bool var_r4;

    temp_r0 = sAlignedWidth - 2;

    if (param2 > temp_r0) {
        param2 = temp_r0;
    } else if (param2 < 2) {
        param2 = 2;
    }

    temp_r0 = sAlignedHeight - 2;
    if (param3 > temp_r0) {
        param3 = temp_r0;
    } else if (param3 < 2) {
        param3 = 2;
    }

    var_r4 = false;
    if (param2 == sLastMouseX && param3 == sLastMouseY) {
        var_r4 = true;
    }

    if (param1 == 0) {
        if (sLastMouseEvent != 0 || !var_r4) {
            if (sIsScrollDragging && sIsMouseDown) {
                sPageScrollDirection = 0;

                if (sManualFrameCount - sScrollDragFrame > 5) {
                    temp_r0 = param3 - sScrollDragY;

                    if (temp_r0 * temp_r0 > 0xF) {
                        sScrollDragY = param3;
                        sScrollDragFrame = sManualFrameCount;
                    }
                }
            }

            if (sIsMouseDown && !(param5->hold & 3)) {
                sIsMouseDown = false;
                WWWSurfaceMouseEvt(2, param2, param3, 1, 0, 0);
            }

            goto block_29;
        }

    } else if (param1 == 1) {
        if (!sIsMouseDown || !var_r4) {
            sIsMouseDown = true;
            goto block_29;
        }

    } else if (param1 == 2) {
        if (sIsMouseDown) {
            sIsMouseDown = false;
            goto block_29;
        }
    } else {
    block_29:
        WWWSurfaceMouseEvt(param1, param2, param3, param4, 0, 0);
        sLastMouseEvent = param1;
        sLastMouseX = param2;
        sLastMouseY = param3;
        sLastMouseButton = param4;
    }

    return;
}

void vcmvSendWheelEvent(s32 param1, s32 param2, s32 param3, VCMV_VOLATILE VCMVControllerState* param4) {
    s32 var_r31 = param2;
    s32 var_r30 = param3;

    if (param2 > sAlignedWidth - 0x14) {
        var_r31 = sAlignedWidth - 0x14;
    } else if (param2 < 2) {
        var_r31 = 2;
    }

    if (param3 > sAlignedHeight - 2) {
        var_r30 = sAlignedHeight - 2;
    } else if (param3 < 2) {
        var_r30 = 2;
    }

    if (var_r31 != param2 || var_r30 != param3) {
        vcmvSendMouseEvent(0, var_r31, var_r30, 4, param4);
    }

    WWWSurfaceWheelEvt(0, var_r31, var_r30, param1, 0);
}

void vcmvProcessInput(void) {
    int i;
    static s32 sLastInputFrame = 0;

    u8 value = 0;

    if (vcmvIsPageFading) {
        if (sManualInitialized) {
            sManualInitialized = false;
            vcmvSendMouseEvent(0, 3, 0xA, 4, &sManualCursors[sManualActiveCursor]);
            vcmvSendMouseEvent(0, 3, 0xE4, 4, &sManualCursors[sManualActiveCursor]);
        }
    } else {
        sManualInitialized = true;
    }

    if (sLastInputFrame == sManualFrameCount) {
        return;
    }

    value = vcmvIsPageFading | sIsPageLoading | sRequestReload | sRequestPrevPage;

    if (value == 0) {
        sLastInputFrame = sManualFrameCount;

        if (!sIsPageLoading) {
            for (i = 0; i < 4; i++) {
                VCMV_VOLATILE VCMVControllerState* var_r29 = &sManualCursors[i];

                if (var_r29->active != 0 && sManualActiveCursor == i) {
                    if (var_r29->trig & 1) {
                        if (var_r29->screenX < sAlignedWidth - 0x10) {
                            sIsScrollDragging = false;
                            vcmvSendMouseEvent(0, var_r29->screenX, var_r29->screenY, 4, var_r29);
                            vcmvSendMouseEvent(1, var_r29->screenX, var_r29->screenY, 1, var_r29);
                            vcmvSendMouseEvent(2, var_r29->screenX, var_r29->screenY, 1, var_r29);
                        } else {
                            sIsScrollDragging = true;
                            sScrollDragY = var_r29->screenY;
                            vcmvSendMouseEvent(0, var_r29->screenX, var_r29->screenY, 4, var_r29);
                            vcmvSendMouseEvent(1, var_r29->screenX, var_r29->screenY, 1, var_r29);
                        }
                    } else if (var_r29->release & 1) {
                        vcmvSendMouseEvent(2, var_r29->screenX, var_r29->screenY, 1, var_r29);
                    } else {
                        vcmvSendMouseEvent(0, var_r29->screenX, var_r29->screenY, 4, var_r29);
                    }

                    if (vcmvFadeTimer == vcmvFadeDuration && !(vcmvIsPageFading | vcmvIsTransitioning | sRequestReload | sRequestPrevPage)) {
                        if (var_r29->trig & 0x20) {
                            if (strcmp(sCurrentPageUrl, sStartPageUrl) != 0) {
                                VCMVPlaySound(5);
                                sRequestReload = true;
                            }
                        } else if (var_r29->trig & 0x10) {
                            sRequestPrevPage = true;
                        }

                        if (var_r29->trig & 4) {
                            sScrollDragFrame = sManualFrameCount;
                            sPageScrollDirection = 1;
                            vcmvSendWheelEvent(1, var_r29->screenX, var_r29->screenY, var_r29);
                            sScrollRepeatFrame = sManualFrameCount + 0x10;
                        } else if (var_r29->hold & 0x04) {
                            if (sScrollRepeatFrame < sManualFrameCount) {
                                sScrollDragFrame = sManualFrameCount;
                                sPageScrollDirection = 1;
                                vcmvSendWheelEvent(1, var_r29->screenX, var_r29->screenY, var_r29);
                                s32 curFrame = sManualFrameCount;
                                sScrollRepeatFrame = curFrame;
                                if (sAlignedWidth <= 0x280) {
                                    sScrollRepeatFrame = curFrame + 1;
                                }
                            }
                        } else if (var_r29->trig & 8) {
                            sScrollDragFrame = sManualFrameCount;
                            sPageScrollDirection = -1;
                            vcmvSendWheelEvent(-1, var_r29->screenX, var_r29->screenY, var_r29);
                            sScrollRepeatFrame = sManualFrameCount + 0x10;
                        } else if (var_r29->hold & 0x08 && sScrollRepeatFrame < sManualFrameCount) {
                            sScrollDragFrame = sManualFrameCount;
                            sPageScrollDirection = -1;
                            vcmvSendWheelEvent(-1, var_r29->screenX, var_r29->screenY, var_r29);
                            s32 curFrame = sManualFrameCount;
                            sScrollRepeatFrame = curFrame;
                            if (sAlignedWidth <= 0x280) {
                                sScrollRepeatFrame = curFrame + 1;
                            }
                        }
                    }

                    var_r29->release &= 0x40;
                    var_r29->trig &= 0x40;
                }
            }
        }
    }

    for (int j = 0; j < 4; j++) {
        VCMV_VOLATILE VCMVControllerState* var_r30 = &sManualCursors[j];

        if (sManualActiveCursor == j) {
            if (var_r30->trig & 0x40) {
                VCMVPlaySound(3);
                sExitRequested = true;

                if (sMotorActive) {
                    sMotorActive = false;
                    sMotorStartTime = sManualFrameCount;
                    WPADControlMotor(sManualActiveCursor, WPAD_MOTOR_STOP);
                }
            }

            var_r30->release &= ~0x40;
            var_r30->trig &= ~0x40;
        } else {
            if (var_r30->trig & 0x01 || var_r30->trig & 0x02) {
                WPADControlMotor(sManualActiveCursor, WPAD_MOTOR_STOP);
                sCursorAnimTimer = 0x14;
                sManualActiveCursor = j;

                if (var_r30->isConnected != 0) {
                    sTriggerRumble = true;
                }
            }

            var_r30->trig = 0;
            var_r30->release = 0;
        }
    }
}

static void* sBrowserHeap;
static UNKWORD sBrowserAllocFunc;
static UNKWORD sBrowserFreeFunc;
static UNKWORD sBrowserReallocFunc[2]; //! TODO: real?
static OSThreadQueue sThreadQueue;
static bool sIsThreadRunning;
static volatile UNKWORD sRepeatDelay;
static volatile UNKWORD sRepeatRate;
static volatile UNKWORD sScrollSpeed;

bool vcmvAlloc(void** param1, u32 size, MEMAllocator* allocator1, MEMAllocator* allocator2) {
    return vcmvAllocBlock(param1, size, allocator1, allocator2);
}

void vcmvFree(void** param1) {
    if (*param1 != NULL) {
        vcmvFreeBlock(*param1);
        *param1 = NULL;
    }
}

void InitializeWWWAndOpera(void) {
    const char* sp8[6];
    s32 temp_r3;

    if (WWWSurfaceInit(sAlignedWidth, sAlignedHeight, sAlignedWidth * 4, 0, sSurfaceBuffer) != 0) {
        OSPanic("vcmv_main.cpp", 904, "Failed to initialize WWW");
    }

    if (WWWSurfaceSetFlushCallback((void*)vcmvFlushCallback, 0) != 0) {
        OSPanic("vcmv_main.cpp", 907, "Failed to init flush callback for WWW");
    }

    wwwAddFonts();

    sp8[0] = "Wii NTLG PGothic";
    sp8[1] = "Wii NTLG PGothic";
    sp8[4] = "Wii NTLG PGothic";
    sp8[3] = "Wii NTLG PGothic";
    sp8[2] = "Wii NTLG PGothic";
    sp8[5] = "Wii NTLG PGothic";

    const char* temp = "/flash/tmp/opera.arc/opera";
    temp_r3 = WWWCreateBrowser(&sBrowserHandle, (void*)vcmvOperaCallback, sp8, temp);

    if (temp_r3 != 0) {
        OSReport("Failed to init Opera: %d, %s\n", temp_r3, temp_r3 == -1 ? "OOM" : "Failure");
        WWWSurfaceShutdown();
    } else {
        if (!wwwStarted) {
            vcmvJsInit();
        }

        WWWAddProtocol("arc");
        wwwStarted = true;
    }
}

void vcmvAlarmHandler(OSAlarm* alarm, OSContext* ctx) { OSWakeupThread(&sThreadQueue); }

void* vcmvThreadFunc(void* arg) {
    OSAlarm sp40;
    Mtx sp10;
    GXColor sp8;
    f32 var_f1;
    s32 diff;
    int i;
    int j;
    u8 var_r27;

    diff = sManualFrameCount;
    sManualFrameCount = 100;
    diff -= sManualFrameCount;
    sMotorStartTime = 0;
    sMotorActive = false;

    for (i = 0; i < 4; i++) {
        sManualCursors[i].releaseStartFrame = sManualFrameCount - 7;
        if (i == sManualActiveCursor) {
            sManualCursors[i].active = 2000;
        }
        sManualCursors[i].cosAngle = 1.0f;
        sManualCursors[i].screenY = 0;
        sManualCursors[i].screenX = 0;
        sManualCursors[i].release = 0;
        sManualCursors[i].trig = 0;
        sManualCursors[i].hold = 0;
        sManualCursors[i].timer1 -= diff;
        sManualCursors[i].timer2 -= diff;
    }

    sIsThreadRunning = true;
    OSInitThreadQueue(&sThreadQueue);

    s64 period = OSMicrosecondsToTicks(1000000 / ((u32)VIGetTvFormat() == VI_TV_FMT_PAL ? 50 : 60));
    OSCreateAlarm(&sp40);
    OSSetPeriodicAlarm(&sp40, OSGetTime(), period, vcmvAlarmHandler);
    PSMTXIdentity(sp10);
    GXLoadPosMtxImm(sp10, 0);
    VISetBlack(false);

    var_r27 = 0;

    while (sRepeatDelay != 0) {
        sManualFrameCount++;

        if (sExitRequested) {
            sRepeatDelay--;

            if (sRepeatDelay >= sRepeatRate) {
                var_f1 = 1.0f;
            } else {
                var_f1 = (f32)sRepeatDelay / (f32)sRepeatRate;
            }

            if (sRepeatDelay >= sScrollSpeed) {
                var_r27 = 0;
            } else {
                var_r27 = 255 - (sRepeatDelay * 255 / sScrollSpeed);
            }

            HBMSetSoundVolume(var_f1);

            *(u32*)&sp8 = 0;
            GXSetCopyClear(sp8, 0xFFFFFF);
        } else if (sTriggerRumble) {
            sTriggerRumble = false;
            sMotorActive = true;
            sMotorStartTime = sManualFrameCount;
            WPADControlMotor(sManualActiveCursor, WPAD_MOTOR_RUMBLE);
        }

        if (sMotorActive && sManualFrameCount - sMotorStartTime >= 2) {
            sMotorActive = false;
            sMotorStartTime = sManualFrameCount;
            WPADControlMotor(sManualActiveCursor, WPAD_MOTOR_STOP);
        }

        GXInvalidateVtxCache();
        GXInvalidateTexAll();
        vcmvUpdateCursorPositions();
        vcmvSetupGX();
        vcmvDrawScreen(0.0f);
        for (j = 0; j < 4; j++) {
            if (j != sManualActiveCursor) {
                vcmvDrawCursor(j);
            }
        }
        vcmvDrawCursor(sManualActiveCursor);
        sExitCallback(var_r27, sActiveRenderMode);
        HBMUpdateSoundArchivePlayer();
    }

    HBMDeleteSoundArchivePlayer();
    OSCancelAlarm(&sp40);
    sIsThreadRunning = false;
    return NULL;
}

char* vcmvRunManual(Callback_8025D2D0 param1, char* param2, u8 param3) {
    void* var_r28 = NULL;

    sRepeatDelay = 0x32;
    sRepeatRate = 0x1E;
    sScrollSpeed = 0x1E;
    sExitRequested = false;
    OSEnableInterrupts();
    sManualActiveCursor = param3;
    sExitCallback = param1;
    sCurrentPageUrl = param2;

    if (sStartPageUrl == 0) {
        sStartPageUrl = param2;
    }

    sRenderModeId = 6;

    if (wwwLibLoaded) {
        sInitDelayCounter = 2;
        InitializeWWWAndOpera();
        vcmvSetupGX();
        vcmvCursorInit();
        vcmvOpenBrowserWindow(sRenderModeId);
        sIsBrowserActive = true;
        sSelectSoundPlayed = false;
        vcmvUpdateBrowser();

        vcmvAllocBlock(&var_r28, 0x4000, wwwMem1Allocator, wwwMem2Allocator);

        if (var_r28 != NULL) {
            OSCreateThread((OSThread*)&sManualThread, vcmvThreadFunc, 0, (u8*)var_r28 + 0x4000, 0x4000, 0x0E, 1);
            OSResumeThread((OSThread*)&sManualThread);

            do {
                if (!sExitRequested) {
                    vcmvUpdateBrowser();
                    vcmvProcessInput();
                }

                if (!sIsTransitioning) {
                    if (sRequestReload) {
                        vcmvReloadStartPage();
                    } else if (sRequestPrevPage) {
                        WWWPrevPage(sWindowId);
                    }

                    sRequestReload = false;
                    sRequestPrevPage = false;
                }

                if (!sNeedsInitialReload) {
                    sNeedsInitialReload = true;
                    vcmvReloadStartPage();
                }
            } while (!OSIsThreadTerminated((OSThread*)&sManualThread));

            if (var_r28 != NULL) {
                vcmvFreeBlock(var_r28);
            }

            WWWSurfaceShutdown();
        }
    }

    return sCurrentPageUrl;
}

extern f32 sWidescreenScale;

void ManualInit(MEMAllocator* param1, MEMAllocator* param2) {
    wwwLoadSettings();

    VCMV_VOLATILE VCMVControllerState* cursors = sManualCursors;
    cursors[0].active = 500;
    cursors[1].active = 500;
    cursors[2].active = 500;
    wwwMem1Allocator = param1;
    cursors[3].active = 500;
    wwwMem2Allocator = param2;
}

s32 ManualOpen(void) { return wwwInitialize(); }

void ManualClose(void) { wwwShutdown(); }

bool vcmvIsWidescreen(void) {
    wwwLoadSettings();
    if (wwwAspectRatio != 0 && wwwSettingsBuf.flag5 != 0) {
        sWidescreenScale = 1.333333f;
        return true;
    } else {
        sWidescreenScale = 1.0f;
        return false;
    }
}

void ManualSetRenderMode(GXRenderModeObj* param1, GXRenderModeObj* param2, u32 param3) {
    vcmvSetRenderMode(param1, param2, (u8)param3);
}

s32 ManualCreateScreen(s32 width, s32 height) { return vcmvCreateScreen(width, height); }

void ManualSetMargin(u16 arg0) { vcmvSetMargin(arg0); }

void ManualDestroyScreen(void) { vcmvDestroyScreen(); }

bool ManualAllocHeap(u32 param1) {
    vcmvAllocBlock(&sBrowserHeap, param1, wwwMem2Allocator, wwwMem1Allocator);

    if (sBrowserHeap == NULL) {
        return false;
    }

    WWWGetBrowserAllocationFunctions(sBrowserHeap, param1, &sBrowserAllocFunc, &sBrowserFreeFunc, sBrowserReallocFunc);
    WWWSetAllocationFunctions(sBrowserAllocFunc, sBrowserFreeFunc, sBrowserReallocFunc[0], sBrowserAllocFunc, sBrowserFreeFunc, sBrowserAllocFunc,
                              sBrowserFreeFunc);
    return true;
}

void ManualFreeHeap(void) {
    WWWShutdownBrowserAllocationFunctions();

    if (sBrowserHeap != NULL) {
        vcmvFreeBlock(sBrowserHeap);
        sBrowserHeap = NULL;
    }
}

void ManualSetArchive(void* param1) { vcmvSetArchive(param1); }

char* ManualRun(Callback_8025D2D0 param1, char* param2, u8 param3) { return vcmvRunManual(param1, param2, param3); }

void ManualSetStartPage(const char* param1) { sStartPageUrl = param1; }

void ManualRequestExit(s32 param1) {
    sExitRequested = true;

    if (sRepeatDelay > param1) {
        if (param1 >= 0x1E) {
            VCMVPlaySound(3);
        }

        sScrollSpeed = param1;
        sRepeatRate = param1;
        sRepeatDelay = param1;
    }
}

void ManualSetHBMInfo(void** param1) { vcmvSetHBMInfo(param1); }
