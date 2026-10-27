#include <revolution/BASE.h>
#include <revolution/GX.h>
#include <revolution/OS.h>

#include <cstring>

static GXFifoObjImpl CPUFifo;
static GXFifoObjImpl GPFifo;

static GXBool CPGPLinked;
static OSThread* __GXCurrentThread;
static BOOL GXOverflowSuspendInProgress;
static GXBreakPtCallback BreakPointCB;
static void* __GXCurrentBP;
static u32 __GXOverflowCount;
static GXBool GPFifoReady;
static GXBool CPUFifoReady;

static void __GXFifoReadEnable(void) {
    GX_CP_SET_BIT(gxdt->cpCtrlReg, GX_CP_CONTROL_FIFO_READ, 1);
    __cpReg[GX_CP_MMIO_CONTROL] = gxdt->cpCtrlReg;
}

static void __GXFifoReadDisable(void) {
    GX_CP_SET_BIT(gxdt->cpCtrlReg, GX_CP_CONTROL_FIFO_READ, 0);
    __cpReg[GX_CP_MMIO_CONTROL] = gxdt->cpCtrlReg;
}

static void __GXFifoLink(GXBool link) {
    GX_CP_SET_BIT(gxdt->cpCtrlReg, GX_CP_CONTROL_GP_LINK, link ? 1 : 0);
    __cpReg[GX_CP_MMIO_CONTROL] = gxdt->cpCtrlReg;
}

static void __GXWriteFifoIntEnable(GXBool overflow, GXBool underflow) {
    GX_CP_SET_BIT(gxdt->cpCtrlReg, GX_CP_CONTROL_OVERFLOW_INT, overflow);
    GX_CP_SET_BIT(gxdt->cpCtrlReg, GX_CP_CONTROL_UNDERFLOW_INT, underflow);
    __cpReg[GX_CP_MMIO_CONTROL] = gxdt->cpCtrlReg;
}

static void __GXWriteFifoIntReset(GXBool overflow, GXBool underflow) {
    GX_CP_SET_BIT(gxdt->cpClrReg, GX_CP_CLEAR_OVERFLOW, overflow);
    GX_CP_SET_BIT(gxdt->cpClrReg, GX_CP_CLEAR_UNDERFLOW, underflow);
    __cpReg[GX_CP_MMIO_CLEAR] = gxdt->cpClrReg;
}

void GXCPInterruptHandler(s16 interrupt, OSContext* context) {
    OSContext tempContext;

    gxdt->cpStatReg = __cpReg[GX_CP_MMIO_STATUS];

    // The GP drained the FIFO below the low watermark: resume the writer.
    if (GX_CP_GET_BIT(gxdt->cpCtrlReg, GX_CP_CONTROL_UNDERFLOW_INT) &&
        GX_CP_GET_BIT(gxdt->cpStatReg, GX_CP_STATUS_UNDERFLOW)) {
        OSResumeThread(__GXCurrentThread);
        GXOverflowSuspendInProgress = FALSE;
        __GXWriteFifoIntReset(TRUE, TRUE);
        __GXWriteFifoIntEnable(TRUE, FALSE);
    }

    // The FIFO filled past the high watermark: suspend the writer.
    if (GX_CP_GET_BIT(gxdt->cpCtrlReg, GX_CP_CONTROL_OVERFLOW_INT) &&
        GX_CP_GET_BIT(gxdt->cpStatReg, GX_CP_STATUS_OVERFLOW)) {
        __GXOverflowCount++;
        __GXWriteFifoIntEnable(FALSE, TRUE);
        __GXWriteFifoIntReset(TRUE, FALSE);
        GXOverflowSuspendInProgress = TRUE;
        OSSuspendThread(__GXCurrentThread);
    }

    if (GX_CP_GET_BIT(gxdt->cpCtrlReg, GX_CP_CONTROL_BREAKPOINT_INT) &&
        GX_CP_GET_BIT(gxdt->cpStatReg, GX_CP_STATUS_BREAKPOINT)) {
        GX_CP_SET_BIT(gxdt->cpCtrlReg, GX_CP_CONTROL_BREAKPOINT_INT, 0);
        __cpReg[GX_CP_MMIO_CONTROL] = gxdt->cpCtrlReg;

        if (BreakPointCB != NULL) {
            OSClearContext(&tempContext);
            OSSetCurrentContext(&tempContext);
            BreakPointCB();
            OSClearContext(&tempContext);
            OSSetCurrentContext(context);
        }
    }
}

