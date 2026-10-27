// d_weather_base_day.cpp: a single day's forecast panel
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

wchar_t* FormatNumber(s32 value, wchar_t* pBuf, s32 digits, BOOL zeroPad);
void DrawWeatherIcon(u16 icon, const Vec2* pos, f32 scale, s32 alpha);

static const f32 sBoxScalesJP[20] = {
    1.0f, 0.7f, 0.7f, 0.7f, 0.7f, 0.7f, 0.7f, 0.7f, 0.7f, 0.6f,
    0.7f, 1.0f, 0.55f, 1.0f, 0.55f, 0.65f, 0.65f, 0.65f, 0.65f, 1.0f,
};
static const s32 sRainBoxesJP[4] = {15, 16, 17, 18};
static const f32 sBoxScales[5] = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f};

static const char* sBoxNamesJP[20] = {
    "telop",       "text_kion",   "text_rain_a", "text_rain_b", "text_rain_c", "text_rain_d", "text_kionH",
    "text_kionL",  "text_ondo",   "text_rain",   "text_percent", "kionH_1-100", "kionH_e",     "kionL_1-100",
    "kionL_e",     "rain_a_10",   "rain_b_10",   "rain_c_10",   "rain_d_10",   "icon",
};
static const char* sBoxNames[5] = {
    "telop",
    "wind_telop",
    "kion_doF_cnt",
    "kion_do_cnt",
    "icon",
};

static inline void SetColor(nw4r::ut::Color& dst, const Color& src) {
    dst.r = src.r;
    dst.g = src.g;
    dst.b = src.b;
    dst.a = src.a;
}

typedef const wchar_t* WindDirNames[17];
typedef const wchar_t* WindUnitNames[2];

static inline const wchar_t* GetWindDirName(u8 dir) {
    if (gLanguage > 6) {
        return ((WindDirNames*)gWindDirNamesUS)[1][dir];
    } else if (gRegion == 1) {
        return ((WindDirNames*)gWindDirNamesEU)[gLanguage][dir];
    } else {
        return ((WindDirNames*)gWindDirNamesUS)[gLanguage][dir];
    }
}

static inline const wchar_t* GetWindUnitName() {
    s32 unit = gWindUnit;
    if (gRegion == 1) {
        return ((WindUnitNames*)gWindUnitNames)[gLanguage][unit];
    } else {
        return ((WindUnitNames*)gWindUnitNames2)[gLanguage][unit];
    }
}

WeatherBaseDay::WeatherBaseDay(const Vec2& pos, void* arc, const Vec2& size, s32 type)
    : WeatherBase(pos, size, type), mNoServiceLayout(NULL), mNoServiceText(NULL) {
    mStartHour = 0;
    if (gLanguage == 0) {
        SetupJP(arc);
    } else {
        Setup(arc);
    }
}

