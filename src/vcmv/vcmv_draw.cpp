#include <types.h>
#include <revolution/ARC.h>
#include <revolution/GX.h>
#include <revolution/OS.h>
#include "vcmv.h"
#include "www.h"
#include "vcmv_www.h"

#define ALIGN_NEXT(val, alignment) (((val) + ((alignment) - 1)) & ~((alignment) - 1))

#include <revolution/MTX.h>
#include <cmath>
#include <cstring>

static char sUrlBuffer[256];
static VCMVRect sWindowRect;
// Nothing references these two arrays, but the original link kept them.
#pragma push
#pragma force_active on
static u8 sUnusedBss1[0x30];
#pragma pop
static GXTexObj sPageTexObj[4];
static void* sPageTexBuffer[4];
#pragma push
#pragma force_active on
static u8 sUnusedBss2[0x240];
#pragma pop
static volatile VCMVTransitionState sTransitionState;
static ARCHandle sManualArcHandle;

f32 sHalfScreenWidth = 304.0f;
f32 sHalfScreenHeight = 228.0f;
f32 sWidescreenScale = 1.0f;
s32 vcmvFadeDuration = 0x0F; // frames

const char* sStartPageUrl;
char* sCurrentPageUrl;
u32* sSurfaceBuffer;
static u32* sDirtyTileBuffer;
static u32 sSurfaceBufferSize;
GXRenderModeObj* vcmvPageTargetWidth;
GXRenderModeObj* vcmvPageCurrentWidth;
GXRenderModeObj* sActiveRenderMode;
u16 sClampedScreenWidth;
u16 sFbWidth;
u16 sEfbHeight;
u8 vcmvNimsGuiFlag;
u16 sAlignedWidth;
u16 sAlignedHeight;
static u16 sMarginWidth;
u16 sRightMargin;
f32 sLeftMargin;
static f32 sInvHalfWidth;
static f32 sInvHalfHeight;
static s32 sBrowserRenderMode;
s32 sWindowId; // Opera browser window handle
static s32 sPageChangePending; // set while a prev/next page request is in flight
s32 vcmvFadeTimer = vcmvFadeDuration; // frames since fade start
static f32 sTransitionProgress;
static f32 sTransitionFactor;
static bool sBrowserNeedsUpdate;
static bool sTransitionDir;
bool vcmvIsTransitioning;
bool sIsPageLoading;
static bool sPageLoadedFlag;
static bool sNeedsRedraw;
static u32 sActiveScrollCount;
static bool sTransitionActive;
bool vcmvIsPageFading;
static bool sTextureInitialized;
static bool sPendingTransition;
static u8 sActiveTexIndex;
static u8 sPrevTexIndex;
static u8 sFadeTexIndex;
static u8 sDisplayTexIndex;
s8 sPageScrollDirection;
static int sScrollStep; // int (not s32): copy of sLastMouseY at last texture convert
volatile bool sIsTransitioning;
static volatile bool sScrollStarted;
static void* sManualArcBuffer;
static u8 sCachedFadeIndex;
static u8 sCachedPrevIndex;
static u8 sCachedActiveIndex;
static u8 sCachedDisplayIndex;
#pragma push
#pragma force_active on
static u32 sUnusedSbss; // nothing references it, but the original link kept it
#pragma pop

static u8 sDitherTable[] = {
    0x4F, 0x07, 0x24, 0x78, 0x43, 0x1B, 0x38, 0x11, 0x5D, 0x70, 0x2B, 0x7F, 0x66, 0x49, 0x21, 0x55, 0x09, 0x3B, 0x14,
    0x71, 0x5E, 0x2E, 0x68, 0x45, 0x50, 0x02, 0x1F, 0x77, 0x37, 0x59, 0x6D, 0x63, 0x0B, 0x15, 0x4A, 0x2D, 0x3F, 0x01,
    0x54, 0x69, 0x5F, 0x1C, 0x0D, 0x35, 0x26, 0x76, 0x4D, 0x42, 0x03, 0x5B, 0x3A, 0x13, 0x1E, 0x6B, 0x29, 0x53, 0x48,
    0x33, 0x0A, 0x79, 0x3E, 0x60, 0x22, 0x16, 0x2C, 0x6C, 0x51, 0x46, 0x36, 0x00, 0x0C, 0x74, 0x20, 0x64, 0x2A, 0x58,
    0x17, 0x40, 0x34, 0x4C, 0x7A, 0x6F, 0x05, 0x10, 0x61, 0x25, 0x56, 0x39, 0x44, 0x1A, 0x7C, 0x6A, 0x30, 0x08, 0x4E,
    0x5C, 0x73, 0x27, 0x12, 0x41, 0x65, 0x7B, 0x1D, 0x04, 0x32, 0x6E, 0x57, 0x4B, 0x0E, 0x28, 0x7E, 0x3D, 0x75, 0x19,
    0x62, 0x06, 0x31, 0x52, 0x47, 0x23, 0x0F, 0x7D, 0x72, 0x3C, 0x18, 0x5A, 0x67, 0x2F,
};

struct VCMVRgb565 {
    u16 r : 5;
    u16 g : 6;
    u16 b : 5;
};

union VCMVPixel565 {
    u16 raw;
    VCMVRgb565 field;
};

static inline u16 vcmvRgbToRgb565(u32 value) {
    VCMVPixel565 pixel;
    pixel.raw = (value >> 3) & 0x1F;
    pixel.field.g = value >> 10;
    pixel.field.r = value >> 19;
    return pixel.raw;
}

static inline void vcmvInitWindow(s32 windowId) {
    if (windowId == 0) {
        WWWCloseBrowserWindow(sBrowserHandle, windowId);
    } else {
        WWWSetBrowserWindowRect(windowId, &sWindowRect);
        WWWSetImageMode(windowId, 4);
        WWWSetRenderingMode(windowId, sBrowserRenderMode);
        WWWShowBrowserWindow(windowId);

        sWindowId = windowId;
        if (windowId != 0) {
            WWWRaiseBrowserWindow(windowId);
        }

        WWWSetFocus(windowId);
        WWWSurfaceUpdateScreen(0);
    }
}

static inline void vcmvCreateWindow(const char* url) {
    s32 windowId;

    if (WWWCreateBrowserWindow(sBrowserHandle, &windowId, 0) != 0) {
        OSReport("NO MEMORY\n");
    } else {
        if (url != NULL) {
            WWWOpenUrl(windowId, url);
        }

        vcmvInitWindow(windowId);
    }
}