inline void GXInitFifoPtrs(GXFifoObj* fifo, void* readPtr, void* writePtr) {
    GXFifoObjImpl* impl = (GXFifoObjImpl*)fifo;
    BOOL enabled = OSDisableInterrupts();

    impl->readPtr = readPtr;
    impl->writePtr = writePtr;
    impl->count = (u8*)writePtr - (u8*)readPtr;
    if (impl->count < 0) {
        impl->count += impl->size;
    }

    OSRestoreInterrupts(enabled);
}

inline void GXInitFifoLimits(GXFifoObj* fifo, u32 hiWatermark,
                             u32 loWatermark) {
    GXFifoObjImpl* impl = (GXFifoObjImpl*)fifo;
    impl->hiWatermark = hiWatermark;
    impl->loWatermark = loWatermark;
}

void GXInitFifoBase(GXFifoObj* fifo, void* base, u32 size) {
    GXFifoObjImpl* impl = (GXFifoObjImpl*)fifo;

    impl->base = base;
    impl->end = (u8*)base + size - 4;
    impl->size = size;
    impl->count = 0;

    GXInitFifoLimits(fifo, size - 0x4000, ROUND_DOWN(size / 2, 32));
    GXInitFifoPtrs(fifo, base, base);
}

GXBool CPGPLinkCheck(void) {
    u32 sharedEnds = 0;
    s32 cpuEndToGpBase;
    s32 gpEndToCpuBase;
    BOOL overlap;

    if (!CPUFifoReady || !GPFifoReady) {
        return FALSE;
    }

    if (CPUFifo.base == GPFifo.base) {
        sharedEnds++;
    }
    if (CPUFifo.end == GPFifo.end) {
        sharedEnds++;
    }

    // The FIFOs are linked when they are the same buffer.
    if (sharedEnds == 2) {
        return TRUE;
    }

    // Two different FIFOs must not overlap.
    cpuEndToGpBase = (u8*)CPUFifo.end - (u8*)GPFifo.base;
    gpEndToCpuBase = (u8*)GPFifo.end - (u8*)CPUFifo.base;
    overlap = (cpuEndToGpBase > 0 && gpEndToCpuBase > 0) ||
              (cpuEndToGpBase < 0 && gpEndToCpuBase < 0);
    if (overlap) {
        OSReport("CPUFifo: %08X - %08X\n", CPUFifo.base, CPUFifo.end);
        OSReport("GP Fifo: %08X - %08X\n", GPFifo.base, GPFifo.end);
    }

    return FALSE;
}

void GXSetCPUFifo(GXFifoObj* fifo) {
    BOOL enabled = OSDisableInterrupts();

    if (fifo == NULL) {
        CPUFifoReady = FALSE;
        CPGPLinked = FALSE;
        CPUFifo.bindGP = FALSE;
        CPUFifo.bindCPU = FALSE;
        OSRestoreInterrupts(enabled);
        return;
    }

    CPUFifo = *(GXFifoObjImpl*)fifo;
    CPUFifoReady = TRUE;
    CPUFifo.bindCPU = TRUE;

    if (CPGPLinkCheck()) {
        CPGPLinked = TRUE;
        CPUFifo.bindGP = TRUE;

        __piReg[PI_FIFO_BASE] = (u32)CPUFifo.base & 0x3FFFFFFF;
        __piReg[PI_FIFO_END] = (u32)CPUFifo.end & 0x3FFFFFFF;
        __piReg[PI_FIFO_WRITE_PTR] =
            GX_BITSET(0, 3, 24, ((u32)CPUFifo.writePtr & 0x3FFFFFFF) >> 5);

        __GXWriteFifoIntReset(TRUE, TRUE);
        __GXWriteFifoIntEnable(TRUE, FALSE);
        __GXFifoLink(TRUE);
    } else {
        CPUFifo.bindGP = FALSE;

        if (CPGPLinked) {
            __GXFifoLink(FALSE);
            CPGPLinked = FALSE;
        }

        __GXWriteFifoIntEnable(FALSE, FALSE);

        __piReg[PI_FIFO_BASE] = (u32)CPUFifo.base & 0x3FFFFFFF;
        __piReg[PI_FIFO_END] = (u32)CPUFifo.end & 0x3FFFFFFF;
        __piReg[PI_FIFO_WRITE_PTR] =
            GX_BITSET(0, 3, 24, ((u32)CPUFifo.writePtr & 0x3FFFFFFF) >> 5);
    }

    PPCSync();
    OSRestoreInterrupts(enabled);
}

