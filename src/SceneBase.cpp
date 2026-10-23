// d_scene.cpp: base class of the channel's scenes and the resources they share
#include "revolution/CNT/cnt.h"
#include <channel/SceneBase.h>
#include <channel/Color.h>
#include <channel/Fade.h>
#include <channel/HomeButton.h>
#include <channel/LayoutButton.h>
#include <channel/PointerHistory.h>
#include <channel/SimpleGlobe.h>
#include <channel/SimpleModel.h>
#include <channel/Sound.h>
#include <channel/WorkerThread.h>
#include <channel/System.h>
#include <channel/WiiConnect24.h>

#include <revolution/VI.h>
#include <cstring>
#include <nw4r/math.h>
#include <nw4r/ut.h>
#include <nw4r/g3d/g3d_scnroot.h>
#include <nw4r/g3d/g3d_init.h>
#include <nw4r/lyt/lyt_layout.h>
#include <revolution/CNT.h>
#include <revolution/CX.h>
#include <revolution/MEM.h>
#include <revolution/OS.h>

wchar_t* FormatNumber(s32 value, wchar_t* pBuf, s32 digits, BOOL zeroPad);
void WrapHour(s32* pHour);

struct DragScroll {
    DragScroll();
    ~DragScroll() {}

    void Reset();
    void Update();

    u8 unk0[0x34]; // at 0x0
};

void PlaySE(s32 id);

void UpdateSound();
void CalcSound();
extern "C" void HBMStartBlackOut(void);

static const u32 sGlyphTextureIds[89] = {
    63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 89, 92, 90, 94, 95, 93, 73, 91, 74, 75, 76, 77, 78,
    79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 7,  8,  9,  10, 11, 12, 13, 14, 15, 16, 62, 17, 25,
    61, 57, 55, 59, 60, 58, 54, 56, 36, 34, 38, 39, 37, 33, 35, 29, 27, 31, 32, 30, 26, 28, 50,
    48, 52, 53, 51, 47, 49, 43, 41, 45, 46, 44, 40, 42, 21, 19, 23, 24, 22, 18, 20,
};

static const u32 sLanguageTextures[] = {100, 102, 98, 102, 101, 99, 97};

f32 gMenuBrightness = 1.0f;
s32 gGlobeAlpha = 255;
u8 gMenuVisible = TRUE;
static const char* sEarthPath = "/earth.brres.LZ";
static const char* sManualArchives[] = {"html-jp.arc", "html-us.arc", "html-eu.arc"};
static const char* sManualPageJP[] = {"arc:/html/index/index_Frameset.html", NULL};
static const char* sManualPagesUS[] = {
    NULL, "arc:/html/startup.html", NULL, "arc:/html/startup_fra.html", "arc:/html/startup_esp.html", NULL, NULL,
};
static const char* sManualPagesEU[] = {
    NULL,
    "arc:/html/startup.html",
    "arc:/html/startup_noe.html",
    "arc:/html/startup_fra.html",
    "arc:/html/startup_esp.html",
    "arc:/html/startup_ita.html",
    "arc:/html/startup_hol.html",
};

PointerHistory gPointerHistory;
OSCalendarTime sCalendarTime;
GlyphTexture sGlyphTextures[89];
DragScroll gDragScroll;
LayoutButton* sHoveredButtons[WPAD_MAX_CONTROLLERS];
nw4r::ut::TextWriterBase<wchar_t> gTextWriter;
wchar_t sTextBuf[0x100];
char sNameBuf[0x100];
MEMAllocator gSceneAllocator1;
MEMAllocator gSceneAllocator2;

HomeButton* gHomeButton;
f32 gUnkSceneFloat;
u32 gSceneFrameCount;
u32 gBlinkState;
s32 gAnimCounter1;
s32 gAnimCounter2;
u8 gFatalRequested;
u8 gReturnToMenuRequested;
u8 gPointerOverMenu;
static u8 sEarthLoading;
Fade* gFade;
Fade* gFade2;
static void* sSysFontBuf;
static void* sTimeFontBuf;
static void* sSysFontBuf2;
static void* sFutiFontBuf;
static nw4r::ut::ArchiveFont* sSysFont2;
nw4r::ut::ResFont* gFutiFont;
nw4r::ut::ArchiveFont* gSysFont;
nw4r::ut::ResFont* gTimeFont;
Sound* gSound;
static void* sEarthModelData;
SimpleGlobe* gSimpleGlobe;
SimpleModel* gEarthModel;
static u32 sEarthFileSize;
static u32 sEarthUncompSize;
static u32 sEarthChunkSize;
TPLPalette* gCommonTpl;
static void* sSceneMem1;
static void* sSceneMem2;
static MEMiHeapHead* sSceneHeap1;
static MEMiHeapHead* sSceneHeap2;
static WorkerThread* sEarthThread;
static void* sEarthData;
static void* sEarthChunkBuf;
Color gHighlightColor(140, 180, 180, 255);

static inline void ClearHoveredButtonsInline() {
    sHoveredButtons[0] = NULL;
    sHoveredButtons[1] = NULL;
    sHoveredButtons[2] = NULL;
    sHoveredButtons[3] = NULL;
}

