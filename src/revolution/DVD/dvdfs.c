#include <revolution/DVD.h>
#include <revolution/OS.h>

#include <cctype>

// FST (file system table) entry
typedef struct FSTEntry {
    u32 isDirAndStringOff; // at 0x0
    u32 parentOrPosition;  // at 0x4
    u32 nextEntryOrLength; // at 0x8
} FSTEntry;

static OSBootInfo* BootInfo;
static FSTEntry* FstStart;
static char* FstStringStart;
static u32 MaxEntryNum;
static u32 currentDirectory = 0;

OSThreadQueue __DVDThreadQueue;
BOOL __DVDLongFileNameFlag = TRUE;

#define entryIsDir(i) (((FstStart[i].isDirAndStringOff & 0xFF000000) == 0) ? FALSE : TRUE)
#define stringOff(i) (FstStart[i].isDirAndStringOff & 0x00FFFFFF)
#define parentDir(i) (FstStart[i].parentOrPosition)
#define nextDir(i) (FstStart[i].nextEntryOrLength)
#define filePosition(i) (FstStart[i].parentOrPosition)
#define fileLength(i) (FstStart[i].nextEntryOrLength)

static void cbForReadAsync(s32 result, DVDCommandBlock* block);
static void cbForReadSync(s32 result, DVDCommandBlock* block);

void __DVDFSInit(void) {
    BootInfo = (OSBootInfo*)OSPhysicalToCached(OS_PHYS_BOOT_INFO);
    FstStart = (FSTEntry*)BootInfo->fstStart;

    if (FstStart != NULL) {
        // The root entry's length is the number of entries
        MaxEntryNum = FstStart[0].nextEntryOrLength;
        // String table is placed after the entries
        FstStringStart = (char*)&FstStart[MaxEntryNum];
    }
}

static BOOL isSame(const char* path, const char* string) {
    while (*string != '\0') {
        if (tolower(*path++) != tolower(*string++)) {
            return FALSE;
        }
    }

    if (*path == '/' || *path == '\0') {
        return TRUE;
    }

    return FALSE;
}

s32 DVDConvertPathToEntrynum(const char* path) {
    const char* ptr;
    char* stringPtr;
    BOOL isDir;
    u32 length;
    u32 dirLookAt;
    u32 i;
    const char* origPath = path;
    const char* extensionStart;
    BOOL illegal;
    BOOL extension;

    dirLookAt = currentDirectory;

    while (TRUE) {
        if (*path == '\0') {
            return dirLookAt;
        } else if (*path == '/') {
            dirLookAt = 0;
            path++;
            continue;
        } else if (*path == '.') {
            if (*(path + 1) == '.') {
                if (*(path + 2) == '/') {
                    dirLookAt = parentDir(dirLookAt);
                    path += 3;
                    continue;
                } else if (*(path + 2) == '\0') {
                    return parentDir(dirLookAt);
                }
            } else if (*(path + 1) == '/') {
                path += 2;
                continue;
            } else if (*(path + 1) == '\0') {
                return dirLookAt;
            }
        }

        if (__DVDLongFileNameFlag == FALSE) {
            extension = FALSE;
            illegal = FALSE;

            for (ptr = path; *ptr != '\0' && *ptr != '/'; ptr++) {
                if (*ptr == '.') {
                    // 8.3 format limits the file name to 8 characters
                    if (ptr - path > 8 || extension == TRUE) {
                        illegal = TRUE;
                        break;
                    }

                    extension = TRUE;
                    extensionStart = ptr + 1;
                } else if (*ptr == ' ') {
                    illegal = TRUE;
                }
            }

            if (extension == TRUE && ptr - extensionStart > 3) {
                illegal = TRUE;
            }

            // clang-format off
#line 443
            OS_ASSERT(!illegal, "DVDConvertEntrynumToPath(possibly DVDOpen or DVDChangeDir or DVDOpenDir): specified directory or file (%s) doesn't match standard 8.3 format. This is a temporary restriction and will be removed soon\n", origPath);
            // clang-format on
        } else {
            for (ptr = path; *ptr != '\0' && *ptr != '/'; ptr++) {
                ;
            }
        }

        // Does a '/' follow the name? (int literals: the TRUE/FALSE enum changes the register allocation)
        isDir = (*ptr == '\0') ? 0 : 1;
        length = ptr - path;

        ptr = path;

        for (i = dirLookAt + 1; i < nextDir(dirLookAt);
             i = entryIsDir(i) ? nextDir(i) : (i + 1)) {
            if (entryIsDir(i) == FALSE && isDir == TRUE) {
                continue;
            }

            stringPtr = FstStringStart + stringOff(i);

            if (isSame(ptr, stringPtr) == TRUE) {
                goto next_hier;
            }
        }

        return -1;

    next_hier:
        if (!isDir) {
            return i;
        }

        dirLookAt = i;
        path += length + 1;
    }
}

BOOL DVDEntrynumIsDir(s32 entrynum) {
    return entryIsDir(entrynum);
}

BOOL DVDFastOpen(s32 entrynum, DVDFileInfo* info) {
    if (entrynum < 0 || entrynum >= MaxEntryNum || entryIsDir(entrynum)) {
        return FALSE;
    }

    info->offset = filePosition(entrynum) >> __DVDLayoutFormat;
    info->size = fileLength(entrynum);
    info->callback = NULL;
    info->block.state = DVD_STATE_IDLE;

    return TRUE;
}

