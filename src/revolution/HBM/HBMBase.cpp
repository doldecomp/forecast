#include <nw4r_compat/cmath>
#include <nw4r_compat/cstdarg>
#include <nw4r_compat/cstdio>
#include <nw4r_compat/cstring>
#include <nw4r_compat/cwchar>

#include <revolution/AX.h>
#include <revolution/GX.h>
#include <revolution/MEM.h>
#include <revolution/OS.h>
#include <revolution/VI.h>
#include <nw4r/snd.h>
#include <nw4r/ut/ut_lock.h>
#include <types.h>

// RVL_SDK Home Button Menu - the menu (homebutton::HomeButton) and its C API.
//
// The library is a singleton: HBMCreate() allocates one HomeButton from the
// allocator described by HBMDataInfo and stores it in sHbm; the other entry
// points are thin wrappers around that object.

#include <revolution/HBM/HBMBase.h>

using namespace homebutton;

char sHbmVersionString[] =
    "<< RVL_SDK - HBM \trelease build: May 16 2007 10:42:52 (0x4199_60726) >>";
const char* __HBMVersion = sHbmVersionString;

static MEMAllocator sHbmAllocator;
MEMAllocator* sHbmAllocatorPtr = &sHbmAllocator;

// Frame-heap allocator the sound archive is loaded from; the heap itself is
// the application's sound buffer (HBMCreateSound).
static MEMAllocator sHbmSoundAllocator;

static OSMutex sMutex;

static HomeButton* sHbm;

// AXFX allocation hooks HBM installs while the menu is open.
static void* HBMAllocMem(u32 size) {
    return MEMAllocFromAllocator(sHbmAllocatorPtr, size);
}

static void HBMFreeMem(void* block) {
    MEMFreeToAllocator(sHbmAllocatorPtr, block);
}

const char* scCursorLytName[] = {"P1_Def.brlyt", "P2_Def.brlyt", "P3_Def.brlyt", "P4_Def.brlyt"};
const char* scCursorPaneName[] = {"B_btnL_00", "B_btnL_01", "B_btnL_10", "B_btnL_11"};
const char* scCursorTextPaneName[] = {"T_btnL_00", "T_btnL_01", "T_btnL_10", "T_btnL_11"};
const char* scCursorTransPaneName[] = {"N_Trans"};
const char* scCursorRotPaneName[] = {"N_Rot"};
const char* scCursorSRotPaneName[] = {"N_SRot"};

const char* scGroupName[] = {
    "btnL_00_inOut", "btnL_01_inOut", "btnL_10_inOut", "btnL_11_inOut",
    "btnL_00_psh",   "btnL_01_psh",   "btnL_10_psh",   "btnL_11_psh",
};

const char* scAnimName[] = {"_cntBtn_in.brlan", "_cntBtn_psh.brlan", "_cntBtn_out.brlan"};

const char* scPairGroupAnimName[] = {
    "_ltrIcn_on.brlan",      "_optn_bar_psh.brlan", "_close_bar_psh.brlan", "_hmMenu_bar_in.brlan",
    "_hmMenu_bar_psh.brlan", "_link_msg_in.brlan",  "_link_msg_out.brlan",  "_cmn_msg_in.brlan",
    "_cmn_msg_out.brlan",    "_cntrl_up.brlan",     "_cntrl_wndw_opn.brlan", "_cntrl_dwn.brlan",
    "_hmMenu_bar_out.brlan", "_cmn_msg_rtrn.brlan", "_12btn_on.brlan",
};

const char* scPairGroupName[] = {
    "ltrIcn_on",   "optn_bar_psh", "close_bar_psh", "hmMenu_bar_in",  "hmMenu_bar_psh",
    "link_msg_in", "link_msg_out", "cmn_msg_in",    "cmn_msg_out",    "cntrl_up",
    "cntrl_wndw_opn", "cntrl_dwn", "hmMenu_bar_out", "cmn_msg_rtrn", "12btn_on",
};

const char* scGroupAnimName[] = {
    "_hmMenu_strt.brlan",     "_hmMenu_fnsh.brlan",     "_optn_bar_in.brlan",    "_optn_bar_out.brlan",
    "_optn_btn_in.brlan",     "_optn_btn_psh.brlan",    "_vb_btn_wht_psh.brlan", "_optn_btn_out.brlan",
    "_vb_btn_ylw_psh.brlan",  "_sound_gry.brlan",       "_sound_ylw.brlan",      "_cmn_msg_btn_in.brlan",
    "_cmn_msg_btn_out.brlan", "_cmn_msg_btn_psh.brlan", "_vb_btn_ylw_ylw.brlan", "_btry_wink.brlan",
    "_btry_gry.brlan",        "_btry_wht.brlan",        "_btry_wink_gry.brlan",  "_close_bar_in.brlan",
    "_close_bar_out.brlan",   "_btry_red.brlan",
};

const char* scGroupName2[] = {
    "hmMenu_strt",      "hmMenu_fnsh",      "hmMenuBck_strt",   "hmMenuBck_fnsh",   "optn_bar_in",
    "optn_bar_out",     "optnBtn_00_inOut", "optnBtn_01_inOut", "optnBtn_10_inOut", "optnBtn_11_inOut",
    "optnBtn_20_inOut", "optnBtn_00_psh",   "optnBtn_01_psh",   "optnBtn_10_psh",   "optnBtn_11_psh",
    "optnBtn_20_psh",   "optnBtn_10_cntrl", "msgBtn_00_inOut",  "msgBtn_01_inOut",  "msgBtn_00_psh",
    "msgBtn_01_psh",    "vol_00",           "vol_01",           "vol_02",           "vol_03",
    "vol_04",           "vol_05",           "vol_06",           "vol_07",           "vol_08",
    "vol_09",           "plyr_00",          "plyr_01",          "plyr_02",          "plyr_03",
};

const char* scFuncPaneName[] = {"let_icn_00", "N_plyr_00", "N_plyr_01", "N_plyr_02", "N_plyr_03"};

const char* scFuncTouchPaneName[] = {
    "B_btn_00",     "B_bar_10",     "B_optnBtn_00", "B_optnBtn_01", "B_optnBtn_10",
    "B_optnBtn_11", "B_optnBtn_20", "B_BtnA",       "B_BtnB",       "cntrl_00",
};

const char* scFuncTextPaneName[] = {"T_msg_00", "T_msg_01", "T_Dialog"};

static const char* scBatteryPaneName[WPAD_MAX_CONTROLLERS][4] = {
    {"btryPwr_00_0", "btryPwr_00_1", "btryPwr_00_2", "btryPwr_00_3"},
    {"btryPwr_01_0", "btryPwr_01_1", "btryPwr_01_2", "btryPwr_01_3"},
    {"btryPwr_02_0", "btryPwr_02_1", "btryPwr_02_2", "btryPwr_02_3"},
    {"btryPwr_03_0", "btryPwr_03_1", "btryPwr_03_2", "btryPwr_03_3"},
};

struct AnmTable {
    int grp;  // at 0x0
    int anm;  // at 0x4
};

static const AnmTable scAnmTable[12] = {
    {0, 0}, {0, 2}, {4, 1}, {1, 0}, {1, 2}, {5, 1},
    {2, 0}, {2, 2}, {6, 1}, {3, 0}, {3, 2}, {7, 1},
};

static const AnmTable scGroupAnmTable[0x4A] = {
    {0, 0}, {1, 1}, {2, 0}, {3, 1},
    {4, 2}, {4, 19}, {5, 3}, {5, 20},
    {6, 4}, {6, 7}, {7, 4}, {7, 7},
    {8, 4}, {8, 7}, {9, 4}, {9, 7},
    {10, 4}, {10, 7}, {11, 5}, {12, 5},
    {13, 6}, {13, 8}, {14, 14}, {14, 6},
    {14, 8}, {15, 5}, {16, 6}, {16, 14},
    {17, 11}, {17, 12}, {18, 11}, {18, 12},
    {19, 13}, {20, 13}, {21, 9}, {21, 10},
    {22, 9}, {22, 10}, {23, 9}, {23, 10},
    {24, 9}, {24, 10}, {25, 9}, {25, 10},
    {26, 9}, {26, 10}, {27, 9}, {27, 10},
    {28, 9}, {28, 10}, {29, 9}, {29, 10},
    {30, 9}, {30, 10}, {31, 15}, {31, 16},
    {31, 17}, {31, 18}, {31, 21}, {32, 15},
    {32, 16}, {32, 17}, {32, 18}, {32, 21},
    {33, 15}, {33, 16}, {33, 17}, {33, 18},
    {33, 21}, {34, 15}, {34, 16}, {34, 17},
    {34, 18}, {34, 21},
};

// How the application wants HBM to allocate: with its own allocator, with the
// work buffer HBM turns into a heap itself, or with the lyt allocator.
static inline s32 GetAllocatorMode(const HBMDataInfo* info) {
    s32 mode;

    if (info->pAllocator != NULL)
        return 0;

    mode = 2;
    if (info->mem != NULL)
        mode = 1;

    return mode;
}

namespace homebutton {

static void initgx();
static void SpeakerAlarmCallback(OSAlarm* alarm, OSContext* context);
static void SimpleSyncAlarmCallback(OSAlarm* alarm, OSContext* context);
void stopMotorCallback(OSAlarm* alarm);
void netEventCallback(s32 event, s32 arg);

}  // namespace homebutton

inline void HomeButtonFader::draw() {
    u8 alpha = 255 * frame / maxFrame;
    initgx();

    GXColor color;
    color.r = r;
    color.g = g;
    color.b = b;
    color.a = alpha;
    GXSetTevColor(GX_TEVREG0, color);

    GXBegin(GX_QUADS, GX_VTXFMT0, 4);
    GXPosition2f32(-1000.0f, -1000.0f);
    GXPosition2f32(-1000.0f, 1000.0f);
    GXPosition2f32(1000.0f, 1000.0f);
    GXPosition2f32(1000.0f, -1000.0f);
    GXEnd();
}

inline void HomeButton::draw() {
    mpLayout->Draw(mDrawInfo);
    if (!mpHBInfo->cursor) {
        for (int i = WPAD_MAX_CONTROLLERS - 1; i >= 0; i--) {
            mpCursorLayout[i]->Draw(mDrawInfo);
        }
    }

    mFader.draw();
}

inline void HomeButton::play_sound(int id) {
    int ret = 0;

    if (mpHBInfo->sound_callback != NULL) {
        ret = mpHBInfo->sound_callback(5, id);
    }

    if (ret == 0 && mpSoundArchivePlayer != NULL && mpSoundHandle != NULL) {
        nw4r::ut::detail::AutoLock<OSMutex> lock(sMutex);
        mpSoundHandle->DetachSound();
        mpSoundArchivePlayer->StartSound(mpSoundHandle, id);
    }
}

//
// C API
//

extern "C" void HBMCreate(const HBMDataInfo* info) {
    if (GetAllocatorMode(info) == 1) {
        MEMiHeapHead* heap = MEMCreateExpHeapEx(info->mem, info->memSize, 0);
        MEMInitAllocatorForExpHeap(&sHbmAllocator, heap, 32);
        sHbmAllocatorPtr = &sHbmAllocator;
    }

    switch (GetAllocatorMode(info)) {
    case 0:
        nw4r::lyt::Layout::SetAllocator(info->pAllocator);
        sHbmAllocatorPtr = info->pAllocator;
        break;
    case 1:
        nw4r::lyt::Layout::SetAllocator(sHbmAllocatorPtr);
        break;
    case 2:
        sHbmAllocatorPtr = nw4r::lyt::Layout::GetAllocator();
        break;
    }

    void* buf = MEMAllocFromAllocator(sHbmAllocatorPtr, sizeof(HomeButton));
    if (buf != NULL) {
        sHbm = new (buf) HomeButton(info);
    }
    sHbm->create();
}

extern "C" void HBMDelete(void) {
    const HBMDataInfo* info = sHbm->mpHBInfo;

    sHbm->~HomeButton();
    MEMFreeToAllocator(sHbmAllocatorPtr, sHbm);
    sHbm = NULL;

    if (GetAllocatorMode(info) == 1)
        MEMDestroyExpHeap(sHbmAllocatorPtr->heap);
}

