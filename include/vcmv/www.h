#ifndef WWW_H
#define WWW_H

#include <types.h>

#ifdef __cplusplus
extern "C" {
#endif

extern UNKWORD (*pWWWSurfaceInit)(u16, u16, u32, UNKWORD, void*);
#define WWWSurfaceInit pWWWSurfaceInit
extern void (*pWWWSurfaceNewScreen)();
#define WWWSurfaceNewScreen pWWWSurfaceNewScreen
extern void (*pWWWSurfaceDeleteScreen)();
#define WWWSurfaceDeleteScreen pWWWSurfaceDeleteScreen
extern void (*pWWWSurfaceResize)();
#define WWWSurfaceResize pWWWSurfaceResize
extern void (*pWWWSurfaceShutdown)();
#define WWWSurfaceShutdown pWWWSurfaceShutdown
extern UNKWORD (*pWWWSurfaceSetFlushCallback)(void*, UNKWORD);
#define WWWSurfaceSetFlushCallback pWWWSurfaceSetFlushCallback
extern void (*pWWWSurfaceInvalidate)();
#define WWWSurfaceInvalidate pWWWSurfaceInvalidate
extern void (*pWWWSurfaceUpdateScreen)(UNKWORD);
#define WWWSurfaceUpdateScreen pWWWSurfaceUpdateScreen
extern void (*pWWWSurfaceLockArea)();
#define WWWSurfaceLockArea pWWWSurfaceLockArea
extern void (*pWWWSurfaceUnlockArea)();
#define WWWSurfaceUnlockArea pWWWSurfaceUnlockArea
extern void (*pWWWSurfaceMouseEvt)(UNKWORD, UNKWORD, UNKWORD, UNKWORD, UNKWORD, UNKWORD);
#define WWWSurfaceMouseEvt pWWWSurfaceMouseEvt
extern void (*pWWWSurfaceWheelEvt)(UNKWORD, UNKWORD, UNKWORD, UNKWORD, UNKWORD);
#define WWWSurfaceWheelEvt pWWWSurfaceWheelEvt
extern void (*pWWWSurfaceKeyboardEvt)();
#define WWWSurfaceKeyboardEvt pWWWSurfaceKeyboardEvt
extern void (*pWWWSurfaceAddFont)(const char*);
#define WWWSurfaceAddFont pWWWSurfaceAddFont
extern UNKWORD (*pWWWCreateBrowser)(void*, void*, const char**, const char*);
#define WWWCreateBrowser pWWWCreateBrowser
extern void (*pWWWTerminateBrowser)();
#define WWWTerminateBrowser pWWWTerminateBrowser
extern UNKWORD (*pWWWRunSlice)(UNKWORD);
#define WWWRunSlice pWWWRunSlice
extern UNKWORD (*pWWWCreateBrowserWindow)(UNKWORD, void*, UNKWORD);
#define WWWCreateBrowserWindow pWWWCreateBrowserWindow
extern void (*pWWWCloseBrowserWindow)(UNKWORD, UNKWORD);
#define WWWCloseBrowserWindow pWWWCloseBrowserWindow
extern void (*pWWWSetBrowserWindowTransparent)();
#define WWWSetBrowserWindowTransparent pWWWSetBrowserWindowTransparent
extern void (*pWWWGetBrowserWindowRect)();
#define WWWGetBrowserWindowRect pWWWGetBrowserWindowRect
extern void (*pWWWSetBrowserWindowRect)(UNKWORD, void*);
#define WWWSetBrowserWindowRect pWWWSetBrowserWindowRect
extern void (*pWWWRaiseBrowserWindow)(UNKWORD);
#define WWWRaiseBrowserWindow pWWWRaiseBrowserWindow
extern void (*pWWWLowerBrowserWindow)();
#define WWWLowerBrowserWindow pWWWLowerBrowserWindow
extern void (*pWWWShowBrowserWindow)(UNKWORD);
#define WWWShowBrowserWindow pWWWShowBrowserWindow
extern void (*pWWWHideBrowserWindow)();
#define WWWHideBrowserWindow pWWWHideBrowserWindow
extern void (*pWWWCommitIme)();
#define WWWCommitIme pWWWCommitIme
extern void (*pWWWUpdateIme)();
#define WWWUpdateIme pWWWUpdateIme
extern void (*pWWWPostUrl)();
#define WWWPostUrl pWWWPostUrl
extern void (*pWWWOpenUrl)(UNKWORD, const char*);
#define WWWOpenUrl pWWWOpenUrl
extern void (*pWWWGetHistoryCount)();
#define WWWGetHistoryCount pWWWGetHistoryCount
extern void (*pWWWNextPage)(UNKWORD);
#define WWWNextPage pWWWNextPage
extern void (*pWWWPrevPage)(UNKWORD);
#define WWWPrevPage pWWWPrevPage
extern void (*pWWWMoveInHistory)();
#define WWWMoveInHistory pWWWMoveInHistory
extern void (*pWWWStop)();
#define WWWStop pWWWStop
extern void (*pWWWReload)();
#define WWWReload pWWWReload
extern void (*pWWWReflow)();
#define WWWReflow pWWWReflow
extern void (*pWWWSearch)();
#define WWWSearch pWWWSearch
extern void (*pWWWResetSearch)();
#define WWWResetSearch pWWWResetSearch
extern void (*pWWWSetFocus)(UNKWORD);
#define WWWSetFocus pWWWSetFocus
extern void (*pWWWLoseFocus)();
#define WWWLoseFocus pWWWLoseFocus
extern void (*pWWWHistory)();
#define WWWHistory pWWWHistory
extern void (*pWWWClearHistory)();
#define WWWClearHistory pWWWClearHistory
extern void (*pWWWGetTrueZoom)();
#define WWWGetTrueZoom pWWWGetTrueZoom
extern void (*pWWWSetTrueZoom)();
#define WWWSetTrueZoom pWWWSetTrueZoom
extern void (*pWWWGetZoom)();
#define WWWGetZoom pWWWGetZoom
extern void (*pWWWSetZoom)();
#define WWWSetZoom pWWWSetZoom
extern void (*pWWWGetSecurityMode)();
#define WWWGetSecurityMode pWWWGetSecurityMode
extern void (*pWWWGetScroll)();
#define WWWGetScroll pWWWGetScroll
extern void (*pWWWSetScroll)();
#define WWWSetScroll pWWWSetScroll
extern void (*pWWWSetRenderingMode)(UNKWORD, UNKWORD);
#define WWWSetRenderingMode pWWWSetRenderingMode
extern void (*pWWWGetRenderingMode)();
#define WWWGetRenderingMode pWWWGetRenderingMode
extern void (*pWWWSetImageMode)(UNKWORD, UNKWORD);
#define WWWSetImageMode pWWWSetImageMode
extern void (*pWWWCreateCertificateManager)();
#define WWWCreateCertificateManager pWWWCreateCertificateManager
extern void (*pWWWCloseCertificateManager)();
#define WWWCloseCertificateManager pWWWCloseCertificateManager
extern void (*pWWWGetNumberOfCertificates)();
#define WWWGetNumberOfCertificates pWWWGetNumberOfCertificates
extern void (*pWWWSetLanguageEncoding)();
#define WWWSetLanguageEncoding pWWWSetLanguageEncoding
extern void (*pWWWGetDocumentIcon)();
#define WWWGetDocumentIcon pWWWGetDocumentIcon
extern void (*pWWWGetDocumentIconUrl)();
#define WWWGetDocumentIconUrl pWWWGetDocumentIconUrl
extern void (*pWWWClearCookies)();
#define WWWClearCookies pWWWClearCookies
extern void (*pWWWGetDocumentSize)();
#define WWWGetDocumentSize pWWWGetDocumentSize
extern void (*pWWWSetIntPref)();
#define WWWSetIntPref pWWWSetIntPref
extern void (*pWWWGetIntPref)();
#define WWWGetIntPref pWWWGetIntPref
extern void (*pWWWSetStringPref)();
#define WWWSetStringPref pWWWSetStringPref
extern void (*pWWWGetStringPref)();
#define WWWGetStringPref pWWWGetStringPref
extern void (*pWWWCommitPrefs)();
#define WWWCommitPrefs pWWWCommitPrefs
extern void (*pWWWSetFocusColors)();
#define WWWSetFocusColors pWWWSetFocusColors
extern void (*pWWWSetScrollbarColors)();
#define WWWSetScrollbarColors pWWWSetScrollbarColors
extern void (*pWWWSetScrollbarSize)();
#define WWWSetScrollbarSize pWWWSetScrollbarSize
extern void (*pWWWSetWidgetColors)();
#define WWWSetWidgetColors pWWWSetWidgetColors
extern void (*pWWWSetDisabledWidgetColors)();
#define WWWSetDisabledWidgetColors pWWWSetDisabledWidgetColors
extern void (*pWWWSetButtonWidgetColors)();
#define WWWSetButtonWidgetColors pWWWSetButtonWidgetColors
extern void (*pWWWSetUastringExtension)();
#define WWWSetUastringExtension pWWWSetUastringExtension
extern void (*pWWWMarkNextItemInDirection)();
#define WWWMarkNextItemInDirection pWWWMarkNextItemInDirection
extern void (*pWWWResetNavigation)();
#define WWWResetNavigation pWWWResetNavigation
extern void (*pWWWClearHighlight)();
#define WWWClearHighlight pWWWClearHighlight
extern void (*pWWWSetHighlight)();
#define WWWSetHighlight pWWWSetHighlight
extern void (*pWWWGetActiveLinkType)();
#define WWWGetActiveLinkType pWWWGetActiveLinkType
extern void (*pWWWGetBrowserAllocationFunctions)(void*, u32, void*, void*, void*);
#define WWWGetBrowserAllocationFunctions pWWWGetBrowserAllocationFunctions
extern void (*pWWWShutdownBrowserAllocationFunctions)();
#define WWWShutdownBrowserAllocationFunctions pWWWShutdownBrowserAllocationFunctions
extern void (*pWWWSetAllocationFunctions)(UNKWORD, UNKWORD, UNKWORD, UNKWORD, UNKWORD, UNKWORD, UNKWORD);
#define WWWSetAllocationFunctions pWWWSetAllocationFunctions
extern void (*pWWWHTTPCreateHttpLib)();
#define WWWHTTPCreateHttpLib pWWWHTTPCreateHttpLib
extern void (*pWWWHTTPTerminateHttpLib)();
#define WWWHTTPTerminateHttpLib pWWWHTTPTerminateHttpLib
extern void (*pWWWHTTPSessionRunSlice)();
#define WWWHTTPSessionRunSlice pWWWHTTPSessionRunSlice
extern void (*pWWWHTTPInitSession)();
#define WWWHTTPInitSession pWWWHTTPInitSession
extern void (*pWWWHTTPDeleteSession)();
#define WWWHTTPDeleteSession pWWWHTTPDeleteSession
extern void (*pWWWHTTPSetSessionHeader)();
#define WWWHTTPSetSessionHeader pWWWHTTPSetSessionHeader
extern void (*pWWWHTTPRemoveSessionHeader)();
#define WWWHTTPRemoveSessionHeader pWWWHTTPRemoveSessionHeader
extern void (*pWWWHTTPRemoveAllSessionHeaders)();
#define WWWHTTPRemoveAllSessionHeaders pWWWHTTPRemoveAllSessionHeaders
extern void (*pWWWHTTPCreateRequest)();
#define WWWHTTPCreateRequest pWWWHTTPCreateRequest
extern void (*pWWWHTTPDeleteRequest)();
#define WWWHTTPDeleteRequest pWWWHTTPDeleteRequest
extern void (*pWWWHTTPSetRequestHeader)();
#define WWWHTTPSetRequestHeader pWWWHTTPSetRequestHeader
extern void (*pWWWHTTPGetRequestHeader)();
#define WWWHTTPGetRequestHeader pWWWHTTPGetRequestHeader
extern void (*pWWWHTTPRemoveRequestHeader)();
#define WWWHTTPRemoveRequestHeader pWWWHTTPRemoveRequestHeader
extern void (*pWWWHTTPRemoveAllRequestHeaders)();
#define WWWHTTPRemoveAllRequestHeaders pWWWHTTPRemoveAllRequestHeaders
extern void (*pWWWHTTPGetResponseHeader)();
#define WWWHTTPGetResponseHeader pWWWHTTPGetResponseHeader
extern void (*pWWWHTTPGetResponseHeaders)();
#define WWWHTTPGetResponseHeaders pWWWHTTPGetResponseHeaders
extern void (*pWWWHTTPIssue)();
#define WWWHTTPIssue pWWWHTTPIssue
extern void (*pWWWHTTPNbActiveRequests)();
#define WWWHTTPNbActiveRequests pWWWHTTPNbActiveRequests
extern void (*pWWWHTTPPostBodyData)();
#define WWWHTTPPostBodyData pWWWHTTPPostBodyData
extern void (*pWWWHTTPSetAuthCredentials)();
#define WWWHTTPSetAuthCredentials pWWWHTTPSetAuthCredentials
extern void (*pWWWHTTPRemoveAuthCredentials)();
#define WWWHTTPRemoveAuthCredentials pWWWHTTPRemoveAuthCredentials
extern void (*pWWWHTTPSetProxy)();
#define WWWHTTPSetProxy pWWWHTTPSetProxy
extern void (*pWWWHTTPRemoveProxy)();
#define WWWHTTPRemoveProxy pWWWHTTPRemoveProxy
extern void (*pWWWHTTPEndLoading)();
#define WWWHTTPEndLoading pWWWHTTPEndLoading
extern void (*pWWWAddJSPlugin)(const char*, void*, void*, void*, void*);
#define WWWAddJSPlugin pWWWAddJSPlugin
extern void (*pWWWAddNSPlugin)();
#define WWWAddNSPlugin pWWWAddNSPlugin
extern UNKWORD (*pWWWProtocolWrite)(UNKWORD, void*, UNKWORD);
#define WWWProtocolWrite pWWWProtocolWrite
extern UNKWORD (*pWWWProtocolSetMimeType)(UNKWORD, const char*);
#define WWWProtocolSetMimeType pWWWProtocolSetMimeType
extern UNKWORD (*pWWWProtocolFinished)(UNKWORD);
#define WWWProtocolFinished pWWWProtocolFinished
extern UNKWORD (*pWWWProtocolFailed)(UNKWORD);
#define WWWProtocolFailed pWWWProtocolFailed
extern void (*pWWWAddProtocol)(const char*);
#define WWWAddProtocol pWWWAddProtocol

#ifdef __cplusplus
}
#endif

#endif
