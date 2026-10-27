#include "vcmv/vcmv_www.h"
#include "revolution/OS/OSError.h"
#include "revolution/OS/OSCache.h"
#include "revolution/NAND/nand.h"
#include "revolution/NAND/NANDOpenClose.h"
#include <cstring>
#include "revolution/CNT/cnt.h"
#include "revolution/SC/scapi.h"
#include "revolution/VI/vi.h"
#include "types.h"


// strangely, this doesn't get "used" by any serious function
BOOL GetCurrentCalendarTime(OSCalendarTime* time) {
    s64 ticks;

    ticks = OSGetTick();
    OSTicksToCalendarTime(ticks, time);
    return TRUE;
}

void wwwUnlinkedFunction(void){
    OSReport("\nError: call www unlinked function.\n");
    return;
}


// {path, buffer, size} entry, lists end with a null path
struct WWWNandFile {
    char* path; // at 0x0
    void* buf;  // at 0x4
    u32 size;   // at 0x8
};

struct WWWFont {
    const char* name; // at 0x0
    u32 index;        // at 0x4
    void* buf;        // at 0x8
    void* end;        // at 0xC
};

// streaming LZ77 decoder state
struct WWWLZState {
    u8 window[0x1000]; // at 0x0
    const u8* src;     // at 0x1000
    u32 outPos;        // at 0x1004
    s32 remaining;     // at 0x1008
    u32 bit;           // at 0x100C
    u8 flags;          // at 0x1010
    u32 offset;        // at 0x1014
    s32 copyLen;       // at 0x1018
};

// the relocatable module (RSO) containing the www library
struct WWWModule {
    u8 pad0[0x1C];           // at 0x0
    u32 bssSize;             // at 0x1C
    u8 pad20[4];             // at 0x20
    void (*prolog)(void);    // at 0x24
    void (*epilog)(void);    // at 0x28
    u8 pad2C[4];             // at 0x2C
    u32 exportTable;         // at 0x30
    u8 pad34[4];             // at 0x34
    u32 exportStrings;       // at 0x38
    u8 pad3C[0x10];          // at 0x3C
    u32 importTable;         // at 0x4C
    u8 pad50[4];             // at 0x50
    u32 importStrings;       // at 0x54
};

struct WWWImport {
    const char* name; // at 0x0
    void** slot;      // at 0x4
};

BOOL vcmvAlloc(void** out, u32 size, MEMAllocator* primary, MEMAllocator* fallback);
void vcmvFree(void** ptr);
void vcmvLinkRSO(void* image);

extern "C" {
void* RSOFindExportSymbolAddr(WWWModule* module, const char* name);
void RSOLinkList(WWWModule* module, void* bss);
u32 RSOGetFixedSize(void* header, int which);
}

// Auto-generated data definitions for vcmv_www.cpp

