// Save data on NAND and the error dialog shown when connecting or saving fails
#include <channel/WeatherViews.h>
#include <channel/WeatherScene.h>
#include <channel/Fade.h>
#include <channel/LayoutButton.h>
#include <channel/SceneBase.h>
#include <channel/System.h>

#include <nw4r/ut.h>
#include <revolution/NAND.h>
#include <revolution/NET.h>
#include <revolution/OS.h>
#include <wstring.h>

extern nw4r::ut::ArchiveFont* gSysFont;

static const char sSavePath[] = "noerase/savedata.dat";
static const char sSaveDir[] = "noerase";
static const char sSaveMagic[] = "HAF0";

static SaveData* sSaveBuffer;
static u32 sSaveSize;

void SetSaveDataBuffer(SaveData* buf, u32 size) {
    sSaveBuffer = buf;
    sSaveSize = size;
}

s32 ReadSaveData() {
    NANDFileInfo info;
    u32* end;
    BOOL valid = FALSE;

    s32 result = NANDOpen(sSavePath, &info, NAND_ACCESS_READ);
    if (result == NAND_RESULT_NOEXISTS) {
        return 1;
    }

    if (result != NAND_RESULT_OK) {
        OSReport("NANDOpen() failed(%d).\n", result);
        return 2;
    }

    result = NANDRead(&info, sSaveBuffer, sSaveSize);
    if (result == NAND_RESULT_CORRUPT) {
        OSReport("NANDRead() failed(CORRUPT).\n");
    } else if (result != sSaveSize) {
        OSReport("NANDRead() failed(%d).\n", result);
    } else {
        end = (u32*)((u8*)sSaveBuffer + sSaveSize);
        if (end[-1] != NETCalcCRC32(sSaveBuffer, sSaveSize - 4)) {
            OSReport("NAND data broken.\n");
        } else {
            for (int i = 0; i < 4; i++) {
                if (sSaveMagic[i] != ((char*)sSaveBuffer)[i]) {
                    OSReport("NAND data invalid label.\n");
                    goto close;
                }
            }

            valid = TRUE;
        }
    }

close:
    result = NANDClose(&info);
    if (result == NAND_RESULT_CORRUPT) {
        OSReport("NANDClose() failed(CORRUPT).\n");
        return 3;
    }

    if (result != NAND_RESULT_OK) {
        OSReport("NANDClose() failed(%d).\n", result);
        return 2;
    }

    s32 ret = 2;
    if (valid) {
        ret = 0;
    }

    return ret;
}

s32 WriteSaveData() {
    NANDFileInfo info;
    u32* end;
    BOOL written = FALSE;

    s32 result = NANDCreateDir(sSaveDir, NAND_PERM_RUSR | NAND_PERM_WUSR | NAND_PERM_RGRP | NAND_PERM_WGRP | NAND_PERM_ROTH, 0);
    if (result == NAND_RESULT_CORRUPT) {
        OSReport("NANDCreateDir() failed(CORRUPT).\n");
        return 3;
    }

    if (result != NAND_RESULT_OK && result != NAND_RESULT_EXISTS) {
        OSReport("NANDCreateDir() failed(%d).\n", result);
        return 2;
    }

    result = NANDCreate(sSavePath, NAND_PERM_RUSR | NAND_PERM_WUSR | NAND_PERM_RGRP | NAND_PERM_WGRP | NAND_PERM_ROTH, 0);
    if (result == NAND_RESULT_CORRUPT) {
        OSReport("NANDCreate() failed(CORRUPT).\n");
        return 3;
    }

    if (result != NAND_RESULT_OK && result != NAND_RESULT_EXISTS) {
        OSReport("NANDCreate() failed(%d).\n", result);
        return 2;
    }

    result = NANDOpen(sSavePath, &info, NAND_ACCESS_WRITE);
    if (result != NAND_RESULT_OK) {
        OSReport("NANDOpen() failed(%d).\n", result);
        return 2;
    }

    ((char*)sSaveBuffer)[0] = 'H';
    ((char*)sSaveBuffer)[1] = 'A';
    ((char*)sSaveBuffer)[2] = 'F';
    ((char*)sSaveBuffer)[3] = '0';
    end = (u32*)((u8*)sSaveBuffer + sSaveSize);
    end[-1] = NETCalcCRC32(sSaveBuffer, sSaveSize - 4);

    result = NANDWrite(&info, sSaveBuffer, sSaveSize);
    if (result == NAND_RESULT_CORRUPT) {
        OSReport("NANDRead() failed(CORRUPT).\n", result);
    } else if (result != sSaveSize) {
        OSReport("NANDRead() failed(%d).\n", result);
    } else {
        written = TRUE;
    }

    result = NANDClose(&info);
    if (result == NAND_RESULT_CORRUPT) {
        OSReport("NANDClose() failed(CORRUPT).\n");
        return 3;
    }

    if (result != NAND_RESULT_OK) {
        OSReport("NANDClose() failed(%d).\n", result);
        return 2;
    }

    s32 ret = 2;
    if (written) {
        ret = 0;
    }

    return ret;
}

