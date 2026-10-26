// The retail TU only sees a declaration for ButtonColors' destructor and for
// the nw4r RTTI accessor, so neither is emitted into this object.
#define NW4R_HEADER_DECLARATIONS_ONLY
#include <channel/LayoutButton.h>
#include <channel/WeatherScene.h>
#include <channel/MtxUtil.h>
#include <channel/System.h>
#include <channel/TextTagProcessor.h>
#include <nw4r/lyt.h>
#include <nw4r/lyt/lyt_drawInfo.h>
#include <nw4r/lyt/lyt_material.h>
#include <nw4r/lyt/lyt_pane.h>
#include <nw4r/lyt/lyt_textBox.h>
#include <nw4r/ut/ut_WideTagProcessor.h>
#include <cstdio>
#include <cstring>

void SetTextBoxColors(nw4r::lyt::TextBox* textBox, nw4r::ut::Color top, nw4r::ut::Color bottom);

// Shared with LayoutObj.cpp: Update calls these out of line, and the linker keeps
// LayoutObj's copies.
inline const char* GetPaneUserData(nw4r::lyt::Pane* pane) {
    return pane->GetUserData();
}

inline void SetPaneVisible(nw4r::lyt::Pane* pane, bool visible) {
    pane->SetVisible(visible);
}

// Sets the tag processor of the pane if it is a text box
static inline void SetTextBoxTagProcessor(nw4r::lyt::Pane* pane, nw4r::ut::WideTagProcessor* processor) {
    nw4r::lyt::TextBox* textBox = nw4r::ut::DynamicCast<nw4r::lyt::TextBox*>(pane);
    if (textBox != NULL) {
        textBox->SetTagProcessor(processor);
    }
}

// Sets the tag processor of every text box in the pane's tree. As in LayoutObj.cpp, the
// original expands the recursion three levels deep before it calls itself, both here and
// where the constructor uses it (SetPaneTagProcessorExpanded).
static void SetPaneTagProcessor(nw4r::lyt::Pane* pane, nw4r::ut::WideTagProcessor* processor) {
    // Declared up front: their default constructor and assignment then get their
    // out-of-line copies placed right after this function, as in the original.
    nw4r::lyt::PaneList::Iterator it1, it2, it3;
    SetTextBoxTagProcessor(pane, processor);

    nw4r::lyt::PaneList& list1 = pane->GetChildList();
    for (it1 = list1.GetBeginIter(); it1 != list1.GetEndIter(); it1++) {
        nw4r::lyt::Pane* child1 = &*it1;
        SetTextBoxTagProcessor(child1, processor);

        nw4r::lyt::PaneList& list2 = child1->GetChildList();
        for (it2 = list2.GetBeginIter(); it2 != list2.GetEndIter(); it2++) {
            nw4r::lyt::Pane* child2 = &*it2;
            SetTextBoxTagProcessor(child2, processor);

            nw4r::lyt::PaneList& list3 = child2->GetChildList();
            for (it3 = list3.GetBeginIter(); it3 != list3.GetEndIter(); it3++) {
                SetPaneTagProcessor(&*it3, processor);
            }
        }
    }
}

// SetPaneTagProcessor as the constructor inlines it
static inline void SetPaneTagProcessorExpanded(nw4r::lyt::Pane* pane, nw4r::ut::WideTagProcessor* processor) {
    SetTextBoxTagProcessor(pane, processor);

    nw4r::lyt::PaneList& list1 = pane->GetChildList();
    for (nw4r::lyt::PaneList::Iterator it1 = list1.GetBeginIter(); it1 != list1.GetEndIter(); ++it1) {
        nw4r::lyt::Pane* child1 = &*it1;
        SetTextBoxTagProcessor(child1, processor);

        nw4r::lyt::PaneList& list2 = child1->GetChildList();
        for (nw4r::lyt::PaneList::Iterator it2 = list2.GetBeginIter(); it2 != list2.GetEndIter(); ++it2) {
            nw4r::lyt::Pane* child2 = &*it2;
            SetTextBoxTagProcessor(child2, processor);

            nw4r::lyt::PaneList& list3 = child2->GetChildList();
            for (nw4r::lyt::PaneList::Iterator it3 = list3.GetBeginIter(); it3 != list3.GetEndIter(); ++it3) {
                SetPaneTagProcessor(&*it3, processor);
            }
        }
    }
}

