#include <revolution/VF.h>

int VFipf2_fsnext(struct PF_DTA* p_dta) {
    s32 err;

    err = VFiPFDIR_fsnext(p_dta);
    return VFiPFAPI_convertReturnValue(err);
}
