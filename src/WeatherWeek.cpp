// d_weather_a_week.cpp: the weekly forecast panel
#include <channel/WeatherBase.h>
#include <channel/ColorWhite.h>
#include <channel/ForecastData.h>
#include <channel/LayoutButton.h>
#include <channel/SceneBase.h>
#include <channel/System.h>
#include <channel/WeatherScene.h>

#include <revolution/GX.h>
#include <wstring.h>

extern f32 gUnkSceneFloat;
extern wchar_t sTextBuf[0x100];
extern nw4r::ut::Font* gSysFont;
extern nw4r::math::MTX34 gModelMtx;

wchar_t* FormatNumber(s32 value, wchar_t* pBuf, s32 digits, BOOL zeroPad);
void DrawWeatherIcon(u16 icon, const Vec2* pos, f32 scale, s32 alpha);
void MinutesToCalendarTime(u32 minutes, OSCalendarTime* time);

// Weekday textures, Sunday first
extern const u32 gWeekdayTexturesJP[7];
extern const u32 gWeekdayTextures[7][7];

static Color sColorSunday(240, 49, 49, 255);
static Color sColorSaturday(49, 162, 240, 255);
static Color sColorWeekday(0, 24, 88, 255);

static const f32 sBoxScalesJP[45] = {
    0.75f, 0.75f, 0.75f,
    0.45f, 1.0f, 0.5f, 0.5f, 0.5f, 0.5f,
    0.45f, 1.0f, 0.5f, 0.5f, 0.5f, 0.5f,
    0.45f, 1.0f, 0.5f, 0.5f, 0.5f, 0.5f,
    0.45f, 1.0f, 0.5f, 0.5f, 0.5f, 0.5f,
    0.45f, 1.0f, 0.5f, 0.5f, 0.5f, 0.5f,
    0.45f, 1.0f, 0.5f, 0.5f, 0.5f, 0.5f,
    0.45f, 1.0f, 0.5f, 0.5f, 0.5f, 0.5f,
};
// Per day: date, weekday, weather icon, high, low, rain
static const s32 sDayBoxesJP[7][6] = {
    {3, 4, 5, 6, 7, 8},
    {9, 10, 11, 12, 13, 14},
    {15, 16, 17, 18, 19, 20},
    {21, 22, 23, 24, 25, 26},
    {27, 28, 29, 30, 31, 32},
    {33, 34, 35, 36, 37, 38},
    {39, 40, 41, 42, 43, 44},
};
static const f32 sBoxScales[30] = {
    1.0f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f,
    1.0f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f,
    1.0f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f,
    1.0f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f,
    1.0f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f,
};
// Per day: weekday, weather icon, high with unit, high, low with unit, low
static const s32 sDayBoxes[5][6] = {
    {0, 1, 2, 3, 4, 5},
    {6, 7, 8, 9, 10, 11},
    {12, 13, 14, 15, 16, 17},
    {18, 19, 20, 21, 22, 23},
    {24, 25, 26, 27, 28, 29},
};

