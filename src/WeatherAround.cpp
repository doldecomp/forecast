// d_weather_around.cpp: forecast of the surrounding cities on the globe
#include <channel/WeatherViews.h>
#include <channel/CityLabel.h>
#include <channel/ColorWhite.h>
#include <channel/DrawUtil.h>
#include <channel/ForecastData.h>
#include <channel/GlobeDots.h>
#include <channel/LayoutButton.h>
#include <channel/PointerHistory.h>
#include <channel/SceneBase.h>
#include <channel/SimpleGlobe.h>
#include <channel/System.h>
#include <channel/WeatherScene.h>

#include <nw4r/lyt.h>
#include <nw4r/math.h>
#include <revolution/OS.h>
#include <cstring>
#include <wstring.h>

extern nw4r::ut::Font* gSysFont;
extern nw4r::ut::ResFont* gFutiFont;
extern nw4r::ut::TextWriterBase<wchar_t> gTextWriter;
extern f32 gUnkSceneFloat;
extern s32 gCursorState[4];
extern f32 gTitleOffsetY;
extern s32 gForecastPageStep;
extern Vector2 gCityPos;
extern PointerHistory gPointerHistory;
extern LayoutButton* sHoveredButtons[WPAD_MAX_CONTROLLERS];
extern const s32 gWeatherIconIds[];
extern const s32 gWeatherIconIds2[];

void DrawWeatherIcon(u16 icon, const Vec2* pos, f32 scale, s32 alpha);
wchar_t* FormatNumber(s32 value, wchar_t* pBuf, s32 digits, BOOL zeroPad);
void RequestWeatherSounds(u32 type, f32 volume);
f32 EaseCos(u16 t);
void PlayLoopSE(s32 id, f32 volume, f32 pitch, f32 pan, f32 fade);

// Zero-initialized statics live in .sdata here, not .sbss
#pragma explicit_zero_data on

const s32 gLabelDetailZoom = 3;
const f32 gLabelIconSize = 80.0f;

static f32 sZero = 0.0f;

static const char* sTitleNames[] = {"text_kionH", "text_kionL", "text_ondo"};

static const WeatherAround::Func sPageSetupJP[6] = {
    &WeatherAround::SetupIconsJP,  &WeatherAround::SetupTempJP,  &WeatherAround::SetupRainJP,
    &WeatherAround::SetupIcons2JP, &WeatherAround::SetupTemp2JP, &WeatherAround::SetupRain2JP,
};

static const WeatherAround::Page sPagesJP[6] = {
    {&WeatherAround::DrawIcons, &WeatherAround::GetIconSize, 0},
    {&WeatherAround::DrawTempsToday, &WeatherAround::GetTempSize, 0},
    {&WeatherAround::DrawDetails, &WeatherAround::GetTempSizeSmall, 0},
    {&WeatherAround::DrawIcons, &WeatherAround::GetIconSize, 1},
    {&WeatherAround::DrawTempsTomorrow, &WeatherAround::GetTempSize, 1},
    {&WeatherAround::DrawDetails, &WeatherAround::GetTempSizeSmall, 1},
};

static const WeatherAround::Page sPages[6] = {
    {&WeatherAround::DrawIcons, &WeatherAround::GetIconSize, 2},
    {&WeatherAround::DrawHigh, &WeatherAround::GetTempSizeLarge, 2},
    {&WeatherAround::DrawIcons, &WeatherAround::GetIconSize, 0},
    {&WeatherAround::DrawRain, &WeatherAround::GetTempSizeLarge, 0},
    {&WeatherAround::DrawIcons, &WeatherAround::GetIconSize, 1},
    {&WeatherAround::DrawRain, &WeatherAround::GetTempSizeLarge, 1},
};

static const f32 sTitleScales[4] = {0.6f, 0.6f, 0.6f};

// Volume of the weather sounds at each zoom level
static const f32 sZoomVolumes[10] = {1.0f, 0.89f, 0.78f, 0.67f, 0.56f, 0.45f, 0.34f, 0.23f, 0.12f, 0.06f};

#define SET_COLOR(dst, src)                                                                                  \
    {                                                                                                        \
        (dst).r = (src).r;                                                                                   \
        (dst).g = (src).g;                                                                                   \
        (dst).b = (src).b;                                                                                   \
        (dst).a = (src).a;                                                                                   \
    }

static void ZoomOutColorCallback(nw4r::lyt::Pane* pane, const GXColor* color);
static void ZoomOutCalcCallback(void* arg);

static inline void UpdateLabelScale(WeatherAround* self) {
    f32 zoom = gSimpleGlobe != NULL ? gSimpleGlobe->mZoom : 1.0f;
    f32 scale;
    if (gLanguage != 0) {
        scale = 0.26f + 0.2f / nw4r::math::FSqrt(zoom);
    } else {
        scale = 0.24f + 0.6f / nw4r::math::FSqrt(zoom);
        if (scale > 0.6f) {
            scale = 0.6f;
        }
    }
    self->mLabelScale = scale;
    self->mZoomLevel = gSimpleGlobe != NULL ? gSimpleGlobe->mZoomLevel : 0;
}

static inline void SetVec2(Vec2& v, f32 x, f32 y) {
    v.x = x;
    v.y = y;
}

static inline void DecTimer(s32* timer) {
    if (*timer != 0) {
        (*timer)--;
    }
}

static inline BOOL IsNearZero(f32 value) {
    BOOL result = FALSE;
    if (value < 0.0008f && value > -0.0008f) {
        result = TRUE;
    }
    return result;
}

static inline BOOL InRect(s32 index, const nw4r::ut::Rect& rect, f32 x, f32 y) {
    if (index >= 0 && x >= rect.left && x <= rect.right && y >= rect.top && y <= rect.bottom) {
        return TRUE;
    }
    return FALSE;
}

static inline void AppendLabel(CityLabel*& head, CityLabel* label, CityLabel* CityLabel::*next) {
    if (head != NULL) {
        CityLabel* last = head;
        while (last->*next != NULL) {
            last = last->*next;
        }
        last->*next = label;
    } else {
        head = label;
    }
}

static inline void SetBoxPos(TextBox* box, f32 centerX, f32 scaleX, f32 centerY) {
    Vec2F center = box->mPane->GetCenter();
    box->mX = center.x;
    box->mY = center.y;
    box->mX = centerX + box->mX * scaleX;
    box->mY = centerY - box->mY;
}

