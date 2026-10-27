#include <revolution/GX.h>

// Preset combiner settings for GXSetTevOp, indexed by GXTevMode. Stage 0 has
// no previous stage, so its presets read the rasterized color instead of
// GX_CC_CPREV/GX_CA_APREV. The top byte (the BP opcode) is not used.
static u32 TEVCOpTableST0[] = {
    0xC008F8AF, // GX_MODULATE
    0xC008A89F, // GX_DECAL
    0xC008AC8F, // GX_BLEND
    0xC008FFF8, // GX_REPLACE
    0xC008FFFA, // GX_PASSCLR
};

static u32 TEVCOpTableST1[] = {
    0xC008F80F, // GX_MODULATE
    0xC008089F, // GX_DECAL
    0xC0080C8F, // GX_BLEND
    0xC008FFF8, // GX_REPLACE
    0xC008FFF0, // GX_PASSCLR
};

static u32 TEVAOpTableST0[] = {
    0xC108F2F0, // GX_MODULATE
    0xC108FFD0, // GX_DECAL
    0xC108F2F0, // GX_BLEND
    0xC108FFC0, // GX_REPLACE
    0xC108FFD0, // GX_PASSCLR
};

static u32 TEVAOpTableST1[] = {
    0xC108F070, // GX_MODULATE
    0xC108FF80, // GX_DECAL
    0xC108F070, // GX_BLEND
    0xC108FFC0, // GX_REPLACE
    0xC108FF80, // GX_PASSCLR
};

void GXSetTevOp(GXTevStageID stage, GXTevMode mode) {
    u32* colorOp;
    u32* alphaOp;
    u32 tevReg;

    if (stage == GX_TEVSTAGE0) {
        colorOp = &TEVCOpTableST0[mode];
        alphaOp = &TEVAOpTableST0[mode];
    } else {
        colorOp = &TEVCOpTableST1[mode];
        alphaOp = &TEVAOpTableST1[mode];
    }

    // Keep the opcode byte of the color combiner.
    tevReg = gxdt->tevc[stage];
    tevReg = (*colorOp & ~0xFF000000) | (tevReg & 0xFF000000);
    GX_BP_LOAD_REG(tevReg);
    gxdt->tevc[stage] = tevReg;

    // Keep the opcode byte and the swap selects of the alpha combiner.
    tevReg = gxdt->teva[stage];
    tevReg = (*alphaOp & ~0xFF00000F) | (tevReg & 0xFF00000F);
    GX_BP_LOAD_REG(tevReg);
    gxdt->teva[stage] = tevReg;

    gxdt->lastWriteWasXF = FALSE;
}

void GXSetTevColorIn(GXTevStageID stage, GXTevColorArg a, GXTevColorArg b,
                     GXTevColorArg c, GXTevColorArg d) {
    u32 tevReg = gxdt->tevc[stage];
    GX_BP_SET_TEVCOLORCOMBINER_A(tevReg, a);
    GX_BP_SET_TEVCOLORCOMBINER_B(tevReg, b);
    GX_BP_SET_TEVCOLORCOMBINER_C(tevReg, c);
    GX_BP_SET_TEVCOLORCOMBINER_D(tevReg, d);

    GX_BP_LOAD_REG(tevReg);
    gxdt->tevc[stage] = tevReg;
    gxdt->lastWriteWasXF = FALSE;
}

void GXSetTevAlphaIn(GXTevStageID stage, GXTevAlphaArg a, GXTevAlphaArg b,
                     GXTevAlphaArg c, GXTevAlphaArg d) {
    u32 tevReg = gxdt->teva[stage];
    GX_BP_SET_TEVALPHACOMBINER_A(tevReg, a);
    GX_BP_SET_TEVALPHACOMBINER_B(tevReg, b);
    GX_BP_SET_TEVALPHACOMBINER_C(tevReg, c);
    GX_BP_SET_TEVALPHACOMBINER_D(tevReg, d);

    GX_BP_LOAD_REG(tevReg);
    gxdt->teva[stage] = tevReg;
    gxdt->lastWriteWasXF = FALSE;
}