// Sets the text color of every text box in the pane's tree, except under panes tagged "F"
static void SetPaneTextColor(nw4r::lyt::Pane* pane, const GXColor* color) {
    char tag = pane->GetUserData()[0];
    if (tag == 'F' || tag == 'f') {
        return;
    }

    nw4r::lyt::TextBox* textBox = nw4r::ut::DynamicCast<nw4r::lyt::TextBox*>(pane);
    if (textBox != NULL) {
        nw4r::ut::Color c = *color;
        SetTextBoxColors(textBox, c, c);
    }

    nw4r::lyt::PaneList& list = pane->GetChildList();
    for (nw4r::lyt::PaneList::Iterator it = list.GetBeginIter(); it != list.GetEndIter(); ++it) {
        SetPaneTextColor(&*it, color);
    }
}

// Defined after SetPaneTextColor so that call stays out of line, as in LayoutObj.cpp (the
// linker keeps WeatherAround's copy). Its Color assignments place the out-of-line copy of
// Color::operator= that Update calls.
inline void SetTextBoxColors(nw4r::lyt::TextBox* textBox, nw4r::ut::Color top, nw4r::ut::Color bottom) {
    // Writes the protected TextBox::mTextColors directly, like LayoutObj.cpp and
    // WeatherAround.cpp: the original does not call TextBox::SetTextColor, which is a
    // function of its own in this nw4r version.
    nw4r::ut::Color* colors = reinterpret_cast<nw4r::ut::Color*>(reinterpret_cast<u8*>(textBox) + 0xD8);
    colors[0] = top;
    colors[1] = bottom;
}

// Sets TEV color 0 or 1 (RGB) of every material in the pane's tree, except under panes
// tagged "F"
static void SetPaneTevColor(nw4r::lyt::Pane* pane, const GXColor* color, s32 which) {
    char tag = pane->GetUserData()[0];
    if (tag == 'F' || tag == 'f') {
        return;
    }

    nw4r::lyt::Material* material = pane->FindMaterialByName(pane->GetName(), true);
    if (material != NULL) {
        switch (which) {
        case 0:
            material->SetColorElement(4, color->r);
            material->SetColorElement(5, color->g);
            material->SetColorElement(6, color->b);
            break;
        case 1:
            material->SetColorElement(8, color->r);
            material->SetColorElement(9, color->g);
            material->SetColorElement(10, color->b);
            break;
        }
    }

    nw4r::lyt::PaneList& list = pane->GetChildList();
    for (nw4r::lyt::PaneList::Iterator it = list.GetBeginIter(); it != list.GetEndIter(); ++it) {
        SetPaneTevColor(&*it, color, which);
    }
}

void SetTevColors(nw4r::lyt::Pane* pane, const GXColor* color0, const GXColor* color1) {
    nw4r::lyt::Material* material = pane->FindMaterialByName(pane->GetName(), true);
    if (material != NULL) {
        if (color0 != NULL) {
            GXColorS10 c;
            c.r = color0->r;
            c.g = color0->g;
            c.b = color0->b;
            c.a = color0->a;
            material->SetTevColor(0, c);
        }
        if (color1 != NULL) {
            GXColorS10 c;
            c.r = color1->r;
            c.g = color1->g;
            c.b = color1->b;
            c.a = color1->a;
            material->SetTevColor(1, c);
        }
    }
}

void GetTevColors(nw4r::lyt::Pane* pane, GXColor* color0, GXColor* color1) {
    nw4r::lyt::Material* material = pane->FindMaterialByName(pane->GetName(), true);
    if (material != NULL) {
        if (color0 != NULL) {
            GXColorS10 c = material->GetTevColor(0);
            color0->r = c.r;
            color0->g = c.g;
            color0->b = c.b;
            color0->a = c.a;
        }
        if (color1 != NULL) {
            GXColorS10 c = material->GetTevColor(1);
            color1->r = c.r;
            color1->g = c.g;
            color1->b = c.b;
            color1->a = c.a;
        }
    } else {
        if (color0 != NULL) {
            color0->a = 0xFF;
            color0->b = 0xFF;
            color0->g = 0xFF;
            color0->r = 0xFF;
        }
        if (color1 != NULL) {
            color1->a = 0xFF;
            color1->b = 0xFF;
            color1->g = 0xFF;
            color1->r = 0xFF;
        }
    }
}

