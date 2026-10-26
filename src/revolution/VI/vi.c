#include <revolution/DVD.h>
#include <revolution/OS.h>
#include <revolution/SC.h>
#include <revolution/SI.h>
#include <revolution/VI.h>

// Display timing of one TV mode
typedef struct timing_s {
    u8 equ;          // at 0x0
    u16 acv;         // at 0x2
    u16 prbOdd;      // at 0x4
    u16 prbEven;     // at 0x6
    u16 psbOdd;      // at 0x8
    u16 psbEven;     // at 0xA
    u8 bs1;          // at 0xC
    u8 bs2;          // at 0xD
    u8 bs3;          // at 0xE
    u8 bs4;          // at 0xF
    u16 be1;         // at 0x10
    u16 be2;         // at 0x12
    u16 be3;         // at 0x14
    u16 be4;         // at 0x16
    u16 nhlines;     // at 0x18
    u16 hlw;         // at 0x1A
    u8 hsy;          // at 0x1C
    u8 hcs;          // at 0x1D
    u8 hce;          // at 0x1E
    u8 hbe640;       // at 0x1F
    u16 hbs640;      // at 0x20
    u8 hbeCCIR656;   // at 0x22
    u16 hbsCCIR656;  // at 0x24
} timing_s;

// Current position and size of the display, the frame buffer and the pan window
typedef struct HorVer_s {
    u16 DispPosX;          // at 0x0
    u16 DispPosY;          // at 0x2
    u16 DispSizeX;         // at 0x4
    u16 DispSizeY;         // at 0x6
    u16 AdjustedDispPosX;  // at 0x8
    u16 AdjustedDispPosY;  // at 0xA
    u16 AdjustedDispSizeY; // at 0xC
    u16 AdjustedPanPosY;   // at 0xE
    u16 AdjustedPanSizeY;  // at 0x10
    u16 FBSizeX;           // at 0x12
    u16 FBSizeY;           // at 0x14
    u16 PanPosX;           // at 0x16
    u16 PanPosY;           // at 0x18
    u16 PanSizeX;          // at 0x1A
    u16 PanSizeY;          // at 0x1C
    VIXFBMode FBMode;      // at 0x20
    u32 nonInter;          // at 0x24
    u32 tv;                // at 0x28
    u8 wordPerLine;        // at 0x2C
    u8 std;                // at 0x2D
    u8 wpl;                // at 0x2E
    u32 bufAddr;           // at 0x30
    u32 tfbb;              // at 0x34
    u32 bfbb;              // at 0x38
    u8 xof;                // at 0x3C
    BOOL black;            // at 0x40
    BOOL threeD;           // at 0x44
    u32 rbufAddr;          // at 0x48
    u32 rtfbb;             // at 0x4C
    u32 rbfbb;             // at 0x50
    timing_s* timing;      // at 0x54
} HorVer_s;

const char* __VIVersion = "<< RVL_SDK - VI \trelease build: Jun  6 2007 03:01:19 (0x4199_60831) >>";

static BOOL IsInitialized = FALSE;
static void __VIRetraceHandler(s16 interrupt, OSContext* context);

static volatile u32 retraceCount;
static volatile u32 flushFlag;
static volatile u32 flushFlag3in1;
static volatile u32 vsync_timing_err_cnt = 0;
static volatile u32 vsync_timing_test_flag = 0;

// Screen dimming and DVD motor stop after a period without input (in retraces)
static volatile BOOL __VIDimming_All_Clear = FALSE;
static volatile BOOL __VIDimmingFlag_Enable;
static volatile BOOL __VIDVDStopFlag_Enable;
static volatile VITimeToDIM g_current_time_to_dim;
static volatile u32 THD_TIME_TO_DIMMING = 0;
static volatile u32 NEW_TIME_TO_DIMMING = 0;
static volatile u32 THD_TIME_TO_DVD_STOP = 0;
static volatile u32 _gIdleCount_dimming = 0;
static volatile u32 _gIdleCount_dvd = 0;
static volatile u32 __VIDimmingFlag_RF_IDLE;
static volatile u32 __VIDimmingFlag_SI_IDLE;
static volatile u32 __VIDimmingFlag_DEV_IDLE[10];
static volatile BOOL __VIDimmingState = FALSE;

static OSThreadQueue retraceQueue;
static VIRetraceCallback PreCB;
static VIRetraceCallback PostCB;
static VIPositionCallback PositionCallback = NULL;

static u32 encoderType;
static s16 displayOffsetH = 0;
static s16 displayOffsetV = 0;

#define ToPhysical(fb) (u32)(((u32)(fb)) & 0x3FFFFFFF)
#define IS_LOWER_16MB(x) ((x) < 16 * 1024 * 1024)
#define ONES(x) ((1 << (x)) - 1)

// Registers up to VI_HBS are shadowed; a set bit in a 64-bit mask marks a
// register as changed, counting from the most significant bit
#define VI_NUMREGS (VI_HBS + 1)
#define VI_REG_BIT(reg) (1ull << (63 - (reg)))

// Register field access; the casts to u32 are part of the original code shape
#define VI_REG_GET(reg, mask, shift) ((((u32)(reg)) & (mask)) >> (shift))
#define VI_REG_SET(reg, val, mask, shift) ((((u32)(reg)) & ~(mask)) | (((u32)(val)) << (shift)))

static volatile u32 changeMode = 0;
static volatile u64 changed = 0;
static u16 regs[VI_NUMREGS];

static volatile u32 shdwChangeMode = 0;
static volatile u64 shdwChanged = 0;
static u16 shdwRegs[VI_NUMREGS];

static HorVer_s HorVer;
static timing_s* CurrTiming;
static u32 CurrTvMode;

static u32 NextBufAddr;
static u32 CurrBufAddr;

static u32 FBSet = 0;

// Display timings, see getTiming
static timing_s timing[] = {
    // NTSC/EURGB60 interlaced
    {6, 240, 24, 25, 3, 2, 12, 13, 12, 13, 520, 519, 520, 519, 525, 429, 64, 71, 105, 162, 373, 122, 412},
    // NTSC/EURGB60 double-strike
    {6, 240, 24, 24, 4, 4, 12, 12, 12, 12, 520, 520, 520, 520, 526, 429, 64, 71, 105, 162, 373, 122, 412},
    // PAL interlaced
    {5, 287, 35, 36, 1, 0, 13, 12, 11, 10, 619, 618, 617, 620, 625, 432, 64, 75, 106, 172, 380, 133, 420},
    // PAL double-strike
    {5, 287, 33, 33, 2, 2, 13, 11, 13, 11, 619, 621, 619, 621, 624, 432, 64, 75, 106, 172, 380, 133, 420},
    // MPAL interlaced
    {6, 240, 24, 25, 3, 2, 16, 15, 14, 13, 518, 517, 516, 519, 525, 429, 64, 78, 112, 162, 373, 122, 412},
    // MPAL double-strike
    {6, 240, 24, 24, 4, 4, 16, 14, 16, 14, 518, 520, 518, 520, 526, 429, 64, 78, 112, 162, 373, 122, 412},
    // progressive
    {12, 480, 48, 48, 6, 6, 24, 24, 24, 24, 1038, 1038, 1038, 1038, 1050, 429, 64, 71, 105, 162, 373, 122, 412},
    // NTSC 3D
    {12, 480, 44, 44, 10, 10, 24, 24, 24, 24, 1038, 1038, 1038, 1038, 1050, 429, 64, 71, 105, 168, 379, 122, 412},
    // GCA interlaced
    {6, 241, 24, 25, 1, 0, 12, 13, 12, 13, 520, 519, 520, 519, 525, 429, 64, 71, 105, 159, 370, 122, 412},
    // GCA progressive
    {12, 480, 48, 48, 6, 6, 24, 24, 24, 24, 1038, 1038, 1038, 1038, 1050, 429, 64, 71, 105, 180, 391, 122, 412},
    // PAL progressive
    {10, 576, 62, 62, 6, 6, 20, 20, 20, 20, 1240, 1240, 1240, 1240, 1250, 432, 64, 75, 106, 172, 380, 122, 412},
};

