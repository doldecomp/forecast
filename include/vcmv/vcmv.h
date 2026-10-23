#ifndef VCMV_H
#define VCMV_H

#include <revolution/GX.h>
#include <revolution/KPAD.h>
#include <types.h>
#include <revolution/MEM.h>
#include <revolution/OS.h>
#include <revolution/TPL.h>
#include "vcmv_www.h"

#define VCMV_VOLATILE volatile

#define WPAD_DEV_INITIALIZING 251
#define WPAD_DEV_252 252
#define WPAD_DEV_FS WPAD_DEV_FREESTYLE
#define WPAD_DEV_NONE WPAD_DEV_NOT_FOUND
#define ARRAY_COUNT(arr) (sizeof(arr) / sizeof((arr)[0]))
#define VI_TV_FMT_PAL VI_TVFORMAT_PAL
#define OSMicrosecondsToTicks OS_USEC_TO_TICKS

struct HBMController {
    KPADStatus* kpad;
    Vec2 pos;
    u32 use_devtype;
};
struct HBMControllerData {
    HBMController wiiCon[4];
};

extern "C" {
void HBMPlaySound(s32);
void HBMSetSoundVolume(f32);
void HBMUpdateSoundArchivePlayer(void);
void HBMDeleteSoundArchivePlayer(void);
}


typedef struct Rect {
    /* 00 */ f32 x0;
    /* 04 */ f32 y0;
    /* 08 */ f32 x1;
    /* 0C */ f32 y1;
} Rect; // size = 0x10

typedef struct VCMVQuad {
    /* 00 */ f32 x1;
    /* 04 */ f32 x2;
    /* 08 */ f32 x3;
    /* 0C */ f32 x4;
    /* 10 */ f32 y1;
    /* 14 */ f32 y2;
    /* 18 */ f32 y3;
    /* 1C */ f32 y4;
} VCMVQuad;

typedef struct VCMVControllerState {
    /* 00 */ u32 active;
    /* 04 */ u32 hold;
    /* 08 */ u32 trig;
    /* 0C */ u32 release;
    /* 10 */ u32 pad10;
    /* 14 */ s32 screenX;
    /* 18 */ s32 screenY;
    /* 1C */ f32 posX;
    /* 20 */ f32 posY;
    /* 24 */ f32 stickX;
    /* 28 */ f32 stickY;
    union {
        /* 2C */ f32 cosAngle;
        /* 2C */ f32 horizonX;
    };
    union {
        /* 30 */ f32 sinAngle;
        /* 30 */ f32 horizonY;
    };
    /* 34 */ f32 cursorX;
    /* 38 */ f32 cursorY;
    /* 3C */ f32 pad3C;
    /* 40 */ s32 holdStartFrame;
    /* 44 */ s32 releaseStartFrame;
    /* 48 */ s32 timer1;
    /* 4C */ s32 timer2;
    /* 50 */ bool isConnected;
} VCMVControllerState; // size = 0x54

typedef struct VCMVRect {
    /* 00 */ int x;
    /* 04 */ int y;
    /* 08 */ int width;
    /* 0C */ int height;
} VCMVRect;

typedef struct VCMVScrollHistory {
    /* 00 */ int lastMouseY;
    /* 04 */ int scrollPos;
} VCMVScrollHistory;

typedef struct VCMVTransitionState {
    /* 00 */ VCMVScrollHistory history[4];
    /* 20 */ VCMVRect rect;
    /* 30 */ int activeTexIndex;
    /* 34 */ int prevTexIndex;
    /* 38 */ int fadeTexIndex;
    /* 3C */ int lastFrame;
    /* 40 */ f32 targetScroll;
    /* 44 */ f32 currentScroll;
    /* 48 */ f32 scrollVelocity;
    /* 4C */ int accumScroll;
    /* 50 */ int scrollOffset;
    /* 51 */ bool hasRect;
    /* 52 */ bool needsAdjustment;
} VCMVTransitionState; // size = 0x54

typedef struct WWWProtocolRequest {
    /* 00 */ const char* url;
    /* 04 */ const char* method;
    /* 08 */ void* handle;
} WWWProtocolRequest; // size = 0x0C

