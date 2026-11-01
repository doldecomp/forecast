// d_s_weather.cpp: the main forecast scene
#include <channel/WeatherScene.h>
#include <channel/ColorWhite.h>
#include <channel/Fade.h>
#include <channel/ForecastData.h>
#include <channel/HomeButton.h>
#include <channel/LayoutButton.h>
#include <channel/SimpleGlobe.h>
#include <channel/Sound.h>
#include <channel/System.h>
#include <channel/Vec3.h>
#include <channel/Vector2.h>
#include <channel/WeatherSetting.h>
#include <channel/WeatherViews.h>

#include <nw4r/math.h>
#include <revolution/OS.h>
#include <revolution/TPL.h>

extern TPLPalette* gCommonTpl;
extern u32 gSceneFrameCount;
extern nw4r::snd::SoundHandle gAmbientSounds[5];
extern Vec gGlobeRotation;
extern const f32 gGlobeZooms[];
extern s32 gCursorState[4];

u32 GetUniversalMinutes();
f32 SmoothApproach(f32* value, f32 target, f32 rate, f32 maxStep, f32 minStep);
void UpdateDragScroll();


s32 gSettingResult = 2;
s32 gLastSettingResult = 2;
s32 gAmPm = 1;
s32 gAmPmNext = 2;
u8 gShowAmbientSound = TRUE;

extern const s32 gWeatherIconIds[];
extern const s32 gWeatherIconIds2[];

