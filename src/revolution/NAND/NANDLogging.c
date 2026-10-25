#include <revolution/FS.h>
#include <revolution/NAND.h>
#include <revolution/OS.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

#define NAND_LOG_PATH "/shared2/test2/nanderr.log"
#define NAND_LOG_LINE_SIZE 256
#define NAND_LOG_LINE_MAX 64

// s_fd states besides a real descriptor
#define NAND_LOG_FD_FREE -255
#define NAND_LOG_FD_RESERVED -254

static void asyncRoutine(s32 result, void* arg);

static s32 s_fd = NAND_LOG_FD_FREE;
static int s_stage;
static char s_message[NAND_LOG_LINE_SIZE] ALIGN(64);
static NANDLoggingCallback s_callback = NULL;

static void prepareLine(char* line, int count, const char* msg) {
    int len;
    char titleId[64];
    OSCalendarTime time;

    memset(line, ' ', NAND_LOG_LINE_SIZE - 2);
    OSTicksToCalendarTime(OSGetTime(), &time);

    // Home dir is "/title/XXXXXXXX/XXXXXXXX/data"; keep "XXXXXXXX-XXXXXXXX"
    strncpy(titleId, nandGetHomeDir() + 7, 8 + 1 + 8);
    titleId[8] = '-';
    titleId[8 + 1 + 8] = '\0';

    len = snprintf(line, NAND_LOG_LINE_SIZE,
                   "%d %04d/%02d/%02d %02d:%02d:%02d %s %s",
                   count % (NAND_LOG_LINE_MAX - 1) + 1, time.year,
                   time.month + 1, time.mday, time.hour, time.min, time.sec,
                   titleId, msg);

    if (len < NAND_LOG_LINE_SIZE) {
        line[len] = ' ';
    }

    line[NAND_LOG_LINE_SIZE - 2] = '\r';
    line[NAND_LOG_LINE_SIZE - 1] = '\n';
}

static BOOL reserveFileDescriptor(void) {
    BOOL enabled = FALSE;
    BOOL busy = FALSE;

    enabled = OSDisableInterrupts();

    if (s_fd == NAND_LOG_FD_FREE) {
        s_fd = NAND_LOG_FD_RESERVED;
        busy = FALSE;
    } else if (s_fd == NAND_LOG_FD_RESERVED) {
        busy = TRUE;
    } else if (s_fd >= 0) {
        busy = TRUE;
    } else {
        busy = TRUE;
    }

    OSRestoreInterrupts(enabled);
    return busy ? FALSE : TRUE;
}

BOOL NANDLoggingAddMessageAsync(NANDLoggingCallback callback, const char* fmt,
                                ...) {
    va_list list;
    s32 result;

    if (!reserveFileDescriptor()) {
        return FALSE;
    }

    va_start(list, fmt);
    vsnprintf(s_message, NAND_LOG_LINE_SIZE, fmt, list);
    va_end(list);

    s_callback = callback;
    s_stage = 1;

    result = ISFS_OpenAsync(NAND_LOG_PATH, IPC_OPEN_RW, asyncRoutine, NULL);
    if (result == IPC_RESULT_OK) {
        return TRUE;
    } else {
        return FALSE;
    }
}

#define CALLBACK(success)                                                      \
    if (s_callback != NULL)                                                    \
    s_callback(success)

/**
 * Appends the message as one line of the log file. The first line holds the
 * number of the last line written, so the log is a ring of 63 messages.
 */
static void asyncRoutine(s32 result, void* arg) {
#pragma unused(arg)

    s32 ret;
    static char s_rBuf[NAND_LOG_LINE_SIZE] ALIGN(64);
    static char s_wBuf[NAND_LOG_LINE_SIZE] ALIGN(64);

    s_stage++;

    if (s_stage == 2) {
        if (result >= 0) {
            s_fd = result;
            ret = ISFS_SeekAsync(s_fd, 0, IPC_SEEK_BEG, asyncRoutine, NULL);
            if (ret != IPC_RESULT_OK) {
                CALLBACK(FALSE);
            }
        } else {
            CALLBACK(FALSE);
        }
    } else if (s_stage == 3) {
        if (result == IPC_RESULT_OK) {
            ret = ISFS_ReadAsync(s_fd, s_rBuf, NAND_LOG_LINE_SIZE, asyncRoutine,
                                 NULL);
            if (ret != IPC_RESULT_OK) {
                CALLBACK(FALSE);
            }
        } else {
            CALLBACK(FALSE);
        }
    } else if (s_stage == 4) {
        if (result == NAND_LOG_LINE_SIZE) {
            ret = ISFS_SeekAsync(s_fd, 0, IPC_SEEK_BEG, asyncRoutine, NULL);
            if (ret != IPC_RESULT_OK) {
                CALLBACK(FALSE);
            }
        } else {
            CALLBACK(FALSE);
        }
    } else if (s_stage == 5) {
        if (result == IPC_RESULT_OK) {
            int count = 0;
            s_rBuf[NAND_LOG_LINE_SIZE - 1] = '\0';
            count = atoi(s_rBuf);
            prepareLine(s_wBuf, count, s_message);

            ret = ISFS_WriteAsync(s_fd, s_wBuf, NAND_LOG_LINE_SIZE,
                                  asyncRoutine, NULL);
            if (ret != IPC_RESULT_OK) {
                CALLBACK(FALSE);
            }
        } else {
            CALLBACK(FALSE);
        }
    } else if (s_stage == 6) {
        if (result == NAND_LOG_LINE_SIZE) {
            int count = atoi(s_rBuf);
            ret = ISFS_SeekAsync(s_fd, count * NAND_LOG_LINE_SIZE,
                                 IPC_SEEK_BEG, asyncRoutine, NULL);
            if (ret != IPC_RESULT_OK) {
                CALLBACK(FALSE);
            }
        } else {
            CALLBACK(FALSE);
        }
    } else if (s_stage == 7) {
        int count = atoi(s_rBuf);
        if (result == count * NAND_LOG_LINE_SIZE) {
            ret = ISFS_WriteAsync(s_fd, s_wBuf, NAND_LOG_LINE_SIZE,
                                  asyncRoutine, NULL);
            if (ret != IPC_RESULT_OK) {
                CALLBACK(FALSE);
            }
        } else {
            CALLBACK(FALSE);
        }
    } else if (s_stage == 8) {
        if (result == NAND_LOG_LINE_SIZE) {
            ret = ISFS_CloseAsync(s_fd, asyncRoutine, NULL);
            if (ret != IPC_RESULT_OK) {
                CALLBACK(FALSE);
            }
        } else {
            CALLBACK(FALSE);
        }
    } else if (s_stage == 9) {
        if (result == IPC_RESULT_OK) {
            s_fd = NAND_LOG_FD_FREE;
            CALLBACK(TRUE);
        } else {
            CALLBACK(FALSE);
        }
    }
}