SceneBase::SceneBase(bool arg)
    : mDrawFunc(NULL), mState(NULL), mClockX(0.0f), mClockY(0.0f), unk84(0.0f), mMenuBarY(0.0f), unk8C(0.0f),
      unk90(0.0f), mAmPmOffsetY(0.0f), mStatePhase(0), mClockAlpha(0), unkA0(0), unkA4(arg), mLayoutArc(NULL) {
    gFatalRequested = FALSE;
    sEarthModelData = NULL;
    sEarthLoading = FALSE;

    OSTicksToCalendarTime(OSGetTime(), &sCalendarTime);

    const GXColor clearColor = {0, 0, 0, 255};
    GXSetCopyClear(clearColor, 0xFFFFFF);

    sSceneMem1 = MEM1Alloc(0x700000, 0);
    sSceneMem2 = MEM2Alloc(0x1B00000, 0);
    sSceneHeap1 = MEMCreateExpHeapEx(sSceneMem1, 0x700000, 0);
    sSceneHeap2 = MEMCreateExpHeapEx(sSceneMem2, 0x1B00000, 0);
    MEMInitAllocatorForExpHeap(&gSceneAllocator1, sSceneHeap1, 32);
    MEMInitAllocatorForExpHeap(&gSceneAllocator2, sSceneHeap2, 32);

    gDragScroll.Reset();
    gPointerHistory.Reset();

    gSceneFrameCount = 0;
    gFade = NULL;
    gFade2 = NULL;
    sSysFontBuf2 = NULL;
    sSysFont2 = NULL;
    gReturnToMenuRequested = FALSE;
    ClearHoveredButtonsInline();
    nw4r::lyt::Layout::SetAllocator(&gMEM1Allocator);

    int i;
    GlyphTexture* tex;
    const u32* id;
    const char* page;
    switch (gRegion) {
    case 0:
        page = sManualPageJP[0];
        break;
    case 1:
        page = sManualPagesUS[gLanguage];
        break;
    case 2:
        page = sManualPagesEU[gLanguage];
        break;
    }

    gHomeButton = new HomeButton(8, sManualArchives[gRegion], page, &gSceneAllocator1, &gSceneAllocator2,
                                 &gMEM1Allocator);
    if (gHomeButton == NULL || !gHomeButton->mIsReady) {
        OSReport("%s[%d]\n", "d_scene.cpp", 388);
        gFatalRequested = TRUE;
        goto end;
    }

    gHomeButton->Init();
    gHomeButton->unkE = FALSE;
    gHomeButton->unkF = FALSE;

    void* timeFont;
    if (gLanguage == 0) {
        timeFont = sTimeFontBuf =
            LoadCompressedContentFile(gUnk80330B64, "font_weather_time.brfnt.LZ", 32, NULL, gMEM1Heap);
    } else {
        timeFont = sTimeFontBuf =
            LoadCompressedContentFile(gUnk80330B64, "font_weather_timeWW.brfnt.LZ", 32, NULL, gMEM1Heap);
    }

    if (timeFont == NULL) {
        OSReport("%s[%d]\n", "d_scene.cpp", 413);
        gFatalRequested = TRUE;
        goto end;
    }

    gTimeFont = new nw4r::ut::ResFont;
    if (gTimeFont == NULL) {
        OSPanic("d_scene.cpp", 421, "m_pTimeFont\n");
    }

    if (!gTimeFont->SetResource(sTimeFontBuf)) {
        OSPanic("d_scene.cpp", 425, "nw4r::ut::ResFont::SetResource() failed.\n");
    }

    if (LoadSysFont()) {
        OSReport("%s[%d]\n", "d_scene.cpp", 430);
        gFatalRequested = TRUE;
        goto end;
    }

    sFutiFontBuf = LoadCompressedContentFile(gUnk80330B64, "/font_weather_city.brfnt.LZ", 32, NULL, gMEM2Heap);
    if (sFutiFontBuf == NULL) {
        OSReport("%s[%d]\n", "d_scene.cpp", 439);
        gFatalRequested = TRUE;
        goto end;
    }

    gFutiFont = new nw4r::ut::ResFont;
    if (gFutiFont == NULL) {
        OSPanic("d_scene.cpp", 448, "m_pFutiFont\n");
    }

    if (!gFutiFont->SetResource(sFutiFontBuf)) {
        OSPanic("d_scene.cpp", 454, "m_pFutiFont->SetResource() failed.\n");
    }

    gFutiFont->SetAlternateChar(0xE06B);

    gCommonTpl = (TPLPalette*)LoadCompressedContentFile(gUnk80330B64, "TPLCommon.tpl.LZ", 32, NULL, gMEM1Heap);
    if (gCommonTpl == NULL) {
        OSReport("%s[%d]\n", "d_scene.cpp", 462);
        gFatalRequested = TRUE;
        goto end;
    }

    TPLBind(gCommonTpl);

    gFade = new Fade(nw4r::ut::Color(0, 0, 0, 255));
    if (gFade == NULL) {
        OSPanic("d_scene.cpp", 473, "m_pFade\n");
    }

    gFade2 = new Fade(nw4r::ut::Color(0, 0, 0, 160));
    if (gFade2 == NULL) {
        OSPanic("d_scene.cpp", 481, "m_pFade2\n");
    }

    gSimpleGlobe = new SimpleGlobe;
    if (gSimpleGlobe == NULL) {
        OSPanic("d_scene.cpp", 488, "m_pSimpleGlobe\n");
    }

    mTextWriter.SetFont(*gFutiFont);
    mTextWriter.SetCharSpace(0.0f);

    tex = sGlyphTextures;
    id = sGlyphTextureIds;
    for (i = 0; i < 89; i++, tex++, id++) {
        tex->texture = *id;
        tex->width = GetTexWidth(gCommonTpl, tex->texture);
        tex->height = GetTexHeight(gCommonTpl, tex->texture);
    }

    if (gLanguage == 0) {
        mClockX = gWidescreen ? 36 : 28;
        mClockY = gWidescreen ? 19 : 34;
        unk84 = 165.0f + mClockX;
        mMenuBarY = 41.0f + mClockY;
    } else {
        mClockX = gWidescreen ? 36 : 28;
        mClockY = gWidescreen ? 19 : 34;
        unk84 = 165.0f + mClockX;
        mMenuBarY = 36.0f + mClockY;

        f32 height = gTimeFont->GetHeight();
        mAmPmOffsetY = height - 0.75f * height;
    }

    switch (gLanguage) {
    case 0:
        mDrawFunc = &SceneBase::DrawTimeJP;
        break;
    case 1:
        if (gRegion == 1) {
            mDrawFunc = &SceneBase::DrawTimeUS;
        } else {
            mDrawFunc = &SceneBase::DrawTimeEN;
        }
        break;
    case 2:
        mDrawFunc = &SceneBase::DrawTimeDE;
        break;
    case 3:
        mDrawFunc = &SceneBase::DrawTimeFR;
        break;
    case 4:
        mDrawFunc = &SceneBase::DrawTimeES;
        break;
    case 5:
        mDrawFunc = &SceneBase::DrawTimeIT;
        break;
    case 6:
        mDrawFunc = &SceneBase::DrawTimeNL;
        break;
    }

    ChangeState(&SceneBase::StateMain);
end:;
}

static inline void FreeSysFontsInline() {
    if (sSysFont2 != NULL) {
        sSysFont2->Destroy();
        delete sSysFont2;
        sSysFont2 = NULL;
    }

    if (sSysFontBuf2 != NULL) {
        MEMFreeToAllocator(&gSceneAllocator2, sSysFontBuf2);
        sSysFontBuf2 = NULL;
    }

    if (gSysFont != NULL) {
        gSysFont->Destroy();
        delete gSysFont;
        gSysFont = NULL;
    }

    if (sSysFontBuf != NULL) {
        MEMFreeToAllocator(&gSceneAllocator2, sSysFontBuf);
        sSysFontBuf = NULL;
    }
}

