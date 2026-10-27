#include <revolution/GX.h>
#include <revolution/VI.h>

// clang-format off
GXRenderModeObj GXNtsc480IntDf = {
    VI_TVMODE_NTSC_INT,
    640, 480, 480,
    40, 0, 640, 480,
    VI_XFBMODE_DF,
    FALSE, FALSE,
    {{6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6},
     {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}},
    {8, 8, 10, 12, 10, 8, 8},
};

GXRenderModeObj GXMpal480IntDf = {
    VI_TVMODE_MPAL_INT,
    640, 480, 480,
    40, 0, 640, 480,
    VI_XFBMODE_DF,
    FALSE, FALSE,
    {{6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6},
     {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}},
    {8, 8, 10, 12, 10, 8, 8},
};

GXRenderModeObj GXPal528IntDf = {
    VI_TVMODE_PAL_INT,
    640, 528, 528,
    40, 23, 640, 528,
    VI_XFBMODE_DF,
    FALSE, FALSE,
    {{6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6},
     {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}},
    {8, 8, 10, 12, 10, 8, 8},
};

GXRenderModeObj GXEurgb60Hz480IntDf = {
    VI_TVMODE_EURGB60_INT,
    640, 480, 480,
    40, 0, 640, 480,
    VI_XFBMODE_DF,
    FALSE, FALSE,
    {{6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6},
     {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}, {6, 6}},
    {8, 8, 10, 12, 10, 8, 8},
};
// clang-format on

void GXSetDispCopySrc(u16 left, u16 top, u16 width, u16 height) {
    gxdt->cpDispSrc = 0;
    GX_SET_REG_FIELD(gxdt->cpDispSrc, 10, 0, left);
    GX_SET_REG_FIELD(gxdt->cpDispSrc, 10, 10, top);
    GX_SET_REG_FIELD(gxdt->cpDispSrc, 8, 24, GX_BP_REG_TEXCOPYSRCXY);

    gxdt->cpDispSize = 0;
    GX_SET_REG_FIELD(gxdt->cpDispSize, 10, 0, width - 1);
    GX_SET_REG_FIELD(gxdt->cpDispSize, 10, 10, height - 1);
    GX_SET_REG_FIELD(gxdt->cpDispSize, 8, 24, GX_BP_REG_TEXCOPYSRCWH);
}

void GXSetTexCopySrc(u16 left, u16 top, u16 width, u16 height) {
    gxdt->cpTexSrc = 0;
    GX_SET_REG_FIELD(gxdt->cpTexSrc, 10, 0, left);
    GX_SET_REG_FIELD(gxdt->cpTexSrc, 10, 10, top);
    GX_SET_REG_FIELD(gxdt->cpTexSrc, 8, 24, GX_BP_REG_TEXCOPYSRCXY);

    gxdt->cpTexSize = 0;
    GX_SET_REG_FIELD(gxdt->cpTexSize, 10, 0, width - 1);
    GX_SET_REG_FIELD(gxdt->cpTexSize, 10, 10, height - 1);
    GX_SET_REG_FIELD(gxdt->cpTexSize, 8, 24, GX_BP_REG_TEXCOPYSRCWH);
}

void GXSetDispCopyDst(u16 width, u16 height) {
    // The XFB is YUYV, two bytes per pixel; the stride is in 32-byte units.
    u16 stride = width * 2;

    gxdt->cpDispStride = 0;
    GX_SET_REG_FIELD(gxdt->cpDispStride, 10, 0, stride >> 5);
    GX_SET_REG_FIELD(gxdt->cpDispStride, 8, 24, GX_BP_REG_DISPCOPYSTRIDE);
}