WeatherAround::WeatherAround(void* arc)
    : mBackList(NULL), mFrontList(NULL), mOverlapList(NULL), mLayout(NULL), mBeltLayout(NULL), mResetButton(NULL),
      mBelt(NULL), mKion(NULL), mRain(NULL), mHigh(NULL), mNextButton(NULL), mZoomInButton(NULL),
      mZoomOutButton(NULL), mRotAButton(NULL), mRotBButton(NULL), mZoomOutI0(NULL), mZoomOutI1(NULL), mState(NULL),
      mPageState(NULL), mDrawLabels(NULL), mZoomState(NULL), mTiltState(NULL), unkEC(NULL), mDrawLegend(NULL),
      mLabelSize(NULL), mPressPos(0.0f, 0.0f), mTitlePos(0.0f, 68.0f, 0.0f), mInputActive(FALSE), mNextPressed(FALSE), mActive(FALSE),
      mShowLegend(FALSE), mBlinking(TRUE), mHitIndex(-1), unk258(0x105), mSelected(-1), mPressIndex(-1),
      mPointerIdle(0), mIdleTimer(0), mHoverTimer(0), unk280(0), unk284(0), unk288(0), unk28C(0), mTempUnit(gTempUnit), mZoomOutAlpha(0), mHome(34.8f, 135.4f),
      mFontScale(3.0f), mSlideOffset(0.0f), mSlideMax(0.0f), mBlink(0.0f), mRotateAmount(0.0f), mFrame(0),
      unk2FC(0), unk2FE(0), mDay(0), mPhase(0), mZoomLevel(0), mPagePhase(0), mBlinkOn(FALSE), mAnimFrame(0),
      mBlinkDir(0), mDots(NULL) {

    s32 i;
    for (i = 0; i < 11; i++) {
        mBackBuckets[i] = NULL;
        mFrontBuckets[i] = NULL;
    }

    mLayout = new ButtonGroup(arc, "around.brlyt", gButtonColors, false);
    mNextButton = mLayout->FindButton("next");
    mZoomInButton = mLayout->FindButton("zoom_in");
    mZoomOutButton = mLayout->FindButton("zoom_out");
    mZoomOutButton->mColorCallback = ZoomOutColorCallback;
    mZoomOutI0 = mZoomOutButton->FindPane("zoom_outI0");
    mZoomOutAlpha = GetTevColor1Alpha(mZoomOutI0);
    GetTevColors(mZoomOutI0, &mZoomOutI0Color0, &mZoomOutI0Color1);
    mZoomOutI1 = mZoomOutButton->FindPane("zoom_outI1");
    GetTevColors(mZoomOutI1, &mZoomOutI1Color0, &mZoomOutI1Color1);
    mZoomOutButton->SetCalcCallback(ZoomOutCalcCallback, this);
    mRotAButton = mLayout->FindButton("rot_a");
    mRotBButton = mLayout->FindButton("rot_b");
    mResetButton = mLayout->FindButton("reset");

    mBeltLayout = new ButtonGroup(arc, "around_belt.brlyt", gButtonColors, false);
    mBelt = mBeltLayout->FindButton("belt");
    mBelt->ShowLanguagePane("beltB");
    mKion = mBeltLayout->FindButton("kion");
    mRain = mBeltLayout->FindButton("rain");
    mHigh = mBeltLayout->FindButton("high");

    for (i = 0; i < 4; i++) {
        mHovering[i] = FALSE;
        mTouchPos[i].x = sZero;
        mTouchPos[i].y = sZero;
        mPressTimers[i] = 0;
    }

    UpdateLabelScale(this);

    mLabels = new CityLabel*[gForecastData->mHeader->mNumPlaces];
    CityLabel** label = mLabels;
    for (i = 0; i < (s32)gForecastData->mHeader->mNumPlaces; i++, label++) {
        *label = new CityLabel(gCities[i], mFontScale, gFutiFont);
    }

    f32 centerY = 228.0f;
    if (gLanguage == 0) {
        f32 centerX = 0.5f * GetScreenWidth();
        f32 scaleX = gWidescreen ? 1.3684211f : 1.0f;
        TextBox* box = mTitles;
        for (i = 0; i < 3; i++, box++) {
            box->mPane = mBeltLayout->FindButton(sTitleNames[i]);
            if (box->mPane != NULL) {
                SetBoxPos(box, centerX, scaleX, centerY);
                LayoutButton* pane = box->mPane;
                f32 h = pane->mRect.top - pane->mRect.bottom;
                f32 w = pane->mRect.right - pane->mRect.left;
                box->mWidth = w;
                box->mHeight = __fabsf(h);
                box->mScaleX = sTitleScales[i];
                box->mScaleY = sTitleScales[i];
                SET_COLOR(box->mColor, gColorWhite);
                SET_COLOR(box->mShadowColor, gColorDarkGray);
            } else {
                OSReport("%s[%d]: ", "d_weather_around.cpp", 296);
                OSReport("WARNING!! ...%s\x82\xAA\x8C\xA9\x82\xC2\x82\xA9\x82\xE8\x82\xDC\x82\xB9\x82\xF1!!\n", sTitleNames[i]);
                OSPanic("d_weather_around.cpp", 298, "");
            }
        }
    }

    Vec2F center = mBelt->GetCenter();
    f32 beltH = __fabsf(mBelt->mRect.top - mBelt->mRect.bottom);
    center.y = (centerY - center.y) - 0.5f * beltH;
    mSlideMax = center.y - (gWidescreen ? 19 : 34);

    mDots = new GlobeDots();

    {
        StateFunc state = &WeatherAround::StateGlobe;
        if (mState) {
            mPhase = -1;
            (this->*mState)();
        }
        mState = state;
        mPhase = 0;
        (this->*mState)();
    }
    {
        PageStateFunc state = &WeatherAround::StatePage;
        if (mPageState) {
            mPagePhase = -1;
            (this->*mPageState)(0);
        }
        mPageState = state;
        mPagePhase = 0;
        if (mPageState) {
            (this->*mPageState)(0);
        }
    }

    mCalcFunc = &WeatherAround::DrawGlobe;
}

WeatherAround::~WeatherAround() {
    delete mDots;
    if (mLabels != NULL) {
        CityLabel** label = mLabels;
        for (s32 i = 0; i < (s32)gForecastData->mHeader->mNumPlaces; i++) {
            delete mLabels[i];
        }
        delete[] mLabels;
    }
    delete mBeltLayout;
    delete mLayout;
}

inline void WeatherAround::ResetLabels() {
    CityLabel** label = mLabels;
    for (s32 i = 0; i < (s32)gForecastData->mHeader->mNumPlaces; i++, label++) {
        (*label)->UpdateTempText();
        (*label)->ResetDrawFunc();
        (*label)->CalcSize();
    }
}

void WeatherAround::Reset() {
    SimpleGlobe* globe = gSimpleGlobe;
    if (globe != NULL) {
        CityInfo* info = gCurrentCity != NULL ? gCurrentCity->mInfo : NULL;
        PlaceEntry* place = (PlaceEntry*)info->mId;
        Vec2 deg;
        ToDegrees(place->mLongitude, place->mLatitude, &deg);
        f32 ry = deg.y;
        f32 rx = deg.x;
        Vec rot;
        rot.x = rx;
        rot.y = ry;
        rot.z = 0.0f;
        globe->SetZoomLevel(0);
        globe->Setup(&rot);
    }
    ResetLabels();

    mLayout->Reset();
    mBeltLayout->Reset();
    mDots->ResetAlpha();

    if (mTempUnit != gTempUnit) {
        mTempUnit = gTempUnit;
        CityLabel** label = mLabels;
        for (s32 i = 0; i < (s32)gForecastData->mHeader->mNumPlaces; i++, label++) {
            (*label)->UpdateTempText();
        }
    }

    if (mCalcFunc) {
        (this->*mCalcFunc)();
    }
    UpdateBlink();
}

void WeatherAround::Show() {
    SimpleGlobe* globe = gSimpleGlobe;
    if (globe != NULL) {
        CityInfo* info = gCurrentCity != NULL ? gCurrentCity->mInfo : NULL;
        PlaceEntry* place = (PlaceEntry*)info->mId;
        Vec2 deg;
        ToDegrees(place->mLongitude, place->mLatitude, &deg);
        nw4r::math::VEC3 rot(deg.x, deg.y, 0.0f);
        globe->SetZoomLevel(0);
        globe->Setup(rot);
    }
}