void SceneBase::RequestFatal() {
    gFatalRequested = TRUE;
}

SceneBase::~SceneBase() {
    Exit(FALSE, 0);

    if (gEarthModel != NULL) {
        delete gEarthModel;
        gEarthModel = NULL;
    }

    if (sEarthModelData != NULL) {
        MEMFreeToExpHeap(sSceneHeap2, sEarthModelData);
        sEarthModelData = NULL;
    }

    if (sEarthThread != NULL) {
        delete sEarthThread;
        sEarthThread = NULL;
    }

    if (gSound != NULL) {
        delete gSound;
        gSound = NULL;
    }

    if (gSimpleGlobe != NULL) {
        delete gSimpleGlobe;
        gSimpleGlobe = NULL;
    }

    if (gFade2 != NULL) {
        delete gFade2;
        gFade2 = NULL;
    }

    if (gFade != NULL) {
        delete gFade;
        gFade = NULL;
    }

    if (gCommonTpl != NULL) {
        MEM1Free(gCommonTpl);
        gCommonTpl = NULL;
    }

    if (gFutiFont != NULL) {
        delete gFutiFont;
        gFutiFont = NULL;
    }

    if (sFutiFontBuf != NULL) {
        MEM2Free(sFutiFontBuf);
        sFutiFontBuf = NULL;
    }

    if (gTimeFont != NULL) {
        delete gTimeFont;
        gTimeFont = NULL;
    }

    if (sTimeFontBuf != NULL) {
        MEM1Free(sTimeFontBuf);
        sTimeFontBuf = NULL;
    }

    FreeSysFontsInline();

    if (sEarthChunkBuf != NULL) {
        MEMFreeToExpHeap(sSceneHeap2, sEarthChunkBuf);
        sEarthChunkBuf = NULL;
    }

    if (gHomeButton != NULL) {
        delete gHomeButton;
    }

    MEMDestroyExpHeap(sSceneHeap2);
    MEMDestroyExpHeap(sSceneHeap1);
    MEM2Free(sSceneMem2);
    MEM1Free(sSceneMem1);
}

void SceneBase::Exit(BOOL shutdownNet, s32 event) {
    VISetBlack(TRUE);
    VIFlush();
    VIWaitForRetrace();
    StopRumble(-1, 0);

    if (shutdownNet) {
        ShutdownDownloader(event);
    }
}

BOOL LoadSysFont() {
    void* file = LoadContentFile(5, "wbf1.brfna", -32, NULL, sSceneHeap1);
    if (file == NULL) {
        return TRUE;
    }

    u32 size = nw4r::ut::ArchiveFont::GetRequireBufferSize(file);
    sSysFontBuf = MEMAllocFromAllocator(&gSceneAllocator2, size);
    if (sSysFontBuf == NULL) {
        MEM1Free(file);
        OSPanic("d_scene.cpp", 725, "m_pSysFontBuf\n");
    }

    gSysFont = new nw4r::ut::ArchiveFont;
    if (gSysFont == NULL) {
        OSPanic("d_scene.cpp", 732, "m_pSysFont\n");
    }

    if (!gSysFont->Construct(sSysFontBuf, size, file)) {
        MEM1Free(file);
        OSPanic("d_scene.cpp", 737, "nw4r::ut::ArchiveFont::Construct() failed.\n");
    }

    gSysFont->SetAlternateChar(0xE06B);
    MEMFreeToExpHeap(sSceneHeap1, file);
    return FALSE;
}

void FreeSysFonts() {
    FreeSysFontsInline();
}

inline void SceneBase::UpdatePointerOverMenu() const {
    gPointerOverMenu = FALSE;
    for (int i = 0; i < WPAD_MAX_CONTROLLERS; i++) {
        BOOL valid = FALSE;
        if (gPointerValid[i][0] && gKPADLatest[i] >= 0) {
            valid = TRUE;
        }

        if (valid && GetPointerY(i) < mMenuBarY) {
            gPointerOverMenu = TRUE;
            break;
        }
    }
}

void SceneBase::Calc() {
    if (gFatalRequested) {
        if (!IsState(&SceneBase::StateFatal)) {
            ChangeState(&SceneBase::StateFatal);
        } else {
            switch (mStatePhase) {
            case 0: {
                mStatePhase++;
                GXColor clear = {0, 0, 0, 0};
                GXSetCopyClear(clear, 0xFFFFFF);
                StartFade(2, 20, 0, 0);
                break;
            }
            case -1:
                break;
            default:
                gNextScene = 'FATL';
                break;
            }
        }
        return;
    }

    if (OSGetResetButtonState()) {
        if (gHomeButton->IsOpen()) {
            HBMStartBlackOut();
        } else {
            Exit(TRUE, 4);
            Restart();
        }
    }

    nw4r::g3d::G3dReset();
    OSTicksToCalendarTime(OSGetTime(), &sCalendarTime);
    gSceneFrameCount++;
    unk20();

    if (sEarthLoading && sEarthModelData != NULL) {
        if (sEarthChunkBuf != NULL) {
            MEMFreeToExpHeap(sSceneHeap2, sEarthChunkBuf);
            sEarthChunkBuf = NULL;
        }

        if (gEarthModel == NULL) {
            gEarthModel = new (-32) SimpleModel(sEarthModelData);
        }
    }

    UpdatePointerOverMenu();

    if (mState) {
        (this->*mState)();
    }

    if (!gFatalRequested) {
        UpdateMenuFade();

        u32 frame = gSceneFrameCount;
        unkA0 = (frame >> 8) & 1;
        if ((frame & 31) == 0) {
            gBlinkState ^= 1;
        }

        if ((frame & 15) == 0) {
            if (++gAnimCounter1 > 2) {
                gAnimCounter1 = 0;
            }
            if (++gAnimCounter2 > 2) {
                gAnimCounter2 = 0;
            }
        }

        if (gFade != NULL) {
            gFade->Calc();
        }
        if (gFade2 != NULL) {
            gFade2->Calc();
        }

        unk2C();

        if (gSound != NULL) {
            CalcSound();
        }
    }
}

