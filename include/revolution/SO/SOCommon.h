#ifndef RVL_SDK_SO_COMMON_H
#define RVL_SDK_SO_COMMON_H
#include <types.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
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
    SO_EXDEV = -78,
    SO_EWOULDBLOCK = SO_EAGAIN,

    SO_ERR_DHCP_TIMEOUT = -100,
    SO_ERR_DHCP_EXPIRED = -101,
    SO_ERR_DHCP_NAK = -102,
    SO_ERR_LCP = -107,
    SO_ERR_AUTH = -108,
    SO_ERR_IPCP = -109,
    SO_ERR_ADDR_COLLISION = -111,
    SO_ERR_LINK_DOWN = -112,
    SO_ERR_LINK_UP_TIMEOUT = -121
} SOError;

typedef void* (*SOAllocFunc)(u32 name, s32 size);
typedef void (*SOFreeFunc)(u32 name, void* ptr, s32 size);

typedef struct SOLibraryConfig {
    SOAllocFunc alloc; // at 0x0
    SOFreeFunc free;   // at 0x4
} SOLibraryConfig;

int SOInit(const SOLibraryConfig* config);
int SOStartup(void);
int SOStartupEx(int timeOut);
int SOCleanup(void);

// Internal (SOPrivate.h)

// /dev/net/ip/top ioctl commands (names unofficial)
enum {
    SO_IOCTL_GETHOSTID = 0x10,
    SO_IOCTL_GETINTERFACEOPT = 0x1C,
    SO_IOCTL_STARTUP = 0x1F
};

// Names SO passes to the allocator for its blocks (names unofficial)
enum {
    SO_MEM_RESOLVER = 0xB,
    SO_MEM_INTERFACE_OPT = 0xC
};

typedef enum {
    SO_INTERNAL_RM_STATE_OPENED = 0,
    SO_INTERNAL_RM_STATE_WORKING = -1,
    SO_INTERNAL_RM_STATE_CLOSED = -2
} SOInternalRmState;

typedef struct SOSysWork {
    SOAllocFunc alloc; // at 0x0
    SOFreeFunc free;   // at 0x4
    s32 rmState;       // at 0x8
    s32 rmFd;          // at 0xC
    void* pResolver;   // at 0x10
    s32 allocCount;    // at 0x14
} SOSysWork;

void* SOiAlloc(u32 name, s32 size);
void SOiFree(u32 name, void* ptr, s32 size);
int SOiPrepare(const char* funcName, s32* pRmId);
int SOiConclude(const char* funcName, int result);
int SOiPrepareTempRm(const char* funcName, s32* pRmId, BOOL* pIsTempRm);
int SOiConcludeTempRm(const char* funcName, int result, BOOL isTempRm);
int SOiWaitForDHCPEx(int timeOut);

#ifdef __cplusplus
}
#endif
#endif