void WeatherAround::UpdateButtonFade() {
    SimpleGlobe* globe = gSimpleGlobe;
    f32 minDist;
    f32 x;
    f32 y;
    f32 left = -16.0f;
    f32 right = 16.0f + GetScreenWidth();
    f32 top = 63.0f;
    f32 bottom = 393.0f;
    minDist = 900.0f;
    s32 i;
    BOOL moved = FALSE;
    BOOL onScreen = FALSE;
    BOOL hovered = FALSE;

    for (i = 0; i < 4; i++) {
        if (globe->mGrabbed[i]) {
            continue;
        }
        if (!IsPointerValid(i)) {
            continue;
        }
        x = gPointerX[i][0];
        y = gPointerY[i][0];
        f32 oldX, oldY;
        if (gPointerHistory.GetOldest(i, &oldX, &oldY)) {
            if (x >= left && x < right && oldX >= left && oldX < right &&
                (x - oldX) * (x - oldX) + (y - oldY) * (y - oldY) > minDist) {
                moved = TRUE;
            } else if (y <= top || y > bottom) {
                moved = TRUE;
            }
        }
        if (x >= left && x < right) {
            onScreen = TRUE;
        }
        if (sHoveredButtons[i] != NULL) {
            hovered = TRUE;
        }
    }

    if (onScreen) {
        mPointerIdle = 0;
    } else if (mPointerIdle < 10) {
        mPointerIdle++;
    }

    if (moved || hovered) {
        mIdleTimer = 0;
    } else if (mIdleTimer < 90) {
        mIdleTimer++;
    }

    if (mPointerIdle < 10 && mIdleTimer < 90) {
        mLayout->SlideIn(15);
    } else {
        mLayout->SlideOut(30);
    }

    if (hovered) {
        if (mHoverTimer > 0) {
            mHoverTimer--;
        }
    } else if (mHoverTimer < 15) {
        mHoverTimer++;
    }

    s32 minAlpha = 32;
    s32 alpha = minAlpha;
    alpha += (s32)(255.0f - minAlpha) * nw4r::math::SinRad((1.5708f * (15 - mHoverTimer)) / 15.0f);
    mLayout->SetButtonParams(alpha, mHoverTimer, 15);
    UpdateZoomButtons();
}

void WeatherAround::UpdateZoomButtons() {
    SimpleGlobe* globe = gSimpleGlobe;

    if (globe->mZoomLevel >= 9) {
        mZoomOutButton->Lock();
    } else {
        mZoomOutButton->Unlock();
    }

    if (globe->mZoomLevel <= 0) {
        mZoomInButton->Lock();
    } else {
        mZoomInButton->Unlock();
    }

    if (globe->mTiltLevel <= 0) {
        mRotBButton->Lock();
    } else {
        mRotBButton->Unlock();
    }

    if (globe->mTiltLevel >= 5) {
        mRotAButton->Lock();
    } else {
        mRotAButton->Unlock();
    }
}

void WeatherAround::Calc() {
    if (mTempUnit != gTempUnit) {
        mTempUnit = gTempUnit;
        CityLabel** label = mLabels;
        for (s32 i = 0; i < (s32)gForecastData->mHeader->mNumPlaces; i++, label++) {
            (*label)->UpdateTempText();
        }
    }

    if (mCalcFunc) {
        (this->*mCalcFunc)();
    }
    UpdateBlink();
}

static f32 sBackSpeedX = 0.0f;
static f32 sBackSpeedY = 0.0f;

void WeatherAround::CalcActive() {
    mInputActive = TRUE;
    mCanSelect = TRUE;
    mNextPressed = FALSE;
    gSimpleGlobe->ClearInput();
    mHovering[0] = FALSE;
    mHovering[1] = FALSE;
    mHovering[2] = FALSE;
    mHovering[3] = FALSE;

    mBeltLayout->Calc();
    mLayout->FadeIn(15);
    mLayout->Calc();
    mSlideOffset = mSlideMax * ((f32)mLayout->mSlideFrame / (f32)mLayout->mSlideFrames);
    mBeltLayout->SetSlideOffset(mSlideOffset);
    gTitleOffsetY = mSlideOffset;

    if (gSimpleGlobe->IsDefaultView()) {
        mResetButton->Unlock();
    } else {
        mResetButton->Lock();
    }

    UpdateButtons(mLayout, 40);
    UpdateButtonFade();

    if (CheckButtonPressed("back", WPAD_BUTTON_A) >= 0) {
        PlaySE(38);
        SimpleGlobe* globe = gSimpleGlobe;
        globe->mSpeed.x = sBackSpeedX;
        globe->mSpeed.y = sBackSpeedY;
        gSimpleGlobe->mSpinning = FALSE;
        gSettingResult = 6;
        gCurrentCity = FindCity(gCurrentCityId);
        LayoutButton* back = mLayout->FindButton("back");
        back->mPressed = TRUE;
        return;
    }

    if (CheckButtonHeld("zoom_out", WPAD_BUTTON_A) >= 0) {
        mBlinking = FALSE;
        gSimpleGlobe->mZoomIn = TRUE;
    } else if (gTrigAll & WPAD_BUTTON_MINUS) {
        mBlinking = FALSE;
        gSimpleGlobe->mZoomIn = TRUE;
        mZoomOutButton->Press(TRUE);
    }

    if (CheckButtonHeld("zoom_in", WPAD_BUTTON_A) >= 0) {
        gSimpleGlobe->mZoomOut = TRUE;
    } else if (gTrigAll & WPAD_BUTTON_PLUS) {
        gSimpleGlobe->mZoomOut = TRUE;
        mZoomInButton->Press(TRUE);
    }

    if (CheckButtonHeld("rot_a", WPAD_BUTTON_A) >= 0) {
        gSimpleGlobe->mTiltUp = TRUE;
    } else if (gTrigAll & WPAD_BUTTON_UP) {
        gSimpleGlobe->mTiltUp = TRUE;
        mRotAButton->Press(TRUE);
    }

    if (CheckButtonHeld("rot_b", WPAD_BUTTON_A) >= 0) {
        gSimpleGlobe->mTiltDown = TRUE;
    } else if (gTrigAll & WPAD_BUTTON_DOWN) {
        gSimpleGlobe->mTiltDown = TRUE;
        mRotBButton->Press(TRUE);
    }

    if (CheckButtonPressed("next", WPAD_BUTTON_A) >= 0) {
        mNextPressed = TRUE;
    } else if (gTrigAll & WPAD_BUTTON_RIGHT) {
        mNextPressed = TRUE;
        mNextButton->Press(TRUE);
    }

    BOOL reset;
    GlobeView* view = gSimpleGlobe->mView;
    if (view == NULL) {
        reset = FALSE;
    } else if (view->getResetting() == 0 && CheckButtonPressed("reset", WPAD_BUTTON_A) >= 0) {
        PlaySE(19);
        gSimpleGlobe->SetZoom(0, 1);
        reset = TRUE;
    } else {
        reset = FALSE;
    }

    if (mPageState) {
        (this->*mPageState)(0);
    }
    if (!reset && mState) {
        (this->*mState)();
    }

    gSimpleGlobe->UpdateRotation(mHitIndex >= 0);
    CalcGlobe();

    f32 y;
    f32 x;
    f32 top = 63.0f;
    f32 bottom = 393.0f;
    for (s32 i = 0; i < 4; i++) {
        y = gCursorY[i];
        x = gCursorX[i];
        if (InRect(mHitIndex, mHitRect, x, y)) {
            mHovering[i] = TRUE;
        }
        f32 pointerY = gPointerY[i][0];
        if (gSimpleGlobe->mGrabbed[i]) {
            gCursorState[i] = 2;
        } else if (mHovering[i] == TRUE) {
            gCursorState[i] = 3;
        } else if (pointerY <= top || pointerY > bottom) {
            gCursorState[i] = 0;
        } else {
            gCursorState[i] = 1;
        }
    }

    mDots->UpdateAlpha(gSimpleGlobe->mSpeed.x, gSimpleGlobe->mSpeed.y);
}

void WeatherAround::CalcGlobe() {
    SimpleGlobe* globe = gSimpleGlobe;
    if (globe != NULL) {
        globe->UpdateView();
        globe->UpdateFacing();
        UpdateLabels();
        globe->UpdateLight();
    }

    mFrame++;
    if ((mFrame & 0x1F) == 0) {
        mBlinkOn ^= 1;
    }
    if ((mFrame & 0xF) == 0) {
        mAnimFrame++;
        if (mAnimFrame > 2) {
            mAnimFrame = 0;
        }
    }

    if (globe != NULL) {
        globe->Calc();
    }
}

