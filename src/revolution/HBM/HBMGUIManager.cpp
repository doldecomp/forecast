#include <nw4r_compat/cmath>
#include <nw4r_compat/cstdarg>
#include <nw4r_compat/cstdio>
#include <nw4r_compat/cstring>
#include <nw4r_compat/cwchar>

#include <revolution/GX.h>
#include <types.h>

// RVL_SDK Home Button Menu - gui framework.
//
// The menu is a tree of gui::Component objects, one per lyt pane, owned by a
// gui::PaneManager.

#include <revolution/HBM/HBMGui.h>

namespace homebutton {

namespace gui {

// One entry of Manager::mComponentList.
struct ComponentLink {
    ComponentLink(u32 id, Component* component) : mID(id), mpComponent(component) {}

    u32 mID;                  // at 0x0
    Component* mpComponent;   // at 0x4
    nw4r::ut::Link mLink;     // at 0x8
};

// One entry of PaneManager::mPaneList.
struct PaneLink {
    PaneLink(nw4r::lyt::Pane* pane, PaneComponent* component) : mpPane(pane), mpComponent(component) {}

    nw4r::lyt::Pane* mpPane;      // at 0x0
    PaneComponent* mpComponent;   // at 0x4
    nw4r::ut::Link mLink;         // at 0x8
};

u32 PaneManager::suIDCounter;

static void drawLine_(f32 x0, f32 y0, f32 x1, f32 y1, f32 z, u8 width, GXColor& color) {
    Mtx mtx;

    GXClearVtxDesc();
    GXSetVtxDesc(GX_VA_POS, GX_DIRECT);
    GXSetVtxDesc(GX_VA_CLR0, GX_DIRECT);
    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
    GXSetCullMode(GX_CULL_NONE);
    GXSetNumChans(1);
    GXSetChanCtrl(GX_COLOR0A0, GX_FALSE, GX_SRC_VTX, GX_SRC_VTX, GX_LIGHT_NULL, GX_DF_NONE, GX_AF_NONE);
    GXSetNumTexGens(0);
    GXSetNumTevStages(1);
    GXSetTevOp(GX_TEVSTAGE0, GX_PASSCLR);
    GXSetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD_NULL, GX_TEXMAP_NULL, GX_COLOR0A0);
    GXSetBlendMode(GX_BM_NONE, GX_BL_ZERO, GX_BL_ZERO, GX_LO_NOOP);
    PSMTXTrans(mtx, 0.0f, 0.0f, 0.0f);
    GXLoadPosMtxImm(mtx, GX_PNMTX0);
    GXSetLineWidth(width, GX_TO_ZERO);

