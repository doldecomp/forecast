#ifndef CHANNEL_WEATHER_BASE_H
#define CHANNEL_WEATHER_BASE_H
#include <types.h>
#include <nw4r/ut.h>
#include <revolution/MTX.h>
#include <channel/Vec2F.h>

class ButtonGroup;
class LayoutButton;

// A text field or icon slot placed by a layout pane (size 0x24)
struct TextBox {
    LayoutButton* mPane;          // at 0x0
    f32 mX;                       // at 0x4
    f32 mY;                       // at 0x8
    f32 mWidth;                   // at 0xC
    f32 mHeight;                  // at 0x10
    f32 mScaleX;                  // at 0x14
    f32 mScaleY;                  // at 0x18
    nw4r::ut::Color mColor;       // at 0x1C
    nw4r::ut::Color mShadowColor; // at 0x20
};

// d_weather_base.cpp: base of the forecast panels that slide around on screen (size 0xA0)
// Weekday name textures, Sunday first: the Japanese set, then one row per language
extern const u32 gWeekdayTexturesJP[7];
extern const u32 gWeekdayTextures[7][7];

class WeatherBase {
public:
    WeatherBase(const Vec2& pos, const Vec2& size, s32 type);
    virtual ~WeatherBase() {}
    virtual void Reset() = 0;
    virtual void Draw() = 0;
    virtual void SetPosition(const Vec2& pos, const f32& scale, const bool& visible, BOOL checkHover);
    virtual BOOL IsEmpty() {
        return FALSE;
    }

    void DrawIcon(TextBox* box, u32 id, s32 alpha);
    void DrawIconLarge(TextBox* box, u32 id, s32 alpha);
    void DrawText(TextBox* box, const wchar_t* text, s32 alpha);

    ButtonGroup* mLayout;                    // at 0x4
    Vec2 mPos;                               // at 0x8
    Vec2 mSize;                              // at 0x10
    nw4r::ut::Rect mRect;                    // at 0x18
    nw4r::ut::TextWriterBase<wchar_t> mWriter; // at 0x28
    f32 mScale;                              // at 0x88
    s32 mType;                               // at 0x8C
    s32 unk90;                               // at 0x90
    s32 mAlpha;                              // at 0x94
    s32 unk98;                               // at 0x98
    u8 mCulled;                              // at 0x9C
    u8 mVisible;                             // at 0x9D
};

struct DayForecast;
struct WeatherType;

// A forecast's weather icon and its description
struct WeatherInfo {
    WeatherInfo();
    ~WeatherInfo();
    s32 Setup(void* base, WeatherType* type);

    WeatherType* mType;   // at 0x0
    const wchar_t* mText; // at 0x4
};

// d_weather_base_day.cpp: a single day's forecast panel (size 0x380)
class WeatherBaseDay : public WeatherBase {
public:
    WeatherBaseDay(const Vec2& pos, void* arc, const Vec2& size, s32 type);
    virtual ~WeatherBaseDay();
    virtual void SetPosition(const Vec2& pos, const f32& scale, const bool& visible, BOOL checkHover);

    void SetupJP(void* arc);
    void Setup(void* arc);
    void DrawTextFit(TextBox* box, const wchar_t* text, s32 alpha);
    void DrawTempC(s32 temp, s32 alpha);
    void DrawTemp(s32 temp, s32 alpha);
    void DrawDayJP(DayForecast* day, WeatherInfo* weather, s32 alpha, f32 scale);
    void DrawDay(DayForecast* day, WeatherInfo* weather, s32 alpha, f32 scale);
    void FormatWind(u8 dir, u8 speed);
    void FormatWindDefault(u8 dir, u8 speed);
    void UpdateHover(BOOL enable);
    Vec2F GetIconPos();

    void PrintBox(TextBox* box, const wchar_t* text, s32 alpha);

    ButtonGroup* mNoServiceLayout;  // at 0xA0, life_no_service.brlyt
    LayoutButton* mNoServiceText;   // at 0xA4
    TextBox* mIconBox;              // at 0xA8
    TextBox mBoxes[20];             // at 0xAC
    s32 mStartHour;                 // at 0x37C
};

struct CityForecast;

// d_weather_a_other.cpp: the lifestyle index panel (UV, laundry, pollen) (size 0x194)
class WeatherOther : public WeatherBase {
public:
    typedef BOOL (WeatherOther::*HasDataFunc)(CityForecast* forecast);