// .rodata strings
const char sSym_WWWSurfaceInit[] = "WWWSurfaceInit";
const char sSym_WWWSurfaceNewScreen[] = "WWWSurfaceNewScreen";
const char sSym_WWWSurfaceDeleteScreen[] = "WWWSurfaceDeleteScreen";
const char sSym_WWWSurfaceResize[] = "WWWSurfaceResize";
const char sSym_WWWSurfaceShutdown[] = "WWWSurfaceShutdown";
const char sSym_WWWSurfaceSetFlushCallback[] = "WWWSurfaceSetFlushCallback";
const char sSym_WWWSurfaceInvalidate[] = "WWWSurfaceInvalidate";
const char sSym_WWWSurfaceUpdateScreen[] = "WWWSurfaceUpdateScreen";
const char sSym_WWWSurfaceLockArea[] = "WWWSurfaceLockArea";
const char sSym_WWWSurfaceUnlockArea[] = "WWWSurfaceUnlockArea";
const char sSym_WWWSurfaceMouseEvt[] = "WWWSurfaceMouseEvt";
const char sSym_WWWSurfaceWheelEvt[] = "WWWSurfaceWheelEvt";
const char sSym_WWWSurfaceKeyboardEvt[] = "WWWSurfaceKeyboardEvt";
const char sSym_WWWSurfaceAddFont[] = "WWWSurfaceAddFont";
const char sSym_WWWCreateBrowser[] = "WWWCreateBrowser";
const char sSym_WWWTerminateBrowser[] = "WWWTerminateBrowser";
const char sSym_WWWRunSlice[] = "WWWRunSlice";
const char sSym_WWWCreateBrowserWindow[] = "WWWCreateBrowserWindow";
const char sSym_WWWCloseBrowserWindow[] = "WWWCloseBrowserWindow";
const char sSym_WWWSetBrowserWindowTransparent[] = "WWWSetBrowserWindowTransparent";
const char sSym_WWWGetBrowserWindowRect[] = "WWWGetBrowserWindowRect";
const char sSym_WWWSetBrowserWindowRect[] = "WWWSetBrowserWindowRect";
const char sSym_WWWRaiseBrowserWindow[] = "WWWRaiseBrowserWindow";
const char sSym_WWWLowerBrowserWindow[] = "WWWLowerBrowserWindow";
const char sSym_WWWShowBrowserWindow[] = "WWWShowBrowserWindow";
const char sSym_WWWHideBrowserWindow[] = "WWWHideBrowserWindow";
const char sSym_WWWCommitIme[] = "WWWCommitIme";
const char sSym_WWWUpdateIme[] = "WWWUpdateIme";
const char sSym_WWWPostUrl[] = "WWWPostUrl";
const char sSym_WWWOpenUrl[] = "WWWOpenUrl";
const char sSym_WWWGetHistoryCount[] = "WWWGetHistoryCount";
const char sSym_WWWNextPage[] = "WWWNextPage";
const char sSym_WWWPrevPage[] = "WWWPrevPage";
const char sSym_WWWMoveInHistory[] = "WWWMoveInHistory";
const char sSym_WWWReload[] = "WWWReload";
const char sSym_WWWReflow[] = "WWWReflow";
const char sSym_WWWSearch[] = "WWWSearch";
const char sSym_WWWResetSearch[] = "WWWResetSearch";
const char sSym_WWWSetFocus[] = "WWWSetFocus";
const char sSym_WWWLoseFocus[] = "WWWLoseFocus";
const char sSym_WWWHistory[] = "WWWHistory";
const char sSym_WWWClearHistory[] = "WWWClearHistory";
const char sSym_WWWGetTrueZoom[] = "WWWGetTrueZoom";
const char sSym_WWWSetTrueZoom[] = "WWWSetTrueZoom";
const char sSym_WWWGetZoom[] = "WWWGetZoom";
const char sSym_WWWSetZoom[] = "WWWSetZoom";
const char sSym_WWWGetSecurityMode[] = "WWWGetSecurityMode";
const char sSym_WWWGetScroll[] = "WWWGetScroll";
const char sSym_WWWSetScroll[] = "WWWSetScroll";
const char sSym_WWWSetRenderingMode[] = "WWWSetRenderingMode";
const char sSym_WWWGetRenderingMode[] = "WWWGetRenderingMode";
const char sSym_WWWSetImageMode[] = "WWWSetImageMode";
const char sSym_WWWCreateCertificateManager[] = "WWWCreateCertificateManager";
const char sSym_WWWCloseCertificateManager[] = "WWWCloseCertificateManager";
const char sSym_WWWGetNumberOfCertificates[] = "WWWGetNumberOfCertificates";
const char sSym_WWWSetLanguageEncoding[] = "WWWSetLanguageEncoding";
const char sSym_WWWGetDocumentIcon[] = "WWWGetDocumentIcon";
const char sSym_WWWGetDocumentIconUrl[] = "WWWGetDocumentIconUrl";
const char sSym_WWWClearCookies[] = "WWWClearCookies";
const char sSym_WWWGetDocumentSize[] = "WWWGetDocumentSize";
const char sSym_WWWSetIntPref[] = "WWWSetIntPref";
const char sSym_WWWGetIntPref[] = "WWWGetIntPref";
const char sSym_WWWSetStringPref[] = "WWWSetStringPref";
const char sSym_WWWGetStringPref[] = "WWWGetStringPref";
const char sSym_WWWCommitPrefs[] = "WWWCommitPrefs";
const char sSym_WWWSetFocusColors[] = "WWWSetFocusColors";
const char sSym_WWWSetScrollbarColors[] = "WWWSetScrollbarColors";
const char sSym_WWWSetScrollbarSize[] = "WWWSetScrollbarSize";
const char sSym_WWWSetWidgetColors[] = "WWWSetWidgetColors";
const char sSym_WWWSetDisabledWidgetColors[] = "WWWSetDisabledWidgetColors";
const char sSym_WWWSetButtonWidgetColors[] = "WWWSetButtonWidgetColors";
const char sSym_WWWSetUastringExtension[] = "WWWSetUastringExtension";
const char sSym_WWWMarkNextItemInDirection[] = "WWWMarkNextItemInDirection";
const char sSym_WWWResetNavigation[] = "WWWResetNavigation";
const char sSym_WWWClearHighlight[] = "WWWClearHighlight";
const char sSym_WWWSetHighlight[] = "WWWSetHighlight";
const char sSym_WWWGetActiveLinkType[] = "WWWGetActiveLinkType";
const char sSym_WWWGetBrowserAllocationFunctions[] = "WWWGetBrowserAllocationFunctions";
const char sSym_WWWShutdownBrowserAllocationFunctions[] = "WWWShutdownBrowserAllocationFunctions";
const char sSym_WWWSetAllocationFunctions[] = "WWWSetAllocationFunctions";
const char sSym_WWWHTTPCreateHttpLib[] = "WWWHTTPCreateHttpLib";
const char sSym_WWWHTTPTerminateHttpLib[] = "WWWHTTPTerminateHttpLib";
const char sSym_WWWHTTPSessionRunSlice[] = "WWWHTTPSessionRunSlice";
const char sSym_WWWHTTPInitSession[] = "WWWHTTPInitSession";
const char sSym_WWWHTTPDeleteSession[] = "WWWHTTPDeleteSession";
const char sSym_WWWHTTPSetSessionHeader[] = "WWWHTTPSetSessionHeader";
const char sSym_WWWHTTPRemoveSessionHeader[] = "WWWHTTPRemoveSessionHeader";
const char sSym_WWWHTTPRemoveAllSessionHeaders[] = "WWWHTTPRemoveAllSessionHeaders";
const char sSym_WWWHTTPCreateRequest[] = "WWWHTTPCreateRequest";
const char sSym_WWWHTTPDeleteRequest[] = "WWWHTTPDeleteRequest";
const char sSym_WWWHTTPSetRequestHeader[] = "WWWHTTPSetRequestHeader";
const char sSym_WWWHTTPGetRequestHeader[] = "WWWHTTPGetRequestHeader";
const char sSym_WWWHTTPRemoveRequestHeader[] = "WWWHTTPRemoveRequestHeader";
const char sSym_WWWHTTPRemoveAllRequestHeaders[] = "WWWHTTPRemoveAllRequestHeaders";
const char sSym_WWWHTTPGetResponseHeader[] = "WWWHTTPGetResponseHeader";
const char sSym_WWWHTTPGetResponseHeaders[] = "WWWHTTPGetResponseHeaders";
const char sSym_WWWHTTPIssue[] = "WWWHTTPIssue";
const char sSym_WWWHTTPNbActiveRequests[] = "WWWHTTPNbActiveRequests";
const char sSym_WWWHTTPPostBodyData[] = "WWWHTTPPostBodyData";
const char sSym_WWWHTTPSetAuthCredentials[] = "WWWHTTPSetAuthCredentials";
const char sSym_WWWHTTPRemoveAuthCredentials[] = "WWWHTTPRemoveAuthCredentials";
const char sSym_WWWHTTPSetProxy[] = "WWWHTTPSetProxy";
const char sSym_WWWHTTPRemoveProxy[] = "WWWHTTPRemoveProxy";
const char sSym_WWWHTTPEndLoading[] = "WWWHTTPEndLoading";
const char sSym_WWWAddJSPlugin[] = "WWWAddJSPlugin";
const char sSym_WWWAddNSPlugin[] = "WWWAddNSPlugin";
const char sSym_WWWProtocolWrite[] = "WWWProtocolWrite";
const char sSym_WWWProtocolSetMimeType[] = "WWWProtocolSetMimeType";
const char sSym_WWWProtocolFinished[] = "WWWProtocolFinished";
const char sSym_WWWProtocolFailed[] = "WWWProtocolFailed";
const char sSym_WWWAddProtocol[] = "WWWAddProtocol";
const char sFontFileName[] = "WiiNTLG-Regular.ttc";
const char sSettingsPath[] = "/shared2/menu/vc/settings.sav";


// .sdata2
#pragma push
#pragma section all_types ".sdata2" ".sdata2"
const char sSym_WWWStop[8] = "WWWStop";
const char sEmptyStringSdata2[8] = {0};
const char sFontWeightRegular[8] = "Regular";
#pragma pop