void WeatherAround::DrawGlobe() {
    mInputActive = FALSE;
    mCanSelect = FALSE;
    mNextPressed = FALSE;
    for (s32 i = 0; i < 4; i++) {
        mHovering[i] = FALSE;
        mPressTimers[i] = 0;
    }

    mLayout->ReleaseAll();
    mLayout->Calc();
    mBeltLayout->Calc();
    mSlideOffset = mSlideMax * ((f32)mLayout->mSlideFrame / (f32)mLayout->mSlideFrames);
    mBeltLayout->SetSlideOffset(mSlideOffset);
    gTitleOffsetY = mSlideOffset;

    if (gSimpleGlobe->IsDefaultView()) {
        mResetButton->Unlock();
    } else {
        mResetButton->Lock();
    }

    UpdateZoomButtons();

    if (gSimpleGlobe != NULL) {
        gSimpleGlobe->ClearInput();
        if (gSimpleGlobe != NULL) {
            gSimpleGlobe->SyncZoom();
        }
        UpdateLabelScale(this);
    }

    CalcGlobe();
}

void WeatherAround::Draw() {
    SimpleGlobe* globe = gSimpleGlobe;
    mDots->Draw();
    if (globe != NULL) {
        globe->DrawModel();
        globe->Draw();
    }

    if (mDrawLabels) {
        (this->*mDrawLabels)();
    }

    if (mActive) {
        mLayout->Draw();
        mBeltLayout->Draw();
        if (mShowLegend && mDrawLegend) {
            (this->*mDrawLegend)();
        }
    }
}

void WeatherAround::DrawTitles() {
    SetDefaultGXState();
    SetOrthoProjection();
    gTextWriter.SetFont(*gSysFont);
    gTextWriter.SetDrawFlag(0x111);
    gTextWriter.SetupGX();

    TextBox* box = mTitles;
    wchar_t* text = mLegendText[0];
    for (s32 i = 0; i < 3; i++, box++, text += 10) {
        Vec2 pos;
        pos.y = box->mY + mSlideOffset;
        pos.x = box->mX;
        gTextWriter.SetScale(box->mScaleX, box->mScaleY);
        f32 space = gUnkSceneFloat;
        gTextWriter.SetCharSpace(box->mScaleX * space);
        gTextWriter.SetTextColor(box->mColor);
        gTextWriter.SetCursor(pos.x, pos.y);
        gTextWriter.Print(text);
    }
}

void WeatherAround::DrawLegend() {
    Vec2 pos;
    f32 space;
    f32 half;

    SetDefaultGXState();
    SetOrthoProjection();
    gTextWriter.SetFont(*gSysFont);
    gTextWriter.SetDrawFlag(0x122);
    gTextWriter.SetupGX();

    half = 0.5f * mTitles[0].mWidth;
    pos.y = mTitles[0].mY + mSlideOffset;
    pos.x = mTitles[0].mX + half;
    gTextWriter.SetScale(mTitles[0].mScaleX, mTitles[0].mScaleY);
    space = gUnkSceneFloat;
    gTextWriter.SetCharSpace(mTitles[0].mScaleX * space);
    gTextWriter.SetTextColor(mTitles[0].mColor);
    gTextWriter.SetCursor(pos.x, pos.y);
    gTextWriter.Print(mLegendText[0]);

    half = 0.5f * mTitles[1].mWidth;
    pos.y = mTitles[1].mY + mSlideOffset;
    pos.x = mTitles[1].mX + half;
    gTextWriter.SetScale(mTitles[1].mScaleX, mTitles[1].mScaleY);
    space = gUnkSceneFloat;
    gTextWriter.SetCharSpace(mTitles[1].mScaleX * space);
    gTextWriter.SetTextColor(mTitles[1].mColor);
    gTextWriter.SetCursor(pos.x, pos.y);
    gTextWriter.Print(mLegendText[1]);

    gTextWriter.SetDrawFlag(0x111);
    gTextWriter.SetupGX();
    pos.y = mTitles[2].mY + mSlideOffset;
    pos.x = mTitles[2].mX;
    gTextWriter.SetScale(mTitles[2].mScaleX, mTitles[2].mScaleY);
    space = gUnkSceneFloat;
    gTextWriter.SetCharSpace(mTitles[2].mScaleX * space);
    gTextWriter.SetTextColor(mTitles[2].mColor);
    gTextWriter.SetCursor(pos.x, pos.y);
    gTextWriter.Print(mLegendText[2]);
}

#define FOR_EACH_BUCKET(buckets, next, body)                                                                 \
    for (i = 0; i < 11; i++) {                                                                               \
        for (label = buckets[i]; label != NULL; label = label->next) {                                       \
            body;                                                                                            \
        }                                                                                                    \
    }

#define FOR_EACH_LIST(list, next, body)                                                                      \
    for (label = list; label != NULL; label = label->next) {                                                 \
        body;                                                                                                \
    }

void WeatherAround::DrawIcons() {
    CityLabel* label;
    s32 i;
    f32 scale = mLabelScale;
    FOR_EACH_BUCKET(mFrontBuckets, mNextFrontBack, label->DrawBg());
    FOR_EACH_BUCKET(mBackBuckets, mNextBack, {
        u32 code = label->GetWeatherCode(mDay);
        DrawWeatherIcon(gForecastData->GetWeatherIcon(code), &label->mPos, scale, label->mAlpha);
    });
    FOR_EACH_LIST(mFrontList, mNextFront, label->DrawBg());
    FOR_EACH_BUCKET(mFrontBuckets, mNextFrontBack, label->DrawName());
    FOR_EACH_LIST(mBackList, mNext, {
        f32 bounce = scale + 0.2f * EaseCos(label->mBounce);
        u32 code = label->GetWeatherCode(mDay);
        DrawWeatherIcon(gForecastData->GetWeatherIcon(code), &label->mPos, bounce, label->mAlpha);
    });
    FOR_EACH_LIST(mFrontList, mNextFront, label->DrawName());
}

void WeatherAround::DrawTempsToday() {
    CityLabel* label;
    s32 i;
    f32 scale = mLabelScale;
    FOR_EACH_BUCKET(mFrontBuckets, mNextFrontBack, label->DrawBg());
    FOR_EACH_BUCKET(mBackBuckets, mNextBack, label->DrawTemp(0, scale));
    FOR_EACH_BUCKET(mFrontBuckets, mNextFrontBack, label->DrawName());
    FOR_EACH_LIST(mFrontList, mNextFront, label->DrawBg());
    FOR_EACH_LIST(mBackList, mNext, label->DrawTemp(0, scale + 0.2f * EaseCos(label->mBounce)));
    FOR_EACH_LIST(mFrontList, mNextFront, label->DrawName());
}

void WeatherAround::DrawTempsTomorrow() {
    CityLabel* label;
    s32 i;
    f32 scale = mLabelScale;
    FOR_EACH_BUCKET(mFrontBuckets, mNextFrontBack, label->DrawBg());
    FOR_EACH_BUCKET(mBackBuckets, mNextBack, label->DrawTempAlt(1, scale));
    FOR_EACH_BUCKET(mFrontBuckets, mNextFrontBack, label->DrawName());
    FOR_EACH_LIST(mFrontList, mNextFront, label->DrawBg());
    FOR_EACH_LIST(mBackList, mNext, label->DrawTempAlt(1, scale + 0.2f * EaseCos(label->mBounce)));
    FOR_EACH_LIST(mFrontList, mNextFront, label->DrawName());
}

