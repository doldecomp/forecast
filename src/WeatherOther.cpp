// d_weather_a_other.cpp: the lifestyle index panel (UV, laundry, pollen)
#include <channel/WeatherBase.h>
#include <channel/ColorWhite.h>
#include <channel/ForecastData.h>
#include <channel/LayoutButton.h>
#include <channel/SceneBase.h>
#include <channel/System.h>
#include <channel/WeatherScene.h>

#include <wstring.h>

extern f32 gUnkSceneFloat;
extern wchar_t sTextBuf[0x100];
extern nw4r::ut::Font* gSysFont;
extern nw4r::math::MTX34 gModelMtx;

wchar_t* FormatNumber(s32 value, wchar_t* pBuf, s32 digits, BOOL zeroPad);

static const f32 sBoxScalesJP[6] = {1.0f, 1.0f, 1.0f, 0.5f, 0.5f, 0.5f};

static const char* sBoxNamesJP[6] = {
    "life_a_telop", "life_b_telop", "life_c_telop", "life_a_1-1", "life_b_1-1", "life_c_1-1",
};
static const char* sBoxNames[2] = {
    "life_b_telop",
    "life_b_1-1",
};
static const f32 sBoxScales[2] = {1.0f, 0.75f};

#define UPDATE_DAY()                                                                                         \
    {                                                                                                        \
        CityForecast* forecast = gCurrentCity != NULL ? gCurrentCity->mForecast : NULL;                      \
        if (forecast != NULL) {                                                                              \
            if (forecast->mEntry->mTime / 1440 < forecast->mMinutes / 1440) {                                \
                mDay = 1;                                                                                    \
            } else {                                                                                         \
                mDay = forecast->mTime.hour >= 18;                                                           \
            }                                                                                                \
        } else {                                                                                             \
            mDay = gCalendarTime.hour >= 18;                                                                 \
        }                                                                                                    \
    }

WeatherOther::WeatherOther(const Vec2& pos, void* arc, const Vec2& size, s32 type)
    : WeatherBase(pos, size, type), mNoServiceLayout(NULL), mNoServiceText(NULL), mHasData(NULL) {
    UPDATE_DAY();

    if (gLanguage == 0) {
        SetupJP(arc);
    } else {
        Setup(arc);
    }
}

WeatherOther::~WeatherOther() {
    delete mNoServiceLayout;
    delete mLayout;
    mLayout = NULL;
}

void WeatherOther::SetupJP(void* arc) {
    char layoutName[0x100] = "life_nJP.brlyt";
    f32 centerX = 0.5f * GetScreenWidth();
    f32 centerY = 228.0f;
    f32 scaleX = gWidescreen ? 1.3684211f : 1.0f;

    mLayout = new ButtonGroup(arc, layoutName, gButtonColors, false);
    mNoServiceLayout = new ButtonGroup(arc, "life_no_service.brlyt", gButtonColors, false);
    mNoServiceText = mNoServiceLayout->FindButton("text");
    if (mNoServiceText == NULL) {
        OSPanic("d_weather_a_other.cpp", 107,
                "text が見つかりません!!\n");
    }

    TextBox* box = mBoxes;
    for (s32 i = 0; i < 6; i++, box++) {
        box->mPane = mLayout->FindButton(sBoxNamesJP[i]);
        if (box->mPane == NULL) {
            OSReport("%sが見つかりません!!\n", sBoxNamesJP[i]);
            OSPanic("d_weather_a_other.cpp", 118, "");
        }

        Vec2F center = box->mPane->GetCenter();
        box->mX = center.x;
        box->mY = center.y;
        box->mX = centerX + box->mX * scaleX;
        box->mY = centerY - box->mY;
        LayoutButton* pane = box->mPane;
        f32 h = __fabsf(pane->mRect.top - pane->mRect.bottom);
        f32 w = pane->mRect.right - pane->mRect.left;
        box->mWidth = scaleX * w;
        box->mHeight = h;
        box->mScaleX = sBoxScalesJP[i];
        box->mScaleY = sBoxScalesJP[i];
        box->mColor.r = gColorWhite.r;
        box->mColor.g = gColorWhite.g;
        box->mColor.b = gColorWhite.b;
        box->mColor.a = gColorWhite.a;
        box->mShadowColor.r = gColorDarkGray.r;
        box->mShadowColor.g = gColorDarkGray.g;
        box->mShadowColor.b = gColorDarkGray.b;
        box->mShadowColor.a = gColorDarkGray.a;

        switch (i) {
        case 0:
        case 1:
        case 2:
            box->mX -= 0.5f * box->mWidth;
            break;
        }
    }

    mHasData = &WeatherOther::HasDataJP;
}