    WeatherOther(const Vec2& pos, void* arc, const Vec2& size, s32 type);
    virtual ~WeatherOther();
    virtual void Reset();
    virtual void Draw();
    virtual void SetPosition(const Vec2& pos, const f32& scale, const bool& visible, BOOL checkHover);
    virtual BOOL IsEmpty();

    void SetupJP(void* arc);
    void Setup(void* arc);
    BOOL HasDataJP(CityForecast* forecast);
    BOOL HasData(CityForecast* forecast);
    void DrawJP(CityForecast* forecast, const s32& alpha);

    ButtonGroup* mNoServiceLayout; // at 0xA0, life_no_service.brlyt
    LayoutButton* mNoServiceText;  // at 0xA4
    LayoutButton* mIconPane;       // at 0xA8
    TextBox mBoxes[6];             // at 0xAC
    HasDataFunc mHasData;          // at 0x184
    s32 mDay;                      // at 0x190, 0 = today, 1 = tomorrow
};

// d_weather_a_today.cpp: today's forecast panel (size 0x38C)
class WeatherToday : public WeatherBaseDay {
public:
    typedef void (WeatherToday::*DrawFunc)(const s32& alpha);

    WeatherToday(const Vec2& pos, void* arc, const Vec2& size, s32 type);
    virtual ~WeatherToday();
    virtual void Reset();
    virtual void Draw();
    virtual void SetPosition(const Vec2& pos, const f32& scale, const bool& visible, BOOL checkHover);
    virtual BOOL IsEmpty();

    void DrawForecast(const s32& alpha);
    void DrawSummary(const s32& alpha);

    DrawFunc mDrawFunc; // at 0x380
};

// d_weather_a_tomorrow.cpp: tomorrow's forecast panel (size 0x38C)
class WeatherTomorrow : public WeatherBaseDay {
public:
    typedef void (WeatherTomorrow::*DrawFunc)(const s32& alpha);

    WeatherTomorrow(const Vec2& pos, void* arc, const Vec2& size, s32 type);
    virtual ~WeatherTomorrow();
    virtual void Reset();
    virtual void Draw();
    virtual void SetPosition(const Vec2& pos, const f32& scale, const bool& visible, BOOL checkHover);
    virtual BOOL IsEmpty();

    void DrawForecast(const s32& alpha);
    void DrawSummary(const s32& alpha);

    DrawFunc mDrawFunc; // at 0x380
};

// d_weather_a_week.cpp: the weekly forecast panel (size 0x730)
class WeatherWeek : public WeatherBase {
public:
    typedef BOOL (WeatherWeek::*HasDataFunc)(CityForecast* forecast);
    typedef void (WeatherWeek::*DrawFunc)(const s32& alpha);

    WeatherWeek(const Vec2& pos, void* arc, const Vec2& size, s32 type);
    virtual ~WeatherWeek();
    virtual void Reset();
    virtual void Draw();
    virtual void SetPosition(const Vec2& pos, const f32& scale, const bool& visible, BOOL checkHover);

    void SetupJP(void* arc);
    void Setup(void* arc);
    BOOL HasDataJP(CityForecast* forecast);
    BOOL HasData(CityForecast* forecast);
    void DrawWeekJP(const s32& alpha);
    void DrawWeek(const s32& alpha);

    ButtonGroup* mNoServiceLayout; // at 0xA0, life_no_service.brlyt
    LayoutButton* mDayPanes[7];    // at 0xA4
    LayoutButton* mNoServiceText;  // at 0xC0
    TextBox mBoxes[45];            // at 0xC4
    HasDataFunc mHasData;          // at 0x718
    DrawFunc mDrawFunc;            // at 0x724
};

// d_weather.cpp: the current weather panel, from short.bin
class WeatherNow : public WeatherBaseDay {
public:
    WeatherNow(const Vec2& pos, void* arc, const Vec2& size, s32 type);
    virtual ~WeatherNow();
    virtual void Reset();
    virtual void Draw();
    virtual void SetPosition(const Vec2& pos, const f32& scale, const bool& visible, BOOL checkHover);
    virtual BOOL IsEmpty();

    void DrawNow(const s32& alpha);
};

#endif