// .sbss
void* pWWWSurfaceInit;
void* pWWWSurfaceNewScreen;
void* pWWWSurfaceDeleteScreen;
void* pWWWSurfaceResize;
void* pWWWSurfaceShutdown;
void* pWWWSurfaceSetFlushCallback;
void* pWWWSurfaceInvalidate;
void* pWWWSurfaceUpdateScreen;
void* pWWWSurfaceLockArea;
void* pWWWSurfaceUnlockArea;
void* pWWWSurfaceMouseEvt;
void* pWWWSurfaceWheelEvt;
void* pWWWSurfaceKeyboardEvt;
void (*pWWWSurfaceAddFont)(const char* name);
void* pWWWCreateBrowser;
void* pWWWTerminateBrowser;
void* pWWWRunSlice;
void* pWWWCreateBrowserWindow;
void* pWWWCloseBrowserWindow;
void* pWWWSetBrowserWindowTransparent;
void* pWWWGetBrowserWindowRect;
void* pWWWSetBrowserWindowRect;
void* pWWWRaiseBrowserWindow;
void* pWWWLowerBrowserWindow;
void* pWWWShowBrowserWindow;
void* pWWWHideBrowserWindow;
void* pWWWCommitIme;
void* pWWWUpdateIme;
void* pWWWPostUrl;
void* pWWWOpenUrl;
void* pWWWGetHistoryCount;
void* pWWWNextPage;
void* pWWWPrevPage;
void* pWWWMoveInHistory;
void* pWWWStop;
void* pWWWReload;
void* pWWWReflow;
void* pWWWSearch;
void* pWWWResetSearch;
void* pWWWSetFocus;
void* pWWWLoseFocus;
void* pWWWHistory;
void* pWWWClearHistory;
void* pWWWGetTrueZoom;
void* pWWWSetTrueZoom;
void* pWWWGetZoom;
void* pWWWSetZoom;
void* pWWWGetSecurityMode;
void* pWWWGetScroll;
void* pWWWSetScroll;
void* pWWWSetRenderingMode;
void* pWWWGetRenderingMode;
void* pWWWSetImageMode;
void* pWWWCreateCertificateManager;
void* pWWWCloseCertificateManager;
void* pWWWGetNumberOfCertificates;
void* pWWWSetLanguageEncoding;
void* pWWWGetDocumentIcon;
void* pWWWGetDocumentIconUrl;
void* pWWWClearCookies;
void* pWWWGetDocumentSize;
void* pWWWSetIntPref;
void* pWWWGetIntPref;
void* pWWWSetStringPref;
void* pWWWGetStringPref;
void* pWWWCommitPrefs;
void* pWWWSetFocusColors;
void* pWWWSetScrollbarColors;
void* pWWWSetScrollbarSize;
void* pWWWSetWidgetColors;
void* pWWWSetDisabledWidgetColors;
void* pWWWSetButtonWidgetColors;
void* pWWWSetUastringExtension;
void* pWWWMarkNextItemInDirection;
void* pWWWResetNavigation;
void* pWWWClearHighlight;
void* pWWWSetHighlight;
void* pWWWGetActiveLinkType;
void* pWWWGetBrowserAllocationFunctions;
void* pWWWShutdownBrowserAllocationFunctions;
void* pWWWSetAllocationFunctions;
void* pWWWHTTPCreateHttpLib;
void* pWWWHTTPTerminateHttpLib;
void* pWWWHTTPSessionRunSlice;
void* pWWWHTTPInitSession;
void* pWWWHTTPDeleteSession;
void* pWWWHTTPSetSessionHeader;
void* pWWWHTTPRemoveSessionHeader;
void* pWWWHTTPRemoveAllSessionHeaders;
void* pWWWHTTPCreateRequest;
void* pWWWHTTPDeleteRequest;
void* pWWWHTTPSetRequestHeader;
void* pWWWHTTPGetRequestHeader;
void* pWWWHTTPRemoveRequestHeader;
void* pWWWHTTPRemoveAllRequestHeaders;
void* pWWWHTTPGetResponseHeader;
void* pWWWHTTPGetResponseHeaders;
void* pWWWHTTPIssue;
void* pWWWHTTPNbActiveRequests;
void* pWWWHTTPPostBodyData;
void* pWWWHTTPSetAuthCredentials;
void* pWWWHTTPRemoveAuthCredentials;
void* pWWWHTTPSetProxy;
void* pWWWHTTPRemoveProxy;
void* pWWWHTTPEndLoading;
void* pWWWAddJSPlugin;
void* pWWWAddNSPlugin;
void* pWWWProtocolWrite;
void* pWWWProtocolSetMimeType;
void* pWWWProtocolFinished;
void* pWWWProtocolFailed;
void* pWWWAddProtocol;
u8 wwwAspectRatio;
u8* wwwAspectPtr;
WWWModule* wwwModule;
void* wwwModuleBss;

// .bss
WWWFont wwwFontList[4];
WWWSettings wwwSettingsBuf;
WWWNandFile wwwFileList[1];
NANDFileInfo wwwNandFileInfo;

// .sdata
const char* wwwFontPaths[2] = { sFontFileName, sEmptyStringSdata2 };
const char* wwwFontNames[1] = { sFontWeightRegular };



