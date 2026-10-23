#include <types.h>
#include <revolution/ARC.h>
#include <revolution/TPL.h>
#include <revolution/GX.h>
#include <cstring>
#include "vcmv.h"

static VCMVCursorDef sCursorColors[] = {
    {50.0f, 50.0f, 0.38f, 0.92f, NULL, {0, 140, 255, 255}}, // blue
    {50.0f, 50.0f, 0.38f, 0.92f, NULL, {255, 56, 56, 255}}, // red
    {50.0f, 50.0f, 0.38f, 0.92f, NULL, {16, 189, 13, 255}}, // green
    {50.0f, 50.0f, 0.38f, 0.92f, NULL, {255, 156, 0, 255}}, // orange
};

static s32 sLastSoundId = -1;
static s32 sLastSoundFrame;
#pragma push
#pragma force_active on
static s32 sUnused1;
static s32 sUnused2;
static s32 sUnused3;
static s32 sUnused4;
static s32 sUnused5;
static s32 sUnused6;
static s32 sUnused7;
#pragma pop
static TPLPalette* sDefaultCursorTpl = NULL;
s32 sCursorAnimTimer;

void VCMVPlaySound(s32 soundId) {
    if (sLastSoundId == soundId && sManualFrameCount - sLastSoundFrame < 3) {
        return;
    }

    sLastSoundId = soundId;
    sLastSoundFrame = sManualFrameCount;

    switch (soundId) {
        case 0: {
            volatile VCMVControllerState* ptr = &sManualCursors[sManualActiveCursor];

            if (!sMotorActive && ptr->isConnected && sManualFrameCount - sMotorStartTime >= 0x0F) {
                sTriggerRumble = true;
            }

            HBMPlaySound(0x17);
            break;
        }
        case 1:
            HBMPlaySound(0x18);
            break;
        case 2:
            HBMPlaySound(0x16);
            break;
        case 3:
            HBMPlaySound(0x1B);
            break;
        case 5:
            HBMPlaySound(0x1A);
            break;
        default:
            break;
    }
}

void vcmvCursorInit(void) {}

void vcmvSetHBMInfo(void** archiveData) {
    char path[40] = "arc/timg/defcursor_final";
    ARCHandle arc;
    ARCHandle defaultArc;
    ARCHandle cursorArc;
    ARCFileInfo defaultFile;
    ARCFileInfo cursorFile;
    VCMVCursorDef* cursorDef;
    int i;

    char* suffix = &path[strlen(path)];
    ARCInitHandle(*archiveData, &arc);
    strcpy(suffix, "64_a.tpl");

    defaultArc = arc;
    ARCOpen(&defaultArc, path, &defaultFile);
    sDefaultCursorTpl = (TPLPalette*)ARCGetStartAddrInMem(&defaultFile);
    ARCClose(&defaultFile);
    TPLBind(sDefaultCursorTpl);

    strcpy(suffix, "_p1.tpl");

    cursorDef = &sCursorColors[0];
    for (i = 0; i < (int)ARRAY_COUNT(sCursorColors); i++, cursorDef++) {
        suffix[2] = i + '1';
        cursorArc = arc;
        ARCOpen(&cursorArc, path, &cursorFile);
        cursorDef->tpl = (TPLPalette*)ARCGetStartAddrInMem(&cursorFile);
        ARCClose(&cursorFile);
        TPLBind(cursorDef->tpl);
    }
}

