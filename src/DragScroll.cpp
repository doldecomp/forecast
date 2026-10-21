#include <channel/System.h>
#include <nw4r/math.h>

extern const f32 sDragScrollZero;
extern const f32 sDragScrollOffset;
extern const f32 sDragScrollThreshold;
extern const f32 sDragScrollNegThreshold;
extern const f32 sDragScrollMax;
extern const f32 sDragScrollAngleScale;
extern const f32 sDragScrollHalfPi;
extern const f32 sDragScrollTimerScale;

// Layout matches gDragScroll in SceneBase.cpp (size 0x34).
struct DragScroll {
    DragScroll();
    void Reset();
    void Update();
    void UpdateChan(s32 chan);

    f32 mScroll[4]; // at 0x00
    s32 mTimer[4];  // at 0x10
    s32 mState[4];  // at 0x20
    u8 mActive;     // at 0x30
    u8 unk31[3];    // at 0x31
};

DragScroll::DragScroll() {
    for (int i = 0; i < 4; i++) {
        mScroll[i] = sDragScrollZero;
        mTimer[i] = 0;
        mState[i] = 0;
    }
    mActive = 0;
}

void DragScroll::Reset() {
    for (int i = 0; i < 4; i++) {
        mScroll[i] = sDragScrollZero;
        mTimer[i] = 0;
        mState[i] = 0;
    }
    mActive = 0;
}

void DragScroll::UpdateChan(s32 chan) {
    f32* pScroll = &mScroll[chan];
    s32* pTimer = &mTimer[chan];
    s32* pState = &mState[chan];

    f32 scroll = sDragScrollOffset - gPointerY[chan][0];
    *pScroll = scroll;

    BOOL zero = scroll < sDragScrollThreshold && scroll > sDragScrollNegThreshold;
    if (zero) {
        *pScroll = sDragScrollZero;
        return;
    }

    if (*pTimer > 0)
        (*pTimer)--;

    if (*pTimer != 0)
        return;

    *pState = *pScroll > sDragScrollZero ? 1 : 2;

    f32 max = sDragScrollMax;
    f32 amount = __fabsf(*pScroll) - sDragScrollThreshold;
    if (amount > max)
        amount = max;

    f32 angle = sDragScrollAngleScale * (sDragScrollHalfPi * amount / max);
    *pTimer = (s32)(sDragScrollTimerScale * nw4r::math::CosFIdx(angle)) + 1;
}

void DragScroll::Update() {
    s32* pTimer = mTimer;
    s32* pState = mState;
    int i;

    for (i = 0; i < 4; i++, pState++, pTimer++) {
        *pState = 0;

        if (gHold[i] & 0x400) { // WPAD_BUTTON_B
            mActive = TRUE;
            UpdateChan(i);
        } else {
            *pTimer = 0;
        }
    }
}