const s32 gWeatherIconIds[] = {2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
const s32 gWeatherIconIds2[] = {12, 13, 14, 15, 16, 17, 30, 28, 29, 34, 33, 31, 32, 34};
// Per-frame volume steps of the ambient sounds
static const f32 sFadeInDay[4] = {1.0f / 135.0f, 1.0f / 240.0f, 1.0f / 240.0f, 1.0f / 240.0f};
static const f32 sFadeOutDay[4] = {1.0f / 240.0f, 1.0f / 135.0f, 1.0f / 135.0f, 1.0f / 135.0f};
static const f32 sFadeInNight[4] = {1.0f / 120.0f, 1.0f / 240.0f, 1.0f / 240.0f, 1.0f / 240.0f};
static const f32 sFadeOutNight[4] = {1.0f / 240.0f, 1.0f / 120.0f, 1.0f / 120.0f, 1.0f / 120.0f};

ForecastData* gForecastData;
City** gCities;
City* gCurrentCity;
OSCalendarTime gCalendarTime;
LoopSound gThunderSound;
LoopSound gRainSound;
Vec3 sTitlePos(0.0f, 0.0f, 0.0f);
Vector2 gCityPos(0.0f, 0.0f);
f32 gTitleOffsetY;
static s32 sHoldTimer;
s32 gForecastPage;
s32 gForecastPageCount;
s32 gForecastPageStep;
static s32 sIsNight;
s32 gTempUnit;
s32 gWindUnit;
static s32 sTitleFlags;
u32 gHomeCountryCode;
u32 gCurrentCityId;
u32 gStartMinutes;
static u8 sSetupDone;
u8 gViewsCreated;
s32 gCursorState[4];

Color gColorWhite(255, 255, 255, 255);
Color gColorDarkGray(32, 32, 32, 255);
Color gColorRed(238, 0, 0, 255);
Color gColorCyan(0, 255, 255, 255);
Color gColorLightGray(238, 238, 238, 255);
Color gColorDarkGray2(32, 32, 32, 255);
Color gColorPink(255, 192, 192, 255);
Color gColorRed2(238, 0, 0, 255);
Color gColorCyan2(0, 255, 255, 255);
Color gColorBlue(0, 0, 255, 255);
Color gColorWhite2(255, 255, 255, 255);
Color gColorDarkGray3(32, 32, 32, 255);
Color gColorLightCyan(128, 255, 255, 255);
Color gColorOrange(255, 208, 0, 255);

static const GXColor sBtnColor0 = {0, 0, 0, 0};
static const GXColor sBtnColor1 = {255, 255, 255, 0};
static const GXColor sBtnColor2 = {0, 0, 0, 0};
static const GXColor sBtnColor3 = {255, 255, 255, 0};
static const GXColor sBtnColor4 = {0, 0, 0, 0};
static const GXColor sBtnColor4b = {255, 255, 255, 0};
static const GXColor sBtnColor4c = {255, 255, 255, 0};
static const GXColor sBtnColor5 = {0, 0, 0, 0};
static const GXColor sBtnColor6 = {0, 192, 255, 0};
static const GXColor sBtnColor6b = {255, 255, 255, 0};
static const GXColor sBtnColor6c = {0, 192, 255, 0};
static const GXColor sBtnColor6d = {89, 221, 255, 0};
static const GXColor sBtnColor7 = {255, 255, 255, 0};
static const GXColor sBtnColor8 = {0, 0, 0, 0};
static const GXColor sBtnColor9 = {255, 255, 255, 0};
static const GXColor sBtnColor9b = {0, 0, 0, 0};
static const GXColor sBtnColor9c = {255, 255, 255, 0};
static const GXColor sBtnColor10 = {193, 0, 0, 0};
static const GXColor sBtnColor11 = {193, 0, 0, 0};
static const GXColor sBtnColor12 = {255, 255, 255, 0};
static const GXColor sBtnColor13 = {255, 255, 255, 0};
static const GXColor sBtnColor14 = {0, 0, 0, 0};

ButtonColors gButtonColors[4] = {
    {sBtnColor0, sBtnColor1, sBtnColor2, sBtnColor3, sBtnColor4, sBtnColor5, sBtnColor6, sBtnColor7, sBtnColor8,
     sBtnColor9, sBtnColor10, sBtnColor11, sBtnColor12, sBtnColor13, sBtnColor14},
    {sBtnColor0, sBtnColor1, sBtnColor2, sBtnColor3, sBtnColor4b, sBtnColor5, sBtnColor6b, sBtnColor7, sBtnColor8,
     sBtnColor9b, sBtnColor10, sBtnColor11, sBtnColor12, sBtnColor13, sBtnColor14},
    {sBtnColor0, sBtnColor1, sBtnColor2, sBtnColor3, sBtnColor4c, sBtnColor5, sBtnColor6c, sBtnColor7, sBtnColor8,
     sBtnColor9c, sBtnColor10, sBtnColor11, sBtnColor12, sBtnColor13, sBtnColor14},
    {sBtnColor0, sBtnColor1, sBtnColor2, sBtnColor3, sBtnColor4, sBtnColor5, sBtnColor6d, sBtnColor7, sBtnColor8,
     sBtnColor9, sBtnColor10, sBtnColor11, sBtnColor12, sBtnColor13, sBtnColor14},
};

static inline City* LookupCity(u32 id) {
    City** city = gCities;
    for (int i = 0; i < (s32)gForecastData->mHeader->mNumPlaces; i++) {
        if (id == *(*city)->mInfo->mId) {
            return *city;
        }
        city++;
    }

    return NULL;
}

WeatherScene::WeatherScene()
    : SceneBase(false), mTPLWeather(NULL), mAddress(NULL), mNormal(NULL), mAround(NULL), mSetting(NULL), mState(NULL),
      mDrawFunc(NULL), mLogoColor(255, 255, 255, 255), mIconPos(0.0f, 0.0f, 0.0f), mStatePhase(0), mAroundOpened(0), mSettingOpen(FALSE), mConnect(NULL), mErrorWindow(NULL),
      mSaveData(NULL) {
    OSTicksToCalendarTime(OSGetTime(), &gCalendarTime);
    gStartMinutes = GetUniversalMinutes();
    sSetupDone = FALSE;
    gViewsCreated = FALSE;
    gShowAmbientSound = TRUE;
    gForecastPageStep = 0;

    if (gFatalRequested) {
        return;
    }

    if (gRegion == 1) {
        gMenuVisible = TRUE;
    }

    if (gLanguage == 0) {
        gForecastPage = 1;
        gForecastPageCount = 4;
    } else {
        gForecastPage = 1;
        gForecastPageCount = 5;
    }

    BOOL day = FALSE;
    if (gCalendarTime.hour >= 5 && gCalendarTime.hour < 18) {
        day = TRUE;
    }
    sIsNight = !day;

    sTitlePos.x = gWidescreen ? 36 : 28;
    sTitlePos.y = 365.0f;
    sTitlePos.z = 0.0f;
    gTitleOffsetY = 0.0f;
    gHomeCountryCode = gCountryCode;

    gSound = new Sound("rev_wtr.brsar", gHomeButton->mSoundData);
    if (gSound == NULL || gSound->mData == NULL) {
        OSReport("%s[%d]:m_pSound\n", "d_s_weather.cpp", 345);
        RequestFatal();
        return;
    }

    gForecastData = new ForecastData;

    mTPLWeather = LoadCompressedContentFile(gUnk80330B64, "TPLWeather.tpl.LZ", 32, NULL, gMEM2Heap);
    if (mTPLWeather == NULL) {
        OSReport("%s[%d]:m_pTPLWeather\n", "d_s_weather.cpp", 356);
        RequestFatal();
        return;
    }
    gUnk80330B74 = (u32)mTPLWeather;
    TPLBind((TPLPalette*)gUnk80330B74);

    mLayoutArc = LoadCompressedContentFile(gUnk80330B64, "weather_layout.arc.LZ", 32, NULL, gMEM2Heap);
    if (mLayoutArc == NULL) {
        OSReport("%s[%d]:m_pLayout\n", "d_s_weather.cpp", 367);
        RequestFatal();
        return;
    }

    mConnect = new Connect(mLayoutArc, gForecastData);
    mErrorWindow = new ErrorWindow(mLayoutArc);
    mSaveData = (SaveData*)MEM2Alloc(sizeof(SaveData), 32);
    SetSaveDataBuffer(mSaveData, sizeof(SaveData));
    mReadResult = 0;
    mWriteResult = 0;

    ChangeState(&WeatherScene::StateConnect);
}

WeatherScene::~WeatherScene() {
    if (mSetting != NULL) {
        delete mSetting;
    }

    if (mAround != NULL) {
        delete mAround;
    }

    if (mNormal != NULL) {
        delete mNormal;
    }

    s32 numPlaces;
    City** city = gCities;
    if (city != NULL) {
        numPlaces = gForecastData->mHeader->mNumPlaces;
        for (int i = 0; i < numPlaces; i++, city++) {
            if (*city != NULL) {
                delete *city;
            }
        }

        delete[] gCities;
        gCities = NULL;
    }

    if (mAddress != NULL) {
        delete mAddress;
    }

    if (gForecastData != NULL) {
        delete gForecastData;
        gForecastData = NULL;
    }

    if (mSaveData != NULL) {
        MEM2Free(mSaveData);
    }

    if (mErrorWindow != NULL) {
        delete mErrorWindow;
    }

    if (mConnect != NULL) {
        delete mConnect;
    }

    if (mLayoutArc != NULL) {
        MEM2Free(mLayoutArc);
    }

    if (mTPLWeather != NULL) {
        MEM2Free(mTPLWeather);
    }

    sSetupDone = FALSE;
    gViewsCreated = FALSE;
}

void WeatherScene::Exit(BOOL shutdownNet, s32 event) {
    for (int i = 0; i < 4; i++) {
        StopSound(&gAmbientSounds[i], 0);
    }

    if (gSound != NULL) {
        delete gSound;
        gSound = NULL;
    }

    SceneBase::Exit(shutdownNet, event);
}

void WeatherScene::Init() {}

void WeatherScene::Draw() {
    if (!gFatalRequested) {
        if (mDrawFunc) {
            (this->*mDrawFunc)();
        }

        SceneBase::Draw();
    }
}

void WeatherScene::unk38() {
    SetDefaultGXState();
    SetOrthoProjection();
    mLogoColor.a = mLogoAlpha;
    GXSetTevColor(GX_TEVREG0, mLogoColor);
    DrawTextureAt(gCommonTpl, GetLanguageTexture(), 1.0f, 1.0f, mLogoPos);
}

void WeatherScene::unk20() {
    SceneBase::unk20();
    gThunderSound.ClearRequest();
    gRainSound.ClearRequest();
}

void WeatherScene::unk2C() {
    SceneBase::unk2C();
    gThunderSound.Update();
    gRainSound.Update();
}

void WeatherScene::unk24() {
    if (gHoldAll) {
        sHoldTimer = 300;
    } else if (sHoldTimer != 0) {
        sHoldTimer--;
    }

    if (gCalendarTime.hour < 12) {
        gAmPm = 1;
        gAmPmNext = 2;
    } else {
        gAmPm = 2;
        gAmPmNext = 3;
    }

    UpdateDragScroll();

    if (mAddress != NULL) {
        mAddress->Calc();
    }

    if (!mSettingOpen && mSetting != NULL) {
        mSetting->Calc();
    }

    if (mNormal != NULL) {
        mNormal->Calc();
    }

    if (mAround != NULL) {
        mAround->Calc();
    }

    if (mState) {
        (this->*mState)();
    }

    SmoothApproach(&sTitlePos.y, (sTitleFlags & 1) ? 358 : 365, 0.1f, 5.0f, 1.0f);

    nw4r::snd::SoundHandle* sound = gAmbientSounds;
    Tween* volume = mVolumes;
    for (int i = 0; i < 4; i++, sound++, volume++) {
        if (IsSoundPlaying(sound)) {
            volume->Update();
            SetSoundVolume(sound, volume->mValue);
        }
    }

    if (!mVolumes[0].mValue && IsSoundPlaying(&gAmbientSounds[0])) {
        PauseSound(&gAmbientSounds[0], true, 0);
    }

    if (!mVolumes[1].mValue && !mVolumes[2].mValue) {
        if (IsSoundPlaying(&gAmbientSounds[1])) {
            PauseSound(&gAmbientSounds[1], true, 0);
        }

        if (IsSoundPlaying(&gAmbientSounds[2])) {
            PauseSound(&gAmbientSounds[2], true, 0);
        }
    }

    if (!mVolumes[3].mValue && IsSoundPlaying(&gAmbientSounds[3])) {
        PauseSound(&gAmbientSounds[3], true, 0);
    }
}

void WeatherScene::unk28() {
    for (int i = 0; i < 4; i++) {
        SetSoundVolume(&gAmbientSounds[i], 0.0f);
    }

    if (mConnect != NULL) {
        mConnect->PauseSound(true);
    }
}

BOOL WeatherScene::Setup() {
    if (sSetupDone) {
        return TRUE;
    }

    if (mReadResult == 0) {
        gCurrentCityId = mSaveData->mCityId;
        gTempUnit = mSaveData->mTempUnit;
        gWindUnit = mSaveData->mWindUnit;
    } else {
        gCurrentCityId = gHomeCountryCode;
        switch (gForecastData->mHeader->mUnitType) {
        case 1:
            gTempUnit = 1;
            gWindUnit = 0;
            break;
        case 2:
        default:
            gTempUnit = 0;
            gWindUnit = 1;
            break;
        }
    }

    mAddress = new WeatherAddress(mLayoutArc);

    TPLPalette* tpl = gCommonTpl;
    s32 width = GetScreenWidth();
    mLogoPos.x = 0.5f * width - 0.5f * GetTexWidth(tpl, GetLanguageTexture());
    TPLPalette* tpl2 = gCommonTpl;
    mLogoPos.y = 228.0f - 0.5f * GetTexHeight(tpl2, GetLanguageTexture());
    mLogoPos.z = 0.0f;
    mLogoAlpha = 0;
    mAddress->Reset();
    sSetupDone = TRUE;

    if (mReadResult == 0) {
        if (!CreateViews()) {
            return FALSE;
        }

        ClearHoveredButtons();
        gLastSettingResult = 2;
        gSettingResult = 2;
        ChangeState(&WeatherScene::StateNormal);
    } else {
        gLastSettingResult = 1;
        gSettingResult = 1;
        ChangeState(&WeatherScene::StateAddress);
    }

    gFade->FadeIn(25);
    return TRUE;
}

BOOL WeatherScene::CreateViews() {
    if (gViewsCreated) {
        return TRUE;
    }

    gGlobeRotation.x = gGlobeZooms[0];
    gGlobeRotation.y = 0.0f;
    gGlobeRotation.z = 0.0f;

    s32 numPlaces = gForecastData->mHeader->mNumPlaces;
    int i;
    City** city = gCities = new City*[numPlaces];
    CityInfo* info = gForecastData->mPlaces;
    for (i = 0; i < numPlaces; i++, city++, info++) {
        *city = new City(info);
    }

    gCurrentCity = LookupCity(gCurrentCityId);
    f32 y = 228.0f;
    gCityPos.x = 0.5f * GetScreenWidth();
    gCityPos.y = y;

    mNormal = new WeatherNormal(mLayoutArc);
    mAround = new WeatherAround(mLayoutArc);
    mSetting = new WeatherSetting(mLayoutArc);

    gSceneFrameCount = 0;
    gLastSettingResult = 2;
    gSettingResult = 2;
    sHoldTimer = 0;
    ClearHoveredButtons();

    gCursorState[0] = 0;
    gCursorState[1] = 0;
    gCursorState[2] = 0;
    gCursorState[3] = 0;

    gFade->FadeIn(25);

    f32 height = GetTexHeight(gCommonTpl, 4);
    f32 x = gWidescreen ? 36 : 28;
    mIconPos.y = 393.0f - height - 10.0f;
    mIconPos.x = x;

    ChangeState(&WeatherScene::StateNormal);

    mNormal->Reset();
    mAround->Reset();
    mSetting->Reset();

    if (!LoadEarthModel()) {
        OSReport("%s[%d]:Globe Read!!\n", "d_s_weather.cpp", 976);
        RequestFatal();
        return FALSE;
    }

    gViewsCreated = TRUE;
    return TRUE;
}

void WeatherScene::DrawAddress() {
    mAddress->Draw();
}

void WeatherScene::DrawSetting() {
    mSetting->Draw();
}

void WeatherScene::DrawNormal() {
    u32 id;

    mAround->Draw();
    mNormal->Draw();

    if (gLanguage == 0) {
        id = (sTitleFlags & 2) ? 0x17 : 0x16;
    } else {
        id = (sTitleFlags & 2) ? 0x19 : 0x18;
    }

    nw4r::math::VEC3 pos(sTitlePos.x, sTitlePos.y + gTitleOffsetY, sTitlePos.z);

    SetDefaultGXState();
    SetOrthoProjection();
    DrawTextureAt((TPLPalette*)gUnk80330B74, id, 1.0f, 1.0f, pos);

    if (sTitleFlags & 1) {
        mIconPos.x = pos.x;
        mIconPos.y = pos.y + GetTexHeight((TPLPalette*)gUnk80330B74, id);
        mIconPos.z = pos.z;
        TPLPalette* tpl = gCommonTpl;
        u32 icon = 3;
        if (sTitleFlags & 2) {
            icon = 6;
        }
        DrawTextureAt(tpl, icon, 1.0f, 1.0f, mIconPos);
    }
}

void WeatherScene::DrawConnect() {
    mConnect->Draw();
}

void WeatherScene::DrawError() {
    mErrorWindow->Draw();
}

void WeatherScene::StartSettingSounds() {
    mSoundIndex = 3;

    if (!IsSoundPlaying(&gAmbientSounds[3])) {
        StartSound(&gAmbientSounds[3], 0x22);
    } else {
        PauseSound(&gAmbientSounds[3], false, 0);
    }

    mVolumes[1].mTarget = 0.0f;
    mVolumes[1].mStep = (!sIsNight ? sFadeOutDay[1] : sFadeOutNight[1]);
    mVolumes[0].mTarget = 0.0f;
    mVolumes[0].mStep = (!sIsNight ? sFadeOutDay[0] : sFadeOutNight[0]);
    mVolumes[2].mTarget = 0.0f;
    mVolumes[2].mStep = (!sIsNight ? sFadeOutDay[2] : sFadeOutNight[2]);
    mVolumes[3].mTarget = 1.0f;
    mVolumes[3].mStep = (!sIsNight ? sFadeInDay[mSoundIndex] : sFadeInNight[mSoundIndex]);
}

BOOL WeatherScene::StateAddress() {
    switch (mStatePhase) {
    case -1:
        mAddress->Close();
        break;
    case 0:
        mStatePhase++;
        gLastSettingResult = 1;
        gSettingResult = 1;
        mDrawFunc = &WeatherScene::DrawAddress;
        StartSettingSounds();
        mAddress->Reset();
        mAddress->SelectCurrentCity();
        gMenuVisible = TRUE;
        break;
    default:
        switch (mStatePhase) {
        case 1:
            if (gFade->mFading == 0) {
                mStatePhase++;
            }
            break;
        case 2:
            switch (gSettingResult) {
            case 2:
                mStatePhase++;
                gFade->FadeOut(25);
                return TRUE;
            case 4:
                mStatePhase = 4;
                gFade->FadeOut(25);
                return TRUE;
            }
            break;
        case 3:
            if (gFade->mFading == 0) {
                mSaveData->mCityId = gCurrentCityId;
                mSaveData->mCountryCode = gHomeCountryCode;
                mSaveData->mTempUnit = gTempUnit;
                mSaveData->mWindUnit = gWindUnit;
                mWriteResult = WriteSaveData();
                if (mWriteResult == 0) {
                    if (!CreateViews()) {
                        return FALSE;
                    }

                    gFade->FadeIn(25);
                    gLastSettingResult = gSettingResult;
                    ClearHoveredButtons();
                    ChangeState(&WeatherScene::StateNormal);
                } else {
                    ClearHoveredButtons();
                    ChangeState(&WeatherScene::StateError);
                }
                return TRUE;
            }
            break;
        case 4:
        default:
            if (gFade->mFading == 0) {
                if (!CreateViews()) {
                    return FALSE;
                }

                mSetting->Reset();
                gFade->FadeIn(25);
                gLastSettingResult = gSettingResult;
                ClearHoveredButtons();
                ChangeState(&WeatherScene::StateSetting);
                return TRUE;
            }
            break;
        }
        break;
    }

    return TRUE;
}

void WeatherScene::StartNormalSounds() {
    if (gShowAmbientSound) {
        if (!IsSoundPlaying(&gAmbientSounds[0])) {
            if (!sIsNight) {
                StartSound(&gAmbientSounds[0], 0x1E);
            } else {
                StartSound(&gAmbientSounds[0], 0x21);
            }

            mVolumes[0].mTarget = 1.0f;
            mVolumes[0].mValue = 1.0f;
            SetSoundVolume(&gAmbientSounds[0], 1.0f);
        } else {
            PauseSound(&gAmbientSounds[0], false, 0);
            mVolumes[0].mTarget = 1.0f;
            mVolumes[0].mStep = (!sIsNight ? sFadeInDay[0] : sFadeInNight[0]);
        }

        mVolumes[1].mTarget = 0.0f;
        mVolumes[1].mStep = (!sIsNight ? sFadeOutDay[1] : sFadeOutNight[1]);
    } else {
        PauseSound(&gAmbientSounds[1], false, 0);
        PauseSound(&gAmbientSounds[2], false, 0);
        mVolumes[0].mTarget = 0.0f;
        mVolumes[0].mStep = (!sIsNight ? sFadeOutDay[0] : sFadeOutNight[0]);
        mVolumes[1].mTarget = 1.0f;
        mVolumes[1].mStep = (!sIsNight ? sFadeInDay[1] : sFadeInNight[1]);
    }

    mVolumes[2].mTarget = 0.0f;
    mVolumes[2].mStep = (!sIsNight ? sFadeOutDay[2] : sFadeOutNight[2]);
    mVolumes[3].mTarget = 0.0f;
    mVolumes[3].mStep = (!sIsNight ? sFadeOutDay[mSoundIndex] : sFadeOutNight[mSoundIndex]);
}

BOOL WeatherScene::StateNormal() {
    switch (mStatePhase) {
    case -1:
        mNormal->mActive = FALSE;
        break;
    case 0:
        mStatePhase++;
        sTitleFlags = gShowAmbientSound == FALSE;
        mDrawFunc = &WeatherScene::DrawNormal;
        StartNormalSounds();
        mNormal->SetCity();
        mNormal->Open();
        if (gRegion != 1) {
            gMenuVisible = FALSE;
        }
        break;
    default:
        switch (mStatePhase) {
        case 1:
            switch (gSettingResult) {
            case 5:
                mStatePhase = 4;
                gSettingResult = 3;
                mNormal->mCalcFunc = &WeatherNormal::DrawCity;
                gFade2->SetBaseColor(0, 0, 0, 255);
                gFade2->FadeOut(20);
                mLogoAlphaStep = 12;
                mWait = 40;
                StartAroundSounds();
                break;
            case 4:
                mStatePhase = 3;
                gFade->FadeOut(25);
                mNormal->mCalcFunc = &WeatherNormal::DrawCity;
                break;
            case 3:
                mWait = 12;
                mStatePhase++;
                mNormal->mCalcFunc = &WeatherNormal::DrawCity;
                StartAroundSounds();
                break;
            }
            break;
        case 2:
            if (mWait != 0) {
                mWait--;
                break;
            }

            gLastSettingResult = gSettingResult;
            ClearHoveredButtons();
            ChangeState(&WeatherScene::StateAround);
            return TRUE;
        case 3:
            if (gFade->mFading == 0) {
                gLastSettingResult = gSettingResult;
                ClearHoveredButtons();
                mNormal->OpenAround();
                ChangeState(&WeatherScene::StateSetting);
                return TRUE;
            }
            break;
        case 4:
            mLogoAlpha += mLogoAlphaStep;
            if (mLogoAlpha > 255) {
                mLogoAlpha = 255;
            }

            if (gFade2->mFading == 0) {
                if (mWait != 0) {
                    mWait--;
                } else if (gEarthModel != NULL) {
                    mStatePhase++;
                    CityInfo* info = gCurrentCity != NULL ? gCurrentCity->mInfo : NULL;
                    if (info != NULL) {
                        PlaceEntry* place = (PlaceEntry*)info->mId;
                        Vec2 deg;
                        ToDegrees(place->mLongitude, place->mLatitude, &deg);
                        nw4r::math::VEC3 rot(deg.x, deg.y, 0.0f);
                        gSimpleGlobe->SetRotation(rot, 0);
                        gSimpleGlobe->SetZoom(0, 0);
                        gSimpleGlobe->SetMode(0);
                        gSimpleGlobe->SetSpeed(0.0f);
                    }

                    gFade2->FadeIn(40);
                    mNormal->OpenAround();
                    mNormal->mActive = FALSE;
                    mAround->mActive = TRUE;
                    if (mAroundOpened == 0) {
                        mAroundOpened++;
                        mAround->Show();
                    }

                    sTitleFlags = 3;
                }
            }
            break;
        case 5:
        default:
            mLogoAlpha -= mLogoAlphaStep;
            if (mLogoAlpha < 0) {
                mLogoAlpha = 0;
            }

            if (gFade2->mFading == 0) {
                mLogoAlpha = 0;
                gLastSettingResult = gSettingResult;
                ClearHoveredButtons();
                ChangeState(&WeatherScene::StateAround);
                return TRUE;
            }
            break;
        }
        break;
    }

    return TRUE;
}

void WeatherScene::StartAroundSounds() {
    mSoundIndex = 2;
    mVolumes[0].mTarget = 0.0f;
    mVolumes[0].mStep = (!sIsNight ? sFadeOutDay[0] : sFadeOutNight[0]);

    if (!IsSoundPlaying(&gAmbientSounds[1])) {
        if (!sIsNight) {
            StartSound(&gAmbientSounds[1], 0x1C);
        } else {
            StartSound(&gAmbientSounds[1], 0x1F);
        }
    } else {
        PauseSound(&gAmbientSounds[1], false, 0);
    }

    if (!IsSoundPlaying(&gAmbientSounds[2])) {
        if (!sIsNight) {
            StartSound(&gAmbientSounds[2], 0x1D);
        } else {
            StartSound(&gAmbientSounds[2], 0x20);
        }
    } else {
        PauseSound(&gAmbientSounds[2], false, 0);
    }

    mVolumes[1].mTarget = 1.0f;
    mVolumes[1].mStep = (!sIsNight ? sFadeInDay[1] : sFadeInNight[1]);
    mVolumes[2].mTarget = 1.0f;
    mVolumes[2].mStep = (!sIsNight ? sFadeInDay[2] : sFadeInNight[2]);
    mVolumes[3].mTarget = 0.0f;
    mVolumes[3].mStep = (!sIsNight ? sFadeOutDay[mSoundIndex] : sFadeOutNight[mSoundIndex]);
}

static inline void UpdateForecastPage() {
    if (gLanguage == 0) {
        switch (gForecastPageStep) {
        case 3:
        case 4:
        case 5:
            gForecastPage = 2;
            break;
        case 0:
        case 1:
        case 2:
        default:
            gForecastPage = 1;
            break;
        }
    } else {
        switch (gForecastPageStep) {
        case 4:
        case 5:
            gForecastPage = 3;
            break;
        case 2:
        case 3:
            gForecastPage = 2;
            break;
        default:
            gForecastPage = 1;
            break;
        }
    }
}

BOOL WeatherScene::StateAround() {
    switch (mStatePhase) {
    case -1:
        mAround->mActive = FALSE;
        break;
    case 0:
        mStatePhase++;
        mDrawFunc = &WeatherScene::DrawNormal;
        sTitleFlags = 3;
        if (gRegion != 1) {
            gMenuVisible = FALSE;
        }
        mAround->unk250 = FALSE;
        mAround->Open();
        break;
    default:
        switch (mStatePhase) {
        case 1:
            switch (gSettingResult) {
            case 6:
                mStatePhase++;
                gSettingResult = 2;
                mAround->mCalcFunc = &WeatherAround::DrawGlobe;
                gFade2->SetBaseColor(0, 0, 0, 255);
                gFade2->FadeOut(12);
                gShowAmbientSound = TRUE;
                StartNormalSounds();
                mWait = 30;
                break;
            case 2:
                gLastSettingResult = gSettingResult;
                UpdateForecastPage();
                ClearHoveredButtons();
                mAround->unk250 = TRUE;
                mAround->mCalcFunc = &WeatherAround::DrawGlobe;
                mNormal->CloseAround();
                ChangeState(&WeatherScene::StateNormal);
                return TRUE;
            }
            break;
        case 2:
            if (gFade2->mFading == 0) {
                if (mWait != 0) {
                    mWait--;
                } else {
                    mStatePhase++;
                    sTitleFlags = 0;
                    sTitlePos.y = 365.0f;
                    gFade2->FadeIn(25);
                    gShowAmbientSound = TRUE;
                    UpdateForecastPage();
                    mAround->mActive = FALSE;
                    mNormal->mActive = TRUE;
                    mNormal->SetCity();
                    mNormal->Show();
                }
            }
            break;
        default:
            if (gFade2->mFading == 0) {
                gLastSettingResult = gSettingResult;
                ClearHoveredButtons();
                ChangeState(&WeatherScene::StateNormal);
                return TRUE;
            }
            break;
        }
        break;
    }

    return TRUE;
}

BOOL WeatherScene::StateSetting() {
    switch (mStatePhase) {
    case -1:
        mSettingOpen = FALSE;
        break;
    case 0:
        mStatePhase++;
        mDrawFunc = &WeatherScene::DrawSetting;
        StartSettingSounds();
        mSetting->Reset();
        gFade->FadeIn(25);
        gMenuVisible = TRUE;
        break;
    default:
        switch (mStatePhase) {
        case 1:
            if (gFade->mFading == 0) {
                mStatePhase++;
                mSetting->Open();
            }
            break;
        case 2: {
            s32 result = gLastSettingResult;
            if (gSettingResult != result) {
                gLastSettingResult = gSettingResult;
                result = gSettingResult;
                ClearHoveredButtons();
            }

            switch (result) {
            case 1:
                mStatePhase = 4;
                gFade->FadeOut(25);
                mSetting->Close();
                mSettingOpen = TRUE;
                break;
            case 2:
                mStatePhase++;
                gFade->FadeOut(25);
                mSetting->Close();
                break;
            }
            break;
        }
        case 3:
            if (gFade->mFading == 0) {
                if (mSaveData->mCityId != gCurrentCityId || mSaveData->mTempUnit != gTempUnit ||
                    mSaveData->mWindUnit != gWindUnit) {
                    mSaveData->mCityId = gCurrentCityId;
                    mSaveData->mCountryCode = gHomeCountryCode;
                    mSaveData->mTempUnit = gTempUnit;
                    mSaveData->mWindUnit = gWindUnit;
                    mWriteResult = WriteSaveData();
                    if (mWriteResult != 0) {
                        ChangeState(&WeatherScene::StateError);
                        return TRUE;
                    }
                }

                gFade->FadeIn(25);
                gShowAmbientSound = TRUE;
                mNormal->Show();
                ChangeState(&WeatherScene::StateNormal);
                return TRUE;
            }
            break;
        case 4:
            if (gFade->mFading == 0) {
                gFade->FadeIn(25);
                ChangeState(&WeatherScene::StateAddress);
                return TRUE;
            }
            break;
        }
        break;
    }

    return TRUE;
}

BOOL WeatherScene::StateConnect() {
    switch (mStatePhase) {
    case -1:
        break;
    case 0:
        gLastSettingResult = 0;
        gSettingResult = 0;
        gMenuVisible = TRUE;
        mStatePhase = 1;
        break;
    case 1:
        gFade->SetOpaque();
        mConnect->Start();
        mDrawFunc = &WeatherScene::DrawConnect;
        mStatePhase = 2;
        break;
    case 2:
        mConnect->Calc();
        if (mConnect->IsDone()) {
            mStatePhase = 3;
        }
        break;
    case 3:
        mReadResult = ReadSaveData();
        switch (mReadResult) {
        case 0:
            if (mSaveData->mCountryCode == gHomeCountryCode && gForecastData->FindForecast(mSaveData->mCityId)) {
                mStatePhase = 9;
            } else {
                mErrorWindow->Open(3);
                mDrawFunc = &WeatherScene::DrawError;
                mStatePhase = 4;
            }
            break;
        case 1:
            mErrorWindow->Open(1);
            mDrawFunc = &WeatherScene::DrawError;
            mStatePhase = 4;
            break;
        case 2:
            mErrorWindow->Open(2);
            mDrawFunc = &WeatherScene::DrawError;
            mStatePhase = 4;
            PlaySE(25);
            break;
        case 3:
            mErrorWindow->Open(4);
            mDrawFunc = &WeatherScene::DrawError;
            mStatePhase = 4;
            PlaySE(25);
            break;
        }
        break;
    case 4:
        mErrorWindow->Calc();
        if (mErrorWindow->mState == 5) {
            mStatePhase = 9;
            if (mReadResult == 0) {
                mReadResult = 1;
            }
        }
        break;
    case 9:
    default:
        Setup();
        break;
    }

    return TRUE;
}

BOOL WeatherScene::StateError() {
    switch (mStatePhase) {
    case -1:
        break;
    case 0:
        if (mWriteResult == 3) {
            mErrorWindow->Open(4);
        } else {
            mErrorWindow->Open(5);
        }

        mDrawFunc = &WeatherScene::DrawError;
        mStatePhase = 1;
        PlaySE(25);
        break;
    case 1:
        mErrorWindow->Calc();
        break;
    }

    return TRUE;
}

void WeatherScene::unk3C() {
    for (int i = 0; i < 4; i++) {
        BOOL valid = FALSE;
        if (gPointerValid[i][0] && gKPADLatest[i] >= 0) {
            valid = TRUE;
        }

        if (valid) {
            s32 type;
            switch (gCursorState[i]) {
            case 4:
                type = 5;
                break;
            case 1:
                type = 3;
                break;
            case 2:
                type = 2;
                break;
            case 0:
            default:
                type = 1;
                break;
            }

            SetCursor(i, type);
        }
    }
}

City* FindCity(u32 id) {
    City** city = gCities;
    for (int i = 0; i < (s32)gForecastData->mHeader->mNumPlaces; i++) {
        if (id == *(*city)->mInfo->mId) {
            return *city;
        }
        city++;
    }

    return NULL;
}

void UpdateCurrentCity() {
    gCurrentCity = LookupCity(gCurrentCityId);
    gCityPos.x = 0.5f * GetScreenWidth();
    gCityPos.y = 228.0f;
}

s32 LoadForecastData() {
    gForecastData = new ForecastData;

    switch (gForecastData->LoadForecast(NULL)) {
    case 10:
        OSReport("ERROR_MESSAGE_e !!\n");
        return 10;
    case 0:
        OSReport("MEM_ERROR_ForecastBin_e !!\n");
        break;
    case 2:
        OSReport("MEM_ERROR_WeatherForecasts_e !!\n");
        break;
    case 3:
        OSReport("MEM_ERROR_WeatherSummary_e !!\n");
        break;
    case 4:
        OSReport("MEM_ERROR_WeatherType_e !!\n");
        break;
    case 7:
        OSReport("MEM_ERROR_UVIndex_e !!\n");
        break;
    case 8:
        OSReport("MEM_ERROR_LaundryIndex_e !!\n");
        break;
    case 9:
        OSReport("MEM_ERROR_PollenIndex_e !!\n");
        break;
    case 5:
        OSReport("MEM_ERROR_Places_e !!\n");
        break;
    case 11:
        OSReport("ERROR_NumForecasts_e !!\n");
        break;
    case 12:
        OSReport("ERROR_NumWeatherTypes_e !!\n");
        break;
    case 13:
        OSReport("ERROR_NumPlaces_e !!\n");
        break;
    case 14:
        OSReport("ERROR_ShortNumNowWeathers_e !!\n");
        break;
    case 15:
        OSReport("ERROR_ForecastArrayAddr_e !!\n");
        break;
    case 16:
        OSReport("ERROR_SummaryForecastArrayAddr_e !!\n");
        break;
    case 17:
        OSReport("ERROR_WeatherTypesArrayAddr_e !!\n");
        break;
    case 18:
        OSReport("ERROR_PlaceArrayAddr_e !!\n");
        break;
    case 19:
        OSReport("ERROR_WeatherTypeNameAddr_e !!\n");
        break;
    case 24:
        break;
    default:
        OSReport("天気情報生成失敗!!\n");
        break;
    }

    switch (gForecastData->LoadShort(NULL)) {
    case 1:
        OSReport("MEM_ERROR_ShortBin_e !!\n");
        break;
    case 6:
        OSReport("MEM_ERROR_WeatherNow_e !!\n");
        break;
    case 14:
        OSReport("ERROR_ShortNumNowWeathers_e !!\n");
        break;
    case 20:
        OSReport("ERROR_ShortNowWeatherArrayAddr_e !!\n");
        break;
    case 24:
        break;
    default:
        OSReport("天気情報生成失敗!!\n");
        break;
    }

    return 24;
}

void RequestWeatherSounds(u32 weather, f32 volume) {
    BOOL rain = FALSE;
    BOOL thunder = FALSE;

    switch (weather & 0x7FFF) {
    case 3:
    case 7:
    case 12:
    case 16:
    case 19:
    case 20:
    case 21:
    case 22:
    case 23:
    case 24:
    case 25:
    case 31:
    case 32:
    case 103:
    case 105:
    case 108:
    case 110:
    case 111:
    case 112:
    case 113:
    case 114:
    case 115:
    case 119:
    case 120:
    case 121:
        rain = TRUE;
        break;
    case 5:
    case 9:
    case 14:
    case 18:
    case 125:
        rain = TRUE;
        thunder = TRUE;
        break;
    }

    if (rain) {
        gRainSound.Request(0x31, volume, 1.0f, 0.0f);
    }

    if (thunder) {
        gThunderSound.Request(0x32, volume, 1.0f, 0.0f);
    }
}