void WeatherOther::Setup(void* arc) {
    char layoutName[0x100] = "life_nWW.brlyt";
    f32 centerX = 0.5f * GetScreenWidth();
    f32 centerY = 228.0f;
    f32 scaleX = gWidescreen ? 1.3684211f : 1.0f;

    mLayout = new ButtonGroup(arc, layoutName, gButtonColors, false);
    mNoServiceLayout = new ButtonGroup(arc, "life_no_service.brlyt", gButtonColors, false);
    mIconPane = mLayout->FindButton("life_b");
    if (mIconPane == NULL) {
        OSPanic("d_weather_a_other.cpp", 156,
                "life_b が見つかりません!!\n");
    }

    mNoServiceText = mNoServiceLayout->FindButton("text");
    if (mNoServiceText == NULL) {
        OSPanic("d_weather_a_other.cpp", 162,
                "text が見つかりません!!\n");
    }

    TextBox* box = mBoxes;
    for (s32 i = 0; i < 2; i++, box++) {
        box->mPane = mLayout->FindButton(sBoxNames[i]);
        if (box->mPane == NULL) {
            OSReport("%sが見つかりません!!\n", sBoxNames[i]);
            OSPanic("d_weather_a_other.cpp", 173, "");
        }

        Vec2F center = box->mPane->GetCenter();
        box->mX = center.x;
        box->mY = center.y;
        box->mX = centerX + box->mX * scaleX;
        box->mY = centerY - box->mY;
        LayoutButton* pane = box->mPane;
        f32 h = __fabsf(pane->mRect.top - pane->mRect.bottom);
        f32 w = pane->mRect.right - pane->mRect.left;
        box->mWidth = scaleX * w;
        box->mHeight = h;
        box->mScaleX = sBoxScales[i];
        box->mScaleY = sBoxScales[i];
        box->mColor.r = gColorWhite.r;
        box->mColor.g = gColorWhite.g;
        box->mColor.b = gColorWhite.b;
        box->mColor.a = gColorWhite.a;
        box->mShadowColor.r = gColorDarkGray.r;
        box->mShadowColor.g = gColorDarkGray.g;
        box->mShadowColor.b = gColorDarkGray.b;
        box->mShadowColor.a = gColorDarkGray.a;
    }

    mHasData = &WeatherOther::HasData;
}

void WeatherOther::Reset() {
    mLayout->Reset();
    mNoServiceLayout->Reset();
}

void WeatherOther::Draw() {
    if (mCulled) {
        return;
    }

    s32 alpha = mScale * mAlpha;
    CityForecast* forecast = gCurrentCity != NULL ? gCurrentCity->mForecast : NULL;
    if (forecast != NULL) {
        BOOL hasData = mHasData ? (this->*mHasData)(forecast) : TRUE;
        if (hasData) {
            mLayout->SetPaneAlpha(alpha);
            mLayout->Draw();
            SetDefaultGXState();
            SetOrthoProjection();
            mWriter.SetFont(*gSysFont);
            if (gLanguage == 0) {
                DrawJP(forecast, alpha);
                return;
            }

            IndexInfo* uv = gForecastData->FindUVIndex((u8)forecast->mEntry->mDays[mDay].mUVIndex);
            mWriter.SetDrawFlag(0x111);
            mWriter.SetupGX();
            if (uv != NULL) {
                DrawText(&mBoxes[0], uv->mText, alpha);
                SetDefaultGXState();
                SetOrthoProjection();
                u8 a = alpha;
                wchar_t* buf = sTextBuf;
                s32 code = uv->mIndex->mCode;
                if (code == 0xFF) {
                    wcscpy(buf, L"--");
                } else {
                    FormatNumber(code, buf, 4, FALSE);
                }

                Vec2F pos(mBoxes[1].mX + mPos.x, mBoxes[1].mY - mPos.y);
                mBoxes[1].mColor.a = a;
                mBoxes[1].mShadowColor.a = a;
                DrawTempCentered(sTextBuf, &pos, mBoxes[1].mScaleX, mBoxes[1].mScaleY, &mBoxes[1].mColor,
                                 &mBoxes[1].mShadowColor);
            }
        } else {
            mNoServiceText->SetState(5);
            mNoServiceLayout->SetPaneAlpha(alpha);
            mNoServiceLayout->Draw();
        }
    } else {
        mNoServiceText->SetState(0);
        mNoServiceLayout->SetPaneAlpha(alpha);
        mNoServiceLayout->Draw();
    }
}