void GXSetTexCopyDst(u16 width, u16 height, GXTexFmt format, GXBool mipmap) {
    u32 rowTiles, colTiles, cmpTiles;
    u32 peTexFmt;

    gxdt->cpTexZ = FALSE;

    peTexFmt = format & 0xF;
    if (format == GX_TF_Z16) {
        peTexFmt = 0xB;
    }

    switch (format) {
    case GX_TF_I4:
    case GX_TF_I8:
    case GX_TF_IA4:
    case GX_TF_IA8:
    case GX_CTF_YUVA8:
        GX_SET_REG_FIELD(gxdt->cpTex, 2, 15, 3);
        break;
    default:
        GX_SET_REG_FIELD(gxdt->cpTex, 2, 15, 2);
        break;
    }

    // Formats with bit 4 set copy the Z buffer.
    gxdt->cpTexZ = (format & 0x10) == 0x10;

    GX_SET_REG_FIELD(gxdt->cpTex, 1, 3, peTexFmt >> 3);
    peTexFmt &= 7;

    __GetImageTileCount(format, width, height, &rowTiles, &colTiles,
                        &cmpTiles);

    gxdt->cpTexStride = 0;
    GX_SET_REG_FIELD(gxdt->cpTexStride, 10, 0, rowTiles * cmpTiles);
    GX_SET_REG_FIELD(gxdt->cpTexStride, 8, 24, GX_BP_REG_DISPCOPYSTRIDE);

    GX_SET_REG_FIELD(gxdt->cpTex, 1, 9, mipmap);
    GX_SET_REG_FIELD(gxdt->cpTex, 3, 4, peTexFmt);
}

void GXSetDispCopyFrame2Field(GXCopyMode mode) {
    GX_SET_REG_FIELD(gxdt->cpDisp, 2, 12, mode);
    GX_SET_REG_FIELD(gxdt->cpTex, 2, 12, 0);
}

void GXSetCopyClamp(GXCopyClamp clamp) {
    GXBool clampTop = (clamp & GX_CLAMP_TOP) == GX_CLAMP_TOP;
    GXBool clampBottom = (clamp & GX_CLAMP_BOTTOM) == GX_CLAMP_BOTTOM;

    GX_SET_REG_FIELD(gxdt->cpDisp, 1, 0, clampTop);
    GX_SET_REG_FIELD(gxdt->cpDisp, 1, 1, clampBottom);

    GX_SET_REG_FIELD(gxdt->cpTex, 1, 0, clampTop);
    GX_SET_REG_FIELD(gxdt->cpTex, 1, 1, clampBottom);
}

// Number of XFB lines that a copy of efbHeight lines with the given
// 8.8 fixed point Y scale produces.
static u32 __GXGetNumXfbLines(u32 efbHeight, u32 scale) {
    u32 count;
    u32 realHeight;
    u32 divisor;

    count = (efbHeight - 1) * 256;
    realHeight = count / scale + 1;

    divisor = scale;
    if (divisor > 128 && divisor < 256) {
        while (divisor % 2 == 0) {
            divisor /= 2;
        }

        if (efbHeight % divisor == 0) {
            realHeight++;
        }
    }

    if (realHeight > 1024) {
        realHeight = 1024;
    }

    return realHeight;
}

inline u32 GXGetNumXfbLines(u16 efbHeight, f32 scaleY) {
    u32 scale = (u32)(256.0f / scaleY) & 0x1FF;
    return __GXGetNumXfbLines(efbHeight, scale);
}

f32 GXGetYScaleFactor(u16 efbHeight, u16 xfbHeight) {
    f32 bestScale;
    f32 scaleY;
    u32 scale;
    u32 targetHeight;
    u32 realHeight;

    targetHeight = xfbHeight;
    scaleY = (f32)xfbHeight / (f32)efbHeight;
    scale = (u32)(256.0f / scaleY) & 0x1FF;
    realHeight = __GXGetNumXfbLines(efbHeight, scale);

    // Shrink the scale until the copy fits in the XFB...
    while (realHeight > xfbHeight) {
        targetHeight--;
        scaleY = (f32)targetHeight / (f32)efbHeight;
        scale = (u32)(256.0f / scaleY) & 0x1FF;
        realHeight = __GXGetNumXfbLines(efbHeight, scale);
    }

    // ...then grow it back to the largest scale that still fits.
    bestScale = scaleY;
    while (realHeight < xfbHeight) {
        bestScale = scaleY;
        targetHeight++;
        scaleY = (f32)targetHeight / (f32)efbHeight;
        scale = (u32)(256.0f / scaleY) & 0x1FF;
        realHeight = __GXGetNumXfbLines(efbHeight, scale);
    }

    return bestScale;
}

