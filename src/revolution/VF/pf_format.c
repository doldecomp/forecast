#include <revolution/VF.h>

int VFipf2_format(s8 drive, const u8* param) {
    s32 err;

    err = VFiPFVOL_format(drive, param);
    return VFiPFAPI_convertReturnValue(err);
}
