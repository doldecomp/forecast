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

// settings.sav contents (0x20 bytes at wwwSettingsBuf)
struct WWWSettings {
    u32 magic;     // at 0x0
    u8 unk4;       // at 0x4
    u8 flag5;      // at 0x5
    u8 unk6;       // at 0x6
    u8 unk7;       // at 0x7
    u32 magic2;    // at 0x8
    u8 unkC;       // at 0xC
    u8 unkD[0x13]; // at 0xD
};

// {path, buffer, size} entry, lists end with a null path
struct WWWNandFile {
    char* path; // at 0x0
    void* buf;  // at 0x4
    u32 size;   // at 0x8
};

struct WWWFont {
    const char* name; // at 0x0
    u32 unk4;         // at 0x4
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
    u8 unk0[0x1C];           // at 0x0
    u32 bssSize;             // at 0x1C
    u8 unk20[4];             // at 0x20
    void (*prolog)(void);    // at 0x24
    void (*epilog)(void);    // at 0x28
    u8 unk2C[4];             // at 0x2C
    u32 unk30;               // at 0x30
    u8 unk34[4];             // at 0x34
    u32 unk38;               // at 0x38
    u8 unk3C[0x10];          // at 0x3C
    u32 unk4C;               // at 0x4C
    u8 unk50[4];             // at 0x50
    u32 unk54;               // at 0x54
};

struct WWWImport {
    const char* name; // at 0x0
    void** slot;      // at 0x4
};

extern "C" {
BOOL fn_800832A0(void** out, u32 size, MEMAllocator* primary, MEMAllocator* fallback);
void fn_80083350(void** ptr);
void fn_80084B74(void* image);
void* fn_8004545C(WWWModule* module, const char* name);
void fn_80045A70(WWWModule* module, void* bss);
u32 fn_80045A78(void* header, int which);
}

extern WWWFont wwwFontList[];       // 0x8028B980
extern WWWSettings wwwSettingsBuf;  // 0x8028B9C0
extern WWWNandFile wwwFileList[];   // 0x8028B9E0
extern NANDFileInfo wwwNandFileInfo; // 0x8028B9EC
extern WWWNandFile wwwSettingsFile; // 0x801B6960
extern WWWImport wwwImportTable[0x70]; // 0x801B65E0
extern u8 lbl_801B56E0[];
extern const char* wwwFontPaths[2]; // 0x803303F8
extern const char* wwwFontNames[1]; // 0x80330400
extern u8 wwwFirstInit;            // 0x80330404
extern u8 wwwStarted;              // 0x80330E57
extern u8 wwwLibLoaded;            // 0x80330E58
extern MEMAllocator* wwwMem1Allocator; // 0x80330E60
extern MEMAllocator* wwwMem2Allocator; // 0x80330E64
extern u8 wwwAspectRatio;          // 0x80331090
extern u8* wwwAspectPtr;           // 0x80331094
extern WWWModule* wwwModule;       // 0x80331098
extern void* wwwModuleBss;         // 0x8033109C
extern void (*pWWWSurfaceAddFont)(const char* name);

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
        if (!fn_800832A0(&file->buf, file->size, wwwMem2Allocator, wwwMem1Allocator)) {
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

    if (wwwSettingsBuf.magic == 0x35465768 && wwwSettingsBuf.unkC >= 1) {
        return;
    }
    wwwAspectRatio = SCGetAspectRatio();
    if (VIGetDTVStatus()) {
        wwwAspectPtr = &wwwSettingsBuf.unk7;
    } else {
        wwwAspectPtr = &wwwSettingsBuf.unk6;
    }
    ret = wwwReadNandFile(&wwwSettingsFile);
    DCInvalidateRange(&wwwSettingsBuf, sizeof(wwwSettingsBuf));
    if (ret != -12) {
        if (ret < 0) {
            return;
        }
        if (wwwSettingsBuf.magic == 0x35465768 && wwwSettingsBuf.magic2 == 0x35465768 &&
            wwwSettingsBuf.unkC >= 1) {
            return;
        }
    }
    wwwSettingsBuf.unk6 = 1;
    wwwSettingsBuf.unk7 = 1;
    wwwSettingsBuf.flag5 = 0;
    wwwSettingsBuf.unkC = 1;
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
    if (!fn_800832A0(&packed, (contentGetLengthNAND(&file) + 31) & ~31, wwwMem2Allocator, wwwMem1Allocator)) {
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
    size = fn_80045A78(header, 2);
    if (!fn_800832A0((void**)&wwwModule, (size + 31) & ~31, wwwMem1Allocator, wwwMem1Allocator)) {
        ret = -0xE11;
        goto fail;
    }
    lz.src = (u8*)packed;
    lz.outPos = 0;
    wwwLZRead((u8*)wwwModule, &lz, size);
    module = wwwModule;
    if (module->bssSize != 0) {
        if (!fn_800832A0(&wwwModuleBss, module->bssSize, wwwMem1Allocator, wwwMem2Allocator)) {
            ret = -0xE13;
            goto fail;
        }
        memset(wwwModuleBss, 0, module->bssSize);
    }
    restSize = 0x902420 - size;
    if (!fn_800832A0(&rest, (restSize + 31) & ~31, wwwMem2Allocator, wwwMem1Allocator)) {
        ret = -0xE14;
        goto fail;
    }
    wwwLZRead((u8*)rest, &lz, restSize);
    fn_80083350(&packed);
    delta = (u8*)rest - size - (u8*)module;
    module->unk30 += delta;
    module->unk38 += delta;
    module->unk4C += delta;
    module->unk54 += delta;
    fn_80045A70(module, wwwModuleBss);
    if (module->prolog != NULL) {
        module->prolog();
    }
    imp = wwwImportTable;
    for (i = 0; i < 0x70; i++, imp++) {
        *imp->slot = fn_8004545C(module, imp->name);
    }
    fn_80083350(&rest);
    contentReleaseHandleNAND(&handle);
    if (ret == 0) {
        wwwLibLoaded = 1;
        return 0;
    }
fail:
    fn_80083350(&rest);
    fn_80083350(&wwwModuleBss);
    fn_80083350((void**)&wwwModule);
    fn_80083350(&packed);
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
        fn_800832A0(&font->buf, (contentGetLengthNAND(&file) + 31) & ~31, wwwMem2Allocator, wwwMem1Allocator);
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

#pragma opt_common_subs off
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
    s32 fonts;

    wwwStarted = 0;
    if (wwwFirstInit) {
        wwwFirstInit = 0;
        fn_80084B74(lbl_801B56E0);
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
        fn_80083350(&wwwModuleBss);
    }
    fn_80083350((void**)&wwwModule);
    for (font = wwwFontList; font->name != NULL; font++) {
        fn_80083350(&font->buf);
    }
}