WeatherBaseDay::~WeatherBaseDay() {
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

static f32 sIconWidthJP = 180.0f;
static f32 sIconHeightJP = 120.0f;

void WeatherBaseDay::SetupJP(void* arc) {
    char layoutName[0x100] = "day_nJP.brlyt";
    f32 centerX = 0.5f * GetScreenWidth();
    f32 centerY = 228.0f;
    f32 scaleX = gWidescreen ? 1.3684211f : 1.0f;

    mLayout = new ButtonGroup(arc, layoutName, gButtonColors, true);
    mNoServiceLayout = new ButtonGroup(arc, "life_no_service.brlyt", gButtonColors, false);
    mNoServiceText = mNoServiceLayout->FindButton("text");
    if (mNoServiceText == NULL) {
        OSPanic("d_weather_base_day.cpp", 178,
                "text が見つかりません!!\n");
    }

    TextBox* box = mBoxes;
    for (s32 i = 0; i < 20; i++, box++) {
        box->mPane = mLayout->FindButton(sBoxNamesJP[i]);
        if (box->mPane == NULL) {
            OSReport("%sが見つかりません!!\n", sBoxNamesJP[i]);
            OSPanic("d_weather_base_day.cpp", 189, "");
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
        case 6:
            SetColor(box->mColor, gColorRed);
            break;
        case 7:
            SetColor(box->mColor, gColorCyan);
            break;
        case 11:
            SetColor(box->mColor, gColorPink);
            SetColor(box->mShadowColor, gColorRed2);
            break;
        case 12:
        case 14:
            SetColor(box->mColor, gColorLightGray);
            SetColor(box->mShadowColor, gColorDarkGray2);
            break;
        case 13:
            SetColor(box->mColor, gColorCyan2);
            SetColor(box->mShadowColor, gColorBlue);
            break;
        case 15:
        case 16:
        case 17:
        case 18:
            SetColor(box->mColor, gColorWhite2);
            SetColor(box->mShadowColor, gColorDarkGray3);
            break;
        default:
            SetColor(box->mColor, gColorWhite);
            SetColor(box->mShadowColor, gColorDarkGray);
            break;
        }
    }

    mIconBox = &mBoxes[19];
    mIconBox->mWidth = sIconWidthJP;
    mIconBox->mHeight = sIconHeightJP;
}

static f32 sIconWidth = 180.0f;
static f32 sIconHeight = 120.0f;

void WeatherBaseDay::Setup(void* arc) {
    char layoutName[0x100] = "day_nWW.brlyt";
    f32 centerX = 0.5f * GetScreenWidth();
    f32 centerY = 228.0f;
    f32 scaleX = gWidescreen ? 1.3684211f : 1.0f;

    mLayout = new ButtonGroup(arc, layoutName, gButtonColors, true);
    mNoServiceLayout = new ButtonGroup(arc, "life_no_service.brlyt", gButtonColors, false);
    mNoServiceText = mNoServiceLayout->FindButton("text");
    if (mNoServiceText == NULL) {
        OSPanic("d_weather_base_day.cpp", 252,
                "text が見つかりません!!\n");
    }

    TextBox* box = mBoxes;
    for (s32 i = 0; i < 5; i++, box++) {
        box->mPane = mLayout->FindButton(sBoxNames[i]);
        if (box->mPane == NULL) {
            OSReport("%sが見つかりません!!\n", sBoxNames[i]);
            OSPanic("d_weather_base_day.cpp", 263, "");
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
        SetColor(box->mColor, gColorWhite);
        SetColor(box->mShadowColor, gColorDarkGray);
    }

    s32 margin = gWidescreen ? 36 : 28;
    mBoxes[1].mX = GetScreenWidth() - margin;
    f32 left = mBoxes[0].mX + 0.5f * mBoxes[0].mWidth;
    mBoxes[1].mWidth = mBoxes[1].mX - (20.0f + left);
    mIconBox = &mBoxes[4];
    mIconBox->mWidth = sIconWidth;
    mIconBox->mHeight = sIconHeight;
}

void WeatherBaseDay::SetPosition(const Vec2& pos, const f32& scale, const bool& visible, BOOL checkHover) {
    WeatherBase::SetPosition(pos, scale, visible, checkHover);

    City* city = gCurrentCity;
    mStartHour = 0;
    if (city != NULL) {
        if (city->mForecast != NULL) {
            mStartHour = 0;
        } else if (city->mSummary != NULL) {
            mStartHour = 0;
        }
    }

    if (!IsEmpty()) {
        UpdateHover(checkHover);
    }
}

void WeatherBaseDay::DrawTextFit(TextBox* box, const wchar_t* text, s32 alpha) {
    Vec2 pos;
    f32 scale;

    pos.x = box->mX;
    pos.y = box->mY - mPos.y;
    mWriter.SetScale(box->mScaleX, box->mScaleY);

    f32 width = mWriter.CalcStringWidth(text);
    if (width > box->mWidth) {
        scale = (box->mScaleX * box->mWidth) / width;
    } else {
        scale = box->mScaleX;
    }

    mWriter.SetDrawFlag(0x122);
    mWriter.SetupGX();
    box->mShadowColor.a = alpha;
    box->mColor.a = alpha;
    mWriter.SetLineSpace(0.0f);
    mWriter.SetCharSpace(0.0f);
    mWriter.SetScale(scale, scale);
    mWriter.SetTextColor(box->mShadowColor);
    mWriter.SetCursor(1.0f + pos.x, 1.0f + pos.y);
    mWriter.Print(text);
    mWriter.SetTextColor(box->mColor);
    mWriter.SetCursor(pos.x, pos.y);
    mWriter.Print(text);
}

static inline void FormatTemp(s32 temp) {
    if (temp <= -128) {
        wcscpy(sTextBuf, L"--");
    } else {
        FormatNumber(temp, sTextBuf, 4, FALSE);
    }
}

#define DRAW_TEMP_C(temp, alpha) \
    { \
        wchar_t* buf = sTextBuf; \
        if (temp <= -128) { \
            wcscpy(buf, L"--"); \
        } else { \
            FormatNumber(temp, buf, 4, FALSE); \
        } \
     \
        wcscat(sTextBuf, L"\xFF9F" L"C"); \
        Vec2F pos(mBoxes[2].mX + mPos.x, mBoxes[2].mY - mPos.y); \
        mBoxes[2].mColor.a = alpha; \
        mBoxes[2].mShadowColor.a = alpha; \
        DrawTempCentered(sTextBuf, &pos, mBoxes[2].mScaleX, mBoxes[2].mScaleY, &mBoxes[2].mColor, \
                         &mBoxes[2].mShadowColor); \
    }

void WeatherBaseDay::DrawTempC(s32 temp, s32 alpha) {
    DRAW_TEMP_C(temp, alpha);
}

void WeatherBaseDay::DrawTemp(s32 temp, s32 alpha) {
    TextBox* box;
    wchar_t* buf = sTextBuf;

    if (temp <= -128) {
        wcscpy(buf, L"--");
    } else {
        FormatNumber(temp, buf, 4, FALSE);
    }

    s32 len = wcslen(buf);
    if (gRegion == 1) {
        wcscat(sTextBuf, L"\xFF9F");
        box = &mBoxes[3];
    } else {
        wcscat(sTextBuf, L"\xFF9F" L"F");
        box = &mBoxes[2];
    }

    Vec2F pos(box->mX + mPos.x, box->mY - mPos.y);
    box->mColor.a = alpha;
    box->mShadowColor.a = alpha;
    DrawTempCentered(sTextBuf, &pos, box->mScaleX, box->mScaleY, &box->mColor, &box->mShadowColor);
}

inline void WeatherBaseDay::PrintBox(TextBox* box, const wchar_t* text, s32 alpha) {
    box->mColor.a = alpha;
    mWriter.SetScale(box->mScaleX, box->mScaleY);
    mWriter.SetTextColor(box->mColor);
    mWriter.SetCursor(box->mX + mPos.x, box->mY - mPos.y);
    mWriter.Print(text);
}

static inline void FormatHours(s32 start, s32 end) {
    wchar_t* p = FormatNumber(start, sTextBuf, 4, FALSE);
    p[0] = L'-';
    p[1] = L'\0';
    p = FormatNumber(end, p + 1, 4, FALSE);
    p[0] = L'\x6642';
    p[1] = L'\0';
}

static inline void FormatTempDiff(s8 diff) {
    if (diff <= -128) {
        wcscpy(sTextBuf, L"--");
    } else {
        wchar_t* p = sTextBuf;
        *p++ = L'(';
        if (diff > 0) {
            *p++ = L'+';
        }
        FormatNumber(diff, p, 4, FALSE);
        wcscat(sTextBuf, L")");
    }
}

#define PRINT_BOX(box, text, alpha)                                                                          \
    (box)->mColor.a = alpha;                                                                                 \
    mWriter.SetScale((box)->mScaleX, (box)->mScaleY);                                                        \
    mWriter.SetTextColor((box)->mColor);                                                                     \
    mWriter.SetCursor((box)->mX + mPos.x, (box)->mY - mPos.y);                                               \
    mWriter.Print(text)

#define DRAW_NUM_BOX(box, alpha)                                                                             \
    pos.x = (box)->mX + mPos.x;                                                                              \
    pos.y = (box)->mY - mPos.y;                                                                              \
    (box)->mColor.a = alpha;                                                                                 \
    (box)->mShadowColor.a = 0;                                                                               \
    DrawNumRightAligned(buf, &pos, (box)->mScaleX, (box)->mScaleY, &(box)->mColor, &(box)->mShadowColor)

void WeatherBaseDay::DrawDayJP(DayForecast* day, WeatherInfo* weather, s32 alpha, f32 scale) {
    s32 hour = mStartHour;
    Vec2 pos;
    wchar_t* buf;
    wchar_t* p;
    s32 temp;

    pos.x = mBoxes[19].mX + mPos.x;
    pos.y = mBoxes[19].mY - mPos.y;

    f32 iconScale = scale * mBoxes[19].mScaleX;
    if ((weather->mType->mIcon & 0x7FFF) >= 100) {
        iconScale *= 0.8f;
    }
    DrawWeatherIcon(weather->mType->mIcon, &pos, iconScale, alpha);

    SetDefaultGXState();
    SetOrthoProjection();
    mWriter.SetFont(*gSysFont);
    mWriter.SetDrawFlag(0x111);
    mWriter.SetScale(scale);
    mWriter.SetCharSpace(scale * gUnkSceneFloat);
    mWriter.SetupGX();

    TextBox* boxes = mBoxes;
    DrawText(&boxes[0], weather->mText, alpha);
    PRINT_BOX(&boxes[1], L"\x6C17\x6E29 (\x524D\x65E5\x6BD4)", alpha);

    p = FormatNumber(hour, sTextBuf, 4, FALSE);
    *p++ = L'-';
    *p = L'\0';
    p = FormatNumber(hour + 6, p, 4, FALSE);
    p[0] = L'\x6642';
    p[1] = L'\0';
    PRINT_BOX(&boxes[2], sTextBuf, alpha);

    p = FormatNumber(hour + 6, sTextBuf, 4, FALSE);
    *p++ = L'-';
    *p = L'\0';
    p = FormatNumber(hour + 12, p, 4, FALSE);
    p[0] = L'\x6642';
    p[1] = L'\0';
    PRINT_BOX(&boxes[3], sTextBuf, alpha);

    p = FormatNumber(hour + 12, sTextBuf, 4, FALSE);
    *p++ = L'-';
    *p = L'\0';
    p = FormatNumber(hour + 18, p, 4, FALSE);
    p[0] = L'\x6642';
    p[1] = L'\0';
    PRINT_BOX(&boxes[4], sTextBuf, alpha);

    p = FormatNumber(hour + 18, sTextBuf, 4, FALSE);
    *p++ = L'-';
    s32 end = hour + 24;
    *p = L'\0';
    if (end > 24) {
        end -= 24;
    }
    p = FormatNumber(end, p, 4, FALSE);
    p[0] = L'\x6642';
    p[1] = L'\0';
    PRINT_BOX(&boxes[5], sTextBuf, alpha);

    PRINT_BOX(&boxes[6], L"\x6700\x9AD8", alpha);
    PRINT_BOX(&boxes[7], L"\x6700\x4F4E", alpha);
    boxes[8].mColor.a = alpha;
    mWriter.SetScale(boxes[8].mScaleX, boxes[8].mScaleY);
    mWriter.SetTextColor(boxes[8].mColor);
    mWriter.SetCursor(boxes[8].mX + mPos.x, boxes[8].mY - mPos.y);
    if (gTempUnit == 0) {
        mWriter.Print(L"(\xFF9F" L"C)");
    } else {
        mWriter.Print(L"(\xFF9F" L"F)");
    }
    PRINT_BOX(&boxes[9], L"\x964D\x6C34\x78BA\x7387", alpha);
    PRINT_BOX(&boxes[10], L"(%)", alpha);

    SetDefaultGXState();
    SetOrthoProjection();
    GXSetTevColorIn(GX_TEVSTAGE0, (GXTevColorArg)4, (GXTevColorArg)8, (GXTevColorArg)2, (GXTevColorArg)4);

    if (gTempUnit == 0) {
        temp = day->mMaxC;
    } else {
        temp = day->mMaxF;
    }
    buf = sTextBuf;
    if (temp <= -128) {
        wcscpy(buf, L"--");
    } else {
        FormatNumber(temp, buf, 4, FALSE);
    }
    DRAW_NUM_BOX(&mBoxes[11], alpha);

    if (gTempUnit == 0) {
        temp = day->mMaxDiffC;
    } else {
        temp = day->mMaxDiffF;
    }
    p = sTextBuf;
    if (temp <= -128) {
        wcscpy(p, L"--");
    } else {
        *p++ = L'(';
        if (temp > 0) {
            *p++ = L'+';
        }
        FormatNumber(temp, p, 4, FALSE);
        wcscat(sTextBuf, L")");
    }
    buf = sTextBuf;
    DRAW_NUM_BOX(&mBoxes[12], alpha);

    if (gTempUnit == 0) {
        temp = day->mMinC;
    } else {
        temp = day->mMinF;
    }
    buf = sTextBuf;
    if (temp <= -128) {
        wcscpy(buf, L"--");
    } else {
        FormatNumber(temp, buf, 4, FALSE);
    }
    DRAW_NUM_BOX(&mBoxes[13], alpha);

    if (gTempUnit == 0) {
        temp = day->mMinDiffC;
    } else {
        temp = day->mMinDiffF;
    }
    p = sTextBuf;
    if (temp <= -128) {
        wcscpy(p, L"--");
    } else {
        *p++ = L'(';
        if (temp > 0) {
            *p++ = L'+';
        }
        FormatNumber(temp, p, 4, FALSE);
        wcscat(sTextBuf, L")");
    }
    buf = sTextBuf;
    DRAW_NUM_BOX(&mBoxes[14], alpha);

    SetDefaultGXState();
    SetOrthoProjection();
    buf = sTextBuf;
    for (s32 i = 0; i < 4; i++) {
        s32 percent = day->mPercent[i];
        if (percent == 0xFF) {
            wcscpy(buf, L"--");
        } else {
            FormatNumber(percent, buf, 4, FALSE);
        }

        s32 len = wcslen(buf);
        TextBox* box = &mBoxes[sRainBoxesJP[i]];
        pos.x = box->mX + mPos.x;
        pos.y = box->mY - mPos.y;
        box->mColor.a = alpha;
        box->mShadowColor.a = alpha;
        DrawDateCentered(sTextBuf, &pos, box->mScaleX, box->mScaleY, &box->mColor, &box->mShadowColor);
    }
}

#define FORMAT_WIND(dir, speed) \
    { \
        wchar_t* p; \
     \
        switch (gLanguage) { \
        case 6: \
            wcscpy(sTextBuf, GetWindDirName(dir)); \
            if (dir != 0) { \
                wcscat(sTextBuf, L", "); \
                p = &sTextBuf[wcslen(sTextBuf)]; \
                if (speed != 0xFF) { \
                    FormatNumber(speed, p, 4, FALSE); \
                } else { \
                    wcscat(sTextBuf, L"--"); \
                } \
                wcscat(sTextBuf, L" "); \
                wcscat(sTextBuf, GetWindUnitName()); \
            } \
            break; \
        case 5: \
            if (dir == 0) { \
                wcscpy(sTextBuf, GetWindDirName(dir)); \
            } else { \
                wcscpy(sTextBuf, L"Vento da "); \
                wcscat(sTextBuf, GetWindDirName(dir)); \
                wcscat(sTextBuf, L", "); \
                p = &sTextBuf[wcslen(sTextBuf)]; \
                if (speed != 0xFF) { \
                    FormatNumber(speed, p, 4, FALSE); \
                } else { \
                    wcscat(sTextBuf, L"--"); \
                } \
                wcscat(sTextBuf, L" "); \
                wcscat(sTextBuf, GetWindUnitName()); \
            } \
            break; \
        case 4: \
            wcscpy(sTextBuf, GetWindDirName(dir)); \
            if (dir != 0) { \
                wcscat(sTextBuf, L", "); \
                p = &sTextBuf[wcslen(sTextBuf)]; \
                if (speed != 0xFF) { \
                    FormatNumber(speed, p, 4, FALSE); \
                } else { \
                    wcscat(sTextBuf, L"--"); \
                } \
                wcscat(sTextBuf, L" "); \
                wcscat(sTextBuf, GetWindUnitName()); \
            } \
            break; \
        case 3: \
            wcscpy(sTextBuf, GetWindDirName(dir)); \
            if (dir != 0) { \
                wcscat(sTextBuf, L", "); \
                p = &sTextBuf[wcslen(sTextBuf)]; \
                if (speed != 0xFF) { \
                    FormatNumber(speed, p, 4, FALSE); \
                } else { \
                    wcscat(sTextBuf, L"--"); \
                } \
                wcscat(sTextBuf, L" "); \
                wcscat(sTextBuf, GetWindUnitName()); \
            } \
            break; \
        case 2: \
            if (dir == 0) { \
                wcscpy(sTextBuf, GetWindDirName(dir)); \
            } else { \
                wcscpy(sTextBuf, L"Wind aus "); \
                wcscat(sTextBuf, GetWindDirName(dir)); \
                wcscat(sTextBuf, L", "); \
                p = &sTextBuf[wcslen(sTextBuf)]; \
                if (speed != 0xFF) { \
                    FormatNumber(speed, p, 4, FALSE); \
                } else { \
                    wcscat(sTextBuf, L"--"); \
                } \
                wcscat(sTextBuf, L" "); \
                wcscat(sTextBuf, GetWindUnitName()); \
            } \
            break; \
        default: \
            FormatWindDefault(dir, speed); \
            break; \
        } \
    }

void WeatherBaseDay::DrawDay(DayForecast* day, WeatherInfo* weather, s32 alpha, f32 scale) {
    Vec2F pos(mBoxes[4].mX + mPos.x, mBoxes[4].mY - mPos.y);
    f32 iconScale = scale * mBoxes[4].mScaleX;
    DrawWeatherIcon(weather->mType->mIcon, &pos, iconScale, alpha);

    SetDefaultGXState();
    SetOrthoProjection();
    mWriter.SetFont(*gSysFont);
    mWriter.SetDrawFlag(0x111);
    mWriter.SetScale(scale);
    mWriter.SetCharSpace(scale * gUnkSceneFloat);
    mWriter.SetupGX();

    DrawText(&mBoxes[0], weather->mText, alpha);

    u8 dir = day->mWindDirection;
    if (dir != 0xFF) {
        u8 speed = gWindUnit == 0 ? day->mWindSpeedMph : day->mWindSpeedKmh;
        FORMAT_WIND(dir, speed);
        DrawTextFit(&mBoxes[1], sTextBuf, alpha);
    }

    SetDefaultGXState();
    SetOrthoProjection();
    GXSetTevColorIn(GX_TEVSTAGE0, (GXTevColorArg)4, (GXTevColorArg)8, (GXTevColorArg)2, (GXTevColorArg)4);
    if (gTempUnit == 0) {
        s32 maxC = day->mMaxC;
        DRAW_TEMP_C(maxC, alpha);
    } else {
        DrawTemp(day->mMaxF, alpha);
    }
}

void WeatherBaseDay::FormatWind(u8 dir, u8 speed) {
    FORMAT_WIND(dir, speed);
}

void WeatherBaseDay::FormatWindDefault(u8 dir, u8 speed) {
    if (gRegion == 1) {
        if (dir != 0) {
            if (speed != 0xFF) {
                swprintf(sTextBuf, 0x100, L"%ls %d %ls", GetWindDirName(dir), speed, GetWindUnitName());
            } else {
                swprintf(sTextBuf, 0x100, L"%ls -- %ls", GetWindDirName(dir), GetWindUnitName());
            }
        } else if (speed != 0xFF) {
            swprintf(sTextBuf, 0x100, L"%d %ls", speed, GetWindUnitName());
        } else {
            swprintf(sTextBuf, 0x100, L"-- %ls", speed, GetWindUnitName());
        }
    } else {
        wcscpy(sTextBuf, GetWindDirName(dir));
        if (dir != 0) {
            wcscat(sTextBuf, L" ");
            wchar_t* p = &sTextBuf[wcslen(sTextBuf)];
            if (speed != 0xFF) {
                FormatNumber(speed, p, 4, FALSE);
            } else {
                wcscat(sTextBuf, L"--");
            }
            wcscat(sTextBuf, L" ");
            wcscat(sTextBuf, GetWindUnitName());
        }
    }
}

void WeatherBaseDay::UpdateHover(BOOL enable) {
    TextBox* icon;
    f32 x, y;
    s32 prev = unk98;
    unk98 = -1;
    if (!enable) {
        return;
    }

    icon = mIconBox;
    f32 left = (icon->mX + mPos.x) - 0.5f * icon->mWidth;
    f32 top = (icon->mY - mPos.y) - 0.5f * icon->mHeight;
    f32 right = left + icon->mWidth;
    f32 bottom = top + icon->mHeight;

    for (s32 i = 0; i < WPAD_MAX_CONTROLLERS; i++) {
        if (gKPADLatest[i] >= 0) {
            x = gCursorX[i];
            y = gCursorY[i];
            if (x > left && x < right && y > top && y < bottom) {
                unk98 = i;
                break;
            }
        }
    }

    if (unk98 >= 0) {
        if (mIconBox->mScaleX < 1.3f) {
            mIconBox->mScaleX += 0.05f;
            if (mIconBox->mScaleX >= 1.3f) {
                mIconBox->mScaleX = 1.3f;
            }
        }

        if (prev == -1) {
            StartRumble(unk98, 3, 20);
        }
    } else if (mIconBox->mScaleX > 1.0f) {
        mIconBox->mScaleX -= 0.05f;
        if (mIconBox->mScaleX <= 1.0f) {
            mIconBox->mScaleX = 1.0f;
        }
    }
}

Vec2F WeatherBaseDay::GetIconPos() {
    return Vec2F(mIconBox->mX + mPos.x, mIconBox->mY - mPos.y);
}
