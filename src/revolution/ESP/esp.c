#include <revolution/ESP.h>
#include <revolution/ESP/esp.h>
#include <revolution/IPC.h>

#undef NULL
#define NULL ((void*)0)

// Size of the 32-byte aligned stack buffer that holds the ioctlv arguments.
#define ES_WORK_SIZE 256

// The ioctlv vectors live at the end of the work buffer, the arguments at
// 32-byte aligned offsets from its start.
#define ES_WORK_VECTORS(work)                                                  \
    ((IPCIOVector*)((work) + ES_WORK_SIZE - 6 * sizeof(IPCIOVector)))

#define ES_IS_ALIGNED(ptr) (((u32)(ptr) & 31) == 0)

static s32 __esFd = -1;

ESError ESP_InitLib(void) {
    ESError ret = ES_ERR_OK;

    if (__esFd < 0) {
        __esFd = IOS_Open("/dev/es", IPC_OPEN_NONE);

        if (__esFd < 0) {
            ret = __esFd;
        }
    }

    return ret;
}

ESError ESP_CloseLib(void) {
    ESError ret = ES_ERR_OK;

    if (__esFd >= 0) {
        ret = IOS_Close(__esFd);

        if (ret == ES_ERR_OK) {
            __esFd = -1;
        }
    }

    return ret;
}

ESError ESP_LaunchTitle(ESTitleId titleId, ESTicketView* ticketView) {
    u8 work[ES_WORK_SIZE] ALIGN(32);
    IPCIOVector* vec = ES_WORK_VECTORS(work);
    ESTitleId* titleIdWork = (ESTitleId*)work;
    ESError ret;

    if (__esFd < 0) {
        return ES_ERR_INVALID;
    }

    if (!ES_IS_ALIGNED(ticketView)) {
        return ES_ERR_INVALID;
    }

    *titleIdWork = titleId;
    vec[0].base = titleIdWork;
    vec[0].length = sizeof(ESTitleId);
    vec[1].base = ticketView;
    vec[1].length = sizeof(ESTicketView);

    ret = IOS_IoctlvReboot(__esFd, ES_IOCTLV_LAUNCH_TITLE, 2, 0, vec);

    // IOS reboots into the new title, which invalidates the handle.
    __esFd = -1;
    return ret;
}

ESFd ESP_OpenContentFile(ESContentId contentId) {
    u8 work[ES_WORK_SIZE] ALIGN(32);
    IPCIOVector* vec = ES_WORK_VECTORS(work);
    ESContentId* contentIdWork = (ESContentId*)work;

    if (__esFd < 0) {
        return ES_ERR_INVALID;
    }

    *contentIdWork = contentId;
    vec[0].base = contentIdWork;
    vec[0].length = sizeof(ESContentId);

    return IOS_Ioctlv(__esFd, ES_IOCTLV_OPEN_CONTENT_FILE, 1, 0, vec);
}

ESError ESP_ReadContentFile(ESFd fd, void* buffer, u32 size) {
    u8 work[ES_WORK_SIZE] ALIGN(32);
    IPCIOVector* vec = ES_WORK_VECTORS(work);
    ESFd* fdWork = (ESFd*)work;

    if (__esFd < 0 || fd < 0 || buffer == NULL || size == 0) {
        return ES_ERR_INVALID;
    }

    if (!ES_IS_ALIGNED(buffer)) {
        return ES_ERR_INVALID;
    }

    *fdWork = fd;
    vec[1].base = buffer;
    vec[1].length = size;
    vec[0].base = fdWork;
    vec[0].length = sizeof(ESFd);

    return IOS_Ioctlv(__esFd, ES_IOCTLV_READ_CONTENT_FILE, 1, 1, vec);
}