extern "C" void HBMInit(void) {
    OSRegisterVersion(__HBMVersion);
    sHbm->init();
}

extern "C" s32 HBMCalc(const HBMControllerData* controllerData) {
    sHbm->calc(controllerData);

    if (sHbm->mState != 0x12)
        return -1;

    return sHbm->mSelectBtnNum;
}

extern "C" void HBMDraw(void) {
    sHbm->draw();
}

extern "C" s32 HBMGetSelectBtnNum(void) {
    if (sHbm->mState != 0x12)
        return -1;

    return sHbm->mSelectBtnNum;
}

extern "C" void HBMSetAdjustFlag(u8 widescreen) {
    sHbm->setAdjustFlag(widescreen);
}

extern "C" void HBMStartBlackOut(void) {
    sHbm->startBlackOut();
}

extern "C" void HBMPlaySound(int id) {
    sHbm->play_sound(id);
}

extern "C" void HBMUpdateSoundArchivePlayer(void) {
    HomeButton* hbm = sHbm;

    if (hbm->mpSoundArchivePlayer != NULL) {
        nw4r::ut::detail::AutoLock<OSMutex> lock(sMutex);
        hbm->mpSoundArchivePlayer->Update();
    }
}

extern "C" void HBMSetSoundVolume(f32 volume) {
    u32 i;
    HomeButton* hbm = sHbm;

    AXSetAuxAReturnVolume(32768.0f * volume);

    if (hbm->mpSoundArchivePlayer != NULL) {
        nw4r::ut::detail::AutoLock<OSMutex> lock(sMutex);
        for (i = 0; i < hbm->mpSoundArchivePlayer->GetSoundPlayerCount(); i++) {
            hbm->mpSoundArchivePlayer->GetSoundPlayer(i).SetVolume(volume);
        }
    }
}

extern "C" void HBMDeleteSoundArchivePlayer(void) {
    sHbm->shutdownSound();
}

extern "C" void HBMCreateSound(void* soundData, void* memBuf, u32 memSize) {
    MEMiHeapHead* heap = MEMCreateFrmHeapEx(memBuf, memSize, 0);
    MEMInitAllocatorForFrmHeap(&sHbmSoundAllocator, heap, 32);

    HomeButton* hbm = sHbm;
    nw4r::ut::detail::AutoLock<OSMutex> lock(sMutex);

    void* buf = MEMAllocFromAllocator(&sHbmSoundAllocator, sizeof(nw4r::snd::MemorySoundArchive));
    if (buf != NULL) {
        hbm->mpSoundArchive = new (buf) nw4r::snd::MemorySoundArchive();
    }
    hbm->mpSoundArchive->Setup(soundData);
    hbm->init_sound(hbm->mpSoundArchive, 0);

}

extern "C" void HBMDeleteSound(void) {
    sHbm->shutdownSoundArchives();
    MEMDestroyFrmHeap(sHbmSoundAllocator.heap);
}

extern "C" void HBMUpdateSound(void) {
    HomeButton* hbm = sHbm;

    if (hbm->mpSoundArchivePlayer != NULL) {
        nw4r::ut::detail::AutoLock<OSMutex> lock(sMutex);
        hbm->mpSoundArchivePlayer->Update();
    }

    for (int i = 0; i < WPAD_MAX_CONTROLLERS; i++) {
        hbm->mpController[i]->updateSound();
    }
}

//
// HomeButton
//

HomeButton::HomeButton(const HBMDataInfo* info) : mpHBInfo(info), mpLayout(NULL), mpPaneManager(NULL) {
    int i;

    mFader.frame = 0;
    mFader.maxFrame = 30;
    mFader.state = 0;
    mFader.r = 0;
    mFader.g = 0;
    mFader.b = 0;

    mState = 2;
    mSelectBtnNum = -1;
    mSelectAnmNum = -1;
    mMsgCount = 0;
    mSequence = 0;
    mForcusSEWaitTime = 0;
    mLetterFlag = false;
    mBar0AnmRev = 0;
    mBar1AnmRev = 0;
    mBar0AnmRevHold = 0;
    mBar1AnmRevHold = 0;
    mAdjustFlag = false;
    mReassignedFlag = false;

    for (i = 0; i < WPAD_MAX_CONTROLLERS; i++) {
        OSCreateAlarm(&mAlarm[i]);
        OSCreateAlarm(&mSpeakerAlarm[i]);
    }
    OSCreateAlarm(&mSimpleSyncAlarm);
    OSInitMutex(&sMutex);

    mpSoundArchivePlayer = NULL;
    mpDvdSoundArchive = NULL;
    mpSoundArchive = NULL;
    mpNandSoundArchive = NULL;
    mpSoundHeap = NULL;
    mpSoundHandle = NULL;
}

HomeButton::~HomeButton() {
    mpResAccessor->~ArcResourceAccessor();
    MEMFreeToAllocator(sHbmAllocatorPtr, mpResAccessor);

    mpLayout->~Layout();
    MEMFreeToAllocator(sHbmAllocatorPtr, mpLayout);

    if (!mpHBInfo->cursor) {
        for (int i = 0; i < WPAD_MAX_CONTROLLERS; i++) {
            mpCursorLayout[i]->~Layout();
            MEMFreeToAllocator(sHbmAllocatorPtr, mpCursorLayout[i]);
        }
    }

    for (int i = 0; i < mAnmNum; i++) {
        mpAnmController[i]->~GroupAnmController();
        MEMFreeToAllocator(sHbmAllocatorPtr, mpAnmController[i]);
    }

    for (int i = 0; i < 15; i++) {
        mpPairGroupAnmController[i]->~GroupAnmController();
        MEMFreeToAllocator(sHbmAllocatorPtr, mpPairGroupAnmController[i]);
    }

    for (int i = 0; i < 0x4A; i++) {
        mpGroupAnmController[i]->~GroupAnmController();
        MEMFreeToAllocator(sHbmAllocatorPtr, mpGroupAnmController[i]);
    }

    MEMFreeToAllocator(sHbmAllocatorPtr, mpHomeButtonEventHandler);

    mpPaneManager->~PaneManager();
    MEMFreeToAllocator(sHbmAllocatorPtr, mpPaneManager);

    for (int i = 0; i < WPAD_MAX_CONTROLLERS; i++) {
        mpController[i]->~Controller();
        MEMFreeToAllocator(sHbmAllocatorPtr, mpController[i]);
    }

    mpRemoteSpk->~RemoteSpk();
    MEMFreeToAllocator(sHbmAllocatorPtr, mpRemoteSpk);
    mpRemoteSpk = NULL;

    MEMFreeToAllocator(sHbmAllocatorPtr, mpLayoutName);
    MEMFreeToAllocator(sHbmAllocatorPtr, mpAnmName);

    for (int i = 0; i < WPAD_MAX_CONTROLLERS; i++) {
        OSCancelAlarm(&mAlarm[i]);
        OSCancelAlarm(&mSpeakerAlarm[i]);
    }
    OSCancelAlarm(&mSimpleSyncAlarm);
}

// Puts the menu's messages, each up to its closing quote, into the text panes.
inline void HomeButton::init_msg() {
    int i, len;

    for (i = 0; i < 3; i++) {
        nw4r::lyt::Pane* pane = mpLayout->GetRootPane()->FindPaneByName(scFuncTextPaneName[i], true);
        nw4r::lyt::TextBox* textBox = nw4r::ut::DynamicCast<nw4r::lyt::TextBox*>(pane);

        for (len = 0;; len++) {
            if (mpText[mpHBInfo->region][i][len] == L'"') {
                break;
            }
        }

        textBox->SetString(mpText[mpHBInfo->region][i], 0, len);
    }
}

void HomeButton::create() {
    char path[0x40];
    int i;

    mInitFlag = false;
    mForceSttInitProcFlag = false;
    mForceSttFadeInProcFlag = false;

    set_config();
    set_text();

    void* buf = MEMAllocFromAllocator(sHbmAllocatorPtr, sizeof(nw4r::lyt::ArcResourceAccessor));
    if (buf != NULL) {
        mpResAccessor = new (buf) nw4r::lyt::ArcResourceAccessor();
    }
    mpResAccessor->Attach(mpHBInfo->layoutBuf, "arc");

    if (!mpHBInfo->cursor) {
        for (i = 0; i < WPAD_MAX_CONTROLLERS; i++) {
            buf = MEMAllocFromAllocator(sHbmAllocatorPtr, sizeof(nw4r::lyt::Layout));
            if (buf != NULL) {
                mpCursorLayout[i] = new (buf) nw4r::lyt::Layout();
            }
            void* res = mpResAccessor->GetResource(0, scCursorLytName[i], NULL);
            mpCursorLayout[i]->Build(res, mpResAccessor);
        }
    }

    buf = MEMAllocFromAllocator(sHbmAllocatorPtr, sizeof(nw4r::lyt::Layout));
    if (buf != NULL) {
        mpLayout = new (buf) nw4r::lyt::Layout();
    }
    void* res = mpResAccessor->GetResource(0, mpLayoutName, NULL);
    mpLayout->Build(res, mpResAccessor);

    nw4r::lyt::Pane* rootPane = mpLayout->GetRootPane();

    NW4R_UT_LINKLIST_FOREACH(it, rootPane->GetChildList(), {
        if (strcmp(it->GetName(), "back_01") == 0) {
            it->SetScale(nw4r::math::VEC2(1.2f, 1.2f));
            break;
        }
    })

    for (i = 0; i < mAnmNum; i++) {
        strcpy(path, mpAnmName);
        strcat(path, scAnimName[scAnmTable[i].anm]);
        void* anmRes = mpResAccessor->GetResource(0, path, NULL);

        buf = MEMAllocFromAllocator(sHbmAllocatorPtr, sizeof(GroupAnmController));
        if (buf != NULL) {
            mpAnmController[i] = new (buf) GroupAnmController();
        }
        mpAnmController[i]->mpAnimGroup = mpLayout->CreateAnimTransform(anmRes, mpResAccessor);
        mpAnmController[i]->mpGroup = mpLayout->GetGroupContainer()->FindGroupByName(scGroupName[scAnmTable[i].grp]);

        nw4r::lyt::detail::PaneLinkList& paneList = mpAnmController[i]->mpGroup->GetPaneList();
        for (nw4r::lyt::detail::PaneLinkList::Iterator it = paneList.GetBeginIter(); it != paneList.GetEndIter(); it++) {
            it->mTarget->BindAnimation(mpAnmController[i]->mpAnimGroup, false);
        }

        mpAnmController[i]->init(FrameController::ANM_TYPE_FORWARD, mpAnmController[i]->mpAnimGroup->GetFrameSize(), 0.0f,
                                 mpHBInfo->frameDelta);
    }

    for (i = 0; i < 0x4A; i++) {
        strcpy(path, mpAnmName);
        strcat(path, scGroupAnimName[scGroupAnmTable[i].anm]);
        void* anmRes = mpResAccessor->GetResource(0, path, NULL);

        buf = MEMAllocFromAllocator(sHbmAllocatorPtr, sizeof(GroupAnmController));
        if (buf != NULL) {
            mpGroupAnmController[i] = new (buf) GroupAnmController();
        }
        mpGroupAnmController[i]->mpAnimGroup = mpLayout->CreateAnimTransform(anmRes, mpResAccessor);
        mpGroupAnmController[i]->mpGroup =
            mpLayout->GetGroupContainer()->FindGroupByName(scGroupName2[scGroupAnmTable[i].grp]);

        nw4r::lyt::detail::PaneLinkList& paneList = mpGroupAnmController[i]->mpGroup->GetPaneList();
        for (nw4r::lyt::detail::PaneLinkList::Iterator it = paneList.GetBeginIter(); it != paneList.GetEndIter(); it++) {
            it->mTarget->BindAnimation(mpGroupAnmController[i]->mpAnimGroup, false);
        }

        mpGroupAnmController[i]->init(FrameController::ANM_TYPE_FORWARD,
                                      mpGroupAnmController[i]->mpAnimGroup->GetFrameSize(), 0.0f, mpHBInfo->frameDelta);
    }

    for (i = 0; i < 15; i++) {
        strcpy(path, mpAnmName);
        strcat(path, scPairGroupAnimName[i]);
        void* anmRes = mpResAccessor->GetResource(0, path, NULL);

        buf = MEMAllocFromAllocator(sHbmAllocatorPtr, sizeof(GroupAnmController));
        if (buf != NULL) {
            mpPairGroupAnmController[i] = new (buf) GroupAnmController();
        }
        mpPairGroupAnmController[i]->mpAnimGroup = mpLayout->CreateAnimTransform(anmRes, mpResAccessor);
        mpPairGroupAnmController[i]->mpGroup = mpLayout->GetGroupContainer()->FindGroupByName(scPairGroupName[i]);

        nw4r::lyt::detail::PaneLinkList& paneList = mpPairGroupAnmController[i]->mpGroup->GetPaneList();
        for (nw4r::lyt::detail::PaneLinkList::Iterator it = paneList.GetBeginIter(); it != paneList.GetEndIter(); it++) {
            it->mTarget->BindAnimation(mpPairGroupAnmController[i]->mpAnimGroup, false);
        }

        mpPairGroupAnmController[i]->init(FrameController::ANM_TYPE_FORWARD,
                                          mpPairGroupAnmController[i]->mpAnimGroup->GetFrameSize(), 0.0f,
                                          mpHBInfo->frameDelta);
    }

    buf = MEMAllocFromAllocator(sHbmAllocatorPtr, sizeof(HomeButtonEventHandler));
    if (buf != NULL) {
        mpHomeButtonEventHandler = new (buf) HomeButtonEventHandler(this);
    }

    buf = MEMAllocFromAllocator(sHbmAllocatorPtr, sizeof(gui::PaneManager));
    if (buf != NULL) {
        mpPaneManager = new (buf) gui::PaneManager(mpHomeButtonEventHandler, NULL, sHbmAllocatorPtr);
    }
    mpPaneManager->createLayoutScene(*mpLayout);
    mpPaneManager->setAllComponentTriggerTarget(false);

    for (i = 0; i < mButtonNum; i++) {
        nw4r::lyt::Pane* pane = mpLayout->GetRootPane()->FindPaneByName(scCursorPaneName[i], true);
        mpPaneManager->getPaneComponentByPane(pane)->setTriggerTarget(true);
    }

    buf = MEMAllocFromAllocator(sHbmAllocatorPtr, sizeof(RemoteSpk));
    if (buf != NULL) {
        mpRemoteSpk = new (buf) RemoteSpk(mpHBInfo->spkSeBuf);
    }

    for (i = 0; i < WPAD_MAX_CONTROLLERS; i++) {
        buf = MEMAllocFromAllocator(sHbmAllocatorPtr, sizeof(Controller));
        if (buf != NULL) {
            mpController[i] = new (buf) Controller(i, mpRemoteSpk);
        }
    }

    mpPaneManager->setDrawInfo(&mDrawInfo);

    nw4r::math::VEC2 adjustScale(1.0f / mpHBInfo->adjust.x, 1.0f);
    mDrawInfo.SetLocationAdjustScale(adjustScale);
    mDrawInfo.SetLocationAdjust(mAdjustFlag);

    nw4r::math::MTX34 viewMtx;
    PSMTXIdentity(viewMtx);
    mDrawInfo.SetViewMtx(viewMtx);

    init_msg();
}

