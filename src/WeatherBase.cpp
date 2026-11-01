// d_weather_base.cpp: base of the forecast panels
#include <channel/WeatherBase.h>
#include <channel/LayoutButton.h>
#include <channel/SceneBase.h>
#include <channel/System.h>

#include <revolution/GX.h>

extern TPLPalette* gCommonTpl;
extern f32 gUnkSceneFloat;

const u32 gWeekdayTexturesJP[7] = {0x5C, 0x5A, 0x5E, 0x5F, 0x5D, 0x49, 0x5B};

const u32 gWeekdayTextures[7][7] = {
    {0x39, 0x37, 0x3B, 0x3C, 0x3A, 0x36, 0x38},
    {0x39, 0x37, 0x3B, 0x3C, 0x3A, 0x36, 0x38},
    {0x24, 0x22, 0x26, 0x27, 0x25, 0x21, 0x23},
    {0x1D, 0x1B, 0x1F, 0x20, 0x1E, 0x1A, 0x1C},
    {0x32, 0x30, 0x34, 0x35, 0x33, 0x2F, 0x31},
    {0x2B, 0x29, 0x2D, 0x2E, 0x2C, 0x28, 0x2A},
    {0x15, 0x13, 0x17, 0x18, 0x16, 0x12, 0x14},
};

WeatherBase::WeatherBase(const Vec2& pos, const Vec2& size, s32 type)
    : mLayout(NULL), mPos(pos), mSize(size), mRect(0.0f, 0.0f, GetScreenWidth(), 456.0f), mScale(1.0f), mType(type),
      unk90(0), mAlpha(255), unk98(-1), mCulled(FALSE), mVisible(TRUE) {
    f32 x = mPos.x - 0.5f * GetScreenWidth();
    f32 y = mPos.y - 228.0f;
    mRect.MoveTo(x, y);
}

void WeatherBase::SetPosition(const Vec2& pos, const f32& scale, const bool& visible, BOOL checkHover) {
    // HACK: dead u32->f32 conversion, only to reproduce .sdata2 pool order (unsigned magic double before 2.0f)
    f32 unusedConv = (f32)(u32)checkHover;
    mPos = pos;
    mScale = scale;
    mVisible = visible;
    mRect.MoveTo(pos.x, -pos.y);

    if (!mVisible || mRect.top >= 456.0f || mRect.bottom <= 0.0f || mRect.left >= GetScreenWidth() ||
        mRect.right <= 0.0f) {
        mCulled = TRUE;
    } else {
        mCulled = FALSE;
    }
}

void WeatherBase::DrawIcon(TextBox* box, u32 id, s32 alpha) {
    f32 halfWidth = box->mScaleX * (0.5f * GetTexWidth(gCommonTpl, id));
    Vec pos;
    Vec shadowPos;

    f32 x, y;
    f32 hh = box->mScaleX * (0.5f * GetTexHeight(gCommonTpl, id));
    y = box->mY - mPos.y - hh;
    x = box->mX + mPos.x - halfWidth;
    pos.z = 0.0f;
    shadowPos.z = 0.0f;
    pos.x = x;
    pos.y = y;
    f32 sx = 2.0f + x;
    f32 sy = 2.0f + y;
    shadowPos.x = sx;
    shadowPos.y = sy;

    box->mShadowColor.a = alpha;
    box->mColor.a = alpha;
    GXSetTevColor(GX_TEVREG0, box->mShadowColor);
    DrawTextureAt(gCommonTpl, id, box->mScaleX, box->mScaleX, &shadowPos);
    GXSetTevColor(GX_TEVREG0, box->mColor);
    DrawTextureAt(gCommonTpl, id, box->mScaleX, box->mScaleX, &pos);
}

void WeatherBase::DrawIconLarge(TextBox* box, u32 id, s32 alpha) {
    f32 halfWidth = box->mScaleX * (0.5f * GetTexWidth(gCommonTpl, id));
    Vec pos;
    Vec shadowPos;

    f32 x, y;
    f32 hh = box->mScaleX * (0.5f * GetTexHeight(gCommonTpl, id));
    y = box->mY - mPos.y - hh;
    x = box->mX + mPos.x - halfWidth;
    pos.z = 0.0f;
    shadowPos.z = 0.0f;
    pos.x = x;
    pos.y = y;
    f32 sx = 3.0f + x;
    f32 sy = 3.0f + y;
    shadowPos.x = sx;
    shadowPos.y = sy;

    box->mShadowColor.a = alpha;
    box->mColor.a = alpha;
    GXSetTevColor(GX_TEVREG0, box->mShadowColor);
    DrawTextureAt(gCommonTpl, id, box->mScaleX, box->mScaleX, &shadowPos);
    GXSetTevColor(GX_TEVREG0, box->mColor);
    DrawTextureAt(gCommonTpl, id, box->mScaleX, box->mScaleX, &pos);
}

static inline f32 Shrink(f32 scale, f32 max, f32 cur) {
    f32 r = max / cur;
    return scale * r;
}

void WeatherBase::DrawText(TextBox* box, const wchar_t* text, s32 alpha) {
    Vec2 pos;
    f32 scaleX;
    f32 scaleY;

    pos.y = box->mY - mPos.y;
    pos.x = box->mX + mPos.x;
    box->mShadowColor.a = alpha;
    box->mColor.a = alpha;

    mWriter.SetScale(box->mScaleX, box->mScaleY);
    f32 space = gUnkSceneFloat;
    mWriter.SetCharSpace(box->mScaleX * space);
    mWriter.SetLineSpace(0.0f);

    f32 width = mWriter.CalcStringWidth(text);
    if (width > box->mWidth) {
        scaleX = Shrink(box->mScaleX, box->mWidth, width);
        f32 cs = gUnkSceneFloat;
        mWriter.SetCharSpace(scaleX * cs);
    } else {
        scaleX = box->mScaleX;
    }

    f32 height = mWriter.CalcStringHeight(text);
    if (height > box->mHeight) {
        scaleY = box->mScaleY * (box->mHeight / height);
        mWriter.SetLineSpace(-4.0f);
    } else {
        scaleY = box->mScaleY;
    }

    scaleX = scaleX < scaleY ? scaleX : scaleY;

    mWriter.SetScale(scaleX);
    mWriter.SetTextColor(box->mShadowColor);
    mWriter.SetCursor(1.0f + pos.x, 1.0f + pos.y);
    mWriter.Print(text);
    mWriter.SetTextColor(box->mColor);
    mWriter.SetCursor(pos.x, pos.y);
    mWriter.Print(text);
}