// .data
u32 sEmbeddedWwwRso[960] __attribute__((aligned(32))) = {
    0x00000000, 0x00000000, 0x00000019, 0x00000058, 0x00000120, 0x00000022, 0x00000001, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000964, 0x00000000, 0x00000964, 0x00000000,
    0x00000144, 0x00000820, 0x00000964, 0x00000964, 0x00000000, 0x00000EF4, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x633A5C76, 0x636D765C, 0x76636D76, 0x74657374, 0x2E62696E, 0x5C56636D, 0x76546573, 0x744E2E65,
    0x6C660000, 0x000000CB, 0x00007AC8, 0x00000002, 0x00006793, 0x00000223, 0x000147DC, 0x00000002,
    0x00006A63, 0x00000245, 0x00014D44, 0x00000002, 0x00006CF0, 0x0000024E, 0x00014D4C, 0x00000002,
    0x00007357, 0x00000252, 0x00014D50, 0x00000002, 0x00007767, 0x0000022D, 0x00014BE8, 0x00000002,
    0x000079FE, 0x00000231, 0x00014CC0, 0x00000002, 0x00007A7E, 0x00000235, 0x00014D38, 0x00000002,
    0x00067A63, 0x0000023A, 0x00014D3C, 0x00000002, 0x000689FE, 0x00000219, 0x00014430, 0x00000002,
    0x00068A7E, 0x000001BE, 0x0000EF74, 0x00000002, 0x00068B59, 0x000001C3, 0x0000F038, 0x00000002,
    0x00068B5C, 0x0000021E, 0x00014670, 0x00000002, 0x00069BFC, 0x00000400, 0x0008B1A8, 0x00000002,
    0x0006CF04, 0x00000249, 0x00014D48, 0x00000002, 0x0006D454, 0x00000214, 0x00010D34, 0x00000002,
    0x00070B51, 0x000000CF, 0x00007AD8, 0x00000002, 0x00072793, 0x00000143, 0x0000B508, 0x00000002,
    0x00078844, 0x00000256, 0x00014FA0, 0x00000002, 0x0007A894, 0x000001FD, 0x00010C14, 0x00000002,
    0x00679694, 0x0000023F, 0x00014D40, 0x00000002, 0x0068A812, 0x000001C8, 0x0000F5F8, 0x00000002,
    0x006A359B, 0x00000227, 0x000148B0, 0x00000002, 0x006D3662, 0x0000013D, 0x0000B398, 0x00000002,
    0x0078A694, 0x00000148, 0x0000B528, 0x00000002, 0x007A8844, 0x00000322, 0x0002E55C, 0x00000002,
    0x01337EA5, 0x00000533, 0x0008DA90, 0x00000002, 0x028A84F5, 0x00000496, 0x0008B668, 0x00000002,
    0x028F0614, 0x0000039D, 0x0008237C, 0x00000002, 0x0297BC9E, 0x000004E1, 0x0008CE1C, 0x00000002,
    0x029EE2C5, 0x0000053D, 0x0008DB48, 0x00000002, 0x02F04F2E, 0x00000391, 0x00044514, 0x00000002,
    0x032033A2, 0x000000BB, 0x00003880, 0x00000002, 0x0364E130, 0x000000EC, 0x0000895C, 0x00000002,
    0x03C446B5, 0x0000041B, 0x0008B458, 0x00000002, 0x04302174, 0x000003B8, 0x00084134, 0x00000002,
    0x048684A3, 0x00000558, 0x0008E828, 0x00000002, 0x053F4512, 0x00000450, 0x0008B4C8, 0x00000002,
    0x0540CD44, 0x0000008C, 0x00003268, 0x00000002, 0x055B09E9, 0x000000C3, 0x00000150, 0x00000006,
    0x055D02A3, 0x00000094, 0x00003484, 0x00000002, 0x056457E9, 0x0000009C, 0x00003590, 0x00000002,
    0x0569EFE9, 0x00000119, 0x0000B0EC, 0x00000002, 0x05789806, 0x0000012C, 0x0000B1F0, 0x00000002,
    0x05790206, 0x000002EF, 0x0002A830, 0x00000002, 0x05801684, 0x0000052A, 0x0008D838, 0x00000002,
    0x0629639E, 0x000004D8, 0x0008CD3C, 0x00000002, 0x06298E54, 0x000004EB, 0x0008CEFC, 0x00000002,
    0x06299E9B, 0x0000029C, 0x0002A0DC, 0x00000002, 0x066866FE, 0x0000037B, 0x000441F0, 0x00000002,
    0x0670B192, 0x0000030E, 0x0002A840, 0x00000002, 0x067A32F5, 0x000000B2, 0x0000377C, 0x00000002,
    0x069CB710, 0x00000015, 0x000027D4, 0x00000002, 0x06C75ED7, 0x000001F6, 0x00010B34, 0x00000002,
    0x06DCAC56, 0x000003D8, 0x00089ADC, 0x00000002, 0x072E5CE5, 0x00000576, 0x801B3CA0, 0x0000FFF1,
    0x0730202F, 0x000000F4, 0x00008A28, 0x00000002, 0x073C39F2, 0x000000FB, 0x00008A80, 0x00000002,
    0x073C3A40, 0x00000000, 0x00000000, 0x00000001, 0x073C3A79, 0x00000007, 0x00000104, 0x00000001,
    0x073C49C4, 0x000001CE, 0x0000F5FC, 0x00000002, 0x0742B035, 0x00000102, 0x0000AEE8, 0x00000002,
    0x077905A6, 0x000002DA, 0x0002A81C, 0x00000002, 0x079C79DD, 0x0000014E, 0x0000C9D4, 0x00000002,
    0x07A99846, 0x00000164, 0x0000CC50, 0x00000002, 0x07AB8984, 0x00000182, 0x0000CE24, 0x00000002,
    0x07AB89F2, 0x00000173, 0x0000CCC8, 0x00000002, 0x07AB8A40, 0x00000155, 0x0000CB4C, 0x00000002,
    0x07AB8A79, 0x0000000E, 0x000027B8, 0x00000002, 0x07AB92BE, 0x00000199, 0x0000CF44, 0x00000002,
    0x07AB9A6E, 0x000001A8, 0x0000D08C, 0x00000002, 0x07AB9AB2, 0x000001B7, 0x0000EE88, 0x00000002,
    0x07AB9B5C, 0x000001E8, 0x00000050, 0x00000008, 0x080E9025, 0x00000581, 0x801B5080, 0x0000FFF1,
    0x08521FEF, 0x00000349, 0x00043D6C, 0x00000002, 0x085F7DD2, 0x000002C9, 0x0002A634, 0x00000002,
    0x085F8122, 0x0000025B, 0x0001E7AC, 0x00000002, 0x0879D202, 0x000003AF, 0x000840A4, 0x00000002,
    0x088C73D4, 0x000004C2, 0x0008CA6C, 0x00000002, 0x08AD9E85, 0x000004CD, 0x0008CBCC, 0x00000002,
    0x08AF0A85, 0x00000386, 0x00044488, 0x00000002, 0x08B31192, 0x00000317, 0x0002A91C, 0x00000002,
    0x08B3ED82, 0x0000035A, 0x00043D8C, 0x00000002, 0x08E2CE7E, 0x000004F4, 0x0008D04C, 0x00000002,
    0x08E5E992, 0x000002BD, 0x0002A380, 0x00000002, 0x08EF017E, 0x000001D5, 0x0000F6D4, 0x00000002,
    0x0983DD65, 0x00000405, 0x0008B22C, 0x00000002, 0x098EF4C4, 0x000000D4, 0x00007D3C, 0x00000002,
    0x099B88F8, 0x00000500, 0x0008D11C, 0x00000002, 0x09E811D2, 0x000004AF, 0x0008B784, 0x00000002,
    0x0A3718C4, 0x00000123, 0x0000B170, 0x00000002, 0x0A790276, 0x00000135, 0x0000B2C8, 0x00000002,
    0x0A7905D6, 0x00000366, 0x00043DF4, 0x00000002, 0x0A8A3205, 0x0000001E, 0x0000289C, 0x00000002,
    0x0AB2C014, 0x000001A0, 0x0000CFE8, 0x00000002, 0x0AB8AA1E, 0x00000203, 0x00010C88, 0x00000002,
    0x0AB8FA30, 0x0000016B, 0x0000CC7C, 0x00000002, 0x0AB949F4, 0x0000015C, 0x0000CC0C, 0x00000002,
    0x0AB94A09, 0x0000017A, 0x0000CDE4, 0x00000002, 0x0AB94A30, 0x00000191, 0x0000CE9C, 0x00000002,
    0x0AB969FB, 0x00000189, 0x0000CE54, 0x00000002, 0x0AB98982, 0x000001AF, 0x0000EDE0, 0x00000002,
    0x0AB9B6CC, 0x000000DC, 0x000005A8, 0x00000006, 0x0ACD7865, 0x0000036F, 0x000440C0, 0x00000002,
    0x0B30D17F, 0x0000050E, 0x0008D30C, 0x00000002, 0x0B4AC338, 0x000001DF, 0x0000FBE0, 0x00000002,
    0x0B8DA995, 0x0000020B, 0x00010D30, 0x00000002, 0x0B94FFE0, 0x0000051C, 0x0008D63C, 0x00000002,
    0x0BC80653, 0x00000330, 0x00043A64, 0x00000002, 0x0C42457D, 0x00000485, 0x0008B660, 0x00000002,
    0x0C5B16A4, 0x000003EA, 0x00089ED0, 0x00000002, 0x0CB2EB53, 0x000002A4, 0x0002A3D0, 0x00000002,
    0x0CBEB57D, 0x000003C0, 0x00088E94, 0x00000002, 0x0D207704, 0x00000273, 0x00002020, 0x00000006,
    0x0D3AF96F, 0x00000301, 0x0002A838, 0x00000002, 0x0D4E3368, 0x00000109, 0x0000AFB0, 0x00000002,
    0x0D7905C6, 0x00000111, 0x0000B074, 0x00000002, 0x0D7905D6, 0x000000A4, 0x000035B4, 0x00000002,
    0x0D8C808C, 0x0000004D, 0x00002E1C, 0x00000002, 0x0D965949, 0x0000056A, 0x0008EC64, 0x00000002,
    0x0D9BE2E5, 0x000003CE, 0x00089ABC, 0x00000002, 0x0DC9FC45, 0x00000465, 0x0008B4D0, 0x00000002,
    0x0E1417D4, 0x0000005F, 0x00002F14, 0x00000002, 0x0E359242, 0x0000006D, 0x00002FC8, 0x00000002,
    0x0E3FED1C, 0x0000028E, 0x0002A03C, 0x00000002, 0x0E905065, 0x00000037, 0x00002C5C, 0x00000002,
    0x0E953289, 0x00000475, 0x0008B538, 0x00000002, 0x0ED517C4, 0x0000054A, 0x0008DE38, 0x00000002,
    0x0EFCCFB5, 0x0000042F, 0x0008B4C4, 0x00000002, 0x0F743FD4, 0x0000007A, 0x00002FF0, 0x00000002,
    0x0FA7C7C4, 0x6D656D63, 0x7079006D, 0x656D7365, 0x74007374, 0x726C656E, 0x005F5F76, 0x615F6172,
    0x67005F5F, 0x72656769, 0x73746572, 0x5F676C6F, 0x62616C5F, 0x6F626A65, 0x6374005F, 0x5F636F6E,
    0x73747275, 0x63745F6E, 0x65775F61, 0x72726179, 0x005F5F63, 0x6F6E7374, 0x72756374, 0x5F617272,
    0x6179005F, 0x5F646573, 0x74726F79, 0x5F617272, 0x005F5F70, 0x746D665F, 0x7363616C, 0x6C005F5F,
    0x6376745F, 0x66703275, 0x6E736967, 0x6E656400, 0x5F5F6469, 0x76326900, 0x5F5F6D6F, 0x64326900,
    0x5F5F7368, 0x6C326900, 0x5F5F6376, 0x745F736C, 0x6C5F6462, 0x6C005F5F, 0x7365746A, 0x6D70006C,
    0x6F6E676A, 0x6D70005F, 0x5F66696C, 0x65730061, 0x6273006C, 0x61627300, 0x62736561, 0x72636800,
    0x5F637572, 0x72656E74, 0x5F6C6F63, 0x616C6500, 0x6D656D6D, 0x6F766500, 0x6D656D63, 0x6872006D,
    0x656D636D, 0x70007072, 0x696E7466, 0x00667072, 0x696E7466, 0x00767072, 0x696E7466, 0x0076736E,
    0x7072696E, 0x74660076, 0x73707269, 0x6E746600, 0x736E7072, 0x696E7466, 0x00737072, 0x696E7466,
    0x0071736F, 0x72740072, 0x616E6400, 0x7372616E, 0x64007373, 0x63616E66, 0x00737472, 0x63707900,
    0x7374726E, 0x63707900, 0x73747263, 0x61740073, 0x74726E63, 0x61740073, 0x7472636D, 0x70007374,
    0x726E636D, 0x70007374, 0x72636872, 0x00737472, 0x72636872, 0x00737472, 0x7062726B, 0x00737472,
    0x73706E00, 0x73747263, 0x73706E00, 0x73747273, 0x74720073, 0x7472746F, 0x756C0073, 0x7472746F,
    0x6C006174, 0x6F690061, 0x746F6C00, 0x636C6F63, 0x6B006D6B, 0x74696D65, 0x006C6F63, 0x616C7469,
    0x6D650073, 0x74726674, 0x696D6500, 0x5F5F646F, 0x75626C65, 0x5F687567, 0x65006765, 0x74656E76,
    0x0061626F, 0x72740073, 0x74726963, 0x6D700073, 0x74726E69, 0x636D7000, 0x69746F61, 0x00617461,
    0x6E006365, 0x696C0063, 0x6F730066, 0x6C6F6F72, 0x0073696E, 0x0074616E, 0x0061636F, 0x73006173,
    0x696E0061, 0x74616E32, 0x00657870, 0x00666D6F, 0x64006C6F, 0x6700706F, 0x77007371, 0x7274004E,
    0x45544765, 0x74556E69, 0x76657273, 0x616C4361, 0x6C656E64, 0x61720057, 0x57575F46, 0x4F4E545F,
    0x46494C45, 0x5F444154, 0x415F5441, 0x424C455F, 0x5F004152, 0x43496E69, 0x7448616E, 0x646C6500,
    0x4152434F, 0x70656E00, 0x41524343, 0x6F6E7665, 0x72745061, 0x7468546F, 0x456E7472, 0x796E756D,
    0x00415243, 0x46617374, 0x4F70656E, 0x00415243, 0x456E7472, 0x796E756D, 0x49734469, 0x72004152,
    0x43476574, 0x53746172, 0x74416464, 0x72496E4D, 0x656D0041, 0x52434765, 0x74537461, 0x72744F66,
    0x66736574, 0x00415243, 0x4765744C, 0x656E6774, 0x68004152, 0x43436C6F, 0x73650041, 0x52435265,
    0x61644469, 0x72005343, 0x4765744C, 0x616E6775, 0x61676500, 0x44564443, 0x6F6E7665, 0x72745061,
    0x7468546F, 0x456E7472, 0x796E756D, 0x00445644, 0x456E7472, 0x796E756D, 0x49734469, 0x72004456,
    0x44466173, 0x744F7065, 0x6E004456, 0x44436C6F, 0x73650044, 0x56445265, 0x61645072, 0x696F0044,
    0x56444F70, 0x656E4469, 0x72004456, 0x44526561, 0x64446972, 0x00445644, 0x436C6F73, 0x65446972,
    0x004F5352, 0x65676973, 0x74657256, 0x65727369, 0x6F6E004F, 0x53526570, 0x6F727400, 0x4F535061,
    0x6E696300, 0x4F535969, 0x656C6454, 0x68726561, 0x64004F53, 0x47657454, 0x696D6500, 0x5F5F4F53,
    0x47657453, 0x79737465, 0x6D54696D, 0x65004F53, 0x43616C65, 0x6E646172, 0x54696D65, 0x546F5469,
    0x636B7300, 0x65786974, 0x00636F6E, 0x74656E74, 0x496E6974, 0x48616E64, 0x6C654E41, 0x4E440063,
    0x6F6E7465, 0x6E744661, 0x73744F70, 0x656E4E41, 0x4E440063, 0x6F6E7465, 0x6E74436F, 0x6E766572,
    0x74506174, 0x68546F45, 0x6E747279, 0x6E756D4E, 0x414E4400, 0x636F6E74, 0x656E7447, 0x65744C65,
    0x6E677468, 0x4E414E44, 0x00636F6E, 0x74656E74, 0x5365656B, 0x4E414E44, 0x00636F6E, 0x74656E74,
    0x52656164, 0x4E414E44, 0x00636F6E, 0x74656E74, 0x436C6F73, 0x654E414E, 0x4400636F, 0x6E74656E,
    0x7452656C, 0x65617365, 0x48616E64, 0x6C654E41, 0x4E440063, 0x6F6E7465, 0x6E744F70, 0x656E4469,
    0x724E414E, 0x44004E41, 0x4E444372, 0x65617465, 0x004E414E, 0x4444656C, 0x65746500, 0x4E414E44,
    0x52656164, 0x004E414E, 0x44577269, 0x7465004E, 0x414E4453, 0x65656B00, 0x4E414E44, 0x52656164,
    0x44697200, 0x4E414E44, 0x43726561, 0x74654469, 0x72004E41, 0x4E444765, 0x744C656E, 0x67746800,
    0x4E414E44, 0x47657453, 0x74617475, 0x73004E41, 0x4E444F70, 0x656E004E, 0x414E4443, 0x6C6F7365,
    0x004E414E, 0x44536166, 0x654F7065, 0x6E004E41, 0x4E445361, 0x6665436C, 0x6F736500, 0x4E414E44,
    0x47657443, 0x75727265, 0x6E744469, 0x72004E41, 0x4E444765, 0x74547970, 0x65005F53, 0x44415F42,
    0x4153455F, 0x005F5344, 0x41325F42, 0x4153455F, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
};

