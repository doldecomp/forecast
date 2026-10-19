#include <revolution/OS.h>
#include <revolution/WPAD.h>
#include <revolution/WPAD/WPADInternal.h>
#include <revolution/WPAD/debug_msg.h>

#include <cmath>
#include <cstring>

typedef void (*WPADReportHandler)(u8 chan, u8* pRpt);

void abortInitExtension(s32 chan, s32 result);
void WPADiParseStatusReport(u8 chan, u8* pRpt);
void WPADiParseUserData(u8 chan, u8* pRpt);
void WPADiParseAck(u8 chan, u8* pRpt);
void WPADiParseCoreBtn(u8 chan, u8* pRpt);
void WPADiParseCoreBtnAcc(u8 chan, u8* pRpt);
void WPADiParseCoreBtnExt(u8 chan, u8* pRpt);
void WPADiParseCoreBtnAccExt(u8 chan, u8* pRpt);
void WPADiParseFsBtn(u8 chan, u8* pRpt);
void WPADiParseFsBtnAcc(u8 chan, u8* pRpt);
void WPADiParseClassicBtn(u8 chan, u8* pRpt);
void WPADiParseClassicBtnAcc(u8 chan, u8* pRpt);
void WPADiParseNunchukBtn(u8 chan, u8* pRpt);
void WPADiParseNunchukAccX(u8 chan, u8* pRpt);
void WPADiParseNunchukAccY(u8 chan, u8* pRpt);
void WPADiParseUnsupportedReport(u8 chan, u8* pRpt);

// Input report handlers, indexed by report ID - 0x20.
static WPADReportHandler sReportHandlers[] = {
    WPADiParseStatusReport,
    WPADiParseUserData,
    WPADiParseAck,
    WPADiParseUnsupportedReport,
    WPADiParseUnsupportedReport,
    WPADiParseUnsupportedReport,
    WPADiParseUnsupportedReport,
    WPADiParseUnsupportedReport,
    WPADiParseUnsupportedReport,
    WPADiParseUnsupportedReport,
    WPADiParseUnsupportedReport,
    WPADiParseUnsupportedReport,
    WPADiParseUnsupportedReport,
    WPADiParseUnsupportedReport,
    WPADiParseUnsupportedReport,
    WPADiParseUnsupportedReport,
    WPADiParseCoreBtn,
    WPADiParseCoreBtnAcc,
    WPADiParseCoreBtnExt,
    WPADiParseCoreBtnAccExt,
    WPADiParseFsBtn,
    WPADiParseFsBtnAcc,
    WPADiParseClassicBtn,
    WPADiParseClassicBtnAcc,
    WPADiParseUnsupportedReport,
    WPADiParseUnsupportedReport,
    WPADiParseUnsupportedReport,
    WPADiParseUnsupportedReport,
    WPADiParseUnsupportedReport,
    WPADiParseNunchukBtn,
    WPADiParseNunchukAccX,
    WPADiParseNunchukAccY,
};

extern BOOL __OSInIPL;

// Uninitialised statics are laid out last-defined-first in .sbss.
static u8 sExtType[WPAD_MAX_CONTROLLERS];
static u8 sExtMode[WPAD_MAX_CONTROLLERS];
static u8 sExtFlag[WPAD_MAX_CONTROLLERS];
// How often the extension initialisation has been retried, per channel.
static u8 sExtInitRetry[WPAD_MAX_CONTROLLERS];
static u8 sHidTmpB[WPAD_MAX_CONTROLLERS];
static u8 sHidTmpA[WPAD_MAX_CONTROLLERS];

// .bss is laid out last-defined-first.
// Scratch copy of the raw extension payload, used to detect an all-0xFF read.
static u8 checkBuffer[0x18];
// Per-channel DPD rotation calibration, filled in by getDevConfig.
static f32 rolag[WPAD_MAX_CONTROLLERS];
static f32 centerY[WPAD_MAX_CONTROLLERS];
static f32 centerX[WPAD_MAX_CONTROLLERS];
static f32 calibY[WPAD_MAX_CONTROLLERS];
static f32 calibX[WPAD_MAX_CONTROLLERS];

// All 0xFF: what a read of a missing or unpowered extension returns.
static u8 sNoExtData[RPT_MAX_SIZE] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
};

// Queues the extension identification sequence (re-enable the report type,
// unlock the encryption, read the extension ID). Inlined at both call sites.
static inline void startExtensionInit(s32 chan) {
    WPADCB* p = _wpdcb[chan];

    WPADiClearQueue(&p->extCmdQueue);
    WPADiSendSetReportType(&p->extCmdQueue, p->dataFormat,
                           p->disableContinuous, &abortInitExtension);
    p->unused1 = 1;
    WPADiSendWriteDataCmd(&p->extCmdQueue, 0x55, WM_REG_EXTENSION_F0,
                          &abortInitExtension);
    WPADiSendWriteDataCmd(&p->extCmdQueue, 0x00, WM_REG_EXTENSION_FB,
                          &abortInitExtension);
    WPADiSendReadData(&p->extCmdQueue, p->wmReadDataBuf, 6,
                      WM_REG_EXTENSION_FA, &abortInitExtension);
}

void abortInitExtension(s32 chan, s32 result) {
    WPADCB* p = _wpdcb[chan];
    u8 devType;

    if (result == WPAD_ERR_OK) {
        return;
    }

    WPADiClearQueue(&p->extCmdQueue);
    p->unused1 = 0;

    if (result == WPAD_ERR_NO_CONTROLLER) {
        devType = WPAD_DEV_NOT_FOUND;
    } else if (p->wpInfo.attach) {
        if (sExtInitRetry[chan]++ < 32) {
            p = _wpdcb[chan];
            WPADiClearQueue(&p->extCmdQueue);
            WPADiSendSetReportType(&p->extCmdQueue, p->dataFormat,
                                   p->disableContinuous, &abortInitExtension);
            p->unused1 = 1;
            WPADiSendWriteDataCmd(&p->extCmdQueue, 0x55,
                                  WM_REG_EXTENSION_F0, &abortInitExtension);
            WPADiSendWriteDataCmd(&p->extCmdQueue, 0x00,
                                  WM_REG_EXTENSION_FB, &abortInitExtension);
            WPADiSendReadData(&p->extCmdQueue, p->wmReadDataBuf, 6,
                              WM_REG_EXTENSION_FA, &abortInitExtension);
            return;
        }

        devType = WPAD_DEV_NOT_SUPPORTED;
    } else {
        DEBUGPrint("detaching extension during initialization.\n");
        WPADiSendSetReportType(&p->extCmdQueue, p->dataFormat,
                               p->disableContinuous, NULL);
        return;
    }

    p->devType = devType;

    if (p->extensionCB != NULL) {
        p->extensionCB(chan, devType);
    }
}

void getDevConfig(s32 chan, s32 result) {
    DPDObject obj[WPAD_MAX_DPD_OBJECTS];
    f32 posX[WPAD_MAX_DPD_OBJECTS];
    f32 posY[WPAD_MAX_DPD_OBJECTS];
    f32 angle[WPAD_MAX_DPD_OBJECTS];
    f32 refAngle[WPAD_MAX_DPD_OBJECTS];
    s16 refX[WPAD_MAX_DPD_OBJECTS] = {0x7F, 0x380, 0x380, 0x7F};
    s16 refY[WPAD_MAX_DPD_OBJECTS] = {0x5D, 0x5D, 0x2A2, 0x2A2};
    WPADCB* p = _wpdcb[chan];
    u8* pData = p->wmReadDataPtr;
    u8 i;
    u8 k;
    int sum;
    int start;
    int j;

    f32 aveY;
    f32 aveX;
    f32 deltaX;
    f32 deltaY;

    // Defaults: the sensor bar's four reference points, as seen by an
    // uncalibrated remote.
    for (k = 0; k < WPAD_MAX_DPD_OBJECTS; k++) {
        p->devConfig.dpd[k].x = refX[k];
        p->devConfig.dpd[k].y = refY[k];
        p->devConfig.dpd[k].size = p->defaultDpdSize;
        p->devConfig.dpd[k].traceId = k;
    }

    p->devConfig.accX0g = 0x212;
    p->devConfig.accY0g = 0x212;
    p->devConfig.accZ0g = 0x212;
    p->devConfig.accX1g = 0x27C;
    p->devConfig.accY1g = 0x27C;
    p->devConfig.accZ1g = 0x27C;
    p->devConfig.volume = 0;
    p->devConfig.motor = 0;

    if (result != WPAD_ERR_OK) {
        return;
    }

    if (p->configIndex == 0) {
        for (i = 0; i < 2; i++) {
            start = i * 11;
            sum = 0;
            for (j = start; j < start + 10; j++) {
                sum += pData[j];
            }

            if ((u8)(sum + 0x55) == pData[start + 10]) {
                DEBUGPrint("Dpd Setting is ok.\n");

                obj[0].x = (s16)(s32)pData[start + 0] | ((pData[start + 2] << 4) & 0x300);
                obj[0].y = 0x2FF - (s16)((s16)(s32)pData[start + 1] | ((pData[start + 2] << 2) & 0x300));
                obj[0].size = p->defaultDpdSize;
                obj[0].traceId = 0;
                obj[1].x = (s16)(s32)pData[start + 3] | ((pData[start + 2] << 8) & 0x300);
                obj[1].y = 0x2FF - (s16)((s16)(s32)pData[start + 4] | ((pData[start + 2] << 6) & 0x300));
                obj[1].size = p->defaultDpdSize;
                obj[1].traceId = 1;
                obj[2].x = (s16)(s32)pData[start + 5] | ((pData[start + 7] << 4) & 0x300);
                obj[2].y = 0x2FF - (s16)((s16)(s32)pData[start + 6] | ((pData[start + 7] << 2) & 0x300));
                obj[2].size = p->defaultDpdSize;
                obj[2].traceId = 2;
                obj[3].x = (s16)(s32)pData[start + 8] | ((pData[start + 7] << 8) & 0x300);
                obj[3].y = 0x2FF - (s16)((s16)(s32)pData[start + 9] | ((pData[start + 7] << 6) & 0x300));
                obj[3].size = p->defaultDpdSize;
                obj[3].traceId = 3;

                // Sort the points into the four quadrants of the sensor image.
                for (j = 0; j < WPAD_MAX_DPD_OBJECTS; j++) {
                    if (obj[j].x < 0x200 && obj[j].y < 0x180) {
                        p->devConfig.dpd[0].x = obj[j].x;
                        p->devConfig.dpd[0].y = obj[j].y;
                        p->devConfig.dpd[0].size = obj[j].size;
                        p->devConfig.dpd[0].traceId = obj[j].traceId;
                    }
                    if (obj[j].x > 0x200 && obj[j].y < 0x180) {
                        p->devConfig.dpd[1].x = obj[j].x;
                        p->devConfig.dpd[1].y = obj[j].y;
                        p->devConfig.dpd[1].size = obj[j].size;
                        p->devConfig.dpd[1].traceId = obj[j].traceId;
                    }
                    if (obj[j].x > 0x200 && obj[j].y > 0x180) {
                        p->devConfig.dpd[2].x = obj[j].x;
                        p->devConfig.dpd[2].y = obj[j].y;
                        p->devConfig.dpd[2].size = obj[j].size;
                        p->devConfig.dpd[2].traceId = obj[j].traceId;
                    }
                    if (obj[j].x < 0x200 && obj[j].y > 0x180) {
                        p->devConfig.dpd[3].x = obj[j].x;
                        p->devConfig.dpd[3].y = obj[j].y;
                        p->devConfig.dpd[3].size = obj[j].size;
                        p->devConfig.dpd[3].traceId = obj[j].traceId;
                    }
                }
                break;
            } else {
                DEBUGPrint("Dpd Setting is broken.\n");
            }
        }
    } else {
        DEBUGPrint("Old Firmware uses default values because it has no Dpd Setting.\n");
    }

    aveX = aveY = 0.0f;
    for (k = 0; k < WPAD_MAX_DPD_OBJECTS; k++) {
        posX[k] = p->devConfig.dpd[k].x;
        posY[k] = p->devConfig.dpd[k].y;
        aveX += (f32)(p->devConfig.dpd[k].x - refX[k]);
        aveY += (f32)(p->devConfig.dpd[k].y - refY[k]);
        DEBUGPrint("x = %lf, y = %lf\n", posX[k], posY[k]);
    }
    aveX *= 0.25f;
    aveY *= 0.25f;

    deltaX = (f32)atan((126.5f + aveX) / 1332.5f) - (f32)atan(126.5f / 1332.5f);
    deltaY = (f32)atan((93.0f + aveY) / 1337.4f) - (f32)atan(93.0f / 1337.4f);

    calibX[chan] = 1332.5f * (-1.0f * (f32)tan(deltaX));
    calibY[chan] = 1337.4f * (-1.0f * (f32)tan(deltaY));

    DEBUGPrint("difaveX = %lf, difaveY = %lf\n", aveX, aveY);
    DEBUGPrint("deltaX  = %lf, deltaY  = %lf\n", deltaX, deltaY);
    DEBUGPrint("calibX = %lf, calibY = %lf\n", calibX[chan], calibY[chan]);

    centerX[chan] = centerY[chan] = 0.0f;
    for (k = 0; k < WPAD_MAX_DPD_OBJECTS; k++) {
        posX[k] += calibX[chan];
        posY[k] += calibY[chan];
        centerX[chan] += posX[k];
        centerY[chan] += posY[k];
    }
    centerX[chan] *= 0.25f;
    centerY[chan] *= 0.25f;
    DEBUGPrint("centX  = %lf, centY  = %lf\n", centerX[chan], centerY[chan]);

    rolag[chan] = 0.0f;
    for (k = 0; k < WPAD_MAX_DPD_OBJECTS; k++) {
        angle[k] = atan((posY[k] - centerY[chan]) / (posX[k] - centerX[chan]));
        refAngle[k] = atan(((f32)refY[k] - 383.5f) / ((f32)refX[k] - 511.5f));
        rolag[chan] += angle[k] - refAngle[k];
    }
    rolag[chan] *= 0.25f;
    DEBUGPrint("rolag  = %lf\n", rolag[chan]);

    for (k = 0; k < 2; k++) {
        start = k * 10;
        sum = 0;
        if (p->configIndex == 0) {
            start += 22;
        }

        for (j = start; j < start + 9; j++) {
            sum += pData[j];
        }

        if ((u8)(sum + 0x55) == pData[start + 9]) {
            DEBUGPrint("Acc is ok.\n");
            p->devConfig.accX0g = (pData[start + 0] << 2) | ((pData[start + 3] >> 4) & 3);
            p->devConfig.accY0g = (pData[start + 1] << 2) | ((pData[start + 3] >> 2) & 3);
            p->devConfig.accZ0g = (pData[start + 2] << 2) | (pData[start + 3] & 3);
            p->devConfig.accX1g = (pData[start + 4] << 2) | ((pData[start + 7] >> 4) & 3);
            p->devConfig.accY1g = (pData[start + 5] << 2) | ((pData[start + 7] >> 2) & 3);
            p->devConfig.accZ1g = (pData[start + 6] << 2) | (pData[start + 7] & 3);
            p->devConfig.volume = pData[start + 8] & 0x7F;
            p->devConfig.motor = pData[start + 8] & 0x80;
            break;
        } else {
            DEBUGPrint("Acc is broken.\n");
        }
    }

    DEBUGPrint("0G:  accX = %d,  accY = %d,  accZ = %d\n", p->devConfig.accX0g,
               p->devConfig.accY0g, p->devConfig.accZ0g);
    DEBUGPrint("1G:  accX = %d,  accY = %d,  accZ = %d\n", p->devConfig.accX1g,
               p->devConfig.accY1g, p->devConfig.accZ1g);
    DEBUGPrint("Volume: %d,   Motor: %d\n", p->devConfig.volume, p->devConfig.motor);
}