static inline void vcmvAdvanceTextureIndex() {
    sNeedsRedraw = true;
    sPendingTransition = false;
    sTransitionDir = false;

    do {
        sActiveTexIndex = (sActiveTexIndex + 1) % 4;
    } while (sActiveTexIndex == sFadeTexIndex || sActiveTexIndex == sPrevTexIndex);
}

s32 vcmvCalcDiffRatio(VCMVRect* rect, f32* outRatio) {
    s32 bottom;
    s32 right;
    s32 tilesPerRow;
    s32 firstTileX;
    s32 leftOffset;
    s32 rightOffset;
    u32* dirtyRow;
    u32* rowStart;
    u32* rowEnd;
    s32 sampleCount;
    s32 changedCount;
    s32 y;
    u8 ditherOffset;
    u32* pixel;
    u32* dirty;

    right = rect->x + rect->width;
    bottom = rect->y + rect->height;
    tilesPerRow = (sAlignedWidth + 0x7F) / 128;
    firstTileX = rect->x / 128;
    leftOffset = rect->x & 0x7F;
    rightOffset = right & 0x7F;
    dirtyRow = sDirtyTileBuffer + rect->y * tilesPerRow + firstTileX;
    rowStart = &sSurfaceBuffer[rect->y * sAlignedWidth + firstTileX * 128];
    rowEnd = &sSurfaceBuffer[rect->y * sAlignedWidth + right];

    changedCount = 0;
    y = rect->y;
    sampleCount = rect->height * (right / 128 - firstTileX + 1);

    while (y < bottom) {
        ditherOffset = sDitherTable[y & 0x7F];
        pixel = rowStart + ditherOffset;
        dirty = dirtyRow;

        if (ditherOffset < leftOffset) {
            dirty++;
            pixel += 128;
            sampleCount--;
        }

        while (pixel < rowEnd) {
            if (*dirty != *pixel) {
                *dirty = *pixel;
                changedCount++;
            }

            pixel += 128;
            dirty++;
        }

        if (ditherOffset >= rightOffset) {
            sampleCount--;
        }

        rowStart += sAlignedWidth;
        rowEnd += sAlignedWidth;
        dirtyRow += tilesPerRow;
        y++;
    }

    if (sampleCount != 0) {
        *outRatio = (f32)changedCount / (f32)sampleCount;
    } else {
        *outRatio = 0.0f;
    }

    return sampleCount;
}

bool vcmvDetectScrollDown(VCMVRect* rect, s32* outScroll) {
    u8 offsets[0x4D];
    u8 counts[0x4D];

    s32 bottom;
    s32 candidateCount;
    s32 remainingRows;
    s32 startY;
    s32 i;
    s32 tilesPerRow;
    s32 firstTileX;
    s32 leftOffset;
    u32* dirtyRow;
    u32* rowStart;
    u32* rowEnd;
    s32 right;
    s32 rightOffset;
    s32 y;
    s32 srcIdx;
    s32 k;
    u8 ditherOffset;
    u32* pixel;
    u32* dirty;
    s32 dstIdx;
    u32 refPixel;

    candidateCount = 0x4D;
    bottom = rect->y + rect->height;
    remainingRows = sAlignedHeight - bottom;

    if (candidateCount > remainingRows) {
        candidateCount = remainingRows;
    }

    startY = rect->y + candidateCount;

    for (i = 0; i < candidateCount; i++) {
        offsets[i] = i + 4;
        counts[i] = 3;
    }

    right = rect->x + rect->width;
    tilesPerRow = (sAlignedWidth + 0x7F) / 128;
    firstTileX = rect->x / 128;
    leftOffset = rect->x & 0x7F;
    rightOffset = right & 0x7F;
    dirtyRow = sDirtyTileBuffer + startY * tilesPerRow + firstTileX;
    rowStart = &sSurfaceBuffer[startY * sAlignedWidth + firstTileX * 128];
    rowEnd = &sSurfaceBuffer[startY * sAlignedWidth + right];

    y = startY;

    while (y < bottom + offsets[candidateCount - 1]) {
        ditherOffset = sDitherTable[y & 0x7F];
        pixel = rowStart + ditherOffset;
        dirty = dirtyRow;

        if (ditherOffset < leftOffset) {
            dirty++;
            pixel += 128;
        }

        while (pixel < rowEnd) {
            dstIdx = 0;
            srcIdx = 0;

            for (k = 0; k < candidateCount; k++) {
                counts[dstIdx] = counts[srcIdx];
                offsets[dstIdx] = offsets[srcIdx];

                refPixel = pixel[-offsets[dstIdx] * sAlignedWidth];

                if (*dirty != refPixel) {
                    counts[k]--;

                    if (counts[k] == 0) {
                        dstIdx--;
                    }
                }

                dstIdx++;
                srcIdx++;
            }

            candidateCount = dstIdx;
            pixel += 128;
            dirty++;
        }

        rowStart += sAlignedWidth;
        rowEnd += sAlignedWidth;
        dirtyRow += tilesPerRow;
        y++;
    }

    if (candidateCount == 1) {
        *outScroll = offsets[0];
        return true;
    }

    return false;
}

bool vcmvDetectScrollUp(VCMVRect* rect, s32* outScroll) {
    u8 offsets[0x4D];
    u8 counts[0x4D];
    s32 bottom;
    s32 candidateCount;
    s32 remainingRows;
    s32 startY;
    s32 i;
    s32 tilesPerRow;
    s32 firstTileX;
    s32 leftOffset;
    u32* dirtyRow;
    u32* rowStart;
    u32* rowEnd;
    s32 right;
    s32 rightOffset;
    s32 y;
    s32 srcIdx;
    s32 k;
    u8 ditherOffset;
    u32* pixel;
    u32* dirty;
    s32 dstIdx;
    u32 refPixel;

    candidateCount = 0x4D;
    bottom = rect->y + rect->height;
    remainingRows = sAlignedHeight - bottom;

    if (candidateCount > rect->y) {
        candidateCount = rect->y;
    }

    startY = rect->y;

    for (i = 0; i < candidateCount; i++) {
        offsets[i] = i + 4;
        counts[i] = 3;
    }

    right = rect->x + rect->width;
    tilesPerRow = (sAlignedWidth + 0x7F) / 128;
    firstTileX = rect->x / 128;
    leftOffset = rect->x & 0x7F;
    rightOffset = right & 0x7F;
    dirtyRow = sDirtyTileBuffer + startY * tilesPerRow + firstTileX;
    rowStart = &sSurfaceBuffer[startY * sAlignedWidth + firstTileX * 128];
    rowEnd = &sSurfaceBuffer[startY * sAlignedWidth + right];

    y = startY;

    while (y < bottom - offsets[candidateCount - 1]) {
        ditherOffset = sDitherTable[y & 0x7F];
        pixel = rowStart + ditherOffset;
        dirty = dirtyRow;

        if (ditherOffset < leftOffset) {
            dirty++;
            pixel += 128;
        }

        while (pixel < rowEnd) {
            dstIdx = 0;
            srcIdx = 0;

            for (k = 0; k < candidateCount; k++) {
                counts[dstIdx] = counts[srcIdx];
                offsets[dstIdx] = offsets[srcIdx];

                refPixel = pixel[offsets[dstIdx] * sAlignedWidth];

                if (*dirty != refPixel) {
                    counts[k]--;

                    if (counts[k] == 0) {
                        dstIdx--;
                    }
                }

                dstIdx++;
                srcIdx++;
            }

            candidateCount = dstIdx;
            pixel += 128;
            dirty++;
        }

        rowStart += sAlignedWidth;
        rowEnd += sAlignedWidth;
        dirtyRow += tilesPerRow;
        y++;
    }

    if (candidateCount == 1) {
        *outScroll = -offsets[0];
        return true;
    }

    return false;
}

