#include <revolution/NAND.h>
#include <revolution/OS.h>
#include <cstdio>

static void nandOpenCallback(s32 result, void* arg);
static void nandCloseCallback(s32 result, void* arg);
static s32 nandSafeOpen(const char* path, NANDFileInfo* info, u8 access,
                        void* buffer, u32 bufferSize, BOOL priv,
                        BOOL simple);
static s32 nandSafeClose(NANDFileInfo* info, BOOL simple);

static u32 nandGetUniqueNumber(void) {
    static u32 s_counter = 0;

    BOOL enabled = OSDisableInterrupts();
    u32 num = s_counter++;
    OSRestoreInterrupts(enabled);

    return num;
}

static s32 nandOpen(const char* path, u8 mode, NANDCommandBlock* block,
                    BOOL async, BOOL priv) {
    IPCOpenMode ipcMode;
    char absPath[64];

    MEMCLR(&absPath);
    ipcMode = IPC_OPEN_NONE;
    nandGenerateAbsPath(absPath, path);

    if (!priv && nandIsPrivatePath(absPath)) {
        return IPC_RESULT_ACCESS;
    }

    switch (mode) {
    case NAND_ACCESS_RW:
        ipcMode = IPC_OPEN_RW;
        break;
    case NAND_ACCESS_READ:
        ipcMode = IPC_OPEN_READ;
        break;
    case NAND_ACCESS_WRITE:
        ipcMode = IPC_OPEN_WRITE;
        break;
    }

    if (async) {
        return ISFS_OpenAsync(absPath, ipcMode, nandOpenCallback, block);
    } else {
        return ISFS_Open(absPath, ipcMode);
    }
}

s32 NANDOpen(const char* path, NANDFileInfo* info, u8 mode) {
    s32 result;

    if (!nandIsInitialized()) {
        return NAND_RESULT_FATAL_ERROR;
    }

    result = nandOpen(path, mode, NULL, FALSE, FALSE);
    if (result >= 0) {
        info->fd = result;
        info->mark = 1;
        return NAND_RESULT_OK;
    }

    return nandConvertErrorCode(result);
}

s32 NANDPrivateOpen(const char* path, NANDFileInfo* info, u8 mode) {
    s32 result;

    if (!nandIsInitialized()) {
        return NAND_RESULT_FATAL_ERROR;
    }

    result = nandOpen(path, mode, NULL, FALSE, TRUE);
    if (result >= 0) {
        info->fd = result;
        info->mark = 1;
        return NAND_RESULT_OK;
    }

    return nandConvertErrorCode(result);
}

s32 NANDOpenAsync(const char* path, NANDFileInfo* info, u8 mode,
                  NANDAsyncCallback callback, NANDCommandBlock* block) {
    if (!nandIsInitialized()) {
        return NAND_RESULT_FATAL_ERROR;
    }

    block->callback = callback;
    block->info = info;
    return nandConvertErrorCode(nandOpen(path, mode, block, TRUE, FALSE));
}

s32 NANDPrivateOpenAsync(const char* path, NANDFileInfo* info, u8 mode,
                         NANDAsyncCallback callback, NANDCommandBlock* block) {
    if (!nandIsInitialized()) {
        return NAND_RESULT_FATAL_ERROR;
    }

    block->callback = callback;
    block->info = info;
    return nandConvertErrorCode(nandOpen(path, mode, block, TRUE, TRUE));
}

static void nandOpenCallback(s32 result, void* arg) {
    NANDCommandBlock* block = (NANDCommandBlock*)arg;

    if (result >= 0) {
        block->info->fd = result;
        block->info->stage = 2;
        block->info->mark = 1;
        block->callback(NAND_RESULT_OK, block);
    } else {
        block->callback(nandConvertErrorCode(result), block);
    }
}

s32 NANDClose(NANDFileInfo* info) {
    s32 result;

    if (!nandIsInitialized()) {
        return NAND_RESULT_FATAL_ERROR;
    }

    if (info->mark != 1) {
        return NAND_RESULT_INVALID;
    }

    result = ISFS_Close(info->fd);
    if (result == IPC_RESULT_OK) {
        info->mark = 2;
    }

    return nandConvertErrorCode(result);
}