void WeatherAround::DrawRain() {
    CityLabel* label;
    s32 i;
    s32 day = mDay;
    f32 scale = mLabelScale;
    FOR_EACH_BUCKET(mFrontBuckets, mNextFrontBack, label->DrawBg());
    FOR_EACH_BUCKET(mBackBuckets, mNextBack, label->DrawRain(day, scale));
    FOR_EACH_BUCKET(mFrontBuckets, mNextFrontBack, label->DrawName());
    FOR_EACH_LIST(mFrontList, mNextFront, label->DrawBg());
    FOR_EACH_LIST(mBackList, mNext, label->DrawRain(day, scale + 0.2f * EaseCos(label->mBounce)));
    FOR_EACH_LIST(mFrontList, mNextFront, label->DrawName());
}

void WeatherAround::DrawHigh() {
    CityLabel* label;
    s32 i;
    f32 scale = mLabelScale;
    FOR_EACH_BUCKET(mFrontBuckets, mNextFrontBack, label->DrawBg());
    FOR_EACH_BUCKET(mBackBuckets, mNextBack, label->DrawHigh(scale));
    FOR_EACH_BUCKET(mFrontBuckets, mNextFrontBack, label->DrawName());
    FOR_EACH_LIST(mFrontList, mNextFront, label->DrawBg());
    FOR_EACH_LIST(mBackList, mNext, label->DrawHigh(scale + 0.2f * EaseCos(label->mBounce)));
    FOR_EACH_LIST(mFrontList, mNextFront, label->DrawName());
}

void WeatherAround::DrawDetails() {
    CityLabel* label;
    s32 i;
    f32 scale = mLabelScale;
    BOOL tomorrow = mDay != 0;
    FOR_EACH_BUCKET(mFrontBuckets, mNextFrontBack, label->DrawBg());
    FOR_EACH_BUCKET(mBackBuckets, mNextBack, label->DrawDetail(tomorrow, scale));
    FOR_EACH_BUCKET(mFrontBuckets, mNextFrontBack, label->DrawName());
    FOR_EACH_LIST(mFrontList, mNextFront, label->DrawBg());
    FOR_EACH_LIST(mBackList, mNext, label->DrawDetail(tomorrow, scale + 0.2f * EaseCos(label->mBounce)));
    FOR_EACH_LIST(mFrontList, mNextFront, label->DrawName());
}

#define APPEND_LABEL(head, label, next)                                                                      \
    {                                                                                                        \
        CityLabel* newLabel = label;                                                                         \
        CityLabel* last = head;                                                                              \
        if (last != NULL) {                                                                                  \
            CityLabel* nxt;                                                                                  \
            while ((nxt = last->next) != NULL) {                                                             \
                last = nxt;                                                                                  \
            }                                                                                                \
            last->next = newLabel;                                                                           \
        } else {                                                                                             \
            head = newLabel;                                                                                 \
        }                                                                                                    \
    }

#define RESET_PRESS()                                                                                        \
    {                                                                                                        \
        mPressIndex = mHitIndex;                                                                             \
        mPressRect = mHitRect;                                                                               \
        mSelected = -1;                                                                                      \
    }

template <typename A, typename B>
static inline f32 DistSq(const A& a, const B& b) {
    Vec2 d;
    d.x = a.x - b.x;
    d.y = a.y - b.y;
    return d.x * d.x + d.y * d.y;
}

template <typename A, typename B>
static inline f32 DistSqW(const A& a, const B& b) {
    return DistSq(a, b);
}

static f32 sSelectSpeedX = 0.0f;
static f32 sSelectSpeedY = 0.0f;

// The pointer that just let go of A near where it was pressed
inline s32 WeatherAround::FindReleasedTouch() {
    s32 i = 0;
    Vector2* touch = mTouchPos;
    s32* timer = mPressTimers;
    for (; i < 4; i++, touch++, timer++) {
        if (sHoveredButtons[i] == NULL && (gRelease[i] & WPAD_BUTTON_A) && *timer != 0) {
            f32 x, y;
            y = touch->y;
            x = touch->x;
            if (!InRect(mPressIndex, mPressRect, x, y) && DistSq(mPressPos, *touch) < 1600.0f) {
                return i;
            }
        }
    }
    return -1;
}

// The pointer that just pressed A
inline s32 WeatherAround::FindPressedTouch() {
    s32 i = 0;
    Vector2* touch = mTouchPos;
    for (; i < 4; i++, touch++) {
        if (sHoveredButtons[i] == NULL && (gTrig[i] & WPAD_BUTTON_A)) {
            f32 x, y;
            y = touch->y;
            x = touch->x;
            if (!InRect(mPressIndex, mPressRect, x, y) && DistSq(mPressPos, *touch) < 1600.0f) {
                return i;
            }
        }
    }
    return -1;
}

static inline void GetHitBox(Vec2& pos, const Vector2& box) {
    pos.x = box.x;
    pos.y = box.y;
}
static inline void GetHitSize(Vec2& half, const Vector2& boxSize) {
    half.x = boxSize.x / 2.0f;
    half.y = boxSize.y / 2.0f;
}

static inline void AddFrontBucket(CityLabel** buckets, CityLabel* l) {
    if (*l->mCity->mInfo->mId == gCurrentCityId) {
        buckets[10] = l;
    } else {
        APPEND_LABEL(buckets[l->mPriority], l, mNextFrontBack);
    }
}
static inline void AddBackBucket(CityLabel** buckets, CityLabel* l) {
    if (*l->mCity->mInfo->mId == gCurrentCityId) {
        buckets[10] = l;
    } else {
        APPEND_LABEL(buckets[l->mPriority], l, mNextBack);
    }
}

