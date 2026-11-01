#ifndef RVL_SDK_SO_COMMON_H
#define RVL_SDK_SO_COMMON_H
#include <types.h>
#ifdef __cplusplus
extern "C" {
#endif

// Error codes returned by the SO functions (and stored as the thread's error).
// Names and values from the Mario Kart Wii decompilation (doldecomp/mkw, CC0).
typedef enum SOError {
    SO_SUCCESS = 0,
    SO_EFATAL = (int)0x80000000,
    SO_E2BIG = -1,
    SO_EACCES = -2,
    SO_EADDRINUSE = -3,
    SO_EADDRNOTAVAIL = -4,
    SO_EAFNOSUPPORT = -5,
    SO_EAGAIN = -6,
    SO_EALREADY = -7,
    SO_EBADF = -8,
    SO_EBADMSG = -9,
    SO_EBUSY = -10,
    SO_ECANCELED = -11,
    SO_ECHILD = -12,
    SO_ECONNABORTED = -13,
    SO_ECONNREFUSED = -14,
    SO_ECONNRESET = -15,
    SO_EDEADLK = -16,
    SO_EDESTADDRREQ = -17,
    SO_EDOM = -18,
    SO_EDQUOT = -19,
    SO_EEXIST = -20,
    SO_EFAULT = -21,
    SO_EFBIG = -22,
    SO_EHOSTUNREACH = -23,
    SO_EIDRM = -24,
    SO_EILSEQ = -25,
    SO_EINPROGRESS = -26,
    SO_EINTR = -27,
    SO_EINVAL = -28,
    SO_EIO = -29,
    SO_EISCONN = -30,
    SO_EISDIR = -31,
    SO_ELOOP = -32,
    SO_EMFILE = -33,
    SO_EMLINK = -34,
    SO_EMSGSIZE = -35,
    SO_EMULTIHOP = -36,
    SO_ENAMETOOLONG = -37,
    SO_ENETDOWN = -38,
    SO_ENETRESET = -39,
    SO_ENETUNREACH = -40,
    SO_ENFILE = -41,
    SO_ENOBUFS = -42,
    SO_ENODATA = -43,
    SO_ENODEV = -44,
    SO_ENOENT = -45,
    SO_ENOEXEC = -46,
    SO_ENOLCK = -47,
    SO_ENOLINK = -48,
    SO_ENOMEM = -49,
    SO_ENOMSG = -50,
    SO_ENOPROTOOPT = -51,
    SO_ENOSPC = -52,
    SO_ENOSR = -53,
    SO_ENOSTR = -54,
    SO_ENOSYS = -55,
    SO_ENOTCONN = -56,
    SO_ENOTDIR = -57,
    SO_ENOTEMPTY = -58,
    SO_ENOTSOCK = -59,
    SO_ENOTSUP = -60,
    SO_ENOTTY = -61,
    SO_ENXIO = -62,
    SO_EOPNOTSUPP = -63,
    SO_EOVERFLOW = -64,
    SO_EPERM = -65,
    SO_EPIPE = -66,
    SO_EPROTO = -67,
    SO_EPROTONOSUPPORT = -68,
    SO_EPROTOTYPE = -69,
    SO_ERANGE = -70,
    SO_EROFS = -71,
    SO_ESPIPE = -72,
    SO_ESRCH = -73,
    SO_ESTALE = -74,
    SO_ETIME = -75,
    SO_ETIMEDOUT = -76,
    SO_ETXTBSY = -77,
    SO_EWOULDBLOCK = SO_EAGAIN,
    SO_EXDEV = -78
} SOError;

// SOStartupEx gave up waiting for the network link to come up.
#define SO_ERR_LINK_UP_TIMEOUT (-121)

// Allocator pair the application hands to SOInit. The name tells the
// allocator what the block is for (0xB: resolver buffer, 0xC: IPC requests).
typedef void* (*SOAlloc)(u32 name, s32 size);
typedef void (*SOFree)(u32 name, void* ptr, s32 size);

typedef struct SOLibraryConfig {
    SOAlloc alloc; // at 0x0
    SOFree free;   // at 0x4
} SOLibraryConfig;

// Library state shared by the SO and SOCKET code.
typedef struct SOSysWork {
    SOAlloc allocFunc; // at 0x0
    SOFree freeFunc;   // at 0x4
    s32 rmState;       // at 0x8
    s32 rmFd;          // at 0xC (descriptor of /dev/net/ip/top)
    void* hostBuffer;  // at 0x10 (0x460 bytes for the resolver)
    s32 allocCount;    // at 0x14
} SOSysWork;

s32 SOInit(const SOLibraryConfig* config);
s32 SOStartup(void);
s32 SOStartupEx(s32 timeout);
s32 SOCleanup(void);

void* SOiAlloc(u32 name, s32 size);
void SOiFree(u32 name, void* ptr, s32 size);
s32 SOiPrepare(const char* funcName, s32* pRmFd);
s32 SOiConclude(const char* funcName, s32 result);
s32 SOiPrepareTempRm(const char* funcName, s32* pRmFd, BOOL* pIsTempRm);
s32 SOiConcludeTempRm(const char* funcName, s32 result, BOOL isTempRm);
s32 SOiWaitForDHCPEx(s32 timeout);

#ifdef __cplusplus
}
#endif
#endif