void getExtConfig(s32 chan, s32 result) {
    WPADCB* p = _wpdcb[chan];
    u8* pData = p->wmReadDataPtr;
    int found = -1;
    int base;
    int block;
    int i;
    int sum;

    switch (result) {
    case WPAD_ERR_NO_CONTROLLER:
        p->devType = WPAD_DEV_NOT_FOUND;
        break;
    case WPAD_ERR_OK:
        p->devType = sExtType[chan];
        break;
    default:
        p->devType = WPAD_DEV_NOT_SUPPORTED;
        break;
    }

    p->devMode = sExtMode[chan];

    if ((u8)(p->devType + 0xFF) <= 1) {
        for (block = 0, base = 0; block < WPAD_MAX_CONFIG_BLOCKS;
             block++, base += 16) {
            sum = 0;
            for (i = base; i < base + WPAD_DEV_CONFIG_BLOCK_SIZE + 4; i++) {
                sum += pData[i];
            }

            if ((u8)(sum + WPAD_CONFIG_BLOCK_CHECKSUM_BIAS) == pData[base + 14] &&
                (u8)(sum + 0xAA) == pData[base + 15]) {
                found = base;
            }
        }

        switch (p->devType) {
        case WPAD_DEV_FREESTYLE:
            if (found < 0) {
                DEBUGPrint("check sum error.\n");
                p->extConfig.u.fs.accX0g = 0x200;
                p->extConfig.u.fs.accY0g = 0x200;
                p->extConfig.u.fs.accZ0g = 0x200;
                p->extConfig.u.fs.accX1g = 0x2CC;
                p->extConfig.u.fs.accY1g = 0x2CC;
                p->extConfig.u.fs.accZ1g = 0x2CC;
            } else {
                p->extConfig.u.fs.accX0g = (pData[found] << 2) | ((pData[found + 3] >> 4) & 3);
                p->extConfig.u.fs.accY0g = (pData[found + 1] << 2) | ((pData[found + 3] >> 2) & 3);
                p->extConfig.u.fs.accZ0g = (pData[found + 2] << 2) | (pData[found + 3] & 3);
                p->extConfig.u.fs.accX1g = (pData[found + 4] << 2) | ((pData[found + 7] >> 4) & 3);
                p->extConfig.u.fs.accY1g = (pData[found + 5] << 2) | ((pData[found + 7] >> 2) & 3);
                p->extConfig.u.fs.accZ1g = (pData[found + 6] << 2) | (pData[found + 7] & 3);
                p->extConfig.u.fs.at_0x04 = (s8)pData[found + 8];
                p->extConfig.u.fs.at_0x02 = (s8)pData[found + 9];
                p->extConfig.u.fs.stickXCenter = (s8)pData[found + 10];
                p->extConfig.u.fs.at_0x0a = (s8)pData[found + 11];
                p->extConfig.u.fs.at_0x08 = (s8)pData[found + 12];
                p->extConfig.u.fs.stickYCenter = (s8)pData[found + 13];

                DEBUGPrint("0G:  fsaccX = %d,  fsaccY = %d,  fsaccZ = %d\n",
                           p->extConfig.u.fs.accX0g, p->extConfig.u.fs.accY0g,
                           p->extConfig.u.fs.accZ0g);
                DEBUGPrint("1G:  fsaccX = %d,  fsaccY = %d,  fsaccZ = %d\n",
                           p->extConfig.u.fs.accX1g, p->extConfig.u.fs.accY1g,
                           p->extConfig.u.fs.accZ1g);
                DEBUGPrint("FS:  X = %d,  X max = %d,  X min = %d\n",
                           p->extConfig.u.fs.stickXCenter, p->extConfig.u.fs.at_0x04,
                           p->extConfig.u.fs.at_0x02);
                DEBUGPrint("FS:  Y = %d,  Y max = %d,  Y min = %d\n",
                           p->extConfig.u.fs.stickYCenter, p->extConfig.u.fs.at_0x0a,
                           p->extConfig.u.fs.at_0x08);
            }
            break;
        case WPAD_DEV_CLASSIC:
            p->extConfig.u.cl.at_0x04 = (s8)pData[found];
            p->extConfig.u.cl.at_0x02 = (s8)pData[found + 1];
            p->extConfig.u.cl.lStickXCenter = (s8)pData[found + 2];
            p->extConfig.u.cl.at_0x0a = (s8)pData[found + 3];
            p->extConfig.u.cl.at_0x08 = (s8)pData[found + 4];
            p->extConfig.u.cl.lStickYCenter = (s8)pData[found + 5];
            p->extConfig.u.cl.at_0x10 = (s8)pData[found + 6];
            p->extConfig.u.cl.at_0x0e = (s8)pData[found + 7];
            p->extConfig.u.cl.rStickXCenter = (s8)pData[found + 8];
            p->extConfig.u.cl.at_0x16 = (s8)pData[found + 9];
            p->extConfig.u.cl.at_0x14 = (s8)pData[found + 10];
            p->extConfig.u.cl.rStickYCenter = (s8)pData[found + 11];
            p->extConfig.u.cl.triggerLZero = pData[found + 12];
            p->extConfig.u.cl.triggerRZero = pData[found + 13];

            DEBUGPrint("CL:  X = %d,  X max = %d,  X min = %d\n",
                       p->extConfig.u.cl.lStickXCenter, p->extConfig.u.cl.at_0x04,
                       p->extConfig.u.cl.at_0x02);
            DEBUGPrint("CL:  Y = %d,  Y max = %d,  Y min = %d\n",
                       p->extConfig.u.cl.lStickYCenter, p->extConfig.u.cl.at_0x0a,
                       p->extConfig.u.cl.at_0x08);
            DEBUGPrint("CR:  X = %d,  X max = %d,  X min = %d\n",
                       p->extConfig.u.cl.rStickXCenter, p->extConfig.u.cl.at_0x10,
                       p->extConfig.u.cl.at_0x0e);
            DEBUGPrint("CR:  Y = %d,  Y max = %d,  Y min = %d\n",
                       p->extConfig.u.cl.rStickYCenter, p->extConfig.u.cl.at_0x16,
                       p->extConfig.u.cl.at_0x14);
            DEBUGPrint("LR:  L = %d,  R = %d\n", p->extConfig.u.cl.triggerLZero,
                       p->extConfig.u.cl.triggerRZero);
            break;
        }

        p->calibrated = FALSE;
    }

    if (p->cmdBlkCB != NULL) {
        p->cmdBlkCB(chan, p->devType);
        p->cmdBlkCB = NULL;
    }
}

void getExtType(s32 chan, s32 result) {
    WPADCB* p = _wpdcb[chan];
    u8* pData = p->wmReadDataPtr;
    WPADCommandQueue* pQueue = &p->extCmdQueue;

    if (result == WPAD_ERR_OK) {
        DEBUGPrint("devId: %d, subId: %d\n", pData[5], pData[0]);

        sExtFlag[chan] = 0;
        sExtMode[chan] = pData[4];

        switch (pData[5]) {
        case 0:
            sExtType[chan] = WPAD_DEV_FREESTYLE;
            break;
        case 1:
            sExtType[chan] = WPAD_DEV_CLASSIC;
            break;
        case 3:
            if (__OSInIPL) {
                sExtType[chan] = WPAD_DEV_CLASSIC;
                sExtMode[chan] = 1;
                sExtFlag[chan] = 1;
            } else {
                sExtType[chan] = WPAD_DEV_FUTURE;
            }
            break;
        case 2:
        default:
            sExtType[chan] = WPAD_DEV_FUTURE;
            break;
        }

        if (sExtType[chan] == WPAD_DEV_CLASSIC &&
            (sExtMode[chan] < 1 || sExtMode[chan] > 3)) {
            sExtType[chan] = WPAD_DEV_NOT_SUPPORTED;
        }

        DEBUGPrint("type : %d\n", sExtType[chan]);
        DEBUGPrint("mode : %d\n", sExtMode[chan]);

        if ((u8)(sExtType[chan] + 5) <= 1) {
            p->devType = sExtType[chan];
            p->devMode = sExtMode[chan];

            if (p->extensionCB != NULL) {
                p->extensionCB(chan, p->devType);
            }
        } else if ((u8)(sExtType[chan] + 0xFF) <= 1) {
            p->unused1 = 2;
            WPADiSendWriteDataCmd(pQueue, 0xAA, WM_REG_EXTENSION_F0,
                                  &abortInitExtension);
            WPADiCreateKey(chan);
            WPADiSendWriteData(pQueue, &p->encryptionKey[0], 6,
                               WM_EXT_REG_ADDR(EXTENSION, 0x40),
                               &abortInitExtension);
            WPADiSendWriteData(pQueue, &p->encryptionKey[6], 6,
                               WM_EXT_REG_ADDR(EXTENSION, 0x46),
                               &abortInitExtension);
            WPADiSendWriteData(pQueue, &p->encryptionKey[12], 4,
                               WM_EXT_REG_ADDR(EXTENSION, 0x4C),
                               &abortInitExtension);
            WPADiSendReadData(pQueue, p->wmReadDataBuf, 0x20,
                              WM_REG_EXTENSION_CONFIG,
                              (WPADCallback)p->extensionCB);
        } else {
            p->unused1 = 3;
            WPADiSendWriteDataCmd(pQueue, 0xAA, WM_REG_EXTENSION_F0,
                                  &abortInitExtension);
            WPADiCreateKeyFor3rd(chan);
            WPADiSendWriteData(pQueue, &p->encryptionKey[0], 6,
                               WM_EXT_REG_ADDR(EXTENSION, 0x40),
                               &abortInitExtension);
            WPADiSendWriteData(pQueue, &p->encryptionKey[6], 6,
                               WM_EXT_REG_ADDR(EXTENSION, 0x46),
                               &abortInitExtension);
            WPADiSendWriteData(pQueue, &p->encryptionKey[12], 4,
                               WM_EXT_REG_ADDR(EXTENSION, 0x4C),
                               &abortInitExtension);
            WPADiSendReadData(pQueue, p->wmReadDataBuf, 0x20,
                              WM_REG_EXTENSION_CONFIG,
                              (WPADCallback)p->extensionCB);
        }
    } else {
        p->devType = WPAD_DEV_NOT_SUPPORTED;
        p->devMode = 0;
    }
}