u32 GXSetDispCopyYScale(f32 scaleY) {
    u32 scale;
    GXBool enable;
    u32 cmd;
    u32 height;

    scale = (u32)(256.0f / scaleY) & 0x1FF;
    enable = scale != 256;

    cmd = 0;
    GX_SET_REG_FIELD(cmd, 9, 0, scale);
    GX_SET_REG_FIELD(cmd, 8, 24, GX_BP_REG_DISPCOPYSCALEY);
    GX_BP_LOAD_REG(cmd);
    gxdt->lastWriteWasXF = FALSE;

    GX_SET_REG_FIELD(gxdt->cpDisp, 1, 10, enable);

    height = GX_GET_REG_FIELD(gxdt->cpDispSize, 10, 10) + 1;
    return __GXGetNumXfbLines(height, scale);
}

void GXSetCopyClear(GXColor color, u32 z) {
    u32 cmd;

    cmd = 0;
    GX_SET_REG_FIELD(cmd, 8, 0, color.r);
    GX_SET_REG_FIELD(cmd, 8, 8, color.a);
    GX_SET_REG_FIELD(cmd, 8, 24, GX_BP_REG_COPYCLEARAR);
    GX_BP_LOAD_REG(cmd);

    cmd = 0;
    GX_SET_REG_FIELD(cmd, 8, 0, color.b);
    GX_SET_REG_FIELD(cmd, 8, 8, color.g);
    GX_SET_REG_FIELD(cmd, 8, 24, GX_BP_REG_COPYCLEARGB);
    GX_BP_LOAD_REG(cmd);

    cmd = 0;
    GX_SET_REG_FIELD(cmd, 24, 0, z);
    GX_SET_REG_FIELD(cmd, 8, 24, GX_BP_REG_COPYCLEARZ);
    GX_BP_LOAD_REG(cmd);

    gxdt->lastWriteWasXF = FALSE;
}