void WeatherAround::UpdateLabels() {
    f32 top = 63.0f;
    f32 bottom = 393.0f;

    ClearLists();

    if (unk250) {
        RESET_PRESS();
        return;
    }

    s32 hoverIndex[4];
    f32 hoverDist[4];
    Vec2 pos;
    Vec2 cursor;
    Vec2 half;
    s32 i;
    s32 selected = gForecastData->mHeader->mNumPlaces;
    for (s32 j = 0; j < 4; j++) {
        hoverIndex[j] = selected;
        hoverDist[j] = 2500.0f;
    }
    f32 releaseDist;
    f32 pressDist;
    releaseDist = pressDist = 2500.0f;

    CityLabel** label = mLabels;
    for (i = 0; i < (s32)gForecastData->mHeader->mNumPlaces; i++, label++) {
        (*label)->mScale = mLabelScale;
        s32 zoom = mZoomLevel;
        (*label)->Project(gSimpleGlobe->mView, zoom);
        (*label)->UpdateEdgeFade();
        if (!((*label)->mFlags & CITY_LABEL_ON_SCREEN)) {
            continue;
        }

        pos = (*label)->mPos;

        s32 chan = FindReleasedTouch();
        if (chan >= 0) {
            f32 dist = DistSqW(mTouchPos[chan], pos);
            if (dist < releaseDist) {
                releaseDist = dist;
                selected = i;
            }
        }

        chan = FindPressedTouch();
        if (chan >= 0) {
            f32 dist = DistSqW(mTouchPos[chan], pos);
            if (dist < pressDist) {
                pressDist = dist;
            }
        }

        if (mInputActive) {
            for (s32 j = 0; j < 4; j++) {
                if (sHoveredButtons[j] == NULL && gKPADLatest[j] >= 0) {
                    f32 x = gCursorX[j];
                    f32 y = gCursorY[j];
                    if (y > top && y < bottom && !InRect(mHitIndex, mHitRect, x, y)) {
                        cursor.x = x;
                        cursor.y = y;
                        f32 dist = DistSqW(cursor, pos);
                        if (dist < hoverDist[j]) {
                            hoverDist[j] = dist;
                            hoverIndex[j] = i;
                        }
                    }
                }
            }
        }
    }

    if (mInputActive) {
        for (s32 j = 0; j < 4; j++) {
            if (hoverIndex[j] < (s32)gForecastData->mHeader->mNumPlaces) {
                mLabels[hoverIndex[j]]->mFlags |= CITY_LABEL_HOVERED;
                mHovering[j] = TRUE;
            }
        }
    }

    if (selected < (s32)gForecastData->mHeader->mNumPlaces) {
        if (mPressIndex != selected) {
            mSelected = selected;
        }
    } else if (gTrigAll & WPAD_BUTTON_A) {
        RESET_PRESS();
    }

    label = mLabels;
    for (i = 0; i < (s32)gForecastData->mHeader->mNumPlaces; i++, label++) {
        if (mSelected == i) {
            mCanSelect = FALSE;
            if (gLastSettingResult == 3) {
                PlaySE(37);
                SimpleGlobe* globe = gSimpleGlobe;
                globe->mSpeed.x = sSelectSpeedX;
                globe->mSpeed.y = sSelectSpeedY;
                gSimpleGlobe->mSpinning = FALSE;
                gSettingResult = 2;
                (Vec2&)gCityPos = (*label)->mPos;
                gCurrentCity = (*label)->mCity;
            }
        } else if ((*label)->mInputFlags & CITY_LABEL_SELECTED) {
            (*label)->mInputFlags &= ~CITY_LABEL_SELECTED;
        }

        if ((*label)->IsDrawn()) {
            APPEND_LABEL(mOverlapList, *label, mNextOverlap);
        }
    }

    CityLabel* b;
    CityLabel* a;
    for (a = mOverlapList; a != NULL; a = a->mNextOverlap) {
        for (b = a->mNextOverlap; b != NULL; b = b->mNextOverlap) {
            if (CheckOverlap(a, b)) {
                a->mFlags |= CITY_LABEL_HIDDEN;
            }
        }
    }

    f32 scale = mLabelScale;
    f32 unusedLabelW = 75.0f; // HACK
    f32 nameScale = 2.5f * scale;
    if (nameScale > 1.1f) {
        nameScale = 1.1f;
    }

    label = mLabels;
    mHitIndex = -1;
    for (i = 0; i < (s32)gForecastData->mHeader->mNumPlaces; i++, label++) {
        (*label)->UpdateHover();
        if (!((*label)->mFlags & CITY_LABEL_ON_SCREEN)) {
            continue;
        }

        (*label)->UpdateAlpha();
        if ((*label)->mFlags & CITY_LABEL_HOVERED) {
            PlayWeatherSound(*label);
        }

        pos = (*label)->mPos;
        f32 bounce = 0.2f * EaseCos((*label)->mBounce);
        Vector2 size;
        size = GetLabelSize(*label);
        if (bounce > 0.0f) {
            bounce += scale;
            pos.x += 0.5f * size.x * bounce;
            (*label)->SetPosition(&pos, nameScale + 0.1f * EaseCos((*label)->mBounce));
            if ((*label)->IsDrawn()) {
                APPEND_LABEL(mFrontList, *label, mNextFront);
            }
            APPEND_LABEL(mBackList, *label, mNext);
        } else {
            pos.x += 0.5f * size.x * scale;
            (*label)->SetPosition(&pos, nameScale);
            if ((*label)->IsDrawn()) {
                AddFrontBucket(mFrontBuckets, *label);
            }
            AddBackBucket(mBackBuckets, *label);
        }

        if ((*label)->mInputFlags & CITY_LABEL_HIT) {
            GetHitBox(pos, (*label)->GetBoxPos());
            GetHitSize(half, (*label)->GetSize());
            mHitIndex = i;
            mHitRect.left = pos.x - half.x;
            mHitRect.right = pos.x + half.x;
            mHitRect.top = pos.y - half.y;
            mHitRect.bottom = pos.y + half.y;
        }
    }
}

#define CLEAR_LIST(head, next)                                                                               \
    {                                                                                                        \
        CityLabel* label = head;                                                                             \
        while (label != NULL) {                                                                              \
            CityLabel* cur = label;                                                                          \
            label = label->next;                                                                             \
            cur->next = NULL;                                                                                \
        }                                                                                                    \
        head = NULL;                                                                                         \
    }

void WeatherAround::ClearLists() {
    CLEAR_LIST(mBackList, mNext);
    for (s32 i = 0; i < 11; i++) {
        CLEAR_LIST(mBackBuckets[i], mNextBack);
    }
    CLEAR_LIST(mFrontList, mNextFront);
    for (s32 i = 0; i < 11; i++) {
        CLEAR_LIST(mFrontBuckets[i], mNextFrontBack);
    }
    CLEAR_LIST(mOverlapList, mNextOverlap);
}

BOOL WeatherAround::StateGlobe() {
    switch (mPhase) {
    case 0:
        mPhase++;
        {
            Func state = &WeatherAround::StateZoom;
            if (mZoomPhase) {
                mZoomPhase = -1;
                (this->*mZoomState)();
            }
            mZoomState = state;
            mZoomPhase = 0;
            if (mZoomState) {
                (this->*mZoomState)();
                if (gSimpleGlobe != NULL) {
                    gSimpleGlobe->SyncZoom();
                }
                UpdateLabelScale(this);
            }
        }
        {
            Func state = &WeatherAround::StateTilt;
            if (mTiltPhase) {
                mTiltPhase = -1;
                (this->*mTiltState)();
            }
            mTiltState = state;
            mTiltPhase = 0;
            (this->*mTiltState)();
        }
        break;
    case -1:
        break;
    default: {
        SimpleGlobe* globe = gSimpleGlobe;
        globe->UpdateZoom(gWeatherIconIds);
        if (gSimpleGlobe != NULL) {
            gSimpleGlobe->SyncZoom();
        }
        UpdateLabelScale(this);
        UpdateTouch();
        UpdateDrag();
        globe->UpdateTilt(1, gWeatherIconIds2);
        break;
    }
    }
    return TRUE;
}

void WeatherAround::UpdateDrag() {
    f32 top = 63.0f;
    f32 bottom = 393.0f;
    f32 startRot = gSimpleGlobe->mView != NULL ? gSimpleGlobe->mView->getOrientation()->z : 0.0f;
    BOOL dragging = FALSE;

    for (s32 i = 0; i < 4; i++) {
        switch (gSimpleGlobe->UpdateDrag(i)) {
        case 0:
            if (sHoveredButtons[i] == NULL) {
                f32 x = gCursorX[i];
                f32 y = gCursorY[i];
                if (y > top && y < bottom) {
                    if (!(InRect(mHitIndex, mHitRect, x, y)) && gSimpleGlobe->UpdateGrab(i) &&
                        mCanSelect == TRUE) {
                        PlaySE(21);
                    }
                }
            }
            break;
        case 2:
            gSimpleGlobe->PlayRotateSound(20);
        case 1:
            dragging = TRUE;
            break;
        }
    }

    if (!dragging) {
        mRotateAmount = 0.0f;
    }

    f32 rot = gSimpleGlobe->mView != NULL ? gSimpleGlobe->mView->getOrientation()->z : 0.0f;
    f32 delta = __fabsf(rot - startRot);
    if (!IsNearZero(delta)) {
        if (IsNearZero(mRotateAmount)) {
            f32 t = delta > 2.0f ? 2.0f : delta;
            t *= 0.5f;
            PlayLoopSE(18, 0.5f + 0.5f * t, 1.0f + t, 0.0f, 0.5f);
        }
    }

    mRotateAmount += delta;
    if (mRotateAmount > 10.0f) {
        mRotateAmount = 0.0f;
    }
}