// Sets the alpha of TEV color 1 of every material in the pane's tree. The original expands
// the recursion three levels deep, like SetPaneTagProcessor.
static void SetPaneTevAlpha(nw4r::lyt::Pane* pane, s32 alpha) {
    nw4r::lyt::Material* material = pane->FindMaterialByName(pane->GetName(), true);
    if (material != NULL) {
        material->SetColorElement(11, (u8)alpha);
    }

    nw4r::lyt::PaneList& list1 = pane->GetChildList();
    for (nw4r::lyt::PaneList::Iterator it1 = list1.GetBeginIter(); it1 != list1.GetEndIter(); ++it1) {
        nw4r::lyt::Pane* child1 = &*it1;
        material = child1->FindMaterialByName(child1->GetName(), true);
        if (material != NULL) {
            material->SetColorElement(11, (u8)alpha);
        }

        nw4r::lyt::PaneList& list2 = child1->GetChildList();
        for (nw4r::lyt::PaneList::Iterator it2 = list2.GetBeginIter(); it2 != list2.GetEndIter(); ++it2) {
            nw4r::lyt::Pane* child2 = &*it2;
            material = child2->FindMaterialByName(child2->GetName(), true);
            if (material != NULL) {
                material->SetColorElement(11, (u8)alpha);
            }

            nw4r::lyt::PaneList& list3 = child2->GetChildList();
            for (nw4r::lyt::PaneList::Iterator it3 = list3.GetBeginIter(); it3 != list3.GetEndIter(); ++it3) {
                SetPaneTevAlpha(&*it3, alpha);
            }
        }
    }
}

s16 GetTevColor1Alpha(nw4r::lyt::Pane* pane) {
    nw4r::lyt::Material* material = pane->FindMaterialByName(pane->GetName(), true);
    if (material != NULL) {
        return material->GetColorElement(11);
    }
    return 0;
}

// The original returns nw4r::ut::Color through a hidden pointer.
nw4r::ut::Color LerpColor(const nw4r::ut::Color& a, const nw4r::ut::Color& b, s32 num, s32 den) {
    nw4r::ut::Color c;
    c.r = (a.r * (den - num) + b.r * num) / den;
    c.g = (a.g * (den - num) + b.g * num) / den;
    c.b = (a.b * (den - num) + b.b * num) / den;
    return c;
}

// The colors of buttons given none, by type. Each entry lists the normal colors
// (base 0 and 1, icon 0 and 1, text), then the active base and icon colors. The rest stay
// zero in .bss, and __sinit stores nothing for them.
ButtonColors sDefaultColors[4] = {
    {
        nw4r::ut::Color(0, 0, 0, 255),
        nw4r::ut::Color(255, 255, 255, 255),
        nw4r::ut::Color(0, 0, 0, 255),
        nw4r::ut::Color(216, 216, 216, 255),
        nw4r::ut::Color(0, 0, 0, 255),
        nw4r::ut::Color(255, 255, 255, 255),
        nw4r::ut::Color(193, 0, 0, 255),
        nw4r::ut::Color(255, 255, 255, 255),
        nw4r::ut::Color(0, 0, 0, 255),
    },
    {
        nw4r::ut::Color(0, 0, 0, 255),
        nw4r::ut::Color(255, 255, 255, 255),
        nw4r::ut::Color(0, 0, 0, 255),
        nw4r::ut::Color(255, 120, 0, 255),
        nw4r::ut::Color(0, 0, 0, 255),
        nw4r::ut::Color(255, 255, 255, 255),
        nw4r::ut::Color(193, 0, 0, 255),
        nw4r::ut::Color(255, 255, 255, 255),
        nw4r::ut::Color(0, 0, 0, 255),
    },
    {
        nw4r::ut::Color(0, 0, 0, 255),
        nw4r::ut::Color(255, 255, 255, 255),
        nw4r::ut::Color(0, 0, 0, 255),
        nw4r::ut::Color(248, 190, 0, 255),
        nw4r::ut::Color(0, 0, 0, 255),
        nw4r::ut::Color(255, 255, 255, 255),
        nw4r::ut::Color(193, 0, 0, 255),
        nw4r::ut::Color(255, 255, 255, 255),
        nw4r::ut::Color(0, 0, 0, 255),
    },
    {
        nw4r::ut::Color(0, 0, 0, 255),
        nw4r::ut::Color(255, 255, 255, 255),
        nw4r::ut::Color(0, 0, 0, 255),
        nw4r::ut::Color(89, 221, 255, 255),
        nw4r::ut::Color(0, 0, 0, 255),
        nw4r::ut::Color(255, 255, 255, 255),
        nw4r::ut::Color(193, 0, 0, 255),
        nw4r::ut::Color(255, 255, 255, 255),
        nw4r::ut::Color(0, 0, 0, 255),
    },
};

