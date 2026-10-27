#include <revolution/BASE.h>
#include <revolution/GX.h>
#include <revolution/OS.h>
#include <revolution/VI.h>

const char* __GXVersion =
    "<< RVL_SDK - GX \trelease build: May  8 2007 12:59:16 (0x4199_60831) >>";

// A 4x4 opaque white IA8 texture for the unused texture maps.
static u8 GXDefaultTexData[32] ATTRIBUTE_ALIGN(32) = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
};
static GXVtxAttrFmtList GXDefaultVATList[] = {
    {GX_VA_POS, GX_POS_XYZ, GX_F32, 0},
    {GX_VA_NRM, GX_NRM_XYZ, GX_F32, 0},
    {GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0},
    {GX_VA_CLR1, GX_CLR_RGBA, GX_RGBA8, 0},
    {GX_VA_TEX0, GX_TEX_ST, GX_F32, 0},
    {GX_VA_TEX1, GX_TEX_ST, GX_F32, 0},
    {GX_VA_TEX2, GX_TEX_ST, GX_F32, 0},
    {GX_VA_TEX3, GX_TEX_ST, GX_F32, 0},
    {GX_VA_TEX4, GX_TEX_ST, GX_F32, 0},
    {GX_VA_TEX5, GX_TEX_ST, GX_F32, 0},
    {GX_VA_TEX6, GX_TEX_ST, GX_F32, 0},
    {GX_VA_TEX7, GX_TEX_ST, GX_F32, 0},
    {GX_VA_NULL, 0, 0, 0},
};
// Default projection; element 0 is the type (GX_ORTHOGRAPHIC).
static f32 GXDefaultProjData[GX_PROJECTION_SZ] = {
    1.0f, 0.0f, 1.0f, 0.0f, -1.0f, -2.0f, 0.0f,
};

static GXFifoObj FifoObj;
static GXData gxData;
GXData* const __GXData = &gxData;

volatile u32* __piReg = NULL;
volatile u16* __cpReg = NULL;
volatile u16* __peReg = NULL;
volatile u16* __memReg = NULL;

// TMEM addresses (even, odd) of each map's cache region for the three default
// region sets: TexRegions0, TexRegions1 and the 32-bit mipmap TexRegions2.
static u32 GXTexRegionAddrTable[] = {
    0x00000, 0x10000, 0x20000, 0x30000, 0x40000, 0x50000, 0x60000, 0x70000,
    0x08000, 0x18000, 0x28000, 0x38000, 0x48000, 0x58000, 0x68000, 0x78000,
    0x00000, 0x90000, 0x20000, 0xB0000, 0x40000, 0x98000, 0x60000, 0xB8000,
    0x80000, 0x10000, 0xA0000, 0x30000, 0x88000, 0x50000, 0xA8000, 0x70000,
    0x00000, 0x90000, 0x20000, 0xB0000, 0x40000, 0x90000, 0x60000, 0xB0000,
    0x80000, 0x10000, 0xA0000, 0x30000, 0x80000, 0x50000, 0xA0000, 0x70000,
};

static GXTexRegion* __GXDefaultTexRegionCallback(const GXTexObj* obj,
                                                 GXTexMapID id) {
    GXTexFmt format = GXGetTexObjFmt(obj);
    GXBool mipmap = GXGetTexObjMipMap(obj);

    id = (GXTexMapID)(id % GX_MAX_TEXMAP);

    switch (format) {
    case GX_TF_RGBA8:
        if (mipmap) {
            return &gxdt->TexRegions2[id];
        }
        return &gxdt->TexRegions1[id];
    case GX_TF_C4:
    case GX_TF_C8:
    case GX_TF_C14X2:
        return &gxdt->TexRegions0[id];
    default:
        if (mipmap) {
            return &gxdt->TexRegions1[id];
        }
        return &gxdt->TexRegions0[id];
    }
}

static GXTlutRegion* __GXDefaultTlutRegionCallback(u32 tlutName) {
    if (tlutName >= 20) {
        return NULL;
    }

    return &gxdt->TlutRegions[tlutName];
}

