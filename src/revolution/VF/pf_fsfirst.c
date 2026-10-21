#include <revolution/VF.h>

extern PF_VOLUME_SET VFipf_vol_set;

int VFipf2_fsfirst(const s8* path, u8 attr, struct PF_DTA* p_dta) {
    s32 err;
    struct PF_STR path_str;

    err = VFiPFSTR_InitStr(&path_str, (const s8*)path, 1);
    if (err == 0) {
        err = VFiPFDIR_fsfirst(&path_str, attr, p_dta);
    } else {
        VFipf_vol_set.last_error = err;
    }

    return VFiPFAPI_convertReturnValue(err);
}