s32 NANDCloseAsync(NANDFileInfo* info, NANDAsyncCallback callback,
                   NANDCommandBlock* block) {
    if (!nandIsInitialized()) {
        return NAND_RESULT_FATAL_ERROR;
    }

    if (info->mark != 1) {
        return NAND_RESULT_INVALID;
    }

    block->callback = callback;
    block->info = info;
    return nandConvertErrorCode(
        ISFS_CloseAsync(info->fd, nandCloseCallback, block));
}

s32 NANDSafeOpen(const char* path, NANDFileInfo* info, u8 access,
                 void* buffer, u32 bufferSize) {
    return nandSafeOpen(path, info, access, buffer, bufferSize, FALSE, FALSE);
}

// Copies the whole file at srcFd into dstFd through the caller's buffer
static s32 nandCopy(s32 dstFd, s32 srcFd, void* buffer, u32 bufferSize) {
    s32 readResult;
    s32 writeResult;

    while (TRUE) {
        readResult = ISFS_Read(srcFd, buffer, bufferSize);
        if (readResult == 0) {
            return IPC_RESULT_OK;
        }

        if (readResult < 0) {
            return readResult;
        }

        writeResult = ISFS_Write(dstFd, buffer, readResult);
        if (writeResult < 0) {
            return writeResult;
        }
    }
}

static s32 nandSafeOpen(const char* path, NANDFileInfo* info, u8 access,
                        void* buffer, u32 bufferSize, BOOL priv,
                        BOOL simple) {
    if (!nandIsInitialized()) {
        return NAND_RESULT_FATAL_ERROR;
    }

    // Simple mode needs a copy buffer of whole NAND clusters
    if (simple && (bufferSize & (NAND_CLUSTER_SIZE - 1)) != 0) {
        return NAND_RESULT_INVALID;
    }

    info->access = access;
    info->stage = 0;
    nandGenerateAbsPath(info->openPath, path);

    if (!priv && nandIsPrivatePath(info->openPath)) {
        return NAND_RESULT_ACCESS;
    }

    if (access == NAND_ACCESS_READ) {
        s32 fd = ISFS_Open(info->openPath, IPC_OPEN_READ);
        if (fd >= 0) {
            info->fd = fd;
            info->stage = 2;

            if (!simple) {
                info->mark = 3;
            } else {
                info->mark = 5;
            }

            return NAND_RESULT_OK;
        } else {
            return nandConvertErrorCode(fd);
        }
    } else if (access == NAND_ACCESS_WRITE || access == NAND_ACCESS_RW) {
        char tempDir[64];
        char relativeName[13] = {0};
        s32 result;
        u32 ownerId;
        u16 groupId;
        u32 attr;
        u32 ownerPerm;
        u32 groupPerm;
        u32 otherPerm;
        u32 uniqueNo = -1;

        result = ISFS_CreateDir("/tmp/sys", 0, IPC_OPEN_RW, IPC_OPEN_RW,
                                IPC_OPEN_RW);
        if (result != IPC_RESULT_OK && result != IPC_RESULT_EXISTS) {
            return nandConvertErrorCode(result);
        }

        info->stage = 1;
        result = ISFS_GetAttr(info->openPath, &ownerId, &groupId, &attr,
                              &ownerPerm, &groupPerm, &otherPerm);
        if (result != IPC_RESULT_OK) {
            return nandConvertErrorCode(result);
        }

        info->tempFd = ISFS_Open(info->openPath, IPC_OPEN_READ);
        if (info->tempFd < 0) {
            return nandConvertErrorCode(info->tempFd);
        }

        info->stage = 2;

        if (!simple) {
            uniqueNo = nandGetUniqueNumber();
            sprintf(tempDir, "%s/%08x", "/tmp/sys", uniqueNo);
            result = ISFS_CreateDir(tempDir, 0, IPC_OPEN_RW, IPC_OPEN_NONE,
                                    IPC_OPEN_NONE);
            if (result != IPC_RESULT_OK) {
                return nandConvertErrorCode(result);
            }

            info->stage = 3;
        }

        nandGetRelativeName(relativeName, info->openPath);

        if (!simple) {
            sprintf(info->tempPath, "%s/%08x/%s", "/tmp/sys", uniqueNo,
                    relativeName);
        } else {
            sprintf(info->tempPath, "%s/%s", "/tmp/sys", relativeName);
        }

        result = ISFS_CreateFile(info->tempPath, attr, ownerPerm, groupPerm,
                                 otherPerm);
        if (result != IPC_RESULT_OK) {
            return nandConvertErrorCode(result);
        }

        info->stage = 4;

        if (access == NAND_ACCESS_WRITE) {
            info->fd = ISFS_Open(info->tempPath, IPC_OPEN_WRITE);
        } else if (access == NAND_ACCESS_RW) {
            info->fd = ISFS_Open(info->tempPath, IPC_OPEN_RW);
        }

        if (info->fd < 0) {
            return nandConvertErrorCode(info->fd);
        }

        info->stage = 5;

        result = nandCopy(info->fd, info->tempFd, buffer, bufferSize);
        if (result != IPC_RESULT_OK) {
            return nandConvertErrorCode(result);
        }

        result = ISFS_Seek(info->fd, 0, IPC_SEEK_BEG);
        if (result != IPC_RESULT_OK) {
            return nandConvertErrorCode(result);
        }

        if (simple) {
            info->mark = 5;
        } else {
            info->mark = 3;
        }

        return nandConvertErrorCode(IPC_RESULT_OK);
    } else {
        return NAND_RESULT_INVALID;
    }
}