ESError ESP_SeekContentFile(ESFd fd, s32 offset, u32 whence) {
    u8 work[ES_WORK_SIZE] ALIGN(32);
    IPCIOVector* vec = ES_WORK_VECTORS(work);
    ESFd* fdWork = (ESFd*)work;
    s32* offsetWork = (s32*)(work + 32);
    u32* whenceWork = (u32*)(work + 64);

    if (__esFd < 0) {
        return ES_ERR_INVALID;
    }

    *fdWork = fd;
    *offsetWork = offset;
    *whenceWork = whence;
    vec[0].base = fdWork;
    vec[0].length = sizeof(ESFd);
    vec[1].base = offsetWork;
    vec[1].length = sizeof(s32);
    vec[2].base = whenceWork;
    vec[2].length = sizeof(u32);

    return IOS_Ioctlv(__esFd, ES_IOCTLV_SEEK_CONTENT_FILE, 3, 0, vec);
}

ESError ESP_CloseContentFile(ESFd fd) {
    u8 work[ES_WORK_SIZE] ALIGN(32);
    IPCIOVector* vec = ES_WORK_VECTORS(work);
    ESFd* fdWork = (ESFd*)work;

    if (__esFd < 0 || fd < 0) {
        return ES_ERR_INVALID;
    }

    *fdWork = fd;
    vec[0].base = fdWork;
    vec[0].length = sizeof(ESFd);

    return IOS_Ioctlv(__esFd, ES_IOCTLV_CLOSE_CONTENT_FILE, 1, 0, vec);
}

ESError ESP_ListTitleContentsOnCard(ESTitleId titleId, ESContentId* contentIds,
                                    u32* numContents) {
    ESError ret = ES_ERR_OK;
    u8 work[ES_WORK_SIZE] ALIGN(32);
    IPCIOVector* vec = ES_WORK_VECTORS(work);
    ESTitleId* titleIdWork = (ESTitleId*)work;
    u32* countWork = (u32*)(work + 32);

    if (__esFd < 0 || numContents == NULL) {
        ret = ES_ERR_INVALID;
        goto out;
    }

    if (!ES_IS_ALIGNED(contentIds)) {
        ret = ES_ERR_INVALID;
        goto out;
    }

    *titleIdWork = titleId;

    if (contentIds == NULL) {
        vec[0].base = titleIdWork;
        vec[0].length = sizeof(ESTitleId);
        vec[1].base = countWork;
        vec[1].length = sizeof(u32);
        ret = IOS_Ioctlv(__esFd, ES_IOCTLV_GET_NUM_TITLE_CONTENTS_ON_CARD, 1,
                         1, vec);

        if (ret == ES_ERR_OK) {
            *numContents = *countWork;
        }
    } else {
        if (*numContents == 0) {
            ret = ES_ERR_INVALID;
            goto out;
        }

        *countWork = *numContents;
        vec[0].base = titleIdWork;
        vec[0].length = sizeof(ESTitleId);
        vec[1].base = countWork;
        vec[1].length = sizeof(u32);
        vec[2].base = contentIds;
        vec[2].length = sizeof(ESContentId) * *numContents;
        ret = IOS_Ioctlv(__esFd, ES_IOCTLV_LIST_TITLE_CONTENTS_ON_CARD, 2, 1,
                         vec);
    }

out:
    return ret;
}

ESError ESP_GetTicketViews(ESTitleId titleId, ESTicketView* ticketViewList,
                           u32* numTicketViews) {
    ESError ret = ES_ERR_OK;
    u8 work[ES_WORK_SIZE] ALIGN(32);
    IPCIOVector* vec = ES_WORK_VECTORS(work);
    ESTitleId* titleIdWork = (ESTitleId*)work;
    u32* countWork = (u32*)(work + 32);

    if (__esFd < 0 || numTicketViews == NULL) {
        ret = ES_ERR_INVALID;
        goto out;
    }

    if (!ES_IS_ALIGNED(ticketViewList)) {
        ret = ES_ERR_INVALID;
        goto out;
    }

    *titleIdWork = titleId;

    if (ticketViewList == NULL) {
        vec[0].base = titleIdWork;
        vec[0].length = sizeof(ESTitleId);
        vec[1].base = countWork;
        vec[1].length = sizeof(u32);
        ret = IOS_Ioctlv(__esFd, ES_IOCTLV_GET_NUM_TICKET_VIEWS, 1, 1, vec);

        if (ret == ES_ERR_OK) {
            *numTicketViews = *countWork;
        }
    } else {
        if (*numTicketViews == 0) {
            ret = ES_ERR_INVALID;
            goto out;
        }

        *countWork = *numTicketViews;
        vec[0].base = titleIdWork;
        vec[0].length = sizeof(ESTitleId);
        vec[1].base = countWork;
        vec[1].length = sizeof(u32);
        vec[2].base = ticketViewList;
        vec[2].length = sizeof(ESTicketView) * *numTicketViews;
        ret = IOS_Ioctlv(__esFd, ES_IOCTLV_GET_TICKET_VIEWS, 2, 1, vec);
    }

out:
    return ret;
}

