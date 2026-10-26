// d_weather_setting.cpp: settings screen (temperature unit, wind unit, selected city)
#include <channel/WeatherSetting.h>
#include <channel/ColorWhite.h>
#include <channel/LayoutButton.h>
#include <channel/SceneBase.h>
#include <channel/System.h>
#include <channel/ForecastData.h>
#include <channel/WeatherScene.h>

#include <wstring.h>
#include <nw4r/ut.h>

extern nw4r::ut::TextWriterBase<wchar_t> gTextWriter;
extern nw4r::ut::ArchiveFont* gSysFont;

static Color sColorTranslucent(255, 255, 255, 64);
static Color sColorBlack(0, 0, 0, 255);

WeatherSetting::WeatherSetting(void* arc)
    : mSetLayout(NULL), mBaseLayout(NULL), mKionSet(NULL), mCitySet(NULL), mWindSet(NULL), mKionBtn(NULL),
      mCityBtn(NULL), mWindBtn(NULL), mState(NULL), mTextX(0.0f), mTextY(0.0f), mTextMaxWidth(0.0f),
      mTextHeight(0.0f), mTextScaleX(1.0f), mTextScaleY(1.0f), mTextCharSpace(0.0f), mKionBrightness(64),
      mCityBrightness(64), mWindBrightness(64), mStatePhase(0), unkBC(0) {
    mSetLayout = new ButtonGroup(arc, "set.brlyt", gButtonColors, false);
    mBaseLayout = new ButtonGroup(arc, "base.brlyt", gButtonColors, false);

    mKionSet = mSetLayout->FindButton("kion_set");
    if (mKionSet == NULL) {
        OSPanic("d_weather_setting.cpp", 97, "kion_set がないです!!\n");
    }
    mKionSet->mToggle = TRUE;

    mCitySet = mSetLayout->FindButton("city_set");
    if (mCitySet == NULL) {
        OSPanic("d_weather_setting.cpp", 104, "city_set がないです!!\n");
    }
    mCitySet->mToggle = TRUE;

    mCityBtn = mSetLayout->FindButton("city_btn");
    if (mCityBtn == NULL) {
        OSPanic("d_weather_setting.cpp", 111, "city_btn がないです!!\n");
    }
    mCityBtn->SetLinked(mCitySet);

    mKionBtn = mSetLayout->FindButton("kion_btn");
    if (mKionBtn == NULL) {
        OSPanic("d_weather_setting.cpp", 118, "kion_btn がないです!!\n");
    }
    mKionBtn->SetLinked(mKionSet);

    if (gLanguage == 0) {
        LayoutButton* button;
        if ((button = mSetLayout->FindButton("wind")) != NULL) {
            button->Hide();
        }
        if ((button = mSetLayout->FindButton("wind_set")) != NULL) {
            button->Hide();
        }
        if ((button = mSetLayout->FindButton("wind_btn")) != NULL) {
            button->Hide();
        }
    } else {
        LayoutButton* button;
        if ((button = mSetLayout->FindButton("text")) != NULL) {
            button->Hide();
        }

        mWindSet = mSetLayout->FindButton("wind_set");
        if (mWindSet == NULL) {
            OSPanic("d_weather_setting.cpp", 153, "wind_set がないです!!\n");
        }
        mWindSet->mToggle = TRUE;

        mWindBtn = mSetLayout->FindButton("wind_btn");
        if (mWindBtn == NULL) {
            OSPanic("d_weather_setting.cpp", 160, "wind_btn がないです!!\n");
        }
        mWindBtn->SetLinked(mWindSet);
    }

    f32 halfWidth = 0.5f * GetScreenWidth();
    f32 halfHeight = 0.5f * GetScreenHeight();
    nw4r::math::VEC3 pos;
    pos.x = mCitySet->mTextPos.x;
    pos.y = mCitySet->mTextPos.y;
    pos.z = mCitySet->mTextPos.z;
    nw4r::math::VEC2 size;
    size.x = mCitySet->mTextSize.x;
    size.y = mCitySet->mTextSize.y;
    mTextX = halfWidth + pos.x;
    mTextY = halfHeight - pos.y;
    mTextMaxWidth = size.x;
    mTextHeight = size.y;

    UpdateCityName();

    mKionSet->SetState(gTempUnit);
    if (gLanguage != 0) {
        mWindSet->SetState(gWindUnit);
    }

    ChangeState(&WeatherSetting::StateIdle);
}

WeatherSetting::~WeatherSetting() {
    delete mBaseLayout;
    delete mSetLayout;
}