s32 Lerp(s32 a, s32 b, s32 t, s32 n) {
    return (a * (n - t) + b * t) / n;
}

LayoutButton::LayoutButton(nw4r::lyt::Pane* pane, nw4r::lyt::DrawInfo* drawInfo,
                           ButtonColors* colors, TextTagProcessor* tagProcessor) {
    char name[128];

    mDrawInfo = drawInfo;
    mPane = pane;
    mColors = colors;
    mBasePane = NULL;
    mIconPane = NULL;
    mTextPane = NULL;
    mReleasedPane = NULL;
    mPressedPane = NULL;
    mLinked = NULL;
    mLinked2 = NULL;
    mColorCallback = NULL;
    mCalcCallback = NULL;
    static_cast<nw4r::ut::Color&>(mTextColor) = 0xFFFFFFFF;
    mRect.left = 0.0f;
    mRect.top = 0.0f;
    mRect.right = 0.0f;
    mRect.bottom = 0.0f;
    mTextSize.x = 0.0f;
    mTextSize.y = 0.0f;
    mType = 0;
    mDisabled = 0;
    mHeld = 0;
    mPressed = 0;
    mToggle = 0;
    mHidden = 0;
    mLocked = 0;
    mSkipAlphaWhenDisabled = 1;

    sprintf(name, "%sB", mPane->GetName());
    mBasePane = mPane->FindPaneByName(name, true);
    if (mBasePane == NULL) {
        mBasePane = mPane;
    }

    sprintf(name, "%sR", mPane->GetName());
    nw4r::lyt::Pane* found = mPane->FindPaneByName(name, true);
    if (found == NULL) {
        found = mBasePane;
    }

    mOrigTrans = mPane->GetTranslate();
    mRect = found->GetPaneRect(*mDrawInfo);
    mRect.left += mOrigTrans.x;
    mRect.right += mOrigTrans.x;
    mRect.top += mOrigTrans.y;
    mRect.bottom += mOrigTrans.y;
    if (mBasePane != mPane) {
        nw4r::math::VEC3 trans = mBasePane->GetTranslate();
        mRect.left += trans.x;
        mRect.right += trans.x;
        mRect.top += trans.y;
        mRect.bottom += trans.y;
    }

    for (s32 i = 0; i < 8; i++) {
        switch (mPane->GetUserData()[i]) {
        case 'D':
        case 'd':
            mLocked = 1;
            break;
        case 'F':
        case 'f':
            mDisabled = 1;
            break;
        case 'C':
        case 'c':
            i++;
            if (i < 8) {
                s32 digit = mPane->GetUserData()[i] - '0';
                if (digit >= 0 && digit <= 9) {
                    mType = digit;
                }
            }
            break;
        }
    }

    sprintf(name, "%sI", mPane->GetName());
    found = mPane->FindPaneByName(name, true);
    if (found == NULL) {
        mIconPane = NULL;
    } else {
        sprintf(name, "%s%s", found->GetName(), GetLanguageCode());
        mIconPane = found->FindPaneByName(name, true);
        if (mIconPane == NULL) {
            mIconPane = found;
        } else {
            nw4r::lyt::PaneList& list = found->GetChildList();
            for (nw4r::lyt::PaneList::Iterator it = list.GetBeginIter(); it != list.GetEndIter(); ++it) {
                if (&*it != mIconPane) {
                    it->SetVisible(false);
                }
            }
        }
    }

    mTextPos = mPane->GetTranslate();
    mTextSize.x = mPane->GetSize().width;
    mTextSize.y = mPane->GetSize().height;

    sprintf(name, "%sT", mPane->GetName());
    found = mPane->FindPaneByName(name, true);
    if (found == NULL) {
        mTextPane = NULL;
    } else {
        mTextPos += found->GetTranslate();
        mTextSize.x = found->GetSize().width;
        mTextSize.y = found->GetSize().height;

        mTextPos.x *= gWidescreen ? 1.3684211f : 1.0f;
        mTextSize.x *= gWidescreen ? 1.3684211f : 1.0f;

        sprintf(name, "%s%s", found->GetName(), GetLanguageCode());
        mTextPane = mPane->FindPaneByName(name, true);
        if (mTextPane != NULL) {
            mTextPos += mTextPane->GetTranslate();
            mTextSize.x = mTextPane->GetSize().width;
            mTextSize.y = mTextPane->GetSize().height;
        }
        nw4r::lyt::PaneList& list = found->GetChildList();
        for (nw4r::lyt::PaneList::Iterator it = list.GetBeginIter(); it != list.GetEndIter(); ++it) {
            if (&*it != mTextPane) {
                it->SetVisible(false);
            }
        }
    }

    if (mDisabled && mTextPane != NULL) {
        SetPaneTagProcessorExpanded(mTextPane, tagProcessor);
    }

    sprintf(name, "%sF", mPane->GetName());
    found = mPane->FindPaneByName(name, true);
    if (found == NULL) {
        mReleasedPane = NULL;
        mPressedPane = NULL;
    } else {
        sprintf(name, "%sF0", mPane->GetName());
        mReleasedPane = found->FindPaneByName(name, true);
        sprintf(name, "%sF1", mPane->GetName());
        mPressedPane = found->FindPaneByName(name, true);
    }

    nw4r::lyt::TextBox* textBox = nw4r::ut::DynamicCast<nw4r::lyt::TextBox*>(mBasePane);
    if (textBox != NULL) {
        textBox->SetVisible(true);
        SetPaneTagProcessorExpanded(textBox, tagProcessor);
    }

    mUnused = 0;
    if (colors == NULL) {
        mColors = sDefaultColors;
    }

    mSlideOffset = 0.0f;
    mAlpha = 0xFF;
    mBrightness = 0xFF;
    mHovered = 0;
    mHoverFrame = 0;
    mPressing = 0;
    mPressTimer = 0;
    mState = 0;
    mNextState = 0;
    mSelected = 0;
    mPressed = 0;
}