void HomeButton::set_config() {
    const char* config = (const char*)mpHBInfo->configBuf;
    const char* end = config + mpHBInfo->configBufSize;
    int layoutLen;
    int anmLen;
    int i;
    int num;

    for (layoutLen = 0; config[layoutLen] != '\0'; layoutLen++) {
        if (config[layoutLen] == ',') {
            break;
        }
    }
    mpLayoutName = (char*)MEMAllocFromAllocator(sHbmAllocatorPtr, layoutLen + 1);
    strncpy(mpLayoutName, config, layoutLen);
    mpLayoutName[layoutLen] = '\0';
    config += layoutLen + 1;

    for (anmLen = 0; config[anmLen] != '\0'; anmLen++) {
        if (config[anmLen] == ',') {
            break;
        }
    }
    mpAnmName = (char*)MEMAllocFromAllocator(sHbmAllocatorPtr, anmLen + 1);
    strncpy(mpAnmName, config, anmLen);
    mpAnmName[anmLen] = '\0';
    config += anmLen;

    i = 0;
    num = 0;
    for (; config[i] != '\0' && &config[i] < end; i++) {
        if (config[i] == ',') {
            if (config[i + 1] == '1') {
                mDialogFlag[num] = 1;
            } else {
                mDialogFlag[num] = 0;
            }
            num++;
        }
    }

    mButtonNum = num;
    mAnmNum = num * 3;
}

void HomeButton::set_text() {
    int i = 0;
    int lang = 0;
    int index = 0;
    bool inString = false;
    const wchar_t* message = (const wchar_t*)mpHBInfo->msgBuf;

    for (; message[i] != L'\0'; i++) {
        if (message[i] == L'"') {
            if (!inString) {
                mpText[lang][index] = &message[i + 1];
                inString = true;
                if (++lang == 7) {
                    lang = 0;
                    index++;
                }
            } else {
                inString = false;
            }
        }
    }
}

void HomeButton::init() {
    int i;

    if (mInitFlag) {
        return;
    }

    mInitFlag = true;
    mForceSttInitProcFlag = false;
    mForceSttFadeInProcFlag = false;
    mStartBlackOutFlag = false;
    mForceStopSyncFlag = false;
    mSimpleSyncCallback = NULL;
    mReConnectTime = 3600.0f / sHbm->mpHBInfo->frameDelta;
    mReConnectTime2 = 3570.0f / sHbm->mpHBInfo->frameDelta;

    if (mSelectBtnNum != 3) {
        mEndInitSoundFlag = false;
    }

    for (i = 0; i < 14; i++) {
        mPaneCounter[i] = 0;
    }

    mState = 0;
    mSequence = 0;
    mReassignedFlag = false;

    mpPaneManager->init();
    updateTrigPane();

    mDrawInfo.SetViewRect(mpLayout->GetLayoutRect());
    mpLayout->CalculateMtx(mDrawInfo);
    for (i = 0; i < WPAD_MAX_CONTROLLERS; i++) {
        mpCursorLayout[i]->CalculateMtx(mDrawInfo);
    }

    reset_guiManager(-1);

    for (int i = 0; i < WPAD_MAX_CONTROLLERS; i++) {
        mPadDrawTime[i] = 0;
        mpController[i]->resetVolumeLimits();
        mpController[i]->resetCounters();
        mpController[i]->noOp0();
        mpController[i]->clearPlayerFlag();
        mpController[i]->initCallback();
        mpController[i]->clearSoundFlags();
        mOnPaneVibFrame[i] = 0.0f;
        mOnPaneVibWaitFrame[i] = 0.0f;
    }

    mpLayout->GetRootPane()->FindPaneByName(scFuncPaneName[0], true)->SetVisible(false);
    for (i = 2; i < 7; i++) {
        mpLayout->GetRootPane()->FindPaneByName(scFuncTouchPaneName[i], true)->SetVisible(false);
    }
    for (i = 0; i < 3; i++) {
        mpLayout->GetRootPane()->FindPaneByName(scFuncTextPaneName[i], true)->SetVisible(false);
    }

    mpRemoteSpk->Start();

    if (mpSoundArchivePlayer != NULL) {
        nw4r::ut::detail::AutoLock<OSMutex> lock(sMutex);
        for (i = 0; i < mpSoundArchivePlayer->GetSoundPlayerCount(); i++) {
            mpSoundArchivePlayer->GetSoundPlayer(i).SetVolume(1.0f);
        }
    }

    calc(NULL);

    mFader.init(30.0f / sHbm->mpHBInfo->frameDelta);
}

void HomeButton::init_volume() {
    mVolumeNum = 0.9f + 0.07874016f * WPADGetSpeakerVolume();
    WPADSetSpeakerVolume(0x7F);

    for (int i = 0; i < WPAD_MAX_CONTROLLERS; i++) {
        mpController[i]->setVolume(mVolumeNum / 10.0f);
        mpController[i]->connectSpeaker();
    }

    for (int i = 0; i < 10; i++) {
        if (i < mVolumeNum) {
            mpGroupAnmController[findGroupAnimator(i + 21, 10)]->start();
        } else {
            mpGroupAnmController[findGroupAnimator(i + 21, 9)]->start();
        }
    }
}

void HomeButton::init_sound2() {
    if (mpHBInfo->sound_callback != NULL) {
        mpHBInfo->sound_callback(0, 0);
    }

    mAppVolume[0] = AXGetAuxAReturnVolume();
    mAppVolume[1] = AXGetAuxBReturnVolume();
    mAppVolume[2] = AXGetAuxCReturnVolume();
    AXFXGetHooks(&mAppFxAlloc, &mAppFxFree);
    AXGetAuxACallback(&mAppAuxCallback, &mpAppAuxContext);
    AXFXSetHooks(HBMAllocMem, HBMFreeMem);

    mReverb.preDelay = 0.0f;
    mReverb.time = 2.5f;
    mReverb.coloration = 0.5f;
    mReverb.damping = 0.0f;
    mReverb.crosstalk = 0.0f;
    mReverb.mix = 1.0f;
    AXFXReverbHiInit(&mReverb);
    AXRegisterAuxACallback(AXFXReverbHiCallback, &mReverb);
    AXSetAuxAReturnVolume(0x8000);
    AXSetAuxBReturnVolume(0);
    AXSetAuxCReturnVolume(0);

    if (mpHBInfo->sound_callback != NULL) {
        mpHBInfo->sound_callback(1, 0);
    }

    mEndInitSoundFlag = true;
}

// Hides every controller's battery icons.
inline void HomeButton::reset_battery() {
    for (int i = 0; i < WPAD_MAX_CONTROLLERS; i++) {
        for (int j = 0; j < 4; j++) {
            mpLayout->GetRootPane()->FindPaneByName(scBatteryPaneName[i][j], true)->SetVisible(false);
        }
    }
}

void HomeButton::init_battery(const HBMControllerData* controller) {
    for (int i = 0; i < WPAD_MAX_CONTROLLERS; i++) {
        if (controller->wiiCon[i].kpad != NULL) {
            if (!mpHBInfo->cursor) {
                mpCursorLayout[i]->GetRootPane()->FindPaneByName(scCursorTransPaneName[0], true)->SetVisible(true);
            }
            mpGroupAnmController[findGroupAnimator(i + 31, 17)]->start();
            mControllerFlag[i] = true;
            mpController[i]->getInfoAsync(&mWpadInfo[i]);
        } else {
            if (!mpHBInfo->cursor) {
                mpCursorLayout[i]->GetRootPane()->FindPaneByName(scCursorTransPaneName[0], true)->SetVisible(false);
            }
            mpGroupAnmController[findGroupAnimator(i + 31, 16)]->start();
            mControllerFlag[i] = false;
        }
    }

    reset_battery();
    mGetPadInfoTime = 0;
}

inline int HomeButton::findAnimator(int group, int anim) {
    for (int i = 0; i < mAnmNum; i++) {
        if (scAnmTable[i].grp == group && scAnmTable[i].anm == anim) {
            return i;
        }
    }

    return -1;
}

inline int HomeButton::findGroupAnimator(int group, int anim) {
    for (int i = 0; i < 0x4A; i++) {
        if (scGroupAnmTable[i].grp == group && scGroupAnmTable[i].anm == anim) {
            return i;
        }
    }

    return -1;
}

inline bool HomeButton::isActive() const {
    return mState == 2;
}

inline bool HomeButton::isUpBarActive() const {
    bool flag = true;

    if (!isActive() || mpPairGroupAnmController[12]->mState == FrameController::STATE_PLAY ||
        mpPairGroupAnmController[3]->mState == FrameController::STATE_PLAY) {
        flag = false;
    }

    return flag;
}