void SceneBase::unk20() {
    if (gSound != NULL) {
        UpdateSound();
    }
}

void SceneBase::unk2C() {
    if (gEarthModel != NULL) {
        gGlobeAlpha -= 6;
        if (gGlobeAlpha < 0) {
            gGlobeAlpha = 0;
        }
    } else {
        gGlobeAlpha = 255;
    }
}

void SceneBase::Draw() {
    if (!gFatalRequested) {
        (this->*mDrawFunc)();

        if (!gHomeButton->IsOpen()) {
            unk3C();
        }
    }
}

void SceneBase::DrawOverlay() {
    if (!gFatalRequested) {
        if (gFade != NULL) {
            gFade->Draw();
        }
        if (gFade2 != NULL) {
            gFade2->Draw();
        }

        unk38();
        gHomeButton->Draw();
    }
}

void SceneBase::unk38() {}

void SceneBase::unk3C() {
    SetCursor(-1, 1);
}

void SceneBase::DrawTimeJP() {
    if (mClockAlpha != 0) {
        wchar_t* p = FormatNumber(sCalendarTime.hour % 12, sTextBuf, 2, FALSE);
        *p = L':';
        FormatNumber(sCalendarTime.min, p + 1, 2, TRUE);

        nw4r::ut::WideTextWriter writer;
        SetDefaultGXState();
        SetOrthoProjection();
        writer.SetFont(*gTimeFont);
        writer.SetDrawFlag(0);
        writer.SetupGX();
        writer.SetTextColor(nw4r::ut::Color(255, 255, 255, mClockAlpha));
        writer.SetScale(1.0f);
        writer.SetCharSpace(0.0f);
        writer.SetCursor(mClockX, mClockY);
        writer.Print(sTextBuf);
    }
}

void SceneBase::DrawTimeUS() {
    if (mClockAlpha != 0) {
        s32 hour = sCalendarTime.hour % 12;
        if (hour == 0) {
            hour = 12;
        }
        wchar_t* p = FormatNumber(hour, sTextBuf, 2, FALSE);
        *p = L':';
        FormatNumber(sCalendarTime.min, p + 1, 2, TRUE);

        nw4r::ut::WideTextWriter writer;
        SetDefaultGXState();
        SetOrthoProjection();
        writer.SetFont(*gTimeFont);
        writer.SetDrawFlag(0);
        writer.SetupGX();
        writer.SetTextColor(nw4r::ut::Color(255, 255, 255, mClockAlpha));
        writer.SetScale(1.0f);
        writer.SetCharSpace(0.0f);
        writer.SetCursor(mClockX, mClockY);
        writer.Print(sTextBuf);
        f32 width = writer.CalcStringWidth(sTextBuf);
        writer.SetScale(0.75f);
        writer.SetCursor(mClockX + width, mClockY + mAmPmOffsetY);
        if (sCalendarTime.hour < 12) {
            writer.Print(L" a.m.");
        } else {
            writer.Print(L" p.m.");
        }
    }
}

void SceneBase::DrawTimeEN() {
    if (mClockAlpha != 0) {
        s32 hour = sCalendarTime.hour;
        WrapHour(&hour);
        wchar_t* p = FormatNumber(hour, sTextBuf, 2, TRUE);
        *p = L':';
        FormatNumber(sCalendarTime.min, p + 1, 2, TRUE);

        nw4r::ut::WideTextWriter writer;
        SetDefaultGXState();
        SetOrthoProjection();
        writer.SetFont(*gTimeFont);
        writer.SetDrawFlag(0);
        writer.SetupGX();
        writer.SetTextColor(nw4r::ut::Color(255, 255, 255, mClockAlpha));
        writer.SetScale(1.0f);
        writer.SetCharSpace(0.0f);
        writer.SetCursor(mClockX, mClockY);
        writer.Print(sTextBuf);
    }
}

void SceneBase::DrawTimeDE() {
    if (mClockAlpha != 0) {
        s32 hour = sCalendarTime.hour;
        WrapHour(&hour);
        wchar_t* p = FormatNumber(hour, sTextBuf, 2, TRUE);
        *p = L':';
        FormatNumber(sCalendarTime.min, p + 1, 2, TRUE);

        nw4r::ut::WideTextWriter writer;
        SetDefaultGXState();
        SetOrthoProjection();
        writer.SetFont(*gTimeFont);
        writer.SetDrawFlag(0);
        writer.SetupGX();
        writer.SetTextColor(nw4r::ut::Color(255, 255, 255, mClockAlpha));
        writer.SetScale(1.0f);
        writer.SetCharSpace(0.0f);
        writer.SetCursor(mClockX, mClockY);
        writer.Print(sTextBuf);
    }
}

void SceneBase::DrawTimeFR() {
    if (mClockAlpha != 0) {
        s32 hour = sCalendarTime.hour;
        WrapHour(&hour);
        wchar_t* p = FormatNumber(hour, sTextBuf, 2, TRUE);
        *p = L':';
        FormatNumber(sCalendarTime.min, p + 1, 2, TRUE);

        nw4r::ut::WideTextWriter writer;
        SetDefaultGXState();
        SetOrthoProjection();
        writer.SetFont(*gTimeFont);
        writer.SetDrawFlag(0);
        writer.SetupGX();
        writer.SetTextColor(nw4r::ut::Color(255, 255, 255, mClockAlpha));
        writer.SetScale(1.0f);
        writer.SetCharSpace(0.0f);
        writer.SetCursor(mClockX, mClockY);
        writer.Print(sTextBuf);
    }
}

void SceneBase::DrawTimeES() {
    if (mClockAlpha != 0) {
        s32 hour = sCalendarTime.hour;
        WrapHour(&hour);
        wchar_t* p = FormatNumber(hour, sTextBuf, 2, TRUE);
        *p = L':';
        FormatNumber(sCalendarTime.min, p + 1, 2, TRUE);

        nw4r::ut::WideTextWriter writer;
        SetDefaultGXState();
        SetOrthoProjection();
        writer.SetFont(*gTimeFont);
        writer.SetDrawFlag(0);
        writer.SetupGX();
        writer.SetTextColor(nw4r::ut::Color(255, 255, 255, mClockAlpha));
        writer.SetScale(1.0f);
        writer.SetCharSpace(0.0f);
        writer.SetCursor(mClockX, mClockY);
        writer.Print(sTextBuf);
    }
}