BOOL DVDOpen(const char* path, DVDFileInfo* info) {
    s32 entrynum;
    char dir[128];

    entrynum = DVDConvertPathToEntrynum(path);

    if (entrynum < 0) {
        DVDGetCurrentDir(dir, sizeof(dir));
        OSReport("Warning: DVDOpen(): file '%s' was not found under %s.\n",
                 path, dir);
        return FALSE;
    }

    if (entryIsDir(entrynum)) {
        return FALSE;
    }

    info->offset = filePosition(entrynum) >> __DVDLayoutFormat;
    info->size = fileLength(entrynum);
    info->callback = NULL;
    info->block.state = DVD_STATE_IDLE;

    return TRUE;
}

BOOL DVDClose(DVDFileInfo* info) {
    DVDCancel(&info->block);
    return TRUE;
}

static u32 myStrncpy(char* dst, const char* src, u32 maxlen) {
    u32 i = maxlen;

    while (i > 0 && *src != '\0') {
        *dst++ = *src++;
        i--;
    }

    return maxlen - i;
}

static u32 entryToPath(u32 entrynum, char* string, u32 maxlen) {
    char* name;
    u32 loc;

    if (entrynum == 0) {
        return 0;
    }

    name = FstStringStart + stringOff(entrynum);
    loc = entryToPath(parentDir(entrynum), string, maxlen);

    if (loc == maxlen) {
        return loc;
    }

    string[loc++] = '/';
    loc += myStrncpy(string + loc, name, maxlen - loc);
    return loc;
}

static BOOL DVDConvertEntrynumToPath(s32 entrynum, char* string, u32 maxlen) {
    u32 loc = entryToPath(entrynum, string, maxlen);

    if (loc == maxlen) {
        string[maxlen - 1] = '\0';
        return FALSE;
    }

    if (entryIsDir(entrynum)) {
        if (loc == maxlen - 1) {
            string[loc] = '\0';
            return FALSE;
        }

        string[loc++] = '/';
    }

    string[loc] = '\0';
    return TRUE;
}

// Only inlined copies remain in the DOL (DVDOpen, DVDOpenDir)
inline BOOL DVDGetCurrentDir(char* buffer, u32 maxlen) {
    return DVDConvertEntrynumToPath(currentDirectory, buffer, maxlen);
}

BOOL DVDReadAsyncPrio(DVDFileInfo* info, void* dst, s32 size, s32 offset,
                      DVDAsyncCallback callback, s32 prio) {
    // clang-format off
#line 823
    OS_ASSERT(offset >= 0 && offset <= info->size, "DVDReadAsync(): specified area is out of the file  ");
    // clang-format on

    // clang-format off
#line 829
    OS_ASSERT(offset + size >= 0 && offset + size < info->size + 32, "DVDReadAsync(): specified area is out of the file  ");
    // clang-format on

    info->callback = callback;

    DVDReadAbsAsyncPrio(&info->block, dst, size, info->offset + (offset >> 2),
                        cbForReadAsync, prio);

    return TRUE;
}

static void cbForReadAsync(s32 result, DVDCommandBlock* block) {
    DVDFileInfo* info = (DVDFileInfo*)block;

    if (info->callback != NULL) {
        info->callback(result, info);
    }
}

s32 DVDReadPrio(DVDFileInfo* info, void* dst, s32 size, s32 offset, s32 prio) {
    DVDCommandBlock* block;
    BOOL enabled;
    s32 ret;

    // clang-format off
#line 893
    OS_ASSERT(offset >= 0 && offset <= info->size, "DVDRead(): specified area is out of the file  ");
    // clang-format on

    // clang-format off
#line 899
    OS_ASSERT(offset + size >= 0 && offset + size < info->size + 32, "DVDRead(): specified area is out of the file  ");
    // clang-format on

    block = &info->block;
    if (!DVDReadAbsAsyncPrio(block, dst, size, info->offset + (offset >> 2),
                             cbForReadSync, prio)) {
        return DVD_RESULT_FATAL_ERROR;
    }

    enabled = OSDisableInterrupts();

    while (TRUE) {
        s32 state = block->state;

        if (state == DVD_STATE_IDLE) {
            ret = block->transferTotal;
            break;
        } else if (state == DVD_STATE_FATAL) {
            ret = DVD_RESULT_FATAL_ERROR;
            break;
        } else if (state == DVD_STATE_CANCELED) {
            ret = DVD_RESULT_CANCELED;
            break;
        }

        OSSleepThread(&__DVDThreadQueue);
    }

    OSRestoreInterrupts(enabled);
    return ret;
}

static void cbForReadSync(s32 result, DVDCommandBlock* block) {
#pragma unused(result)
#pragma unused(block)

    OSWakeupThread(&__DVDThreadQueue);
}

BOOL DVDOpenDir(const char* path, DVDDir* dir) {
    s32 entrynum;
    char currentDir[128];

    entrynum = DVDConvertPathToEntrynum(path);

    if (entrynum < 0) {
        DVDGetCurrentDir(currentDir, sizeof(currentDir));
        OSReport("Warning: DVDOpenDir(): file '%s' was not found under %s.\n",
                 path, currentDir);
        return FALSE;
    }

    if (!entryIsDir(entrynum)) {
        return FALSE;
    }

    dir->entryNum = entrynum;
    dir->location = entrynum + 1;
    dir->next = nextDir(entrynum);
    return TRUE;
}

BOOL DVDReadDir(DVDDir* dir, DVDDirEntry* entry) {
    u32 loc = dir->location;

    if (loc <= dir->entryNum || dir->next <= loc) {
        return FALSE;
    }

    entry->entryNum = loc;
    entry->isDir = entryIsDir(loc);
    entry->name = FstStringStart + stringOff(loc);
    dir->location = entryIsDir(loc) ? nextDir(loc) : loc + 1;
    return TRUE;
}

BOOL DVDCloseDir(DVDDir* dir) {
#pragma unused(dir)

    return TRUE;
}