void WeatherOther::SetPosition(const Vec2& pos, const f32& scale, const bool& visible, BOOL checkHover) {
    WeatherBase::SetPosition(pos, scale, visible, checkHover);
    UPDATE_DAY();
    mLayout->Calc();
    mNoServiceLayout->Calc();
    nw4r::math::VEC3 trans(mPos.x, mPos.y, 0.0f);
    PSMTXTrans(gModelMtx, trans.x, trans.y, trans.z);
    mLayout->SetViewMtx(gModelMtx);
    mNoServiceLayout->SetViewMtx(gModelMtx);
}

BOOL WeatherOther::IsEmpty() {
    CityForecast* forecast = gCurrentCity != NULL ? gCurrentCity->mForecast : NULL;
    if (forecast != NULL) {
        BOOL hasData = mHasData ? (this->*mHasData)(forecast) : TRUE;
        return !hasData;
    }

    return TRUE;
}

BOOL WeatherOther::HasDataJP(CityForecast* forecast) {
    if (gForecastData->FindUVIndex((u8)forecast->mEntry->mDays[mDay].mUVIndex) != NULL) {
        return TRUE;
    }

    if (gForecastData->FindLaundryIndex((u8)forecast->mEntry->mDays[mDay].mLaundryIndex) != NULL) {
        return TRUE;
    }

    return gForecastData->FindPollenIndex((u8)forecast->mEntry->mDays[mDay].mPollenIndex) != NULL;
}

BOOL WeatherOther::HasData(CityForecast* forecast) {
    return gForecastData->FindUVIndex((u8)forecast->mEntry->mDays[mDay].mUVIndex) != NULL;
}

#define DRAW_INDEX_NUM(box, info, alpha_)                                                                    \
    {                                                                                                        \
        u8 a = alpha_;                                                                                       \
        wchar_t* buf = sTextBuf;                                                                             \
        s32 code = (info)->mIndex->mCode;                                                                    \
        if (code == 0xFF) {                                                                                  \
            wcscpy(buf, L"--");                                                                              \
        } else {                                                                                             \
            FormatNumber(code, buf, 4, FALSE);                                                               \
        }                                                                                                    \
        Vec2F pos((box)->mX + mPos.x, (box)->mY - mPos.y);                                                   \
        (box)->mColor.a = a;                                                                                 \
        (box)->mShadowColor.a = a;                                                                           \
        DrawDateCentered(sTextBuf, &pos, (box)->mScaleX, (box)->mScaleY, &(box)->mColor, &(box)->mShadowColor); \
    }

#define DRAW_NO_INDEX_NUM(box, alpha_)                                                                       \
    {                                                                                                        \
        u8 a = alpha_;                                                                                       \
        wcscpy(sTextBuf, L"--");                                                                             \
        Vec2F pos((box)->mX + mPos.x, (box)->mY - mPos.y);                                                   \
        (box)->mColor.a = a;                                                                                 \
        (box)->mShadowColor.a = a;                                                                           \
        DrawDateCentered(sTextBuf, &pos, (box)->mScaleX, (box)->mScaleY, &(box)->mColor, &(box)->mShadowColor); \
    }

void WeatherOther::DrawJP(CityForecast* forecast, const s32& alpha) {
    IndexInfo* uv = gForecastData->FindUVIndex((u8)forecast->mEntry->mDays[mDay].mUVIndex);
    IndexInfo* laundry = gForecastData->FindLaundryIndex((u8)forecast->mEntry->mDays[mDay].mLaundryIndex);
    IndexInfo* pollen = gForecastData->FindPollenIndex((u8)forecast->mEntry->mDays[mDay].mPollenIndex);

    mWriter.SetDrawFlag(0x100);
    mWriter.SetupGX();

    if (laundry != NULL && laundry->mIndex->mCode != 0xFF && laundry->mText != NULL) {
        DrawText(&mBoxes[0], laundry->mText, alpha);
    }

    if (uv != NULL && uv->mIndex->mCode != 0xFF && uv->mText != NULL) {
        DrawText(&mBoxes[1], uv->mText, alpha);
    }

    if (pollen != NULL && pollen->mIndex->mCode != 0xFF && pollen->mText != NULL) {
        DrawText(&mBoxes[2], pollen->mText, alpha);
    }

    SetDefaultGXState();
    SetOrthoProjection();

    if (laundry != NULL) {
        DRAW_INDEX_NUM(&mBoxes[3], laundry, alpha);
    } else {
        DRAW_NO_INDEX_NUM(&mBoxes[3], alpha);
    }

    if (uv != NULL) {
        DRAW_INDEX_NUM(&mBoxes[4], uv, alpha);
    } else {
        DRAW_NO_INDEX_NUM(&mBoxes[4], alpha);
    }

    if (pollen != NULL) {
        DRAW_INDEX_NUM(&mBoxes[5], pollen, alpha);
    } else {
        DRAW_NO_INDEX_NUM(&mBoxes[5], alpha);
    }
}