#pragma scheduling once
BOOL WeatherAround::CheckOverlap(CityLabel* a, CityLabel* b) {
    f32 aRight, aBottom, bRight, bBottom, bw, bh, hbh, hah, hbw, haw, half, bTop, aTop, bLeft, aLeft, aw, ah, acx, sumW,
        bcx, sumH, diffX, bcy, acy, diffY, dy, maxX, dx, maxY;
    aLeft = a->mBounds.left;
    aRight = a->mBounds.right;
    bLeft = b->mBounds.left;
    aw = aRight - aLeft;
    aTop = a->mBounds.top;
    aBottom = a->mBounds.bottom;
    bRight = b->mBounds.right;
    ah = aBottom - aTop;
    half = 0.5f;
    bTop = b->mBounds.top;
    bBottom = b->mBounds.bottom;
    bw = bRight - bLeft;
    haw = half * aw;
    bh = bBottom - bTop;
    hah = half * ah;
    hbw = half * bw;
    hbh = half * bh;
    acx = haw + aLeft;
    sumW = aw + bw;
    bcx = hbw + bLeft;
    bcy = hbh + bTop;
    acy = hah + aTop;
    sumH = ah + bh;
    diffX = acx - bcx;
    diffY = acy - bcy;
    maxX = half * sumW;
    dx = __fabsf(diffX);
    dy = __fabsf(diffY);
    maxY = half * sumH;

    if ((a->mFlags & CITY_LABEL_HIDDEN) || dx > maxX || dy > maxY) {
        return FALSE;
    }
    if (a->mFlags & CITY_LABEL_HOVERED) {
        b->mFlags |= CITY_LABEL_HIDDEN;
        return FALSE;
    }
    if (b->mFlags & CITY_LABEL_HOVERED) {
        return TRUE;
    }
    if (*a->mCity->mInfo->mId == gCurrentCityId) {
        b->mFlags |= CITY_LABEL_HIDDEN;
        return FALSE;
    }
    if (*b->mCity->mInfo->mId == gCurrentCityId) {
        return TRUE;
    }
    if (a->mPriority < b->mPriority) {
        return TRUE;
    }
    b->mFlags |= CITY_LABEL_HIDDEN;
    return FALSE;
}
#pragma scheduling reset

void WeatherAround::UpdateTouch() {
    s32 i;
    if (gTrigAll & 0xF7FF) {
        mSelected = -1;
    }

    s32* timer = mPressTimers;
    for (i = 0; i < 4; i++, timer++) {
        if (gHold[i] & WPAD_BUTTON_A) {
            f32 y = gCursorY[i];
            f32 x = gCursorX[i];
            mTouchPos[i].x = x;
            mTouchPos[i].y = y;
            if (*timer != 0) {
                (*timer)--;
            }
        }
        if (gTrig[i] & WPAD_BUTTON_A) {
            mPressTimers[i] = 30;
            mPressPos.x = mTouchPos[i].x;
            mPressPos.y = mTouchPos[i].y;
        }
    }

    for (s32 i = 0; i < 4; i++) {
        s32 hit = mHitIndex;
        f32 y = gCursorY[i];
        f32 x = gCursorX[i];
        if (InRect(hit, mHitRect, x, y) && (gTrig[i] & WPAD_BUTTON_A)) {
            mPressIndex = hit;
            mPressRect = mHitRect;
            mSelected = -1;
        }
    }
}

#define SET_PAGE(page)                                                                                       \
    {                                                                                                        \
        const Page* p = &page;                                                                               \
        mDay = p->mDay;                                                                                      \
        mDrawLabels = p->mDraw;                                                                              \
        mLabelSize = p->mSize;                                                                               \
    }

void WeatherAround::StatePage(s32 arg) {
    switch (mPagePhase) {
    case 0:
        mPagePhase++;
        if (gLanguage == 0) {
            (this->*sPageSetupJP[gForecastPageStep])();
        } else {
            SET_PAGE(sPages[gForecastPageStep]);
            mBelt->SetState(gForecastPageStep);
            mKion->Hide();
            mRain->Hide();
            mHigh->Hide();
        }
        break;
    case -1:
        break;
    default:
        if (gLanguage == 0) {
            (this->*sPageSetupJP[gForecastPageStep])();
        }
        if (mNextPressed) {
            PlaySE(44);
            if (++gForecastPageStep >= 6) {
                gForecastPageStep = 0;
            }
            if (gLanguage != 0) {
                SET_PAGE(sPages[gForecastPageStep]);
                mBelt->SetState(gForecastPageStep);
            }
        } else if (gTrigAll & WPAD_BUTTON_LEFT) {
            PlaySE(44);
            if (--gForecastPageStep < 0) {
                gForecastPageStep = 5;
            }
            if (gLanguage != 0) {
                SET_PAGE(sPages[gForecastPageStep]);
                mBelt->SetState(gForecastPageStep);
            }
        }
        break;
    }
}

static inline void CopyTempUnit(wchar_t* dst) {
    wchar_t* buf = dst;
    if (gTempUnit == 0) {
        wcscpy(buf, L"(\xFF9F" L"C)");
    } else {
        wcscpy(buf, L"(\xFF9F" L"F)");
    }
}

void WeatherAround::SetupIconsJP() {
    SET_PAGE(sPagesJP[gForecastPageStep]);
    mBelt->SetState(gForecastPageStep);
    mKion->Hide();
    mRain->Hide();
    mHigh->Hide();
    mShowLegend = FALSE;
    mDrawLegend = NULL;
}

void WeatherAround::SetupTempDetailJP() {
    SET_PAGE(sPagesJP[gForecastPageStep]);
    mBelt->SetState(gForecastPageStep);
    mKion->Hide();
    mRain->Hide();
    mHigh->mHidden = FALSE;
    mShowLegend = TRUE;
    mDrawLegend = &WeatherAround::DrawTitles;
    wcscpy(mLegendText[0], L"");
    mTitles[1].mColor.r = 255;
    mTitles[1].mColor.g = 160;
    mTitles[1].mColor.b = 0;
    mTitles[1].mColor.a = 255;
    wcscpy(mLegendText[1], L"\x6700\x9AD8");
    SET_COLOR(mTitles[2].mColor, gColorWhite);
    CopyTempUnit(mLegendText[2]);
}

void WeatherAround::SetupTempJP() {
    if (gSimpleGlobe != NULL && gSimpleGlobe->mZoomLevel > gLabelDetailZoom) {
        SetupTempDetailJP();
    } else {
        SET_PAGE(sPagesJP[gForecastPageStep]);
        mBelt->SetState(gForecastPageStep);
        mKion->mHidden = FALSE;
        mRain->Hide();
        mHigh->Hide();
        mShowLegend = TRUE;
        mDrawLegend = &WeatherAround::DrawTitles;
        SET_COLOR(mTitles[0].mColor, gColorRed);
        wcscpy(mLegendText[0], L"\x6700\x9AD8");
        SET_COLOR(mTitles[1].mColor, gColorWhite);
        wcscpy(mLegendText[1], L"\x524D\x65E5\x6BD4");
        SET_COLOR(mTitles[2].mColor, gColorWhite);
        CopyTempUnit(mLegendText[2]);
    }
}

#define FORMAT_HOURS(buf, start, end)                                                                        \
    {                                                                                                        \
        wchar_t* p = FormatNumber(start, buf, 4, FALSE);                                                     \
        *p++ = L'-';                                                                                         \
        *p = L'\0';                                                                                          \
        p = FormatNumber(end, p, 4, FALSE);                                                                  \
        *p++ = L'\x6642';                                                                                    \
        *p = L'\0';                                                                                          \
    }