#pragma dont_inline on
void getGameInfo(s32 chan, s32 result, u8 index) {
    WPADCB* p = _wpdcb[chan];
    u8* pData = p->wmReadDataPtr;
    int i;
    u32 sum = 0;

    if (result == WPAD_ERR_OK) {
        for (i = 0; i < 0x2F; i++) {
            sum += pData[i];
        }

        if (pData[0x2F] ==
            (u8)(sum + WPAD_CONFIG_BLOCK_CHECKSUM_BIAS)) {
            memcpy(&p->gameInfo, pData, sizeof(WPADGameInfo));
            p->UNK_0x38[index] = 0;
        } else {
            p->UNK_0x38[index] = -4;
        }
    }
}

#pragma dont_inline off

s32 WPADiHIDParser(u8 port, u8* pRpt) {
    BOOL enabled;
    s32 result = 0;

    if (pRpt[RPT_REPORT_ID] >= RPTID_IN_LO &&
        pRpt[RPT_REPORT_ID] <= RPTID_IN_HI) {
        enabled = OSDisableInterrupts();
        sReportHandlers[pRpt[RPT_REPORT_ID] - RPTID_IN_LO](port, pRpt);
        OSRestoreInterrupts(enabled);

        WPADiExcludeButton(port);
        WPADiCopyOut(port);
    } else {
        result = -1;
    }

    return result;
}

void WPADiParseStatusReport(u8 chan, u8* pRpt) {
    WPADCB* p;
    BOOL enabled;
    BOOL attach;
    u8* pBuf;
    WPADExtensionCallback extCB;
    int i;
    u8 battery;

    DEBUGPrint("Received report 20\n");

    enabled = OSDisableInterrupts();

    p = _wpdcb[chan];

    if (!p->handshakeFinished) {
        OSRestoreInterrupts(enabled);
        return;
    }

    pBuf = p->rxBufs[p->rxBufIndex];
    for (i = 0; i < (int)RX_BUFFER_SIZE; i++) {
        pBuf[i] = 0;
    }

    attach = p->wpInfo.attach;

    ((WPADStatus*)pBuf)->button =
        (pRpt[RPT_BTN0] | (u16)((u16)(pRpt[RPT_BTN1] << 8) & 0xFF00)) & 0x9F1F;

    p->wpInfo.nearempty = (u8)((pRpt[RPT_BTN0] & 0x80) >> 7);
    p->wpInfo.dpd = (BOOL)((pRpt[RPT20_FLAGS] & 0x8) >> 3);
    p->wpInfo.speaker = (BOOL)((pRpt[RPT20_FLAGS] & 0x4) >> 2);
    p->wpInfo.attach = (BOOL)((pRpt[RPT20_FLAGS] & 0x2) >> 1);
    p->wpInfo.lowBat = (BOOL)(pRpt[RPT20_FLAGS] & 0x1);
    p->wpInfo.led = (u8)((pRpt[RPT20_FLAGS] & 0xF0) >> 4);
    // The protocol nibble is masked out by the shift: always 0.
    p->wpInfo.protocol = (u8)((pRpt[RPT20_PROTO_FW] & 0xF) >> 4);
    p->wpInfo.firmware = (u8)(pRpt[RPT20_PROTO_FW] & 0xF0);

    battery = pRpt[RPT20_BATTERY];
    if (battery >= 0x55) {
        p->wpInfo.battery = 4;
    } else if (battery >= 0x44) {
        p->wpInfo.battery = 3;
    } else if (battery >= 0x33) {
        p->wpInfo.battery = 2;
    } else if (battery >= 3) {
        p->wpInfo.battery = 1;
    } else {
        p->wpInfo.battery = 0;
    }

    if (p->wpInfo.attach) {
        if (!attach) {
            DEBUGPrint("initialize attachment\n");

            sExtInitRetry[chan] = 0;
            extCB = p->extensionCB;
            startExtensionInit(chan);

            p->calibrated = 0;
            p->devType = 0xFF;
            p->devMode = 0;

            if (extCB != NULL) {
                extCB(chan, 0xFF);
            }
        } else {
            DEBUGPrint("already initialized\n");
        }
    } else {
        p->devType = 0;
        p->devMode = 0;
        WPADiClearQueue(&p->extCmdQueue);
        WPADiSendSetReportType(&p->extCmdQueue, p->dataFormat,
                               p->disableContinuous, NULL);

        if (attach) {
            if (p->extensionCB != NULL) {
                p->extensionCB(chan, 0);
            }
        } else {
            DEBUGPrint("already disconnected\n");
        }
    }

    if (p->wpInfoOut != NULL) {
        memcpy(p->wpInfoOut, &p->wpInfo, sizeof(WPADInfo));
        p->wpInfoOut = NULL;
    }

    if (p->cmdBlkCB != NULL && p->statusReqBusy) {
        p->cmdBlkCB(chan, 0);
        p->cmdBlkCB = NULL;
    }

    ((WPADStatus*)pBuf)->err = WPAD_ERR_COMMUNICATION_ERROR;
    ((WPADStatus*)pBuf)->dev = p->devType;
    p->statusReqBusy = 0;
    p->rxBufIndex = p->rxBufIndex == 0;
    OSRestoreInterrupts(enabled);
}

void WPADiParseUserData(u8 chan, u8* pRpt) {
    u16 address;
    WPADCB* p;
    BOOL enabled;
    u8* pBuf;
    u16 start;
    u8 len;
    u16 addr;
    int i;
    s32 result;
    s16 offset;

    enabled = OSDisableInterrupts();

    p = _wpdcb[chan];
    pBuf = p->rxBufs[p->rxBufIndex];
    for (i = 0; i < (int)RX_BUFFER_SIZE; i++) {
        pBuf[i] = 0;
    }

    ((WPADStatus*)pBuf)->button =
        (pRpt[RPT_BTN0] | (pRpt[RPT_BTN1] << 8)) & 0x9F1F;

    len = (pRpt[RPT21_SIZE_ERR] >> 4) + 1;
    addr = (pRpt[RPT21_ADDR0] << 8) | pRpt[RPT21_ADDR1];

    if (pRpt[RPT21_SIZE_ERR] & 0xF) {
        DEBUGPrint("read error happens!\n");

        p->wmReadHadError = -1;

        if (p->cmdBlkCB != NULL) {
            if (p->extensionCB == NULL || p->extensionCB != p->cmdBlkCB) {
                p->cmdBlkCB(chan, WPAD_ERR_TRANSFER);
            }
            p->cmdBlkCB = NULL;
        }

        p->status = 0;
    }

    start = p->wmReadAddress;
    address = p->wmReadAddress >> 16;

    offset = addr - start;
    if (addr >= start && (s32)addr <= (s32)(start + p->wmReadLength)) {
        memcpy(p->wmReadDataPtr + offset, pRpt + RPT21_DATA, len);

        if (start + p->wmReadLength == addr + len) {
            DEBUGPrint("base addr: %08x\n", p->wmReadAddress);
            DEBUGPrint("length   : %d\n", p->wmReadLength);

            result = p->wmReadHadError < 0 ? -3 : 0;

            DEBUGPrint("i2c = %04x\n", address);
            DEBUGPrint("enc = %d\n", p->unused1);

            if (address == (WM_EXT_REG_ADDR(EXTENSION, 0x20) >> 16)) {
                DEBUGPrint("Access to extension register.\n");

                if ((u8)(p->unused1 + 0xFE) <= 1) {
                    DEBUGPrint("Decode!!!!\n");
                    DEBUGPrint("    len = %d, addr = %04x\n", len,
                               (u32)start);
                    WPADiDecode(chan, p->wmReadDataPtr, len, start);
                }
            }

            if ((p->wmReadAddress == WM_ADDR_MEM_DEV_CONFIG_0 &&
                 p->configIndex == 0) ||
                (p->wmReadAddress == WM_ADDR_MEM_176C &&
                 p->configIndex == 1)) {
                getDevConfig(chan, result);
            }

            if (p->wmReadAddress == WM_REG_EXTENSION_CONFIG) {
                getExtConfig(chan, result);
            }

            if (p->wmReadAddress == WM_REG_EXTENSION_FA) {
                getExtType(chan, result);
            }

            if (p->wmReadAddress == WM_ADDR_MEM_GAME_INFO_0) {
                getGameInfo(chan, result, 0);
            }

            if (p->wmReadAddress == WM_ADDR_MEM_GAME_INFO_1) {
                getGameInfo(chan, result, 1);
            }

            if (p->cmdBlkCB != NULL) {
                p->cmdBlkCB(chan, result);
                p->cmdBlkCB = NULL;
            }

            p->status = 0;
        }
    } else {
        DEBUGPrint("received data is out of range!\n");
    }

    ((WPADStatus*)pBuf)->err = WPAD_ERR_COMMUNICATION_ERROR;
    ((WPADStatus*)pBuf)->dev = p->devType;
    p->rxBufIndex = p->rxBufIndex == 0;
    OSRestoreInterrupts(enabled);
}

void WPADiParseAck(u8 chan, u8* pRpt) {
    WPADCB* p;
    BOOL enabled;
    u8* pBuf;
    u8 reportID;
    u8 errCode;
    s32 result;
    int i;

    enabled = OSDisableInterrupts();

    DEBUGPrint("Received ack!\n");

    p = _wpdcb[chan];
    pBuf = p->rxBufs[p->rxBufIndex];

    for (i = 0; i < (int)RX_BUFFER_SIZE; i++) {
        pBuf[i] = 0;
    }

    ((WPADStatus*)pBuf)->button = (pRpt[RPT_BTN0] | (u16)((u16)(pRpt[RPT_BTN1] << 8) & 0xFF00)) & 0x9F1F;

    reportID = pRpt[RPT22_ACKED_RPT_ID];
    errCode = pRpt[RPT22_ERR_CODE];

    DEBUGPrint("ack --> report ID = %02x, error code = %d\n", reportID,
               errCode);

    result = (errCode == 0) ? 0 : -3;
    ((WPADStatus*)pBuf)->err = -2;
    ((WPADStatus*)pBuf)->dev = p->devType;

    if (errCode != 0) {
        DEBUGPrint("ack error --> report ID = %d, error code = %d\n", reportID,
                   errCode);
    }

    if (p->lastReportID == reportID) {
        if (p->cmdBlkCB != NULL) {
            p->cmdBlkCB(chan, result);
            p->cmdBlkCB = NULL;
        }

        p->status = WPAD_ERR_OK;
    } else {
        DEBUGPrint("invalid ack!\n");
    }

    p->rxBufIndex = p->rxBufIndex == 0;

    OSRestoreInterrupts(enabled);
}

