#include <nw4r_compat/cmath>
#include <nw4r_compat/cstdarg>
#include <nw4r_compat/cstdio>
#include <nw4r_compat/cstring>
#include <nw4r_compat/cwchar>

#include <types.h>

// RVL_SDK Home Button Menu - group animation controller.

#include <revolution/HBM/HBMGui.h>

namespace homebutton {

GroupAnmController::GroupAnmController() : mpGroup(NULL), mpAnimGroup(NULL) {}

GroupAnmController::~GroupAnmController() {}

void GroupAnmController::do_calc() {
    bool enable;

    if (mState == STATE_PLAY) {
        calc();
        mpAnimGroup->SetFrame(mFrame);
        enable = true;
    } else {
        enable = false;
    }

    nw4r::lyt::detail::PaneLinkList& list = mpGroup->GetPaneList();
    for (nw4r::lyt::detail::PaneLinkList::Iterator it = list.GetBeginIter(); it != list.GetEndIter(); it++) {
        it->mTarget->SetAnimationEnable(mpAnimGroup, enable, false);
    }
}

}  // namespace homebutton
