#include <revolution/BASE.h>
#include <revolution/GX.h>
#include <revolution/OS.h>

// 16-bit pixel engine registers at __peReg.
enum {
    PE_ZMODE = 0,
    PE_COLOR_MODE0 = 1,
    PE_COLOR_MODE1 = 2,
    PE_ALPHA_MODE = 3,
    PE_ALPHA_READ = 4,
    PE_INTERRUPT = 5,
    PE_TOKEN = 7,
};

static GXDrawSyncCallback TokenCB;
static GXDrawDoneCallback DrawDoneCB;
static volatile GXBool DrawDone;
static OSThreadQueue FinishQueue;

void GXSetMisc(GXMiscToken token, u32 val) {
    switch (token) {
    case GX_MT_NULL:
        break;
    case GX_MT_XF_FLUSH:
        gxdt->vNum = val;
        gxdt->vNumNot = !gxdt->vNum;
        gxdt->lastWriteWasXF = TRUE;
        if (gxdt->vNum) {
            gxdt->gxDirtyFlags |= GX_DIRTY_VCD;
        }
        break;
    case GX_MT_DL_SAVE_CONTEXT:
        gxdt->dlistSave = val > 0;
        break;
    case GX_MT_ABORT_WAIT_COPYOUT:
        gxdt->abortWaitCopyOut = val > 0;
        break;
    }
}

inline void GXFlush(void) {
    if (gxdt->gxDirtyFlags) {
        __GXSetDirtyState();
    }

    // Push the write-gather pipe out with 32 bytes of padding.
    WGPIPE.i = 0;
    WGPIPE.i = 0;
    WGPIPE.i = 0;
    WGPIPE.i = 0;
    WGPIPE.i = 0;
    WGPIPE.i = 0;
    WGPIPE.i = 0;
    WGPIPE.i = 0;

    PPCSync();
}

static void __GXAbortWait(u32 clocks) {
    OSTime start = OSGetTime();
    OSTime now;

    do {
        now = OSGetTime();
    } while (now - start <= clocks / 4);
}

static void __GXAbortWaitPECopyDone(void) {
    u32 count0, count1;

    count0 = __GXReadMEMCounterU32(MEM_PE_REQ_COUNT_LO, MEM_PE_REQ_COUNT_HI);
    do {
        count1 = count0;
        __GXAbortWait(32);
        count0 =
            __GXReadMEMCounterU32(MEM_PE_REQ_COUNT_LO, MEM_PE_REQ_COUNT_HI);
    } while (count0 != count1);
}

void __GXAbort(void) {
    if (gxdt->abortWaitCopyOut && __GXIsGPFifoReady()) {
        __GXAbortWaitPECopyDone();
    }

    PI_HW_REGS[PI_FIFO_RESET] = 1;
    __GXAbortWait(200);
    PI_HW_REGS[PI_FIFO_RESET] = 0;
    __GXAbortWait(20);
}

void GXAbortFrame(void) {
    __GXAbort();

    if (__GXIsGPFifoReady()) {
        __GXCleanGPFifo();
        __GXInitRevisionBits();
        gxdt->gxDirtyFlags = 0;
        GXFlush();
    }
}

inline void GXSetDrawDone(void) {
    BOOL enabled = OSDisableInterrupts();

    GX_BP_LOAD_REG(GX_BP_REG_DRAWDONE << GX_BP_OPCODE_SHIFT | 2);
    GXFlush();
    DrawDone = FALSE;

    OSRestoreInterrupts(enabled);
}

inline void GXWaitDrawDone(void) {
    BOOL enabled = OSDisableInterrupts();

    while (!DrawDone) {
        OSSleepThread(&FinishQueue);
    }

    OSRestoreInterrupts(enabled);
}

void GXDrawDone(void) {
    GXSetDrawDone();
    GXWaitDrawDone();
}

void GXPixModeSync(void) {
    GX_BP_LOAD_REG(gxdt->zControl);
    gxdt->lastWriteWasXF = FALSE;
}

void GXPokeAlphaMode(GXCompare func, u8 threshold) {
    u32 reg = func << 8 | threshold;
    __peReg[PE_ALPHA_MODE] = reg;
}

void GXPokeAlphaRead(GXAlphaReadMode mode) {
    u32 reg = 0;
    GX_SET_REG_FIELD(reg, 2, 0, mode);
    GX_SET_REG_FIELD(reg, 1, 2, 1);
    __peReg[PE_ALPHA_READ] = reg;
}

void GXPokeAlphaUpdate(GXBool update) {
    u32 reg = __peReg[PE_COLOR_MODE0];
    GX_SET_REG_FIELD(reg, 1, 4, update);
    __peReg[PE_COLOR_MODE0] = reg;
}

