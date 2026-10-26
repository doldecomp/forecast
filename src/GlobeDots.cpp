#include <channel/GlobeDots.h>
#include <channel/SimpleGlobe.h>
#include <channel/System.h>

#include <nw4r/g3d/g3d_camera.h>
#include <nw4r/math.h>
#include <nw4r/ut.h>
#include <revolution/GX.h>

extern "C" TPLPalette* gCommonTpl;

// HACK: non-const (so MWCC reloads the words inside the ctor loop like the target) but forced into .rodata.
__declspec(section ".rodata") static Vec sBase0 = {0.0f, 0.0f, -100.0f};
__declspec(section ".rodata") static Vec sBase1 = {0.0f, 0.0f, -100.0f};
__declspec(section ".rodata") static Vec sBase2 = {0.0f, 0.0f, -100.0f};

static const f32 sTexCoords[3][2] = {
    {0.0f, 0.0f},
    {1.0f, 0.0f},
    {0.0f, 1.0f},
};

static inline void TransformVert(Mtx mtx, const Vec& base, f32 x, f32 y, Vec* out) {
    Vec v = base;
    v.x = x;
    v.y = y;
    PSMTXMultVec(mtx, &v, out);
}

GlobeDots::GlobeDots() {
    for (int i = 0; i < GLOBE_DOT_COUNT; i++) {
        Mtx rotX;
        Mtx rotY;
        Mtx mtx;

        u16 lon = ((const u16*)gGlobeDotPositions)[i*2];
        int n = i * 9;
        f32 size = 0.0045f * gGlobeDotSizes[i];
        f32 far = 3.0f * size;
        u16 lat = ((const u16*)gGlobeDotPositions)[i*2+1];

        PSMTXRotTrig(rotX, nw4r::math::SinIdx(lon), nw4r::math::CosIdx(lon), 'x');
        PSMTXRotTrig(rotY, nw4r::math::SinIdx(lat), nw4r::math::CosIdx(lat), 'y');
        PSMTXConcat(rotY, rotX, mtx);


        f32* fv = (f32*)mVerts;
        TransformVert(mtx, sBase0, -size, -size, (Vec*)&fv[n]);
        TransformVert(mtx, sBase1, far, -size, (Vec*)&fv[n + 3]);
        TransformVert(mtx, sBase2, -size, far, (Vec*)&fv[n + 6]);
    }
}

GlobeDots::~GlobeDots() {}

void GlobeDots::ResetAlpha() {
    mAlpha = 255;
}

void GlobeDots::UpdateAlpha(f32 speedX, f32 speedY) {
    u8 alpha = nw4r::ut::Clamp(255.0f - 200.0f * (nw4r::math::FAbs(speedX) + nw4r::math::FAbs(speedY)), 0.0f, 255.0f);
    if (mAlpha > alpha) {
        if (mAlpha - alpha < 32) {
            mAlpha = alpha;
        } else {
            mAlpha -= 32;
        }
    } else if (mAlpha >= 254) {
        mAlpha = 255;
    } else {
        mAlpha += 2;
    }
}

static inline void SetDotColor(u8 alpha) {
    GXColor color = {0, 0, 0, 0};
    color.a = alpha;
    GXSetTevColor(GX_TEVREG0, color);
}

void GlobeDots::Draw() {
    GlobeView* view = gSimpleGlobe->GetView();
    nw4r::math::MTX34 viewMtx;
    nw4r::math::MTX44 projMtx;
    GXTexObj texObj;

    SetDefaultGXState();
    GXSetZMode(GX_FALSE, GX_LEQUAL, GX_FALSE);

    view->getCamera()->GetCameraMtx(&viewMtx);
    view->getCamera()->GetProjectionMtx(&projMtx);
    f32 aspect = (f32)GetScreenWidth() / SCREEN_HEIGHT;
    C_MTXPerspective(projMtx, 40.0f, aspect, 1.0f, 1000.0f);

    viewMtx._03 = 0.0f;
    viewMtx._13 = 0.0f;
    viewMtx._23 = 0.0f;
    GXLoadPosMtxImm(viewMtx, GX_PNMTX0);
    GXSetCurrentMtx(GX_PNMTX0);
    GXSetProjection(projMtx, GX_PERSPECTIVE);

    GXClearVtxDesc();
    GXSetVtxDesc(GX_VA_POS, GX_INDEX16);
    GXSetVtxDesc(GX_VA_CLR0, GX_INDEX8);
    GXSetVtxDesc(GX_VA_TEX0, GX_INDEX8);
    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGB, GX_RGB8, 0);
    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);
    GXSetArray(GX_VA_POS, mVerts, sizeof(Vec));
    GXSetArray(GX_VA_CLR0, gGlobeDotColors, 3);
    GXSetArray(GX_VA_TEX0, sTexCoords, sizeof(sTexCoords[0]));

    GXSetChanCtrl(GX_COLOR0, GX_FALSE, GX_SRC_VTX, GX_SRC_VTX, GX_LIGHT_NULL, GX_DF_NONE, GX_AF_NONE);
    GetTexObj(gCommonTpl, 0x60, &texObj);
    GXLoadTexObj(&texObj, GX_TEXMAP0);
    GXSetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0);
    GXSetTevColorIn(GX_TEVSTAGE0, GX_CC_RASC, GX_CC_ONE, GX_CC_TEXC, GX_CC_ZERO);
    GXSetTevAlphaIn(GX_TEVSTAGE0, GX_CA_ZERO, GX_CA_TEXA, GX_CA_A0, GX_CA_ZERO);

    SetDotColor(mAlpha);

    GXBegin(GX_TRIANGLES, GX_VTXFMT0, GLOBE_DOT_COUNT * 3);
    int v = 0;
    for (int i = 0; i < GLOBE_DOT_COUNT; i++) {
        u8 colorIdx = gGlobeDotColorIndices[(u32)i];
        v += 3;

        GXPosition1x16(v - 3);
        GXColor1x8(colorIdx);
        GXTexCoord1x8(0);
        GXPosition1x16(v - 2);
        GXColor1x8(colorIdx);
        GXTexCoord1x8(1);
        GXPosition1x16(v - 1);
        GXColor1x8(colorIdx);
        GXTexCoord1x8(2);
    }
    GXEnd();
}