ErrorWindow::ErrorWindow(void* arc) {
    mFade = gFade;
    mYesNoLayout = new ButtonGroup(arc, "error3.brlyt", gButtonColors, false);
    mSaveLayout = new ButtonGroup(arc, "error4.brlyt", gButtonColors, false);
    mFatalLayout = new ButtonGroup(arc, "error2.brlyt", gButtonColors, false);
    Open(0);
}

ErrorWindow::~ErrorWindow() {
    delete mFatalLayout;
    delete mSaveLayout;
    delete mYesNoLayout;
}

void ErrorWindow::Open(s32 message) {
    mState = 0;
    mMessage = message;
    mYesNoLayout->Reset();
    mSaveLayout->Reset();
    mFatalLayout->Reset();
    LayoutButton* button = mYesNoLayout->FindButton("yes");
    button->mPressed = TRUE;
    button = mYesNoLayout->FindButton("no");
    button->mPressed = TRUE;
    button = mSaveLayout->FindButton("next");
    button->mPressed = TRUE;
    button = mFatalLayout->FindButton("next");
    button->mPressed = TRUE;
}

void ErrorWindow::Calc() {
    mYesNoLayout->Calc();
    mSaveLayout->Calc();
    mFatalLayout->Calc();

    switch (mState) {
    case 0:
        mFade->FadeIn(30);
        mState = 1;
        break;
    case 1:
        if (mFade->mFading == 0) {
            mState = 2;
        }
        break;
    case 2:
        switch (mMessage) {
        case 1:
            UpdateButtons(mYesNoLayout, 40);
            if (CheckButtonPressed("yes", WPAD_BUTTON_A) >= 0) {
                mFade->FadeOut(30);
                mState = 4;
                PlaySE(26);
            } else if (CheckButtonPressed("no", WPAD_BUTTON_A) >= 0) {
                mFade->FadeOut(30);
                mState = 3;
                PlaySE(27);
            }
            break;
        case 2:
        case 3:
            UpdateButtons(mSaveLayout, 40);
            if (CheckButtonPressed("next", WPAD_BUTTON_A) >= 0) {
                mFade->FadeOut(30);
                mState = 4;
                PlaySE(26);
            }
            break;
        case 4:
        case 5:
        case 6:
            UpdateButtons(mFatalLayout, 40);
            if (CheckButtonPressed("next", WPAD_BUTTON_A) >= 0) {
                PlaySE(26);
                gReturnToMenuRequested = TRUE;
            }
            break;
        }
        break;
    case 3:
        if (mFade->mFading == 0) {
            mFade->FadeIn(30);
            mState = 2;
            mMessage = 6;
        }
        break;
    case 4:
        if (mFade->mFading == 0) {
            mState = 5;
        }
        break;
    case 5:
        break;
    }
}