void __parse_dpd_data(s32 chan, WPADStatus** ppStatus, u8 format, u8* pData, s32 len) {
    u8 i;
    u8 x;
    u8 y;
    u8 xy;
    f32 a;
    f32 b;

    if (format == WPAD_DPD_STANDARD) {
        for (i = 0; i < WPAD_MAX_DPD_OBJECTS; i++) {
            if (i * 3 + 2 < len) {
                x = *(pData + i * 3);
                y = *(pData + i * 3 + 1);
                xy = *(pData + i * 3 + 2);

                (*ppStatus)->obj[i].x = (s16)((s16)((s16)(((u16)x) & 0xFF) | (u16)(((u16)xy & 0x30) << 4)));
                (*ppStatus)->obj[i].y = (s16)(0x2FF - (s16)((s16)(((u16)y) & 0xFF) | (u16)(((u16)xy & 0xC0) << 2)));
                (*ppStatus)->obj[i].size = (u16)(((xy & 0xF) & 0xFF));
                (*ppStatus)->obj[i].size = (u8)((f32)((*ppStatus)->obj[i].size) * (f32)((*ppStatus)->obj[i].size) * 3.141592f);
                if ((*ppStatus)->obj[i].size == 0 || (*ppStatus)->obj[i].x == (s16)(0x3FF) ||
                    (*ppStatus)->obj[i].y == (s16)(0x2FF)) {
                    (*ppStatus)->obj[i].x = (s16)0;
                    (*ppStatus)->obj[i].y = (s16)(0x2FF);
                    (*ppStatus)->obj[i].size = 0;
                }
                (*ppStatus)->obj[i].traceId = i;
            } else {
                (*ppStatus)->obj[i].x = (s16)0;
                (*ppStatus)->obj[i].y = (s16)(0x2FF);
                (*ppStatus)->obj[i].size = 0;
                (*ppStatus)->obj[i].traceId = i;
            }
        }
    } else if (format == WPAD_DPD_BASIC) {
        for (i = 0; i < WPAD_MAX_DPD_OBJECTS; i++) {
            x = *(pData + i * 3 - (i / 2));
            y = *(pData + i * 3 - (i / 2) + 1);
            xy = *(pData + (i / 2) * 5 + 2);

            if ((i % 2) == 0) {
                (*ppStatus)->obj[i].x = (s16)((s16)((s16)(((u16)x) & 0xFF) | (u16)(((u16)(xy & 0x30)) << 4)));
                (*ppStatus)->obj[i].y = (s16)(0x2FF - (s16)((s16)(((u16)y) & 0xFF) | (u16)(((u16)(xy & 0xC0)) << 2)));
            } else {
                (*ppStatus)->obj[i].x = (s16)((s16)((s16)(((u16)x) & 0xFF) | (u16)(((u16)(xy & (u8)(0x30 >> 4))) << 8)));
                (*ppStatus)->obj[i].y = (s16)(0x2FF - (s16)((s16)(((u16)y) & 0xFF) | (u16)(((u16)(xy & (u8)(0xC0 >> 4))) << 6)));
            }
            if ((*ppStatus)->obj[i].x == (s16)(0x3FF) || (*ppStatus)->obj[i].y == (s16)(0x2FF)) {
                (*ppStatus)->obj[i].x = (s16)0;
                (*ppStatus)->obj[i].y = (s16)(0x2FF);
                (*ppStatus)->obj[i].size = 0;
            } else {
                (*ppStatus)->obj[i].size = 12;
            }
            (*ppStatus)->obj[i].traceId = i;
        }
    }

    for (i = 0; i < WPAD_MAX_DPD_OBJECTS; i++) {
        if ((*ppStatus)->obj[i].x != 0 || (*ppStatus)->obj[i].y != (0x2FF)) {
            WPADStatus* p_cur = *ppStatus;
            a = (((f32)(*ppStatus)->obj[i].x) + calibX[chan] - centerX[chan]);
            b = (((f32)p_cur->obj[i].y) + calibY[chan] - centerY[chan]);
            p_cur->obj[i].x =
                (s16)((f32)((f32)a * (f32)cos(-1.0f * rolag[chan])) - (f32)((f32)b * (f32)sin(-1.0f * rolag[chan])) + centerX[chan]);
            (*ppStatus)->obj[i].y =
                (s16)((f32)((f32)a * (f32)sin(-1.0f * rolag[chan])) + (f32)((f32)b * (f32)cos(-1.0f * rolag[chan])) + centerY[chan]);
        }
    }
}

void WPADiParseCoreBtn(u8 chan, u8* pRpt) {
    WPADCB* p;
    BOOL enabled;
    u8* pBuf;
    int i;

    enabled = OSDisableInterrupts();

    p = _wpdcb[chan];
    pBuf = p->rxBufs[p->rxBufIndex];

    for (i = 0; i < (int)RX_BUFFER_SIZE; i++) {
        pBuf[i] = 0;
    }

    ((WPADStatus*)pBuf)->button =
        (pRpt[RPT_BTN0] | (u16)((u16)(pRpt[RPT_BTN1] << 8) & 0xFF00)) & 0x9F1F;

    if (p->dataFormat == WPAD_FMT_CORE_BTN) {
        ((WPADStatus*)pBuf)->err = WPAD_ERR_OK;
    } else {
        ((WPADStatus*)pBuf)->err = WPAD_ERR_INVALID;
    }

    ((WPADStatus*)pBuf)->dev = p->devType;

    p->wpInfo.nearempty = (pRpt[RPT_BTN0] & 0x80) != 0;

    p->rxBufIndex = p->rxBufIndex == 0;

    OSRestoreInterrupts(enabled);
}

void WPADiParseCoreBtnAcc(u8 chan, u8* pRpt) {
    WPADCB* p;
    u8* pBuf;
    int i;
    BOOL enabled;

    p = _wpdcb[chan];
    pBuf = p->rxBufs[p->rxBufIndex];

    for (i = 0; i < (int)RX_BUFFER_SIZE; i++) {
        pBuf[i] = 0;
    }

    ((WPADStatus*)pBuf)->button =
        (pRpt[RPT_BTN0] | (u16)((u16)(pRpt[RPT_BTN1] << 8) & 0xFF00)) & 0x9F1F;

    if (p->dataFormat <= WPAD_FMT_CORE_BTN_ACC) {
        ((WPADStatus*)pBuf)->err = WPAD_ERR_OK;
    } else {
        ((WPADStatus*)pBuf)->err = WPAD_ERR_INVALID;
    }

    ((WPADStatus*)pBuf)->dev = p->devType;

    p->wpInfo.nearempty = (pRpt[RPT_BTN0] & 0x80) != 0;

    // The button byte carries the low two bits of each accelerometer sample.
    ((WPADStatus*)pBuf)->accX =
        (s16)((s16)((s16)(pRpt[RPT_ACC_OFFSET + 2] << 2) & ~3) |
              ((pRpt[RPT_ACC_OFFSET] >> 5) & 3)) -
        p->devConfig.accX0g;
    ((WPADStatus*)pBuf)->accY =
        (s16)((s16)((s16)(pRpt[RPT_ACC_OFFSET + 3] << 2) & ~3) |
              (s16)((pRpt[RPT_ACC_OFFSET + 1] >> 4) & 2)) -
        p->devConfig.accY0g;
    ((WPADStatus*)pBuf)->accZ =
        (s16)((s16)((s16)(pRpt[RPT_ACC_OFFSET + 4] << 2) & ~3) |
              (s16)((pRpt[RPT_ACC_OFFSET + 1] >> 5) & 2)) -
        p->devConfig.accZ0g;

    enabled = OSDisableInterrupts();

    p->rxBufIndex = p->rxBufIndex == 0;

    OSRestoreInterrupts(enabled);
}

void WPADiParseCoreBtnExt(u8 chan, u8* pRpt) {
    WPADCB* p;
    WPADStatus* pStatus;
    WPADFSStatus* pFS;
    WPADCLStatus* pCL;
    u8* pClear;
    BOOL enabled;
    int i;
    s16 dx;
    s16 dy;

    enabled = OSDisableInterrupts();

    p = _wpdcb[chan];
    pStatus = (WPADStatus*)p->rxBufs[p->rxBufIndex];
    pClear = (u8*)pStatus;
    for (i = 0; i < (int)RX_BUFFER_SIZE; i++) {
        pClear[i] = 0;
    }

    pStatus->button =
        (pRpt[RPT_BTN0] | (u16)((u16)(pRpt[RPT_BTN1] << 8) & 0xFF00)) & 0x9F1F;

    if (p->dataFormat == WPAD_FMT_CORE_BTN ||
        p->dataFormat == WPAD_FMT_FS_BTN ||
        p->dataFormat == WPAD_FMT_CLASSIC_BTN) {
        pStatus->err = WPAD_ERR_OK;
    } else {
        pStatus->err = WPAD_ERR_INVALID;
    }
    pStatus->dev = p->devType;
    p->wpInfo.nearempty = (pRpt[RPT_BTN0] & 0x80) != 0;

    memcpy(checkBuffer, pRpt + RPT32_EXT_OFFSET, RPT32_EXT_LENGTH);
    WPADiDecode(chan, pRpt + RPT32_EXT_OFFSET, RPT32_EXT_LENGTH, 0);

    if (p->wpInfo.attach) {
        if (p->devType == WPAD_DEV_FREESTYLE) {
            pFS = (WPADFSStatus*)p->rxBufs[p->rxBufIndex];
            pFS->fsStickX = (s8)pRpt[RPT32_EXT_OFFSET];
            pFS->fsStickY = (s8)pRpt[RPT32_EXT_OFFSET + 1];
            pFS->fsAccX =
                (s16)((s16)((s16)(pRpt[RPT32_EXT_OFFSET + 2] << 2) & ~3) |
                      ((s16)(pRpt[RPT32_EXT_OFFSET + 5] >> 2) & 3)) -
                p->extConfig.u.fs.accX0g;
            pFS->fsAccY =
                (s16)((s16)((s16)(pRpt[RPT32_EXT_OFFSET + 3] << 2) & ~3) |
                      ((s16)(pRpt[RPT32_EXT_OFFSET + 5] >> 4) & 3)) -
                p->extConfig.u.fs.accY0g;
            pFS->fsAccZ =
                (s16)((s16)((s16)(pRpt[RPT32_EXT_OFFSET + 4] << 2) & ~3) |
                      (s16)(pRpt[RPT32_EXT_OFFSET + 5] >> 6)) -
                p->extConfig.u.fs.accZ0g;
            pFS->button |=
                (~pRpt[RPT32_EXT_OFFSET + 5] << 13) & 0x6000;

            if (!p->calibrated) {
                p->calibrated = 1;
                p->extConfig.u.fs.stickXCenter = pFS->fsStickX;
                p->extConfig.u.fs.stickYCenter = pFS->fsStickY;
            }

            dx = (u8)pFS->fsStickX - (u8)p->extConfig.u.fs.stickXCenter;
            dy = (u8)pFS->fsStickY - (u8)p->extConfig.u.fs.stickYCenter;

            if (dx < -0x80) {
                pFS->fsStickX = -0x80;
            } else if (dx > 0x7F) {
                pFS->fsStickX = 0x7F;
            } else {
                pFS->fsStickX = dx;
            }
            if (dy < -0x80) {
                pFS->fsStickY = -0x80;
            } else if (dy > 0x7F) {
                pFS->fsStickY = 0x7F;
            } else {
                pFS->fsStickY = dy;
            }
        } else if (p->devType == WPAD_DEV_CLASSIC) {
            switch (p->devMode) {
            case 1:
                pCL = (WPADCLStatus*)p->rxBufs[p->rxBufIndex];
                pCL->clLStickX =
                    (s16)((pRpt[RPT32_EXT_OFFSET] << 4) & 0x3F0) - 0x200;
                pCL->clLStickY =
                    (s16)((pRpt[RPT32_EXT_OFFSET + 1] << 4) & 0x3F0) - 0x200;
                pCL->clRStickX =
                    (s16)((s16)((((s16)(pRpt[RPT32_EXT_OFFSET] >> 3) & 0x18) | (s16)((s16)(pRpt[RPT32_EXT_OFFSET + 1] >> 5) & 6)) | (s16)(pRpt[RPT32_EXT_OFFSET + 2] >> 7)) << 5) -
                    0x200;
                pCL->clRStickY =
                    (s16)((pRpt[RPT32_EXT_OFFSET + 2] << 5) & 0x3E0) - 0x200;
                pCL->clTriggerL =
                    (u8)(((((s16)(pRpt[RPT32_EXT_OFFSET + 2] >> 2) & 0x18) | (pRpt[RPT32_EXT_OFFSET + 3] >> 5)) << 3) & 0xF8);
                pCL->clTriggerR =
                    (u8)((pRpt[RPT32_EXT_OFFSET + 3] << 3) & 0xF8);
                pCL->clButton =
                    (u16)(((pRpt[RPT32_EXT_OFFSET + 4] << 8) |
                           pRpt[RPT32_EXT_OFFSET + 5])) ^ 0xFFFF;
                break;
            case 2:
                pCL = (WPADCLStatus*)p->rxBufs[p->rxBufIndex];
                pCL->clLStickX =
                    (s16)((s16)((s16)(pRpt[RPT32_EXT_OFFSET] << 2) & ~3) |
                          (pRpt[RPT32_EXT_OFFSET + 4] & 3)) -
                    0x200;
                pCL->clRStickX =
                    (s16)((s16)((s16)(pRpt[RPT32_EXT_OFFSET + 1] << 2) & ~3) |
                          ((s16)(pRpt[RPT32_EXT_OFFSET + 4] >> 2) & 3)) -
                    0x200;
                pCL->clLStickY =
                    (s16)((s16)((s16)(pRpt[RPT32_EXT_OFFSET + 2] << 2) & ~3) |
                          ((s16)(pRpt[RPT32_EXT_OFFSET + 4] >> 4) & 3)) -
                    0x200;
                pCL->clRStickY =
                    (s16)((s16)((s16)(pRpt[RPT32_EXT_OFFSET + 3] << 2) & ~3) |
                          (s16)(pRpt[RPT32_EXT_OFFSET + 4] >> 6)) -
                    0x200;
                pCL->clTriggerL = pRpt[RPT32_EXT_OFFSET + 5];
                pCL->clTriggerR = pRpt[RPT32_EXT_OFFSET + 6];
                pCL->clButton =
                    (u16)(((pRpt[RPT32_EXT_OFFSET + 7] << 8) |
                           pRpt[RPT32_EXT_OFFSET + 8])) ^ 0xFFFF;
                break;
            case 3:
                pCL = (WPADCLStatus*)p->rxBufs[p->rxBufIndex];
                pCL->clLStickX = (s16)((s16)(s32)pRpt[RPT32_EXT_OFFSET] << 2) - 0x200;
                pCL->clRStickX =
                    (s16)((s16)(s32)pRpt[RPT32_EXT_OFFSET + 1] << 2) - 0x200;
                pCL->clLStickY =
                    (s16)((s16)(s32)pRpt[RPT32_EXT_OFFSET + 2] << 2) - 0x200;
                pCL->clRStickY =
                    (s16)((s16)(s32)pRpt[RPT32_EXT_OFFSET + 3] << 2) - 0x200;
                pCL->clTriggerL = pRpt[RPT32_EXT_OFFSET + 4];
                pCL->clTriggerR = pRpt[RPT32_EXT_OFFSET + 5];
                pCL->clButton =
                    (u16)(((pRpt[RPT32_EXT_OFFSET + 6] << 8) |
                           pRpt[RPT32_EXT_OFFSET + 7])) ^ 0xFFFF;
                break;
            }

            if (!p->calibrated) {
                p->calibrated = 1;
                p->extConfig.u.cl.lStickXCenter = pCL->clLStickX;
                p->extConfig.u.cl.lStickYCenter = pCL->clLStickY;
                p->extConfig.u.cl.rStickXCenter = pCL->clRStickX;
                p->extConfig.u.cl.rStickYCenter = pCL->clRStickY;
                p->extConfig.u.cl.triggerLZero = pCL->clTriggerL;
                p->extConfig.u.cl.triggerRZero = pCL->clTriggerR;
            }

            dx = pCL->clLStickX - p->extConfig.u.cl.lStickXCenter;
            dy = pCL->clLStickY - p->extConfig.u.cl.lStickYCenter;
            if (dx < -0x200) {
                pCL->clLStickX = -0x200;
            } else if (dx > 0x1FF) {
                pCL->clLStickX = 0x1FF;
            } else {
                pCL->clLStickX = dx;
            }
            if (dy < -0x200) {
                pCL->clLStickY = -0x200;
            } else if (dy > 0x1FF) {
                pCL->clLStickY = 0x1FF;
            } else {
                pCL->clLStickY = dy;
            }

            dx = pCL->clRStickX - p->extConfig.u.cl.rStickXCenter;
            dy = pCL->clRStickY - p->extConfig.u.cl.rStickYCenter;
            if (dx < -0x200) {
                pCL->clRStickX = -0x200;
            } else if (dx > 0x1FF) {
                pCL->clRStickX = 0x1FF;
            } else {
                pCL->clRStickX = dx;
            }
            if (dy < -0x200) {
                pCL->clRStickY = -0x200;
            } else if (dy > 0x1FF) {
                pCL->clRStickY = 0x1FF;
            } else {
                pCL->clRStickY = dy;
            }

            dx = pCL->clTriggerL - p->extConfig.u.cl.triggerLZero;
            dy = pCL->clTriggerR - p->extConfig.u.cl.triggerRZero;
            if (dx < 0) {
                pCL->clTriggerL = 0;
            } else {
                pCL->clTriggerL = dx;
            }
            if (dy < 0) {
                pCL->clTriggerR = 0;
            } else {
                pCL->clTriggerR = dy;
            }

            if (sExtFlag[chan] != 0) {
                pCL->clRStickX = 0;
                pCL->clRStickY = 0;
                pCL->clTriggerL = 0;
                pCL->clTriggerR = 0;
            }
        }

        if (memcmp(checkBuffer, sNoExtData, RPT32_EXT_LENGTH) == 0 &&
            pStatus->err == WPAD_ERR_OK) {
            pStatus->err = WPAD_ERR_CORRUPTED;
        }
    }

    p->rxBufIndex = p->rxBufIndex == 0;
    OSRestoreInterrupts(enabled);
}