void GXSetCopyFilter(GXBool antiAlias, const u8 samplePattern[12][2],
                     GXBool verticalFilter, const u8 vfilter[GX_VFILTER_SZ]) {
    u32 sampleLoc0, sampleLoc1, sampleLoc2, sampleLoc3;
    u32 coeff0, coeff1;

    // Each register holds the sample offsets of three pixels.
    if (antiAlias) {
        sampleLoc0 = 0;
        GX_SET_REG_FIELD(sampleLoc0, 4, 0, samplePattern[0][0]);
        GX_SET_REG_FIELD(sampleLoc0, 4, 4, samplePattern[0][1]);
        GX_SET_REG_FIELD(sampleLoc0, 4, 8, samplePattern[1][0]);
        GX_SET_REG_FIELD(sampleLoc0, 4, 12, samplePattern[1][1]);
        GX_SET_REG_FIELD(sampleLoc0, 4, 16, samplePattern[2][0]);
        GX_SET_REG_FIELD(sampleLoc0, 4, 20, samplePattern[2][1]);
        GX_SET_REG_FIELD(sampleLoc0, 8, 24, 1);

        sampleLoc1 = 0;
        GX_SET_REG_FIELD(sampleLoc1, 4, 0, samplePattern[3][0]);
        GX_SET_REG_FIELD(sampleLoc1, 4, 4, samplePattern[3][1]);
        GX_SET_REG_FIELD(sampleLoc1, 4, 8, samplePattern[4][0]);
        GX_SET_REG_FIELD(sampleLoc1, 4, 12, samplePattern[4][1]);
        GX_SET_REG_FIELD(sampleLoc1, 4, 16, samplePattern[5][0]);
        GX_SET_REG_FIELD(sampleLoc1, 4, 20, samplePattern[5][1]);
        GX_SET_REG_FIELD(sampleLoc1, 8, 24, 2);

        sampleLoc2 = 0;
        GX_SET_REG_FIELD(sampleLoc2, 4, 0, samplePattern[6][0]);
        GX_SET_REG_FIELD(sampleLoc2, 4, 4, samplePattern[6][1]);
        GX_SET_REG_FIELD(sampleLoc2, 4, 8, samplePattern[7][0]);
        GX_SET_REG_FIELD(sampleLoc2, 4, 12, samplePattern[7][1]);
        GX_SET_REG_FIELD(sampleLoc2, 4, 16, samplePattern[8][0]);
        GX_SET_REG_FIELD(sampleLoc2, 4, 20, samplePattern[8][1]);
        GX_SET_REG_FIELD(sampleLoc2, 8, 24, 3);

        sampleLoc3 = 0;
        GX_SET_REG_FIELD(sampleLoc3, 4, 0, samplePattern[9][0]);
        GX_SET_REG_FIELD(sampleLoc3, 4, 4, samplePattern[9][1]);
        GX_SET_REG_FIELD(sampleLoc3, 4, 8, samplePattern[10][0]);
        GX_SET_REG_FIELD(sampleLoc3, 4, 12, samplePattern[10][1]);
        GX_SET_REG_FIELD(sampleLoc3, 4, 16, samplePattern[11][0]);
        GX_SET_REG_FIELD(sampleLoc3, 4, 20, samplePattern[11][1]);
        GX_SET_REG_FIELD(sampleLoc3, 8, 24, 4);
    } else {
        // Every sample at the pixel center (6, 6).
        sampleLoc0 = 0x01666666;
        sampleLoc1 = 0x02666666;
        sampleLoc2 = 0x03666666;
        sampleLoc3 = 0x04666666;
    }

    GX_BP_LOAD_REG(sampleLoc0);
    GX_BP_LOAD_REG(sampleLoc1);
    GX_BP_LOAD_REG(sampleLoc2);
    GX_BP_LOAD_REG(sampleLoc3);

    coeff0 = 0;
    GX_SET_REG_FIELD(coeff0, 8, 24, GX_BP_REG_COPYFILTER0);
    coeff1 = 0;
    GX_SET_REG_FIELD(coeff1, 8, 24, GX_BP_REG_COPYFILTER1);

    if (verticalFilter) {
        GX_SET_REG_FIELD(coeff0, 6, 0, vfilter[0]);
        GX_SET_REG_FIELD(coeff0, 6, 6, vfilter[1]);
        GX_SET_REG_FIELD(coeff0, 6, 12, vfilter[2]);
        GX_SET_REG_FIELD(coeff0, 6, 18, vfilter[3]);
        GX_SET_REG_FIELD(coeff1, 6, 0, vfilter[4]);
        GX_SET_REG_FIELD(coeff1, 6, 6, vfilter[5]);
        GX_SET_REG_FIELD(coeff1, 6, 12, vfilter[6]);
    } else {
        // Pass-through: only the center tap, 21 + 22 + 21 = 64.
        GX_SET_REG_FIELD(coeff0, 6, 0, 0);
        GX_SET_REG_FIELD(coeff0, 6, 6, 0);
        GX_SET_REG_FIELD(coeff0, 6, 12, 21);
        GX_SET_REG_FIELD(coeff0, 6, 18, 22);
        GX_SET_REG_FIELD(coeff1, 6, 0, 21);
        GX_SET_REG_FIELD(coeff1, 6, 6, 0);
        GX_SET_REG_FIELD(coeff1, 6, 12, 0);
    }

    GX_BP_LOAD_REG(coeff0);
    GX_BP_LOAD_REG(coeff1);

    gxdt->lastWriteWasXF = FALSE;
}

void GXSetDispCopyGamma(GXGamma gamma) {
    GX_SET_REG_FIELD(gxdt->cpDisp, 2, 7, gamma);
}

void GXCopyDisp(void* dest, GXBool clear) {
    u32 cmd;
    u32 peControl;
    u32 physAddr;
    GXBool restorePeControl;

    // Clearing writes Z, color and alpha with an always-pass Z test.
    if (clear) {
        cmd = gxdt->zMode;
        GX_SET_REG_FIELD(cmd, 1, 0, 1);
        GX_SET_REG_FIELD(cmd, 3, 1, GX_ALWAYS);
        GX_BP_LOAD_REG(cmd);

        cmd = gxdt->blendMode;
        GX_SET_REG_FIELD(cmd, 1, 0, 0);
        GX_SET_REG_FIELD(cmd, 1, 1, 0);
        GX_BP_LOAD_REG(cmd);
    }

    // Copying must not run with the early Z test of a Z-format EFB.
    restorePeControl = FALSE;
    if ((clear || GX_GET_REG_FIELD(gxdt->zControl, 3, 0) == 3) &&
        GX_GET_REG_FIELD(gxdt->zControl, 1, 6) == 1) {
        restorePeControl = TRUE;
        peControl = gxdt->zControl;
        GX_SET_REG_FIELD(peControl, 1, 6, 0);
        GX_BP_LOAD_REG(peControl);
    }

    GX_BP_LOAD_REG(gxdt->cpDispSrc);
    GX_BP_LOAD_REG(gxdt->cpDispSize);
    GX_BP_LOAD_REG(gxdt->cpDispStride);

    physAddr = (u32)dest & 0x3FFFFFFF;
    cmd = 0;
    GX_SET_REG_FIELD(cmd, 24, 0, physAddr >> 5);
    GX_SET_REG_FIELD(cmd, 8, 24, GX_BP_REG_TEXCOPYDST);
    GX_BP_LOAD_REG(cmd);

    GX_SET_REG_FIELD(gxdt->cpDisp, 1, 11, clear);
    GX_SET_REG_FIELD(gxdt->cpDisp, 1, 14, 1);
    GX_SET_REG_FIELD(gxdt->cpDisp, 8, 24, GX_BP_REG_COPYEXECUTE);
    GX_BP_LOAD_REG(gxdt->cpDisp);

    if (clear) {
        GX_BP_LOAD_REG(gxdt->zMode);
        GX_BP_LOAD_REG(gxdt->blendMode);
    }

    if (restorePeControl) {
        GX_BP_LOAD_REG(gxdt->zControl);
    }

    gxdt->lastWriteWasXF = FALSE;
}

