// Based on SOGetHostID and SOGetInterfaceOpt in src/SO/socket.cpp from the
// Monster Hunter Tri decompilation (InusualZ/mhtri-decomp, CC0 1.0), which
// has a newer build of these functions. Adjusted to this channel's build
// from its assembly.
#include <revolution/IPC.h>
#include <revolution/SO.h>
#include <cstring>

// Ioctls of /dev/net/ip/top
enum {
    SO_IOCTL_GET_HOST_ID = 0x10,
    SO_IOCTLV_GET_INTERFACE_OPT = 0x1C
};

// Capacity the caller gave for an option value (0 without a valid length)
#define SO_OPT_LEN(optlen) ((optlen) == NULL || *(optlen) < 0 ? 0 : *(optlen))

// IPC block of SOGetInterfaceOpt. The request is followed by the answer length
// and the answer, each starting a new 32-byte line.
typedef struct SOInterfaceOptBlock {
    IPCIOVector vectors[3]; // at 0x0
    u8 _18[0x20 - 0x18];
    s32 level;              // at 0x20
    s32 optname;            // at 0x24
} SOInterfaceOptBlock;

u32 SOGetHostID(void) {
    s32 rmFd;
    s32 result;
    u32 hostId = 0;

    result = SOiPrepare(NULL, &rmFd);
    if (result == SO_SUCCESS) {
        hostId = IOS_Ioctl(rmFd, SO_IOCTL_GET_HOST_ID, NULL, 0, NULL, 0);
        SOiConclude(NULL, result);
    }
    return hostId;
}

s32 SOGetInterfaceOpt(SOInterface* interface, s32 level, s32 optname,
                      void* optval, s32* optlen) {
#pragma unused(interface)
    s32 rmFd;
    BOOL isTempRm;
    s32 size;
    s32 result;
    SOInterfaceOptBlock* block;
    u8* request;
    s32* outLen;
    u8* out;

    result = SOiPrepareTempRm(NULL, &rmFd, &isTempRm);
    if (result == SO_SUCCESS) {
        if (optname == SO_CONFIG_FILTER_INPUT ||
            optname == SO_CONFIG_FILTER_OUTPUT) {
            result = SO_EINVAL;
        } else {
            size = (SO_OPT_LEN(optlen) + 0x7F) & ~0x1F;
            block = (SOInterfaceOptBlock*)SOiAlloc(0xC, size);
            if (block == NULL) {
                result = SO_ENOMEM;
            } else {
                block->level = level;
                block->optname = optname;
                request = (u8*)block + 0x20;
                outLen = (s32*)(request + 0x20);
                out = (u8*)outLen + 0x20;
                *outLen = SO_OPT_LEN(optlen);

                block->vectors[0].base = request;
                block->vectors[0].length = 8;
                block->vectors[1].base = out;
                block->vectors[1].length = SO_OPT_LEN(optlen);
                block->vectors[2].base = outLen;
                block->vectors[2].length = 4;

                result = IOS_Ioctlv(rmFd, SO_IOCTLV_GET_INTERFACE_OPT, 1, 2,
                                    block->vectors);
                if (result >= 0 && optlen != NULL) {
                    if (*optlen >= *outLen) {
                        if (optval != NULL) {
                            memcpy(optval, out, *outLen);
                        }
                        *optlen = *outLen;
                    } else {
                        // Tell the caller how much room the value needs
                        *optlen = *outLen;
                        result = SO_EINVAL;
                    }
                }
                SOiFree(0xC, block, size);
            }
        }
        result = SOiConcludeTempRm(NULL, result, isTempRm);
    }
    return result;
}