// Shutdown waits until the PE has stopped writing before aborting the GP.
static BOOL __GXShutdown(BOOL final, u32 event) {
    static u32 peCount;
    static OSTime time;
    static BOOL calledOnce = FALSE;

    u32 newPeCount;
    OSTime now;

    if (!final) {
        if (!calledOnce) {
            peCount = __GXReadMEMCounterU32(MEM_PE_REQ_COUNT_LO,
                                            MEM_PE_REQ_COUNT_HI);
            time = OSGetTime();
            calledOnce = TRUE;
            return FALSE;
        }

        now = OSGetTime();
        newPeCount =
            __GXReadMEMCounterU32(MEM_PE_REQ_COUNT_LO, MEM_PE_REQ_COUNT_HI);

        if (now - time < 10) {
            return FALSE;
        }

        if (newPeCount != peCount) {
            peCount = newPeCount;
            time = now;
            return FALSE;
        }
    } else {
        GXSetBreakPtCallback(NULL);
        GXSetDrawSyncCallback(NULL);
        GXSetDrawDoneCallback(NULL);

        WGPIPE.i = 0;
        WGPIPE.i = 0;
        WGPIPE.i = 0;
        WGPIPE.i = 0;
        WGPIPE.i = 0;
        WGPIPE.i = 0;
        WGPIPE.i = 0;
        WGPIPE.i = 0;
        PPCSync();

        __cpReg[GX_CP_MMIO_CONTROL] = 0;
        __cpReg[GX_CP_MMIO_CLEAR] = 3;

        gxdt->abortWaitCopyOut = TRUE;
        __GXAbort();
    }

    return TRUE;
}

static OSShutdownFunctionInfo GXShutdownFuncInfo = {__GXShutdown, 127};

void __GXInitRevisionBits(void) {
    u32 i;
    u32 xfReg;
    u32 xfPerf;
    u32 bpReg;

    // Mark every vertex format as using the new attribute layout.
    for (i = 0; i < GX_MAX_VTXFMT; i++) {
        GX_SET_REG_FIELD(gxdt->vatA[i], 1, 30, 1);
        GX_SET_REG_FIELD(gxdt->vatB[i], 1, 31, 1);
        GX_CP_LOAD_REG(GX_CP_REG_VAT_GROUP1 | i, gxdt->vatB[i]);
    }

    xfReg = 0;
    GX_SET_REG_FIELD(xfReg, 1, 0, 1);
    GX_SET_REG_FIELD(xfReg, 1, 1, 1);
    GX_SET_REG_FIELD(xfReg, 1, 2, 1);
    GX_SET_REG_FIELD(xfReg, 1, 3, 1);
    GX_SET_REG_FIELD(xfReg, 1, 4, 1);
    GX_SET_REG_FIELD(xfReg, 1, 5, 1);
    GX_XF_LOAD_REG(0x1000, xfReg);

    xfPerf = 0;
    GX_SET_REG_FIELD(xfPerf, 1, 0, 1);
    GX_XF_LOAD_REG(0x1012, xfPerf);

    bpReg = 0;
    GX_SET_REG_FIELD(bpReg, 1, 0, 1);
    GX_SET_REG_FIELD(bpReg, 1, 1, 1);
    GX_SET_REG_FIELD(bpReg, 1, 2, 1);
    GX_SET_REG_FIELD(bpReg, 1, 3, 1);
    GX_SET_REG_FIELD(bpReg, 8, 24, 0x58);
    GX_BP_LOAD_REG(bpReg);
}

static inline void EnableWriteGatherPipe(void) {
    u32 hid2 = PPCMfhid2();
    PPCMtwpar((u32)OSUncachedToPhysical((void*)0xCC008000));
    PPCMthid2(hid2 | 0x40000000);
}

