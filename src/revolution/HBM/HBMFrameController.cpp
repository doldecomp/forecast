#include <nw4r_compat/cmath>
#include <nw4r_compat/cstdarg>
#include <nw4r_compat/cstdio>
#include <nw4r_compat/cstring>
#include <nw4r_compat/cwchar>

#include <types.h>

// RVL_SDK Home Button Menu - frame controller.

#include <revolution/HBM/HBMGui.h>

namespace homebutton {

void FrameController::init(int anmType, f32 maxFrame, f32 minFrame, f32 delta) {
    mAnmType = anmType;
    mMaxFrame = maxFrame;
    mMinFrame = minFrame;
    mDelta = delta;
    mState = STATE_STOP;
    mAltFlag = false;
    initFrame();
}

void FrameController::initFrame() {
    mFrame = mAnmType == ANM_TYPE_BACKWARD ? mMaxFrame : mMinFrame;
}

void FrameController::calc() {
    if (mState != STATE_PLAY) {
        return;
    }

    switch (mAnmType) {
    case ANM_TYPE_FORWARD: {
        f32 last = mMaxFrame - 1.0f;
        if ((mFrame += mDelta) >= last) {
            mFrame = last;
            mState = STATE_STOP;
        }
        break;
    }
    case ANM_TYPE_BACKWARD:
        if ((mFrame -= mDelta) <= mMinFrame) {
            mFrame = mMinFrame;
            mState = STATE_STOP;
        }
        break;
    case ANM_TYPE_LOOP:
        if ((mFrame += mDelta) >= mMaxFrame) {
            mFrame -= mMaxFrame - mMinFrame;
        }
        break;
    case ANM_TYPE_ALTERNATE:
        if (!mAltFlag) {
            f32 last = mMaxFrame - 1.0f;
            if ((mFrame += mDelta) >= last) {
                mFrame = last;
                mAltFlag = true;
            }
        } else {
            if ((mFrame -= mDelta) <= mMinFrame) {
                mFrame = mMinFrame;
                mAltFlag = false;
            }
        }
        break;
    }
}

}  // namespace homebutton