typedef struct VCMVCursorDef {
    /* 00 */ f32 width;
    /* 04 */ f32 height;
    /* 08 */ f32 hotspotX;
    /* 0C */ f32 hotspotY;
    /* 10 */ TPLPalette* tpl;
    /* 14 */ GXColor color;
} VCMVCursorDef; // size = 0x18

// vcmv_cursor.cpp
void VCMVPlaySound(s32 param1);
void vcmvCursorInit(void);
void vcmvSetHBMInfo(void** param1);
void vcmvDrawCursor(s32 param1);

extern s32 sCursorAnimTimer;

// vcmv_draw.cpp
s32 vcmvCalcDiffRatio(VCMVRect* param1, f32* param2);
bool vcmvDetectScrollDown(VCMVRect* param1, s32* param2);
bool vcmvDetectScrollUp(VCMVRect* param1, s32* param2);
void vcmvApplyScroll(VCMVRect* param1, s32 param2);
void vcmvFlushCallback(VCMVRect* param1, s32 param2);
void vcmvConvertSurfaceToTexture(void);
s32 vcmvOperaCallback(void* param1, int param2, int param3, WWWProtocolRequest* param4);
void vcmvSetArchive(void* param1);
void vcmvSetRenderMode(GXRenderModeObj* param1, GXRenderModeObj* param2, u8 param3);
void vcmvSetMargin(u16 param1);
bool vcmvCreateScreen(s32 param1, s32 param2);
void vcmvDestroyScreen(void);
void vcmvSetupGX(void);
void vcmvOpenBrowserWindow(s32 param1);
void vcmvUpdateBrowser(void);
void vcmvSetupQuadRender(GXTexObj* param1, GXColor param2);
void VCMVDrawQuad(const VCMVQuad* param1);
void vcmvDrawScreen(f32 param1);
void vcmvReloadStartPage(void);

// vcmv_rso.cpp
int vcmvAbs(int x);
void vcmvLinkRSO(void* param1);

// vcmv_main.cpp
bool vcmvAlloc(void** param1, u32 size, MEMAllocator* allocator1, MEMAllocator* allocator2);
void vcmvFree(void** param1);
bool vcmvIsWidescreen(void);

extern const char* sStartPageUrl;
extern char* sCurrentPageUrl;
extern u32* sSurfaceBuffer;
extern u16 sClampedScreenWidth;
extern u16 sFbWidth;
extern u16 sAlignedWidth;
extern u16 sAlignedHeight;
extern f32 sLeftMargin;
extern s32 sWindowId;
extern s32 vcmvFadeTimer;
extern bool vcmvIsTransitioning;
extern bool sIsPageLoading;
extern bool vcmvIsPageFading;
extern s8 sPageScrollDirection;
extern volatile bool sIsTransitioning;
extern f32 sWidescreenScale;
extern GXRenderModeObj* sActiveRenderMode;
extern f32 sHalfScreenWidth;
extern f32 sHalfScreenHeight;
extern s32 vcmvFadeDuration;
extern u16 sRightMargin;
extern u8 vcmvNimsGuiFlag;
extern GXRenderModeObj* vcmvPageTargetWidth;
extern GXRenderModeObj* vcmvPageCurrentWidth;

// vcmv_javascript.cpp
typedef struct ESValue {
    /* 00 */ s32 type;
    /* 04 & 08 */ union {
        f64 value_double;
        void* ptr;
        s32 value_int;
    };
} ESValue; // size = 0x0C

typedef struct ESPluginApi {
    /* 00 */ s32 (*registerClass)(s32, s32, s32, void*, void*, const char*, s32, void*);
    /* 04 */ s32 (*registerMethod)(s32, void*, s32, s32, void*);
} ESPluginApi;

typedef struct ESPluginDef {
    /* 00 */ const char** extNames;
    /* 04 */ u32 reserved04;
    /* 08 */ void* initFunc;
    /* 0C */ u32 reserved0C;
    /* 10 */ u32 reserved10;
    /* 14 */ u32 reserved14;
    /* 18 */ u32 reserved18;
    /* 1C */ void* checkFunc;
} ESPluginDef;