static const char* sBoxNamesJP[45] = {
    "text_kionH", "text_kionL", "text_percent",
    "week_a_1", "week_a_youbi", "week_a_icon", "week_aH_10", "week_aL_10", "week_ar_10",
    "week_b_1", "week_b_youbi", "week_b_icon", "week_bH_10", "week_bL_10", "week_br_10",
    "week_c_1", "week_c_youbi", "week_c_icon", "week_cH_10", "week_cL_10", "week_cr_10",
    "week_d_1", "week_d_youbi", "week_d_icon", "week_dH_10", "week_dL_10", "week_dr_10",
    "week_e_1", "week_e_youbi", "week_e_icon", "week_eH_10", "week_eL_10", "week_er_10",
    "week_f_1", "week_f_youbi", "week_f_icon", "week_fH_10", "week_fL_10", "week_fr_10",
    "week_g_1", "week_g_youbi", "week_g_icon", "week_gH_10", "week_gL_10", "week_gr_10",
};
static const char* sDayPaneNamesJP[7] = {
    "week_a", "week_b", "week_c", "week_d", "week_e", "week_f", "week_g",
};
static const char* sDayColorPaneNames[7] = {
    "week_aB0", "week_bB0", "week_cB0", "week_dB0", "week_eB0", "week_fB0", "week_gB0",
};
static const char* sBoxNames[30] = {
    "week_a_youbi", "week_a_icon", "week_aH_do_cnt", "week_aH_cnt", "week_aL_do_cnt", "week_aL_cnt",
    "week_b_youbi", "week_b_icon", "week_bH_do_cnt", "week_bH_cnt", "week_bL_do_cnt", "week_bL_cnt",
    "week_c_youbi", "week_c_icon", "week_cH_do_cnt", "week_cH_cnt", "week_cL_do_cnt", "week_cL_cnt",
    "week_d_youbi", "week_d_icon", "week_dH_do_cnt", "week_dH_cnt", "week_dL_do_cnt", "week_dL_cnt",
    "week_e_youbi", "week_e_icon", "week_eH_do_cnt", "week_eH_cnt", "week_eL_do_cnt", "week_eL_cnt",
};
static const char* sDayPaneNames[5] = {
    "week_a", "week_b", "week_c", "week_d", "week_e",
};

#define SET_COLOR(dst, src)                                                                                  \
    (dst).r = (src).r;                                                                                       \
    (dst).g = (src).g;                                                                                       \
    (dst).b = (src).b;                                                                                       \
    (dst).a = (src).a

WeatherWeek::WeatherWeek(const Vec2& pos, void* arc, const Vec2& size, s32 type)
    : WeatherBase(pos, size, type), mNoServiceLayout(NULL), mNoServiceText(NULL), mHasData(NULL), mDrawFunc(NULL) {
    if (gLanguage == 0) {
        SetupJP(arc);
    } else {
        Setup(arc);
    }
}

WeatherWeek::~WeatherWeek() {
    if (mNoServiceLayout != NULL) {
        delete mNoServiceLayout;
        mNoServiceLayout = NULL;
        mNoServiceText = NULL;
    }

    if (mLayout != NULL) {
        delete mLayout;
        mLayout = NULL;
    }
}