void vcmvDrawCursor(s32 index) {
    volatile VCMVControllerState* cursor = &sManualCursors[index];
    f32 cursorX;
    f32 cursorY;
    f32 cosAngle;
    f32 sinAngle;
    f32 width;
    f32 height;
    VCMVCursorDef* cursorDef;
    u8 alpha;
    GXTexObj texObj;
    VCMVQuad quad;

    if (cursor->active == 0) {
        return;
    }

    cursorX = cursor->cursorX;
    cursorY = -cursor->cursorY;
    cosAngle = cursor->cosAngle;
    sinAngle = cursor->sinAngle;

    cursorDef = &sCursorColors[index];
    width = cursorDef->width;
    height = cursorDef->height;

    if (index == sManualActiveCursor) {
        if (cursor->active >= 0x240) {
            alpha = 0xC0;
        } else {
            alpha = cursor->active / 3;
        }

        if (sCursorAnimTimer != 0) {
            f32 var_f0;

            if (sCursorAnimTimer > 0x0A) {
                var_f0 = ((0x14 - sCursorAnimTimer) * 0.02f) + 1.0f;
            } else {
                var_f0 = (sCursorAnimTimer * 0.02f) + 1.0f;
            }

            width *= var_f0;
            height *= var_f0;

            sCursorAnimTimer--;
        }
    } else {
        if (cursor->active >= 0x100) {
            alpha = 0x40;
        } else {
            alpha = cursor->active / 4;
        }
    }

    GXColor alphaColor = {224, 224, 224, alpha};

    quad.x2 = cursorX - (cosAngle * width * cursorDef->hotspotX) - (sinAngle * height * cursorDef->hotspotY);
    quad.x1 = quad.x2 + (sinAngle * height);
    quad.x4 = quad.x1 + (cosAngle * width);
    quad.x3 = quad.x4 - (sinAngle * height);

    quad.y2 = cursorY + (sinAngle * width * cursorDef->hotspotX) - (cosAngle * height * cursorDef->hotspotY);
    quad.y1 = quad.y2 + (cosAngle * height);
    quad.y4 = quad.y1 - (sinAngle * width);
    quad.y3 = quad.y4 - (cosAngle * height);

    TPLGetGXTexObjFromPalette(sDefaultCursorTpl, &texObj, 0);
    GXLoadTexObj(&texObj, GX_TEXMAP1);
    TPLGetGXTexObjFromPalette(cursorDef->tpl, &texObj, 0);
    GXLoadTexObj(&texObj, GX_TEXMAP0);

    GXColor white = {255, 255, 255, 255};
    GXSetTevColor(GX_TEVREG0, cursorDef->color);
    GXSetTevColor(GX_TEVREG1, white);
    GXSetTevColor(GX_TEVREG2, alphaColor);

    GXClearVtxDesc();
    GXInvalidateVtxCache();
    GXInvalidateTexAll();
    GXSetVtxDesc(GX_VA_POS, GX_DIRECT);
    GXSetVtxDesc(GX_VA_TEX0, GX_DIRECT);
    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_CLR_RGBA, GX_F32, 0);
    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_CLR_RGBA, GX_F32, 0);
    GXSetNumChans(0);
    GXSetNumTexGens(1);
    GXSetNumIndStages(0);
    GXSetNumTevStages(2);
    GXSetTevDirect(GX_TEVSTAGE0);
    GXSetTevColorIn(GX_TEVSTAGE0, GX_CC_C0, GX_CC_C1, GX_CC_TEXC, GX_CC_ZERO);
    GXSetTevAlphaIn(GX_TEVSTAGE0, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO);
    GXSetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
    GXSetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
    GXSetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR_NULL);
    GXSetTexCoordGen2(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, 0x3C, GX_FALSE, 0x7D);
    GXSetTevDirect(GX_TEVSTAGE1);
    GXSetTevColorIn(GX_TEVSTAGE1, GX_CC_ZERO, GX_CC_CPREV, GX_CC_TEXC, GX_CC_ZERO);
    GXSetTevAlphaIn(GX_TEVSTAGE1, GX_CA_ZERO, GX_CA_A2, GX_CA_TEXA, GX_CA_ZERO);
    GXSetTevColorOp(GX_TEVSTAGE1, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
    GXSetTevAlphaOp(GX_TEVSTAGE1, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
    GXSetTevOrder(GX_TEVSTAGE1, GX_TEXCOORD0, GX_TEXMAP1, GX_COLOR_NULL);
    GXSetColorUpdate(GX_TRUE);
    GXSetAlphaUpdate(GX_FALSE);
    GXSetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_SET);
    GXSetZMode(GX_FALSE, GX_ALWAYS, GX_FALSE);
    GXSetAlphaCompare(GX_ALWAYS, 0, GX_AOP_OR, GX_ALWAYS, 0);
    GXSetCullMode(GX_CULL_NONE);
    GXSetClipMode(GX_CLIP_ENABLE);
    VCMVDrawQuad(&quad);
}