inline bool HomeButton::isDownBarActive() {
    bool flag = true;

    int idx[4];
    idx[0] = findGroupAnimator(4, 2);
    idx[1] = findGroupAnimator(5, 3);
    idx[2] = findGroupAnimator(4, 19);
    idx[3] = findGroupAnimator(5, 20);

    if (!isActive() || mpGroupAnmController[idx[0]]->mState == FrameController::STATE_PLAY ||
        mpGroupAnmController[idx[1]]->mState == FrameController::STATE_PLAY ||
        mpGroupAnmController[idx[2]]->mState == FrameController::STATE_PLAY ||
        mpGroupAnmController[idx[3]]->mState == FrameController::STATE_PLAY) {
        flag = false;
    }

    return flag;
}

inline void HomeButton::setForcusSE() {
    if (mForcusSEWaitTime <= 2) {
        return;
    }

    play_sound(4);
    mForcusSEWaitTime = 0;
}

// Starts the highlight animation of the five controller-menu buttons.
inline void HomeButton::reset_control() {
    int anm_no;

    for (int i = 0; i < 5; i++) {
        anm_no = findGroupAnimator(i + 6, 7);
        mpGroupAnmController[anm_no]->start();
    }
}

// Retries WPADStartSimpleSync (type 0) or WPADStopSimpleSync (1) in 100 ms.
inline void HomeButton::setSimpleSyncAlarm(int type) {
    OSCancelAlarm(&mSimpleSyncAlarm);
    OSSetAlarmUserData(&mSimpleSyncAlarm, (void*)type);
    OSSetAlarm(&mSimpleSyncAlarm, OS_MSEC_TO_TICKS(100), SimpleSyncAlarmCallback);
}

// Starts the vibration on/off button animations for the current motor setting.
inline void HomeButton::init_vib() {
    if ((mVibFlag = WPADIsMotorEnabled())) {
        mpGroupAnmController[findGroupAnimator(13, 6)]->start();
        mpGroupAnmController[findGroupAnimator(14, 8)]->start();
    } else {
        mpGroupAnmController[findGroupAnimator(13, 8)]->start();
        mpGroupAnmController[findGroupAnimator(14, 6)]->start();
    }
}

// Sets the volume of the application's sounds while the menu fades out.
inline void HomeButton::fadeout_sound(f32 gain) {
    if (mSelectBtnNum == 3) {
        return;
    }

    if (mEndInitSoundFlag) {
        AXSetAuxAReturnVolume(32768.0f * gain);
    }

    if (mpSoundArchivePlayer != NULL) {
        nw4r::ut::detail::AutoLock<OSMutex> lock(sMutex);
        for (u32 i = 0; i < mpSoundArchivePlayer->GetSoundPlayerCount(); i++) {
            mpSoundArchivePlayer->GetSoundPlayer(i).SetVolume(gain);
        }
    }
}

void HomeButton::calc(const HBMControllerData* controller) {
    mpPaneManager->calc();

    for (int i = 0; i < mAnmNum; i++) {
        mpAnmController[i]->do_calc();
    }
    for (int i = 0; i < 15; i++) {
        mpPairGroupAnmController[i]->do_calc();
    }
    for (int i = 0; i < 0x4A; i++) {
        mpGroupAnmController[i]->do_calc();
    }

    for (int i = 0; i < WPAD_MAX_CONTROLLERS; i++) {
        if (mOnPaneVibFrame[i] > 0.0f) {
            if (!mControllerFlag[i] || !mpController[i]->mHBController.rumble) {
                mOnPaneVibFrame[i] = 0.0f;
                mOnPaneVibWaitFrame[i] = 0.0f;
                if (mpController[i]->mHBController.rumble) {
                    mpController[i]->stopMotor();
                }
            } else {
                mOnPaneVibFrame[i] -= mpHBInfo->frameDelta;
                if (mOnPaneVibFrame[i] <= 0.0f || mState == 17) {
                    mpController[i]->stopMotor();
                    mOnPaneVibFrame[i] = 0.0f;
                    mOnPaneVibWaitFrame[i] = 9.0f;
                }
            }
        } else if (mOnPaneVibWaitFrame[i] > 0.0f) {
            mOnPaneVibWaitFrame[i] -= mpHBInfo->frameDelta;
            if (mOnPaneVibWaitFrame[i] <= 0.0f) {
                mOnPaneVibWaitFrame[i] = 0.0f;
            }
        }
    }

    switch (mState) {
    case 0:
        if (mpHBInfo->backFlag) {
            mSelectAnmNum = findGroupAnimator(2, 0);
            mpLayout->GetRootPane()->FindPaneByName("back_00", true)->SetVisible(false);
            mpLayout->GetRootPane()->FindPaneByName("back_02", true)->SetVisible(true);
        } else {
            mSelectAnmNum = findGroupAnimator(0, 0);
            mpLayout->GetRootPane()->FindPaneByName("back_00", true)->SetVisible(true);
            mpLayout->GetRootPane()->FindPaneByName("back_02", true)->SetVisible(false);
        }
        mpGroupAnmController[mSelectAnmNum]->start();

        if (controller != NULL) {
            mState = 1;
            init_battery(controller);
        }
        break;
    case 1:
        if (mpGroupAnmController[mSelectAnmNum]->mState != FrameController::STATE_PLAY) {
            init_volume();

            init_vib();

            init_sound2();
            play_sound(0);
            mState = 2;
        }
        break;
    case 2:
        if (mLetterFlag && mpPairGroupAnmController[0]->mState != FrameController::STATE_PLAY) {
            mpLayout->GetRootPane()->FindPaneByName(scFuncPaneName[0], true)->SetVisible(true);
            mpPairGroupAnmController[0]->mAnmType = FrameController::ANM_TYPE_LOOP;
            mpPairGroupAnmController[0]->start();
        } else if (!mLetterFlag) {
            mpLayout->GetRootPane()->FindPaneByName(scFuncPaneName[0], true)->SetVisible(false);
            mpPairGroupAnmController[0]->stop();
        }
        break;
    case 3:
        if (mpGroupAnmController[mSelectAnmNum]->mState != FrameController::STATE_PLAY && mSelectAnmNum != 5) {
            reset_battery();

            mSelectAnmNum = 5;
            mpPairGroupAnmController[mSelectAnmNum]->start();
        }

        if (--mWaitStopMotorCount <= 0) {
            for (int i = 0; i < WPAD_MAX_CONTROLLERS; i++) {
                WPADDisconnect(i);
            }

            mState = 4;
            unk1E4 = 180.0f / sHbm->mpHBInfo->frameDelta;
        }
        break;
    case 4:
        if (mpGroupAnmController[mSelectAnmNum]->mState != FrameController::STATE_PLAY) {
            if (unk1E4 > 0) {
                s32 type;
                int i;
                for (i = 0; i < WPAD_MAX_CONTROLLERS; i++) {
                    if (WPADProbe(i, &type) != WPAD_ERR_NO_CONTROLLER) {
                        break;
                    }
                }

                if (i < WPAD_MAX_CONTROLLERS) {
                    unk1E4--;
                    break;
                }

                unk1E4 = 0;
            }

            mState = 5;
            mMsgCount = 0;
            mSoundRetryCnt = 0;
            mSimpleSyncCallback = WPADSetSimpleSyncCallback(netEventCallback);
            mNetEventFlag = false;
            mpController[0]->mMotorEnabled = true;
            mpController[1]->mMotorEnabled = true;
            mpController[2]->mMotorEnabled = true;
            mpController[3]->mMotorEnabled = true;

            if (!(mSimpleSyncFlag = WPADStartSimpleSync())) {
                setSimpleSyncAlarm(0);
            }
        }
        break;
    case 5:
    case 6:
        if (mSimpleSyncFlag && mpPairGroupAnmController[mSelectAnmNum]->mState != FrameController::STATE_PLAY) {
            if (mMsgCount == 0) {
                reset_control();

                reset_btn();
                mpPairGroupAnmController[14]->mAnmType = FrameController::ANM_TYPE_LOOP;
                mpPairGroupAnmController[14]->start();
            }

            if (mControllerFlag[3]) {
                if (mState != 6) {
                    if (mpController[3]->isPlayingSoundId(5)) {
                        mState = 6;
                        mMsgCount = mReConnectTime2;
                    }

                    if (++mSoundRetryCnt > mReConnectTime2) {
                        mMsgCount = mReConnectTime2;
                        mState = 6;
                    }
                } else if (++mMsgCount > mReConnectTime) {
                    mState = 7;
                }
            } else if (++mMsgCount > mReConnectTime) {
                mState = 7;

                if (!WPADStopSimpleSync()) {
                    setSimpleSyncAlarm(1);
                }

                mNetEventFlag = true;
            }
        }
        break;
    case 7:
        if (mNetEventFlag) {
            WPADSetSimpleSyncCallback(mSimpleSyncCallback);
            mpRemoteSpk->ClearPcm();
            reset_guiManager(-1);

            mSelectAnmNum = 6;
            mpPairGroupAnmController[mSelectAnmNum]->start();
            mState = 8;
            mpPairGroupAnmController[14]->mAnmType = FrameController::ANM_TYPE_FORWARD;
            play_sound(21);
        }
        break;
    case 8:
        if (mpPairGroupAnmController[mSelectAnmNum]->mState != FrameController::STATE_PLAY) {
            if (mSelectAnmNum == 13) {
                mpGroupAnmController[findGroupAnimator(17, 12)]->start();
                mpGroupAnmController[findGroupAnimator(18, 12)]->start();
                mpLayout->GetRootPane()->FindPaneByName(scFuncTextPaneName[2], true)->SetVisible(false);
            } else if (mSelectAnmNum == 6) {
                mpLayout->GetRootPane()->FindPaneByName(scFuncTextPaneName[0], true)->SetVisible(false);
                mpLayout->GetRootPane()->FindPaneByName(scFuncTextPaneName[1], true)->SetVisible(false);
            }

            mState = 2;
        }
        break;
    case 9:
        if (mpGroupAnmController[mSelectAnmNum]->mState != FrameController::STATE_PLAY) {
            if (mVibFlag) {
                for (int i = 0; i < WPAD_MAX_CONTROLLERS; i++) {
                    mpController[i]->stopMotor();
                }
            }

            mState = 2;
        }
        break;
    case 10:
        if (mpPairGroupAnmController[mSelectAnmNum]->mState != FrameController::STATE_PLAY) {
            mBar0AnmRev = 0;
            mBar1AnmRev = 0;
            mBar0AnmRevHold = 0;
            mBar1AnmRevHold = 0;

            if (mSequence != 1) {
                for (int i = 2; i < 7; i++) {
                    mpLayout->GetRootPane()->FindPaneByName(scFuncTouchPaneName[i], true)->SetVisible(false);
                }

                mState = 2;
            } else if (mSequence == 1) {
                mSelectAnmNum = 10;
                mpPairGroupAnmController[mSelectAnmNum]->start();
                mSelectAnmNum = 1;
                play_sound(7);
                mState = 8;
            }

            updateTrigPane();
        }
        break;
    case 11:
        if (mpAnmController[mSelectAnmNum]->mState != FrameController::STATE_PLAY) {
            mSelectAnmNum = 7;
            mpPairGroupAnmController[mSelectAnmNum]->start();
            mState = 12;
        }
        break;
    case 12:
        if (mpPairGroupAnmController[mSelectAnmNum]->mState != FrameController::STATE_PLAY) {
            updateTrigPane();
            reset_btn();
            mState = 2;
        }
        break;
    case 13:
        if (mpGroupAnmController[mSelectAnmNum]->mState != FrameController::STATE_PLAY) {
            if (mSelectBtnNum >= 0) {
                mFader.state = 1;
                mState = 19;
                mFadeOutFrames = mFader.maxFrame;

                if (mpHBInfo->sound_callback != NULL) {
                    mpHBInfo->sound_callback(mSelectBtnNum != 3 ? 3 : 6, mFadeOutFrames);
                }
            } else {
                updateTrigPane();
                mSelectAnmNum = 13;
                mpPairGroupAnmController[mSelectAnmNum]->start();
                mState = 8;
            }

            for (int i = 0; i < 8; i++) {
                mpPaneManager->update(i, -10000.0f, -10000.0f, 0, 0, 0, NULL);
            }
        }
        break;
    case 14:
        if (mpPairGroupAnmController[mSelectAnmNum]->mState != FrameController::STATE_PLAY) {
            mpLayout->GetRootPane()->FindPaneByName(scFuncTextPaneName[2], true)->SetVisible(false);

            if (mpHBInfo->backFlag) {
                mSelectAnmNum = findGroupAnimator(3, 1);
            } else {
                mSelectAnmNum = findGroupAnimator(1, 1);
            }

            mpGroupAnmController[mSelectAnmNum]->start();
            mState = 16;
            mFadeOutFrames = mpGroupAnmController[mSelectAnmNum]->mMaxFrame;

            if (mpHBInfo->sound_callback != NULL) {
                mpHBInfo->sound_callback(2, mFadeOutFrames);
            }
        }
        break;
    case 15:
        if (mpAnmController[mSelectAnmNum]->mState != FrameController::STATE_PLAY) {
            mFader.state = 1;
            mStartBlackOutFlag = true;
            mState = 19;
            mFadeOutFrames = mFader.maxFrame;

            if (mpHBInfo->sound_callback != NULL) {
                mpHBInfo->sound_callback(mSelectBtnNum != 3 ? 3 : 6, mFadeOutFrames);
            }
        }
        break;
    case 16: {
        GroupAnmController* anm = mpGroupAnmController[mSelectAnmNum];

        if (anm->mState != FrameController::STATE_PLAY) {
            mState = 17;

            if (mSelectBtnNum != 3) {
                if (mEndInitSoundFlag) {
                    AXSetAuxAReturnVolume(0);
                }

                if (mpSoundArchivePlayer != NULL) {
                    nw4r::ut::detail::AutoLock<OSMutex> lock(sMutex);
                    for (u32 i = 0; i < mpSoundArchivePlayer->GetSoundPlayerCount(); i++) {
                        mpSoundArchivePlayer->GetSoundPlayer(i).SetVolume(0.0f);
                    }
                }
            }
        } else {
            f32 restFrame = anm->getMaxFrame() - anm->getCurrentFrame();
            f32 volume = restFrame / mFadeOutFrames;

            if (mSelectBtnNum != 3) {
                if (mEndInitSoundFlag) {
                    AXSetAuxAReturnVolume(32768.0f * volume);
                }

                if (mpSoundArchivePlayer != NULL) {
                    nw4r::ut::detail::AutoLock<OSMutex> lock(sMutex);
                    for (u32 i = 0; i < mpSoundArchivePlayer->GetSoundPlayerCount(); i++) {
                        mpSoundArchivePlayer->GetSoundPlayer(i).SetVolume(volume);
                    }
                }
            }
        }
        break;
    }
    case 17:
        mState = 18;

        if (mSelectBtnNum != 3) {
            if (mpSoundArchivePlayer != NULL) {
                nw4r::ut::detail::AutoLock<OSMutex> lock(sMutex);
                for (u32 i = 0; i < mpSoundArchivePlayer->GetSoundPlayerCount(); i++) {
                    mpSoundArchivePlayer->GetSoundPlayer(i).StopAllSound(0);
                }
            }

            if (mEndInitSoundFlag) {
                AXFXReverbHiShutdown(&mReverb);
                AXRegisterAuxACallback(mAppAuxCallback, mpAppAuxContext);
                AXFXSetHooks(mAppFxAlloc, mAppFxFree);
                AXSetAuxAReturnVolume(mAppVolume[0]);
                AXSetAuxBReturnVolume(mAppVolume[1]);
                AXSetAuxCReturnVolume(mAppVolume[2]);
            }
        }

        WPADSetSpeakerVolume(12.7f * mVolumeNum);
        WPADSaveConfig(NULL);
        mpRemoteSpk->cancelAlarm();

        for (int i = 0; i < WPAD_MAX_CONTROLLERS; i++) {
            mpController[i]->clearCallback();
        }

        if (mSelectBtnNum != 3 && mpHBInfo->sound_callback != NULL) {
            mpHBInfo->sound_callback(4, 0);
        }

        mInitFlag = false;
        break;
    case 18:
        mState = 2;
        break;
    case 19:
        if (mForceSttInitProcFlag) {
            init_battery(controller);
            mForceSttInitProcFlag = false;
        }

        if (mForceSttFadeInProcFlag) {
            init_volume();

            init_vib();

            mForceSttFadeInProcFlag = false;
        }

        if (mFader.isDone()) {
            if (mForceStopSyncFlag) {
                if (!mNetEventFlag) {
                    break;
                }

                WPADSetSimpleSyncCallback(mSimpleSyncCallback);
                mForceStopSyncFlag = false;
            }

            if (mForceEndMsgAnmFlag) {
                mpPairGroupAnmController[5]->initFrame();
                mpPairGroupAnmController[5]->stop();
                mpLayout->GetRootPane()->FindPaneByName(scFuncTextPaneName[0], true)->SetVisible(false);
                mpLayout->GetRootPane()->FindPaneByName(scFuncTextPaneName[1], true)->SetVisible(false);

                if (mpPairGroupAnmController[14]->mState == FrameController::STATE_PLAY) {
                    mpPairGroupAnmController[14]->initFrame();
                    mpPairGroupAnmController[14]->stop();
                }
            }

            mState = 17;
            VISetBlack(TRUE);
            VIFlush();

            if (mSelectBtnNum != 3) {
                if (mEndInitSoundFlag) {
                    AXSetAuxAReturnVolume(0);
                }

                if (mpSoundArchivePlayer != NULL) {
                    nw4r::ut::detail::AutoLock<OSMutex> lock(sMutex);
                    for (u32 i = 0; i < mpSoundArchivePlayer->GetSoundPlayerCount(); i++) {
                        mpSoundArchivePlayer->GetSoundPlayer(i).SetVolume(0.0f);
                    }
                }
            }
        } else {
            f32 volume = (f32)(mFader.maxFrame - mFader.frame) / mFadeOutFrames;

            if (mSelectBtnNum != 3) {
                if (mEndInitSoundFlag) {
                    AXSetAuxAReturnVolume(32768.0f * volume);
                }

                if (mpSoundArchivePlayer != NULL) {
                    nw4r::ut::detail::AutoLock<OSMutex> lock(sMutex);
                    for (u32 i = 0; i < mpSoundArchivePlayer->GetSoundPlayerCount(); i++) {
                        mpSoundArchivePlayer->GetSoundPlayer(i).SetVolume(volume);
                    }
                }
            }
        }
        break;
    }

    if (mBar0AnmRev != 0) {
        if (isUpBarActive()) {
            if (mBar0AnmRev != 0 && mBar0AnmRev != mBar0AnmRevHold) {
                mpPairGroupAnmController[mBar0AnmRev]->start();
                mBar0AnmRevHold = mBar0AnmRev;
            }
            mBar0AnmRev = 0;
        }
    }

    if (mBar1AnmRev != 0) {
        if (isDownBarActive()) {
            if (mBar1AnmRev != 0 && mBar1AnmRev != mBar1AnmRevHold) {
                mpGroupAnmController[mBar1AnmRev]->start();
                mBar1AnmRevHold = mBar1AnmRev;
            }
            mBar1AnmRev = 0;
        }
    }

    if (controller != NULL) {
        update(controller);
    }

    mpLayout->Animate(0);
    mpLayout->CalculateMtx(mDrawInfo);
    if (!mpHBInfo->cursor) {
        for (int i = 0; i < WPAD_MAX_CONTROLLERS; i++) {
            mpCursorLayout[i]->CalculateMtx(mDrawInfo);
        }
    }

    if (mForcusSEWaitTime <= 2) {
        mForcusSEWaitTime++;
    }
}