void vcmvApplyScroll(VCMVRect* rect, s32 scroll) {
    if (!sIsTransitioning) {
        sScrollStarted = true;

        sTransitionState.history[0].scrollPos = -100000;
        sTransitionState.history[1].scrollPos = -100000;
        sTransitionState.history[2].scrollPos = -100000;
        sTransitionState.history[3].scrollPos = -100000;

        sFadeTexIndex = sPrevTexIndex;
        sTransitionState.history[sPrevTexIndex].lastMouseY = (0, sScrollStep); //! TODO: fake match
        sTransitionState.history[sPrevTexIndex].scrollPos = 0;
        sTransitionState.fadeTexIndex = sPrevTexIndex;
        sTransitionState.prevTexIndex = sPrevTexIndex;
        sTransitionState.activeTexIndex = sPrevTexIndex;
        sTransitionState.accumScroll = 0;
        sTransitionState.scrollOffset = 0;
        sTransitionState.currentScroll = sTransitionState.scrollVelocity = 0.0f;
    }

    sTransitionState.accumScroll += scroll;

    if (!sTransitionState.hasRect) {
        sTransitionState.hasRect = true;
        sTransitionState.rect.x = rect->x;
        sTransitionState.rect.y = rect->y;
        sTransitionState.rect.width = rect->width;
        sTransitionState.rect.height = rect->height;

        if (scroll < 0) {
            sTransitionState.rect.height -= scroll;
            sTransitionState.rect.y += scroll;
        } else {
            sTransitionState.rect.height += scroll;
        }
    }
}

void vcmvFlushCallback(VCMVRect* rect, s32 mode) {
    s32 scroll;

    if (sActiveScrollCount < 1 && !sIsPageLoading) {
        if (sPageLoadedFlag) {
            if (sInitDelayCounter == 0 || vcmvFadeTimer >= 0x0C) {
                sPageLoadedFlag = false;

                if (!sTransitionActive) {
                    sTransitionActive = true;
                }
            } else {
                sPendingTransition = true;
                return;
            }
        } else if (sPageScrollDirection != 0) {
            if (mode == 1 && rect->height > 0x50) {
                sTransitionState.needsAdjustment = false;

                if (sIsTransitioning && sTransitionState.hasRect != 0) {
                    if ((rect->x != sTransitionState.rect.x ||
                         rect->width != sTransitionState.rect.width)) {
                        sTransitionState.needsAdjustment = true;
                        goto end; //! TODO: fake match?
                    }
                }

                if (sPageScrollDirection == 1) {
                    if (vcmvDetectScrollDown(rect, &scroll)) {
                        vcmvApplyScroll(rect, scroll);
                    }
                } else if (sPageScrollDirection == -1 && vcmvDetectScrollUp(rect, &scroll)) {
                    vcmvApplyScroll(rect, scroll);
                }
            }
        }

    end:
        vcmvAdvanceTextureIndex();
    }
}

// The pixel row below row (the surface is stride bytes wide)
static inline const u32* vcmvNextRow(const void* row, u32 stride) {
    return (const u32*)((const u8*)row + stride);
}

void vcmvConvertSurfaceToTexture(void) {
    void* tex;
    u32 srcStep;
    u8* src;
    u32 stride;
    u8* dst;
    int y;
    tex = sPageTexBuffer[sActiveTexIndex];
    stride = sAlignedWidth * 4;
    srcStep = sAlignedWidth * 16;
    src = (u8*)sSurfaceBuffer;
    dst = (u8*)tex;
    sNeedsRedraw = false;
    y = 0;
    sActiveScrollCount = 0;

    for (; y < sAlignedHeight; y += 4) {
        u16* tile_dst;
        const u32* row0;
        const u32* row1;
        const u32* row2;
        const u32* row3;
        int x;
        row1 = vcmvNextRow(src, stride);
        row2 = vcmvNextRow(row1, stride);
        row3 = vcmvNextRow(row2, stride);
        tile_dst = (u16*)dst;
        row0 = (const u32*)src;
        x = 0;

        for (; x < sAlignedWidth; x += 4) {
            tile_dst[0] = vcmvRgbToRgb565(row0[0]);
            tile_dst[1] = vcmvRgbToRgb565(row0[1]);
            tile_dst[2] = vcmvRgbToRgb565(row0[2]);
            tile_dst[3] = vcmvRgbToRgb565(row0[3]);
            row0 += 4;

            tile_dst[4] = vcmvRgbToRgb565(row1[0]);
            tile_dst[5] = vcmvRgbToRgb565(row1[1]);
            tile_dst[6] = vcmvRgbToRgb565(row1[2]);
            tile_dst[7] = vcmvRgbToRgb565(row1[3]);
            row1 += 4;

            tile_dst[8] = vcmvRgbToRgb565(row2[0]);
            tile_dst[9] = vcmvRgbToRgb565(row2[1]);
            tile_dst[10] = vcmvRgbToRgb565(row2[2]);
            tile_dst[11] = vcmvRgbToRgb565(row2[3]);
            row2 += 4;

            tile_dst[12] = vcmvRgbToRgb565(row3[0]);
            tile_dst[13] = vcmvRgbToRgb565(row3[1]);
            tile_dst[14] = vcmvRgbToRgb565(row3[2]);
            tile_dst[15] = vcmvRgbToRgb565(row3[3]);
            row3 += 4;

            tile_dst += 16;
        }

        src += srcStep;
        dst += sAlignedWidth * 8;
    }

    DCStoreRange(tex, sAlignedWidth * sAlignedHeight * 2);
    GXInitTexObj(&sPageTexObj[sActiveTexIndex], tex, sAlignedWidth, sAlignedHeight, GX_TF_RGB565, GX_CLAMP, GX_CLAMP, GX_FALSE);
    GXInitTexObjLOD(&sPageTexObj[sActiveTexIndex], GX_LINEAR, GX_LINEAR, 0.0f, 0.0f, 0.0f, GX_FALSE, GX_FALSE, GX_ANISO_1);
    sTextureInitialized = true;
}

