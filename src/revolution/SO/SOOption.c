#include <revolution/IPC.h>
#include <revolution/SO.h>
#include <cstring>

// Length of an optional in/out option buffer
#define SO_OPTLEN(p) (((p) == NULL || *(p) < 0) ? 0 : *(p))

int SOGetInterfaceOpt(SOInterface* interface, int level, int optname,
                      void* optval, int* optlen) {
    u32 bufSize;
    u8* buf;
    s32 rmId;
    int result;
    BOOL isTempRm;
    IPCIOVector* vec;
    int* in;
    int* outLen;
    void* outVal;
#pragma unused(interface)

    result = SOiPrepareTempRm(NULL, &rmId, &isTempRm);
    if (result == SO_SUCCESS) {
        if (optname == SO_CONFIG_FILTER_INPUT ||
            optname == SO_CONFIG_FILTER_OUTPUT) {
            result = SO_EINVAL;
        } else {
            // One IPC buffer holds the vectors, the request, the output length
            // and the output value, each 32-byte aligned
            bufSize = OSRoundUp32B(SO_OPTLEN(optlen) + 0x60);
            buf = (u8*)SOiAlloc(SO_MEM_INTERFACE_OPT, (s32)bufSize);
            if (buf == NULL) {
                result = SO_ENOMEM;
            } else {
                vec = (IPCIOVector*)buf;
                in = (int*)(buf + 0x20);
                outLen = (int*)((u8*)in + 0x20);
                outVal = (u8*)outLen + 0x20;

                in[0] = level;
                in[1] = optname;
                *outLen = SO_OPTLEN(optlen);

                vec[0].base = in;
                vec[0].length = 2 * sizeof(int);
                vec[1].base = outVal;
                vec[1].length = SO_OPTLEN(optlen);
                vec[2].base = outLen;
                vec[2].length = sizeof(int);

                result = IOS_Ioctlv(rmId, SO_IOCTL_GETINTERFACEOPT, 1, 2, vec);
                if (result >= 0 && optlen != NULL) {
                    if (*optlen >= *outLen) {
                        if (optval != NULL) {
                            memcpy(optval, outVal, *outLen);
                        }
                        *optlen = *outLen;
                    } else {
                        *optlen = *outLen;
                        result = SO_EINVAL;
                    }
                }

                SOiFree(SO_MEM_INTERFACE_OPT, buf, (s32)bufSize);
            }
        }

        result = SOiConcludeTempRm(NULL, result, isTempRm);
    }

    return result;
}