void HomeButton::calc_battery(int chan) {
    for (int i = 0; i < 4; i++) {
        if (i < mWpadInfo[chan].battery) {
            mpLayout->GetRootPane()->FindPaneByName(scBatteryPaneName[chan][i], true)->SetVisible(true);
        } else {
            mpLayout->GetRootPane()->FindPaneByName(scBatteryPaneName[chan][i], true)->SetVisible(false);
        }
    }

    if (mWpadInfo[chan].battery < 2) {
        mpGroupAnmController[findGroupAnimator(chan + 31, 21)]->start();
    } else {
        mpGroupAnmController[findGroupAnimator(chan + 31, 17)]->start();
    }

    if (mGetPadInfoTime < 100) {
        mGetPadInfoTime = 0;
    }

    mpController[chan]->clearPlayerFlag();
}

inline HomeButton* HomeButton::getInstance() {
    return sHbm;
}

// Schedules SpeakerAlarmCallback for a controller in msec milliseconds.
inline void HomeButton::setSpeakerAlarm(int chan, int msec) {
    OSSetAlarmUserData(&mSpeakerAlarm[chan], (void*)((chan << 16) | (chan + 2)));
    OSCancelAlarm(&mSpeakerAlarm[chan]);
    OSSetAlarm(&mSpeakerAlarm[chan], OS_MSEC_TO_TICKS(msec), SpeakerAlarmCallback);
}

namespace homebutton {

// Plays a sound on a Wiimote speaker once the channel is ready; the user data
// is (channel << 16) | sound id.
static void SpeakerAlarmCallback(OSAlarm* alarm, OSContext* context) {
    u32 data = (u32)OSGetAlarmUserData(alarm);
    s32 chan = data >> 16;
    int id = data & 0xFFFF;
    HomeButton* hbm = HomeButton::getInstance();

    if (!WPADIsSpeakerEnabled(chan) || !hbm->mpController[chan]->isPlayReady()) {
        hbm->setSpeakerAlarm(chan, 50);
    } else {
        hbm->mpController[chan]->playSound(hbm->mpSoundArchivePlayer, id);
    }
}

void stopMotorCallback(OSAlarm* alarm) {
    ((Controller*)OSGetAlarmUserData(alarm))->stopMotor();
}

// Retries WPADStartSimpleSync (user data 0) or WPADStopSimpleSync (1).
static void SimpleSyncAlarmCallback(OSAlarm* alarm, OSContext* context) {
    HomeButton* hbm = sHbm;
    s32 type = (s32)OSGetAlarmUserData(alarm);
    bool done = false;

    if (type == 0) {
        if (WPADStartSimpleSync()) {
            hbm->mSimpleSyncFlag = true;
            done = true;
        }
    } else {
        if (WPADStopSimpleSync()) {
            done = true;
        }
    }

    if (!done) {
        hbm->setSimpleSyncAlarm(type);
    }
}

void netEventCallback(s32 event, s32 arg) {
    if (event == 1)
        sHbm->mNetEventFlag = true;

    if (sHbm->mSimpleSyncCallback != NULL)
        sHbm->mSimpleSyncCallback(event, arg);
}

}  // namespace homebutton

// Moves a controller's cursor to its pointer position on the layout.
inline void HomeButton::update_posController(int chan) {
    HBController* con = mpController[chan]->getController();
    nw4r::ut::Rect layoutRect = mpLayout->GetLayoutRect();

    f32 x = con->x * layoutRect.right;
    f32 y = con->y * layoutRect.bottom;
    nw4r::math::VEC2 pos(x, y);

    mpCursorLayout[chan]->GetRootPane()->FindPaneByName(scCursorTransPaneName[0], true)->SetTranslate(pos);
}

// Moves a controller's pointer off the panes; chan < 0 does all of them.
inline void HomeButton::reset_guiManager(int chan) {
    if (chan < 0) {
        for (int i = 0; i < 8; i++) {
            mpPaneManager->update(i, -10000.0f, -10000.0f, 0, 0, 0, NULL);
        }
    } else {
        mpPaneManager->update(chan, -10000.0f, -10000.0f, 0, 0, 0, NULL);
    }
}