void SceneBase::DrawTimeIT() {
    if (mClockAlpha != 0) {
        s32 hour = sCalendarTime.hour;
        WrapHour(&hour);
        wchar_t* p = FormatNumber(hour, sTextBuf, 2, TRUE);
        *p = L':';
        FormatNumber(sCalendarTime.min, p + 1, 2, TRUE);

        nw4r::ut::WideTextWriter writer;
        SetDefaultGXState();
        SetOrthoProjection();
        writer.SetFont(*gTimeFont);
        writer.SetDrawFlag(0);
        writer.SetupGX();
        writer.SetTextColor(nw4r::ut::Color(255, 255, 255, mClockAlpha));
        writer.SetScale(1.0f);
        writer.SetCharSpace(0.0f);
        writer.SetCursor(mClockX, mClockY);
        writer.Print(sTextBuf);
    }
}

void SceneBase::DrawTimeNL() {
    if (mClockAlpha != 0) {
        s32 hour = sCalendarTime.hour;
        WrapHour(&hour);
        wchar_t* p = FormatNumber(hour, sTextBuf, 2, TRUE);
        *p = L':';
        FormatNumber(sCalendarTime.min, p + 1, 2, TRUE);

        nw4r::ut::WideTextWriter writer;
        SetDefaultGXState();
        SetOrthoProjection();
        writer.SetFont(*gTimeFont);
        writer.SetDrawFlag(0);
        writer.SetupGX();
        writer.SetTextColor(nw4r::ut::Color(255, 255, 255, mClockAlpha));
        writer.SetScale(1.0f);
        writer.SetCharSpace(0.0f);
        writer.SetCursor(mClockX, mClockY);
        writer.Print(sTextBuf);
    }
}

void SceneBase::unk40() {}

void SceneBase::UpdateMenuFade() {
    if (gPointerOverMenu || gMenuVisible) {
        if (mClockAlpha != 0) {
            mClockAlpha -= 20;
            if (mClockAlpha < 0) {
                mClockAlpha = 0;
            }
        }

        gMenuBrightness += 0.1f;
        if (gMenuBrightness > 1.0f) {
            gMenuBrightness = 1.0f;
        }
    } else {
        if (mClockAlpha != 255) {
            mClockAlpha += 20;
            if (mClockAlpha > 255) {
                mClockAlpha = 255;
            }
        }

        gMenuBrightness -= 0.1f;
        if (gMenuBrightness < 0.2f) {
            gMenuBrightness = 0.2f;
        }
    }
}

BOOL SceneBase::StateMain() {
    switch (mStatePhase) {
    case 0:
        mStatePhase++;
        break;
    case -1:
        break;
    default:
        if (gReturnToMenuRequested) {
            ChangeState(&SceneBase::StateReturnToMenu);
            return TRUE;
        }

        gHomeButton->unkE = gEarthModel != NULL || !sEarthLoading;
        gHomeButton->unkF = sEarthLoading;

        switch (gHomeButton->Calc()) {
        case 1:
            ChangeState(&SceneBase::StateReturnToMenu);
            return TRUE;
        case 2:
            ChangeState(&SceneBase::StateReset);
            break;
        case 3:
            gFatalRequested = TRUE;
            break;
        case 0:
            if (gHomeButton->IsOpen()) {
                unk28();
            } else {
                gPointerHistory.Update();
                unk24();
            }
            break;
        }
        break;
    }

    return TRUE;
}

void SceneBase::unk28() {}

void SceneBase::unk24() {}

BOOL SceneBase::StateReset() {
    switch (mStatePhase) {
    case -1:
        break;
    case 0:
        Exit(TRUE, 4);
        Restart();
        break;
    case 1:
        if (unk48()) {
            gFade->FadeOut(30);
            if (gReturnToMenuRequested) {
                ChangeState(&SceneBase::StateReturnToMenu);
            } else {
                mStatePhase = 2;
            }
        }
        break;
    case 2:
        if (gFade->mFading == 0) {
            mStatePhase = 3;
        }
        break;
    case 3:
        Exit(TRUE, 4);
        Restart();
        break;
    }

    return TRUE;
}

BOOL SceneBase::StateReturnToMenu() {
    switch (mStatePhase) {
    case -1:
        break;
    case 0:
        if (unk44()) {
            gFade->FadeOut(30);
            mStatePhase = 2;
        } else {
            mStatePhase = 1;
        }
        break;
    case 1:
        if (unk48()) {
            gFade->FadeOut(30);
            mStatePhase = 2;
        }
        break;
    case 2:
    default:
        if (gFade->mFading == 0) {
            if (!sEarthLoading) {
                Exit(TRUE, 5);
                ReturnToMenu();
            } else if (gEarthModel != NULL) {
                Exit(TRUE, 5);
                ReturnToMenu();
            }
        }
        break;
    }

    return TRUE;
}

BOOL SceneBase::StateFatal() {
    switch (mStatePhase) {
    case 0:
        mStatePhase++;
        GXColor clear = {0, 0, 0, 0};
        GXSetCopyClear(clear, 0xFFFFFF);
        StartFade(FADE_CAPTURE_BRIGHTNESS, 20, FADE_NONE, 0);
        break;
    case -1:
        break;
    default:
        gNextScene = SCENE_FATAL;
        break;
    }

    return TRUE;
}

static void* EarthLoadThread(void* arg);

BOOL LoadEarthModel() {
    CNTFileInfoNAND file;
    u8 header[32] ATTRIBUTE_ALIGN(32);
    s32 result;

    sEarthLoading = TRUE;

    result = contentOpenNAND(&gContentHandles[6], sEarthPath, &file);
    switch (result) {
    case 0:
        sEarthFileSize = (contentGetLengthNAND(&file) + 31) & ~31;
        result = contentReadNAND(&file, header, sizeof(header), 0);
        contentCloseNAND(&file);
        if (result == 0) {
            OSReport("Error!! (%s) CNTRead() failed. %d\n", sEarthPath, result);
            return FALSE;
        }

        sEarthUncompSize = CXGetUncompressedSize(header);
        break;
    default:
        OSReport("Error!! (%s) CNTOpen() failed. %d\n", sEarthPath, result);
        return FALSE;
    }

    sEarthChunkSize = 0x10000;
    sEarthData = MEMAllocFromExpHeapEx(sSceneHeap2, sEarthUncompSize, -32);
    sEarthChunkBuf = MEMAllocFromExpHeapEx(sSceneHeap2, sEarthChunkSize, -32);

    if (sEarthThread == NULL) {
        sEarthThread = new WorkerThread(EarthLoadThread);
        if (sEarthThread == NULL) {
            OSPanic("d_scene.cpp", 1699, "メモリがない！！\n");
            return FALSE;
        }
    } else {
        sEarthThread->Restart(EarthLoadThread);
    }

    return TRUE;
}