static timing_s* timingExtra = NULL;

// Coefficients of the horizontal scaling filter
static u16 taps[] = {
    0x1f0, 0x1dc, 0x1ae, 0x174, 0x129, 0x0db, 0x08e, 0x046, 0x0c, 0xe2, 0xcb, 0xc0, 0xc4,
    0xcf,  0xde,  0xec,  0xfc,  0x08,  0x0f,  0x13,  0x13,  0x0f, 0x0c, 0x08, 0x01,
};

GXRenderModeObj GXPal528Prog = {
    VI_TVMODE_PAL_PROG,
    640,
    528,
    528,
    (VI_MAX_WIDTH_PAL - 640) / 2,
    (VI_MAX_HEIGHT_PAL - 528) / 2,
    640,
    528,
    VI_XFBMODE_SF,
    GX_FALSE,
    GX_FALSE,
    {{6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}},
    {0, 0, 21, 22, 21, 0, 0},
};

GXRenderModeObj GXPal528ProgSoft = {
    VI_TVMODE_PAL_PROG,
    640,
    528,
    528,
    (VI_MAX_WIDTH_PAL - 640) / 2,
    (VI_MAX_HEIGHT_PAL - 528) / 2,
    640,
    528,
    VI_XFBMODE_SF,
    GX_FALSE,
    GX_FALSE,
    {{6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}},
    {8, 8, 10, 12, 10, 8, 8},
};

GXRenderModeObj GXPal524ProgAa = {
    VI_TVMODE_PAL_PROG,
    640,
    264,
    524,
    (VI_MAX_WIDTH_PAL - 640) / 2,
    (VI_MAX_HEIGHT_PAL - 528) / 2,
    640,
    524,
    VI_XFBMODE_SF,
    GX_FALSE,
    GX_TRUE,
    {{3, 2}, {9, 6}, {3, 10}, {3, 2}, {9, 6}, {3, 10}, {9, 2}, {3, 6}, {9, 10}, {9, 2}, {3, 6}, {9, 10}},
    {4, 8, 12, 16, 12, 8, 4},
};

static BOOL OnShutdown(BOOL final, u32 event);
static OSShutdownFunctionInfo ShutdownFunctionInfo = {OnShutdown, 127};

// Switches the encoder to RGB before a shutdown and waits one retrace for it
static BOOL OnShutdown(BOOL final, u32 event) {
    BOOL retval;
    static BOOL first = TRUE;
    static u32 count;

    if (final == FALSE) {
        switch (event) {
        case OS_SD_EVENT_3:
        case OS_SD_EVENT_1:
        case OS_SD_EVENT_SHUTDOWN:
            if (first) {
                VISetRGBModeImm();
                VIFlush();
                count = retraceCount;
                first = FALSE;
                retval = FALSE;
            } else {
                if (count == retraceCount) {
                    retval = FALSE;
                } else {
                    retval = TRUE;
                }
            }
            break;

        case OS_SD_EVENT_RESTART:
        case OS_SD_EVENT_FATAL:
        case OS_SD_EVENT_LAUNCH_APP:
        case OS_SD_EVENT_RETURN_TO_MENU:
            retval = TRUE;
            break;
        }
    } else {
        retval = TRUE;
    }

    return retval;
}

static void GetCurrentDisplayPosition(u32* hct, u32* vct) {
    u32 hcount, vcount0, vcount;

    vcount = (u32)((VI_HW_REGS[VI_DPV]) & 0x7FF);

    do {
        vcount0 = vcount;
        hcount = (u32)(VI_HW_REGS[VI_DPH] & 0x7FF);
        vcount = (u32)(VI_HW_REGS[VI_DPV] & 0x7FF);
    } while (vcount0 != vcount);

    *hct = hcount;
    *vct = vcount;
}

static u32 getCurrentFieldEvenOdd(void);
static s32 cntlzd(u64);

static BOOL VISetRegs(void) {
    s32 regIndex;

    if (!((shdwChangeMode == 1) && (getCurrentFieldEvenOdd() == 0))) {
        while (shdwChanged) {
            regIndex = cntlzd(shdwChanged);
            VI_HW_REGS[regIndex] = shdwRegs[regIndex];
            shdwChanged &= ~VI_REG_BIT(regIndex);
        }

        shdwChangeMode = 0;
        CurrTiming = HorVer.timing;
        CurrTvMode = HorVer.tv;
        CurrBufAddr = NextBufAddr;

        return TRUE;
    } else {
        return FALSE;
    }
}