void WPADiParseCoreBtnAccExt(u8 chan, u8* pRpt) {
    WPADCB* p;
    u8* pBuf;
    u8* pDst;
    BOOL enabled;
    int i;

    p = _wpdcb[chan];
    pBuf = p->rxBufs[p->rxBufIndex];

    pDst = pBuf;
    for (i = 0; i < (int)RX_BUFFER_SIZE; i++) {
        pDst[i] = 0;
    }

    ((WPADStatus*)pBuf)->button =
        (pRpt[RPT_BTN0] | (u16)((u16)(pRpt[RPT_BTN1] << 8) & 0xFF00)) & 0x9F1F;
    if (p->dataFormat <= WPAD_FMT_CORE_BTN_ACC_DPD) {
        ((WPADStatus*)pBuf)->err = WPAD_ERR_OK;
    } else {
        ((WPADStatus*)pBuf)->err = WPAD_ERR_INVALID;
    }
    ((WPADStatus*)pBuf)->dev = p->devType;
    p->wpInfo.nearempty = (pRpt[RPT_BTN0] & 0x80) != 0;

    ((WPADStatus*)pBuf)->accX =
        (s16)((s16)((s16)(pRpt[RPT_ACC_OFFSET + 2] << 2) & ~3) |
              ((pRpt[RPT_ACC_OFFSET] >> 5) & 3)) -
        p->devConfig.accX0g;
    ((WPADStatus*)pBuf)->accY =
        (s16)((s16)((s16)(pRpt[RPT_ACC_OFFSET + 3] << 2) & ~3) |
              (s16)((pRpt[RPT_ACC_OFFSET + 1] >> 4) & 2)) -
        p->devConfig.accY0g;
    ((WPADStatus*)pBuf)->accZ =
        (s16)((s16)((s16)(pRpt[RPT_ACC_OFFSET + 4] << 2) & ~3) |
              (s16)((pRpt[RPT_ACC_OFFSET + 1] >> 5) & 2)) -
        p->devConfig.accZ0g;

    __parse_dpd_data(chan, (WPADStatus**)&pBuf, p->currentDpdCommand,
                     pRpt + RPT33_DPD_OFFSET, RPT33_DPD_LENGTH);

    enabled = OSDisableInterrupts();
    p->rxBufIndex = p->rxBufIndex == 0;
    OSRestoreInterrupts(enabled);
}

void WPADiParseFsBtn(u8 chan, u8* pRpt) {
    WPADCB* p;
    u8* pBuf;
    BOOL enabled;
    int i;
    u16 button;

    enabled = OSDisableInterrupts();

    p = _wpdcb[chan];
    pBuf = p->rxBufs[p->rxBufIndex];

    for (i = 0; i < (int)RX_BUFFER_SIZE; i++) {
        pBuf[i] = 0;
    }

    button = (pRpt[RPT_BTN0] | (u16)((u16)(pRpt[RPT_BTN1] << 8) & 0xFF00)) & 0x9F1F;
    ((WPADStatus*)pBuf)->button = button;
    ((WPADStatus*)pBuf)->err = WPAD_ERR_INVALID;
    ((WPADStatus*)pBuf)->dev = p->devType;
    p->wpInfo.nearempty = (pRpt[RPT_BTN0] & 0x80) != 0;

    memcpy(checkBuffer, pRpt + RPT34_EXT_OFFSET, RPT34_EXT_LENGTH);
    WPADiDecode(chan, pRpt + RPT34_EXT_OFFSET, RPT34_EXT_LENGTH, 0);

    if (p->wpInfo.attach &&
        memcmp(checkBuffer, sNoExtData, RPT34_EXT_LENGTH) == 0 &&
        ((WPADStatus*)pBuf)->err == WPAD_ERR_OK) {
        ((WPADStatus*)pBuf)->err = WPAD_ERR_CORRUPTED;
    }

    p->rxBufIndex = p->rxBufIndex == 0;

    OSRestoreInterrupts(enabled);
}