LayoutButton::~LayoutButton() {}

void LayoutButton::Reset() {
    mSlideOffset = 0.0f;
    mAlpha = 0xFF;
    mBrightness = 0xFF;
    mHovered = 0;
    mHoverFrame = 0;
    mPressing = 0;
    mPressTimer = 0;
    mState = 0;
    mNextState = 0;
    mSelected = 0;
    mPressed = 0;
}

void LayoutButton::Calc() {
    if (mHidden) {
        return;
    }

    if (mHovered) {
        if (mHoverFrame < 12) {
            mHoverFrame++;
        }
        mHovered = 0;
    } else if (mHoverFrame > 0) {
        mHoverFrame--;
    }

    if (mPressing) {
        if (mPressTimer > 0) {
            if (--mPressTimer == 0) {
                mState = mNextState;
            }
        }
        if (mPressTimer == 0) {
            mPressing = 0;
        }
        if (mPressTimer == 9) {
            mHoverFrame = 12;
        }
    } else if (mPressTimer > 1) {
        mPressTimer--;
    }
}

// The original calls every inline helper here out of line (list iterators, Color copies,
// SinRad, VEC3), like LayoutObjItem::Draw. The linker keeps the weak copies of the helpers
// that no earlier unit emits; those are the ones placed in this unit.
#pragma dont_inline on
void LayoutButton::Update() {
    nw4r::lyt::Pane* textPane = NULL;
    if (mHidden) {
        return;
    }

    if (mSkipAlphaWhenDisabled) {
        if (!mDisabled) {
            SetPaneTevAlpha(mPane, mAlpha);
        }
    } else {
        SetPaneTevAlpha(mPane, mAlpha);
    }

    if (mTextPane != NULL) {
        nw4r::lyt::PaneList& list = mTextPane->GetChildList();
        nw4r::lyt::PaneList::Iterator it;
        if (mState >= 0) {
            textPane = mTextPane;
            s32 i = 0;
            for (it = list.GetBeginIter(); it != list.GetEndIter(); i++, it++) {
                if (mState == i) {
                    textPane = &*it;
                    break;
                }
            }
            SetPaneVisible(textPane, true);
        }
        for (it = list.GetBeginIter(); it != list.GetEndIter(); it++) {
            if (&*it != textPane) {
                SetPaneVisible(&*it, false);
            }
        }
    }

    if (!mDisabled) {
        nw4r::ut::Color color0, color1;
        s32 alphaA = mAlpha * mBrightness / 255;
        s32 limit = mBrightness + 128 > 255 ? 255 : mBrightness + 128;
        s32 alphaB = mAlpha * limit / 255;
        s32 alphaC = alphaB;

        if (mSelected) {
            color0 = mColors[mType].mSelected.mBase0;
            color1 = mColors[mType].mSelected.mBase1;
        } else if (mPressed && mPressTimer > 0) {
            color0 = mColors[mType].mActive.mBase0;
            color1 = mColors[mType].mActive.mBase1;
        } else if (mHoverFrame > 0) {
            color0 = LerpColor(mColors[mType].mNormal.mBase0, mColors[mType].mActive.mBase0, mHoverFrame, 12);
            color1 = LerpColor(mColors[mType].mNormal.mBase1, mColors[mType].mActive.mBase1, mHoverFrame, 12);
        } else if (mPressTimer > 0) {
            color0 = mColors[mType].mActive.mBase0;
            color1 = mColors[mType].mActive.mBase1;
        } else {
            color0 = mColors[mType].mNormal.mBase0;
            color1 = mColors[mType].mNormal.mBase1;
        }

        if (mBrightness < 255) {
            GXColor white0 = {0xFF, 0xFF, 0xFF, 0x00};
            color0 = LerpColor(color0, white0, 255 - mBrightness, 255);
            GXColor white1 = {0xFF, 0xFF, 0xFF, 0x00};
            color1 = LerpColor(color1, white1, 255 - mBrightness, 255);
        }

        SetPaneTevColor(mBasePane, &color0, 0);
        SetPaneTevColor(mBasePane, &color1, 1);
        SetPaneTevAlpha(mBasePane, alphaA);

        if (mIconPane != NULL && GetPaneUserData(mIconPane)[0] != 'F') {
            if (mSelected) {
                color0 = mColors[mType].mSelected.mIcon0;
                color1 = mColors[mType].mSelected.mIcon1;
            } else if (mLocked) {
                alphaB = 0;
            } else if (mPressed && mPressTimer > 0) {
                color0 = mColors[mType].mActive.mIcon0;
                color1 = mColors[mType].mActive.mIcon1;
            } else if (mHoverFrame > 0) {
                color0 = LerpColor(mColors[mType].mNormal.mIcon0, mColors[mType].mActive.mIcon0, mHoverFrame, 12);
                color1 = LerpColor(mColors[mType].mNormal.mIcon1, mColors[mType].mActive.mIcon1, mHoverFrame, 12);
            } else if (mPressTimer > 0) {
                color0 = mColors[mType].mActive.mIcon0;
                color1 = mColors[mType].mActive.mIcon1;
            } else if (mHighlightFrame > 0) {
                color0 = LerpColor(mColors[mType].mNormal.mIcon0, mColors[mType].mActive.mIcon0, mHighlightFrame, mHighlightFrames);
                color1 = LerpColor(mColors[mType].mNormal.mIcon1, mColors[mType].mActive.mIcon1, mHighlightFrame, mHighlightFrames);
            } else {
                color0 = mColors[mType].mNormal.mIcon0;
                color1 = mColors[mType].mNormal.mIcon1;
            }

            SetPaneTevColor(mIconPane, &color0, 0);
            SetPaneTevColor(mIconPane, &color1, 1);
            SetPaneTevAlpha(mIconPane, alphaB);
        }

        if (textPane != NULL) {
            if (mSelected) {
                color0 = mColors[mType].mSelected.mText;
            } else if (mLocked) {
                alphaC = 0;
            } else if (mPressed && mPressTimer > 0) {
                color0 = mColors[mType].mActive.mText;
            } else if (mHoverFrame > 0) {
                color0 = LerpColor(mColors[mType].mNormal.mText, mColors[mType].mActive.mText, mHoverFrame, 12);
            } else if (mPressTimer > 0) {
                color0 = mColors[mType].mActive.mText;
            } else if (mHighlightFrame > 0) {
                color0 = LerpColor(mColors[mType].mNormal.mText, mColors[mType].mActive.mText, mHighlightFrame, mHighlightFrames);
            } else {
                color0 = mColors[mType].mNormal.mText;
            }

            color0.a = alphaC;
            static_cast<nw4r::ut::Color&>(mTextColor) = color0;
            SetPaneTextColor(textPane, &color0);
            if (mColorCallback != NULL) {
                mColorCallback(textPane, &color0);
            }
        }

        if (mReleasedPane != NULL && mPressedPane != NULL) {
            if (mPressTimer > 0) {
                SetPaneVisible(mReleasedPane, false);
                SetPaneVisible(mPressedPane, true);
            } else {
                SetPaneVisible(mReleasedPane, true);
                SetPaneVisible(mPressedPane, false);
            }
        }
    }

    f32 shake;
    if (mPressTimer > 0 && mReleasedPane == NULL) {
        if (mPressed) {
            shake = 12.0f;
            if (mLinked != NULL) {
                mLinked->Hover();
            }
            if (mLinked2 != NULL) {
                mLinked2->Hover();
            }
        } else {
            s32 n = mPressTimer > 14 ? 14 : mPressTimer;
            f32 t = 1.5708f * (f32)(14 - n) / 14.0f;
            shake = 18.0f * (1.0f - nw4r::math::SinRad(t));
        }
    } else {
        shake = 0.0f;
    }

    mPane->SetTranslate(mOrigTrans + nw4r::math::VEC3(0.0f, mSlideOffset - shake, 0.0f));

    if (mCalcCallback != NULL) {
        mCalcCallback(mCallbackArg);
    }
}
#pragma dont_inline reset