void GXSetTevColorOp(GXTevStageID stage, GXTevOp op, GXTevBias bias,
                     GXTevScale scale, GXBool clamp, GXTevRegID out) {
    u32 tevReg = gxdt->tevc[stage];
    GX_BP_SET_TEVCOLORCOMBINER_OP_OR_COMPARISON(tevReg, op & 1);

    if (op <= GX_TEV_SUB) {
        GX_BP_SET_TEVCOLORCOMBINER_SCALE_OR_COMPARE_MODE(tevReg, scale);
        GX_BP_SET_TEVCOLORCOMBINER_BIAS(tevReg, bias);
    } else {
        // Compare ops encode the compare mode in the scale field and use the
        // reserved bias value 3.
        GX_BP_SET_TEVCOLORCOMBINER_SCALE_OR_COMPARE_MODE(tevReg, (op >> 1) & 3);
        GX_BP_SET_TEVCOLORCOMBINER_BIAS(tevReg, GX_MAX_TEVBIAS);
    }

    GX_BP_SET_TEVCOLORCOMBINER_CLAMP(tevReg, clamp);
    GX_BP_SET_TEVCOLORCOMBINER_DEST(tevReg, out);

    GX_BP_LOAD_REG(tevReg);
    gxdt->tevc[stage] = tevReg;
    gxdt->lastWriteWasXF = FALSE;
}

void GXSetTevAlphaOp(GXTevStageID stage, GXTevOp op, GXTevBias bias,
                     GXTevScale scale, GXBool clamp, GXTevRegID out) {
    u32 tevReg = gxdt->teva[stage];
    GX_BP_SET_TEVALPHACOMBINER_OP_OR_COMPARISON(tevReg, op & 1);

    if (op <= GX_TEV_SUB) {
        GX_BP_SET_TEVALPHACOMBINER_SCALE_OR_COMPARE_MODE(tevReg, scale);
        GX_BP_SET_TEVALPHACOMBINER_BIAS(tevReg, bias);
    } else {
        GX_BP_SET_TEVALPHACOMBINER_SCALE_OR_COMPARE_MODE(tevReg, (op >> 1) & 3);
        GX_BP_SET_TEVALPHACOMBINER_BIAS(tevReg, GX_MAX_TEVBIAS);
    }

    GX_BP_SET_TEVALPHACOMBINER_CLAMP(tevReg, clamp);
    GX_BP_SET_TEVALPHACOMBINER_DEST(tevReg, out);

    GX_BP_LOAD_REG(tevReg);
    gxdt->teva[stage] = tevReg;
    gxdt->lastWriteWasXF = FALSE;
}

void GXSetTevColor(GXTevRegID id, GXColor color) {
    u32 rgba = *(u32*)&color;
    u32 regRA = (GX_BP_REG_TEVREG0LO + id * 2) << GX_BP_OPCODE_SHIFT;
    u32 regBG = (GX_BP_REG_TEVREG0HI + id * 2) << GX_BP_OPCODE_SHIFT;

    // 8-bit colors only fill the low byte of each 11-bit component.
    regRA = GX_BITSET(regRA, 24, 8, rgba >> 24);
    regRA = GX_BITSET(regRA, 12, 8, rgba);
    regBG = GX_BITSET(regBG, 24, 8, rgba >> 8);
    regBG = GX_BITSET(regBG, 12, 8, rgba >> 16);

    // The hardware needs the high register three times to latch it.
    GX_BP_LOAD_REG(regRA);
    GX_BP_LOAD_REG(regBG);
    GX_BP_LOAD_REG(regBG);
    GX_BP_LOAD_REG(regBG);

    gxdt->lastWriteWasXF = FALSE;
}

void GXSetTevColorS10(GXTevRegID id, GXColorS10 color) {
    u32 redGreen = *(u32*)&color.r;
    u32 blueAlpha = *(u32*)&color.b;
    u32 regRA;
    u32 regBG;

    regRA = (GX_BP_REG_TEVREG0LO + id * 2) << GX_BP_OPCODE_SHIFT;
    GX_BP_SET_TEVREGLO_RED(regRA, redGreen >> 16);
    GX_BP_SET_TEVREGLO_ALPHA(regRA, blueAlpha);

    regBG = (GX_BP_REG_TEVREG0HI + id * 2) << GX_BP_OPCODE_SHIFT;
    GX_BP_SET_TEVREGHI_BLUE(regBG, blueAlpha >> 16);
    GX_BP_SET_TEVREGHI_GREEN(regBG, redGreen);

    GX_BP_LOAD_REG(regRA);
    GX_BP_LOAD_REG(regBG);
    GX_BP_LOAD_REG(regBG);
    GX_BP_LOAD_REG(regBG);

    gxdt->lastWriteWasXF = FALSE;
}