static void __VIRetraceHandler(s16 interrupt, OSContext* context) {
    OSContext exceptionContext;
    u16 reg;
    u32 inter = 0;
    u32 regIndex;
    static u32 old_dtvStatus = 999;
    static u32 old_tvtype = 999;
    u32 now_dtvStatus = 0;
    u32 now_tvtype = 0;
    static BOOL __VIDimmingFlag_Enable_old = TRUE;
    static BOOL __VIDVDStopFlag_Enable_old = TRUE;
    u32 i;
    static u32 DimmingON_Pending = 0;
    static u32 DimmingOFF_Pending = 0;

    reg = VI_HW_REGS[VI_DI0_H];
    if (reg & VI_DI_INT) {
        VI_HW_REGS[VI_DI0_H] = (u16)(reg & ~VI_DI_INT);
        inter |= 1;
    }

    reg = VI_HW_REGS[VI_DI1_H];
    if (reg & VI_DI_INT) {
        VI_HW_REGS[VI_DI1_H] = (u16)(reg & ~VI_DI_INT);
        inter |= 2;
    }

    reg = VI_HW_REGS[VI_DI2_H];
    if (reg & VI_DI_INT) {
        VI_HW_REGS[VI_DI2_H] = (u16)(reg & ~VI_DI_INT);
        inter |= 4;
    }

    reg = VI_HW_REGS[VI_DI3_H];
    if (reg & VI_DI_INT) {
        VI_HW_REGS[VI_DI3_H] = (u16)(reg & ~VI_DI_INT);
        inter |= 8;
    }

    reg = VI_HW_REGS[VI_DI3_H];

    if ((inter & 4) || (inter & 8)) {
        OSClearContext(&exceptionContext);
        OSSetCurrentContext(&exceptionContext);

        if (PositionCallback) {
            s16 x, y;
            __VIGetCurrentPosition(&x, &y);
            (*PositionCallback)(x, y);
        }

        OSClearContext(&exceptionContext);
        OSSetCurrentContext(context);
        return;
    }

    retraceCount++;
    OSClearContext(&exceptionContext);
    OSSetCurrentContext(&exceptionContext);

    if (PreCB) {
        (*PreCB)(retraceCount);
    }

    if (vsync_timing_test_flag) {
        u32 hcount, vcount;

        GetCurrentDisplayPosition(&hcount, &vcount);

        if (!((vcount == 1) || (vcount == (CurrTiming->nhlines / 2 + 1)))) {
            vsync_timing_err_cnt++;
        }
    }

    if (flushFlag) {
        if (VISetRegs()) {
            flushFlag = 0;
            SIRefreshSamplingRate();
        }
    }

    now_dtvStatus = VIGetDTVStatus();
    if (now_dtvStatus != old_dtvStatus) {
        __VISetYUVSEL((VIBool)now_dtvStatus);
    }

    old_dtvStatus = now_dtvStatus;
    now_tvtype = VIGetTvFormat();

    if (now_tvtype != old_tvtype) {
        if (now_tvtype == VI_TVFORMAT_EURGB60) {
            __VISetFilter4EURGB60(VI_ENABLE);
        } else {
            __VISetFilter4EURGB60(VI_DISABLE);
        }

        switch (now_tvtype) {
        case VI_TVFORMAT_PAL:
            switch (g_current_time_to_dim) {
            case VI_DM_10M:
                NEW_TIME_TO_DIMMING = 30000;
                break;
            case VI_DM_15M:
                NEW_TIME_TO_DIMMING = 45000;
                break;
            default:
                NEW_TIME_TO_DIMMING = 15000;
                break;
            }
            THD_TIME_TO_DVD_STOP = 90000;
            break;
        default:
            switch (g_current_time_to_dim) {
            case VI_DM_10M:
                NEW_TIME_TO_DIMMING = 36000;
                break;
            case VI_DM_15M:
                NEW_TIME_TO_DIMMING = 54000;
                break;
            default:
                NEW_TIME_TO_DIMMING = 18000;
                break;
            }
            THD_TIME_TO_DVD_STOP = 108000;
            break;
        }

        _gIdleCount_dimming = 0;
        _gIdleCount_dvd = 0;
    }

    old_tvtype = now_tvtype;

    if (flushFlag3in1) {
        while (Vdac_Flag_Changed) {
            regIndex = (u32)__cntlzw(Vdac_Flag_Changed);
            regIndex = (u32)(1 << (31 - regIndex));

            switch (regIndex) {
            case VI_VDAC_CHANGED_CGMS:
                __VISetCGMS();
                break;
            case VI_VDAC_CHANGED_WSS:
                __VISetWSS();
                break;
            case VI_VDAC_CHANGED_CC:
                __VISetClosedCaption();
                break;
            case VI_VDAC_CHANGED_MACROVISION:
                __VISetMacrovision();
                break;
            case VI_VDAC_CHANGED_GAMMA:
                __VISetGamma();
                break;
            case VI_VDAC_CHANGED_TRAP_FILTER:
                __VISetTrapFilter();
                break;
            case VI_VDAC_CHANGED_RGB_OVERDRIVE:
                __VISetRGBOverDrive();
                break;
            case VI_VDAC_CHANGED_RGB_MODE:
                __VISetRGBModeImm();
                break;
            }

            Vdac_Flag_Changed &= ~regIndex;
        }

        flushFlag3in1 = 0;
    }

    if (PostCB) {
        OSClearContext(&exceptionContext);
        (*PostCB)(retraceCount);
    }

    OSWakeupThread(&retraceQueue);
    OSClearContext(&exceptionContext);
    OSSetCurrentContext(context);

    if (__VIDimming_All_Clear == TRUE) {
        if (__OSSetVIForceDimming(FALSE, 0, 0) == TRUE) {
            __VIDimming_All_Clear = FALSE;
            _gIdleCount_dimming = 0;
        }
    }

    for (i = 0; i < 10; i++) {
        if (__VIDimmingFlag_DEV_IDLE[i] == 0) {
            __VIDimmingFlag_DEV_IDLE[0] = 0;
            break;
        }
    }

    if (__VIDimmingFlag_RF_IDLE && __VIDimmingFlag_SI_IDLE && __VIDimmingFlag_DEV_IDLE[0]) {
        if ((__VIDimmingFlag_Enable == TRUE) && (_gIdleCount_dimming < 0xFFFFFFFF)) {
            _gIdleCount_dimming++;
        }

        if ((__VIDVDStopFlag_Enable == TRUE) && (_gIdleCount_dvd < 0xFFFFFFFF)) {
            _gIdleCount_dvd++;
        }
    } else {
        if (_gIdleCount_dimming >= THD_TIME_TO_DIMMING) {
            DimmingOFF_Pending = 1;
        }
        if (_gIdleCount_dvd >= THD_TIME_TO_DVD_STOP) {
            __DVDRestartMotor();
        }

        _gIdleCount_dimming = 0;
        _gIdleCount_dvd = 0;
        THD_TIME_TO_DIMMING = NEW_TIME_TO_DIMMING;
    }

    if (__VIDimmingFlag_Enable_old != __VIDimmingFlag_Enable) {
        if (__VIDimmingFlag_Enable == FALSE) {
            if (_gIdleCount_dimming >= THD_TIME_TO_DIMMING) {
                DimmingOFF_Pending = 1;
            }
        }

        _gIdleCount_dimming = 0;
        THD_TIME_TO_DIMMING = NEW_TIME_TO_DIMMING;
    }

    if (_gIdleCount_dimming == THD_TIME_TO_DIMMING) {
        DimmingON_Pending = 1;
    }

    if (DimmingOFF_Pending) {
        if (__OSSetVIForceDimming(FALSE, 2, 2) == TRUE) {
            DimmingOFF_Pending = 0;
            __VIDimmingState = FALSE;
        }
    }

    if (DimmingON_Pending) {
        if (__OSSetVIForceDimming(TRUE, 2, 2) == TRUE) {
            DimmingON_Pending = 0;
            __VIDimmingState = TRUE;
        }
    }

    if (__VIDVDStopFlag_Enable_old != __VIDVDStopFlag_Enable) {
        if (__VIDVDStopFlag_Enable == FALSE) {
            if (_gIdleCount_dvd >= THD_TIME_TO_DVD_STOP) {
                __DVDRestartMotor();
            }
        }
        _gIdleCount_dvd = 0;
    }

    if (_gIdleCount_dvd == THD_TIME_TO_DVD_STOP) {
        __DVDStopMotorAsync(&__DVDStopMotorCommandBlock, NULL);
    }

    __VIDimmingFlag_RF_IDLE = 1;
    __VIDimmingFlag_SI_IDLE = 1;

    for (i = 0; i < 10; i++) {
        __VIDimmingFlag_DEV_IDLE[i] = 1;
    }

    __VIDimmingFlag_Enable_old = __VIDimmingFlag_Enable;
    __VIDVDStopFlag_Enable_old = __VIDVDStopFlag_Enable;

    if ((NEW_TIME_TO_DIMMING > _gIdleCount_dimming) && (__VIDimmingState == FALSE)) {
        THD_TIME_TO_DIMMING = NEW_TIME_TO_DIMMING;
    }
}

static void setScalingRegs(u16 panSizeX, u16 dispSizeX, BOOL threeD) {
    u32 scale;

    panSizeX = (u16)(threeD ? panSizeX * 2 : panSizeX);

    if (panSizeX < dispSizeX) {
        scale = (256 * (u32)panSizeX + (u32)dispSizeX - 1) / (u32)dispSizeX;

        regs[VI_HSR] = (u16)(scale | 1 << 12);
        changed |= VI_REG_BIT(VI_HSR);

        regs[VI_0x70] = (u16)panSizeX;
        changed |= VI_REG_BIT(VI_0x70);
    } else {
        regs[VI_HSR] = (u16)256;
        changed |= VI_REG_BIT(VI_HSR);
    }
}

static void calcFbbs(u32 bufAddr, u16 panPosX, u16 panPosY, u8 wordPerLine, VIXFBMode xfbMode, u16 dispPosY, u32* tfbb, u32* bfbb) {
    u32 bytesPerLine, xoffInWords;
    xoffInWords = (u32)panPosX / 16;
    bytesPerLine = (u32)wordPerLine * 32;

    *tfbb = bufAddr + xoffInWords * 32 + bytesPerLine * panPosY;
    *bfbb = (xfbMode == VI_XFBMODE_SF) ? *tfbb : (*tfbb + bytesPerLine);

    if (dispPosY % 2 == 1) {
        u32 tmp = *tfbb;
        *tfbb = *bfbb;
        *bfbb = tmp;
    }

    *tfbb = ToPhysical(*tfbb);
    *bfbb = ToPhysical(*bfbb);
}

static u32 getCurrentHalfLine(void) {
    u32 hcount;
    u32 vcount;

    GetCurrentDisplayPosition(&hcount, &vcount);

    return ((vcount - 1) << 1) + ((hcount - 1) / CurrTiming->hlw);
}