void LayoutButton::Draw() {
    if (mHidden) {
        return;
    }

    mPane->Draw(*mDrawInfo);
}

BOOL LayoutButton::Contains(f32 x, f32 y) {
    if (mHidden || mToggle) {
        return FALSE;
    }

    y -= mSlideOffset;
    return x >= mRect.left && x < mRect.right && y < mRect.top && y >= mRect.bottom;
}

// Hovering over a button also highlights the buttons linked to it. With -ipa file, MWCC
// expands this recursion two levels deep here and three levels deep in Release.
inline void LayoutButton::Hover() {
    if (mDisabled) {
        return;
    }
    if (mLocked) {
        return;
    }
    if (mHidden) {
        return;
    }

    mHovered = 1;
    if (mLinked != NULL) {
        mLinked->Hover();
    }
    if (mLinked2 != NULL) {
        mLinked2->Hover();
    }
}

void LayoutButton::Press(u8 pressing) {
    if (mDisabled) {
        return;
    }
    if (mLocked) {
        return;
    }
    if (mHidden) {
        return;
    }

    mPressing = pressing;
    mPressTimer = 18;
}

void LayoutButton::Release() {
    if (mDisabled) {
        return;
    }

    mPressing = 1;
    if (mLinked != NULL) {
        mLinked->Hover();
    }
    if (mLinked2 != NULL) {
        mLinked2->Hover();
    }
}