void GXSetGPFifo(GXFifoObj* fifo) {
    BOOL enabled = OSDisableInterrupts();
    u32 control;

    __GXFifoReadDisable();
    __GXWriteFifoIntEnable(FALSE, FALSE);

    if (fifo == NULL) {
        GPFifoReady = FALSE;
        CPGPLinked = FALSE;
        __GXFifoLink(FALSE);
        GPFifo.bindCPU = FALSE;
        GPFifo.bindGP = FALSE;
        OSRestoreInterrupts(enabled);
        return;
    }

    GPFifo = *(GXFifoObjImpl*)fifo;
    GPFifoReady = TRUE;
    GPFifo.bindGP = TRUE;

    __cpReg[GX_CP_MMIO_FIFO_BASE_LO] = (u32)GPFifo.base;
    __cpReg[GX_CP_MMIO_FIFO_END_LO] = (u32)GPFifo.end;
    __cpReg[GX_CP_MMIO_FIFO_COUNT_LO] = GPFifo.count;
    __cpReg[GX_CP_MMIO_FIFO_WRITE_PTR_LO] = (u32)GPFifo.writePtr;
    __cpReg[GX_CP_MMIO_FIFO_READ_PTR_LO] = (u32)GPFifo.readPtr;
    __cpReg[GX_CP_MMIO_FIFO_HI_WATERMARK_LO] = GPFifo.hiWatermark;
    __cpReg[GX_CP_MMIO_FIFO_LO_WATERMARK_LO] = GPFifo.loWatermark;

    __cpReg[GX_CP_MMIO_FIFO_BASE_HI] = ((u32)GPFifo.base & 0x3FFFFFFF) >> 16;
    __cpReg[GX_CP_MMIO_FIFO_END_HI] = ((u32)GPFifo.end & 0x3FFFFFFF) >> 16;
    __cpReg[GX_CP_MMIO_FIFO_COUNT_HI] = GPFifo.count >> 16;
    __cpReg[GX_CP_MMIO_FIFO_WRITE_PTR_HI] =
        ((u32)GPFifo.writePtr & 0x3FFFFFFF) >> 16;
    __cpReg[GX_CP_MMIO_FIFO_READ_PTR_HI] =
        ((u32)GPFifo.readPtr & 0x3FFFFFFF) >> 16;
    __cpReg[GX_CP_MMIO_FIFO_HI_WATERMARK_HI] = GPFifo.hiWatermark >> 16;
    __cpReg[GX_CP_MMIO_FIFO_LO_WATERMARK_HI] = GPFifo.loWatermark >> 16;

    PPCSync();

    if (CPGPLinkCheck()) {
        CPGPLinked = TRUE;
        GPFifo.bindCPU = TRUE;
        __GXWriteFifoIntEnable(TRUE, FALSE);
        __GXFifoLink(TRUE);
    } else {
        CPGPLinked = FALSE;
        GPFifo.bindCPU = FALSE;
        __GXWriteFifoIntEnable(FALSE, FALSE);
        __GXFifoLink(FALSE);
    }

    // Pulse the control register with the breakpoint turned off.
    control = gxdt->cpCtrlReg;
    GX_CP_SET_BIT(control, GX_CP_CONTROL_BREAKPOINT, 0);
    GX_CP_SET_BIT(control, GX_CP_CONTROL_BREAKPOINT_INT, 0);
    __cpReg[GX_CP_MMIO_CONTROL] = control;
    __cpReg[GX_CP_MMIO_CONTROL] = gxdt->cpCtrlReg;

    __GXWriteFifoIntReset(TRUE, TRUE);
    __GXFifoReadEnable();

    OSRestoreInterrupts(enabled);
}