WWWImport wwwImportTable[112] = {
    { sSym_WWWSurfaceInit, (void**)&pWWWSurfaceInit },
    { sSym_WWWSurfaceNewScreen, (void**)&pWWWSurfaceNewScreen },
    { sSym_WWWSurfaceDeleteScreen, (void**)&pWWWSurfaceDeleteScreen },
    { sSym_WWWSurfaceResize, (void**)&pWWWSurfaceResize },
    { sSym_WWWSurfaceShutdown, (void**)&pWWWSurfaceShutdown },
    { sSym_WWWSurfaceSetFlushCallback, (void**)&pWWWSurfaceSetFlushCallback },
    { sSym_WWWSurfaceInvalidate, (void**)&pWWWSurfaceInvalidate },
    { sSym_WWWSurfaceUpdateScreen, (void**)&pWWWSurfaceUpdateScreen },
    { sSym_WWWSurfaceLockArea, (void**)&pWWWSurfaceLockArea },
    { sSym_WWWSurfaceUnlockArea, (void**)&pWWWSurfaceUnlockArea },
    { sSym_WWWSurfaceMouseEvt, (void**)&pWWWSurfaceMouseEvt },
    { sSym_WWWSurfaceWheelEvt, (void**)&pWWWSurfaceWheelEvt },
    { sSym_WWWSurfaceKeyboardEvt, (void**)&pWWWSurfaceKeyboardEvt },
    { sSym_WWWSurfaceAddFont, (void**)&pWWWSurfaceAddFont },
    { sSym_WWWCreateBrowser, (void**)&pWWWCreateBrowser },
    { sSym_WWWTerminateBrowser, (void**)&pWWWTerminateBrowser },
    { sSym_WWWRunSlice, (void**)&pWWWRunSlice },
    { sSym_WWWCreateBrowserWindow, (void**)&pWWWCreateBrowserWindow },
    { sSym_WWWCloseBrowserWindow, (void**)&pWWWCloseBrowserWindow },
    { sSym_WWWSetBrowserWindowTransparent, (void**)&pWWWSetBrowserWindowTransparent },
    { sSym_WWWGetBrowserWindowRect, (void**)&pWWWGetBrowserWindowRect },
    { sSym_WWWSetBrowserWindowRect, (void**)&pWWWSetBrowserWindowRect },
    { sSym_WWWRaiseBrowserWindow, (void**)&pWWWRaiseBrowserWindow },
    { sSym_WWWLowerBrowserWindow, (void**)&pWWWLowerBrowserWindow },
    { sSym_WWWShowBrowserWindow, (void**)&pWWWShowBrowserWindow },
    { sSym_WWWHideBrowserWindow, (void**)&pWWWHideBrowserWindow },
    { sSym_WWWCommitIme, (void**)&pWWWCommitIme },
    { sSym_WWWUpdateIme, (void**)&pWWWUpdateIme },
    { sSym_WWWPostUrl, (void**)&pWWWPostUrl },
    { sSym_WWWOpenUrl, (void**)&pWWWOpenUrl },
    { sSym_WWWGetHistoryCount, (void**)&pWWWGetHistoryCount },
    { sSym_WWWNextPage, (void**)&pWWWNextPage },
    { sSym_WWWPrevPage, (void**)&pWWWPrevPage },
    { sSym_WWWMoveInHistory, (void**)&pWWWMoveInHistory },
    { sSym_WWWStop, (void**)&pWWWStop },
    { sSym_WWWReload, (void**)&pWWWReload },
    { sSym_WWWReflow, (void**)&pWWWReflow },
    { sSym_WWWSearch, (void**)&pWWWSearch },
    { sSym_WWWResetSearch, (void**)&pWWWResetSearch },
    { sSym_WWWSetFocus, (void**)&pWWWSetFocus },
    { sSym_WWWLoseFocus, (void**)&pWWWLoseFocus },
    { sSym_WWWHistory, (void**)&pWWWHistory },
    { sSym_WWWClearHistory, (void**)&pWWWClearHistory },
    { sSym_WWWGetTrueZoom, (void**)&pWWWGetTrueZoom },
    { sSym_WWWSetTrueZoom, (void**)&pWWWSetTrueZoom },
    { sSym_WWWGetZoom, (void**)&pWWWGetZoom },
    { sSym_WWWSetZoom, (void**)&pWWWSetZoom },
    { sSym_WWWGetSecurityMode, (void**)&pWWWGetSecurityMode },
    { sSym_WWWGetScroll, (void**)&pWWWGetScroll },
    { sSym_WWWSetScroll, (void**)&pWWWSetScroll },
    { sSym_WWWSetRenderingMode, (void**)&pWWWSetRenderingMode },
    { sSym_WWWGetRenderingMode, (void**)&pWWWGetRenderingMode },
    { sSym_WWWSetImageMode, (void**)&pWWWSetImageMode },
    { sSym_WWWCreateCertificateManager, (void**)&pWWWCreateCertificateManager },
    { sSym_WWWCloseCertificateManager, (void**)&pWWWCloseCertificateManager },
    { sSym_WWWGetNumberOfCertificates, (void**)&pWWWGetNumberOfCertificates },
    { sSym_WWWSetLanguageEncoding, (void**)&pWWWSetLanguageEncoding },
    { sSym_WWWGetDocumentIcon, (void**)&pWWWGetDocumentIcon },
    { sSym_WWWGetDocumentIconUrl, (void**)&pWWWGetDocumentIconUrl },
    { sSym_WWWClearCookies, (void**)&pWWWClearCookies },
    { sSym_WWWGetDocumentSize, (void**)&pWWWGetDocumentSize },
    { sSym_WWWSetIntPref, (void**)&pWWWSetIntPref },
    { sSym_WWWGetIntPref, (void**)&pWWWGetIntPref },
    { sSym_WWWSetStringPref, (void**)&pWWWSetStringPref },
    { sSym_WWWGetStringPref, (void**)&pWWWGetStringPref },
    { sSym_WWWCommitPrefs, (void**)&pWWWCommitPrefs },
    { sSym_WWWSetFocusColors, (void**)&pWWWSetFocusColors },
    { sSym_WWWSetScrollbarColors, (void**)&pWWWSetScrollbarColors },
    { sSym_WWWSetScrollbarSize, (void**)&pWWWSetScrollbarSize },
    { sSym_WWWSetWidgetColors, (void**)&pWWWSetWidgetColors },
    { sSym_WWWSetDisabledWidgetColors, (void**)&pWWWSetDisabledWidgetColors },
    { sSym_WWWSetButtonWidgetColors, (void**)&pWWWSetButtonWidgetColors },
    { sSym_WWWSetUastringExtension, (void**)&pWWWSetUastringExtension },
    { sSym_WWWMarkNextItemInDirection, (void**)&pWWWMarkNextItemInDirection },
    { sSym_WWWResetNavigation, (void**)&pWWWResetNavigation },
    { sSym_WWWClearHighlight, (void**)&pWWWClearHighlight },
    { sSym_WWWSetHighlight, (void**)&pWWWSetHighlight },
    { sSym_WWWGetActiveLinkType, (void**)&pWWWGetActiveLinkType },
    { sSym_WWWGetBrowserAllocationFunctions, (void**)&pWWWGetBrowserAllocationFunctions },
    { sSym_WWWShutdownBrowserAllocationFunctions, (void**)&pWWWShutdownBrowserAllocationFunctions },
    { sSym_WWWSetAllocationFunctions, (void**)&pWWWSetAllocationFunctions },
    { sSym_WWWHTTPCreateHttpLib, (void**)&pWWWHTTPCreateHttpLib },
    { sSym_WWWHTTPTerminateHttpLib, (void**)&pWWWHTTPTerminateHttpLib },
    { sSym_WWWHTTPSessionRunSlice, (void**)&pWWWHTTPSessionRunSlice },
    { sSym_WWWHTTPInitSession, (void**)&pWWWHTTPInitSession },
    { sSym_WWWHTTPDeleteSession, (void**)&pWWWHTTPDeleteSession },
    { sSym_WWWHTTPSetSessionHeader, (void**)&pWWWHTTPSetSessionHeader },
    { sSym_WWWHTTPRemoveSessionHeader, (void**)&pWWWHTTPRemoveSessionHeader },
    { sSym_WWWHTTPRemoveAllSessionHeaders, (void**)&pWWWHTTPRemoveAllSessionHeaders },
    { sSym_WWWHTTPCreateRequest, (void**)&pWWWHTTPCreateRequest },
    { sSym_WWWHTTPDeleteRequest, (void**)&pWWWHTTPDeleteRequest },
    { sSym_WWWHTTPSetRequestHeader, (void**)&pWWWHTTPSetRequestHeader },
    { sSym_WWWHTTPGetRequestHeader, (void**)&pWWWHTTPGetRequestHeader },
    { sSym_WWWHTTPRemoveRequestHeader, (void**)&pWWWHTTPRemoveRequestHeader },
    { sSym_WWWHTTPRemoveAllRequestHeaders, (void**)&pWWWHTTPRemoveAllRequestHeaders },
    { sSym_WWWHTTPGetResponseHeader, (void**)&pWWWHTTPGetResponseHeader },
    { sSym_WWWHTTPGetResponseHeaders, (void**)&pWWWHTTPGetResponseHeaders },
    { sSym_WWWHTTPIssue, (void**)&pWWWHTTPIssue },
    { sSym_WWWHTTPNbActiveRequests, (void**)&pWWWHTTPNbActiveRequests },
    { sSym_WWWHTTPPostBodyData, (void**)&pWWWHTTPPostBodyData },
    { sSym_WWWHTTPSetAuthCredentials, (void**)&pWWWHTTPSetAuthCredentials },
    { sSym_WWWHTTPRemoveAuthCredentials, (void**)&pWWWHTTPRemoveAuthCredentials },
    { sSym_WWWHTTPSetProxy, (void**)&pWWWHTTPSetProxy },
    { sSym_WWWHTTPRemoveProxy, (void**)&pWWWHTTPRemoveProxy },
    { sSym_WWWHTTPEndLoading, (void**)&pWWWHTTPEndLoading },
    { sSym_WWWAddJSPlugin, (void**)&pWWWAddJSPlugin },
    { sSym_WWWAddNSPlugin, (void**)&pWWWAddNSPlugin },
    { sSym_WWWProtocolWrite, (void**)&pWWWProtocolWrite },
    { sSym_WWWProtocolSetMimeType, (void**)&pWWWProtocolSetMimeType },
    { sSym_WWWProtocolFinished, (void**)&pWWWProtocolFinished },
    { sSym_WWWProtocolFailed, (void**)&pWWWProtocolFailed },
    { sSym_WWWAddProtocol, (void**)&pWWWAddProtocol },
};