void HomeButton::update(const HBMControllerData* controller) {
    mFader.calc();

    for (int i = 0; i < WPAD_MAX_CONTROLLERS; i++) {
        if (controller->wiiCon[i].kpad != NULL) {
            if (controller->wiiCon[i].kpad->wpad_err != WPAD_ERR_NO_CONTROLLER) {
                if (mPadDrawTime[i] > (int)(0.5f + 5.0f / sHbm->mpHBInfo->frameDelta)) {
                    if (controller->wiiCon[i].kpad->wpad_err == WPAD_ERR_OK) {
                        bool updatePos;
                        if (controller->wiiCon[i].use_devices != WPAD_DEV_CLASSIC &&
                            controller->wiiCon[i].kpad->dev_type != WPAD_DEV_CLASSIC) {
                            if (controller->wiiCon[i].kpad->dpd_valid_fg > 0) {
                                updatePos = true;
                            } else {
                                updatePos = false;
                            }
                        } else {
                            updatePos = true;
                        }

                        mpController[i]->setKpad(&controller->wiiCon[i], updatePos);
                        if (!mpHBInfo->cursor) {
                            mpCursorLayout[i]->GetRootPane()->FindPaneByName(scCursorTransPaneName[0], true)->SetVisible(true);
                        }
                    }
                } else {
                    mPadDrawTime[i]++;
                }

                if (controller->wiiCon[i].use_devices != WPAD_DEV_CLASSIC &&
                    controller->wiiCon[i].kpad->dev_type != WPAD_DEV_CLASSIC &&
                    controller->wiiCon[i].kpad->dpd_valid_fg <= 0) {
                    s32 type;
                    s32 result = WPADProbe(i, &type);
                    if (controller->wiiCon[i].kpad->wpad_err != WPAD_ERR_BUSY && result != WPAD_ERR_BUSY) {
                        mpController[i]->resetVolumeLimits();
                    }
                }
            } else {
                mpController[i]->resetVolumeLimits();
                if (!mpHBInfo->cursor) {
                    mpCursorLayout[i]->GetRootPane()->FindPaneByName(scCursorTransPaneName[0], true)->SetVisible(false);
                }
            }

            if (!mControllerFlag[i]) {
                mControllerFlag[i] = true;
                mpController[i]->getInfoAsync(&mWpadInfo[i]);
                mpGroupAnmController[findGroupAnimator(i + 31, 17)]->start();
                mpGroupAnmController[findGroupAnimator(i + 31, 18)]->stop();
                mpGroupAnmController[findGroupAnimator(i + 31, 15)]->start();
                play_sound(i + 17);

                mpController[i]->connectSpeaker();
                mpController[i]->startMotor();
                OSSetAlarmUserData(&mAlarm[i], mpController[i]);
                OSCancelAlarm(&mAlarm[i]);
                OSSetAlarm(&mAlarm[i], OS_MSEC_TO_TICKS(300), (OSAlarmHandler)stopMotorCallback);

                setSpeakerAlarm(i, 400);
            }

            if (controller->wiiCon[i].kpad->wpad_err == WPAD_ERR_OK) {
                nw4r::math::VEC3 rotate;

                if (controller->wiiCon[i].use_devices == WPAD_DEV_CLASSIC &&
                    controller->wiiCon[i].kpad->dev_type == WPAD_DEV_CLASSIC) {
                    rotate = nw4r::math::VEC3(0.0f, 0.0f, 15.0f);
                } else {
                    Vec2 horizon = controller->wiiCon[i].kpad->horizon;
                    f32 angle = nw4r::math::Atan2Deg(-horizon.y, horizon.x);
                    rotate = nw4r::math::VEC3(0.0f, 0.0f, angle);
                }

                if (!mpHBInfo->cursor) {
                    mpCursorLayout[i]->GetRootPane()->FindPaneByName(scCursorRotPaneName[0], true)->SetRotate(rotate);
                    mpCursorLayout[i]->GetRootPane()->FindPaneByName(scCursorSRotPaneName[0], true)->SetRotate(rotate);
                }

                if (mGetPadInfoTime > 100) {
                    mpController[i]->getInfoAsync(&mWpadInfo[i]);
                }

                updateControllerMenu(i);

                if (!mpHBInfo->cursor) {
                    update_posController(i);
                }
            }

            if (mpController[i]->getPlayerFlag()) {
                calc_battery(i);
            }
        } else {
            if (mControllerFlag[i]) {
                mpGroupAnmController[findGroupAnimator(i + 31, 17)]->start();
                mpGroupAnmController[findGroupAnimator(i + 31, 18)]->start();

                for (int j = 0; j < 4; j++) {
                    mpLayout->GetRootPane()->FindPaneByName(scBatteryPaneName[i][j], true)->SetVisible(false);
                }

                if (!mpHBInfo->cursor) {
                    mpCursorLayout[i]->GetRootPane()->FindPaneByName(scCursorTransPaneName[0], true)->SetVisible(false);
                }

                mPadDrawTime[i] = 0;
                mControllerFlag[i] = false;
                mpController[i]->resetVolumeLimits();
                mpController[i]->resetCounters();
                mpController[i]->noOp0();
            }

            reset_guiManager(i);
        }
    }

    if (mGetPadInfoTime > 100) {
        mGetPadInfoTime = 0;
    } else {
        mGetPadInfoTime++;
    }
}

void HomeButton::updateControllerMenu(int chan) {
    if (isActive()) {
        HBController* controller = mpController[chan]->getController();
        f32 x = 608.0f * controller->x / 2;
        f32 y = 456.0f * controller->y / 2;

        if (mAdjustFlag) {
            x *= mpHBInfo->adjust.x;
            y *= mpHBInfo->adjust.y;
        }

        mpPaneManager->update(chan, x, -y, controller->trig, controller->hold, controller->release, controller);

        u32 trig = controller->trig;
        if ((trig & (WPAD_BUTTON_HOME | 0x10000000)) && mState == 2) {
            if (mSequence == 1) {
                mpPaneManager->update(chan, 0.0f, -180.0f, 0, 0, 0, NULL);
                mpPairGroupAnmController[4]->start();
                mSelectAnmNum = 2;
                mpPairGroupAnmController[mSelectAnmNum]->start();
                mpPairGroupAnmController[11]->start();
                mState = 10;
                mSequence = 0;
                play_sound(8);
            } else if (mSequence == 0) {
                if (mpPairGroupAnmController[3]->mState == FrameController::STATE_PLAY) {
                    mpPairGroupAnmController[3]->stop();
                }
                if (mpPairGroupAnmController[12]->mState == FrameController::STATE_PLAY) {
                    mpPairGroupAnmController[12]->stop();
                }
                mSelectBtnNum = 0;
                mSelectAnmNum = 4;
                mpPairGroupAnmController[mSelectAnmNum]->start();
                mState = 14;
                play_sound(1);
            }
        } else if (mSequence == 1 && mState == 2) {
            if ((trig & WPAD_BUTTON_MINUS) || (trig & 0x10000)) {
                if (mVolumeNum > 0) {
                    mVolumeNum--;
                    mpGroupAnmController[findGroupAnimator(mVolumeNum + 21, 10)]->stop();
                    mpGroupAnmController[findGroupAnimator(mVolumeNum + 21, 9)]->start();
                    mpGroupAnmController[findGroupAnimator(11, 5)]->start();

                    if (mVolumeNum == 0) {
                        play_sound(12);
                        for (int i = 0; i < WPAD_MAX_CONTROLLERS; i++) {
                            mpController[i]->setVolume(mVolumeNum / 10.0f);
                            mpController[i]->playSound(mpSoundArchivePlayer, 1);
                        }
                    } else {
                        play_sound(10);
                        for (int i = 0; i < WPAD_MAX_CONTROLLERS; i++) {
                            mpController[i]->setVolume(mVolumeNum / 10.0f);
                            mpController[i]->playSound(mpSoundArchivePlayer, 1);
                        }
                    }
                } else {
                    play_sound(13);
                }
            } else if ((trig & WPAD_BUTTON_PLUS) || (trig & 0x20000)) {
                if (mVolumeNum < 10) {
                    mpGroupAnmController[findGroupAnimator(mVolumeNum + 21, 9)]->stop();
                    mpGroupAnmController[findGroupAnimator(mVolumeNum + 21, 10)]->start();
                    mVolumeNum++;
                    mpGroupAnmController[findGroupAnimator(12, 5)]->start();

                    if (mVolumeNum == 10) {
                        play_sound(11);
                        for (int i = 0; i < WPAD_MAX_CONTROLLERS; i++) {
                            mpController[i]->setVolume(mVolumeNum / 10.0f);
                            mpController[i]->playSound(mpSoundArchivePlayer, 1);
                        }
                    } else {
                        play_sound(9);
                        for (int i = 0; i < WPAD_MAX_CONTROLLERS; i++) {
                            mpController[i]->setVolume(mVolumeNum / 10.0f);
                            mpController[i]->playSound(mpSoundArchivePlayer, 1);
                        }
                    }
                } else {
                    play_sound(13);
                }
            }
        }
    } else if (mSequence == 1 && mState == 5 &&
               mpPairGroupAnmController[mSelectAnmNum]->mState != FrameController::STATE_PLAY &&
               mpController[chan]->getController()->trig != 0) {
        mMsgCount = mReConnectTime;
    }
}

void HomeButton::updateTrigPane() {
    switch (mSequence) {
    case 0:
        for (int i = 0; i < 10; i++) {
            if (i < 2 || i == 9) {
                mpPaneManager->getPaneComponentByPane(mpLayout->GetRootPane()->FindPaneByName(scFuncTouchPaneName[i], true))
                    ->setTriggerTarget(true);
            } else {
                mpPaneManager->getPaneComponentByPane(mpLayout->GetRootPane()->FindPaneByName(scFuncTouchPaneName[i], true))
                    ->setTriggerTarget(false);
            }
        }
        break;
    case 1:
        for (int i = 0; i < 10; i++) {
            if ((i > 0 && i < 7) || i == 9) {
                mpPaneManager->getPaneComponentByPane(mpLayout->GetRootPane()->FindPaneByName(scFuncTouchPaneName[i], true))
                    ->setTriggerTarget(true);
            } else {
                mpPaneManager->getPaneComponentByPane(mpLayout->GetRootPane()->FindPaneByName(scFuncTouchPaneName[i], true))
                    ->setTriggerTarget(false);
            }
        }
        break;
    case 2:
        for (int i = 0; i < 10; i++) {
            if (i < 7 || i == 9) {
                mpPaneManager->getPaneComponentByPane(mpLayout->GetRootPane()->FindPaneByName(scFuncTouchPaneName[i], true))
                    ->setTriggerTarget(false);
            } else {
                mpPaneManager->getPaneComponentByPane(mpLayout->GetRootPane()->FindPaneByName(scFuncTouchPaneName[i], true))
                    ->setTriggerTarget(true);
            }
        }
        break;
    }
}