void WeatherSetting::Reset() {
    mSetLayout->Reset();
    mBaseLayout->Reset();

    mCityBrightness = Lerp(64, 192, mCityBtn->mHoverFrame, 12);
    mCitySet->mBrightness = mCityBrightness;
    mKionBrightness = Lerp(64, 192, mKionBtn->mHoverFrame, 12);
    mKionSet->mBrightness = mKionBrightness;

    if (gLanguage != 0) {
        mWindBrightness = Lerp(64, 192, mWindBtn->mHoverFrame, 12);
        mWindSet->mBrightness = mWindBrightness;
    }
}

void WeatherSetting::Draw() {
    mBaseLayout->Draw();
    mSetLayout->Draw();

    GXColor color = mCitySet->mTextColor;
    SetDefaultGXState();
    SetOrthoProjection();
    gTextWriter.SetFont(*gSysFont);
    gTextWriter.SetDrawFlag(0x111);
    gTextWriter.SetupGX();
    gTextWriter.SetTextColor(color);
    gTextWriter.SetCharSpace(mTextCharSpace);
    gTextWriter.SetScale(mTextScaleX, mTextScaleY);
    gTextWriter.SetCursor(mTextX, mTextY);
    gTextWriter.Print(mCityName);
}

void WeatherSetting::Calc() {
    if (mState) {
        (this->*mState)(0);
    }
}

void WeatherSetting::Open() {
    ChangeState(&WeatherSetting::StateMain);
}

void WeatherSetting::Close() {
    ChangeState(&WeatherSetting::StateIdle);
}

void WeatherSetting::StateMain(s32 arg) {
    switch (mStatePhase) {
    case 0:
        mStatePhase++;
        UpdateCityName();
        mKionSet->SetState(gTempUnit);
        if (gLanguage != 0) {
            mWindSet->SetState(gWindUnit);
        }
        break;
    case -1:
        break;
    default:
        mBaseLayout->Calc();
        mSetLayout->Calc();
        UpdateButtons(mSetLayout, 40);

        mCityBrightness = Lerp(64, 192, mCityBtn->mHoverFrame, 12);
        mCitySet->mBrightness = mCityBrightness;
        mKionBrightness = Lerp(64, 192, mKionBtn->mHoverFrame, 12);
        mKionSet->mBrightness = mKionBrightness;

        if (gLanguage != 0) {
            mWindBrightness = Lerp(64, 192, mWindBtn->mHoverFrame, 12);
            mWindSet->mBrightness = mWindBrightness;
        }

        if (CheckButtonPressed("back", WPAD_BUTTON_A) >= 0) {
            LayoutButton* back = mSetLayout->FindButton("back");
            back->mPressed = TRUE;
            PlaySE(39);
            gSettingResult = 2;
        } else if (CheckButtonPressed("city_btn", WPAD_BUTTON_A) >= 0) {
            gSettingResult = 1;
            PlaySE(45);
        } else {
            if (CheckButtonPressed("kion_btn", WPAD_BUTTON_A) >= 0) {
                gTempUnit = !gTempUnit;
                mKionSet->SetState(gTempUnit);
                PlaySE(44);
            }

            if (gLanguage != 0 && CheckButtonPressed("wind_btn", WPAD_BUTTON_A) >= 0) {
                gWindUnit = !gWindUnit;
                if (gLanguage != 0) {
                    mWindSet->SetState(gWindUnit);
                }
                PlaySE(44);
            }
        }
        break;
    }
}

void WeatherSetting::StateIdle(s32 arg) {
    switch (mStatePhase) {
    case 0:
        mStatePhase++;
        break;
    case -1:
        break;
    default:
        mKionSet->SetState(gTempUnit);
        if (gLanguage != 0) {
            mWindSet->SetState(gWindUnit);
        }
        mBaseLayout->Calc();
        mSetLayout->Calc();
        break;
    }
}

void WeatherSetting::UpdateCityName() {
    City* city = FindCity(gCurrentCityId);
    mCity = city;

    if (city != NULL) {
        wcscpy(mCityName, city->mInfo->mName);
        gTextWriter.SetFont(*gSysFont);
        gTextWriter.SetCharSpace(0.0f);
        gTextWriter.SetScale(1.0f);

        f32 width = gTextWriter.CalcStringWidth(mCityName);
        if (width > mTextMaxWidth) {
            mTextScaleX = mTextMaxWidth / width;
            mTextCharSpace = 0.0f;
        } else {
            mTextScaleX = 1.0f;
            mTextCharSpace = 0.0f;
        }
    } else {
        OSPanic("d_weather_setting.cpp", 439,
                "天気住所がエラーです!!\n");
    }
}