void LayoutButton::SetState(s32 state) {
    mState = state;
    mNextState = state;
}

void LayoutButton::SetText(const wchar_t* text) {
    nw4r::lyt::TextBox* textBox = nw4r::ut::DynamicCast<nw4r::lyt::TextBox*>(mBasePane);
    if (textBox == NULL) {
        textBox = nw4r::ut::DynamicCast<nw4r::lyt::TextBox*>(mTextPane);
    }
    if (textBox != NULL) {
        s32 len = 0;
        while (text[len] != 0) {
            len++;
        }
        textBox->AllocStringBuffer((u16)(len + 1));
        textBox->SetString(text, 0);
    }
}

void LayoutButton::Hide() {
    mHovered = 0;
    mHoverFrame = 0;
    mPressing = 0;
    mPressTimer = 0;
    mHidden = TRUE;
    mPane->SetTranslate(mOrigTrans);
}

// The direct child of the pane with this name
static inline nw4r::lyt::Pane* FindChildPane(nw4r::lyt::Pane* pane, const char* name) {
    if (pane != NULL) {
        nw4r::lyt::PaneList& list = pane->GetChildList();
        for (nw4r::lyt::PaneList::Iterator it = list.GetBeginIter(); it != list.GetEndIter(); ++it) {
            if (strcmp(it->GetName(), name) == 0) {
                return &*it;
            }
        }
    }
    return NULL;
}