ESError ESP_DiGetTicketView(void* ticket, ESTicketView* ticketView) {
    ESError ret = ES_ERR_OK;
    u8 work[ES_WORK_SIZE] ALIGN(32);
    IPCIOVector* vec = ES_WORK_VECTORS(work);

    if (__esFd < 0 || ticketView == NULL) {
        ret = ES_ERR_INVALID;
        goto out;
    }

    if (!ES_IS_ALIGNED(ticket) || !ES_IS_ALIGNED(ticketView)) {
        ret = ES_ERR_INVALID;
        goto out;
    }

    // Without a ticket, ES uses the one of the disc's open partition.
    vec[0].base = ticket;

    if (ticket == NULL) {
        vec[0].length = 0;
    } else {
        vec[0].length = sizeof(ESTicket);
    }

    vec[1].base = ticketView;
    vec[1].length = sizeof(ESTicketView);
    ret = IOS_Ioctlv(__esFd, ES_IOCTLV_DI_GET_TICKET_VIEW, 1, 1, vec);

out:
    return ret;
}

ESError ESP_DiGetTmd(ESTitleMeta* tmd, u32* tmdSize) {
    ESError ret = ES_ERR_OK;
    u8 work[ES_WORK_SIZE] ALIGN(32);
    IPCIOVector* vec = ES_WORK_VECTORS(work);
    u32* sizeWork = (u32*)work;

    if (__esFd < 0 || tmdSize == NULL) {
        ret = ES_ERR_INVALID;
        goto out;
    }

    if (!ES_IS_ALIGNED(tmd)) {
        ret = ES_ERR_INVALID;
        goto out;
    }

    if (tmd == NULL) {
        vec[0].base = sizeWork;
        vec[0].length = sizeof(u32);
        ret = IOS_Ioctlv(__esFd, ES_IOCTLV_DI_GET_TMD_SIZE, 0, 1, vec);

        if (ret == ES_ERR_OK) {
            *tmdSize = *sizeWork;
        }
    } else {
        if (*tmdSize == 0) {
            ret = ES_ERR_INVALID;
            goto out;
        }

        *sizeWork = *tmdSize;
        vec[0].base = sizeWork;
        vec[0].length = sizeof(u32);
        vec[1].base = tmd;
        vec[1].length = *tmdSize;
        ret = IOS_Ioctlv(__esFd, ES_IOCTLV_DI_GET_TMD, 1, 1, vec);
    }

out:
    return ret;
}

