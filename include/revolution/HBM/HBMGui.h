#ifndef RVL_SDK_HBM_HBMGUI_H
#define RVL_SDK_HBM_HBMGUI_H
#include <types.h>
#include <revolution/MEM.h>
#include <revolution/MTX.h>
#include <nw4r/lyt.h>
#include <nw4r/ut/ut_list.h>

// RVL_SDK Home Button Menu - animation controllers and the gui framework.

namespace homebutton {

// Steps an animation frame forward, backward, looping or back and forth.
class FrameController {
public:
    enum {
        ANM_TYPE_FORWARD,
        ANM_TYPE_BACKWARD,
        ANM_TYPE_LOOP,
        ANM_TYPE_ALTERNATE,
    };

    enum {
        STATE_STOP,
        STATE_PLAY,
    };

    virtual ~FrameController() {}
    virtual void calc();

    void init(int anmType, f32 maxFrame, f32 minFrame, f32 delta);
    void initFrame();

    void start() {
        initFrame();
        mState = STATE_PLAY;
    }

    void stop() {
        mState = STATE_STOP;
    }

    f32 getMaxFrame() const {
        return mMaxFrame;
    }

    f32 getCurrentFrame() const {
        return mFrame;
    }

    f32 mMaxFrame;   // at 0x04
    f32 mMinFrame;   // at 0x08
    f32 mFrame;      // at 0x0C
    f32 mDelta;      // at 0x10
    int mState;      // at 0x14
    int mAnmType;    // at 0x18
    bool mAltFlag;   // at 0x1C, playing backward in ANM_TYPE_ALTERNATE
};

// Plays one animation on every pane of a layout group.
class GroupAnmController : public FrameController {
public:
    GroupAnmController();
    virtual ~GroupAnmController();

    void do_calc();

    nw4r::lyt::Group* mpGroup;               // at 0x20
    nw4r::lyt::AnimTransform* mpAnimGroup;   // at 0x24
};

namespace gui {

class Manager;
class Component;
class PaneComponent;

class Interface {
public:
    virtual void create() {}
    virtual void init() {}
    virtual void calc() {}
    virtual void draw(Mtx& mtx) {}
    virtual void draw() {}
    virtual ~Interface() {}
};

class EventHandler {
public:
    virtual void onEvent(u32 id, u32 event, void* data) {}
    virtual void setManager(Manager* manager) {
        mpManager = manager;
    }

    Manager* mpManager;  // at 0x4
};

// A menu element that reacts to the pointer.
class Component : public Interface {
public:
    Component(u32 id)
        : mDragging(false), mDraggingButton(0), mID(id), mTriggerTarget(false), mpManager(NULL) {
        init();
    }
    virtual ~Component() {}

    virtual void init() {
        mDragging = false;
        for (int i = 0; i < 8; i++) {
            mabPointed[i] = false;
        }
    }
    virtual u32 getID();
    virtual int isPointed(int n);
    virtual void setPointed(int n, bool pointed);
    virtual void onPoint();
    virtual void offPoint();
    virtual void onDrag(f32 x, f32 y);
    virtual void onMove(f32 x, f32 y);
    virtual void onTrig(u32 flag, Vec& pos) {
        if (flag & mDraggingButton) {
            mDragStartPos = pos;
            mDragging = true;
        }
    }
    virtual void setDraggingButton(u32 button);
    virtual bool isDraggable() const;
    virtual bool update(int i, f32 x, f32 y, u32 trig, u32 hold, u32 release, void* data);
    virtual bool isTriggerTarget() const {
        return mTriggerTarget;
    }
    virtual void setTriggerTarget(bool target) {
        mTriggerTarget = target;
    }
    virtual void setManager(Manager* manager);
    virtual bool isVisible();
    virtual bool contain(f32 x, f32 y) = 0;

    bool mabPointed[8];    // at 0x04
    Vec mDragStartPos;     // at 0x0C
    bool mDragging;        // at 0x18
    u32 mDraggingButton;   // at 0x1C
    u32 mID;               // at 0x20
    bool mTriggerTarget;   // at 0x24
    Manager* mpManager;    // at 0x28
};

// Owns a list of components and forwards their events to an EventHandler.
class Manager : public Interface {
public:
    Manager(EventHandler* eventHandler, MEMAllocator* allocator)
        : mpEventHandler(eventHandler), mpAllocator(allocator) {
        if (eventHandler != NULL) {
            eventHandler->setManager(this);
        }
        nw4r::ut::List_Init(&mComponentList, 8);
    }
    virtual ~Manager();

    virtual void init();
    virtual void calc();
    virtual void draw();
    virtual void addComponent(Component* component);
    virtual Component* getComponent(u32 id);
    virtual bool isTriggerTarget() const {
        return false;
    }
    virtual bool update(int i, f32 x, f32 y, u32 trig, u32 hold, u32 release, void* data);
    virtual void onEvent(u32 id, u32 event, void* data);
    virtual void setAllComponentTriggerTarget(bool target);
    virtual void setEventHandler(EventHandler* eventHandler) {
        mpEventHandler = eventHandler;
        if (eventHandler != NULL) {
            eventHandler->setManager(this);
        }
    }

    EventHandler* mpEventHandler;   // at 0x04
    nw4r::ut::List mComponentList;  // at 0x08
    MEMAllocator* mpAllocator;      // at 0x14
};

// Makes a PaneComponent for every pane of a layout.
class PaneManager : public Manager {
public:
    PaneManager(EventHandler* eventHandler, const nw4r::lyt::DrawInfo* drawInfo, MEMAllocator* allocator)
        : Manager(eventHandler, allocator), mpDrawInfo(drawInfo) {
        nw4r::ut::List_Init(&mPaneList, 8);
    }
    virtual ~PaneManager();

    virtual void createLayoutScene(const nw4r::lyt::Layout& layout);
    virtual PaneComponent* getPaneComponentByPane(nw4r::lyt::Pane* pane);
    virtual const nw4r::lyt::DrawInfo* getDrawInfo();
    virtual void setDrawInfo(const nw4r::lyt::DrawInfo* drawInfo) {
        mpDrawInfo = drawInfo;
    }
    virtual void setAllBoundingBoxComponentTriggerTarget(bool target);
    virtual void walkInChildren(nw4r::lyt::PaneList& children);

    nw4r::ut::List mPaneList;               // at 0x18
    const nw4r::lyt::DrawInfo* mpDrawInfo;  // at 0x24
    u16 mNumPoint;                          // at 0x28
    u16 mPadding;                           // at 0x2A

    static u32 suIDCounter;
};

// A component bound to one layout pane.
class PaneComponent : public Component {
public:
    PaneComponent(u32 id) : Component(id), mpPane(NULL) {}
    virtual ~PaneComponent() {}

    virtual void draw();
    virtual bool isVisible();
    virtual bool contain(f32 x, f32 y);
    virtual void setPane(nw4r::lyt::Pane* pane);
    virtual nw4r::lyt::Pane* getPane() {
        return mpPane;
    }

    nw4r::lyt::Pane* mpPane;  // at 0x2C
};

}  // namespace gui
}  // namespace homebutton

#endif