void HomeButton::startPointEvent(const char* paneName, void* data) {
    int idx;
    int btnNo = getPaneNo(paneName);
    HBController* pController = static_cast<HBController*>(data);
    bool onFlag = false;

    if (isActive() && btnNo != -1 && mPaneCounter[btnNo] == 0) {
        if (mSequence != 2 && btnNo < mButtonNum) {
            idx = findAnimator(btnNo, 0);
            mpAnmController[idx]->start();

            if (mSequence == 0) {
                setForcusSE();
                onFlag = true;
            }
        } else {
            switch (btnNo - mButtonNum) {
            case 0:
                if (mSequence == 0) {
                    if (isUpBarActive()) {
                        mpPairGroupAnmController[3]->start();
                        mBar0AnmRevHold = 3;
                        mBar0AnmRev = 0;

                        setForcusSE();
                        onFlag = true;
                    } else {
                        mBar0AnmRev = 3;
                    }
                }
                break;
            case 1:
            case 9:
                if (mSequence == 0) {
                    idx = findGroupAnimator(4, 2);

                    if (isDownBarActive()) {
                        mpGroupAnmController[idx]->start();
                        mBar1AnmRevHold = idx;
                        mBar1AnmRev = 0;

                        setForcusSE();
                        onFlag = true;
                    } else {
                        mBar1AnmRev = idx;
                    }
                } else if (mSequence == 1) {
                    idx = findGroupAnimator(4, 19);

                    if (isDownBarActive()) {
                        mpGroupAnmController[idx]->start();
                        mBar1AnmRevHold = idx;
                        mBar1AnmRev = 0;

                        setForcusSE();
                        onFlag = true;
                    } else {
                        mBar1AnmRev = idx;
                    }
                }
                break;
            case 2:
                if (mSequence == 1) {
                    idx = findGroupAnimator(6, 4);
                    mpGroupAnmController[idx]->start();

                    setForcusSE();
                    onFlag = true;
                }
                break;
            case 3:
                if (mSequence == 1) {
                    idx = findGroupAnimator(7, 4);
                    mpGroupAnmController[idx]->start();

                    setForcusSE();
                    onFlag = true;
                }
                break;
            case 4:
                if (mSequence == 1) {
                    idx = findGroupAnimator(8, 4);
                    mpGroupAnmController[idx]->start();

                    setForcusSE();
                    onFlag = true;
                }
                break;
            case 5:
                if (mSequence == 1) {
                    idx = findGroupAnimator(9, 4);
                    mpGroupAnmController[idx]->start();

                    setForcusSE();
                    onFlag = true;
                }
                break;
            case 6:
                if (mSequence == 1) {
                    idx = findGroupAnimator(10, 4);
                    mpGroupAnmController[idx]->start();

                    setForcusSE();
                    onFlag = true;
                }
                break;
            case 7:
                if (mSequence == 2) {
                    idx = findGroupAnimator(17, 11);
                    mpGroupAnmController[idx]->start();

                    setForcusSE();
                    onFlag = true;
                }
                break;
            case 8:
                if (mSequence == 2) {
                    idx = findGroupAnimator(18, 11);
                    mpGroupAnmController[idx]->start();

                    setForcusSE();
                    onFlag = true;
                }
                break;
            }
        }
    }

    if (btnNo == mButtonNum + 1 || btnNo == mButtonNum + 9) {
        mPaneCounter[mButtonNum + 1]++;
        mPaneCounter[mButtonNum + 9]++;
    } else {
        mPaneCounter[btnNo]++;
    }

    if (onFlag && pController != NULL) {
        if (!mpController[pController->chan]->mHBController.rumble && mOnPaneVibWaitFrame[pController->chan] <= 0.0f) {
            mOnPaneVibFrame[pController->chan] = 3.0f;
            mpController[pController->chan]->startMotor();
        }
    }
}

void HomeButton::startLeftEvent(const char* paneName) {
    int idx;
    int btnNo = getPaneNo(paneName);

    if (0 < mPaneCounter[btnNo]) {
        if (btnNo == mButtonNum + 1 || btnNo == mButtonNum + 9) {
            mPaneCounter[mButtonNum + 1]--;
            mPaneCounter[mButtonNum + 9]--;
        } else {
            mPaneCounter[btnNo]--;
        }
    }

    if (isActive() && btnNo != -1 && !mPaneCounter[btnNo]) {
        if (mSequence != 2 && btnNo < mButtonNum) {
            idx = findAnimator(btnNo, 2);
            mpAnmController[idx]->start();
        } else {
            switch (btnNo - mButtonNum) {
            case 0:
                if (mSequence == 0) {
                    if (isUpBarActive()) {
                        mpPairGroupAnmController[12]->start();
                        mBar0AnmRevHold = 12;
                        mBar0AnmRev = 0;
                    } else {
                        mBar0AnmRev = 12;
                    }
                }
                break;
            case 1:
            case 9:
                if (mSequence == 0) {
                    idx = findGroupAnimator(5, 3);

                    if (isDownBarActive()) {
                        mpGroupAnmController[idx]->start();
                        mBar1AnmRevHold = idx;
                        mBar1AnmRev = 0;
                    } else {
                        mBar1AnmRev = idx;
                    }
                } else if (mSequence == 1) {
                    idx = findGroupAnimator(5, 20);

                    if (isDownBarActive()) {
                        mpGroupAnmController[idx]->start();
                        mBar1AnmRevHold = idx;
                        mBar1AnmRev = 0;
                    } else {
                        mBar1AnmRev = idx;
                    }
                }
                break;
            case 2:
                if (mSequence == 1) {
                    idx = findGroupAnimator(6, 7);
                    mpGroupAnmController[idx]->start();
                }
                break;
            case 3:
                if (mSequence == 1) {
                    idx = findGroupAnimator(7, 7);
                    mpGroupAnmController[idx]->start();
                }
                break;
            case 4:
                if (mSequence == 1) {
                    idx = findGroupAnimator(8, 7);
                    mpGroupAnmController[idx]->start();
                }
                break;
            case 5:
                if (mSequence == 1) {
                    idx = findGroupAnimator(9, 7);
                    mpGroupAnmController[idx]->start();
                }
                break;
            case 6:
                if (mSequence == 1) {
                    idx = findGroupAnimator(10, 7);
                    mpGroupAnmController[idx]->start();
                }
                break;
            case 7:
                if (mSequence == 2) {
                    idx = findGroupAnimator(17, 12);
                    mpGroupAnmController[idx]->start();
                }
                break;
            case 8:
                if (mSequence == 2) {
                    idx = findGroupAnimator(18, 12);
                    mpGroupAnmController[idx]->start();
                }
                break;
            }
        }
    }
}

void HomeButton::startTrigEvent(const char* paneName) {
    int idx;
    int btnNo = getPaneNo(paneName);

    if (isActive() && btnNo != -1) {
        if (mSequence == 0 && btnNo < mButtonNum) {
            // One of the menu buttons: open its confirmation dialog, if it has one.
            mSelectBtnNum = btnNo + 1;
            mSelectAnmNum = findAnimator(btnNo + 4, 1);
            mpAnmController[mSelectAnmNum]->start();
            play_sound(5);

            if (mDialogFlag[btnNo]) {
                mState = 11;
                mSequence = 2;

                nw4r::lyt::Pane* pane =
                    mpLayout->GetRootPane()->FindPaneByName(scFuncTextPaneName[2], true);
                nw4r::lyt::TextBox* textBox = nw4r::ut::DynamicCast<nw4r::lyt::TextBox*>(pane);

                // The dialog shows the message up to its question mark, or the
                // part inside its quotes.
                u16 len;
                if (mpHBInfo->messageFlag & (btnNo + 1)) {
                    for (len = 0;; len++) {
                        // U+FF1F FULLWIDTH QUESTION MARK
                        if (mpText[mpHBInfo->region][btnNo + 2][len] == 0xFF1F) {
                            break;
                        }

                        if (mpText[mpHBInfo->region][btnNo + 2][len] == L'?') {
                            break;
                        }
                    }
                } else {
                    for (len = 0;; len++) {
                        if (mpText[mpHBInfo->region][btnNo + 2][len + 1] == L'"') {
                            break;
                        }
                    }
                }

                textBox->SetString(mpText[mpHBInfo->region][btnNo + 2], 0, ++len);
                mpLayout->GetRootPane()->FindPaneByName(scFuncTextPaneName[2], true)->SetVisible(true);
            } else {
                mState = 15;
            }
        } else {
            switch (btnNo - mButtonNum) {
            case 0:
                // The home bar: back to the application.
                if (mpPairGroupAnmController[12]->mState == FrameController::STATE_PLAY) {
                    mpPairGroupAnmController[12]->stop();
                }

                if (mpPairGroupAnmController[3]->mState == FrameController::STATE_PLAY) {
                    mpPairGroupAnmController[3]->stop();
                }

                mSelectBtnNum = 0;
                mSelectAnmNum = 4;
                mpPairGroupAnmController[mSelectAnmNum]->start();
                mState = 14;
                play_sound(1);
                break;
            case 1:
            case 9:
                // The controller bar: open or close the controller menu.
                if (mSequence == 1) {
                    mpPairGroupAnmController[4]->start();
                    mpPairGroupAnmController[11]->start();
                    mSelectAnmNum = 2;
                    mpPairGroupAnmController[mSelectAnmNum]->start();
                    mState = 10;
                    mSequence = 0;
                    play_sound(8);
                } else if (mSequence == 0) {
                    mpPairGroupAnmController[1]->start();
                    mSelectAnmNum = 9;
                    mpPairGroupAnmController[mSelectAnmNum]->start();
                    mState = 10;

                    for (int i = 2; i < 7; i++) {
                        mpLayout->GetRootPane()->FindPaneByName(scFuncTouchPaneName[i], true)->SetVisible(true);
                    }

                    mSequence = 1;
                    play_sound(5);
                }
                break;
            case 2:
                // Volume down.
                if (mVolumeNum > 0) {
                    mVolumeNum--;
                    idx = findGroupAnimator(mVolumeNum + 21, 10);
                    mpGroupAnmController[idx]->stop();
                    idx = findGroupAnimator(mVolumeNum + 21, 9);
                    mpGroupAnmController[idx]->start();

                    if (mVolumeNum == 0) {
                        play_sound(12);
                        for (int i = 0; i < WPAD_MAX_CONTROLLERS; i++) {
                            mpController[i]->setVolume(mVolumeNum / 10.0f);
                            mpController[i]->playSound(mpSoundArchivePlayer, 1);
                        }
                    } else {
                        play_sound(10);
                        for (int i = 0; i < WPAD_MAX_CONTROLLERS; i++) {
                            mpController[i]->setVolume(mVolumeNum / 10.0f);
                            mpController[i]->playSound(mpSoundArchivePlayer, 1);
                        }
                    }

                    idx = findGroupAnimator(11, 5);
                    mpGroupAnmController[idx]->start();
                } else {
                    play_sound(13);
                }
                break;
            case 3:
                // Volume up.
                if (mVolumeNum < 10) {
                    idx = findGroupAnimator(mVolumeNum + 21, 9);
                    mpGroupAnmController[idx]->stop();
                    idx = findGroupAnimator(mVolumeNum + 21, 10);
                    mpGroupAnmController[idx]->start();
                    mVolumeNum++;

                    if (mVolumeNum == 10) {
                        play_sound(11);
                        for (int i = 0; i < WPAD_MAX_CONTROLLERS; i++) {
                            mpController[i]->setVolume(mVolumeNum / 10.0f);
                            mpController[i]->playSound(mpSoundArchivePlayer, 1);
                        }
                    } else {
                        play_sound(9);
                        for (int i = 0; i < WPAD_MAX_CONTROLLERS; i++) {
                            mpController[i]->setVolume(mVolumeNum / 10.0f);
                            mpController[i]->playSound(mpSoundArchivePlayer, 1);
                        }
                    }

                    idx = findGroupAnimator(12, 5);
                    mpGroupAnmController[idx]->start();
                } else {
                    play_sound(13);
                }
                break;
            case 4:
                // Vibration on.
                if (!mVibFlag) {
                    mVibFlag = true;
                    setVibFlag(mVibFlag);
                    idx = findGroupAnimator(14, 8);
                    mpGroupAnmController[idx]->start();
                    mSelectAnmNum = findGroupAnimator(16, 6);
                    mpGroupAnmController[mSelectAnmNum]->start();
                    play_sound(14);
                } else {
                    mSelectAnmNum = findGroupAnimator(16, 14);
                    mpGroupAnmController[mSelectAnmNum]->start();
                    play_sound(13);
                }

                for (int i = 0; i < WPAD_MAX_CONTROLLERS; i++) {
                    mpController[i]->startMotor();
                    mOnPaneVibFrame[i] = 0.0f;
                    mOnPaneVibWaitFrame[i] = 0.0f;
                }

                mState = 9;
                break;
            case 5:
                // Vibration off.
                if (mVibFlag) {
                    mVibFlag = false;
                    setVibFlag(mVibFlag);
                    idx = findGroupAnimator(14, 6);
                    mpGroupAnmController[idx]->start();
                    mSelectAnmNum = findGroupAnimator(13, 8);
                    mpGroupAnmController[mSelectAnmNum]->start();
                    play_sound(15);
                } else {
                    play_sound(13);
                }

                mState = 9;
                break;
            case 6:
                // Reconnect the controllers.
                mSelectAnmNum = findGroupAnimator(15, 5);
                mpGroupAnmController[mSelectAnmNum]->start();
                mState = 3;
                setReassignedFlag(true);

                for (int i = 0; i < WPAD_MAX_CONTROLLERS; i++) {
                    mOnPaneVibFrame[i] = 0.0f;
                    mOnPaneVibWaitFrame[i] = 0.0f;
                    mpController[i]->stopMotor();
                    mpController[i]->mMotorEnabled = false;
                }

                mWaitStopMotorCount = 30.0f / sHbm->mpHBInfo->frameDelta;
                mpLayout->GetRootPane()->FindPaneByName(scFuncTextPaneName[0], true)->SetVisible(true);
                mpLayout->GetRootPane()->FindPaneByName(scFuncTextPaneName[1], true)->SetVisible(true);
                play_sound(5);
                play_sound(16);
                break;
            case 7:
                // The dialog's OK button.
                mSelectAnmNum = findGroupAnimator(19, 13);
                mpGroupAnmController[mSelectAnmNum]->start();
                mState = 13;
                mSequence = 0;

                if (mSelectBtnNum == 1) {
                    play_sound(2);
                } else if (mSelectBtnNum == 2) {
                    play_sound(3);
                }
                break;
            case 8:
                // The dialog's cancel button.
                mSelectAnmNum = findGroupAnimator(20, 13);
                mpGroupAnmController[mSelectAnmNum]->start();
                mState = 13;
                mSelectBtnNum = -1;
                mSequence = 0;
                play_sound(6);
                break;
            }
        }
    }
}