GXBool __GXIsGPFifoReady(void) {
    return GPFifoReady;
}

GXBreakPtCallback GXSetBreakPtCallback(GXBreakPtCallback callback) {
    GXBreakPtCallback old = BreakPointCB;
    BOOL enabled = OSDisableInterrupts();
    BreakPointCB = callback;
    OSRestoreInterrupts(enabled);
    return old;
}

void __GXFifoInit(void) {
    __OSSetInterruptHandler(OS_INTR_PI_CP, GXCPInterruptHandler);
    __OSUnmaskInterrupts(OS_INTR_MASK(OS_INTR_PI_CP));

    __GXCurrentThread = OSGetCurrentThread();
    GXOverflowSuspendInProgress = FALSE;

    memset(&CPUFifo, 0, sizeof(GXFifoObjImpl));
    memset(&GPFifo, 0, sizeof(GXFifoObjImpl));

    CPUFifoReady = FALSE;
    GPFifoReady = FALSE;
}

void __GXCleanGPFifo(void) {
    BOOL enabled;

    if (!GPFifoReady) {
        return;
    }

    enabled = OSDisableInterrupts();

    __GXFifoReadDisable();
    __GXWriteFifoIntEnable(FALSE, FALSE);

    // Drop everything the GP has not read yet.
    GPFifo.readPtr = GPFifo.writePtr;
    GPFifo.count = 0;

    __cpReg[GX_CP_MMIO_FIFO_COUNT_LO] = GPFifo.count;
    __cpReg[GX_CP_MMIO_FIFO_WRITE_PTR_LO] = (u32)GPFifo.writePtr;
    __cpReg[GX_CP_MMIO_FIFO_READ_PTR_LO] = (u32)GPFifo.readPtr;
    __cpReg[GX_CP_MMIO_FIFO_COUNT_HI] = GPFifo.count >> 16;
    __cpReg[GX_CP_MMIO_FIFO_WRITE_PTR_HI] =
        ((u32)GPFifo.writePtr & 0x3FFFFFFF) >> 16;
    __cpReg[GX_CP_MMIO_FIFO_READ_PTR_HI] =
        ((u32)GPFifo.readPtr & 0x3FFFFFFF) >> 16;

    PPCSync();

    if (CPGPLinked) {
        CPUFifo.readPtr = GPFifo.readPtr;
        CPUFifo.writePtr = GPFifo.writePtr;
        CPUFifo.count = GPFifo.count;

        __piReg[PI_FIFO_WRITE_PTR] =
            GX_BITSET(0, 3, 24, ((u32)CPUFifo.writePtr & 0x3FFFFFFF) >> 5);

        __GXWriteFifoIntEnable(TRUE, FALSE);
        __GXFifoLink(TRUE);
    }

    // Disable the breakpoint.
    GX_CP_SET_BIT(gxdt->cpCtrlReg, GX_CP_CONTROL_BREAKPOINT, 0);
    GX_CP_SET_BIT(gxdt->cpCtrlReg, GX_CP_CONTROL_BREAKPOINT_INT, 0);
    __cpReg[GX_CP_MMIO_CONTROL] = gxdt->cpCtrlReg;
    __GXCurrentBP = NULL;

    __GXWriteFifoIntReset(TRUE, TRUE);
    __GXFifoReadEnable();

    OSRestoreInterrupts(enabled);
}