static void* EarthLoadThread(void* arg) {
    CNTFileInfoNAND file;
    CXUncompContextLZ ctx;
    s32 result;

    result = contentOpenNAND(&gContentHandles[6], sEarthPath, &file);
    switch (result) {
    case 0: {
        CXInitUncompContextLZ(&ctx, sEarthData);

        for (u32 offset = 0; offset < sEarthFileSize; offset += sEarthChunkSize) {
            u32 size = sEarthFileSize - offset;
            if (size > sEarthChunkSize) {
                size = sEarthChunkSize;
            }

            result = contentReadNAND(&file, sEarthChunkBuf, size, offset);
            if (result == 0) {
                contentCloseNAND(&file);
                OSReport("Error!! (%s) CNTRead() failed. %d\n", sEarthPath, result);
                gFatalRequested = TRUE;
                return NULL;
            }

            CXReadUncompLZ(&ctx, sEarthChunkBuf, size);
        }

        contentCloseNAND(&file);

        BOOL notFinished = ctx.destCount > 0 || ctx.headerSize > 0;
        if (notFinished) {
            OSReport("CXIsFinisiedUncompLZ() is false.");
            gFatalRequested = TRUE;
            return NULL;
        }
        break;
    }
    default:
        OSReport("Error!! (%s) CNTOpen() failed. %d\n", sEarthPath, result);
        gFatalRequested = TRUE;
        return NULL;
    }

    sEarthModelData = sEarthData;
    sEarthData = NULL;
    return NULL;
}

BOOL FreeEarthModel() {
    if (sEarthLoading) {
        if (gEarthModel != NULL) {
            if (gEarthModel != NULL) {
                if (gSimpleGlobe != NULL) {
                    if (gSimpleGlobe->mScnRoot != NULL) {
                        gSimpleGlobe->mScnRoot->Clear();
                    }
                    gSimpleGlobe->Calc();
                }

                delete gEarthModel;
                gEarthModel = NULL;
            }

            if (sEarthModelData != NULL) {
                MEMFreeToExpHeap(sSceneHeap2, sEarthModelData);
                sEarthModelData = NULL;
            }

            sEarthLoading = FALSE;
            return TRUE;
        }

        return FALSE;
    }

    return TRUE;
}

void UpdateButtons(ButtonGroup* group, s32 hoverSound) {
    f32 width = GetScreenWidth();
    f32 halfWidth = 0.5f * width;
    f32 width43 = 608.0f;
    f32 halfWidth43 = 0.5f * width43;
    f32 scale = width43 / width;
    f32 halfHeight = 0.5f * (s32)gRenderMode.efbHeight;

    for (int i = 0; i < WPAD_MAX_CONTROLLERS; i++) {
        BOOL valid = FALSE;
        if (gPointerValid[i][0] && gKPADLatest[i] >= 0) {
            valid = TRUE;
        }

        if (valid) {
            f32 x;
            if (gWidescreen) {
                x = scale * (gPointerX[i][0] - halfWidth);
            } else {
                x = gPointerX[i][0] - halfWidth43;
            }
            f32 y = halfHeight - gPointerY[i][0];

            if (x < -halfWidth43) {
                x = -halfWidth43;
            } else if (x > halfWidth43) {
                x = halfWidth43;
            }

            if (y < -halfHeight) {
                y = -halfHeight;
            } else if (y > halfHeight) {
                y = halfHeight;
            }

            LayoutButton* button = group->HitTest(x, y);
            if (sHoveredButtons[i] != button) {
                if (sHoveredButtons[i] != NULL) {
                    sHoveredButtons[i]->Release();
                    sHoveredButtons[i]->mHeld = FALSE;
                }

                sHoveredButtons[i] = button;
                if (button != NULL && !button->IsInactive()) {
                    PlaySE(hoverSound);
                    StartRumble(i, 3, 20);
                }
            }

            if (sHoveredButtons[i] != NULL) {
                if (gRelease[i] & WPAD_BUTTON_A) {
                    sHoveredButtons[i]->Release();
                    sHoveredButtons[i]->mHeld = FALSE;
                }
                sHoveredButtons[i]->Hover();
            }
        }
    }
}

void ClearHoveredButtons() {
    ClearHoveredButtonsInline();
}

s32 CheckButtonHeld(const char* name, u32 buttons) {
    for (int i = 0; i < WPAD_MAX_CONTROLLERS; i++) {
        BOOL valid = FALSE;
        if (gPointerValid[i][0] && gKPADLatest[i] >= 0) {
            valid = TRUE;
        }

        if (valid) {
            LayoutButton* button = sHoveredButtons[i];
            if (button != NULL && !button->mLocked && button->IsName(name)) {
                u32 pressed;
                if (button->mHeld) {
                    pressed = (u16)buttons & gRepeatSlowButtons[i];
                } else {
                    pressed = (u16)buttons & gTrig[i];
                }

                if (pressed) {
                    sHoveredButtons[i]->mHeld = TRUE;
                    sHoveredButtons[i]->Press(FALSE);
                    return i;
                }
            }
        }
    }

    return -1;
}

s32 CheckButtonPressed(const char* name, u32 buttons) {
    for (int i = 0; i < WPAD_MAX_CONTROLLERS; i++) {
        BOOL valid = FALSE;
        if (gPointerValid[i][0] && gKPADLatest[i] >= 0) {
            valid = TRUE;
        }

        if (valid) {
            LayoutButton* button = sHoveredButtons[i];
            if (button != NULL && ((u16)buttons & gTrig[i]) && !button->mLocked &&
                button->IsName(name)) {
                sHoveredButtons[i]->Press(FALSE);
                return i;
            }
        }
    }

    return -1;
}

void ToDegrees(u16 lon, u16 lat, Vec2* out) {
    f32 scale = 360.0f / 65536.0f;
    out->y = (f32)lat * scale;
    out->x = (f32)(s16)lon * scale;
}

void UpdateDragScroll() {
    gDragScroll.Update();
}