void WeatherWeek::SetupJP(void* arc) {
    char layoutName[0x100] = "week_nJP.brlyt";
    f32 centerX = 0.5f * GetScreenWidth();
    f32 centerY = 228.0f;
    f32 scaleX = gWidescreen ? 1.3684211f : 1.0f;

    mLayout = new ButtonGroup(arc, layoutName, gButtonColors, false);
    mNoServiceLayout = new ButtonGroup(arc, "life_no_service.brlyt", gButtonColors, false);
    mNoServiceText = mNoServiceLayout->FindButton("text");
    if (mNoServiceText == NULL) {
        OSPanic("d_weather_a_week.cpp", 409,
                "text が見つかりません!!\n");
    }

    for (s32 i = 0; i < 7; i++) {
        mDayPanes[i] = mLayout->FindButton(sDayPaneNamesJP[i]);
        if (mDayPanes[i] == NULL) {
            OSReport("%s : ", layoutName);
            OSReport("%sが見つかりません!!\n", sDayPaneNamesJP[i]);
            OSPanic("d_weather_a_week.cpp", 419, "");
        }
    }

    TextBox* box = mBoxes;
    for (s32 i = 0; i < 45; i++, box++) {
        box->mPane = mLayout->FindButton(sBoxNamesJP[i]);
        if (box->mPane == NULL) {
            OSReport("%s : ", layoutName);
            OSReport("%sが見つかりません!!\n", sBoxNamesJP[i]);
            OSPanic("d_weather_a_week.cpp", 432, "");
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

        switch (i) {
        case 0:
            SET_COLOR(box->mColor, gColorRed);
            break;
        case 1:
            SET_COLOR(box->mColor, gColorCyan);
            break;
        case 2:
            SET_COLOR(box->mColor, gColorWhite);
            SET_COLOR(box->mShadowColor, gColorDarkGray);
            break;
        case 6:
        case 12:
        case 18:
        case 24:
        case 30:
        case 36:
        case 42:
            SET_COLOR(box->mColor, gColorPink);
            SET_COLOR(box->mShadowColor, gColorRed2);
            break;
        case 7:
        case 13:
        case 19:
        case 25:
        case 31:
        case 37:
        case 43:
            SET_COLOR(box->mColor, gColorCyan2);
            SET_COLOR(box->mShadowColor, gColorBlue);
            break;
        default:
            SET_COLOR(box->mColor, gColorLightGray);
            SET_COLOR(box->mShadowColor, gColorDarkGray2);
            break;
        }
    }

    mHasData = &WeatherWeek::HasDataJP;
}

void WeatherWeek::Setup(void* arc) {
    char layoutName[0x100] = "week_nWW.brlyt";
    f32 centerX = 0.5f * GetScreenWidth();
    f32 centerY = 228.0f;
    f32 scaleX = gWidescreen ? 1.3684211f : 1.0f;

    mLayout = new ButtonGroup(arc, layoutName, gButtonColors, false);
    mNoServiceLayout = new ButtonGroup(arc, "life_no_service.brlyt", gButtonColors, false);
    mNoServiceText = mNoServiceLayout->FindButton("text");
    if (mNoServiceText == NULL) {
        OSPanic("d_weather_a_week.cpp", 499,
                "text が見つかりません!!\n");
    }

    TextBox* box = mBoxes;
    for (s32 i = 0; i < 30; i++, box++) {
        box->mPane = mLayout->FindButton(sBoxNames[i]);
        if (box->mPane == NULL) {
            OSReport("%s : ", layoutName);
            OSReport("%sが見つかりません!!\n", sBoxNames[i]);
            OSPanic("d_weather_a_week.cpp", 512, "");
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

        switch (i) {
        case 2:
        case 3:
        case 8:
        case 9:
        case 14:
        case 15:
        case 20:
        case 21:
        case 26:
        case 27:
            SET_COLOR(box->mColor, gColorOrange);
            SET_COLOR(box->mShadowColor, gColorDarkGray2);
            break;
        case 4:
        case 5:
        case 10:
        case 11:
        case 16:
        case 17:
        case 22:
        case 23:
        case 28:
        case 29:
            SET_COLOR(box->mColor, gColorLightCyan);
            SET_COLOR(box->mShadowColor, gColorDarkGray2);
            break;
        default:
            SET_COLOR(box->mColor, gColorLightGray);
            SET_COLOR(box->mShadowColor, gColorDarkGray2);
            break;
        }
    }

    mHasData = &WeatherWeek::HasData;
}

void WeatherWeek::Reset() {
    mLayout->Reset();
    mNoServiceLayout->Reset();
}

void WeatherWeek::Draw() {
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
            alpha = 255.0f * mScale;
            if (mDrawFunc) {
                (this->*mDrawFunc)(alpha);
            }
        } else {
            mNoServiceText->SetState(6);
            mNoServiceLayout->SetPaneAlpha(alpha);
            mNoServiceLayout->Draw();
        }
    } else {
        mNoServiceText->SetState(4);
        mNoServiceLayout->SetPaneAlpha(alpha);
        mNoServiceLayout->Draw();
    }
}

void WeatherWeek::SetPosition(const Vec2& pos, const f32& scale, const bool& visible, BOOL checkHover) {
    WeatherBase::SetPosition(pos, scale, visible, checkHover);
    mLayout->Calc();
    mNoServiceLayout->Calc();

    City* city = gCurrentCity;
    mDrawFunc = NULL;
    if (city != NULL && city->mForecast != NULL) {
        if (gLanguage == 0) {
            mDrawFunc = &WeatherWeek::DrawWeekJP;
        } else {
            mDrawFunc = &WeatherWeek::DrawWeek;
        }
    }

    nw4r::math::VEC3 trans(mPos.x, mPos.y, 0.0f);
    PSMTXTrans(gModelMtx, trans.x, trans.y, trans.z);
    mLayout->SetViewMtx(gModelMtx);
    mNoServiceLayout->SetViewMtx(gModelMtx);
}