WWWNandFile wwwSettingsFile = {
    (char*)sSettingsPath,
    (void*)&wwwSettingsBuf,
    0x20
};
#pragma push
#pragma section all_types ".data" ".data"
u32 sWwwDataPad = 0;
#pragma pop


void wwwAddFonts(void) {
    WWWFont* font;

    for (font = wwwFontList; font->name != NULL; font++) {
        pWWWSurfaceAddFont(font->name);
    }
}

#pragma opt_common_subs off
#pragma inline_depth(1)
// Creates the missing parent directories of file->path, deepest first
#pragma always_inline on
s32 wwwCreateParentDirs(WWWNandFile* file) {
    s32 i;
    s32 ret;

    i = strlen(file->path);
    while (i-- != 0) {
        if (file->path[i] == '/') {
            file->path[i] = '\0';
            ret = NANDPrivateCreateDir(file->path, 0x3F, 0);
            if (ret == -12) {
                ret = wwwCreateParentDirs(file);
                if (ret == 0 || ret == -6) {
                    ret = NANDPrivateCreateDir(file->path, 0x3F, 0);
                }
            }
            file->path[i] = '/';
            return ret;
        }
    }
    return -64;
}


s32 wwwWriteNandFile(WWWNandFile* file) {
    s32 i;
    s32 ret;

    ret = NANDPrivateCreate(file->path, NAND_PERM_RWALL, 0);
    if (ret == NAND_RESULT_NOEXISTS) {
        i = strlen(file->path);
        while (i-- != 0) {
            if (file->path[i] == '/') {
                file->path[i] = '\0';
                ret = NANDPrivateCreateDir(file->path, NAND_PERM_RWALL, 0);
                if (ret == NAND_RESULT_NOEXISTS) {
                    ret = wwwCreateParentDirs(file);
                    if (ret == NAND_RESULT_OK || ret == NAND_RESULT_EXISTS) {
                        ret = NANDPrivateCreateDir(file->path, NAND_PERM_RWALL, 0);
                    }
                }
                file->path[i] = '/';
                goto done;
            }
        }
        ret = -64;
    done:
        if (ret == NAND_RESULT_OK || ret == NAND_RESULT_EXISTS) {
            ret = NANDPrivateCreate(file->path, NAND_PERM_RWALL, 0);
        }
    }
    if (ret != NAND_RESULT_OK && ret != NAND_RESULT_EXISTS) {
        return ret;
    }
    ret = NANDPrivateOpen(file->path, &wwwNandFileInfo, NAND_ACCESS_WRITE);
    if (ret != NAND_RESULT_OK) {
        return ret;
    }
    ret = NANDWrite(&wwwNandFileInfo, file->buf, file->size);
    if (ret < 0) {
        return ret;
    }
    NANDClose(&wwwNandFileInfo);
    return ret;
}