static u32 getCurrentFieldEvenOdd(void) {
    return (getCurrentHalfLine() < CurrTiming->nhlines) ? 1u : 0u;
}

VIRetraceCallback VISetPreRetraceCallback(VIRetraceCallback cb) {
    BOOL enabled;
    VIRetraceCallback oldcb = PreCB;
    enabled = OSDisableInterrupts();
    PreCB = cb;
    OSRestoreInterrupts(enabled);
    return oldcb;
}

VIRetraceCallback VISetPostRetraceCallback(VIRetraceCallback cb) {
    BOOL enabled;
    VIRetraceCallback oldcb = PostCB;
    enabled = OSDisableInterrupts();
    PostCB = cb;
    OSRestoreInterrupts(enabled);
    return oldcb;
}

static timing_s* getTiming(VITVMode mode) {
    switch (mode) {
    case VI_TVMODE_NTSC_INT:
        return &timing[0];
    case VI_TVMODE_NTSC_DS:
        return &timing[1];
    case VI_TVMODE_PAL_INT:
        return &timing[2];
    case VI_TVMODE_PAL_DS:
        return &timing[3];
    case VI_TVMODE_EURGB60_INT:
        return &timing[0];
    case VI_TVMODE_EURGB60_DS:
        return &timing[1];
    case VI_TVMODE_MPAL_INT:
        return &timing[4];
    case VI_TVMODE_MPAL_DS:
        return &timing[5];
    case VI_TVMODE_NTSC_PROG:
    case VI_TVMODE_MPAL_PROG:
    case VI_TVMODE_EURGB60_PROG:
        return &timing[6];
    case VI_TVMODE_NTSC_3D:
        return &timing[7];
    case VI_TVMODE_DEBUG_PAL_INT:
        return &timing[2];
    case VI_TVMODE_DEBUG_PAL_DS:
        return &timing[3];
    case VI_TVMODE_GCA_INT:
        return &timing[8];
    case VI_TVMODE_GCA_PROG:
        return &timing[9];
    case VI_TVMODE_PAL_PROG:
        return &timing[10];
    case VI_TVMODE_EXTRA_INT:
    case VI_TVMODE_EXTRA_DS:
    case VI_TVMODE_EXTRA_PROG:
        return timingExtra;
    }

    return NULL;
}

void __VIInit(VITVMode mode) {
    timing_s* tm;
    u32 nonInter;
    u32 tv, tvForReg;
    volatile u32 a;
    u16 hct;
    u16 vct;

    nonInter = (u32)mode & 3;
    tv = (u32)mode >> 2;

    OS_TV_FORMAT = tv;

    tm = getTiming(mode);

    // Reset the interface and give it time to settle
    VI_HW_REGS[VI_DCR] = VI_DCR_RST;
    for (a = 0; a < 1000; a++) {
        ;
    }

    VI_HW_REGS[VI_DCR] = 0;
    VI_HW_REGS[VI_HTR0_L] = (u16)(tm->hlw);
    VI_HW_REGS[VI_HTR0_H] = (u16)(tm->hce | tm->hcs << 8);
    VI_HW_REGS[VI_HTR1_L] = (u16)(tm->hsy | (tm->hbe640 & ONES(9)) << 7);
    VI_HW_REGS[VI_HTR1_H] = (u16)(tm->hbe640 >> 9 | tm->hbs640 << 1);

    if (encoderType == 0) {
        VI_HW_REGS[VI_HBE] = (u16)(tm->hbeCCIR656 | 1 << 15);
        VI_HW_REGS[VI_HBS] = (u16)(tm->hbsCCIR656);
    }

    VI_HW_REGS[VI_VTR] = (u16)(tm->equ);
    VI_HW_REGS[VI_VTO_L] = (u16)(tm->prbOdd + tm->acv * 2 - 2);
    VI_HW_REGS[VI_VTO_H] = (u16)(tm->psbOdd + 2);
    VI_HW_REGS[VI_VTE_L] = (u16)(tm->prbEven + tm->acv * 2 - 2);
    VI_HW_REGS[VI_VTE_H] = (u16)(tm->psbEven + 2);
    VI_HW_REGS[VI_BBEI_L] = (u16)(tm->bs1 | tm->be1 << 5);
    VI_HW_REGS[VI_BBEI_H] = (u16)(tm->bs3 | tm->be3 << 5);
    VI_HW_REGS[VI_BBOI_L] = (u16)(tm->bs2 | tm->be2 << 5);
    VI_HW_REGS[VI_BBOI_H] = (u16)(tm->bs4 | tm->be4 << 5);
    VI_HW_REGS[VI_HSW] = (u16)(40 | 40 << 8);
    VI_HW_REGS[VI_DI1_L] = 1;
    VI_HW_REGS[VI_DI1_H] = (u16)(1 | 1 << 12);

    hct = (u16)(tm->hlw + 1);
    vct = (u16)(tm->nhlines / 2 + 1);
    VI_HW_REGS[VI_DI0_L] = (u16)hct;
    VI_HW_REGS[VI_DI0_H] = (u16)(vct | 1 << 12);

    switch (tv) {
    case VI_TVFORMAT_PAL:
    case VI_TVFORMAT_MPAL:
    case VI_TVFORMAT_DEBUG:
        tvForReg = tv;
        break;

    default:
        tvForReg = VI_TVFORMAT_NTSC;
        break;
    }

    if ((nonInter == VI_SCANMODE_INT) || (nonInter == VI_SCANMODE_DS)) {
        VI_HW_REGS[VI_DCR] =
            (u16)(1 | (nonInter & 1) << 2 | tvForReg << 8);
        VI_HW_REGS[VI_VICLK] = 0;
    } else {
        VI_HW_REGS[VI_DCR] = (u16)(1 | 1 << 2 | tvForReg << 8);
        VI_HW_REGS[VI_VICLK] = 1;
    }
}

static void AdjustPosition(u16 acv) {
    s32 coeff, frac;

    HorVer.AdjustedDispPosX = (u16)CLAMP(0, VI_MAX_WIDTH_NTSC - HorVer.DispSizeX, (s16)HorVer.DispPosX + displayOffsetH);

    coeff = (HorVer.FBMode == VI_XFBMODE_SF) ? 2 : 1;
    frac = HorVer.DispPosY & 1;

    HorVer.AdjustedDispPosY = (u16)MAX((s16)HorVer.DispPosY + displayOffsetV, frac);

    HorVer.AdjustedDispSizeY = (u16)(HorVer.DispSizeY + MIN((s16)HorVer.DispPosY + displayOffsetV - frac, 0) -
                                     MAX((s16)HorVer.DispPosY + (s16)HorVer.DispSizeY + displayOffsetV - ((s16)acv * 2 - frac), 0));

    HorVer.AdjustedPanPosY = (u16)(HorVer.PanPosY - MIN((s16)HorVer.DispPosY + displayOffsetV - frac, 0) / coeff);

    HorVer.AdjustedPanSizeY = (u16)(HorVer.PanSizeY + MIN((s16)HorVer.DispPosY + displayOffsetV - frac, 0) / coeff -
                                    MAX((s16)HorVer.DispPosY + (s16)HorVer.DispSizeY + displayOffsetV - ((s16)acv * 2 - frac), 0) / coeff);
}

static void ImportAdjustingValues(void) {
    displayOffsetH = SCGetDisplayOffsetH();
    displayOffsetV = 0;
}