void WPADiParseFsBtnAcc(u8 chan, u8* pRpt) {
    WPADCB* p;
    WPADStatus* pStatus;
    WPADFSStatus* pFS;
    WPADCLStatus* pCL;
    u8* pClear;
    BOOL enabled;
    int i;
    s16 dx;
    s16 dy;

    p = _wpdcb[chan];
    pStatus = (WPADStatus*)p->rxBufs[p->rxBufIndex];
    pClear = (u8*)pStatus;
    for (i = 0; i < (int)RX_BUFFER_SIZE; i++) {
        pClear[i] = 0;
    }

    pStatus->button =
        (pRpt[RPT_BTN0] | (u16)((u16)(pRpt[RPT_BTN1] << 8) & 0xFF00)) & 0x9F1F;

    if (p->dataFormat == WPAD_FMT_CORE_BTN ||
        p->dataFormat == WPAD_FMT_CORE_BTN_ACC ||
        p->dataFormat == WPAD_FMT_FS_BTN ||
        p->dataFormat == WPAD_FMT_FS_BTN_ACC ||
        p->dataFormat == WPAD_FMT_CLASSIC_BTN ||
        p->dataFormat == WPAD_FMT_CLASSIC_BTN_ACC) {
        pStatus->err = WPAD_ERR_OK;
    } else {
        pStatus->err = WPAD_ERR_INVALID;
    }
    pStatus->dev = p->devType;
    p->wpInfo.nearempty = (pRpt[RPT_BTN0] & 0x80) != 0;

    pStatus->accX =
        (s16)((s16)((s16)(pRpt[RPT_ACC_OFFSET + 2] << 2) & ~3) |
              ((pRpt[RPT_ACC_OFFSET] >> 5) & 3)) -
        p->devConfig.accX0g;
    pStatus->accY =
        (s16)((s16)((s16)(pRpt[RPT_ACC_OFFSET + 3] << 2) & ~3) |
              (s16)((pRpt[RPT_ACC_OFFSET + 1] >> 4) & 2)) -
        p->devConfig.accY0g;
    pStatus->accZ =
        (s16)((s16)((s16)(pRpt[RPT_ACC_OFFSET + 4] << 2) & ~3) |
              (s16)((pRpt[RPT_ACC_OFFSET + 1] >> 5) & 2)) -
        p->devConfig.accZ0g;

    memcpy(checkBuffer, pRpt + RPT35_EXT_OFFSET, RPT35_EXT_LENGTH);
    WPADiDecode(chan, pRpt + RPT35_EXT_OFFSET, RPT35_EXT_LENGTH, 0);

    if (p->wpInfo.attach) {
        if (p->devType == WPAD_DEV_FREESTYLE) {
            pFS = (WPADFSStatus*)p->rxBufs[p->rxBufIndex];
            pFS->fsStickX = (s8)pRpt[RPT35_EXT_OFFSET];
            pFS->fsStickY = (s8)pRpt[RPT35_EXT_OFFSET + 1];
            pFS->fsAccX =
                (s16)((s16)((s16)(pRpt[RPT35_EXT_OFFSET + 2] << 2) & ~3) |
                      ((s16)(pRpt[RPT35_EXT_OFFSET + 5] >> 2) & 3)) -
                p->extConfig.u.fs.accX0g;
            pFS->fsAccY =
                (s16)((s16)((s16)(pRpt[RPT35_EXT_OFFSET + 3] << 2) & ~3) |
                      ((s16)(pRpt[RPT35_EXT_OFFSET + 5] >> 4) & 3)) -
                p->extConfig.u.fs.accY0g;
            pFS->fsAccZ =
                (s16)((s16)((s16)(pRpt[RPT35_EXT_OFFSET + 4] << 2) & ~3) |
                      (s16)(pRpt[RPT35_EXT_OFFSET + 5] >> 6)) -
                p->extConfig.u.fs.accZ0g;
            pFS->button |=
                (~pRpt[RPT35_EXT_OFFSET + 5] << 13) & 0x6000;

            if (!p->calibrated) {
                p->calibrated = 1;
                p->extConfig.u.fs.stickXCenter = pFS->fsStickX;
                p->extConfig.u.fs.stickYCenter = pFS->fsStickY;
            }

            dx = (u8)pFS->fsStickX - (u8)p->extConfig.u.fs.stickXCenter;
            dy = (u8)pFS->fsStickY - (u8)p->extConfig.u.fs.stickYCenter;

            if (dx < -0x80) {
                pFS->fsStickX = -0x80;
            } else if (dx > 0x7F) {
                pFS->fsStickX = 0x7F;
            } else {
                pFS->fsStickX = dx;
            }
            if (dy < -0x80) {
                pFS->fsStickY = -0x80;
            } else if (dy > 0x7F) {
                pFS->fsStickY = 0x7F;
            } else {
                pFS->fsStickY = dy;
            }
        } else if (p->devType == WPAD_DEV_CLASSIC) {
            switch (p->devMode) {
            case 1:
                pCL = (WPADCLStatus*)p->rxBufs[p->rxBufIndex];
                pCL->clLStickX =
                    (s16)((pRpt[RPT35_EXT_OFFSET] << 4) & 0x3F0) - 0x200;
                pCL->clLStickY =
                    (s16)((pRpt[RPT35_EXT_OFFSET + 1] << 4) & 0x3F0) - 0x200;
                pCL->clRStickX =
                    (s16)((s16)((((s16)(pRpt[RPT35_EXT_OFFSET] >> 3) & 0x18) | (s16)((s16)(pRpt[RPT35_EXT_OFFSET + 1] >> 5) & 6)) | (s16)(pRpt[RPT35_EXT_OFFSET + 2] >> 7)) << 5) -
                    0x200;
                pCL->clRStickY =
                    (s16)((pRpt[RPT35_EXT_OFFSET + 2] << 5) & 0x3E0) - 0x200;
                pCL->clTriggerL =
                    (u8)(((((s16)(pRpt[RPT35_EXT_OFFSET + 2] >> 2) & 0x18) | (pRpt[RPT35_EXT_OFFSET + 3] >> 5)) << 3) & 0xF8);
                pCL->clTriggerR =
                    (u8)((pRpt[RPT35_EXT_OFFSET + 3] << 3) & 0xF8);
                pCL->clButton =
                    (u16)(((pRpt[RPT35_EXT_OFFSET + 4] << 8) |
                           pRpt[RPT35_EXT_OFFSET + 5])) ^ 0xFFFF;
                break;
            case 2:
                pCL = (WPADCLStatus*)p->rxBufs[p->rxBufIndex];
                pCL->clLStickX =
                    (s16)((s16)((s16)(pRpt[RPT35_EXT_OFFSET] << 2) & ~3) |
                          (pRpt[RPT35_EXT_OFFSET + 4] & 3)) -
                    0x200;
                pCL->clRStickX =
                    (s16)((s16)((s16)(pRpt[RPT35_EXT_OFFSET + 1] << 2) & ~3) |
                          ((s16)(pRpt[RPT35_EXT_OFFSET + 4] >> 2) & 3)) -
                    0x200;
                pCL->clLStickY =
                    (s16)((s16)((s16)(pRpt[RPT35_EXT_OFFSET + 2] << 2) & ~3) |
                          ((s16)(pRpt[RPT35_EXT_OFFSET + 4] >> 4) & 3)) -
                    0x200;
                pCL->clRStickY =
                    (s16)((s16)((s16)(pRpt[RPT35_EXT_OFFSET + 3] << 2) & ~3) |
                          (s16)(pRpt[RPT35_EXT_OFFSET + 4] >> 6)) -
                    0x200;
                pCL->clTriggerL = pRpt[RPT35_EXT_OFFSET + 5];
                pCL->clTriggerR = pRpt[RPT35_EXT_OFFSET + 6];
                pCL->clButton =
                    (u16)(((pRpt[RPT35_EXT_OFFSET + 7] << 8) |
                           pRpt[RPT35_EXT_OFFSET + 8])) ^ 0xFFFF;
                break;
            case 3:
                pCL = (WPADCLStatus*)p->rxBufs[p->rxBufIndex];
                pCL->clLStickX = (s16)((s16)(s32)pRpt[RPT35_EXT_OFFSET] << 2) - 0x200;
                pCL->clRStickX =
                    (s16)((s16)(s32)pRpt[RPT35_EXT_OFFSET + 1] << 2) - 0x200;
                pCL->clLStickY =
                    (s16)((s16)(s32)pRpt[RPT35_EXT_OFFSET + 2] << 2) - 0x200;
                pCL->clRStickY =
                    (s16)((s16)(s32)pRpt[RPT35_EXT_OFFSET + 3] << 2) - 0x200;
                pCL->clTriggerL = pRpt[RPT35_EXT_OFFSET + 4];
                pCL->clTriggerR = pRpt[RPT35_EXT_OFFSET + 5];
                pCL->clButton =
                    (u16)(((pRpt[RPT35_EXT_OFFSET + 6] << 8) |
                           pRpt[RPT35_EXT_OFFSET + 7])) ^ 0xFFFF;
                break;
            }

            if (!p->calibrated) {
                p->calibrated = 1;
                p->extConfig.u.cl.lStickXCenter = pCL->clLStickX;
                p->extConfig.u.cl.lStickYCenter = pCL->clLStickY;
                p->extConfig.u.cl.rStickXCenter = pCL->clRStickX;
                p->extConfig.u.cl.rStickYCenter = pCL->clRStickY;
                p->extConfig.u.cl.triggerLZero = pCL->clTriggerL;
                p->extConfig.u.cl.triggerRZero = pCL->clTriggerR;
            }

            dx = pCL->clLStickX - p->extConfig.u.cl.lStickXCenter;
            dy = pCL->clLStickY - p->extConfig.u.cl.lStickYCenter;
            if (dx < -0x200) {
                pCL->clLStickX = -0x200;
            } else if (dx > 0x1FF) {
                pCL->clLStickX = 0x1FF;
            } else {
                pCL->clLStickX = dx;
            }
            if (dy < -0x200) {
                pCL->clLStickY = -0x200;
            } else if (dy > 0x1FF) {
                pCL->clLStickY = 0x1FF;
            } else {
                pCL->clLStickY = dy;
            }

            dx = pCL->clRStickX - p->extConfig.u.cl.rStickXCenter;
            dy = pCL->clRStickY - p->extConfig.u.cl.rStickYCenter;
            if (dx < -0x200) {
                pCL->clRStickX = -0x200;
            } else if (dx > 0x1FF) {
                pCL->clRStickX = 0x1FF;
            } else {
                pCL->clRStickX = dx;
            }
            if (dy < -0x200) {
                pCL->clRStickY = -0x200;
            } else if (dy > 0x1FF) {
                pCL->clRStickY = 0x1FF;
            } else {
                pCL->clRStickY = dy;
            }

            dx = pCL->clTriggerL - p->extConfig.u.cl.triggerLZero;
            dy = pCL->clTriggerR - p->extConfig.u.cl.triggerRZero;
            if (dx < 0) {
                pCL->clTriggerL = 0;
            } else {
                pCL->clTriggerL = dx;
            }
            if (dy < 0) {
                pCL->clTriggerR = 0;
            } else {
                pCL->clTriggerR = dy;
            }

            if (sExtFlag[chan] != 0) {
                pCL->clRStickX = 0;
                pCL->clRStickY = 0;
                pCL->clTriggerL = 0;
                pCL->clTriggerR = 0;
            }
        }

        if (memcmp(checkBuffer, sNoExtData, RPT35_EXT_LENGTH) == 0 &&
            pStatus->err == WPAD_ERR_OK) {
            pStatus->err = WPAD_ERR_CORRUPTED;
        }
    }

    enabled = OSDisableInterrupts();

    p->rxBufIndex = p->rxBufIndex == 0;

    OSRestoreInterrupts(enabled);
}
void WPADiParseClassicBtn(u8 chan, u8* pRpt) {
    WPADCB* p;
    u8* pBuf;
    u8* pDst;
    BOOL enabled;
    int i;

    enabled = OSDisableInterrupts();

    p = _wpdcb[chan];
    pBuf = p->rxBufs[p->rxBufIndex];

    pDst = pBuf;
    for (i = 0; i < (int)RX_BUFFER_SIZE; i++) {
        pDst[i] = 0;
    }

    ((WPADStatus*)pBuf)->button =
        (pRpt[RPT_BTN0] | (u16)((u16)(pRpt[RPT_BTN1] << 8) & 0xFF00)) & 0x9F1F;
    ((WPADStatus*)pBuf)->err = WPAD_ERR_INVALID;
    ((WPADStatus*)pBuf)->dev = p->devType;
    p->wpInfo.nearempty = (pRpt[RPT_BTN0] & 0x80) != 0;

    __parse_dpd_data(chan, (WPADStatus**)&pBuf, p->currentDpdCommand,
                     pRpt + RPT36_DPD_OFFSET, RPT36_DPD_LENGTH);

    memcpy(checkBuffer, pRpt + RPT36_EXT_OFFSET, RPT36_EXT_LENGTH);
    WPADiDecode(chan, pRpt + RPT36_EXT_OFFSET, RPT36_EXT_LENGTH, 0);

    if (p->wpInfo.attach &&
        memcmp(checkBuffer, sNoExtData, RPT36_EXT_LENGTH) == 0 &&
        ((WPADStatus*)pBuf)->err == WPAD_ERR_OK) {
        ((WPADStatus*)pBuf)->err = WPAD_ERR_CORRUPTED;
    }

    p->rxBufIndex = p->rxBufIndex == 0;

    OSRestoreInterrupts(enabled);
}