s32 vcmvOperaCallback(void* userData, int windowId, int event, WWWProtocolRequest* request) {
    void* fileData;
    s32 chunkSize;
    char* path;
    u32 offset;
    s32 writeResult;
    s32 mimeResult;

    switch (event) {
        case 0x06:
            sScrollDragFrame = sManualFrameCount - 5;
            sTransitionDir = true;
            vcmvIsTransitioning = true;
            sTransitionFactor = 0.0f;
            break;
        case 0x05:
            strncpy(sUrlBuffer, request->url, sizeof(sUrlBuffer) - 1);
            sPageScrollDirection = 0;
            sCurrentPageUrl = sUrlBuffer;
            sTransitionState.hasRect = 0;

            if (sSelectSoundPlayed) {
                sSelectSoundPlayed = false;
                if (strcmp(sCurrentPageUrl, sStartPageUrl) == 0) {
                    VCMVPlaySound(0x05);
                } else {
                    VCMVPlaySound(0x01);
                }
            }

            vcmvIsTransitioning = true;
            sIsPageLoading = true;
            sPageLoadedFlag = false;
            break;
        case 0x08:
            sTransitionFactor = 1.0f / (2.0f - sTransitionFactor);
            break;
        case 0x07: {
            sScrollDragFrame = sManualFrameCount - 5;
            vcmvIsTransitioning = false;

            if (sIsPageLoading) {
                sIsPageLoading = false;
                sPageLoadedFlag = true;
            }

            if (sInitDelayCounter != 0) {
                sInitDelayCounter--;

                if (sInitDelayCounter == 1) {
                    VCMVPlaySound(2);
                }
            }

            VCMVRect fullRect = {0, 0, sClampedScreenWidth, 456};
            vcmvFlushCallback(&fullRect, 1);
            break;
        }
        case 0x01:
            return 0;
        case 0x02:
            vcmvInitWindow(windowId);
            return 0;
        case 0x23:
            OSReport("!!WWW OUT OF MEMORY!!\n");
            sExitRequested = true;
            VCMVPlaySound(3);
            return 0;
        case 0x2E:
            if (sPageChangePending == 0) {
                if (stricmp(request->url, "GOGI Previous Page") == 0) {
                    WWWPrevPage(windowId);
                } else if (stricmp(request->url, "GOGI Next Page") == 0) {
                    WWWNextPage(windowId);
                }

                return 0;
            }

            break;
        case 0x2C:
            path = (char*)request->url + 5;

            while (*path == 0x2F) {
                path++;
            }

            if (strcmp(request->method, "arc") == 0) {
                mimeResult = WWWProtocolSetMimeType((s32)request->handle, "text/html");
                if (mimeResult != 0) {
                    return mimeResult;
                }

                if (sManualArcBuffer != 0) {
                    ARCFileInfo fileInfo;

                    if (ARCOpen(&sManualArcHandle, path, &fileInfo) != 0) {
                        fileData = ARCGetStartAddrInMem(&fileInfo);

                        offset = 0U;
                        while (offset < fileInfo.length) {
                            chunkSize = fileInfo.length - offset;

                            if (chunkSize > 0x7FFF) {
                                chunkSize = 0x7FFF;
                            }

                            writeResult = WWWProtocolWrite((s32)request->handle, (u8*)fileData + offset, chunkSize);
                            if (writeResult != 0) {
                                ARCClose(&fileInfo);
                                return writeResult;
                            }

                            offset += chunkSize;
                        }

                        ARCClose(&fileInfo);
                        return WWWProtocolFinished((s32)request->handle);
                    }

                    WWWProtocolFailed((s32)request->handle);
                    return 1;
                }
                break;
            }
            break;
        case 0x34:
            sBrowserNeedsUpdate = 1;
            break;
        default:
            break;
    }

    return 0;
}

void vcmvSetArchive(void* userData) {
    sManualArcBuffer = userData;

    if (userData != 0) {
        ARCInitHandle(userData, &sManualArcHandle);
    }
}

void vcmvSetRenderMode(GXRenderModeObj* targetMode, GXRenderModeObj* currentMode, u8 nimsGuiFlag) {
    vcmvPageTargetWidth = targetMode;
    vcmvPageCurrentWidth = currentMode;
    vcmvNimsGuiFlag = nimsGuiFlag;
}

bool vcmvIsWidescreen(void);

void vcmvSetupGX(void) {
    Mtx44 proj;
    Mtx mtx;
    u16 width;

    if (vcmvIsWidescreen()) {
        sActiveRenderMode = vcmvPageCurrentWidth;
    } else {
        sActiveRenderMode = vcmvPageTargetWidth;
    }

    width = sActiveRenderMode->fbWidth;
    sFbWidth = width;
    sEfbHeight = sActiveRenderMode->efbHeight;
    sClampedScreenWidth = width;
    if (sClampedScreenWidth > 640) {
        sClampedScreenWidth = 640;
    }

    GXSetViewport(0.0f, 0.0f, (f32)(u16)sClampedScreenWidth, (f32)sEfbHeight, 0.0f, 1.0f);
    GXSetScissor(0, 0, sClampedScreenWidth, sEfbHeight);
    C_MTXOrtho(proj, sHalfScreenHeight, -sHalfScreenHeight, -sHalfScreenWidth, sHalfScreenWidth, 0.0f, -1.0f);
    GXSetProjection(proj, GX_ORTHOGRAPHIC);
    PSMTXIdentity(mtx);
    GXLoadPosMtxImm(mtx, 3);
    sWindowRect.x = 0;
    sWindowRect.y = 0;
    sWindowRect.width = sAlignedWidth;
    sWindowRect.height = sAlignedHeight;
}