void VIInit(void) {
    u16 dspCfg;
    u32 value, tv, tvInBootrom;

    if (IsInitialized) {
        return;
    }

    OSRegisterVersion(__VIVersion);
    IsInitialized = TRUE;

    if (!(VI_HW_REGS[VI_DCR] & VI_DCR_ENB)) {
        __VIInit(VI_TVMODE_NTSC_INT);
    }

    retraceCount = 0;
    changed = 0;
    shdwChanged = 0;
    changeMode = 0;
    shdwChangeMode = 0;
    flushFlag = 0;
    flushFlag3in1 = 0;

    VI_HW_REGS[VI_FCT0_L] = (u16)(taps[0] | (taps[1] & ONES(6)) << 10);
    VI_HW_REGS[VI_FCT0_H] = (u16)(taps[1] >> 6 | taps[2] << 4);
    VI_HW_REGS[VI_FCT1_L] = (u16)(taps[3] | (taps[4] & ONES(6)) << 10);
    VI_HW_REGS[VI_FCT1_H] = (u16)(taps[4] >> 6 | taps[5] << 4);
    VI_HW_REGS[VI_FCT2_L] = (u16)(taps[6] | (taps[7] & ONES(6)) << 10);
    VI_HW_REGS[VI_FCT2_H] = (u16)(taps[7] >> 6 | taps[8] << 4);
    VI_HW_REGS[VI_FCT3_L] = (u16)(taps[9] | taps[10] << 8);
    VI_HW_REGS[VI_FCT3_H] = (u16)(taps[11] | taps[12] << 8);
    VI_HW_REGS[VI_FCT4_L] = (u16)(taps[13] | taps[14] << 8);
    VI_HW_REGS[VI_FCT4_H] = (u16)(taps[15] | taps[16] << 8);
    VI_HW_REGS[VI_FCT5_L] = (u16)(taps[17] | taps[18] << 8);
    VI_HW_REGS[VI_FCT5_H] = (u16)(taps[19] | taps[20] << 8);
    VI_HW_REGS[VI_FCT6_L] = (u16)(taps[21] | taps[22] << 8);
    VI_HW_REGS[VI_FCT6_H] = (u16)(taps[23] | taps[24] << 8);

    VI_HW_REGS[VI_0x70] = (u16)640;
    ImportAdjustingValues();
    tvInBootrom = OS_TV_FORMAT;
    dspCfg = VI_HW_REGS[VI_DCR];
    HorVer.nonInter = VIGetScanMode();

    HorVer.tv = VI_REG_GET(dspCfg, VI_DCR_FMT, 8);
    if ((tvInBootrom == VI_TVFORMAT_EURGB60) || ((tvInBootrom == VI_TVFORMAT_PAL) && (HorVer.tv == VI_TVFORMAT_NTSC))) {
        HorVer.tv = VI_TVFORMAT_EURGB60;
    }

    tv = (HorVer.tv == VI_TVFORMAT_DEBUG) ? VI_TVFORMAT_NTSC : HorVer.tv;
    HorVer.timing = getTiming((VITVMode)VI_TVMODE(tv, HorVer.nonInter));
    regs[VI_DCR] = dspCfg;
    CurrTiming = HorVer.timing;
    CurrTvMode = HorVer.tv;
    HorVer.DispSizeX = 640;
    HorVer.DispSizeY = (u16)(CurrTiming->acv * 2);
    HorVer.DispPosX = (u16)((720 - HorVer.DispSizeX) / 2);
    HorVer.DispPosY = 0;
    AdjustPosition(CurrTiming->acv);
    HorVer.FBSizeX = 640;
    HorVer.FBSizeY = (u16)(CurrTiming->acv * 2);
    HorVer.PanPosX = 0;
    HorVer.PanPosY = 0;
    HorVer.PanSizeX = 640;
    HorVer.PanSizeY = (u16)(CurrTiming->acv * 2);
    HorVer.FBMode = VI_XFBMODE_SF;
    HorVer.wordPerLine = 40;
    HorVer.std = 40;
    HorVer.wpl = 40;
    HorVer.xof = 0;
    HorVer.black = TRUE;
    HorVer.threeD = FALSE;

    OSInitThreadQueue(&retraceQueue);

    value = VI_HW_REGS[VI_DI0_H];
    value = VI_REG_SET(value, 0, VI_DI_INT, 15);
    VI_HW_REGS[VI_DI0_H] = (u16)value;

    value = VI_HW_REGS[VI_DI1_H];
    value = VI_REG_SET(value, 0, VI_DI_INT, 15);
    VI_HW_REGS[VI_DI1_H] = (u16)value;

    PreCB = (VIRetraceCallback)NULL;
    PostCB = (VIRetraceCallback)NULL;

    __OSSetInterruptHandler(OS_INTR_PI_VI, __VIRetraceHandler);
    __OSUnmaskInterrupts(OS_INTR_MASK(OS_INTR_PI_VI));
    OSRegisterShutdownFunction(&ShutdownFunctionInfo);

    switch (VIGetTvFormat()) {
    case VI_TVFORMAT_PAL:
        THD_TIME_TO_DIMMING = 15000;
        NEW_TIME_TO_DIMMING = 15000;
        THD_TIME_TO_DVD_STOP = 90000;
        break;
    default:
        THD_TIME_TO_DIMMING = 18000;
        NEW_TIME_TO_DIMMING = 18000;
        THD_TIME_TO_DVD_STOP = 108000;
        break;
    }
    _gIdleCount_dimming = 0;
    _gIdleCount_dvd = 0;
    g_current_time_to_dim = VI_DM_DEFAULT;
    __VIDimming_All_Clear = TRUE;
    __VIDimmingState = FALSE;
    VIEnableDimming(TRUE);

    VIEnableDVDStopMotor(FALSE);
    __VISetRevolutionModeSimple();
}

void VIWaitForRetrace(void) {
    BOOL enabled;
    u32 count;

    enabled = OSDisableInterrupts();
    count = retraceCount;

    do {
        OSSleepThread(&retraceQueue);
    } while (count == retraceCount);

    OSRestoreInterrupts(enabled);
}

static void setInterruptRegs(timing_s* tm) {
    u16 vct, hct, borrow;

    vct = (u16)(tm->nhlines / 2);
    borrow = (u16)(tm->nhlines % 2);
    hct = (u16)((borrow) ? tm->hlw : (u16)0);

    vct++;
    hct++;

    regs[VI_DI0_L] = (u16)hct;
    changed |= VI_REG_BIT(VI_DI0_L);

    regs[VI_DI0_H] = (u16)(vct | 1 << 12);
    changed |= VI_REG_BIT(VI_DI0_H);
}

static void setPicConfig(u16 fbSizeX, VIXFBMode xfbMode, u16 panPosX, u16 panSizeX, u8* wordPerLine, u8* std, u8* wpl, u8* xof) {
    *wordPerLine = (u8)((fbSizeX + 15) / 16);
    *std = (u8)((xfbMode == VI_XFBMODE_SF) ? *wordPerLine : (u8)(2 * *wordPerLine));
    *xof = (u8)(panPosX % 16);
    *wpl = (u8)((*xof + panSizeX + 15) / 16);

    regs[VI_HSW] = (u16)(*std | *wpl << 8);
    changed |= VI_REG_BIT(VI_HSW);
}

static void setBBIntervalRegs(timing_s* tm) {
    u16 val;

    val = (u16)(tm->bs1 | tm->be1 << 5);
    regs[VI_BBEI_L] = val;
    changed |= VI_REG_BIT(VI_BBEI_L);

    val = (u16)(tm->bs3 | tm->be3 << 5);
    regs[VI_BBEI_H] = val;
    changed |= VI_REG_BIT(VI_BBEI_H);

    val = (u16)(tm->bs2 | tm->be2 << 5);
    regs[VI_BBOI_L] = val;
    changed |= VI_REG_BIT(VI_BBOI_L);

    val = (u16)(tm->bs4 | tm->be4 << 5);
    regs[VI_BBOI_H] = val;
    changed |= VI_REG_BIT(VI_BBOI_H);
}

