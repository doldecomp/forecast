#ifndef RVL_SDK_HBM_HBMBASE_H
#define RVL_SDK_HBM_HBMBASE_H
#include <types.h>
#include <revolution/MEM.h>
#include <revolution/OS.h>
#include <revolution/AX.h>
#include <revolution/AXFX.h>
#include <revolution/HBM/HBM.h>
#include <revolution/HBM/HBMGui.h>
#include <nw4r/lyt.h>

// RVL_SDK Home Button Menu - the menu itself (homebutton::HomeButton).

namespace nw4r {
namespace snd {
class SoundArchive;
class MemorySoundArchive;
class DvdSoundArchive;
class NandSoundArchive;
class SoundHandle;
class SoundHeap;
}  // namespace snd
}  // namespace nw4r

typedef int (*HBSoundCallback)(int event, int id);

// Configuration block handed to HBM by the application.
struct HBMDataInfo {
    void* layoutBuf;                 // at 0x00
    void* spkSeBuf;                  // at 0x04
    void* msgBuf;                    // at 0x08
    void* configBuf;                 // at 0x0C
    void* mem;                       // at 0x10
    HBSoundCallback sound_callback;  // at 0x14
    int backFlag;                    // at 0x18
    int region;                      // at 0x1C
    int cursor;                      // at 0x20
    int messageFlag;                 // at 0x24
    u32 configBufSize;               // at 0x28
    u32 memSize;                     // at 0x2C
    f32 frameDelta;                  // at 0x30
    Vec2 adjust;                     // at 0x34
    MEMAllocator* pAllocator;        // at 0x3C
};

namespace homebutton {

class HomeButton;

// Forwards the gui events of the menu to the HomeButton.
class HomeButtonEventHandler : public gui::EventHandler {
public:
    HomeButtonEventHandler(HomeButton* homeButton) : mpHomeButton(homeButton) {}

    virtual void onEvent(u32 id, u32 event, void* data);

    HomeButton* mpHomeButton;  // at 0x8
};

// Fade to black used when the menu closes.
struct HomeButtonFader {
    int frame;     // at 0x0
    int maxFrame;  // at 0x4
    int state;     // at 0x8
    u8 r;          // at 0xC
    u8 g;          // at 0xD
    u8 b;          // at 0xE

    void draw();

    void init(int frames) {
        frame = 0;
        maxFrame = frames;
        state = 0;
    }

    bool isDone() {
        if (state == 1 && frame == maxFrame) {
            return true;
        } else if (state == 2 && frame == 0) {
            return true;
        }
        return false;
    }

    void calc() {
        if (state == 1) {
            frame++;
        } else if (state == 2) {
            frame--;
        }

        if (frame < 0) {
            frame = 0;
        } else if (frame > maxFrame) {
            frame = maxFrame;
        }
    }
};

class HomeButton {
public:
    HomeButton(const HBMDataInfo* info);

    static HomeButton* getInstance();
    ~HomeButton();

    void create();
    void init();
    void calc(const HBMControllerData* controller);
    void draw();
    void set_config();
    void set_text();
    void init_sound(nw4r::snd::SoundArchive* archive, int loadGroup);
    void play_sound(int id);
    void init_volume();
    void init_sound2();
    void init_battery(const HBMControllerData* controller);
    void updateControllerMenu(int chan);
    void update(const HBMControllerData* controller);
    void setSpeakerVol();
    void updateTrigPane();
    void startPointEvent(const char* paneName, void* data);
    void startLeftEvent(const char* paneName);
    void startTrigEvent(const char* paneName);
    int getPaneNo(const char* paneName);
    int findAnimator(int group, int anim);
    int findGroupAnimator(int group, int anim);
    bool isActive() const;
    bool isUpBarActive() const;
    bool isDownBarActive();
    void setForcusSE();
    void setAdjustFlag(int flag);
    void setSpeakerAlarm(int chan, int msec);
    void update_posController(int chan);
    void reset_battery();
    void init_vib();
    void reset_control();
    void init_msg();
    void setSimpleSyncAlarm(int type);
    void fadeout_sound(f32 gain);
    void reset_guiManager(int chan);
    void startBlackOut();
    void calc_battery(int chan);
    void reset_btn();
    void shutdownSound();
    void shutdownSoundArchives();

    void setVibFlag(bool flag) {
        WPADEnableMotor(flag);
    }

    void setReassignedFlag(bool flag) {
        mReassignedFlag = flag;
    }