void vcmvSetMargin(u16 width) {
    sMarginWidth = width;
    sLeftMargin = (sAlignedWidth - width) * 0.5f;
    sRightMargin = sAlignedWidth - (s32)(width * 0.5f);
}

bool vcmvCreateScreen(s32 width, s32 height) {
    bool ok;
    bool reusedTexture;
    s32 i;
    s32 texSize;
    u8 texIndex;

    sAlignedWidth = ALIGN_NEXT(width, 4);
    sAlignedHeight = ALIGN_NEXT(height, 4);
    sHalfScreenWidth = sAlignedWidth * 0.5f;
    sHalfScreenHeight = sAlignedHeight * 0.5f;
    sInvHalfWidth = 0.5f / sHalfScreenWidth;
    sInvHalfHeight = 0.5f / sHalfScreenHeight;
    sLeftMargin = (sAlignedWidth - sMarginWidth) * 0.5f;
    sRightMargin = sAlignedWidth - (s32)(sMarginWidth * 0.5f);
    sFadeTexIndex = 5;

    ok = 1;
    reusedTexture = 0;

    if (sActiveTexIndex < 5 && sPageTexBuffer[sActiveTexIndex] != NULL) {
        sPrevTexIndex = sActiveTexIndex;
        reusedTexture = 1;
        texIndex = sActiveTexIndex;
    } else {
        sActiveTexIndex = 0;
        sPrevTexIndex = 0;
        texIndex = 0;
    }

    if (!vcmvIsPageFading) {
        sFadeTexIndex = texIndex;
        vcmvIsPageFading = true;
    }

    texSize = sAlignedWidth * sAlignedHeight * 2;

    for (i = 0; i < 4; i++) {
        // why??
        ok &= vcmvAlloc(&sPageTexBuffer[i], texSize, wwwMem2Allocator, wwwMem1Allocator);

        if (ok == 0) {
            goto loop_at_end;
        }

        GXInitTexObj(&sPageTexObj[i], sPageTexBuffer[i], sAlignedWidth, sAlignedHeight, GX_TF_RGB565, GX_CLAMP,
                     GX_CLAMP, 0);
        GXInitTexObjLOD(&sPageTexObj[i], GX_LINEAR, GX_LINEAR, 0.0f, 0.0f, 0.0f, GX_FALSE, GX_FALSE, GX_ANISO_1);
    }

    if (reusedTexture == 0) {
        memset(sPageTexBuffer[sPrevTexIndex], 0, texSize);
        DCFlushRange(sPageTexBuffer[sPrevTexIndex], texSize);
        GXInitTexObj(&sPageTexObj[sPrevTexIndex], sPageTexBuffer[sPrevTexIndex], sAlignedWidth, sAlignedHeight, GX_TF_RGB565,
                     GX_CLAMP, GX_CLAMP, 0);
        GXInitTexObjLOD(&sPageTexObj[sPrevTexIndex], GX_LINEAR, GX_LINEAR, 0.0f, 0.0f, 0.0f, GX_FALSE, GX_FALSE,
                        GX_ANISO_1);
    }

    sSurfaceBufferSize = sAlignedWidth * sAlignedHeight * 4;
    ok &= vcmvAlloc((void**)&sSurfaceBuffer, sSurfaceBufferSize, wwwMem2Allocator, wwwMem1Allocator);

    if (ok != 0) {
        u32 size;

        size = ALIGN_NEXT(sAlignedHeight * ((sAlignedWidth + 0x7F) / 128) * 4, 32);

        ok &= vcmvAlloc((void**)&sDirtyTileBuffer, size, wwwMem2Allocator, wwwMem1Allocator);

        if (ok != 0) {
            return true;
        }

        vcmvFree((void**)&sSurfaceBuffer);
    }

loop_at_end:
    while (i-- != 0) {
        vcmvFree(&sPageTexBuffer[i]);
    }

    return false;
}

void vcmvDestroyScreen(void) {
    s32 i;

    vcmvFree((void**)&sDirtyTileBuffer);
    vcmvFree((void**)&sSurfaceBuffer);

    for (i = 0; i < 4; i++) {
        vcmvFree(&sPageTexBuffer[i]);
    }
}