static s32 cntlzd(u64 bit) {
    u32 hi, lo;
    s32 value;

    hi = (u32)(bit >> 32);
    lo = (u32)(bit & 0xFFFFFFFF);
    value = __cntlzw(hi);

    if (value < 32) {
        return value;
    }

    return (32 + __cntlzw(lo));
}

static void PrintDebugPalCaution(void) {
    static u32 message = 0;

    if (message == 0) {
        message = 1;
        OSReport("***************************************\n");
        OSReport(" ! ! ! C A U T I O N ! ! !             \n");
        OSReport("This TV format \"DEBUG_PAL\" is only for \n");
        OSReport("temporary solution until PAL DAC board \n");
        OSReport("is available. Please do NOT use this   \n");
        OSReport("mode in real games!!!                  \n");
        OSReport("***************************************\n");
    }
}

void setFbbRegs(HorVer_s* HorVer, u32* tfbb, u32* bfbb, u32* rtfbb, u32* rbfbb) {
    u32 shifted;
    calcFbbs(HorVer->bufAddr, HorVer->PanPosX, HorVer->AdjustedPanPosY, HorVer->wordPerLine, HorVer->FBMode, HorVer->AdjustedDispPosY, tfbb, bfbb);

    if (HorVer->threeD) {
        calcFbbs(HorVer->rbufAddr, HorVer->PanPosX, HorVer->AdjustedPanPosY, HorVer->wordPerLine, HorVer->FBMode, HorVer->AdjustedDispPosY, rtfbb,
                 rbfbb);
    }

    if (IS_LOWER_16MB(*tfbb) && IS_LOWER_16MB(*bfbb) && IS_LOWER_16MB(*rtfbb) && IS_LOWER_16MB(*rbfbb)) {
        shifted = 0;
    } else {
        shifted = 1;
    }

    if (shifted) {
        *tfbb >>= 5;
        *bfbb >>= 5;
        *rtfbb >>= 5;
        *rbfbb >>= 5;
    }

    regs[VI_TFBL_L] = (u16)(*tfbb & 0xFFFF);
    changed |= VI_REG_BIT(VI_TFBL_L);

    regs[VI_TFBL_H] = (u16)(*tfbb >> 16 | HorVer->xof << 8 | shifted << 12);
    changed |= VI_REG_BIT(VI_TFBL_H);

    regs[VI_BFBL_L] = (u16)(*bfbb & 0xFFFF);
    changed |= VI_REG_BIT(VI_BFBL_L);

    regs[VI_BFBL_H] = (u16)(*bfbb >> 16);
    changed |= VI_REG_BIT(VI_BFBL_H);

    if (HorVer->threeD) {
        regs[VI_TFBR_L] = *rtfbb & 0xFFFF;
        changed |= VI_REG_BIT(VI_TFBR_L);

        regs[VI_TFBR_H] = *rtfbb >> 16;
        changed |= VI_REG_BIT(VI_TFBR_H);

        regs[VI_BFBR_L] = *rbfbb & 0xFFFF;
        changed |= VI_REG_BIT(VI_BFBR_L);

        regs[VI_BFBR_H] = *rbfbb >> 16;
        changed |= VI_REG_BIT(VI_BFBR_H);
    }
}

inline void setHorizontalRegs(timing_s* tm, u16 dispPosX, u16 dispSizeX) {
    u32 hbe, hbs, hbeLo, hbeHi;

    regs[VI_HTR0_L] = (u16)tm->hlw;
    changed |= VI_REG_BIT(VI_HTR0_L);

    regs[VI_HTR0_H] = (u16)(tm->hce | tm->hcs << 8);
    changed |= VI_REG_BIT(VI_HTR0_H);

    hbe = (u32)(tm->hbe640 - 40 + dispPosX);
    hbs = (u32)(tm->hbs640 + 40 + dispPosX - (720 - dispSizeX));

    hbeLo = hbe & ONES(9);
    hbeHi = hbe >> 9;

    regs[VI_HTR1_L] = (u16)(tm->hsy | hbeLo << 7);
    changed |= VI_REG_BIT(VI_HTR1_L);

    regs[VI_HTR1_H] = (u16)(hbeHi | hbs << 1);
    changed |= VI_REG_BIT(VI_HTR1_H);
}

void setVerticalRegs(u16 dispPosY, u16 dispSizeY, u8 equ, u16 acv, u16 prbOdd, u16 prbEven, u16 psbOdd, u16 psbEven, BOOL black) {
    u16 actualPrbOdd, actualPrbEven, actualPsbOdd, actualPsbEven, actualAcv, c, d;

    if (regs[VI_VICLK] & VI_VICLK_SPEED) {
        c = 1;
        d = 2;
    } else {
        c = 2;
        d = 1;
    }

    if (dispPosY % 2 == 0) {
        actualPrbOdd = (u16)(prbOdd + d * dispPosY);
        actualPsbOdd = (u16)(psbOdd + d * ((c * acv - dispSizeY) - dispPosY));
        actualPrbEven = (u16)(prbEven + d * dispPosY);
        actualPsbEven = (u16)(psbEven + d * ((c * acv - dispSizeY) - dispPosY));
    } else {
        actualPrbOdd = (u16)(prbEven + d * dispPosY);
        actualPsbOdd = (u16)(psbEven + d * ((c * acv - dispSizeY) - dispPosY));
        actualPrbEven = (u16)(prbOdd + d * dispPosY);
        actualPsbEven = (u16)(psbOdd + d * ((c * acv - dispSizeY) - dispPosY));
    }

    actualAcv = (u16)(dispSizeY / c);

    if (black) {
        actualPrbOdd += 2 * actualAcv - 2;
        actualPsbOdd += 2;
        actualPrbEven += 2 * actualAcv - 2;
        actualPsbEven += 2;
        actualAcv = 0;
    }

    regs[VI_VTR] = (u16)(equ | actualAcv << 4);
    changed |= VI_REG_BIT(VI_VTR);

    regs[VI_VTO_L] = (u16)actualPrbOdd;
    changed |= VI_REG_BIT(VI_VTO_L);

    regs[VI_VTO_H] = (u16)actualPsbOdd;
    changed |= VI_REG_BIT(VI_VTO_H);

    regs[VI_VTE_L] = (u16)actualPrbEven;
    changed |= VI_REG_BIT(VI_VTE_L);

    regs[VI_VTE_H] = (u16)actualPsbEven;
    changed |= VI_REG_BIT(VI_VTE_H);
}