static const wchar_t sDateGlyphs[] = {
    L'0', L'1', L'2', L'3', L'4', L'5', L'6', L'7', L'8', L'9', L'-',
    0x65E5, 0x6708, 0x706B, 0x6C34, 0x6728, 0x91D1, 0x571F,
};

static inline int GetDateGlyph(wchar_t c) {
    for (int i = 0; i < (int)(sizeof(sDateGlyphs) / sizeof(sDateGlyphs[0])); i++) {
        if (c == sDateGlyphs[i]) {
            return i;
        }
    }
    return -1;
}

static const wchar_t sNumGlyphs[33] = {
    L'0', L'1', L'2', L'3', L'4', L'5', L'6', L'7', L'8', L'9', L':', L'(', L')', L'-', L'+',
};

static inline int GetNumGlyph(wchar_t c) {
    for (int i = 0; i < (int)(sizeof(sNumGlyphs) / sizeof(sNumGlyphs[0])); i++) {
        if (c == sNumGlyphs[i]) {
            return i + 18;
        }
    }
    return -1;
}

static const wchar_t sTempGlyphs[47] = {
    L'0', L'1', L'2', L'3', L'4', L'5', L'6', L'7', L'8', L'9', L'-', L'C', L'F', 0xFF9F,
};

static inline int GetTempGlyph(wchar_t c) {
    for (int i = 0; i < (int)(sizeof(sTempGlyphs) / sizeof(sTempGlyphs[0])); i++) {
        if (c == sTempGlyphs[i]) {
            return i + 33;
        }
    }
    return -1;
}

static inline BOOL IsNearZero(f32 x) {
    return x < 0.0008f && x > -0.0008f;
}

static inline void DrawGlyph(int index, const Vec2& at, f32 scaleX, f32 scaleY) {
    f32 halfW = 0.5f * sGlyphTextures[index].width;
    f32 offX = halfW * scaleX;
    f32 halfH = 0.5f * sGlyphTextures[index].height;
    f32 offY = halfH * scaleY;
    f32 px, py;
    py = at.y - offY;
    px = at.x - offX;
    Vec pos;
    pos.x = px;
    pos.y = py;
    pos.z = 0.0f;
    DrawTextureAt(gCommonTpl, sGlyphTextures[index].texture, scaleX, scaleY, &pos);
}

static inline f32 CalcDateWidthImpl(const wchar_t* str, u32 len) {
    f32 width = 0.0f;
    u32 i = 0;
    const wchar_t* p = str;
    int k;
    int glyph;
    wchar_t c;
    for (; i < len; i++, p++) {
        c = *p;
        for (k = 0; k < (int)(sizeof(sDateGlyphs) / sizeof(sDateGlyphs[0])); k++) {
            if (c == sDateGlyphs[k]) {
                glyph = k;
                goto found;
            }
        }
        glyph = -1;
    found:
        if (glyph >= 0) {
            width += sGlyphTextures[glyph].width;
        }
    }
    return width;
}

f32 CalcDateWidth(const wchar_t* str) {
    u32 len = wcslen(str);
    f32 width = 0.0f;
    const wchar_t* p = str;
    for (u32 i = 0; i < len; i++, p++) {
        int glyph = GetDateGlyph(*p);
        if (glyph >= 0) {
            width += sGlyphTextures[glyph].width;
        }
    }
    return width;
}

void DrawDateCentered(const wchar_t* str, const Vec2* pos, f32 scaleX, f32 scaleY, const GXColor* color,
                      const GXColor* shadowColor) {
    f32 width = CalcDateWidthImpl(str, wcslen(str));
    f32 halfWidth = 0.5f * width;
    int first = GetDateGlyph(*str);
    Vec2 start;
    start.y = pos->y;
    start.x = pos->x - scaleX * (halfWidth - 0.5f * sGlyphTextures[first].width);
    DrawDate(str, &start, scaleX, scaleY, color, shadowColor);
}

// Views a GXColor as a ut::Color so the tev colour argument is a class lvalue.
static inline const nw4r::ut::Color& AsColor(const GXColor* c) {
    return *static_cast<const nw4r::ut::Color*>(c);
}

void DrawDate(const wchar_t* str, const Vec2* pos, f32 scaleX, f32 scaleY, const GXColor* color,
              const GXColor* shadowColor) {
    Vec2 main = *pos;
    Vec2 shadow;
    f32 sy = 2.0f + pos->y;
    f32 sx = 2.0f + pos->x;
    shadow.x = sx;
    shadow.y = sy;

    u32 len = wcslen(str);
    f32 prevWidth;

    for (u32 i = 0; i < len; i++, str++) {
        int index = GetDateGlyph(*str);
        if (index >= 0) {
            if (i != 0) {
                f32 m1 = -1.0f;
                prevWidth = scaleX * (m1 + (0.5f * prevWidth + 0.5f * sGlyphTextures[index].width));
                main.x += prevWidth;
                shadow.x += prevWidth;
            }

            GXSetTevColor(GX_TEVREG0, AsColor(shadowColor));
            DrawGlyph(index, shadow, scaleX, scaleY);
            GXSetTevColor(GX_TEVREG0, AsColor(color));
            DrawGlyph(index, main, scaleX, scaleY);

            prevWidth = sGlyphTextures[index].width;
        }
    }
}

static inline f32 CalcNumWidthImpl(const wchar_t* str, u32 len, f32 spacing) {
    f32 width = 0.0f;
    u32 i = 0;
    const wchar_t* p = str;
    u32 c;
    int k;
    int glyph;
    for (; i < len; i++, p++) {
        c = *p;
        for (k = 0; k < (int)(sizeof(sNumGlyphs) / sizeof(sNumGlyphs[0])); k++) {
            if (c == sNumGlyphs[k]) {
                glyph = k + 18;
                goto found;
            }
        }
        glyph = -1;
    found:
        if (glyph >= 0) {
            width += sGlyphTextures[glyph].width;
        }
    }

    if (!IsNearZero(spacing)) {
        width += spacing * (len - 1);
    }

    return width;
}

f32 CalcNumWidth(const wchar_t* str, f32 spacing) {
    u32 len = wcslen(str);
    f32 width = 0.0f;
    const wchar_t* p = str;
    for (u32 i = 0; i < len; i++, p++) {
        int glyph = GetNumGlyph(*p);
        if (glyph >= 0) {
            width += sGlyphTextures[glyph].width;
        }
    }

    if (!IsNearZero(spacing)) {
        width += spacing * (len - 1);
    }

    return width;
}