void WPADiParseClassicBtnAcc(u8 chan, u8* pRpt) {
    WPADCB* p;
    u8* pBuf;
    u8* pDst;
    BOOL enabled;
    int i;
    WPADFSStatus* pFS;
    WPADCLStatus* pCL;
    s16 dx;
    s16 dy;

    p = _wpdcb[chan];
    pBuf = p->rxBufs[p->rxBufIndex];
    pDst = pBuf;
    for (i = 0; i < (int)RX_BUFFER_SIZE; i++) {
        pDst[i] = 0;
    }

    ((WPADStatus*)pBuf)->button =
        (pRpt[RPT_BTN0] | (u16)((u16)(pRpt[RPT_BTN1] << 8) & 0xFF00)) & 0x9F1F;

    if (p->dataFormat <= WPAD_FMT_CLASSIC_BTN_ACC_DPD) {
        ((WPADStatus*)pBuf)->err = WPAD_ERR_OK;
    } else {
        ((WPADStatus*)pBuf)->err = WPAD_ERR_INVALID;
    }
    ((WPADStatus*)pBuf)->dev = p->devType;
    p->wpInfo.nearempty = (pRpt[RPT_BTN0] & 0x80) != 0;

    ((WPADStatus*)pBuf)->accX =
        (s16)((s16)((s16)(pRpt[RPT_ACC_OFFSET + 2] << 2) & ~3) |
              ((pRpt[RPT_ACC_OFFSET] >> 5) & 3)) -
        p->devConfig.accX0g;
    ((WPADStatus*)pBuf)->accY =
        (s16)((s16)((s16)(pRpt[RPT_ACC_OFFSET + 3] << 2) & ~3) |
              (s16)((pRpt[RPT_ACC_OFFSET + 1] >> 4) & 2)) -
        p->devConfig.accY0g;
    ((WPADStatus*)pBuf)->accZ =
        (s16)((s16)((s16)(pRpt[RPT_ACC_OFFSET + 4] << 2) & ~3) |
              (s16)((pRpt[RPT_ACC_OFFSET + 1] >> 5) & 2)) -
        p->devConfig.accZ0g;

    __parse_dpd_data(chan, (WPADStatus**)&pBuf, p->currentDpdCommand, pRpt + 6,
                     RPT37_DPD_LENGTH);

    memcpy(checkBuffer, pRpt + RPT37_EXT_OFFSET, RPT37_EXT_LENGTH);
    WPADiDecode(chan, pRpt + RPT37_EXT_OFFSET, RPT37_EXT_LENGTH, 0);

    if (p->wpInfo.attach) {
        if (p->devType == WPAD_DEV_FREESTYLE) {
            pFS = (WPADFSStatus*)p->rxBufs[p->rxBufIndex];
            pFS->fsStickX = (s8)pRpt[RPT37_EXT_OFFSET];
            pFS->fsStickY = (s8)pRpt[RPT37_EXT_OFFSET + 1];
            pFS->fsAccX =
                (s16)((s16)((s16)(pRpt[RPT37_EXT_OFFSET + 2] << 2) & ~3) |
                      ((s16)(pRpt[RPT37_EXT_OFFSET + 5] >> 2) & 3)) -
                p->extConfig.u.fs.accX0g;
            pFS->fsAccY =
                (s16)((s16)((s16)(pRpt[RPT37_EXT_OFFSET + 3] << 2) & ~3) |
                      ((s16)(pRpt[RPT37_EXT_OFFSET + 5] >> 4) & 3)) -
                p->extConfig.u.fs.accY0g;
            pFS->fsAccZ =
                (s16)((s16)((s16)(pRpt[RPT37_EXT_OFFSET + 4] << 2) & ~3) |
                      (s16)(pRpt[RPT37_EXT_OFFSET + 5] >> 6)) -
                p->extConfig.u.fs.accZ0g;
            pFS->button |=
                (~pRpt[RPT37_EXT_OFFSET + 5] << 13) & 0x6000;

            if (!p->calibrated) {
                p->calibrated = 1;
                p->extConfig.u.fs.stickXCenter = pFS->fsStickX;
                p->extConfig.u.fs.stickYCenter = pFS->fsStickY;
            }

            dx = (u8)pFS->fsStickX - (u8)p->extConfig.u.fs.stickXCenter;
            dy = (u8)pFS->fsStickY - (u8)p->extConfig.u.fs.stickYCenter;

            if (dx < -0x80) {
                pFS->fsStickX = -0x80;
            } else if (dx > 0x7F) {
                pFS->fsStickX = 0x7F;
            } else {
                pFS->fsStickX = dx;
            }
            if (dy < -0x80) {
                pFS->fsStickY = -0x80;
            } else if (dy > 0x7F) {
                pFS->fsStickY = 0x7F;
            } else {
                pFS->fsStickY = dy;
            }
        } else if (p->devType == WPAD_DEV_CLASSIC) {
            switch (p->devMode) {
            case 1:
                pCL = (WPADCLStatus*)p->rxBufs[p->rxBufIndex];
                pCL->clLStickX =
                    (s16)((pRpt[RPT37_EXT_OFFSET] << 4) & 0x3F0) - 0x200;
                pCL->clLStickY =
                    (s16)((pRpt[RPT37_EXT_OFFSET + 1] << 4) & 0x3F0) - 0x200;
                pCL->clRStickX =
                    (s16)((s16)((((s16)(pRpt[RPT37_EXT_OFFSET] >> 3) & 0x18) | (s16)((s16)(pRpt[RPT37_EXT_OFFSET + 1] >> 5) & 6)) | (s16)(pRpt[RPT37_EXT_OFFSET + 2] >> 7)) << 5) -
                    0x200;
                pCL->clRStickY =
                    (s16)((pRpt[RPT37_EXT_OFFSET + 2] << 5) & 0x3E0) - 0x200;
                pCL->clTriggerL =
                    (u8)(((((s16)(pRpt[RPT37_EXT_OFFSET + 2] >> 2) & 0x18) | (pRpt[RPT37_EXT_OFFSET + 3] >> 5)) << 3) & 0xF8);
                pCL->clTriggerR =
                    (u8)((pRpt[RPT37_EXT_OFFSET + 3] << 3) & 0xF8);
                pCL->clButton =
                    (u16)(((pRpt[RPT37_EXT_OFFSET + 4] << 8) |
                           pRpt[RPT37_EXT_OFFSET + 5])) ^ 0xFFFF;
                break;
            case 2:
                pCL = (WPADCLStatus*)p->rxBufs[p->rxBufIndex];
                pCL->clLStickX =
                    (s16)((s16)((s16)(pRpt[RPT37_EXT_OFFSET] << 2) & ~3) |
                          (pRpt[RPT37_EXT_OFFSET + 4] & 3)) -
                    0x200;
                pCL->clRStickX =
                    (s16)((s16)((s16)(pRpt[RPT37_EXT_OFFSET + 1] << 2) & ~3) |
                          ((s16)(pRpt[RPT37_EXT_OFFSET + 4] >> 2) & 3)) -
                    0x200;
                pCL->clLStickY =
                    (s16)((s16)((s16)(pRpt[RPT37_EXT_OFFSET + 2] << 2) & ~3) |
                          ((s16)(pRpt[RPT37_EXT_OFFSET + 4] >> 4) & 3)) -
                    0x200;
                pCL->clRStickY =
                    (s16)((s16)((s16)(pRpt[RPT37_EXT_OFFSET + 3] << 2) & ~3) |
                          (s16)(pRpt[RPT37_EXT_OFFSET + 4] >> 6)) -
                    0x200;
                pCL->clTriggerL = pRpt[RPT37_EXT_OFFSET + 5];
                // The report has no room for the rest of the mode 2 layout.
                pCL->clTriggerR = 0;
                pCL->clButton = 0;
                break;
            case 3:
                pCL = (WPADCLStatus*)p->rxBufs[p->rxBufIndex];
                pCL->clLStickX = (s16)((s16)(s32)pRpt[RPT37_EXT_OFFSET] << 2) - 0x200;
                pCL->clRStickX =
                    (s16)((s16)(s32)pRpt[RPT37_EXT_OFFSET + 1] << 2) - 0x200;
                pCL->clLStickY =
                    (s16)((s16)(s32)pRpt[RPT37_EXT_OFFSET + 2] << 2) - 0x200;
                pCL->clRStickY =
                    (s16)((s16)(s32)pRpt[RPT37_EXT_OFFSET + 3] << 2) - 0x200;
                pCL->clTriggerL = pRpt[RPT37_EXT_OFFSET + 4];
                pCL->clTriggerR = pRpt[RPT37_EXT_OFFSET + 5];
                pCL->clButton = 0;
                break;
            }

            if (!p->calibrated) {
                p->calibrated = 1;
                p->extConfig.u.cl.lStickXCenter = pCL->clLStickX;
                p->extConfig.u.cl.lStickYCenter = pCL->clLStickY;
                p->extConfig.u.cl.rStickXCenter = pCL->clRStickX;
                p->extConfig.u.cl.rStickYCenter = pCL->clRStickY;
                p->extConfig.u.cl.triggerLZero = pCL->clTriggerL;
                p->extConfig.u.cl.triggerRZero = pCL->clTriggerR;
            }

            dx = pCL->clLStickX - p->extConfig.u.cl.lStickXCenter;
            dy = pCL->clLStickY - p->extConfig.u.cl.lStickYCenter;
            if (dx < -0x200) {
                pCL->clLStickX = -0x200;
            } else if (dx > 0x1FF) {
                pCL->clLStickX = 0x1FF;
            } else {
                pCL->clLStickX = dx;
            }
            if (dy < -0x200) {
                pCL->clLStickY = -0x200;
            } else if (dy > 0x1FF) {
                pCL->clLStickY = 0x1FF;
            } else {
                pCL->clLStickY = dy;
            }

            dx = pCL->clRStickX - p->extConfig.u.cl.rStickXCenter;
            dy = pCL->clRStickY - p->extConfig.u.cl.rStickYCenter;
            if (dx < -0x200) {
                pCL->clRStickX = -0x200;
            } else if (dx > 0x1FF) {
                pCL->clRStickX = 0x1FF;
            } else {
                pCL->clRStickX = dx;
            }
            if (dy < -0x200) {
                pCL->clRStickY = -0x200;
            } else if (dy > 0x1FF) {
                pCL->clRStickY = 0x1FF;
            } else {
                pCL->clRStickY = dy;
            }

            dx = pCL->clTriggerL - p->extConfig.u.cl.triggerLZero;
            dy = pCL->clTriggerR - p->extConfig.u.cl.triggerRZero;
            if (dx < 0) {
                pCL->clTriggerL = 0;
            } else {
                pCL->clTriggerL = dx;
            }
            if (dy < 0) {
                pCL->clTriggerR = 0;
            } else {
                pCL->clTriggerR = dy;
            }

            if (sExtFlag[chan] != 0) {
                pCL->clRStickX = 0;
                pCL->clRStickY = 0;
                pCL->clTriggerL = 0;
                pCL->clTriggerR = 0;
            }
        }

        if (memcmp(checkBuffer, sNoExtData, RPT37_EXT_LENGTH) == 0 &&
            ((WPADStatus*)pBuf)->err == WPAD_ERR_OK) {
            ((WPADStatus*)pBuf)->err = WPAD_ERR_CORRUPTED;
        }
    }

    enabled = OSDisableInterrupts();

    p->rxBufIndex = p->rxBufIndex == 0;

    OSRestoreInterrupts(enabled);
}
void WPADiParseNunchukBtn(u8 chan, u8* pRpt) {}
void WPADiParseNunchukAccX(u8 chan, u8* pRpt) {
    WPADCB* p;
    WPADStatusEx* pStatus;
    u8* pClear;
    BOOL enabled;
    int i;

    p = _wpdcb[chan];
    pStatus = (WPADStatusEx*)p->rxBufs[p->rxBufIndex];

    if (sHidTmpA[chan] == 0 && sHidTmpB[chan] == 0) {
        pClear = (u8*)(pStatus);
        for (i = 0; i < (int)RX_BUFFER_SIZE; i++) {
            pClear[i] = 0;
        }
    }

    pStatus->button =
        (u16)((u16)(((u16)((u16)(pRpt[1]) & 0xFF) | (u16)((u16)(pRpt[2] << 8) & 0xFF00))) & (u16)0x9F1F);

    if (p->dataFormat == WPAD_FMT_CORE_BTN || p->dataFormat == WPAD_FMT_CORE_BTN_ACC || p->dataFormat == WPAD_FMT_BTN_ACC_DPD_EXTENDED) {
        pStatus->err = WPAD_ERR_OK;
    } else {
        pStatus->err = WPAD_ERR_INVALID;
    }
    pStatus->dev = p->devType;
    p->wpInfo.nearempty = 0;
    pStatus->accX = (s16)((s16)((s16)((s16)((s16)((s16)(pRpt[3]) << 2) & (s16)0xFFFC) |
                                       (s16)((s16)((u16)(pRpt[1] >> 6)) & (s16)0x0002))) -
                           (s16)(p->devConfig.accX0g));
    pStatus->accZ |= (s16)((
        s16)((s16)((s16)((s16)((s16)(pRpt[2]) << 3) & (s16)0xFF00) | (s16)((s16)((u16)(pRpt[1] << 1)) & (s16)0x00C0))));

    pStatus->obj[0].x = (s16)((s16)((s16)(((u16)(pRpt[RPT3E_DPD0_OFFSET])) & 0xFF) | (u16)(((u16)(pRpt[RPT3E_DPD0_OFFSET + 2] & 0x30)) << 4)));
    pStatus->obj[0].y =
        (s16)(0x2FF - (s16)((s16)(((u16)(pRpt[RPT3E_DPD0_OFFSET + 1])) & 0xFF) | (u16)(((u16)(pRpt[RPT3E_DPD0_OFFSET + 2] & 0xC0)) << 2)));
    pStatus->exp[0].pixel = (u16)(((s16)((s16)(((u16)(pRpt[RPT3E_DPD0_OFFSET + 7]) << 8) & 0xFF00) | (u16)(((u16)(pRpt[RPT3E_DPD0_OFFSET + 8] & 0xFF))))) * 64);
    pStatus->exp[0].radius = (s8)((pRpt[RPT3E_DPD0_OFFSET + 2] & 0x0f));
    pStatus->exp[0].range_x1 = (s16)(((s8)(pRpt[RPT3E_DPD0_OFFSET + 3]) == -1) ? 0 : pRpt[RPT3E_DPD0_OFFSET + 3]);
    pStatus->exp[0].range_y1 = (s16)(((s8)(pRpt[RPT3E_DPD0_OFFSET + 4]) == -1) ? 0 : pRpt[RPT3E_DPD0_OFFSET + 4]);
    pStatus->exp[0].range_x2 = (s16)(((s8)(pRpt[RPT3E_DPD0_OFFSET + 5]) == -1) ? 0 : pRpt[RPT3E_DPD0_OFFSET + 5]);
    pStatus->exp[0].range_y2 = (s16)(((s8)(pRpt[RPT3E_DPD0_OFFSET + 6]) == -1) ? 0 : pRpt[RPT3E_DPD0_OFFSET + 6]);
    pStatus->exp[0].range_x1 = (s16)(pStatus->exp[0].range_x1 * 8);
    pStatus->exp[0].range_y1 = (s16)((s16)0x2FF - (s16)(pStatus->exp[0].range_y1 * 8));
    pStatus->exp[0].range_x2 = (s16)(pStatus->exp[0].range_x2 * 8);
    pStatus->exp[0].range_y2 = (s16)((s16)0x2FF - (s16)(pStatus->exp[0].range_y2 * 8));
    pStatus->obj[0].size = (u16)((f32)pStatus->exp[0].radius * (f32)pStatus->exp[0].radius * 3.141592f);
    if (pStatus->obj[0].size == 0 || pStatus->obj[0].x == (s16)0x3FF ||
        pStatus->obj[0].y == (s16)0x2FF || pStatus->exp[0].radius == 0xf) {
        pStatus->obj[0].x = (s16)0;
        pStatus->obj[0].y = (s16)0x2FF;
        pStatus->obj[0].size = 0;
        pStatus->exp[0].pixel = 0;
        pStatus->exp[0].radius = 0;
    }
    pStatus->obj[0].traceId = 0;

    pStatus->obj[1].x = (s16)((s16)((s16)(((u16)(pRpt[RPT3E_DPD1_OFFSET])) & 0xFF) | (u16)(((u16)(pRpt[RPT3E_DPD1_OFFSET + 2] & 0x30)) << 4)));
    pStatus->obj[1].y =
        (s16)(0x2FF - (s16)((s16)(((u16)(pRpt[RPT3E_DPD1_OFFSET + 1])) & 0xFF) | (u16)(((u16)(pRpt[RPT3E_DPD1_OFFSET + 2] & 0xC0)) << 2)));
    pStatus->exp[1].pixel = (u16)(((s16)((s16)(((u16)(pRpt[RPT3E_DPD1_OFFSET + 7]) << 8) & 0xFF00) | (u16)(((u16)(pRpt[RPT3E_DPD1_OFFSET + 8] & 0xFF))))) * 64);
    pStatus->exp[1].radius = (s8)((pRpt[RPT3E_DPD1_OFFSET + 2] & 0x0f));
    pStatus->exp[1].range_x1 = (s16)(((s8)(pRpt[RPT3E_DPD1_OFFSET + 3]) == -1) ? 0 : pRpt[RPT3E_DPD1_OFFSET + 3]);
    pStatus->exp[1].range_y1 = (s16)(((s8)(pRpt[RPT3E_DPD1_OFFSET + 4]) == -1) ? 0 : pRpt[RPT3E_DPD1_OFFSET + 4]);
    pStatus->exp[1].range_x2 = (s16)(((s8)(pRpt[RPT3E_DPD1_OFFSET + 5]) == -1) ? 0 : pRpt[RPT3E_DPD1_OFFSET + 5]);
    pStatus->exp[1].range_y2 = (s16)(((s8)(pRpt[RPT3E_DPD1_OFFSET + 6]) == -1) ? 0 : pRpt[RPT3E_DPD1_OFFSET + 6]);
    pStatus->exp[1].range_x1 = (s16)(pStatus->exp[1].range_x1 * 8);
    pStatus->exp[1].range_y1 = (s16)((s16)0x2FF - (s16)(pStatus->exp[1].range_y1 * 8));
    pStatus->exp[1].range_x2 = (s16)(pStatus->exp[1].range_x2 * 8);
    pStatus->exp[1].range_y2 = (s16)((s16)0x2FF - (s16)(pStatus->exp[1].range_y2 * 8));
    pStatus->obj[1].size = (u16)((f32)pStatus->exp[1].radius * (f32)pStatus->exp[1].radius * 3.141592f);
    if (pStatus->obj[1].size == 0 || pStatus->obj[1].x == (s16)0x3FF ||
        pStatus->obj[1].y == (s16)0x2FF || pStatus->exp[1].radius == 0xf) {
        pStatus->obj[1].x = (s16)0;
        pStatus->obj[1].y = (s16)0x2FF;
        pStatus->obj[1].size = 0;
        pStatus->exp[1].pixel = 0;
        pStatus->exp[1].radius = 0;
    }
    pStatus->obj[1].traceId = 1;

    enabled = OSDisableInterrupts();
    sHidTmpA[chan] = 1;
    if (sHidTmpA[chan] && sHidTmpB[chan]) {
        pStatus->accZ -= (s16)(p->devConfig.accZ0g);
        p->rxBufIndex = (u8)((p->rxBufIndex) ? 0 : 1);
        sHidTmpA[chan] = sHidTmpB[chan] = 0;
    }
    OSRestoreInterrupts(enabled);
}
void WPADiParseNunchukAccY(u8 chan, u8* pRpt) {
    WPADCB* p;
    WPADStatusEx* pStatus;
    u8* pClear;
    BOOL enabled;
    int i;

    p = _wpdcb[chan];
    pStatus = (WPADStatusEx*)p->rxBufs[p->rxBufIndex];

    if (sHidTmpA[chan] == 0 && sHidTmpB[chan] == 0) {
        pClear = (u8*)(pStatus);
        for (i = 0; i < (int)RX_BUFFER_SIZE; i++) {
            pClear[i] = 0;
        }
    }

    pStatus->button =
        (u16)((u16)(((u16)((u16)(pRpt[1]) & 0xFF) | (u16)((u16)(pRpt[2] << 8) & 0xFF00))) & (u16)0x9F1F);

    if (p->dataFormat == WPAD_FMT_CORE_BTN || p->dataFormat == WPAD_FMT_CORE_BTN_ACC || p->dataFormat == WPAD_FMT_BTN_ACC_DPD_EXTENDED) {
        pStatus->err = WPAD_ERR_OK;
    } else {
        pStatus->err = WPAD_ERR_INVALID;
    }
    pStatus->dev = p->devType;
    p->wpInfo.nearempty = 0;
    pStatus->accY = (s16)((s16)((s16)((s16)((s16)((s16)(pRpt[3]) << 2) & (s16)0xFFFC) |
                                       (s16)((s16)((u16)(pRpt[1] >> 6)) & (s16)0x0002))) -
                           (s16)(p->devConfig.accY0g));
    pStatus->accZ |= (s16)((
        s16)((s16)((s16)((s16)((s16)(pRpt[2]) >> 1) & (s16)0x0030) | (s16)((s16)((u16)(pRpt[1] >> 3)) & (s16)0x000C))));

    pStatus->obj[2].x = (s16)((s16)((s16)(((u16)(pRpt[RPT3F_DPD2_OFFSET])) & 0xFF) | (u16)(((u16)(pRpt[RPT3F_DPD2_OFFSET + 2] & 0x30)) << 4)));
    pStatus->obj[2].y =
        (s16)(0x2FF - (s16)((s16)(((u16)(pRpt[RPT3F_DPD2_OFFSET + 1])) & 0xFF) | (u16)(((u16)(pRpt[RPT3F_DPD2_OFFSET + 2] & 0xC0)) << 2)));
    pStatus->exp[2].pixel = (u16)(((s16)((s16)(((u16)(pRpt[RPT3F_DPD2_OFFSET + 7]) << 8) & 0xFF00) | (u16)(((u16)(pRpt[RPT3F_DPD2_OFFSET + 8] & 0xFF))))) * 64);
    pStatus->exp[2].radius = (s8)((pRpt[RPT3F_DPD2_OFFSET + 2] & 0x0f));
    pStatus->exp[2].range_x1 = (s16)(((s8)(pRpt[RPT3F_DPD2_OFFSET + 3]) == -1) ? 0 : pRpt[RPT3F_DPD2_OFFSET + 3]);
    pStatus->exp[2].range_y1 = (s16)(((s8)(pRpt[RPT3F_DPD2_OFFSET + 4]) == -1) ? 0 : pRpt[RPT3F_DPD2_OFFSET + 4]);
    pStatus->exp[2].range_x2 = (s16)(((s8)(pRpt[RPT3F_DPD2_OFFSET + 5]) == -1) ? 0 : pRpt[RPT3F_DPD2_OFFSET + 5]);
    pStatus->exp[2].range_y2 = (s16)(((s8)(pRpt[RPT3F_DPD2_OFFSET + 6]) == -1) ? 0 : pRpt[RPT3F_DPD2_OFFSET + 6]);
    pStatus->exp[2].range_x1 = (s16)(pStatus->exp[2].range_x1 * 8);
    pStatus->exp[2].range_y1 = (s16)((s16)0x2FF - (s16)(pStatus->exp[2].range_y1 * 8));
    pStatus->exp[2].range_x2 = (s16)(pStatus->exp[2].range_x2 * 8);
    pStatus->exp[2].range_y2 = (s16)((s16)0x2FF - (s16)(pStatus->exp[2].range_y2 * 8));
    pStatus->obj[2].size = (u16)((f32)pStatus->exp[2].radius * (f32)pStatus->exp[2].radius * 3.141592f);
    if (pStatus->obj[2].size == 0 || pStatus->obj[2].x == (s16)0x3FF ||
        pStatus->obj[2].y == (s16)0x2FF || pStatus->exp[2].radius == 0xf) {
        pStatus->obj[2].x = (s16)0;
        pStatus->obj[2].y = (s16)0x2FF;
        pStatus->obj[2].size = 0;
        pStatus->exp[2].pixel = 0;
        pStatus->exp[2].radius = 0;
    }
    pStatus->obj[2].traceId = 2;

    pStatus->obj[3].x = (s16)((s16)((s16)(((u16)(pRpt[RPT3F_DPD3_OFFSET])) & 0xFF) | (u16)(((u16)(pRpt[RPT3F_DPD3_OFFSET + 2] & 0x30)) << 4)));
    pStatus->obj[3].y =
        (s16)(0x2FF - (s16)((s16)(((u16)(pRpt[RPT3F_DPD3_OFFSET + 1])) & 0xFF) | (u16)(((u16)(pRpt[RPT3F_DPD3_OFFSET + 2] & 0xC0)) << 2)));
    pStatus->exp[3].pixel = (u16)(((s16)((s16)(((u16)(pRpt[RPT3F_DPD3_OFFSET + 7]) << 8) & 0xFF00) | (u16)(((u16)(pRpt[RPT3F_DPD3_OFFSET + 8] & 0xFF))))) * 64);
    pStatus->exp[3].radius = (s8)((pRpt[RPT3F_DPD3_OFFSET + 2] & 0x0f));
    pStatus->exp[3].range_x1 = (s16)(((s8)(pRpt[RPT3F_DPD3_OFFSET + 3]) == -1) ? 0 : pRpt[RPT3F_DPD3_OFFSET + 3]);
    pStatus->exp[3].range_y1 = (s16)(((s8)(pRpt[RPT3F_DPD3_OFFSET + 4]) == -1) ? 0 : pRpt[RPT3F_DPD3_OFFSET + 4]);
    pStatus->exp[3].range_x2 = (s16)(((s8)(pRpt[RPT3F_DPD3_OFFSET + 5]) == -1) ? 0 : pRpt[RPT3F_DPD3_OFFSET + 5]);
    pStatus->exp[3].range_y2 = (s16)(((s8)(pRpt[RPT3F_DPD3_OFFSET + 6]) == -1) ? 0 : pRpt[RPT3F_DPD3_OFFSET + 6]);
    pStatus->exp[3].range_x1 = (s16)(pStatus->exp[3].range_x1 * 8);
    pStatus->exp[3].range_y1 = (s16)((s16)0x2FF - (s16)(pStatus->exp[3].range_y1 * 8));
    pStatus->exp[3].range_x2 = (s16)(pStatus->exp[3].range_x2 * 8);
    pStatus->exp[3].range_y2 = (s16)((s16)0x2FF - (s16)(pStatus->exp[3].range_y2 * 8));
    pStatus->obj[3].size = (u16)((f32)pStatus->exp[3].radius * (f32)pStatus->exp[3].radius * 3.141592f);
    if (pStatus->obj[3].size == 0 || pStatus->obj[3].x == (s16)0x3FF ||
        pStatus->obj[3].y == (s16)0x2FF || pStatus->exp[3].radius == 0xf) {
        pStatus->obj[3].x = (s16)0;
        pStatus->obj[3].y = (s16)0x2FF;
        pStatus->obj[3].size = 0;
        pStatus->exp[3].pixel = 0;
        pStatus->exp[3].radius = 0;
    }

    pStatus->obj[3].traceId = 3;
    enabled = OSDisableInterrupts();
    sHidTmpB[chan] = 1;
    if (sHidTmpA[chan] && sHidTmpB[chan]) {
        pStatus->accZ -= (s16)(p->devConfig.accZ0g);
        p->rxBufIndex = (u8)((p->rxBufIndex) ? 0 : 1);
        sHidTmpA[chan] = sHidTmpB[chan] = 0;
    }
    OSRestoreInterrupts(enabled);
}
void WPADiParseUnsupportedReport(u8 chan, u8* pRpt) {}