GXFifoObj* GXInit(void* base, u32 size) {
    static BOOL shutdownFuncRegistered = FALSE;
    u32 i;
    u32 freqBase;
    u32 reg;

    OSRegisterVersion(__GXVersion);

    gxdt->dlistActive = FALSE;
    gxdt->dlistSave = TRUE;
    gxdt->abortWaitCopyOut = TRUE;
    gxdt->tcsManEnab = 0;
    gxdt->tevTcEnab = 0;

    GXSetMisc(GX_MT_XF_FLUSH, 0);

    __piReg = (volatile u32*)OSPhysicalToUncached(0x0C003000);
    __cpReg = (volatile u16*)OSPhysicalToUncached(0x0C000000);
    __peReg = (volatile u16*)OSPhysicalToUncached(0x0C001000);
    __memReg = (volatile u16*)OSPhysicalToUncached(0x0C004000);

    __GXFifoInit();
    GXInitFifoBase(&FifoObj, base, size);
    GXSetCPUFifo(&FifoObj);
    GXSetGPFifo(&FifoObj);

    if (!shutdownFuncRegistered) {
        OSRegisterShutdownFunction(&GXShutdownFuncInfo);
        shutdownFuncRegistered = TRUE;
    }

    __GXPEInit();
    EnableWriteGatherPipe();

    gxdt->genMode = 0;
    GX_SET_REG_FIELD(gxdt->genMode, 8, 24, GX_BP_REG_GENMODE);
    gxdt->ind_imask = 0xFF;
    GX_SET_REG_FIELD(gxdt->ind_imask, 8, 24, GX_BP_REG_INDIMASK);
    gxdt->linePtWidth = 0;
    GX_SET_REG_FIELD(gxdt->linePtWidth, 8, 24, GX_BP_REG_LINEPTWIDTH);

    for (i = 0; i < GX_MAX_TEVSTAGE; i++) {
        gxdt->tevc[i] = 0;
        gxdt->teva[i] = 0;
        gxdt->tref[i / 2] = 0;
        gxdt->texmapId[i] = GX_TEXMAP_NULL;
        GX_SET_REG_FIELD(gxdt->tevc[i], 8, 24, GX_BP_REG_TEVCOLORCOMBINER0 + i * 2);
        GX_SET_REG_FIELD(gxdt->teva[i], 8, 24, GX_BP_REG_TEVALPHACOMBINER0 + i * 2);
        GX_SET_REG_FIELD(gxdt->tevKsel[i / 2], 8, 24, GX_BP_REG_TEVKSEL0 + i / 2);
        GX_SET_REG_FIELD(gxdt->tref[i / 2], 8, 24, GX_BP_REG_RAS1_TREF0 + i / 2);
    }

    gxdt->ras1_iref = 0;
    GX_SET_REG_FIELD(gxdt->ras1_iref, 8, 24, GX_BP_REG_RAS1_IREF);

    for (i = 0; i < GX_MAX_TEXCOORD; i++) {
        gxdt->suTs0[i] = 0;
        gxdt->suTs1[i] = 0;
        GX_SET_REG_FIELD(gxdt->suTs0[i], 8, 24, GX_BP_REG_SU_SSIZE0 + i * 2);
        GX_SET_REG_FIELD(gxdt->suTs1[i], 8, 24, GX_BP_REG_SU_TSIZE0 + i * 2);
    }

    GX_SET_REG_FIELD(gxdt->scissorTL, 8, 24, GX_BP_REG_SCISSORTL);
    GX_SET_REG_FIELD(gxdt->scissorBR, 8, 24, GX_BP_REG_SCISSORBR);
    GX_SET_REG_FIELD(gxdt->blendMode, 8, 24, GX_BP_REG_BLENDMODE);
    GX_SET_REG_FIELD(gxdt->dstAlpha, 8, 24, GX_BP_REG_DSTALPHA);
    GX_SET_REG_FIELD(gxdt->zMode, 8, 24, GX_BP_REG_ZMODE);
    GX_SET_REG_FIELD(gxdt->zControl, 8, 24, GX_BP_REG_ZCONTROL);
    GX_SET_REG_FIELD(gxdt->cpTex, 2, 7, 0);

    gxdt->scaleZ = 16777216.0f;
    gxdt->offsetZ = 0.0f;
    gxdt->gxDirtyFlags = 0;
    gxdt->vatDirtyFlags = 0;

    // Texture cache refresh rate, derived from the bus clock.
    freqBase = OS_BUS_CLOCK_SPEED / 500;
    __GXFlushTextureState();
    reg = freqBase >> 11 | 0x400 | 0x69000000;
    GX_BP_LOAD_REG(reg);
    __GXFlushTextureState();
    reg = freqBase / 4224 | 0x200 | 0x46000000;
    GX_BP_LOAD_REG(reg);

    __GXInitRevisionBits();

    for (i = 0; i < GX_MAX_TEXMAP; i++) {
        GXInitTexCacheRegion(&gxdt->TexRegions0[i], FALSE,
                             GXTexRegionAddrTable[i], GX_TEXCACHE_32K,
                             GXTexRegionAddrTable[i + 8], GX_TEXCACHE_32K);
        GXInitTexCacheRegion(&gxdt->TexRegions1[i], FALSE,
                             GXTexRegionAddrTable[i + 16], GX_TEXCACHE_32K,
                             GXTexRegionAddrTable[i + 24], GX_TEXCACHE_32K);
        GXInitTexCacheRegion(&gxdt->TexRegions2[i], TRUE,
                             GXTexRegionAddrTable[i + 32], GX_TEXCACHE_32K,
                             GXTexRegionAddrTable[i + 40], GX_TEXCACHE_32K);
    }

    for (i = 0; i < 16; i++) {
        GXInitTlutRegion(&gxdt->TlutRegions[i], 0xC0000 + 0x2000 * i,
                         GX_TLUT_256);
    }

    for (i = 0; i < 4; i++) {
        GXInitTlutRegion(&gxdt->TlutRegions[i + 16], 0xE0000 + 0x8000 * i,
                         GX_TLUT_1K);
    }

    // Reset the performance counters.
    __cpReg[GX_CP_MMIO_PERF_SELECT] = 0;
    GX_SET_REG_FIELD(gxdt->perfSel, 4, 4, 0);
    GX_CP_LOAD_REG(GX_CP_REG_PERFSELECT, gxdt->perfSel);
    GX_XF_LOAD_REG(GX_XF_REG_PERF0, 0);
    GX_BP_LOAD_REG(GX_BP_REG_PERF0TRI << GX_BP_OPCODE_SHIFT);
    GX_BP_LOAD_REG(GX_BP_REG_PERF0QUAD << GX_BP_OPCODE_SHIFT);
    GX_BP_LOAD_REG(GX_BP_REG_PERF1 << GX_BP_OPCODE_SHIFT);

    __GXSetIndirectMask(0);
    __GXSetTmemConfig(2);
    __GXInitGX();

    return &FifoObj;
}