#pragma inline_depth(2)

#pragma opt_common_subs on
void wwwSaveSettings(void) {
    wwwSettingsBuf.magic = 0x35465768;
    wwwSettingsBuf.magic2 = 0x35465768;
    DCFlushRange(&wwwSettingsBuf, sizeof(wwwSettingsBuf));
    wwwWriteNandFile(&wwwSettingsFile);
}

// Reads file->path into file->buf, allocating the buffer if needed
static inline s32 wwwReadNandFile(WWWNandFile* file) {
    s32 ret;

    ret = NANDPrivateOpen(file->path, &wwwNandFileInfo, 1);
    if (ret != 0) {
        return ret;
    }
    file->size = (file->size + 31) & ~31;
    if (file->buf == NULL) {
        OSReport("AllocIfNecessary(%d)\n", file->size);
        if (!vcmvAlloc(&file->buf, file->size, wwwMem2Allocator, wwwMem1Allocator)) {
            return -0xED;
        }
    }
    ret = NANDRead(&wwwNandFileInfo, file->buf, file->size);
    if (ret < 0) {
        return ret;
    }
    return NANDClose(&wwwNandFileInfo);
}

#pragma global_optimizer off
void wwwLoadSettings(void) {
    s32 ret;

    if (wwwSettingsBuf.magic == 0x35465768 && wwwSettingsBuf.version >= 1) {
        return;
    }
    wwwAspectRatio = SCGetAspectRatio();
    if (VIGetDTVStatus()) {
        wwwAspectPtr = &wwwSettingsBuf.dtvAspect;
    } else {
        wwwAspectPtr = &wwwSettingsBuf.stdAspect;
    }
    ret = wwwReadNandFile(&wwwSettingsFile);
    DCInvalidateRange(&wwwSettingsBuf, sizeof(wwwSettingsBuf));
    if (ret != -12) {
        if (ret < 0) {
            return;
        }
        if (wwwSettingsBuf.magic == 0x35465768 && wwwSettingsBuf.magic2 == 0x35465768 &&
            wwwSettingsBuf.version >= 1) {
            return;
        }
    }
    wwwSettingsBuf.stdAspect = 1;
    wwwSettingsBuf.dtvAspect = 1;
    wwwSettingsBuf.flag5 = 0;
    wwwSettingsBuf.version = 1;
    wwwSaveSettings();
}

#pragma global_optimizer reset
static inline u32 wwwSwap32(u32 x) {
    return ((x << 24) | ((x << 8) & 0xFF0000)) | ((x >> 24) | ((x >> 8) & 0xFF00));
}