void vcmvOpenBrowserWindow(s32 renderMode) {
    sBrowserRenderMode = renderMode;
    sPageLoadedFlag = false;
    sIsPageLoading = false;

    GXSetVtxAttrFmt(GX_VTXFMT1, GX_VA_POS, GX_POS_XYZ, GX_S16, 0);
    GXSetVtxAttrFmt(GX_VTXFMT1, GX_VA_TEX0, GX_TEX_ST, GX_S16, 8);
    GXSetVtxAttrFmt(GX_VTXFMT1, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
    GXSetZMode(GX_ENABLE, GX_LEQUAL, GX_ENABLE);
    GXSetAlphaUpdate(GX_ENABLE);
    GXSetNumTevStages(1);
    GXSetNumTexGens(1);
    GXSetNumChans(0);
    {
        GXColor clearColor = {0xFF, 0xFF, 0xFF, 0xFF};
        GXSetCopyClear(clearColor, 0x00FFFFFF);
    }

    vcmvAdvanceTextureIndex();
    sPageChangePending = 1;
    vcmvCreateWindow(sCurrentPageUrl);
}

void vcmvUpdateBrowser(void) {
    for (int i = 0; i < 0x14; i++) {
        if (WWWRunSlice(sBrowserHandle) != 0) {
            break;
        }
    }

    if (sBrowserNeedsUpdate) {
        sBrowserNeedsUpdate = false;
    }

    WWWSurfaceUpdateScreen(0);

    if (sPendingTransition && vcmvFadeTimer >= 0x0C) {
        sPageLoadedFlag = false;
        sInitDelayCounter = 0;
        sTransitionActive = true;
        vcmvAdvanceTextureIndex();
    }

    if (sNeedsRedraw) {
        if (sIsTransitioning) {
            do {
                do {
                    sActiveTexIndex = (sActiveTexIndex + 1) % 4;
                } while (sActiveTexIndex == sTransitionState.activeTexIndex || sActiveTexIndex == sTransitionState.prevTexIndex);
            } while (sActiveTexIndex == sTransitionState.fadeTexIndex);
        }

        vcmvConvertSurfaceToTexture();

        sScrollStep = sLastMouseY;
        sDisplayTexIndex = sActiveTexIndex;

        if (sScrollStarted) {
            sScrollStarted = false;
            sIsTransitioning = true;
            sTransitionState.lastFrame = sManualFrameCount;
        }

        u8 texIndex = sActiveTexIndex;
        if (sIsTransitioning) {
            sTransitionState.history[texIndex].lastMouseY = *(volatile s32*)&sLastMouseY; //! TODO: fake match?
            sTransitionState.history[texIndex].scrollPos = sTransitionState.accumScroll;

            s32 bestPrevScroll = -0x2710;
            s32 bestActiveScroll = 0x2710;
            s32 halfHeight = sTransitionState.rect.height >> 1;

            for (int texIdx = 0; texIdx < 4; texIdx++) {
                s32 scrollPos = sTransitionState.history[texIdx].scrollPos;

                if (scrollPos <= sTransitionState.scrollOffset && scrollPos >= sTransitionState.scrollOffset - halfHeight &&
                    (bestActiveScroll > scrollPos || (bestActiveScroll == scrollPos && texIdx == sActiveTexIndex))) {
                    bestActiveScroll = scrollPos;
                    sTransitionState.activeTexIndex = texIdx;
                }

                if (scrollPos >= sTransitionState.scrollOffset && scrollPos <= sTransitionState.scrollOffset + halfHeight &&
                    (bestPrevScroll < scrollPos || (bestPrevScroll == scrollPos && texIdx == sActiveTexIndex))) {
                    bestPrevScroll = scrollPos;
                    sTransitionState.prevTexIndex = texIdx;
                }
            }

            s32 bestDistSq = 0x05F5E100;
            s32 unused =
                sTransitionState.history[sTransitionState.activeTexIndex].scrollPos + sTransitionState.history[sTransitionState.prevTexIndex].scrollPos;
            for (int i = 0; i < 4; i++) {
                if (i != sTransitionState.activeTexIndex && i != sTransitionState.prevTexIndex) {
                    s32 delta = sTransitionState.scrollOffset - sTransitionState.history[i].scrollPos;
                    s32 distSq = delta * delta;

                    if (bestDistSq > distSq) {
                        bestDistSq = distSq;
                        sTransitionState.fadeTexIndex = i;
                    }
                }
            }
        }

        VCMVRect fullRect = {0x00, 0x00, sAlignedWidth, sAlignedHeight};
        f32 diffRatio;
        vcmvCalcDiffRatio(&fullRect, &diffRatio);
    }

    BOOL intrStatus = OSDisableInterrupts();
    if (sTextureInitialized) {
        sTextureInitialized = false;

        if (sTransitionActive) {
            sTransitionActive = false;
            sFadeTexIndex = sPrevTexIndex;
            vcmvIsPageFading = true;
        }

        sPrevTexIndex = sActiveTexIndex;
    }
    OSRestoreInterrupts(intrStatus);
}

void vcmvSetupQuadRender(GXTexObj* texObj, GXColor color) {
    GXClearVtxDesc();
    GXInvalidateVtxCache();
    GXInvalidateTexAll();
    GXSetVtxDesc(GX_VA_POS, GX_DIRECT);
    GXSetVtxDesc(GX_VA_TEX0, GX_DIRECT);
    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_CLR_RGBA, GX_F32, 0);
    GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_CLR_RGBA, GX_F32, 0);
    GXSetNumChans(0);
    GXSetNumTexGens(1);
    GXSetNumIndStages(0);
    GXSetNumTevStages(1);
    GXSetTevDirect(GX_TEVSTAGE0);
    GXSetTevColorIn(GX_TEVSTAGE0, GX_CC_ZERO, GX_CC_C0, GX_CC_TEXC, GX_CC_ZERO);
    GXSetTevAlphaIn(GX_TEVSTAGE0, GX_CA_ZERO, GX_CA_A0, GX_CA_TEXA, GX_CA_ZERO);
    GXSetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
    GXSetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
    GXSetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR_NULL);
    GXSetTexCoordGen2(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, 0x3C, GX_FALSE, 0x7D);
    GXSetColorUpdate(GX_ENABLE);
    GXSetAlphaUpdate(GX_ENABLE);
    GXSetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_SET);
    GXSetZMode(GX_DISABLE, GX_ALWAYS, GX_DISABLE);
    GXSetAlphaCompare(GX_ALWAYS, 0, GX_AOP_OR, GX_ALWAYS, 0);
    GXSetCullMode(GX_CULL_NONE);
    GXSetClipMode(GX_CLIP_ENABLE);
    GXLoadTexObj(texObj, GX_TEXMAP0);
    GXSetTevColor(GX_TEVREG0, color);
}

void VCMVDrawQuad(const VCMVQuad* quad) {
    GXBegin(GX_QUADS, GX_VTXFMT0, 4);

    GXPosition3f32(quad->x1, quad->y1, 0.0f);
    GXPosition2f32(0.0f, 0.0f);

    GXPosition3f32(quad->x2, quad->y2, 0.0f);
    GXPosition2f32(0.0f, 1.0f);

    GXPosition3f32(quad->x3, quad->y3, 0.0f);
    GXPosition2f32(1.0f, 1.0f);

    GXPosition3f32(quad->x4, quad->y4, 0.0f);
    GXPosition2f32(1.0f, 0.0f);

    GXEnd();
}

