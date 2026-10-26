#ifndef CHANNEL_WEATHER_SCENE_H
#define CHANNEL_WEATHER_SCENE_H
#include <types.h>
#include <channel/SceneBase.h>
#include <channel/Tween.h>
#include <channel/Color.h>
#include <channel/LoopSound.h>
#include <revolution/GX.h>
#include <nw4r/ut/ut_Color.h>
#include <nw4r/math/math_types.h>
#include <revolution/MTX.h>
#include <revolution/OS.h>

class Connect;
class ErrorWindow;
struct SaveData;
class WeatherAddress;
class WeatherAround;
class WeatherNormal;
class WeatherSetting;

// 'WTH2': the main forecast scene (d_s_weather.cpp). Runs its own state machine on top of SceneBase's:
// connect -> pick a city (first run) -> forecast <-> surrounding cities / settings.
class WeatherScene : public SceneBase {
public:
    typedef BOOL (WeatherScene::*StateFunc)();
    typedef void (WeatherScene::*DrawFunc)();

    WeatherScene();
    virtual ~WeatherScene();                         // at 0x8
    virtual void Exit(BOOL shutdownNet, s32 event);  // at 0xC
    virtual void Init();                             // at 0x1C
    virtual void unk20();                            // at 0x20
    virtual void unk24();                            // at 0x24
    virtual void unk28();                            // at 0x28
    virtual void unk2C();                            // at 0x2C
    virtual void Draw();                             // at 0x30
    virtual void unk38();                            // at 0x38
    virtual void unk3C();                            // at 0x3C

    BOOL Setup();
    BOOL CreateViews();
    void DrawAddress();
    void DrawSetting();
    void DrawNormal();
    void DrawConnect();
    void DrawError();
    void StartSettingSounds();
    BOOL StateAddress();
    void StartNormalSounds();
    BOOL StateNormal();
    void StartAroundSounds();
    BOOL StateAround();
    BOOL StateSetting();
    BOOL StateConnect();
    BOOL StateError();

    void ChangeState(StateFunc state) {
        if (mState) {
            mStatePhase = -1;
            (this->*mState)();
        }

        mStatePhase = 0;
        mState = state;

        if (mState) {
            (this->*mState)();
        }
    }

    void* mTPLWeather;         // at 0xAC, TPLWeather.tpl
    WeatherAddress* mAddress;  // at 0xB0
    WeatherNormal* mNormal;    // at 0xB4
    WeatherAround* mAround;    // at 0xB8
    WeatherSetting* mSetting;  // at 0xBC
    StateFunc mState;          // at 0xC0
    DrawFunc mDrawFunc;        // at 0xCC
    nw4r::ut::Color mLogoColor; // at 0xD8
    nw4r::math::VEC3 mIconPos; // at 0xDC
    nw4r::math::VEC3 mLogoPos; // at 0xE8
    Tween mVolumes[4];         // at 0xF4, one per ambient sound
    u8 unk124[0x134 - 0x124];  // at 0x124
    s32 mSoundIndex;           // at 0x134
    s32 mStatePhase;           // at 0x138
    s32 mAroundOpened;         // at 0x13C
    s32 mLogoAlpha;            // at 0x140
    s32 mWait;                 // at 0x144
    s32 mLogoAlphaStep;        // at 0x148
    u8 unk14C[0x154 - 0x14C];  // at 0x14C
    u8 mSettingOpen;           // at 0x154
    Connect* mConnect;         // at 0x158
    ErrorWindow* mErrorWindow; // at 0x15C
    SaveData* mSaveData;       // at 0x160
    s32 mReadResult;           // at 0x164
    s32 mWriteResult;          // at 0x168
};

// The colors of a button in one state: TEV colors 0 and 1 of its "<name>B" and
// "<name>I" panes, and the color of its text
struct ButtonStateColors {
    nw4r::ut::Color mBase0; // at 0x0
    nw4r::ut::Color mBase1; // at 0x4
    nw4r::ut::Color mIcon0; // at 0x8
    nw4r::ut::Color mIcon1; // at 0xC
    nw4r::ut::Color mText;  // at 0x10
};

// The colors of one LayoutButton type (size 0x3C). 4-byte aligned: tables of it
// copy their first color as one word (LayoutButton's sDefaultColors __sinit).
struct ButtonColors {
#ifdef NW4R_HEADER_DECLARATIONS_ONLY
    ~ButtonColors();
#else
    ~ButtonColors() {}
#endif

    ButtonStateColors mNormal;   // at 0x0
    ButtonStateColors mActive;   // at 0x14, hovered or pressed
    ButtonStateColors mSelected; // at 0x28
} ATTRIBUTE_ALIGN(4);

extern WeatherScene* gWeatherScene;

extern OSCalendarTime gCalendarTime;
extern s32 gTempUnit;
extern s32 gWindUnit;
extern u32 gCurrentCityId;
extern u32 gHomeCountryCode;
extern s32 gSettingResult;
extern s32 gLastSettingResult;
extern ButtonColors gButtonColors[4];

extern Color gColorWhite;
extern Color gColorDarkGray;
extern Color gColorRed;
extern Color gColorCyan;
extern Color gColorLightGray;
extern Color gColorDarkGray2;
extern Color gColorPink;
extern Color gColorRed2;
extern Color gColorCyan2;
extern Color gColorBlue;
extern Color gColorWhite2;
extern Color gColorDarkGray3;
extern Color gColorLightCyan;
extern Color gColorOrange;

extern const wchar_t* gOtherRegionText[];
extern const wchar_t* gWindDirNamesEU[];
extern const wchar_t* gWindDirNamesUS[];
extern const wchar_t* gWindUnitNames[];
extern const wchar_t* gWindUnitNames2[];

#endif