void __GXInitGX(void) {
    GXRenderModeObj* rmode;
    GXTexObj texObj;
    Mtx identity;
    GXColor clear = {64, 64, 64, 255};
    GXColor black = {0, 0, 0, 0};
    GXColor white = {255, 255, 255, 255};
    u32 i;

    switch (VIGetTvFormat()) {
    case VI_TVFORMAT_NTSC:
        rmode = &GXNtsc480IntDf;
        break;
    case VI_TVFORMAT_PAL:
        rmode = &GXPal528IntDf;
        break;
    case VI_TVFORMAT_EURGB60:
        rmode = &GXEurgb60Hz480IntDf;
        break;
    case VI_TVFORMAT_MPAL:
        rmode = &GXMpal480IntDf;
        break;
    default:
        rmode = &GXNtsc480IntDf;
        break;
    }

    GXSetCopyClear(clear, 0xFFFFFF);

    GXSetTexCoordGen2(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY, FALSE, GX_PTIDENTITY);
    GXSetTexCoordGen2(GX_TEXCOORD1, GX_TG_MTX2x4, GX_TG_TEX1, GX_IDENTITY, FALSE, GX_PTIDENTITY);
    GXSetTexCoordGen2(GX_TEXCOORD2, GX_TG_MTX2x4, GX_TG_TEX2, GX_IDENTITY, FALSE, GX_PTIDENTITY);
    GXSetTexCoordGen2(GX_TEXCOORD3, GX_TG_MTX2x4, GX_TG_TEX3, GX_IDENTITY, FALSE, GX_PTIDENTITY);
    GXSetTexCoordGen2(GX_TEXCOORD4, GX_TG_MTX2x4, GX_TG_TEX4, GX_IDENTITY, FALSE, GX_PTIDENTITY);
    GXSetTexCoordGen2(GX_TEXCOORD5, GX_TG_MTX2x4, GX_TG_TEX5, GX_IDENTITY, FALSE, GX_PTIDENTITY);
    GXSetTexCoordGen2(GX_TEXCOORD6, GX_TG_MTX2x4, GX_TG_TEX6, GX_IDENTITY, FALSE, GX_PTIDENTITY);
    GXSetTexCoordGen2(GX_TEXCOORD7, GX_TG_MTX2x4, GX_TG_TEX7, GX_IDENTITY, FALSE, GX_PTIDENTITY);
    GXSetNumTexGens(1);

    GXClearVtxDesc();
    GXInvalidateVtxCache();

    // Point every array at something valid.
    for (i = GX_VA_POS; i <= GX_LIGHT_ARRAY; i++) {
        GXSetArray((GXAttr)i, gxdt, 0);
    }

    for (i = 0; i < GX_MAX_VTXFMT; i++) {
        GXSetVtxAttrFmtv((GXVtxFmt)i, GXDefaultVATList);
    }

    GXSetLineWidth(6, GX_TO_ZERO);
    GXSetPointSize(6, GX_TO_ZERO);
    GXEnableTexOffsets(GX_TEXCOORD0, FALSE, FALSE);
    GXEnableTexOffsets(GX_TEXCOORD1, FALSE, FALSE);
    GXEnableTexOffsets(GX_TEXCOORD2, FALSE, FALSE);
    GXEnableTexOffsets(GX_TEXCOORD3, FALSE, FALSE);
    GXEnableTexOffsets(GX_TEXCOORD4, FALSE, FALSE);
    GXEnableTexOffsets(GX_TEXCOORD5, FALSE, FALSE);
    GXEnableTexOffsets(GX_TEXCOORD6, FALSE, FALSE);
    GXEnableTexOffsets(GX_TEXCOORD7, FALSE, FALSE);

    identity[0][0] = 1.0f;
    identity[0][1] = 0.0f;
    identity[0][2] = 0.0f;
    identity[0][3] = 0.0f;
    identity[1][0] = 0.0f;
    identity[1][1] = 1.0f;
    identity[1][2] = 0.0f;
    identity[1][3] = 0.0f;
    identity[2][0] = 0.0f;
    identity[2][1] = 0.0f;
    identity[2][2] = 1.0f;
    identity[2][3] = 0.0f;
    GXLoadPosMtxImm(identity, GX_PNMTX0);
    GXLoadNrmMtxImm(identity, GX_PNMTX0);
    GXSetCurrentMtx(GX_PNMTX0);
    GXLoadTexMtxImm(identity, GX_IDENTITY, GX_MTX_3x4);
    GXLoadTexMtxImm(identity, GX_PTIDENTITY, GX_MTX_3x4);

    GXSetViewport(0.0f, 0.0f, rmode->fbWidth, rmode->xfbHeight, 0.0f, 1.0f);
    GXSetProjectionv(GXDefaultProjData);

    __GXSetGenMode();
    GXSetCoPlanar(FALSE);
    GXSetCullMode(GX_CULL_BACK);
    GXSetClipMode(GX_CLIP_ENABLE);
    GXSetScissor(0, 0, rmode->fbWidth, rmode->efbHeight);
    GXSetScissorBoxOffset(0, 0);

    GXSetNumChans(0);
    GXSetChanCtrl(GX_COLOR0A0, FALSE, GX_SRC_REG, GX_SRC_VTX, GX_LIGHT_NULL, GX_DF_NONE, GX_AF_NONE);
    GXSetChanAmbColor(GX_COLOR0A0, black);
    GXSetChanMatColor(GX_COLOR0A0, white);
    GXSetChanCtrl(GX_COLOR1A1, FALSE, GX_SRC_REG, GX_SRC_VTX, GX_LIGHT_NULL, GX_DF_NONE, GX_AF_NONE);
    GXSetChanAmbColor(GX_COLOR1A1, black);
    GXSetChanMatColor(GX_COLOR1A1, white);

    GXInvalidateTexAll();
    GXSetTexRegionCallback(__GXDefaultTexRegionCallback);
    GXSetTlutRegionCallback(__GXDefaultTlutRegionCallback);

    GXInitTexObj(&texObj, GXDefaultTexData, 4, 4, GX_TF_IA8, GX_CLAMP, GX_CLAMP, FALSE);
    GXLoadTexObj(&texObj, GX_TEXMAP0);
    GXLoadTexObj(&texObj, GX_TEXMAP1);
    GXLoadTexObj(&texObj, GX_TEXMAP2);
    GXLoadTexObj(&texObj, GX_TEXMAP3);
    GXLoadTexObj(&texObj, GX_TEXMAP4);
    GXLoadTexObj(&texObj, GX_TEXMAP5);
    GXLoadTexObj(&texObj, GX_TEXMAP6);
    GXLoadTexObj(&texObj, GX_TEXMAP7);

    GXSetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0);
    GXSetTevOrder(GX_TEVSTAGE1, GX_TEXCOORD1, GX_TEXMAP1, GX_COLOR0A0);
    GXSetTevOrder(GX_TEVSTAGE2, GX_TEXCOORD2, GX_TEXMAP2, GX_COLOR0A0);
    GXSetTevOrder(GX_TEVSTAGE3, GX_TEXCOORD3, GX_TEXMAP3, GX_COLOR0A0);
    GXSetTevOrder(GX_TEVSTAGE4, GX_TEXCOORD4, GX_TEXMAP4, GX_COLOR0A0);
    GXSetTevOrder(GX_TEVSTAGE5, GX_TEXCOORD5, GX_TEXMAP5, GX_COLOR0A0);
    GXSetTevOrder(GX_TEVSTAGE6, GX_TEXCOORD6, GX_TEXMAP6, GX_COLOR0A0);
    GXSetTevOrder(GX_TEVSTAGE7, GX_TEXCOORD7, GX_TEXMAP7, GX_COLOR0A0);
    GXSetTevOrder(GX_TEVSTAGE8, GX_TEXCOORD_NULL, GX_TEXMAP_NULL, GX_COLOR_NULL);
    GXSetTevOrder(GX_TEVSTAGE9, GX_TEXCOORD_NULL, GX_TEXMAP_NULL, GX_COLOR_NULL);
    GXSetTevOrder(GX_TEVSTAGE10, GX_TEXCOORD_NULL, GX_TEXMAP_NULL, GX_COLOR_NULL);
    GXSetTevOrder(GX_TEVSTAGE11, GX_TEXCOORD_NULL, GX_TEXMAP_NULL, GX_COLOR_NULL);
    GXSetTevOrder(GX_TEVSTAGE12, GX_TEXCOORD_NULL, GX_TEXMAP_NULL, GX_COLOR_NULL);
    GXSetTevOrder(GX_TEVSTAGE13, GX_TEXCOORD_NULL, GX_TEXMAP_NULL, GX_COLOR_NULL);
    GXSetTevOrder(GX_TEVSTAGE14, GX_TEXCOORD_NULL, GX_TEXMAP_NULL, GX_COLOR_NULL);
    GXSetTevOrder(GX_TEVSTAGE15, GX_TEXCOORD_NULL, GX_TEXMAP_NULL, GX_COLOR_NULL);
    GXSetNumTevStages(1);
    GXSetTevOp(GX_TEVSTAGE0, GX_REPLACE);
    GXSetAlphaCompare(GX_ALWAYS, 0, GX_AOP_AND, GX_ALWAYS, 0);
    GXSetZTexture(GX_ZT_DISABLE, GX_TF_Z8, 0);

    for (i = 0; i < GX_MAX_TEVSTAGE; i++) {
        GXSetTevKColorSel((GXTevStageID)i, GX_TEV_KCSEL_1_4);
        GXSetTevKAlphaSel((GXTevStageID)i, GX_TEV_KASEL_1);
        GXSetTevSwapMode((GXTevStageID)i, GX_TEV_SWAP0, GX_TEV_SWAP0);
    }

    GXSetTevSwapModeTable(GX_TEV_SWAP0, GX_CH_RED, GX_CH_GREEN, GX_CH_BLUE, GX_CH_ALPHA);
    GXSetTevSwapModeTable(GX_TEV_SWAP1, GX_CH_RED, GX_CH_RED, GX_CH_RED, GX_CH_ALPHA);
    GXSetTevSwapModeTable(GX_TEV_SWAP2, GX_CH_GREEN, GX_CH_GREEN, GX_CH_GREEN, GX_CH_ALPHA);
    GXSetTevSwapModeTable(GX_TEV_SWAP3, GX_CH_BLUE, GX_CH_BLUE, GX_CH_BLUE, GX_CH_ALPHA);

    for (i = 0; i < GX_MAX_TEVSTAGE; i++) {
        GXSetTevDirect((GXTevStageID)i);
    }

    GXSetNumIndStages(0);
    GXSetIndTexCoordScale(GX_INDTEXSTAGE0, GX_ITS_1, GX_ITS_1);
    GXSetIndTexCoordScale(GX_INDTEXSTAGE1, GX_ITS_1, GX_ITS_1);
    GXSetIndTexCoordScale(GX_INDTEXSTAGE2, GX_ITS_1, GX_ITS_1);
    GXSetIndTexCoordScale(GX_INDTEXSTAGE3, GX_ITS_1, GX_ITS_1);

    GXSetFog(GX_FOG_NONE, black, 0.0f, 1.0f, 0.1f, 1.0f);
    GXSetFogRangeAdj(FALSE, 0, NULL);
    GXSetBlendMode(GX_BM_NONE, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_CLEAR);
    GXSetColorUpdate(TRUE);
    GXSetAlphaUpdate(TRUE);
    GXSetZMode(TRUE, GX_LEQUAL, TRUE);
    GXSetZCompLoc(TRUE);
    GXSetDither(TRUE);
    GXSetDstAlpha(FALSE, 0);
    GXSetPixelFmt(GX_PF_RGB8_Z24, GX_ZC_LINEAR);
    GXSetFieldMask(TRUE, TRUE);
    GXSetFieldMode(rmode->field_rendering,
                   rmode->viHeight == 2 * rmode->xfbHeight ? TRUE : FALSE);

    GXSetDispCopySrc(0, 0, rmode->fbWidth, rmode->efbHeight);
    GXSetDispCopyDst(rmode->fbWidth, rmode->efbHeight);
    GXSetDispCopyYScale((f32)rmode->xfbHeight / (f32)rmode->efbHeight);
    GXSetCopyClamp(GX_CLAMP_ALL);
    GXSetCopyFilter(rmode->aa, rmode->sample_pattern, TRUE, rmode->vfilter);
    GXSetDispCopyGamma(GX_GM_1_0);
    GXSetDispCopyFrame2Field(GX_COPY_PROGRESSIVE);
    GXClearBoundingBox();

    GXPokeColorUpdate(TRUE);
    GXPokeAlphaUpdate(TRUE);
    GXPokeDither(FALSE);
    GXPokeBlendMode(GX_BM_NONE, GX_BL_ZERO, GX_BL_ONE, GX_LO_SET);
    GXPokeAlphaMode(GX_ALWAYS, 0);
    GXPokeAlphaRead(GX_READ_FF);
    GXPokeDstAlpha(FALSE, 0);
    GXPokeZMode(TRUE, GX_ALWAYS, TRUE);

    GXSetGPMetric(GX_PERF0_NONE, GX_PERF1_NONE);
    GXClearGPMetric();
}
