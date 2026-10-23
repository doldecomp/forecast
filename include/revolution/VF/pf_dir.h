#ifndef RVL_SDK_VF_PF_DIR_H
#define RVL_SDK_VF_PF_DIR_H
#include <types.h>
#ifdef __cplusplus
extern "C" {
#endif

s32 VFiPFDIR_fsfirst(struct PF_STR* p_path, u8 attr, struct PF_DTA* p_dta);
s32 VFiPFDIR_fsnext(struct PF_DTA* p_dta);
s32 VFiPFDIR_mkdir(struct PF_STR* p_path);
void VFiPFDIR_FinalizeAllDirs(struct PF_VOLUME* p_vol);

#ifdef __cplusplus
}
#endif
#endif
