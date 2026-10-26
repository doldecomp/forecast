// A layout whose top-level panes are all buttons, with slide and fade animations
#include <channel/LayoutButton.h>
#include <channel/TextTagProcessor.h>
#include <channel/System.h>

#include <nw4r/lyt/lyt_arcResourceAccessor.h>
#include <nw4r/lyt/lyt_drawInfo.h>
#include <nw4r/lyt/lyt_layout.h>
#include <nw4r/lyt/lyt_pane.h>
#include <nw4r/math.h>
#include <revolution/GX.h>
#include <revolution/MTX.h>

inline void ButtonGroup::ResetButtons() {
    for (int i = 0; i < mNumButtons; i++) {
        mButtons[i]->Reset();
    }
}

ButtonGroup::ButtonGroup(void* arc, const char* layoutName, ButtonColors* colors, bool influencedAlpha)
    : mFadeFrames(0), mFadeFrame(0), mAlpha(255) {
    mResAccessor = new nw4r::lyt::ArcResourceAccessor;
    mResAccessor->Attach(arc, "arc");

    mLayout = new nw4r::lyt::Layout;
    void* layoutBin = mResAccessor->GetResource(0, layoutName, NULL);
    mLayout->Build(layoutBin, mResAccessor);

    mDrawInfo = new nw4r::lyt::DrawInfo;
    mDrawInfo->SetInfluencedAlpha(influencedAlpha);
    mDrawInfo->SetLocationAdjust(true);

    f32 aspect = gWidescreen ? 832.0f / 608.0f : 1.0f;
    f32 sx = 1.0f / aspect;
    mDrawInfo->SetLocationAdjustScale(nw4r::math::VEC2(sx, 1.0f));
    mDrawInfo->SetViewRect(mLayout->GetLayoutRect());

    nw4r::math::MTX34 mtx;
    PSMTXIdentity(mtx);
    mDrawInfo->SetViewMtx(mtx);

    mTagProcessor = new TextTagProcessor;

    nw4r::lyt::PaneList& panes = mLayout->GetRootPane()->GetChildList();
    mNumButtons = 0;
    for (nw4r::lyt::PaneList::Iterator it = panes.GetBeginIter(); it != panes.GetEndIter(); ++it) {
        if (mNumButtons >= BUTTON_GROUP_MAX_BUTTONS) {
            break;
        }

        mButtons[mNumButtons++] = new LayoutButton(&*it, mDrawInfo, colors, mTagProcessor);
    }

    ResetButtons();

    mSlidingOut = FALSE;
    mSlideFrames = 15;
    mSlideFrame = 0;
}

ButtonGroup::~ButtonGroup() {
    for (int i = 0; i < mNumButtons; i++) {
        delete mButtons[i];
    }

    delete mTagProcessor;
    delete mDrawInfo;
    delete mLayout;
    mResAccessor->Detach();
    delete mResAccessor;
}

void ButtonGroup::Reset() {
    for (int i = 0; i < mNumButtons; i++) {
        mButtons[i]->Reset();
    }

    mSlidingOut = FALSE;
    mSlideFrames = 15;
    mSlideFrame = 0;
}

void ButtonGroup::ReleaseAll() {
    for (int i = 0; i < mNumButtons; i++) {
        mButtons[i]->Release();
    }

    mSlidingOut = FALSE;
    mSlideFrames = 15;
    mSlideFrame = 0;
}

inline void ButtonGroup::UpdateAll() {
    for (int i = 0; i < mNumButtons; i++) {
        mButtons[i]->Update();
    }
}

inline void ButtonGroup::ApplySlide(f32 offset) {
    for (int i = 0; i < mNumButtons; i++) {
        mButtons[i]->mSlideOffset = offset * (mButtons[i]->mPane->GetTranslate().y > 0.0f ? 1 : -1);
    }
}

inline void ButtonGroup::ApplyAlpha(s32 alpha) {
    for (int i = 0; i < mNumButtons; i++) {
        mButtons[i]->SetAlpha(alpha);
    }
}

