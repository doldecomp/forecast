#ifndef RVL_SDK_DVD_BROADWAY_H
#define RVL_SDK_DVD_BROADWAY_H
#include <types.h>
#ifdef __cplusplus
extern "C" {
#endif

#define DVD_LOW_OFFSET(x) ((x) >> 2)
#define DVD_LOW_SPEED(x) (((x) & 3) << 16)

// Forward declarations
typedef struct DVDDiskID DVDDiskID;
typedef struct DVDDriveInfo DVDDriveInfo;
typedef struct ESPTicket ESPTicket;
typedef struct ESPTmd ESPTmd;
typedef struct ESTicket ESTicket;
typedef struct ESTicketView ESTicketView;
typedef struct ESTitleMeta ESTitleMeta;
typedef struct OSAlarm OSAlarm;

typedef enum {
    DVD_INTTYPE_TC = (1 << 0),   // Transaction callback?
    DVD_INTTYPE_DE = (1 << 1),   // Drive error
    DVD_INTTYPE_CVR = (1 << 2),  // Something with DVD cover
    DVD_INTTYPE_BR = (1 << 3),   // Break requested
    DVD_INTTYPE_TIME = (1 << 4), // Time out
    DVD_INTTYPE_SERR = (1 << 5), // Security error
    DVD_INTTYPE_VERR = (1 << 6), // Verify error
    DVD_INTTYPE_ARGS = (1 << 7), // Bad arguments
} DVDLowIntType;

// DICVR - DI Cover Register (via DVDLowGetCoverRegister)
#define DVD_DICVR_CVR (1 << 0)
#define DVD_DICVR_CVRINTMASK (1 << 1)
#define DVD_DICVR_CVRINT (1 << 2)

typedef void (*DVDLowCallback)(u32 intType);

BOOL DVDLowFinalize(void);
BOOL DVDLowInit(void);
BOOL DVDLowReadDiskID(DVDDiskID* out, DVDLowCallback callback);
BOOL DVDLowOpenPartition(u32 offset, const ESPTicket* ticket, u32 certsSize,
                         const void* certs, ESPTmd* tmd,
                         DVDLowCallback callback);
BOOL DVDLowOpenPartitionWithTmdAndTicketView(u32 offset,
                                             const ESTicketView* ticketView,
                                             u32 tmdSize, const ESTitleMeta* tmd,
                                             u32 certsSize, const u8* certs,
                                             DVDLowCallback callback);
BOOL DVDLowGetNoDiscBufferSizes(u32 offset, u32* tmdSize, u32* certsSize,
                                DVDLowCallback callback);
BOOL DVDLowGetNoDiscOpenPartitionParams(u32 offset, ESTicket* ticket,
                                        u32* tmdSize, ESTitleMeta* tmd,
                                        u32* certsSize, u8* certs,
                                        u32* dataWordOffset, u8* h3Hash,
                                        DVDLowCallback callback);
BOOL DVDLowClosePartition(DVDLowCallback callback);
BOOL DVDLowUnencryptedRead(void* dst, u32 size, u32 offset,
                           DVDLowCallback callback);
BOOL DVDLowStopMotor(BOOL eject, BOOL kill, DVDLowCallback callback);
BOOL DVDLowInquiry(DVDDriveInfo* out, DVDLowCallback callback);
BOOL DVDLowRequestError(DVDLowCallback callback);
BOOL DVDLowSetSpinupFlag(BOOL enable);
BOOL DVDLowReset(DVDLowCallback callback);
BOOL DVDLowAudioBufferConfig(BOOL enable, u32 size, DVDLowCallback callback);
BOOL DVDLowSetMaximumRotation(u32 speed, DVDLowCallback callback);
BOOL DVDLowRead(void* dst, u32 size, u32 offset, DVDLowCallback callback);
BOOL DVDLowSeek(u32 offset, DVDLowCallback callback);
u32 DVDLowGetCoverRegister(void);
u32 DVDLowGetStatusRegister(void);
BOOL DVDLowPrepareCoverRegister(DVDLowCallback callback);
BOOL DVDLowPrepareStatusRegister(DVDLowCallback callback);
u32 DVDLowGetImmBufferReg(void);
BOOL DVDLowUnmaskStatusInterrupts(void);
BOOL DVDLowMaskCoverInterrupt(void);
BOOL DVDLowClearCoverInterrupt(DVDLowCallback callback);
BOOL __DVDLowTestAlarm(const OSAlarm* alarm);

#ifdef __cplusplus
}
#endif
#endif