BOOL WeatherWeek::HasDataJP(CityForecast* forecast) {
    WeekForecast* week = forecast->mEntry->mWeek;
    for (s32 i = 0; i < 7; i++, week++) {
        u32 code = week->mWeather;
        if (gForecastData->FindWeatherInfo(code) != NULL) {
            return TRUE;
        }
    }

    return FALSE;
}

BOOL WeatherWeek::HasData(CityForecast* forecast) {
    WeekForecast* week = forecast->mEntry->mWeek;
    for (s32 i = 0; i < 5; i++, week++) {
        u32 code = week->mWeather;
        if (gForecastData->FindWeatherInfo(code) != NULL) {
            return TRUE;
        }
    }

    return FALSE;
}

static inline void FormatTemperature(wchar_t*& buf, s32 temp) {
    buf = sTextBuf;
    if (temp <= -128) {
        wcscpy(buf, L"--");
    } else {
        FormatNumber(temp, buf, 4, FALSE);
    }
}

void WeatherWeek::DrawWeekJP(const s32& alpha) {
    CityInfo* cityInfo = gCurrentCity->GetInfo();
    CityForecast* forecast = gCurrentCity->GetForecast();
    nw4r::ut::Color color;
    f32 fade = alpha / 255.0f;
    OSCalendarTime time;

    if (cityInfo == NULL) {
        return;
    }

    Vec2F center(mPos.x + 0.5f * GetScreenWidth(), 228.0f - mPos.y);
    Vec2 pos;
    SetDefaultGXState();
    SetOrthoProjection();
    f32 scale = 1.0f;
    mWriter.SetFont(*gSysFont);
    mWriter.SetDrawFlag(0x111);
    mWriter.SetCharSpace(scale * gUnkSceneFloat);
    mWriter.SetupGX();

    mBoxes[0].mColor.a = alpha;
    mWriter.SetScale(mBoxes[0].mScaleX, mBoxes[0].mScaleY);
    mWriter.SetTextColor(mBoxes[0].mColor);
    mWriter.SetCursor(mBoxes[0].mX + mPos.x, mBoxes[0].mY - mPos.y);
    if (gTempUnit == 0) {
        mWriter.Print(L"(\xFF9F" L"C)");
    } else {
        mWriter.Print(L"(\xFF9F" L"F)");
    }

    mBoxes[1].mColor.a = alpha;
    mWriter.SetScale(mBoxes[1].mScaleX, mBoxes[1].mScaleY);
    mWriter.SetTextColor(mBoxes[1].mColor);
    mWriter.SetCursor(mBoxes[1].mX + mPos.x, mBoxes[1].mY - mPos.y);
    if (gTempUnit == 0) {
        mWriter.Print(L"(\xFF9F" L"C)");
    } else {
        mWriter.Print(L"(\xFF9F" L"F)");
    }

    mBoxes[2].mColor.a = alpha;
    mWriter.SetScale(mBoxes[2].mScaleX, mBoxes[2].mScaleY);
    mWriter.SetTextColor(mBoxes[2].mColor);
    mWriter.SetCursor(mBoxes[2].mX + mPos.x, mBoxes[2].mY - mPos.y);
    mWriter.Print(L"(%)");

    s32 day;
    WeekForecast* week;
    u32 minutes;
    wchar_t* buf;
    buf = sTextBuf;
    week = forecast->mEntry->mWeek;
    day = 0;
    minutes = forecast->mEntry->mTime;
    minutes += 1440;
    for (s32 i = 0; i < 7; i++, minutes += 1440, day++, week++) {
        TextBox* box;
        s32 temp;
        size_t len;

        SetDefaultGXState();
        SetOrthoProjection();
        GXSetTevColorIn(GX_TEVSTAGE0, (GXTevColorArg)4, (GXTevColorArg)8, (GXTevColorArg)2, (GXTevColorArg)4);

        if (gTempUnit == 0) {
            temp = week->mMaxC;
        } else {
            temp = week->mMaxF;
        }
        FormatTemperature(buf, temp);
        len = wcslen(buf);
        box = &mBoxes[sDayBoxesJP[day][3]];
        pos.x = box->mX + mPos.x;
        pos.y = box->mY - mPos.y;
        if (len == 1) {
            pos.x += box->mScaleX * (0.5f * sGlyphTextures[18].width);
        }
        box->mColor.a = alpha;
        box->mShadowColor.a = 0;
        DrawNumCentered(sTextBuf, &pos, box->mScaleX, box->mScaleY, 0.0f, &box->mColor, &box->mShadowColor);

        if (gTempUnit == 0) {
            temp = week->mMinC;
        } else {
            temp = week->mMinF;
        }
        FormatTemperature(buf, temp);
        len = wcslen(buf);
        box = &mBoxes[sDayBoxesJP[day][4]];
        pos.x = box->mX + mPos.x;
        pos.y = box->mY - mPos.y;
        if (len == 1) {
            pos.x += box->mScaleX * (0.5f * sGlyphTextures[18].width);
        }
        box->mColor.a = alpha;
        box->mShadowColor.a = 0;
        DrawNumCentered(sTextBuf, &pos, box->mScaleX, box->mScaleY, 0.0f, &box->mColor, &box->mShadowColor);

        SetDefaultGXState();
        SetOrthoProjection();
        s32 percent = week->mPercent;
        if (percent == 0xFF) {
            wcscpy(buf, L"--");
        } else {
            FormatNumber(percent, buf, 4, FALSE);
        }
        len = wcslen(buf);
        box = &mBoxes[sDayBoxesJP[day][5]];
        pos.x = box->mX + mPos.x;
        pos.y = box->mY - mPos.y;
        if (len == 1) {
            pos.x += box->mScaleX * (0.5f * sGlyphTextures[0].width);
        }
        box->mColor.a = alpha;
        box->mShadowColor.a = alpha;
        DrawDateCentered(sTextBuf, &pos, box->mScaleX, box->mScaleY, &box->mColor, &box->mShadowColor);

        MinutesToCalendarTime(minutes, &time);
        FormatNumber(time.mday, sTextBuf, 2, FALSE);
        box = &mBoxes[sDayBoxesJP[day][0]];
        pos.x = box->mX + mPos.x;
        pos.y = box->mY - mPos.y;
        box->mColor.a = alpha;
        box->mShadowColor.a = alpha;
        DrawDateCentered(sTextBuf, &pos, box->mScaleX, box->mScaleY, &box->mColor, &box->mShadowColor);

        DrawIcon(&mBoxes[sDayBoxesJP[day][1]], gWeekdayTexturesJP[time.wday], alpha);
        switch (time.wday) {
        case 0:
            color.r = sColorSunday.r;
            color.g = sColorSunday.g;
            color.b = sColorSunday.b;
            color.a = 255.0f * fade;
            break;
        case 6:
            color.r = sColorSaturday.r;
            color.g = sColorSaturday.g;
            color.b = sColorSaturday.b;
            color.a = 255.0f * fade;
            break;
        default:
            color.r = sColorWeekday.r;
            color.g = sColorWeekday.g;
            color.b = sColorWeekday.b;
            color.a = 128.0f * fade;
            break;
        }
        mDayPanes[day]->SetPaneColor(sDayColorPaneNames[day], color, TRUE);

        box = &mBoxes[sDayBoxesJP[day][2]];
        f32 y = box->mY - mPos.y;
        f32 x = box->mX + mPos.x;
        pos.x = x;
        pos.y = y;
        WeatherInfo* info = gForecastData->FindWeatherInfo(week->mWeather);
        if (week->mWeather != 0xFFFF && info != NULL) {
            DrawWeatherIcon(info->mType->mIcon, &pos, box->mScaleX, alpha);
        } else {
            wcscpy(sTextBuf, L"--");
            box->mColor.a = alpha;
            box->mShadowColor.a = alpha;
            DrawDateCentered(sTextBuf, &pos, box->mScaleX, box->mScaleY, &box->mColor, &box->mShadowColor);
        }
    }
}