s32 NANDSafeClose(NANDFileInfo* info) {
    return nandSafeClose(info, FALSE);
}

static s32 nandSafeClose(NANDFileInfo* info, BOOL simple) {
    s32 result;
    char tempDir[64];

    MEMCLR(&tempDir);

    if (!nandIsInitialized()) {
        return NAND_RESULT_FATAL_ERROR;
    }

    if (!((info->mark == 3 && !simple) || (info->mark == 5 && simple))) {
        return NAND_RESULT_INVALID;
    }

    if (info->access == NAND_ACCESS_READ) {
        result = ISFS_Close(info->fd);
        if (result == IPC_RESULT_OK) {
            info->stage = 7;

            if (!simple) {
                info->mark = 4;
            } else {
                info->mark = 6;
            }
        }

        return nandConvertErrorCode(result);
    } else if (info->access == NAND_ACCESS_WRITE ||
               info->access == NAND_ACCESS_RW) {
        result = ISFS_Close(info->fd);
        if (result != IPC_RESULT_OK) {
            return nandConvertErrorCode(result);
        }

        info->stage = 6;
        result = ISFS_Close(info->tempFd);
        if (result != IPC_RESULT_OK) {
            return nandConvertErrorCode(result);
        }

        info->stage = 7;
        result = ISFS_Rename(info->tempPath, info->openPath);
        if (result != IPC_RESULT_OK) {
            return nandConvertErrorCode(result);
        }

        info->stage = 8;

        if (!simple) {
            nandGetParentDirectory(tempDir, info->tempPath);
            result = ISFS_Delete(tempDir);
            if (result == IPC_RESULT_OK) {
                info->stage = 9;
                info->mark = 4;
            }
        } else {
            info->mark = 6;
        }

        return nandConvertErrorCode(result);
    } else {
        OSReport("Illegal NANDFileInfo.\n");
    }

    return NAND_RESULT_FATAL_ERROR;
}

static void nandCloseCallback(s32 result, void* arg) {
    NANDCommandBlock* block = (NANDCommandBlock*)arg;

    if (result == IPC_RESULT_OK) {
        block->info->stage = 7;
        block->info->mark = 2;
    }

    block->callback(nandConvertErrorCode(result), block);
}