ESError ESP_GetTmdView(ESTitleId titleId, ESTmdView* tmdView, u32* tmdSize) {
    ESError ret = ES_ERR_OK;
    u8 work[ES_WORK_SIZE] ALIGN(32);
    IPCIOVector* vec = ES_WORK_VECTORS(work);
    ESTitleId* titleIdWork = (ESTitleId*)work;
    u32* sizeWork = (u32*)(work + 32);

    if (__esFd < 0 || tmdSize == NULL) {
        ret = ES_ERR_INVALID;
        goto out;
    }

    if (!ES_IS_ALIGNED(tmdView)) {
        ret = ES_ERR_INVALID;
        goto out;
    }

    *titleIdWork = titleId;

    if (tmdView == NULL) {
        vec[0].base = titleIdWork;
        vec[0].length = sizeof(ESTitleId);
        vec[1].base = sizeWork;
        vec[1].length = sizeof(u32);
        ret = IOS_Ioctlv(__esFd, ES_IOCTLV_GET_TMD_VIEW_SIZE, 1, 1, vec);

        if (ret == ES_ERR_OK) {
            *tmdSize = *sizeWork;
        }
    } else {
        if (*tmdSize == 0) {
            ret = ES_ERR_INVALID;
            goto out;
        }

        *sizeWork = *tmdSize;
        vec[0].base = titleIdWork;
        vec[0].length = sizeof(ESTitleId);
        vec[1].base = sizeWork;
        vec[1].length = sizeof(u32);
        vec[2].base = tmdView;
        vec[2].length = *tmdSize;
        ret = IOS_Ioctlv(__esFd, ES_IOCTLV_GET_TMD_VIEW, 2, 1, vec);
    }

out:
    return ret;
}

ESError ESP_GetDataDir(ESTitleId titleId, char* dataDir) {
    ESError ret = ES_ERR_OK;
    u8 work[ES_WORK_SIZE] ALIGN(32);
    IPCIOVector* vec = ES_WORK_VECTORS(work);
    ESTitleId* titleIdWork = (ESTitleId*)work;

    if (__esFd < 0 || dataDir == NULL) {
        ret = ES_ERR_INVALID;
        goto out;
    }

    if (!ES_IS_ALIGNED(dataDir)) {
        ret = ES_ERR_INVALID;
        goto out;
    }

    *titleIdWork = titleId;
    vec[0].base = titleIdWork;
    vec[0].length = sizeof(ESTitleId);
    vec[1].base = dataDir;
    vec[1].length = 30; // "/title/XXXXXXXX/XXXXXXXX/data"
    ret = IOS_Ioctlv(__esFd, ES_IOCTLV_GET_DATA_DIR, 1, 1, vec);

out:
    return ret;
}

ESError ESP_GetTitleId(ESTitleId* titleId) {
    ESError ret = ES_ERR_OK;
    u8 work[ES_WORK_SIZE] ALIGN(32);
    IPCIOVector* vec = ES_WORK_VECTORS(work);

    if (__esFd < 0 || titleId == NULL) {
        ret = ES_ERR_INVALID;
        goto out;
    }

    vec[0].base = work;
    vec[0].length = sizeof(ESTitleId);
    ret = IOS_Ioctlv(__esFd, ES_IOCTLV_GET_TITLE_ID, 0, 1, vec);

    if (ret == ES_ERR_OK) {
        *titleId = *(ESTitleId*)work;
    }

out:
    return ret;
}

ESError ESP_GetConsumption(ESTicketId ticketId, ESLpEntry* limitEntries,
                           u32* numEntries) {
    ESError ret = ES_ERR_OK;
    u8 work[ES_WORK_SIZE] ALIGN(32);
    IPCIOVector* vec = ES_WORK_VECTORS(work);
    ESTicketId* ticketIdWork = (ESTicketId*)work;
    u32* countWork = (u32*)(work + 32);

    if (__esFd < 0) {
        ret = ES_ERR_INVALID;
        goto out;
    }

    if (!ES_IS_ALIGNED(limitEntries)) {
        ret = ES_ERR_INVALID;
        goto out;
    }

    *ticketIdWork = ticketId;
    vec[0].base = ticketIdWork;
    vec[0].length = sizeof(ESTicketId);

    if (limitEntries == NULL) {
        vec[1].base = NULL;
        vec[1].length = 0;
    } else {
        vec[1].base = limitEntries;
        vec[1].length = sizeof(ESLpEntry) * *numEntries;
        *countWork = *numEntries;
    }

    vec[2].base = countWork;
    vec[2].length = sizeof(u32);
    ret = IOS_Ioctlv(__esFd, ES_IOCTLV_GET_CONSUMPTION, 1, 2, vec);
    *numEntries = *countWork;

out:
    return ret;
}