void HomeButton::reset_btn() {
    for (int i = 0; i < mButtonNum; i++) {
        if (mPaneCounter[i] != 0) {
            mpAnmController[findAnimator(i, 2)]->start();
            mPaneCounter[i] = 0;
        }
    }

    if (mPaneCounter[mButtonNum] != 0) {
        mpPairGroupAnmController[12]->start();
        mPaneCounter[mButtonNum + 1] = 0;
    }

    if (mPaneCounter[mButtonNum + 1] != 0) {
        if (mSequence == 1) {
            if (mMsgCount == 0) {
                mpGroupAnmController[findGroupAnimator(5, 20)]->start();
                mPaneCounter[mButtonNum] = 0;
            }
        } else {
            mpGroupAnmController[findGroupAnimator(5, 3)]->start();
            mPaneCounter[mButtonNum] = 0;
        }
    }
}

int HomeButton::getPaneNo(const char* paneName) {
    int btnNum = mButtonNum;
    int ret = -1;

    for (int i = 0; i < btnNum; i++) {
        if (strcmp(paneName, scCursorPaneName[i]) == 0) {
            ret = i;
            break;
        }
    }

    for (int i = 0; i < 10; i++) {
        if (strcmp(paneName, scFuncTouchPaneName[i]) == 0) {
            ret = i + btnNum;
            break;
        }
    }

    return ret;
}

void HomeButton::setAdjustFlag(int flag) {
    nw4r::math::VEC2 scale;

    mAdjustFlag = flag;
    mDrawInfo.SetLocationAdjust(mAdjustFlag);

    if (mAdjustFlag) {
        scale = nw4r::math::VEC2(mpHBInfo->adjust.x, mpHBInfo->adjust.y);
        mpLayout->GetRootPane()->SetScale(scale);
        if (!mpHBInfo->cursor) {
            for (int i = 0; i < WPAD_MAX_CONTROLLERS; i++) {
                mpCursorLayout[i]->GetRootPane()->SetScale(scale);
            }
        }
    } else {
        scale = nw4r::math::VEC2(1.0f, 1.0f);
        mpLayout->GetRootPane()->SetScale(scale);
        if (!mpHBInfo->cursor) {
            for (int i = 0; i < WPAD_MAX_CONTROLLERS; i++) {
                mpCursorLayout[i]->GetRootPane()->SetScale(scale);
            }
        }
    }

    GXSetCullMode(GX_CULL_NONE);
}

void HomeButtonEventHandler::onEvent(u32 id, u32 event, void* data) {
    HBController* controller = static_cast<HBController*>(data);
    gui::PaneComponent* component = static_cast<gui::PaneComponent*>(mpManager->getComponent(id));
    const char* paneName = component->getPane()->GetName();
    HomeButton* homeButton = mpHomeButton;

    switch (event) {
    case 1:
        homeButton->startPointEvent(paneName, data);
        break;
    case 2:
        homeButton->startLeftEvent(paneName);
        break;
    case 0:
        if ((controller->trig & WPAD_BUTTON_A) || (controller->trig & 0x01000000)) {
            homeButton->startTrigEvent(paneName);
        }
        break;
    }
}

void HomeButton::startBlackOut() {
    if (mStartBlackOutFlag) {
        return;
    }

    mStartBlackOutFlag = true;
    for (int i = 0; i < WPAD_MAX_CONTROLLERS; i++) {
        mpController[i]->stopMotor();
    }

    mForceSttInitProcFlag = false;
    mForceSttFadeInProcFlag = false;
    mForceStopSyncFlag = false;
    mForceEndMsgAnmFlag = false;

    switch (mState) {
    case 0:
        mForceSttInitProcFlag = true;
    case 1:
        mForceSttFadeInProcFlag = true;
        break;
    case 3:
    case 5:
    case 6:
    case 7:
        if ((mState == 3 && mSelectAnmNum == 5) || (mState == 5 && !mSimpleSyncFlag)) {
            OSCancelAlarm(&mSimpleSyncAlarm);
            WPADSetSimpleSyncCallback(mSimpleSyncCallback);
        } else if (!mNetEventFlag && mState > 3) {
            mForceStopSyncFlag = true;
            if (!WPADStopSimpleSync()) {
                OSCancelAlarm(&mSimpleSyncAlarm);
                OSSetAlarmUserData(&mSimpleSyncAlarm, (void*)1);
                OSSetAlarm(&mSimpleSyncAlarm, OS_MSEC_TO_TICKS(100), SimpleSyncAlarmCallback);
            }
        } else {
            WPADSetSimpleSyncCallback(mSimpleSyncCallback);
        }
        mForceEndMsgAnmFlag = true;
        break;
    }

    mState = 19;
    mFader.state = 1;
    mSelectBtnNum = 2;
    mFadeOutFrames = mFader.maxFrame;

    if (mpHBInfo->sound_callback != NULL) {
        mpHBInfo->sound_callback(3, mFadeOutFrames);
    }
}

namespace homebutton {

static void initgx() {
    Mtx mtx;

    PSMTXIdentity(mtx);
    GXLoadPosMtxImm(mtx, GX_PNMTX0);
    GXSetCurrentMtx(GX_PNMTX0);
    GXClearVtxDesc();
    GXSetVtxDesc(GX_VA_POS, GX_DIRECT);
    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XY, GX_F32, 0);
    GXSetNumChans(1);
    GXSetChanCtrl(GX_COLOR0A0, GX_FALSE, GX_SRC_REG, GX_SRC_REG, GX_LIGHT_NULL, GX_DF_NONE, GX_AF_NONE);
    GXSetChanCtrl(GX_COLOR1A1, GX_FALSE, GX_SRC_REG, GX_SRC_REG, GX_LIGHT_NULL, GX_DF_NONE, GX_AF_NONE);
    GXSetNumTexGens(0);
    GXSetNumTevStages(1);
    GXSetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD_NULL, GX_TEXMAP_NULL, GX_COLOR_NULL);
    GXSetTevColorIn(GX_TEVSTAGE0, GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO, GX_CC_C0);
    GXSetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
    GXSetTevAlphaIn(GX_TEVSTAGE0, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_A0);
    GXSetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
    GXSetAlphaCompare(GX_ALWAYS, 0, GX_AOP_OR, GX_ALWAYS, 0);
    GXSetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_NOOP);
    GXSetAlphaUpdate(GX_FALSE);
    GXSetZMode(GX_FALSE, GX_ALWAYS, GX_FALSE);
    GXSetDispCopyGamma(GX_GM_1_0);
    GXSetNumIndStages(0);
    GXSetTevSwapModeTable(GX_TEV_SWAP0, GX_CH_RED, GX_CH_GREEN, GX_CH_BLUE, GX_CH_ALPHA);
    GXSetTevSwapMode(GX_TEVSTAGE0, GX_TEV_SWAP0, GX_TEV_SWAP0);
    GXSetCullMode(GX_CULL_NONE);
}

}  // namespace homebutton

void HomeButton::init_sound(nw4r::snd::SoundArchive* archive, int loadGroup) {
    void* buf = MEMAllocFromAllocator(&sHbmSoundAllocator, sizeof(nw4r::snd::SoundArchivePlayer));
    if (buf != NULL) {
        mpSoundArchivePlayer = new (buf) nw4r::snd::SoundArchivePlayer();
    }

    void* strmBuf;
    u32 memSize;
    u32 strmSize;
    memSize = mpSoundArchivePlayer->GetRequiredMemSize(archive);
    strmSize = mpSoundArchivePlayer->GetRequiredStrmBufferSize(archive);
    strmBuf = MEMAllocFromAllocator(&sHbmSoundAllocator, strmSize);
    void* mem = MEMAllocFromAllocator(&sHbmSoundAllocator, memSize);
    mpSoundArchivePlayer->Setup(archive, mem, memSize, strmBuf, strmSize);

    buf = MEMAllocFromAllocator(&sHbmSoundAllocator, sizeof(nw4r::snd::SoundHandle));
    if (buf != NULL) {
        mpSoundHandle = new (buf) nw4r::snd::SoundHandle();
    }

    if (loadGroup) {
        buf = MEMAllocFromAllocator(&sHbmSoundAllocator, sizeof(nw4r::snd::SoundHeap));
        if (buf != NULL) {
            mpSoundHeap = new (buf) nw4r::snd::SoundHeap();
        }

        u32 heapSize = mButtonNum == 2 ? 0x60000 : 0x6F800;
        mpSoundHeap->Create(MEMAllocFromAllocator(&sHbmSoundAllocator, heapSize), heapSize);
        mpSoundArchivePlayer->LoadGroup(0, mpSoundHeap, 0);
    } else {
        mpSoundHeap = NULL;
    }
}

void HomeButton::shutdownSoundArchives() {
    nw4r::ut::detail::AutoLock<OSMutex> lock(sMutex);

    if (mpDvdSoundArchive != NULL) {
        mpDvdSoundArchive->Close();
        mpDvdSoundArchive->~DvdSoundArchive();
    }

    if (mpSoundArchive != NULL) {
        mpSoundArchive->Shutdown();
        mpSoundArchive->~MemorySoundArchive();
    }

    if (mpNandSoundArchive != NULL) {
        mpNandSoundArchive->Close();
        mpNandSoundArchive->~NandSoundArchive();
    }

    if (mpSoundHeap != NULL) {
        mpSoundHeap->Destroy();
        mpSoundHeap->~SoundHeap();
    }

    if (mpSoundArchivePlayer != NULL) {
        mpSoundArchivePlayer->Shutdown();
        mpSoundArchivePlayer->~SoundArchivePlayer();
    }

    if (mpSoundHandle != NULL) {
        mpSoundHandle->~SoundHandle();
    }
}

void HomeButton::shutdownSound() {
    if (mpSoundArchivePlayer != NULL) {
        nw4r::ut::detail::AutoLock<OSMutex> lock(sMutex);
        for (u32 i = 0; i < mpSoundArchivePlayer->GetSoundPlayerCount(); i++) {
            mpSoundArchivePlayer->GetSoundPlayer(i).StopAllSound(0);
        }
    }

    AXFXReverbHiShutdown(&mReverb);
    AXRegisterAuxACallback(mAppAuxCallback, mpAppAuxContext);
    AXFXSetHooks(mAppFxAlloc, mAppFxFree);
    AXSetAuxAReturnVolume(mAppVolume[0]);
    AXSetAuxBReturnVolume(mAppVolume[1]);
    AXSetAuxCReturnVolume(mAppVolume[2]);
}