bool vcmvJsTrue(void);
void vcmvJsInit(void);
s32 vcmvJsPluginInit(s32 param1, const char* param2, ESValue* param3);
s32 vcmvJsPluginCallback(void* param1, s32 param2, int param3, void* param4, ESValue* param5);
s32 vcmvJsMethodDispatcher(s32 param1, const char* param2, ESValue* param3);
s32 vcmvJsFadeIn(s32 param1, s32 param2, s32 param3, ESValue* param4);
s32 vcmvJsFocusSound(s32 param1, s32 param2, s32 param3, ESValue* param4);
s32 vcmvJsLeftScroll(s32 param1, s32 param2, s32 param3, ESValue* param4);
s32 vcmvJsRightScroll(s32 param1, s32 param2, s32 param3, ESValue* param4);
s32 vcmvJsSelectSound(s32 param1, s32 param2, s32 param3, ESValue* param4);
s32 vcmvJsWriteFlag(s32 param1, s32 param2, s32 param3, ESValue* param4);
s32 vcmvJsGetScrollDirection(void);
s32 vcmvJsGetScrollAmount(void);
void vcmvJsResetScroll(void);

extern bool sSelectSoundPlayed;

// vcmv_main.cpp
typedef void (*Callback_8025D2D0)(u8, GXRenderModeObj*);

void vcmvInitControllerState(VCMV_VOLATILE VCMVControllerState* param1);
void vcmvProcessDpadInput(VCMV_VOLATILE VCMVControllerState* param1, KPADStatus* param2);
void vcmvProcessStickInput(VCMV_VOLATILE VCMVControllerState* param1, KPADStatus* param2);
void vcmvUpdateController(int index);
void vcmvUpdateCursorPositions(void);
void vcmvSendMouseEvent(s32 param1, s32 param2, s32 param3, s32 param4, VCMV_VOLATILE VCMVControllerState* param5);
void vcmvSendWheelEvent(s32 param1, s32 param2, s32 param3, VCMV_VOLATILE VCMVControllerState* param4);
void vcmvProcessInput(void);
void InitializeWWWAndOpera(void);
void vcmvAlarmHandler(OSAlarm* alarm, OSContext* ctx);
void* vcmvThreadFunc(void* arg);
char* vcmvRunManual(Callback_8025D2D0 param1, char* param2, u8 param3);

void ManualInit(MEMAllocator* param1, MEMAllocator* param2);
s32 ManualOpen(void);
void ManualClose(void);
void ManualSetRenderMode(GXRenderModeObj* param1, GXRenderModeObj* param2, u32 param3);
s32 ManualCreateScreen(s32 param1, s32 param2);
void ManualSetMargin(u16 param1);
void ManualDestroyScreen(void);
bool ManualAllocHeap(u32 param1);
void ManualFreeHeap(void);
void ManualSetArchive(void* param1);
char* ManualRun(Callback_8025D2D0 param1, char* param2, u8 param3);
void ManualSetStartPage(const char* param1);
void ManualRequestExit(s32 param1);
void ManualSetHBMInfo(void** param1);

extern HBMControllerData sControllers;
extern VCMVControllerState sManualCursorArray[4];
#define sManualCursors ((VCMV_VOLATILE VCMVControllerState*)sManualCursorArray)
extern volatile bool sExitRequested;
extern volatile bool sTriggerRumble;
extern u8 sInitDelayCounter;
extern s32 sBrowserHandle;
extern u8 sManualActiveCursor;
extern volatile s32 sManualFrameCount;
extern bool sMotorActive;
extern s32 sMotorStartTime;
extern s32 sScrollDragFrame;
extern s32 sLastMouseY;

// vcmv_rso.cpp
struct RSOImportEntry {
    const char* name;
    void* func;
};

struct RSOSection {
    u32 addr;
    u32 size;
};

struct RSOModule {
    u8 pad[0x0C];
    RSOSection* sections;
};

struct RSOSymbol {
    u32 hash;
    u32 offset;
    u32 sectionIdx;
};

#endif