    GXBegin(GX_LINES, GX_VTXFMT0, 2);
    GXPosition3f32(x0, y0, z);
    GXColor1u32(*(u32*)&color);
    GXPosition3f32(x1, y1, z);
    GXColor1u32(*(u32*)&color);
    GXEnd();
}

bool Component::update(int i, f32 x, f32 y, u32 trig, u32 hold, u32 release, void* data) {
    bool ret = false;

    if (isVisible()) {
        if (contain(x, y)) {
            if (isPointed(i)) {
                onMove(x, y);
                mpManager->onEvent(getID(), 3, data);
            } else {
                setPointed(i, true);
                onPoint();
                mpManager->onEvent(getID(), 1, data);
            }
            ret = true;
        } else if (isPointed(i)) {
            setPointed(i, false);
            offPoint();
            mpManager->onEvent(getID(), 2, data);
        }
    }

    return ret;
}

bool Component::isVisible() {
    return true;
}

int Component::isPointed(int n) {
    return mabPointed[n];
}

void Component::onMove(f32 x, f32 y) {}

void Manager::onEvent(u32 id, u32 event, void* data) {
    if (mpEventHandler != NULL) {
        mpEventHandler->onEvent(id, event, data);
    }
}

u32 Component::getID() {
    return mID;
}

void Component::setPointed(int n, bool pointed) {
    mabPointed[n] = pointed;
}

void Component::onPoint() {}

void Component::offPoint() {}

Manager::~Manager() {
    void* link = nw4r::ut::List_GetNext(&mComponentList, NULL);
    while (link != NULL) {
        nw4r::ut::List_Remove(&mComponentList, link);
        if (mpAllocator != NULL) {
            MEMFreeToAllocator(mpAllocator, link);
        } else {
            delete (ComponentLink*)link;
        }
        link = nw4r::ut::List_GetNext(&mComponentList, NULL);
    }
}

void Manager::init() {
    for (u32 i = 0; i < mComponentList.numObjects; i++) {
        ComponentLink* link = (ComponentLink*)nw4r::ut::List_GetNth(&mComponentList, i);
        link->mpComponent->init();
    }
}

void Manager::addComponent(Component* component) {
    u32 id = component->getID();
    component->setManager(this);

    if (mpAllocator != NULL) {
        void* buf = MEMAllocFromAllocator(mpAllocator, sizeof(ComponentLink));
        nw4r::ut::List_Append(&mComponentList, new (buf) ComponentLink(id, component));
    } else {
        nw4r::ut::List_Append(&mComponentList, new ComponentLink(id, component));
    }
}

void Component::setManager(Manager* manager) {
    mpManager = manager;
}

Component* Manager::getComponent(u32 id) {
    return ((ComponentLink*)nw4r::ut::List_GetNth(&mComponentList, id))->mpComponent;
}

bool Manager::update(int i, f32 x, f32 y, u32 trig, u32 hold, u32 release, void* data) {
    bool ret = false;
    Component* target = NULL;

    for (u32 n = 0; n < mComponentList.numObjects; n++) {
        ComponentLink* link = (ComponentLink*)nw4r::ut::List_GetNth(&mComponentList, n);
        if (link->mpComponent->update(i, x, y, trig, hold, release, data)) {
            if (link->mpComponent->isTriggerTarget()) {
                target = link->mpComponent;
            }
            ret = true;
        }
    }

    if (target != NULL) {
        if (trig) {
            Vec pos;
            target->onTrig(trig, pos);
            onEvent(target->getID(), 0, data);
        }

        if (release) {
            Vec pos;
            target->onTrig(release, pos);
            onEvent(target->getID(), 5, data);
        }
    }

    return ret;
}

void Manager::calc() {
    for (u32 i = 0; i < mComponentList.numObjects; i++) {
        ComponentLink* link = (ComponentLink*)nw4r::ut::List_GetNth(&mComponentList, i);
        link->mpComponent->calc();
    }
}

void Manager::draw() {
    for (u32 i = 0; i < mComponentList.numObjects; i++) {
        ComponentLink* link = (ComponentLink*)nw4r::ut::List_GetNth(&mComponentList, i);
        link->mpComponent->draw();
    }
}

void Manager::setAllComponentTriggerTarget(bool target) {
    for (u32 i = 0; i < mComponentList.numObjects; i++) {
        ComponentLink* link = (ComponentLink*)nw4r::ut::List_GetNth(&mComponentList, i);
        link->mpComponent->setTriggerTarget(target);
    }
}

PaneManager::~PaneManager() {
    PaneLink* link = (PaneLink*)nw4r::ut::List_GetNext(&mPaneList, NULL);
    while (link != NULL) {
        nw4r::ut::List_Remove(&mPaneList, link);
        if (mpAllocator != NULL) {
            MEMFreeToAllocator(mpAllocator, link->mpComponent);
            MEMFreeToAllocator(mpAllocator, link);
        } else {
            delete link->mpComponent;
            delete link;
        }
        link = (PaneLink*)nw4r::ut::List_GetNext(&mPaneList, NULL);
    }
}

void PaneManager::createLayoutScene(const nw4r::lyt::Layout& layout) {
    suIDCounter = 0;
    walkInChildren(layout.GetRootPane()->GetChildList());
}

void PaneManager::walkInChildren(nw4r::lyt::PaneList& children) {
    for (nw4r::lyt::PaneList::Iterator it = children.GetBeginIter(); it != children.GetEndIter(); it++) {
        PaneComponent* component;
        PaneLink* link;

        if (mpAllocator != NULL) {
            void* buf = MEMAllocFromAllocator(mpAllocator, sizeof(PaneComponent));
            void* linkBuf = MEMAllocFromAllocator(mpAllocator, sizeof(PaneLink));
            component = new (buf) PaneComponent(suIDCounter);
            link = new (linkBuf) PaneLink(&*it, component);
        } else {
            component = new PaneComponent(suIDCounter);
            link = new PaneLink(&*it, component);
        }

        nw4r::ut::List_Append(&mPaneList, link);
        suIDCounter++;

        nw4r::lyt::Pane* pane = &*it;
        component->setPane(pane);

        if (nw4r::ut::DynamicCast<nw4r::lyt::Picture*>(pane) != NULL) {
            component->setTriggerTarget(true);
        }

        if (nw4r::ut::DynamicCast<nw4r::lyt::Window*>(pane) != NULL) {
            component->setTriggerTarget(true);
        }

        addComponent(component);
        walkInChildren(pane->GetChildList());
    }
}

void PaneComponent::setPane(nw4r::lyt::Pane* pane) {
    mpPane = pane;
}

PaneComponent* PaneManager::getPaneComponentByPane(nw4r::lyt::Pane* pane) {
    for (u32 i = 0; i < mComponentList.numObjects; i++) {
        PaneLink* link = (PaneLink*)nw4r::ut::List_GetNth(&mPaneList, i);
        if (link->mpPane == pane) {
            return link->mpComponent;
        }
    }

    return NULL;
}

// The target has no zero-trip check before this loop: it only appears with
// constant propagation off, as in other decompilations of this SDK.
#pragma push
#pragma opt_propagation off
void PaneManager::setAllBoundingBoxComponentTriggerTarget(bool target) {
    for (u32 i = 0; i < nw4r::ut::List_GetSize(&mComponentList); i++) {
        PaneLink* link = (PaneLink*)nw4r::ut::List_GetNth(&mPaneList, i);
        if (nw4r::ut::DynamicCast<nw4r::lyt::Bounding*>(link->mpPane) != NULL) {
            link->mpComponent->setTriggerTarget(target);
        }
    }
}
#pragma pop

static inline void TransformPos(nw4r::math::VEC3* out, const nw4r::math::MTX34* mtx, const nw4r::math::VEC3& pos) {
    nw4r::math::VEC3Transform(out, mtx, &pos);
}

bool PaneComponent::contain(f32 x, f32 y) {
    if (mpManager == NULL) {
        return false;
    }

    const nw4r::lyt::DrawInfo* drawInfo = static_cast<PaneManager*>(mpManager)->getDrawInfo();
    if (drawInfo == NULL) {
        return false;
    }

    nw4r::math::MTX34 inv;
    nw4r::math::VEC3 local;
    nw4r::math::MTX34Inv(&inv, &mpPane->GetGlobalMtx());
    TransformPos(&local, &inv, nw4r::math::VEC3(x, y, 0.0f));

    nw4r::ut::Rect rect = mpPane->GetPaneRect(*drawInfo);
    if (rect.left <= local.x && local.x <= rect.right && rect.bottom <= local.y && local.y <= rect.top) {
        return true;
    }

    return false;
}

const nw4r::lyt::DrawInfo* PaneManager::getDrawInfo() {
    return mpDrawInfo;
}

void PaneComponent::draw() {
    if (static_cast<PaneManager*>(mpManager)->getDrawInfo() == NULL) {
        return;
    }

    nw4r::lyt::Size size = mpPane->GetSize();
    f32 x = mpPane->GetGlobalMtx()[0][3];
    f32 y = mpPane->GetGlobalMtx()[1][3];
    GXColor color = {0xFF, 0x00, 0x00, 0xFF};

    if (mabPointed[0]) {
        color.r = 0x00;
        color.b = 0xFF;
    }

    drawLine_(x - size.width / 2.0f, y - size.height / 2.0f, x + size.width / 2.0f, y - size.height / 2.0f, 0.0f, 8, color);
    drawLine_(x + size.width / 2.0f, y - size.height / 2.0f, x + size.width / 2.0f, y + size.height / 2.0f, 0.0f, 8, color);
    drawLine_(x + size.width / 2.0f, y + size.height / 2.0f, x - size.width / 2.0f, y + size.height / 2.0f, 0.0f, 8, color);
    drawLine_(x - size.width / 2.0f, y + size.height / 2.0f, x - size.width / 2.0f, y - size.height / 2.0f, 0.0f, 8, color);
}

static bool is_visible(nw4r::lyt::Pane* pane) {
    if (!pane->IsVisible()) {
        return false;
    }

    if (pane->GetParent() == NULL) {
        return true;
    }

    return is_visible(pane->GetParent());
}

bool PaneComponent::isVisible() {
    return is_visible(mpPane);
}

bool Component::isDraggable() const {
    return false;
}

void Component::setDraggingButton(u32 button) {
    mDraggingButton = button;
}

void Component::onDrag(f32 x, f32 y) {}

}  // namespace gui

}  // namespace homebutton