void VIConfigure(const GXRenderModeObj* rm) {
    timing_s* tm;
    u32 regDspCfg, regClksel;
    BOOL enabled;
    u32 newNonInter, tvInBootrom, tvInGame;

    enabled = OSDisableInterrupts();
    newNonInter = (u32)rm->tvInfo & 3;

    if (HorVer.nonInter != newNonInter) {
        changeMode = 1;
        HorVer.nonInter = newNonInter;
    }

    tvInGame = (u32)rm->tvInfo >> 2;
    tvInBootrom = OS_TV_FORMAT;

    if (tvInGame == VI_TVFORMAT_DEBUG_PAL) {
        PrintDebugPalCaution();
    }

    // A game can't switch between 50 Hz and 60 Hz formats
    switch (tvInBootrom) {
    case VI_TVFORMAT_NTSC:
    case VI_TVFORMAT_MPAL:
    case VI_TVFORMAT_GCA:
    case VI_TVFORMAT_EXTRA:
        if (tvInGame == VI_TVFORMAT_NTSC || tvInGame == VI_TVFORMAT_MPAL || tvInGame == VI_TVFORMAT_GCA ||
            tvInGame == VI_TVFORMAT_EXTRA) {
            goto ok;
        }
        break;
    case VI_TVFORMAT_PAL:
    case VI_TVFORMAT_EURGB60:
        if (tvInGame == VI_TVFORMAT_PAL || tvInGame == VI_TVFORMAT_EURGB60) {
            goto ok;
        }
        break;
    }

    OSPanic(__FILE__, 0xA59, "VIConfigure(): Tried to change mode from (%d) to (%d), which is forbidden\n", tvInBootrom, tvInGame);

ok:

    if ((tvInGame == VI_TVFORMAT_NTSC) || (tvInGame == VI_TVFORMAT_MPAL)) {
        HorVer.tv = tvInBootrom;
    } else {
        HorVer.tv = tvInGame;
    }

    HorVer.DispPosX = rm->viXOrigin;
    HorVer.DispPosY = (u16)((HorVer.nonInter == VI_SCANMODE_DS) ? (u16)(rm->viYOrigin * 2) : rm->viYOrigin);
    HorVer.DispSizeX = rm->viWidth;
    HorVer.FBSizeX = rm->fbWidth;
    HorVer.FBSizeY = rm->xfbHeight;
    HorVer.FBMode = (VIXFBMode)rm->xfbMode;
    HorVer.PanSizeX = HorVer.FBSizeX;
    HorVer.PanSizeY = HorVer.FBSizeY;
    HorVer.PanPosX = 0;
    HorVer.PanPosY = 0;

    HorVer.DispSizeY = (u16)((HorVer.nonInter == VI_SCANMODE_PROG) ? HorVer.PanSizeY
                             : (HorVer.nonInter == VI_SCANMODE_3D) ? HorVer.PanSizeY
                             : (HorVer.FBMode == VI_XFBMODE_SF)    ? (u16)(2 * HorVer.PanSizeY)
                                                                   : HorVer.PanSizeY);

    HorVer.threeD = (HorVer.nonInter == VI_SCANMODE_3D) ? TRUE : FALSE;

    tm = getTiming((VITVMode)VI_TVMODE(HorVer.tv, HorVer.nonInter));
    HorVer.timing = tm;

    AdjustPosition(tm->acv);
    setInterruptRegs(tm);

    regDspCfg = regs[VI_DCR];
    regClksel = regs[VI_VICLK];

    if ((HorVer.nonInter == VI_SCANMODE_PROG) || (HorVer.nonInter == VI_SCANMODE_3D)) {
        regDspCfg = VI_REG_SET(regDspCfg, 1, VI_DCR_NIN, 2);

        regClksel = VI_REG_SET(regClksel, VI_VICLK_54MHZ, VI_VICLK_SPEED, 0);
    } else {
        regDspCfg = VI_REG_SET(regDspCfg, HorVer.nonInter & 1, VI_DCR_NIN, 2);
        regClksel = VI_REG_SET(regClksel, VI_VICLK_27MHZ, VI_VICLK_SPEED, 0);
    }

    regDspCfg = VI_REG_SET(regDspCfg, HorVer.threeD, VI_DCR_3D, 3);

    if ((HorVer.tv == VI_TVFORMAT_PAL) || (HorVer.tv == VI_TVFORMAT_MPAL) || (HorVer.tv == VI_TVFORMAT_DEBUG)) {
        regDspCfg = VI_REG_SET(regDspCfg, HorVer.tv, VI_DCR_FMT, 8);
    } else {
        regDspCfg = VI_REG_SET(regDspCfg, VI_TVFORMAT_NTSC, VI_DCR_FMT, 8);
    }

    regs[VI_DCR] = (u16)regDspCfg;
    regs[VI_VICLK] = (u16)regClksel;
    changed |= VI_REG_BIT(VI_DCR);
    changed |= VI_REG_BIT(VI_VICLK);

    setScalingRegs(HorVer.PanSizeX, HorVer.DispSizeX, HorVer.threeD);
    setHorizontalRegs(tm, HorVer.AdjustedDispPosX, HorVer.DispSizeX);
    setBBIntervalRegs(tm);
    setPicConfig(HorVer.FBSizeX, HorVer.FBMode, HorVer.PanPosX, HorVer.PanSizeX, &HorVer.wordPerLine, &HorVer.std, &HorVer.wpl, &HorVer.xof);

    if (FBSet) {
        setFbbRegs(&HorVer, &HorVer.tfbb, &HorVer.bfbb, &HorVer.rtfbb, &HorVer.rbfbb);
    }

    setVerticalRegs(HorVer.AdjustedDispPosY, HorVer.AdjustedDispSizeY, tm->equ, tm->acv, tm->prbOdd, tm->prbEven, tm->psbOdd, tm->psbEven,
                    HorVer.black);
    OSRestoreInterrupts(enabled);
}

void VIConfigurePan(u16 xOrg, u16 yOrg, u16 width, u16 height) {
    BOOL enabled;
    timing_s* tm;

    enabled = OSDisableInterrupts();

    HorVer.PanPosX = xOrg;
    HorVer.PanPosY = yOrg;
    HorVer.PanSizeX = width;
    HorVer.PanSizeY = height;
    HorVer.DispSizeY = (u16)((HorVer.nonInter == VI_SCANMODE_PROG) ? HorVer.PanSizeY
                             : (HorVer.nonInter == VI_SCANMODE_3D) ? HorVer.PanSizeY
                             : (HorVer.FBMode == VI_XFBMODE_SF)    ? (u16)(2 * HorVer.PanSizeY)
                                                                   : HorVer.PanSizeY);

    tm = HorVer.timing;

    AdjustPosition(tm->acv);
    setScalingRegs(HorVer.PanSizeX, HorVer.DispSizeX, HorVer.threeD);
    setPicConfig(HorVer.FBSizeX, HorVer.FBMode, HorVer.PanPosX, HorVer.PanSizeX, &HorVer.wordPerLine, &HorVer.std, &HorVer.wpl, &HorVer.xof);

    if (FBSet) {
        setFbbRegs(&HorVer, &HorVer.tfbb, &HorVer.bfbb, &HorVer.rtfbb, &HorVer.rbfbb);
    }

    setVerticalRegs(HorVer.AdjustedDispPosY, HorVer.DispSizeY, tm->equ, tm->acv, tm->prbOdd, tm->prbEven, tm->psbOdd, tm->psbEven, HorVer.black);

    OSRestoreInterrupts(enabled);
}

void VIFlush(void) {
    BOOL enabled;
    s32 regIndex;

    enabled = OSDisableInterrupts();
    shdwChangeMode |= changeMode;
    changeMode = 0;
    shdwChanged |= changed;

    while (changed) {
        regIndex = cntlzd(changed);
        shdwRegs[regIndex] = regs[regIndex];
        changed &= ~VI_REG_BIT(regIndex);
    }

    flushFlag = 1;
    flushFlag3in1 = 1;
    NextBufAddr = HorVer.bufAddr;
    OSRestoreInterrupts(enabled);
}

void VISetNextFrameBuffer(void* fb) {
    BOOL enabled = OSDisableInterrupts();
    HorVer.bufAddr = (u32)fb;
    FBSet = 1;
    setFbbRegs(&HorVer, &HorVer.tfbb, &HorVer.bfbb, &HorVer.rtfbb, &HorVer.rbfbb);
    OSRestoreInterrupts(enabled);
}

inline void* VIGetNextFrameBuffer(void) {
    return (void*)NextBufAddr;
}

inline void* VIGetCurrentFrameBuffer(void) {
    return (void*)CurrBufAddr;
}

void VISetBlack(BOOL black) {
    BOOL enabled;
    timing_s* tm;

    enabled = OSDisableInterrupts();
    HorVer.black = black;
    tm = HorVer.timing;
    setVerticalRegs(HorVer.AdjustedDispPosY, HorVer.DispSizeY, tm->equ, tm->acv, tm->prbOdd, tm->prbEven, tm->psbOdd, tm->psbEven, HorVer.black);
    OSRestoreInterrupts(enabled);
}

s32 VIGetRetraceCount(void) {
    return retraceCount;
}