void LayoutButton::SetChildVisible(const char* name, bool visible) {
    nw4r::lyt::Pane* pane = mIconPane;
    if (pane == NULL) {
        return;
    }
    if (strcmp(pane->GetName(), name) == 0) {
        pane->SetVisible(visible);
        return;
    }
    if (pane != NULL) {
        pane = FindChildPane(pane, name);
        if (pane != NULL) {
            pane->SetVisible(visible);
        }
    }
}

Vec2F LayoutButton::GetCenter() {
    Vec trans;
    trans.y = (mRect.top + mRect.bottom) / 2.0f;
    trans.x = (mRect.right + mRect.left) / 2.0f;
    trans.z = 0.0f;
    PSMTXTrans(gModelMtx.m, trans.x, trans.y, trans.z);
    PSMTXConcat(mDrawInfo->GetViewMtx(), gModelMtx.m, gModelMtx.m);
    return Vec2F(gModelMtx.m[0][3], gModelMtx.m[1][3]);
}

void LayoutButton::SetChildAlpha(const char* name, s32 alpha) {
    SetChildAlphaRecursive(mPane->FindPaneByName(name, true), alpha);
}

// Expanded three levels deep in SetChildAlpha and two in itself, like Hover.
inline void LayoutButton::SetChildAlphaRecursive(nw4r::lyt::Pane* pane, s32 alpha) {
    if (pane != NULL) {
        pane->SetAlpha(alpha);
        nw4r::lyt::PaneList& list = pane->GetChildList();
        for (nw4r::lyt::PaneList::Iterator it = list.GetBeginIter(); it != list.GetEndIter(); ++it) {
            SetChildAlphaRecursive(&*it, alpha);
        }
    }
}

void LayoutButton::SetPaneAlpha(s32 alpha) {
    mPane->SetAlpha(alpha);
}

// Sets TEV color `which` of the named pane's tree, and its alpha the way SetPaneTevAlpha
// does (expanded here as in the original).
void LayoutButton::SetPaneColor(const char* name, const nw4r::ut::Color& color, int which) {
    nw4r::lyt::Pane* pane = mPane->FindPaneByName(name, true);
    SetPaneTevColor(pane, &color, which);
    u8 alpha = color.a;

    nw4r::lyt::Material* material = pane->FindMaterialByName(pane->GetName(), true);
    if (material != NULL) {
        material->SetColorElement(11, alpha);
    }

    nw4r::lyt::PaneList& list1 = pane->GetChildList();
    for (nw4r::lyt::PaneList::Iterator it1 = list1.GetBeginIter(); it1 != list1.GetEndIter(); ++it1) {
        nw4r::lyt::Pane* child1 = &*it1;
        material = child1->FindMaterialByName(child1->GetName(), true);
        if (material != NULL) {
            material->SetColorElement(11, alpha);
        }

        nw4r::lyt::PaneList& list2 = child1->GetChildList();
        for (nw4r::lyt::PaneList::Iterator it2 = list2.GetBeginIter(); it2 != list2.GetEndIter(); ++it2) {
            nw4r::lyt::Pane* child2 = &*it2;
            material = child2->FindMaterialByName(child2->GetName(), true);
            if (material != NULL) {
                material->SetColorElement(11, alpha);
            }

            nw4r::lyt::PaneList& list3 = child2->GetChildList();
            for (nw4r::lyt::PaneList::Iterator it3 = list3.GetBeginIter(); it3 != list3.GetEndIter(); ++it3) {
                SetPaneTevAlpha(&*it3, alpha);
            }
        }
    }
}

nw4r::lyt::Pane* LayoutButton::FindPane(const char* name) {
    return mPane->FindPaneByName(name, true);
}

void LayoutButton::ShowLanguagePane(const char* name) {
    nw4r::lyt::Pane* pane = mPane->FindPaneByName(name, true);
    if (pane == NULL) {
        return;
    }

    char buf[128];
    sprintf(buf, "%s%s", name, GetLanguageCode());
    nw4r::lyt::Pane* found = pane->FindPaneByName(buf, true);
    nw4r::lyt::PaneList& list = pane->GetChildList();
    for (nw4r::lyt::PaneList::Iterator it = list.GetBeginIter(); it != list.GetEndIter(); ++it) {
        if (&*it != found) {
            it->SetVisible(false);
        }
    }
}