void WeatherWeek::DrawWeek(const s32& alpha) {
    CityForecast* forecast = gCurrentCity->mForecast;
    OSCalendarTime time;

    if (gCurrentCity->mInfo == NULL) {
        return;
    }

    Vec2F center(mPos.x + 0.5f * GetScreenWidth(), 228.0f - mPos.y);
    Vec2 pos;
    const s32* boxes;
    WeekForecast* week;
    u32 minutes;
    week = forecast->mEntry->mWeek;
    boxes = sDayBoxes[0];
    minutes = forecast->mEntry->mTime;
    minutes += 1440;
    for (s32 i = 0; i < 5; i++, minutes += 1440, boxes += 6, week++) {
        TextBox* box;
        wchar_t* buf;
        size_t len;

        SetDefaultGXState();
        SetOrthoProjection();

        if (gTempUnit == 0) {
            FormatTemperature(buf, week->mMaxC);
        } else {
            FormatTemperature(buf, week->mMaxF);
        }
        len = wcslen(buf);
        if (gRegion == 2) {
            wcscat(sTextBuf, L"\xFF9F");
            box = &mBoxes[boxes[2]];
        } else {
            box = &mBoxes[boxes[3]];
        }
        pos.x = box->mX + mPos.x;
        pos.y = box->mY - mPos.y;
        if (len == 1) {
            pos.x += box->mScaleX * (0.5f * sGlyphTextures[33].width);
        }
        box->mColor.a = alpha;
        box->mShadowColor.a = alpha;
        DrawTempCentered(sTextBuf, &pos, box->mScaleX, box->mScaleY, &box->mColor, &box->mShadowColor);

        if (gTempUnit == 0) {
            FormatTemperature(buf, week->mMinC);
        } else {
            FormatTemperature(buf, week->mMinF);
        }
        len = wcslen(buf);
        if (gRegion == 2) {
            wcscat(sTextBuf, L"\xFF9F");
            box = &mBoxes[boxes[4]];
        } else {
            box = &mBoxes[boxes[5]];
        }
        pos.x = box->mX + mPos.x;
        pos.y = box->mY - mPos.y;
        if (len == 1) {
            pos.x += box->mScaleX * (0.5f * sGlyphTextures[33].width);
        }
        box->mColor.a = alpha;
        box->mShadowColor.a = alpha;
        DrawTempCentered(sTextBuf, &pos, box->mScaleX, box->mScaleY, &box->mColor, &box->mShadowColor);

        MinutesToCalendarTime(minutes, &time);
        box = &mBoxes[boxes[0]];
        switch (time.wday) {
        case 0:
        case 6:
            SET_COLOR(box->mColor, gColorOrange);
            break;
        default:
            SET_COLOR(box->mColor, gColorLightGray);
            break;
        }
        box->mColor.a = alpha;
        DrawIconLarge(box, gWeekdayTextures[gLanguage][time.wday], alpha);

        box = &mBoxes[boxes[1]];
        f32 y = box->mY - mPos.y;
        f32 x = box->mX + mPos.x;
        pos.x = x;
        pos.y = y;
        u32 code = week->mWeather;
        WeatherInfo* info = gForecastData->FindWeatherInfo(code);
        if (week->mWeather != 0xFFFF && info != NULL) {
            DrawWeatherIcon(info->mType->mIcon, &pos, box->mScaleX, alpha);
        } else {
            wcscpy(sTextBuf, L"--");
            box->mColor.a = alpha;
            box->mShadowColor.a = alpha;
            DrawTempCentered(sTextBuf, &pos, box->mScaleX, box->mScaleY, &box->mColor, &box->mShadowColor);
        }
    }
}