void ButtonGroup::Calc() {
    for (int i = 0; i < mNumButtons; i++) {
        mButtons[i]->Calc();
    }

    if (mSlidingOut) {
        if (mSlideFrame < mSlideFrames) {
            mSlideFrame++;
        }
    } else if (mSlideFrame > 0) {
        mSlideFrame--;
    }

    f32 height;
    if (mNumButtons > 0) {
        height = nw4r::math::FAbs(mButtons[0]->mRect.top - mButtons[0]->mRect.bottom);
    } else {
        height = 0.0f;
    }

    f32 offset = height * mSlideFrame / mSlideFrames;
    ApplySlide(offset);

    if (mFadingOut) {
        if (mFadeFrame < mFadeFrames) {
            mFadeFrame++;
        }
    } else if (mFadeFrame > 0) {
        mFadeFrame--;
    }

    s32 alpha = 255 - mFadeFrame * 255 / mFadeFrames;
    mAlpha = alpha;
    ApplyAlpha(alpha);
}

void ButtonGroup::Draw() {
    f32 near = 0.0f;
    f32 far = 1.0f;
    if ((s32)mLayout->GetOriginType() == nw4r::lyt::ORIGINTYPE_CENTER) {
        near = -near;
        far = -far;
    }

    nw4r::ut::Rect rect = mLayout->GetLayoutRect();
    Mtx44 proj;
    C_MTXOrtho(proj, rect.top, rect.bottom, rect.left, rect.right, near, far);
    GXSetProjection(proj, GX_ORTHOGRAPHIC);
    GXSetNumChans(1);
    GXSetCullMode(GX_CULL_NONE);
    GXSetZMode(GX_FALSE, GX_NEVER, GX_FALSE);

    UpdateAll();

    mLayout->CalculateMtx(*mDrawInfo);

    for (int j = 0; j < mNumButtons; j++) {
        mButtons[j]->Draw();
    }
}

LayoutButton* ButtonGroup::HitTest(f32 x, f32 y) {
    if (mFadingOut && mFadeFrame >= mFadeFrames) {
        return NULL;
    }

    Vec pos;
    pos.x = x;
    pos.y = y;
    pos.z = 0.0f;

    Mtx inv;
    PSMTXInverse(mDrawInfo->GetViewMtx(), inv);
    PSMTXMultVec(inv, &pos, &pos);

    for (int i = mNumButtons - 1; i >= 0; i--) {
        if (mButtons[i]->Contains(pos.x, pos.y)) {
            return mButtons[i];
        }
    }

    return NULL;
}

LayoutButton* ButtonGroup::FindButton(const char* name) {
    s32 count = mNumButtons;
    for (int i = 0; i < count; i++) {
        if (mButtons[i]->IsName(name)) {
            return mButtons[i];
        }
    }

    return NULL;
}

void ButtonGroup::SlideIn(s32 frames) {
    mSlidingOut = FALSE;
    if (frames != mSlideFrames) {
        mSlideFrame = mSlideFrame * frames / mSlideFrames;
        mSlideFrames = frames;
    }
}

void ButtonGroup::SlideOut(s32 frames) {
    mSlidingOut = TRUE;
    if (frames != mSlideFrames) {
        mSlideFrame = mSlideFrame * frames / mSlideFrames;
        mSlideFrames = frames;
    }
}

void ButtonGroup::FadeIn(s32 frames) {
    mFadingOut = FALSE;
    if (frames != mFadeFrames) {
        mFadeFrame = mFadeFrame * frames / mFadeFrames;
        mFadeFrames = frames;
    }
}

void ButtonGroup::SetButtonParams(s32 brightness, s32 highlightFrame, s32 highlightFrames) {
    for (int i = 0; i < mNumButtons; i++) {
        mButtons[i]->SetParams(brightness, highlightFrame, highlightFrames);
    }
}

void ButtonGroup::SetPaneAlpha(s32 alpha) {
    for (int i = 0; i < mNumButtons; i++) {
        mButtons[i]->SetPaneAlpha(alpha);
    }
}

void ButtonGroup::SetViewMtx(const nw4r::math::MTX34& mtx) {
    mDrawInfo->SetViewMtx(mtx);
}

void ButtonGroup::SetSlideOffset(f32 offset) {
    LayoutButton* b;
    for (int i = 0; i < mNumButtons; i++) {
        b = mButtons[i];
        b->SetSlideOffset(offset);
    }
}
