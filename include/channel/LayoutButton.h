#ifndef CHANNEL_LAYOUT_BUTTON_H
#define CHANNEL_LAYOUT_BUTTON_H
#include <types.h>
#include <cstring>
#include <revolution/GX.h>
#include <nw4r/lyt/lyt_pane.h>
#include <nw4r/math/math_types.h>
#include <nw4r/ut/ut_Color.h>
#include <nw4r/ut/ut_Rect.h>
#include <channel/Vec2F.h>

namespace nw4r {
namespace lyt {
class DrawInfo;
class Layout;
class ArcResourceAccessor;
}
namespace math {
struct MTX34;
}
}

class TextTagProcessor;
struct ButtonColors;

// A clickable layout pane
class LayoutButton {
public:
    LayoutButton(nw4r::lyt::Pane* pane, nw4r::lyt::DrawInfo* drawInfo, ButtonColors* colors, TextTagProcessor* tagProcessor);
    ~LayoutButton();

    void Reset();
    void Calc();
    void Update();
    void Draw();
    BOOL Contains(f32 x, f32 y);
    void SetPaneAlpha(s32 alpha);
    void SetState(s32 state);
    void SetText(const wchar_t* text);
    Vec2F GetCenter();
    void SetPaneColor(const char* name, const nw4r::ut::Color& color, int which);
    void Hide();
    void SetChildVisible(const char* name, bool visible);
    void SetChildAlpha(const char* name, s32 alpha);
    void SetChildAlphaRecursive(nw4r::lyt::Pane* pane, s32 alpha);
    nw4r::lyt::Pane* FindPane(const char* name);
    void ShowLanguagePane(const char* prefix);

    void SetParams(s32 brightness, s32 highlightFrame, s32 highlightFrames) {
        mBrightness = brightness;
        mHighlightFrame = highlightFrame;
        mHighlightFrames = highlightFrames;
    }

    void SetSlideOffset(f32 offset) {
        mSlideOffset = offset * (mPane->GetTranslate().y > 0.0f ? 1 : -1);
    }

    void SetLinked(LayoutButton* button) {
        mLinked = button;
        mLinked2 = NULL;
    }

    void SetAlpha(s32 alpha) {
        mAlpha = alpha;
    }

    void Hover();
    void Release();
    void Press(u8 pressing);

    BOOL IsName(const char* name) const {
        return std::strcmp(mPane->GetName(), name) == 0;
    }

    void SetCalcCallback(void (*callback)(void* arg), void* arg) {
        mCalcCallback = callback;
        mCallbackArg = arg;
    }

    // Locked buttons ignore the pointer
    void Lock() {
        mLocked = TRUE;
        Release();
    }

    void Unlock() {
        mLocked = FALSE;
    }

    BOOL IsInactive() const {
        return mDisabled || mLocked || mHidden;
    }