void GXCopyTex(void* dest, GXBool clear) {
    u32 cmd;
    u32 peControl;
    u32 physAddr;
    GXBool restorePeControl;

    if (clear) {
        cmd = gxdt->zMode;
        GX_SET_REG_FIELD(cmd, 1, 0, 1);
        GX_SET_REG_FIELD(cmd, 3, 1, GX_ALWAYS);
        GX_BP_LOAD_REG(cmd);

        cmd = gxdt->blendMode;
        GX_SET_REG_FIELD(cmd, 1, 0, 0);
        GX_SET_REG_FIELD(cmd, 1, 1, 0);
        GX_BP_LOAD_REG(cmd);
    }

    // Z texture copies need the EFB in Z format.
    restorePeControl = FALSE;
    peControl = gxdt->zControl;
    if (gxdt->cpTexZ && GX_GET_REG_FIELD(peControl, 3, 0) != 3) {
        restorePeControl = TRUE;
        GX_SET_REG_FIELD(peControl, 3, 0, 3);
    }

    if ((clear || GX_GET_REG_FIELD(peControl, 3, 0) == 3) &&
        GX_GET_REG_FIELD(peControl, 1, 6) == 1) {
        restorePeControl = TRUE;
        GX_SET_REG_FIELD(peControl, 1, 6, 0);
    }

    if (restorePeControl) {
        GX_BP_LOAD_REG(peControl);
    }

    GX_BP_LOAD_REG(gxdt->cpTexSrc);
    GX_BP_LOAD_REG(gxdt->cpTexSize);
    GX_BP_LOAD_REG(gxdt->cpTexStride);

    physAddr = (u32)dest & 0x3FFFFFFF;
    cmd = 0;
    GX_SET_REG_FIELD(cmd, 24, 0, physAddr >> 5);
    GX_SET_REG_FIELD(cmd, 8, 24, GX_BP_REG_TEXCOPYDST);
    GX_BP_LOAD_REG(cmd);

    GX_SET_REG_FIELD(gxdt->cpTex, 1, 11, clear);
    GX_SET_REG_FIELD(gxdt->cpTex, 1, 14, 0);
    GX_SET_REG_FIELD(gxdt->cpTex, 8, 24, GX_BP_REG_COPYEXECUTE);
    GX_BP_LOAD_REG(gxdt->cpTex);

    if (clear) {
        GX_BP_LOAD_REG(gxdt->zMode);
        GX_BP_LOAD_REG(gxdt->blendMode);
    }

    if (restorePeControl) {
        GX_BP_LOAD_REG(gxdt->zControl);
    }

    gxdt->lastWriteWasXF = FALSE;
}

void GXClearBoundingBox(void) {
    // Reset left/right and top/bottom to (0x3FF, 0).
    GX_BP_LOAD_REG(GX_BP_REG_BOUNDINGBOX0 << GX_BP_OPCODE_SHIFT | 0x3FF);
    GX_BP_LOAD_REG(GX_BP_REG_BOUNDINGBOX1 << GX_BP_OPCODE_SHIFT | 0x3FF);
    gxdt->lastWriteWasXF = FALSE;
}