// Decodes n more bytes of LZ77 data into dst
void wwwLZRead(u8* dst, WWWLZState* s, u32 n) {
    u32 end;

    if (s->outPos == 0) {
        s->bit = 0;
        s->copyLen = 0;
        s->remaining = wwwSwap32(*(const u32*)s->src) >> 8;
        s->src += 4;
        if (s->remaining == 0) {
            s->remaining = wwwSwap32(*(const u32*)s->src);
            s->src += 4;
        }
    }
    if (s->remaining <= 0) {
        return;
    }
    end = s->outPos + n;
    if (s->copyLen == 0) {
        goto top;
    }
    goto copy;
top:
    while (s->outPos < end) {
        s->bit &= 7;
        if (s->bit == 0) {
            s->flags = *s->src++;
        }
        while (s->bit < 8) {
            if (!(s->flags & 0x80)) {
                s->window[s->outPos & 0xFFF] = *s->src++;
                *dst++ = s->window[s->outPos & 0xFFF];
                s->outPos++;
                s->remaining--;
            } else {
                s->copyLen = (*s->src >> 4) + 3;
                s->offset = (*s->src++ & 0xF) << 8;
                s->offset = (s->offset | *s->src++) + 1;
                s->remaining -= s->copyLen;
                for (;;) {
                    if (s->outPos >= end) {
                        return;
                    }
                copy:
                    s->window[s->outPos & 0xFFF] = s->window[(s->outPos - s->offset) & 0xFFF];
                    *dst++ = s->window[s->outPos & 0xFFF];
                    s->outPos++;
                    if (--s->copyLen == 0) {
                        break;
                    }
                }
            }
            if (s->remaining <= 0) {
                return;
            }
            s->flags = (s->flags & 0x7F) << 1;
            s->bit++;
        }
    }
}

// Decompresses wwwlib-rvl.lz7 from the channel's content and links it
s32 wwwLoadLibrary(void) {
    CNTHandleNAND handle;
    CNTFileInfoNAND file;
    WWWLZState lz;
    u8 header[0x58];
    void* packed = NULL;
    void* rest = NULL;
    s32 ret;
    u32 restSize;
    u32 size;
    WWWModule* module;
    s32 delta;
    WWWImport* imp;
    u32 i;

    ret = contentInitHandleNAND(2, &handle, wwwMem2Allocator);
    if (ret != 0) {
        goto fail;
    }
    ret = contentOpenNAND(&handle, "wwwlib-rvl.lz7", &file);
    if (ret != 0) {
        goto fail;
    }
    if (!vcmvAlloc(&packed, (contentGetLengthNAND(&file) + 31) & ~31, wwwMem2Allocator, wwwMem1Allocator)) {
        ret = -0xE12;
        goto fail;
    }
    ret = contentReadNAND(&file, packed, (contentGetLengthNAND(&file) + 31) & ~31, 0);
    if (ret <= 0) {
        goto fail;
    }
    ret = contentCloseNAND(&file);
    if (ret != 0) {
        goto fail;
    }
    memset(&lz, 0, sizeof(lz));
    lz.src = (u8*)packed;
    lz.outPos = 0;
    wwwLZRead(header, &lz, sizeof(header));
    size = RSOGetFixedSize(header, 2);
    if (!vcmvAlloc((void**)&wwwModule, (size + 31) & ~31, wwwMem1Allocator, wwwMem1Allocator)) {
        ret = -0xE11;
        goto fail;
    }
    lz.src = (u8*)packed;
    lz.outPos = 0;
    wwwLZRead((u8*)wwwModule, &lz, size);
    module = wwwModule;
    if (module->bssSize != 0) {
        if (!vcmvAlloc(&wwwModuleBss, module->bssSize, wwwMem1Allocator, wwwMem2Allocator)) {
            ret = -0xE13;
            goto fail;
        }
        memset(wwwModuleBss, 0, module->bssSize);
    }
    restSize = 0x902420 - size;
    if (!vcmvAlloc(&rest, (restSize + 31) & ~31, wwwMem2Allocator, wwwMem1Allocator)) {
        ret = -0xE14;
        goto fail;
    }
    wwwLZRead((u8*)rest, &lz, restSize);
    vcmvFree(&packed);
    delta = (u8*)rest - size - (u8*)module;
    module->exportTable += delta;
    module->exportStrings += delta;
    module->importTable += delta;
    module->importStrings += delta;
    RSOLinkList(module, wwwModuleBss);
    if (module->prolog != NULL) {
        module->prolog();
    }
    imp = wwwImportTable;
    for (i = 0; i < 0x70; i++, imp++) {
        *imp->slot = RSOFindExportSymbolAddr(module, imp->name);
    }
    vcmvFree(&rest);
    contentReleaseHandleNAND(&handle);
    if (ret == 0) {
        wwwLibLoaded = 1;
        return 0;
    }
fail:
    vcmvFree(&rest);
    vcmvFree(&wwwModuleBss);
    vcmvFree((void**)&wwwModule);
    vcmvFree(&packed);
    return ret;
}

// Loads the fonts from the channel's content
s32 wwwLoadFonts(void) {
    CNTHandleNAND handle;
    CNTFileInfoNAND file;
    const char** path;
    WWWFont* font;
    const char** name;
    s32 count;
    s32 ret;
    s32 entry;
    s32 len;

    ret = contentInitHandleNAND(3, &handle, wwwMem2Allocator);
    if (ret != 0) {
        return ret;
    }
    count = 0;
    font = wwwFontList;
    path = wwwFontPaths;
    name = wwwFontNames;
    for (; **path != '\0'; path++, name++) {
        entry = contentConvertPathToEntrynumNAND(&handle, *path);
        if (entry < 0) {
            continue;
        }
        if (contentFastOpenNAND(&handle, entry, &file) != 0) {
            continue;
        }
        vcmvAlloc(&font->buf, (contentGetLengthNAND(&file) + 31) & ~31, wwwMem2Allocator, wwwMem1Allocator);
        len = contentGetLengthNAND(&file);
        font->end = (u8*)font->buf + len;
        if (contentReadNAND(&file, font->buf, (contentGetLengthNAND(&file) + 31) & ~31, 0) <= 0) {
            continue;
        }
        font->name = *name;
        if (contentCloseNAND(&file) != 0) {
            continue;
        }
        font++;
        count++;
    }
    ret = contentReleaseHandleNAND(&handle);
    if (ret != 0) {
        return ret;
    }
    return count == 0 ? -0xE15 : 0;
}

static inline BOOL wwwSyncFiles(void) {
    WWWNandFile* file;

    for (file = wwwFileList; file->path != NULL; file++) {
        if (file->size != 0) {
            if (wwwWriteNandFile(file) != 0) {
                return FALSE;
            }
        } else {
            if (wwwReadNandFile(file) != 0) {
                return FALSE;
            }
        }
    }
    return TRUE;
}

s32 wwwInitialize(void) {
    static bool wwwFirstInit = true;
    s32 fonts;

    wwwStarted = 0;
    if (wwwFirstInit) {
        wwwFirstInit = 0;
        vcmvLinkRSO(sEmbeddedWwwRso);
        if (!wwwSyncFiles()) {
            return 0;
        }
    }
    fonts = wwwLoadFonts();
    if (wwwLoadFonts() == 0) {
        return wwwLoadLibrary();
    }
    return fonts;
}

#pragma opt_common_subs on
#pragma opt_unroll_loops off
void wwwShutdown(void) {
    WWWFont* font;
    int i;

    wwwLibLoaded = 0;
    if (wwwModule != NULL) {
        if (wwwModule->epilog != NULL) {
            wwwModule->epilog();
        }
        for (i = 0; i < 0x70; i++) {
            *wwwImportTable[i].slot = (void*)wwwUnlinkedFunction;
        }
        vcmvFree(&wwwModuleBss);
    }
    vcmvFree((void**)&wwwModule);
    for (font = wwwFontList; font->name != NULL; font++) {
        vcmvFree(&font->buf);
    }
}