void WeatherAround::SetupRainJP() {
    SET_PAGE(sPagesJP[gForecastPageStep]);
    mBelt->SetState(gForecastPageStep);
    mKion->Hide();
    mRain->mHidden = FALSE;
    mHigh->Hide();
    mShowLegend = TRUE;
    mDrawLegend = &WeatherAround::DrawLegend;

    City* city = FindCity(gCurrentCityId);
    s32 offset = 0;
    if (city != NULL) {
        if (city->mForecast != NULL) {
            offset = 0;
        } else if (city->mSummary != NULL) {
            offset = 0;
        }
    }
    s32 start = offset + 12;
    if (gCalendarTime.hour < 12) {
        start = offset + 6;
    }

    SET_COLOR(mTitles[0].mColor, gColorWhite);
    FORMAT_HOURS(mLegendText[0], start, start + 6);
    SET_COLOR(mTitles[1].mColor, gColorWhite);
    FORMAT_HOURS(mLegendText[1], start + 6, start + 12);
    SET_COLOR(mTitles[2].mColor, gColorWhite);
    wcscpy(mLegendText[2], L"(%)");
}

void WeatherAround::SetupIcons2JP() {
    SET_PAGE(sPagesJP[gForecastPageStep]);
    mBelt->SetState(gForecastPageStep);
    mKion->Hide();
    mRain->Hide();
    mHigh->Hide();
    mShowLegend = FALSE;
    mDrawLegend = NULL;
}

void WeatherAround::SetupTemp2JP() {
    if (gSimpleGlobe != NULL && gSimpleGlobe->mZoomLevel > gLabelDetailZoom) {
        SetupTempDetailJP();
    } else {
        SET_PAGE(sPagesJP[gForecastPageStep]);
        mBelt->SetState(gForecastPageStep);
        mKion->mHidden = FALSE;
        mRain->Hide();
        mHigh->Hide();
        mShowLegend = TRUE;
        mDrawLegend = &WeatherAround::DrawTitles;
        SET_COLOR(mTitles[0].mColor, gColorRed);
        wcscpy(mLegendText[0], L"\x6700\x9AD8");
        SET_COLOR(mTitles[1].mColor, gColorCyan);
        wcscpy(mLegendText[1], L"\x6700\x4F4E");
        SET_COLOR(mTitles[2].mColor, gColorWhite);
        CopyTempUnit(mLegendText[2]);
    }
}

void WeatherAround::SetupRain2JP() {
    SET_PAGE(sPagesJP[gForecastPageStep]);
    mBelt->SetState(gForecastPageStep);
    mKion->Hide();
    mRain->mHidden = FALSE;
    mHigh->Hide();
    mShowLegend = TRUE;
    mDrawLegend = &WeatherAround::DrawLegend;

    City* city = FindCity(gCurrentCityId);
    s32 offset = 0;
    if (city != NULL) {
        if (city->mForecast != NULL) {
            offset = 0;
        } else if (city->mSummary != NULL) {
            offset = 0;
        }
    }

    SET_COLOR(mTitles[0].mColor, gColorWhite);
    FORMAT_HOURS(mLegendText[0], offset + 6, offset + 12);
    SET_COLOR(mTitles[1].mColor, gColorWhite);
    FORMAT_HOURS(mLegendText[1], offset + 12, offset + 18);
    SET_COLOR(mTitles[2].mColor, gColorWhite);
    wcscpy(mLegendText[2], L"(%)");
}

void WeatherAround::StateZoom() {
    switch (mZoomPhase) {
    case 0:
        mZoomPhase++;
        break;
    case -1:
        break;
    default:
        gSimpleGlobe->UpdateZoom(gWeatherIconIds);
        if (gSimpleGlobe != NULL) {
            gSimpleGlobe->SyncZoom();
        }
        UpdateLabelScale(this);
        break;
    }
}

void WeatherAround::StateTilt() {
    switch (mTiltPhase) {
    case 0:
        mTiltPhase++;
        break;
    case -1:
        break;
    default:
        gSimpleGlobe->UpdateTilt(1, gWeatherIconIds2);
        break;
    }
}

void WeatherAround::UpdateBlink() {
    if (mBlinking) {
        switch (mBlinkDir) {
        case 0:
            mBlink += 0.05f;
            if (mBlink > 1.0f) {
                mBlink = 1.0f;
                mBlinkDir++;
            }
            break;
        default:
            mBlink -= 0.05f;
            if (mBlink < 0.0f) {
                mBlink = 0.0f;
                mBlinkDir--;
            }
            break;
        }
        mZoomOutI0Color1.a = mZoomOutAlpha * mBlink;
        mZoomOutButton->SetChildVisible("zoom_outI0", TRUE);
    } else {
        mBlinkDir = 0;
        mBlink = 0.0f;
        mZoomOutButton->SetChildVisible("zoom_outI0", FALSE);
    }
}

void WeatherAround::Open() {
    mActive = TRUE;
    mLayout->Reset();
    mCalcFunc = &WeatherAround::CalcActive;
}

void WeatherAround::PlayWeatherSound(CityLabel* label) {
    if (gLanguage == 0) {
        switch (gForecastPageStep) {
        case 0:
        case 3:
            break;
        default:
            return;
        }
    } else {
        switch (gForecastPageStep) {
        case 0:
        case 2:
        case 4:
            break;
        default:
            return;
        }
    }

    u32 code = label->GetWeatherCode(mDay);
    WeatherInfo* info = gForecastData->FindWeatherInfo(code);
    if (info != NULL) {
        if (gSimpleGlobe != NULL) {
            RequestWeatherSounds(info->mType->mIcon, sZoomVolumes[gSimpleGlobe->mZoomLevel]);
        } else {
            RequestWeatherSounds(info->mType->mIcon, 0.7f);
        }
    }
}

Vector2 WeatherAround::GetLabelSize(CityLabel* label) {
    if (mLabelSize) {
        return (this->*mLabelSize)(label);
    }
    return Vector2(75.0f, 80.0f);
}

Vector2 WeatherAround::GetIconSize(CityLabel* label) {
    return label->GetIconSize(mDay);
}

Vector2 WeatherAround::GetTempSize(CityLabel* label) {
    return label->GetTempSize(mDay);
}

Vector2 WeatherAround::GetTempSizeSmall(CityLabel* label) {
    return label->GetTempSizeSmall(mDay);
}

Vector2 WeatherAround::GetTempSizeLarge(CityLabel* label) {
    return label->GetTempSizeLarge(mDay);
}

inline void SetTextBoxColors(nw4r::lyt::TextBox* textBox, nw4r::ut::Color top, nw4r::ut::Color bottom) {
    // TextBox::mTextColors is protected
    nw4r::ut::Color* colors = (nw4r::ut::Color*)((u8*)textBox + 0xD8);
    colors[0] = top;
    colors[1] = bottom;
}

static void ZoomOutColorCallback(nw4r::lyt::Pane* pane, const GXColor* color) {
    char name[100] = "zoom_outT";
    strcat(name, GetLanguageCode());
    nw4r::lyt::Pane* child = pane->FindPaneByName(name, true);
    if (child != NULL) {
        nw4r::lyt::TextBox* textBox = nw4r::ut::DynamicCast<nw4r::lyt::TextBox*>(child);
        nw4r::ut::Color c = *color;
        f32 brightness = gMenuBrightness;
        c.a = c.a * brightness;
        SetTextBoxColors(textBox, c, c);
    }
}

static void ZoomOutCalcCallback(void* arg) {
    WeatherAround* self = (WeatherAround*)arg;
    if (self->mBlinking) {
        SetTevColors(self->mZoomOutI0, &self->mZoomOutI0Color0, &self->mZoomOutI0Color1);
        SetTevColors(self->mZoomOutI1, &self->mZoomOutI1Color0, &self->mZoomOutI1Color1);
    }
}

static Color sColorUnk0(0xFFFFFFFF);
static Color sColorUnk1(0);