u32 VIGetNextField(void) {
    s32 nextField;
    BOOL enabled;

    enabled = OSDisableInterrupts();
    nextField = getCurrentFieldEvenOdd() ^ 1;
    OSRestoreInterrupts(enabled);
    return nextField ^ (HorVer.AdjustedDispPosY & 1);
}

u32 VIGetCurrentLine(void) {
    u32 halfLine;
    timing_s* tm;
    BOOL enabled;

    tm = CurrTiming;
    enabled = OSDisableInterrupts();
    halfLine = getCurrentHalfLine();
    OSRestoreInterrupts(enabled);

    if (halfLine >= tm->nhlines) {
        halfLine -= tm->nhlines;
    }

    return (halfLine >> 1);
}

VITVFormat VIGetTvFormat(void) {
    u32 format;
    BOOL enabled;

    enabled = OSDisableInterrupts();

    switch (CurrTvMode) {
    case VI_TVFORMAT_NTSC:
    case VI_TVFORMAT_DEBUG:
    case VI_TVFORMAT_GCA:
    case VI_TVFORMAT_EXTRA:
        format = VI_TVFORMAT_NTSC;
        break;

    case VI_TVFORMAT_PAL:
    case VI_TVFORMAT_DEBUG_PAL:
        format = VI_TVFORMAT_PAL;
        break;

    case VI_TVFORMAT_EURGB60:
    case VI_TVFORMAT_MPAL:
        format = CurrTvMode;
        break;

    default:
        break;
    }

    OSRestoreInterrupts(enabled);

    return (VITVFormat)format;
}

VIScanMode VIGetScanMode(void) {
    u32 scanMode;
    BOOL enabled;

    enabled = OSDisableInterrupts();

    if (VI_REG_GET(VI_HW_REGS[VI_VICLK], VI_VICLK_SPEED, 0) == VI_VICLK_54MHZ) {
        scanMode = VI_SCANMODE_PROG;
    } else {
        if (VI_REG_GET(VI_HW_REGS[VI_DCR], VI_DCR_NIN, 2) == 0) {
            scanMode = VI_SCANMODE_INT;
        } else {
            scanMode = VI_SCANMODE_DS;
        }
    }
    OSRestoreInterrupts(enabled);

    return (VIScanMode)scanMode;
}

u32 VIGetDTVStatus(void) {
    u32 dtvStatus;
    BOOL enabled;

    enabled = OSDisableInterrupts();
    dtvStatus = VI_REG_GET(VI_HW_REGS[VI_VISEL], 3, 0);
    OSRestoreInterrupts(enabled);

    return (dtvStatus & 0x01);
}

void __VIDisplayPositionToXY(u32 hct, u32 vct, s16* x, s16* y) {
    u32 halfLine = ((vct - 1) << 1) + ((hct - 1) / CurrTiming->hlw);

    if (HorVer.nonInter == VI_SCANMODE_INT) {
        if (halfLine < CurrTiming->nhlines) {
            if (halfLine < CurrTiming->equ * 3 + CurrTiming->prbOdd) {
                *y = -1;
            } else if (halfLine >= CurrTiming->nhlines - CurrTiming->psbOdd) {
                *y = -1;
            } else {
                *y = (s16)((halfLine - CurrTiming->equ * 3 - CurrTiming->prbOdd) & ~1);
            }
        } else {
            halfLine -= CurrTiming->nhlines;

            if (halfLine < CurrTiming->equ * 3 + CurrTiming->prbEven) {
                *y = -1;
            } else if (halfLine >= CurrTiming->nhlines - CurrTiming->psbEven) {
                *y = -1;
            } else {
                *y = (s16)(((halfLine - CurrTiming->equ * 3 - CurrTiming->prbEven) & ~1) + 1);
            }
        }
    } else if (HorVer.nonInter == VI_SCANMODE_DS) {
        if (halfLine >= CurrTiming->nhlines) {
            halfLine -= CurrTiming->nhlines;
        }

        if (halfLine < CurrTiming->equ * 3 + CurrTiming->prbOdd) {
            *y = -1;
        } else if (halfLine >= CurrTiming->nhlines - CurrTiming->psbOdd) {
            *y = -1;
        } else {
            *y = (s16)((halfLine - CurrTiming->equ * 3 - CurrTiming->prbOdd) & ~1);
        }
    } else if (HorVer.nonInter == VI_SCANMODE_PROG) {
        if (halfLine < CurrTiming->nhlines) {
            if (halfLine < CurrTiming->equ * 3 + CurrTiming->prbOdd) {
                *y = -1;
            } else if (halfLine >= CurrTiming->nhlines - CurrTiming->psbOdd) {
                *y = -1;
            } else {
                *y = (s16)(halfLine - CurrTiming->equ * 3 - CurrTiming->prbOdd);
            }
        } else {
            halfLine -= CurrTiming->nhlines;

            if (halfLine < CurrTiming->equ * 3 + CurrTiming->prbEven) {
                *y = -1;
            } else if (halfLine >= CurrTiming->nhlines - CurrTiming->psbEven) {
                *y = -1;
            } else {
                *y = (s16)((halfLine - CurrTiming->equ * 3 - CurrTiming->prbEven) & ~1);
            }
        }
    }

    *x = (s16)(hct - 1);
}

inline u32 VIGetDimmingCount(void) {
    u32 count;

    if (_gIdleCount_dimming >= THD_TIME_TO_DIMMING) {
        count = 0;
    } else {
        count = (u32)THD_TIME_TO_DIMMING - _gIdleCount_dimming;
    }
    return count;
}

inline void __VIGetCurrentPosition(s16* x, s16* y) {
    u32 hcount;
    u32 vcount;

    GetCurrentDisplayPosition(&hcount, &vcount);
    __VIDisplayPositionToXY(hcount, vcount, x, y);
}

inline BOOL __VIResetDev0Idle(void) {
    __VIDimmingFlag_DEV_IDLE[0] = 0;
    return TRUE;
}

inline BOOL VIEnableDVDStopMotor(BOOL enable) {
    BOOL old = __VIDVDStopFlag_Enable;
    __VIDVDStopFlag_Enable = enable;
    return old;
}

inline u32 VIGetDVDStopMotorCount(void) {
    u32 count;

    if (_gIdleCount_dvd >= THD_TIME_TO_DVD_STOP) {
        count = 0;
    } else {
        count = (u32)THD_TIME_TO_DVD_STOP - _gIdleCount_dvd;
    }

    return count;
}

inline BOOL VIEnableDimming(BOOL enable) {
    u8 value;
    BOOL old = __VIDimmingFlag_Enable;

    if (enable == TRUE) {
        value = SCGetScreenSaverMode();

        if (value == 0) {
            enable = FALSE;
        }
    }

    __VIDimmingFlag_Enable = enable;
    return old;
}

VITimeToDIM VISetTimeToDimming(VITimeToDIM time) {
    VITimeToDIM old_time = g_current_time_to_dim;
    g_current_time_to_dim = time;

    switch (VIGetTvFormat()) {
    case VI_TVFORMAT_PAL:
        switch (g_current_time_to_dim) {
        case VI_DM_10M:
            NEW_TIME_TO_DIMMING = 30000;
            break;
        case VI_DM_15M:
            NEW_TIME_TO_DIMMING = 45000;
            break;
        default:
            NEW_TIME_TO_DIMMING = 15000;
            break;
        }
        break;
    default:
        switch (g_current_time_to_dim) {
        case VI_DM_10M:
            NEW_TIME_TO_DIMMING = 36000;
            break;
        case VI_DM_15M:
            NEW_TIME_TO_DIMMING = 54000;
            break;
        default:
            NEW_TIME_TO_DIMMING = 18000;
            break;
        }
        break;
    }
    return old_time;
}

BOOL VIResetDimmingCount(void) {
    return __VIResetDev0Idle();
}

BOOL __VIResetRFIdle(void) {
    __VIDimmingFlag_RF_IDLE = 0;
    return TRUE;
}