void vcmvDrawScreen(f32 slide) {
    Mtx44 proj;

    if (sCachedFadeIndex != sFadeTexIndex || sCachedPrevIndex != sPrevTexIndex || sCachedActiveIndex != sActiveTexIndex) {
        sCachedFadeIndex = sFadeTexIndex;
        sCachedPrevIndex = sPrevTexIndex;
        sCachedActiveIndex = sActiveTexIndex;
    }

    f32 w = sHalfScreenWidth;
    f32 offset = (slide * w) / (f32)(sAlignedWidth * 2);
    C_MTXOrtho(proj, sHalfScreenHeight, -sHalfScreenHeight, -w + offset, w + offset, 0.0f, -1.0f);
    GXSetProjection(proj, GX_ORTHOGRAPHIC);
    GXSetNumTexGens(1);
    GXSetNumChans(0);
    GXSetTexCoordGen2(GX_TEXCOORD0, GX_TG_MTX3x4, GX_TG_TEX0, 0x3C, GX_FALSE, 0x7D);
    GXSetNumTevStages(1);
    GXSetTevOp(GX_TEVSTAGE0, GX_REPLACE);
    GXSetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR_NULL);
    GXClearVtxDesc();
    GXSetVtxDesc(GX_VA_POS, GX_DIRECT);
    GXSetVtxDesc(GX_VA_TEX0, GX_DIRECT);

    GXColor tevColor = {0xFF, 0xFF, 0xFF, 0xFF};

    if (sIsTransitioning) {
        if (sManualFrameCount - sScrollDragFrame > 0xA) {
            sPageScrollDirection = 0;
        }

        sTransitionState.targetScroll = sTransitionState.accumScroll;
        f32 scrollDelta = sTransitionState.targetScroll - sTransitionState.currentScroll;

        if (scrollDelta > 1.0f) {
            if (!(0.5f * (sTransitionState.scrollVelocity * (sTransitionState.scrollVelocity / 2.0f + 1.0f)) < scrollDelta)) {
                f32 temp = sqrt(0.25f + 2.0f * scrollDelta / 2.0f);
                sTransitionState.scrollVelocity = 2.0f * (temp - 0.5f);
            } else {
                sTransitionState.scrollVelocity += 2.0f;

                if (sTransitionState.scrollVelocity > 40.0f) {
                    sTransitionState.scrollVelocity = 40.0f;
                }
            }
        } else if (scrollDelta < -1.0f) {
            if (!(-0.5f * (sTransitionState.scrollVelocity * (sTransitionState.scrollVelocity / 2.0f - 1.0f)) > scrollDelta)) {
                f32 temp2 = sqrt(0.25f - 2.0f * scrollDelta / 2.0f);
                sTransitionState.scrollVelocity = 2.0f * (0.5f - temp2);
            } else {
                sTransitionState.scrollVelocity -= 2.0f;

                if (sTransitionState.scrollVelocity < -40.0f) {
                    sTransitionState.scrollVelocity = -40.0f;
                }
            }
        } else {
            sTransitionState.scrollVelocity = 0.0f;
            sTransitionState.currentScroll = sTransitionState.targetScroll;
            sPageScrollDirection = 0;
            sIsTransitioning = false;
            goto block_21;
        }

        sTransitionState.currentScroll += sTransitionState.scrollVelocity;
        sTransitionState.scrollOffset = sTransitionState.currentScroll + 0.5f;

        s32 rectRight = sTransitionState.rect.x + sTransitionState.rect.width;
        s32 rectBottom = sTransitionState.rect.y + sTransitionState.rect.height;

        f32 u0 = sInvHalfWidth * sTransitionState.rect.x;
        f32 u1 = sInvHalfWidth * rectRight;
        f32 v0 = sInvHalfHeight * sTransitionState.rect.y;
        f32 v1 = sInvHalfHeight * rectBottom;

        f32 x0 = -sHalfScreenWidth + sTransitionState.rect.x;
        f32 x1 = -sHalfScreenWidth + rectRight;
        f32 y0 = sHalfScreenHeight - sTransitionState.rect.y;
        f32 y1 = sHalfScreenHeight - rectBottom;

        s32 shift = sTransitionState.scrollOffset - sTransitionState.history[sTransitionState.activeTexIndex].scrollPos;
        s32 topRow = sTransitionState.rect.y + shift;
        f32 vTop = topRow * sInvHalfHeight;
        f32 yTop = y1 + shift;

        s32 remaining = sTransitionState.rect.height - shift;
        s32 prevRow = sTransitionState.scrollOffset + remaining - sTransitionState.history[sTransitionState.prevTexIndex].scrollPos;
        s32 midRow = sTransitionState.rect.y + prevRow;
        s32 botRow = midRow + shift;
        f32 vMid = midRow * sInvHalfHeight;
        f32 vBot = botRow * sInvHalfHeight;

        if (vTop < v0 || vMid < v0 || vBot > v1) {
        block_21:
            vcmvSetupQuadRender(&sPageTexObj[sDisplayTexIndex], tevColor);
            {
                GXBegin(GX_QUADS, GX_VTXFMT0, 4);

                GXPosition3f32(-sHalfScreenWidth, sHalfScreenHeight, 0.0f);
                GXTexCoord2f32(0.0f, 0.0f);

                GXPosition3f32(-sHalfScreenWidth, -sHalfScreenHeight, 0.0f);
                GXTexCoord2f32(0.0f, 1.0f);

                GXPosition3f32(sHalfScreenWidth, -sHalfScreenHeight, 0.0f);
                GXTexCoord2f32(1.0f, 1.0f);

                GXPosition3f32(sHalfScreenWidth, sHalfScreenHeight, 0.0f);
                GXTexCoord2f32(1.0f, 0.0f);

                GXEnd();
            }

            sPrevTexIndex = sDisplayTexIndex;
        } else {
            vcmvSetupQuadRender(&sPageTexObj[sTransitionState.activeTexIndex], tevColor);
            {
                GXBegin(GX_QUADS, GX_VTXFMT0, 4);

                GXPosition3f32(x0, yTop, 0.0f);
                GXTexCoord2f32(u0, v1);

                GXPosition3f32(x0, y0, 0.0f);
                GXTexCoord2f32(u0, vTop);

                GXPosition3f32(x1, y0, 0.0f);
                GXTexCoord2f32(u1, vTop);

                GXPosition3f32(x1, yTop, 0.0f);
                GXTexCoord2f32(u1, v1);

                GXEnd();
            }

            if (sTransitionState.prevTexIndex != sTransitionState.activeTexIndex) {
                vcmvSetupQuadRender(&sPageTexObj[sTransitionState.prevTexIndex], tevColor);
            }

            {
                GXBegin(GX_QUADS, GX_VTXFMT0, 4);
                GXPosition3f32(x0, y1, 0.0f);
                GXTexCoord2f32(u0, vBot);

                GXPosition3f32(x0, yTop, 0.0f);
                GXTexCoord2f32(u0, vMid);

                GXPosition3f32(x1, yTop, 0.0f);
                GXTexCoord2f32(u1, vMid);

                GXPosition3f32(x1, y1, 0.0f);
                GXTexCoord2f32(u1, vBot);

                GXEnd();
            }

            if (sDisplayTexIndex != sTransitionState.prevTexIndex) {
                vcmvSetupQuadRender(&sPageTexObj[sDisplayTexIndex], tevColor);
            }

            if (sTransitionState.rect.y != 0) {
                GXBegin(GX_QUADS, GX_VTXFMT0, 4);

                GXPosition3f32(-sHalfScreenWidth, sHalfScreenHeight, 0.0f);
                GXTexCoord2f32(0.0f, 0.0f);

                GXPosition3f32(-sHalfScreenWidth, y0, 0.0f);
                GXTexCoord2f32(0.0f, v0);

                GXPosition3f32(sHalfScreenWidth, y0, 0.0f);
                GXTexCoord2f32(1.0f, v0);

                GXPosition3f32(sHalfScreenWidth, sHalfScreenHeight, 0.0f);
                GXTexCoord2f32(1.0f, 0.0f);

                GXEnd();
            }

            if (sTransitionState.rect.x != 0) {
                GXBegin(GX_QUADS, GX_VTXFMT0, 4);

                GXPosition3f32(-sHalfScreenWidth, sHalfScreenHeight, 0.0f);
                GXTexCoord2f32(0.0f, 0.0f);

                GXPosition3f32(-sHalfScreenWidth, -sHalfScreenHeight, 0.0f);
                GXTexCoord2f32(0.0f, 1.0f);

                GXPosition3f32(x0, -sHalfScreenHeight, 0.0f);
                GXTexCoord2f32(u0, 1.0f);

                GXPosition3f32(x0, sHalfScreenHeight, 0.0f);
                GXTexCoord2f32(u0, 0.0f);

                GXEnd();
            }

            if (rectRight < sAlignedWidth) {
                GXBegin(GX_QUADS, GX_VTXFMT0, 4);

                GXPosition3f32(x1, sHalfScreenHeight, 0.0f);
                GXTexCoord2f32(u1, 0.0f);

                GXPosition3f32(x1, -sHalfScreenHeight, 0.0f);
                GXTexCoord2f32(u1, 1.0f);

                GXPosition3f32(sHalfScreenWidth, -sHalfScreenHeight, 0.0f);
                GXTexCoord2f32(1.0f, 1.0f);

                GXPosition3f32(sHalfScreenWidth, sHalfScreenHeight, 0.0f);
                GXTexCoord2f32(1.0f, 0.0f);

                GXEnd();
            }

            if (rectBottom < sAlignedHeight) {
                GXBegin(GX_QUADS, GX_VTXFMT0, 4);

                GXPosition3f32(-sHalfScreenWidth, y1, 0.0f);
                GXTexCoord2f32(0.0f, v1);

                GXPosition3f32(-sHalfScreenWidth, -sHalfScreenHeight, 0.0f);
                GXTexCoord2f32(0.0f, 1.0f);

                GXPosition3f32(sHalfScreenWidth, -sHalfScreenHeight, 0.0f);
                GXTexCoord2f32(1.0f, 1.0f);

                GXPosition3f32(sHalfScreenWidth, y1, 0.0f);
                GXTexCoord2f32(1.0f, v1);

                GXEnd();
            }
        }
    } else {
        if (vcmvIsPageFading) {
            vcmvIsPageFading = false;
            vcmvFadeTimer = 0;

            s32 scrollDir = vcmvJsGetScrollDirection();
            vcmvJsGetScrollAmount();

            vcmvFadeDuration = 0x0F;

            if (scrollDir == 1) {
                sTransitionProgress = 1.0f / vcmvFadeDuration;
            } else if (scrollDir == 2) {
                sTransitionProgress = -1.0f / vcmvFadeDuration;
            } else {
                sTransitionProgress = 0.0f;
            }

            vcmvJsResetScroll();
        }

        f32 curShift = vcmvFadeTimer * sTransitionProgress;
        f32 curShiftStart = curShift - vcmvFadeDuration * sTransitionProgress;

        curShift *= 0.15f;
        curShiftStart *= 0.15f;

        f32 curQuad[4];
        curQuad[0] = (curShiftStart - 0.5f) * sAlignedWidth;
        curQuad[1] = sHalfScreenHeight;
        curQuad[2] = (curShiftStart + 0.5f) * sAlignedWidth;
        curQuad[3] = -sHalfScreenHeight;

        f32 fadeQuad[4];
        fadeQuad[0] = (curShift - 0.5f) * sAlignedWidth;
        fadeQuad[1] = sHalfScreenHeight;
        fadeQuad[2] = (curShift + 0.5f) * sAlignedWidth;
        fadeQuad[3] = -sHalfScreenHeight;

        if (sPageTexBuffer[sPrevTexIndex] != NULL) {
            vcmvSetupQuadRender(&sPageTexObj[sPrevTexIndex], tevColor);
            {
                GXBegin(GX_QUADS, GX_VTXFMT0, 4);

                GXPosition3f32(curQuad[0], curQuad[1], 0.0f);
                GXTexCoord2f32(0.0f, 0.0f);

                GXPosition3f32(curQuad[0], curQuad[3], 0.0f);
                GXTexCoord2f32(0.0f, 1.0f);

                GXPosition3f32(curQuad[2], curQuad[3], 0.0f);
                GXTexCoord2f32(1.0f, 1.0f);

                GXPosition3f32(curQuad[2], curQuad[1], 0.0f);
                GXTexCoord2f32(1.0f, 0.0f);

                GXEnd();
            }
        }

        if (vcmvFadeTimer < vcmvFadeDuration) {
            s32 alpha = 255 - (u8)(vcmvFadeTimer * 255 / vcmvFadeDuration);

            if (sPageTexBuffer[sFadeTexIndex] != NULL) {
                GXColor fadeColor = {0xFF, 0xFF, 0xFF, alpha};
                vcmvSetupQuadRender(&sPageTexObj[sFadeTexIndex], fadeColor);
                {
                    GXBegin(GX_QUADS, GX_VTXFMT0, 4);

                    GXPosition3f32(fadeQuad[0], fadeQuad[1], 0.0f);
                    GXTexCoord2f32(0.0f, 0.0f);

                    GXPosition3f32(fadeQuad[0], fadeQuad[3], 0.0f);
                    GXTexCoord2f32(0.0f, 1.0f);

                    GXPosition3f32(fadeQuad[2], fadeQuad[3], 0.0f);
                    GXTexCoord2f32(1.0f, 1.0f);

                    GXPosition3f32(fadeQuad[2], fadeQuad[1], 0.0f);
                    GXTexCoord2f32(1.0f, 0.0f);

                    GXEnd();
                }
            }

            vcmvFadeTimer++;
        } else {
            sFadeTexIndex = 5;
            sTransitionProgress = 0.0f;
        }
    }
}

void vcmvReloadStartPage(void) {
    u32 temp_r30 = sWindowId;
    const char* temp_r31 = sStartPageUrl;

    vcmvCreateWindow(temp_r31);

    if (sWindowId != temp_r30) {
        WWWCloseBrowserWindow(sBrowserHandle, temp_r30);
    }

    sPageChangePending = 0;
}