    s32 mUnused;                    // at 0x0, cleared by the constructor and never read
    ButtonColors* mColors;          // at 0x4, indexed by mType
    nw4r::lyt::Pane* mPane;         // at 0x8
    nw4r::lyt::DrawInfo* mDrawInfo; // at 0xC
    nw4r::lyt::Pane* mBasePane;     // at 0x10, "<name>B", or mPane itself
    nw4r::lyt::Pane* mIconPane;     // at 0x14, "<name>I"
    nw4r::lyt::Pane* mTextPane;     // at 0x18, this language's child of "<name>T": a child per state
    nw4r::lyt::Pane* mReleasedPane; // at 0x1C, "<name>F0", shown while the button is not pressed
    nw4r::lyt::Pane* mPressedPane;  // at 0x20, "<name>F1", shown while it is
    LayoutButton* mLinked;          // at 0x24, hovered along with this button (e.g. its "set" button)
    LayoutButton* mLinked2;         // at 0x28, a second one
    void* mCallbackArg;             // at 0x2C
    void (*mColorCallback)(nw4r::lyt::Pane* pane, const GXColor* color); // at 0x30
    void (*mCalcCallback)(void* arg); // at 0x34
    GXColor mTextColor;             // at 0x38
    nw4r::ut::Rect mRect;           // at 0x3C, the hit area: "<name>R" (or "<name>B") in layout coordinates
    nw4r::math::VEC3 mOrigTrans;    // at 0x4C
    nw4r::math::VEC3 mTextPos;      // at 0x58
    Vec2 mTextSize;                 // at 0x64
    s32 mType;                      // at 0x6C, set by a "C<digit>" tag
    s32 mAlpha;                     // at 0x70
    s32 mBrightness;                // at 0x74, 255 = normal, lower fades toward white
    s32 mHoverFrame;                // at 0x78
    s32 mPressTimer;                // at 0x7C, frames left of the press animation
    s32 mState;                     // at 0x80, index of the mTextPane child shown (< 0: none)
    s32 mNextState;                 // at 0x84, becomes mState when the press animation ends
    s32 mHighlightFrame;            // at 0x88
    s32 mHighlightFrames;           // at 0x8C
    u8 mDisabled;                   // at 0x90
    u8 mHeld;                       // at 0x91
    u8 mPressed;                    // at 0x92
    u8 mToggle;                     // at 0x93
    u8 mHidden;                     // at 0x94
    u8 mHovered;                    // at 0x95, set by Hover, consumed by Calc
    u8 mPressing;                   // at 0x96
    u8 mSelected;                   // at 0x97, shows the selected colors (never set by the channel)
    u8 mLocked;                     // at 0x98
    u8 mSkipAlphaWhenDisabled;      // at 0x99, leave the TEV alpha alone while disabled
    f32 mSlideOffset;               // at 0x9C
};

// Material TEV colors 0 and 1 of the pane's own material
void GetTevColors(nw4r::lyt::Pane* pane, GXColor* color0, GXColor* color1);
void SetTevColors(nw4r::lyt::Pane* pane, const GXColor* color0, const GXColor* color1);
s16 GetTevColor1Alpha(nw4r::lyt::Pane* pane);

// Linear interpolation from a to b as t goes from 0 to n
s32 Lerp(s32 a, s32 b, s32 t, s32 n);

#define BUTTON_GROUP_MAX_BUTTONS 256

// A layout whose top-level panes are all buttons, with slide and fade animations
class ButtonGroup {
public:
    ButtonGroup(void* arc, const char* layoutName, ButtonColors* colors, bool influencedAlpha);
    ~ButtonGroup();

    void Reset();
    inline void ResetButtons();
    void ReleaseAll();
    void Calc();
    void Draw();
    LayoutButton* HitTest(f32 x, f32 y);
    LayoutButton* FindButton(const char* name);
    void SlideOut(s32 frames);
    void SlideIn(s32 frames);
    void FadeIn(s32 frames);
    void SetButtonParams(s32 brightness, s32 highlightFrame, s32 highlightFrames);
    void SetPaneAlpha(s32 alpha);
    void SetViewMtx(const nw4r::math::MTX34& mtx);
    void SetSlideOffset(f32 offset);

    inline void UpdateAll();
    inline void ApplySlide(f32 offset);
    inline void ApplyAlpha(s32 alpha);

    nw4r::lyt::ArcResourceAccessor* mResAccessor;        // at 0x0
    nw4r::lyt::Layout* mLayout;                          // at 0x4
    nw4r::lyt::DrawInfo* mDrawInfo;                      // at 0x8
    void* mUnused;                                       // at 0xC, never used
    TextTagProcessor* mTagProcessor;                     // at 0x10
    LayoutButton* mButtons[BUTTON_GROUP_MAX_BUTTONS];    // at 0x14
    s32 mNumButtons;                                     // at 0x414
    u8 mSlidingOut;                                      // at 0x418
    s32 mSlideFrames;                                    // at 0x41C
    s32 mSlideFrame;                                     // at 0x420
    u8 mFadingOut;                                       // at 0x424
    s32 mFadeFrames;                                     // at 0x428
    s32 mFadeFrame;                                      // at 0x42C
    s32 mAlpha;                                          // at 0x430
};

#endif