static inline f32 GetCenterX(const LayoutButton* button) {
    return (button->mRect.right + button->mRect.left) / 2.0f;
}

static inline f32 GetCenterY(const LayoutButton* button) {
    return (button->mRect.top + button->mRect.bottom) / 2.0f;
}

void ErrorWindow::Draw() {
    switch (mMessage) {
    case 1: {
        mYesNoLayout->Draw();
        LayoutButton* message = mYesNoLayout->FindButton("message");
        f32 x = GetCenterX(message) + 0.5f * GetScreenWidth();
        f32 y = -GetCenterY(message) + 0.5f * GetScreenHeight();

        nw4r::ut::TextWriterBase<wchar_t> writer;
        wchar_t buf[0x80];
        SetDefaultGXState();
        SetOrthoProjection();
        writer.SetFont(*gSysFont);
        writer.SetupGX();
        writer.SetTextColor(nw4r::ut::Color(255, 255, 255, 255));
        writer.SetCursor(x, y);
        writer.SetDrawFlag(0x111);
        FormatTime(buf, 0x80, OSGetTime(), gRegion, gLanguage);
        writer.Printf(buf);
        break;
    }
    case 2:
    case 3: {
        LayoutButton* text = mSaveLayout->FindButton("text");
        switch (mMessage) {
        case 2:
            text->SetState(0);
            break;
        case 3:
            text->SetState(1);
            break;
        }
        mSaveLayout->Draw();
        break;
    }
    case 4:
    case 5:
    case 6: {
        LayoutButton* text = mFatalLayout->FindButton("text");
        switch (mMessage) {
        case 4:
            text->SetState(1);
            break;
        case 5:
            text->SetState(2);
            break;
        case 6:
            text->SetState(0);
            break;
        }
        mFatalLayout->Draw();
        break;
    }
    }
}

void FormatTime(wchar_t* buf, size_t size, s64 time, s32 region, u8 language) {
    OSCalendarTime cal;
    OSTicksToCalendarTime(time, &cal);

    if (region == 1 && language == 1) {
        swprintf(buf, size, L"%02d/%02d/%04d %02d:%02d", cal.month + 1, cal.mday, cal.year, cal.hour, cal.min);
    } else if (region == 1 && language == 3) {
        swprintf(buf, size, L"%02d-%02d-%04d, %02d:%02d", cal.month + 1, cal.mday, cal.year, cal.hour, cal.min);
    } else {
        switch (language) {
        case 0:
            swprintf(buf, size, L"%04d/%02d/%02d %02d:%02d", cal.year, cal.month + 1, cal.mday, cal.hour, cal.min);
            break;
        case 1:
        default:
            swprintf(buf, size, L"%02d/%02d/%04d %02d:%02d", cal.mday, cal.month + 1, cal.year, cal.hour, cal.min);
            break;
        case 2:
            swprintf(buf, size, L"%02d.%02d.%04d - %02d:%02d", cal.mday, cal.month + 1, cal.year, cal.hour, cal.min);
            break;
        case 3:
            swprintf(buf, size, L"%02d/%02d/%04d, %02d:%02d", cal.mday, cal.month + 1, cal.year, cal.hour, cal.min);
            break;
        case 4:
            swprintf(buf, size, L"%02d-%02d-%04d %02d:%02d", cal.mday, cal.month + 1, cal.year, cal.hour, cal.min);
            break;
        case 5:
            swprintf(buf, size, L"%02d/%02d/%04d  %02d:%02d", cal.mday, cal.month + 1, cal.year, cal.hour, cal.min);
            break;
        case 6:
            swprintf(buf, size, L"%02d-%02d-%04d %02d:%02d", cal.mday, cal.month + 1, cal.year, cal.hour, cal.min);
            break;
        }
    }
}