void GXPokeBlendMode(GXBlendMode type, GXBlendFactor src, GXBlendFactor dst,
                     GXLogicOp op) {
    u32 reg = __peReg[PE_COLOR_MODE0];
    GX_SET_REG_FIELD(reg, 1, 0, type == GX_BM_BLEND || type == GX_BM_SUBTRACT);
    GX_SET_REG_FIELD(reg, 1, 11, type == GX_BM_SUBTRACT);
    GX_SET_REG_FIELD(reg, 1, 1, type == GX_BM_LOGIC);
    GX_SET_REG_FIELD(reg, 4, 12, op);
    GX_SET_REG_FIELD(reg, 3, 8, src);
    GX_SET_REG_FIELD(reg, 3, 5, dst);
    GX_SET_REG_FIELD(reg, 8, 24, GX_BP_REG_BLENDMODE);
    __peReg[PE_COLOR_MODE0] = reg;
}

void GXPokeColorUpdate(GXBool update) {
    u32 reg = __peReg[PE_COLOR_MODE0];
    GX_SET_REG_FIELD(reg, 1, 3, update);
    __peReg[PE_COLOR_MODE0] = reg;
}

void GXPokeDstAlpha(GXBool enable, u8 alpha) {
    u32 reg = 0;
    GX_SET_REG_FIELD(reg, 8, 0, alpha);
    GX_SET_REG_FIELD(reg, 1, 8, enable);
    __peReg[PE_COLOR_MODE1] = reg;
}

void GXPokeDither(GXBool dither) {
    u32 reg = __peReg[PE_COLOR_MODE0];
    GX_SET_REG_FIELD(reg, 1, 2, dither);
    __peReg[PE_COLOR_MODE0] = reg;
}

void GXPokeZMode(GXBool compareEnable, GXCompare func, GXBool updateEnable) {
    u32 reg = 0;
    GX_SET_REG_FIELD(reg, 1, 0, compareEnable);
    GX_SET_REG_FIELD(reg, 3, 1, func);
    GX_SET_REG_FIELD(reg, 1, 4, updateEnable);
    __peReg[PE_ZMODE] = reg;
}

GXDrawSyncCallback GXSetDrawSyncCallback(GXDrawSyncCallback callback) {
    GXDrawSyncCallback old = TokenCB;
    BOOL enabled = OSDisableInterrupts();
    TokenCB = callback;
    OSRestoreInterrupts(enabled);
    return old;
}

static void GXTokenInterruptHandler(s16 interrupt,
                                    OSContext* context) {
    u16 token;
    OSContext tempContext;
    u32 reg;

    token = __peReg[PE_TOKEN];

    if (TokenCB != NULL) {
        OSClearContext(&tempContext);
        OSSetCurrentContext(&tempContext);
        TokenCB(token);
        OSClearContext(&tempContext);
        OSSetCurrentContext(context);
    }

    // Acknowledge the token interrupt.
    reg = __peReg[PE_INTERRUPT];
    GX_SET_REG_FIELD(reg, 1, 2, 1);
    __peReg[PE_INTERRUPT] = reg;
}

GXDrawDoneCallback GXSetDrawDoneCallback(GXDrawDoneCallback callback) {
    GXDrawDoneCallback old = DrawDoneCB;
    BOOL enabled = OSDisableInterrupts();
    DrawDoneCB = callback;
    OSRestoreInterrupts(enabled);
    return old;
}

static void GXFinishInterruptHandler(s16 interrupt,
                                     OSContext* context) {
    OSContext tempContext;
    u32 reg;

    // Acknowledge the finish interrupt.
    reg = __peReg[PE_INTERRUPT];
    GX_SET_REG_FIELD(reg, 1, 3, 1);
    __peReg[PE_INTERRUPT] = reg;

    DrawDone = TRUE;

    if (DrawDoneCB != NULL) {
        OSClearContext(&tempContext);
        OSSetCurrentContext(&tempContext);
        DrawDoneCB();
        OSClearContext(&tempContext);
        OSSetCurrentContext(context);
    }

    OSWakeupThread(&FinishQueue);
}

void __GXPEInit(void) {
    u32 reg;

    __OSSetInterruptHandler(OS_INTR_PI_PE_TOKEN, GXTokenInterruptHandler);
    __OSSetInterruptHandler(OS_INTR_PI_PE_FINISH, GXFinishInterruptHandler);
    OSInitThreadQueue(&FinishQueue);
    __OSUnmaskInterrupts(OS_INTR_MASK(OS_INTR_PI_PE_TOKEN));
    __OSUnmaskInterrupts(OS_INTR_MASK(OS_INTR_PI_PE_FINISH));

    // Clear pending token/finish interrupts and enable both.
    reg = __peReg[PE_INTERRUPT];
    GX_SET_REG_FIELD(reg, 1, 2, 1);
    GX_SET_REG_FIELD(reg, 1, 3, 1);
    GX_SET_REG_FIELD(reg, 1, 0, 1);
    GX_SET_REG_FIELD(reg, 1, 1, 1);
    __peReg[PE_INTERRUPT] = reg;
}