    int mSequence;                                    // at 0x000
    const HBMDataInfo* mpHBInfo;                      // at 0x004
    int mButtonNum;                                   // at 0x008
    int mAnmNum;                                      // at 0x00C
    int mState;                                       // at 0x010
    int mSelectAnmNum;                                // at 0x014
    int mMsgCount;                                       // at 0x018
    int mPaneCounter[14];                             // at 0x01C
    int mPadDrawTime[WPAD_MAX_CONTROLLERS];           // at 0x054
    int mForcusSEWaitTime;                                       // at 0x064
    int mBar0AnmRev;                                       // at 0x068
    int mBar1AnmRev;                                       // at 0x06C
    int mBar0AnmRevHold;                                       // at 0x070
    int mBar1AnmRevHold;                                       // at 0x074
    int mGetPadInfoTime;                                       // at 0x078
    u8 mControllerFlag[WPAD_MAX_CONTROLLERS];         // at 0x07C
    int mVolumeNum;                                   // at 0x080
    bool mVibFlag;                                    // at 0x084
    bool mControlFlag;                                // at 0x085
    bool mLetterFlag;                                 // at 0x086
    bool mAdjustFlag;                                 // at 0x087
    bool mReassignedFlag;                             // at 0x088
    bool mSimpleSyncFlag;                             // at 0x089
    bool mNetEventFlag;                               // at 0x08A
    bool mInitFlag;                                   // at 0x08B
    bool mForceSttInitProcFlag;                       // at 0x08C
    bool mForceSttFadeInProcFlag;                     // at 0x08D
    bool mEndInitSoundFlag;                           // at 0x08E
    bool mForceStopSyncFlag;                          // at 0x08F
    bool mForceEndMsgAnmFlag;                         // at 0x090
    bool mStartBlackOutFlag;                          // at 0x091
    u8 unk092[0x94 - 0x92];                           // at 0x092
    int mSoundRetryCnt;                                       // at 0x094
    int mDialogFlag[4];                                // at 0x098
    char* mpLayoutName;                               // at 0x0A8
    char* mpAnmName;                                  // at 0x0AC
    int mSelectBtnNum;                                // at 0x0B0
    const wchar_t* mpText[7][6];                      // at 0x0B4
    WPADInfo mWpadInfo[WPAD_MAX_CONTROLLERS];         // at 0x15C
    WPADSyncDeviceCallback mSimpleSyncCallback;       // at 0x1BC
    f32 mOnPaneVibFrame[WPAD_MAX_CONTROLLERS];                 // at 0x1C0
    f32 mOnPaneVibWaitFrame[WPAD_MAX_CONTROLLERS];                 // at 0x1D0
    int mWaitStopMotorCount;                          // at 0x1E0
    int unk1E4;                                       // at 0x1E4
    nw4r::lyt::Layout* mpLayout;                      // at 0x1E8
    nw4r::lyt::Layout* mpCursorLayout[WPAD_MAX_CONTROLLERS];  // at 0x1EC
    nw4r::lyt::ArcResourceAccessor* mpResAccessor;    // at 0x1FC
    gui::PaneManager* mpPaneManager;                  // at 0x200
    HomeButtonEventHandler* mpHomeButtonEventHandler; // at 0x204
    nw4r::lyt::DrawInfo mDrawInfo;                    // at 0x208
    Controller* mpController[WPAD_MAX_CONTROLLERS];   // at 0x25C
    RemoteSpk* mpRemoteSpk;                           // at 0x26C
    GroupAnmController* mpAnmController[12];          // at 0x270
    GroupAnmController* mpGroupAnmController[0x4A];   // at 0x2A0
    GroupAnmController* mpPairGroupAnmController[15]; // at 0x3C8
    HomeButtonFader mFader;                           // at 0x404
    OSAlarm mAlarm[WPAD_MAX_CONTROLLERS];             // at 0x418
    OSAlarm mSpeakerAlarm[WPAD_MAX_CONTROLLERS];      // at 0x4D8
    OSAlarm mSimpleSyncAlarm;                         // at 0x598
    int mReConnectTime;                                  // at 0x5C8
    int mReConnectTime2;                                  // at 0x5CC
    nw4r::snd::SoundArchivePlayer* mpSoundArchivePlayer;  // at 0x5D0
    nw4r::snd::DvdSoundArchive* mpDvdSoundArchive;    // at 0x5D4
    nw4r::snd::MemorySoundArchive* mpSoundArchive;    // at 0x5D8
    nw4r::snd::NandSoundArchive* mpNandSoundArchive;  // at 0x5DC
    nw4r::snd::SoundHeap* mpSoundHeap;                // at 0x5E0
    nw4r::snd::SoundHandle* mpSoundHandle;            // at 0x5E4
    u16 mAppVolume[3];                                // at 0x5E8
    AXFXAllocHook mAppFxAlloc;                        // at 0x5F0
    AXFXFreeHook mAppFxFree;                          // at 0x5F4
    AXFX_REVERBHI mReverb;                            // at 0x5F8
    AXAuxCallback mAppAuxCallback;                    // at 0x758
    void* mpAppAuxContext;                            // at 0x75C
    f32 mFadeOutFrames;                               // at 0x760
    u8 unk764[0x768 - 0x764];                         // at 0x764
};

}  // namespace homebutton

#endif