void GXSetTevKColor(GXTevKColorID id, GXColor color) {
    u32 rgba = *(u32*)&color;
    u32 regRA = (GX_BP_REG_TEVREG0LO + id * 2) << GX_BP_OPCODE_SHIFT;
    u32 regBG = (GX_BP_REG_TEVREG0HI + id * 2) << GX_BP_OPCODE_SHIFT;

    // Bits 8-11 = 8 selects the konstant color registers.
    regRA = GX_BITSET(regRA, 24, 8, rgba >> 24);
    regRA = GX_BITSET(regRA, 12, 8, rgba);
    regRA = GX_BITSET(regRA, 8, 4, 8);
    regBG = GX_BITSET(regBG, 24, 8, rgba >> 8);
    regBG = GX_BITSET(regBG, 12, 8, rgba >> 16);
    regBG = GX_BITSET(regBG, 8, 4, 8);

    GX_BP_LOAD_REG(regRA);
    GX_BP_LOAD_REG(regBG);

    gxdt->lastWriteWasXF = FALSE;
}

void GXSetTevKColorSel(GXTevStageID stage, GXTevKColorSel sel) {
    u32* kselReg = &gxdt->tevKsel[stage >> 1];

    if (stage & 1) {
        GX_BP_SET_TEVKSEL_KCSEL_ODD(*kselReg, sel);
    } else {
        GX_BP_SET_TEVKSEL_KCSEL_EVEN(*kselReg, sel);
    }

    GX_BP_LOAD_REG(*kselReg);
    gxdt->lastWriteWasXF = FALSE;
}

void GXSetTevKAlphaSel(GXTevStageID stage, GXTevKAlphaSel sel) {
    u32* kselReg = &gxdt->tevKsel[stage >> 1];

    if (stage & 1) {
        GX_BP_SET_TEVKSEL_KASEL_ODD(*kselReg, sel);
    } else {
        GX_BP_SET_TEVKSEL_KASEL_EVEN(*kselReg, sel);
    }

    GX_BP_LOAD_REG(*kselReg);
    gxdt->lastWriteWasXF = FALSE;
}

void GXSetTevSwapMode(GXTevStageID stage, GXTevSwapSel rasSel,
                      GXTevSwapSel texSel) {
    u32* tevReg = &gxdt->teva[stage];
    GX_BP_SET_TEVALPHACOMBINER_RSWAP(*tevReg, rasSel);
    GX_BP_SET_TEVALPHACOMBINER_TSWAP(*tevReg, texSel);

    GX_BP_LOAD_REG(*tevReg);
    gxdt->lastWriteWasXF = FALSE;
}

void GXSetTevSwapModeTable(GXTevSwapSel table, GXTevColorChan red,
                           GXTevColorChan green, GXTevColorChan blue,
                           GXTevColorChan alpha) {
    // Each swap table is split over the swap fields of two KSEL registers.
    GX_BP_SET_TEVKSEL_SWAP_RB(gxdt->tevKsel[table * 2], red);
    GX_BP_SET_TEVKSEL_SWAP_GA(gxdt->tevKsel[table * 2], green);
    GX_BP_LOAD_REG(gxdt->tevKsel[table * 2]);

    GX_BP_SET_TEVKSEL_SWAP_RB(gxdt->tevKsel[table * 2 + 1], blue);
    GX_BP_SET_TEVKSEL_SWAP_GA(gxdt->tevKsel[table * 2 + 1], alpha);
    GX_BP_LOAD_REG(gxdt->tevKsel[table * 2 + 1]);

    gxdt->lastWriteWasXF = FALSE;
}