void DrawNumCentered(const wchar_t* str, const Vec2* pos, f32 scaleX, f32 scaleY, f32 spacing, const GXColor* color,
                     const GXColor* color2) {
    f32 width = CalcNumWidthImpl(str, wcslen(str), spacing);
    f32 halfWidth = 0.5f * width;
    int first = GetNumGlyph(*str);
    Vec2 start;
    start.y = pos->y;
    start.x = pos->x - scaleX * (halfWidth - 0.5f * sGlyphTextures[first].width);
    DrawNum(str, &start, scaleX, scaleY, spacing, color, color2);
}

void DrawNum(const wchar_t* str, const Vec2* pos, f32 scaleX, f32 scaleY, f32 spacing, const GXColor* color,
             const GXColor* color2) {
    Vec2 cur = *pos;
    u32 len = wcslen(str);

    if (IsNearZero(spacing)) {
        spacing = 0.0f;
    }

    GXSetTevColor(GX_TEVREG0, *color);
    GXSetTevColor(GX_TEVREG1, *color2);

    f32 prevWidth;

    for (u32 i = 0; i < len; i++, str++) {
        int index = GetNumGlyph(*str);
        if (index >= 0) {
            if (i != 0) {
                prevWidth = scaleX * (spacing + 0.5f * (prevWidth + sGlyphTextures[index].width));
                cur.x += prevWidth;
            }

            DrawGlyph(index, cur, scaleX, scaleY);
            prevWidth = sGlyphTextures[index].width;
        }
    }
}

void DrawNumRightAligned(const wchar_t* str, const Vec2* pos, f32 scaleX, f32 scaleY, const GXColor* color,
                         const GXColor* color2) {
    u32 i;
    f32 narrowSpacing;
    f32 wideSpacing;
    const wchar_t* p;
    u32 len;
    f32 prevWidth;
    f32 spacing;
    f32 parenOffset;
    Vec2 curStore = *pos;
    Vec2& cur = curStore;
    len = wcslen(str);
    narrowSpacing = -3.0f * scaleX;
    wideSpacing = -6.0f * scaleX;
    parenOffset = 2.0f * scaleX;

    GXSetTevColor(GX_TEVREG0, AsColor(color));
    GXSetTevColor(GX_TEVREG1, AsColor(color2));

    p = str + (len - 1);
    for (i = 0; i < len; i++, p--) {
        int index = GetNumGlyph(*p);
        if (index >= 0) {
            if (i != 0) {
                prevWidth = 0.5f * (prevWidth + sGlyphTextures[index].width);
                cur.x -= spacing + prevWidth * scaleX;
                if (*p == L'(') {
                    cur.x += parenOffset;
                }
            }

            DrawGlyph(index, cur, scaleX, scaleY);
            prevWidth = sGlyphTextures[index].width;

            switch (*p) {
            case L')':
                spacing = wideSpacing;
                break;
            default:
                spacing = narrowSpacing;
                break;
            }
        }
    }
}

static inline f32 CalcTempWidthImpl(const wchar_t* str, u32 len) {
    f32 width = 0.0f;
    u32 i = 0;
    const wchar_t* p = str;
    u32 c;
    int k;
    int glyph;
    for (; i < len; i++, p++) {
        c = *p;
        for (k = 0; k < (int)(sizeof(sTempGlyphs) / sizeof(sTempGlyphs[0])); k++) {
            if (c == sTempGlyphs[k]) {
                glyph = k + 33;
                goto found;
            }
        }
        glyph = -1;
    found:
        if (glyph >= 0) {
            width += sGlyphTextures[glyph].width;
        }
    }
    return width;
}

f32 CalcTempWidth(const wchar_t* str) {
    u32 len = wcslen(str);
    f32 width = 0.0f;
    const wchar_t* p = str;
    for (u32 i = 0; i < len; i++, p++) {
        int glyph = GetTempGlyph(*p);
        if (glyph >= 0) {
            width += sGlyphTextures[glyph].width;
        }
    }
    return width;
}

f32 GetTempGlyphHeight(const wchar_t* str) {
    int glyph = GetTempGlyph(*str);
    if (glyph >= 0) {
        return sGlyphTextures[glyph].height;
    }
    return 0.0f;
}

void DrawTempCentered(const wchar_t* str, const Vec2* pos, f32 scaleX, f32 scaleY, const GXColor* color,
                      const GXColor* shadowColor) {
    f32 width = CalcTempWidthImpl(str, wcslen(str));
    f32 halfWidth = 0.5f * width;
    int first = GetTempGlyph(*str);
    Vec2 start;
    start.y = pos->y;
    start.x = pos->x - scaleX * (halfWidth - 0.5f * sGlyphTextures[first].width);
    DrawTemp(str, &start, scaleX, scaleY, color, shadowColor);
}

void DrawTemp(const wchar_t* str, const Vec2* pos, f32 scaleX, f32 scaleY, const GXColor* color,
              const GXColor* shadowColor) {
    Vec2 main = *pos;
    Vec2 shadow;
    f32 sy = 2.0f + pos->y;
    f32 sx = 2.0f + pos->x;
    shadow.x = sx;
    shadow.y = sy;

    u32 len = wcslen(str);
    f32 prevWidth;

    for (u32 i = 0; i < len; i++, str++) {
        int index = GetTempGlyph(*str);
        if (index >= 0) {
            if (i != 0) {
                prevWidth = scaleX * (0.5f * prevWidth + 0.5f * sGlyphTextures[index].width);
                main.x += prevWidth;
                shadow.x += prevWidth;
            }

            GXSetTevColor(GX_TEVREG0, AsColor(shadowColor));
            DrawGlyph(index, shadow, scaleX, scaleY);
            GXSetTevColor(GX_TEVREG0, AsColor(color));
            DrawGlyph(index, main, scaleX, scaleY);

            prevWidth = sGlyphTextures[index].width;
        }
    }
}

u32 GetLanguageTexture() {
    return sLanguageTextures[gLanguage];
}

void SceneBase::OnShutdown() {
    Exit(TRUE, 2);
}

void SceneBase::OnPowerButton() {
    if (gHomeButton != NULL) {
        gHomeButton->OnReset();
    }
}

void SceneBase::OnResetButton() {
    if (gHomeButton != NULL) {
        gHomeButton->OnReset();
    }
}

void SceneBase::Init() {}