void GXSetAlphaCompare(GXCompare comp0, u8 ref0, GXAlphaOp op,
                       GXCompare comp1, u8 ref1) {
    u32 cmd = GX_BP_REG_ALPHACOMPARE << GX_BP_OPCODE_SHIFT;
    GX_BP_SET_ALPHACOMPARE_REF0(cmd, ref0);
    GX_BP_SET_ALPHACOMPARE_REF1(cmd, ref1);
    GX_BP_SET_ALPHACOMPARE_COMP0(cmd, comp0);
    GX_BP_SET_ALPHACOMPARE_COMP1(cmd, comp1);
    GX_BP_SET_ALPHACOMPARE_LOGIC(cmd, op);

    GX_BP_LOAD_REG(cmd);
    gxdt->lastWriteWasXF = FALSE;
}

void GXSetZTexture(GXZTexOp op, GXTexFmt format, u32 bias) {
    u32 zTex0 = 0;
    u32 zTex1 = 0;
    u32 type;

    GX_BP_SET_ZTEXTURE0_BIAS(zTex0, bias);
    GX_BP_SET_OPCODE(zTex0, GX_BP_REG_ZTEXTURE0);

    switch (format) {
    case GX_TF_Z8:
        type = 0;
        break;
    case GX_TF_Z16:
        type = 1;
        break;
    case GX_TF_Z24X8:
        type = 2;
        break;
    default:
        type = 2;
        break;
    }

    GX_BP_SET_ZTEXTURE1_TYPE(zTex1, type);
    GX_BP_SET_ZTEXTURE1_OP(zTex1, op);
    GX_BP_SET_OPCODE(zTex1, GX_BP_REG_ZTEXTURE1);

    GX_BP_LOAD_REG(zTex0);
    GX_BP_LOAD_REG(zTex1);
    gxdt->lastWriteWasXF = FALSE;
}

// Rasterizer color select for each GXChannelID.
static u32 c2r[] = {0, 1, 0, 1, 0, 1, 7, 5, 6};

void GXSetTevOrder(GXTevStageID stage, GXTexCoordID coord, GXTexMapID map,
                   GXChannelID color) {
    u32* tref = &gxdt->tref[stage / 2];
    u32 texMap;
    u32 texCoord;

    gxdt->texmapId[stage] = map;

    texMap = map & ~GX_TEX_DISABLE;
    texMap = texMap >= GX_MAX_TEXMAP ? GX_TEXMAP0 : texMap;

    if (coord >= GX_MAX_TEXCOORD) {
        texCoord = GX_TEXCOORD0;
        gxdt->tevTcEnab &= ~(1 << stage);
    } else {
        texCoord = coord;
        gxdt->tevTcEnab |= 1 << stage;
    }

    // Each TREF register holds two stages.
    if (stage & 1) {
        GX_BP_SET_RAS1_TREF_TEXMAP_ODD(*tref, texMap);
        GX_BP_SET_RAS1_TREF_TEXCOORD_ODD(*tref, texCoord);
        GX_BP_SET_RAS1_TREF_COLORCHAN_ODD(
            *tref, color == GX_COLOR_NULL ? 7 : c2r[color]);
        GX_BP_SET_RAS1_TREF_ENABLE_TEX_ODD(
            *tref, map != GX_TEXMAP_NULL && !(map & GX_TEX_DISABLE));
    } else {
        GX_BP_SET_RAS1_TREF_TEXMAP_EVEN(*tref, texMap);
        GX_BP_SET_RAS1_TREF_TEXCOORD_EVEN(*tref, texCoord);
        GX_BP_SET_RAS1_TREF_COLORCHAN_EVEN(
            *tref, color == GX_COLOR_NULL ? 7 : c2r[color]);
        GX_BP_SET_RAS1_TREF_ENABLE_TEX_EVEN(
            *tref, map != GX_TEXMAP_NULL && !(map & GX_TEX_DISABLE));
    }

    GX_BP_LOAD_REG(*tref);
    gxdt->lastWriteWasXF = FALSE;
    gxdt->gxDirtyFlags |= GX_DIRTY_SU_TEX;
}

void GXSetNumTevStages(u8 num) {
    GX_BP_SET_GENMODE_NUMTEVSTAGES(gxdt->genMode, num - 1);
    gxdt->gxDirtyFlags |= GX_DIRTY_GEN_MODE;
}
