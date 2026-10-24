// VF directory lookups (first/next) and directory creation.  The retail
// library keeps these three public VFiPFDIR entry points in their own
// translation unit (0x8004E064..0x8004E3C0), separate from the p_* helpers
// that live in pf_dir.c.
#include <revolution/VF.h>

extern PF_VOLUME_SET VFipf_vol_set;

s32 VFiPFDIR_fsfirst(PF_STR* p_path_str, u8 attr, struct PF_DTA* p_dta) {
    PF_VOLUME* p_vol;
    s32 err;

    if (p_path_str == NULL) {
        return VFipf_vol_set.last_error = 0xA;
    }

    p_vol = VFiPFPATH_GetVolumeFromPath(p_path_str);
    if (p_vol == NULL) {
        return VFipf_vol_set.last_error = 9;
    }

    if (p_dta == NULL) {
        return p_vol->last_error = VFipf_vol_set.last_error = 0xA;
    }

    if (attr != 0x7F && (((attr & 0xFFFFFF00) != 0) || ((attr & 0x18) == 0x18))) {
        return p_vol->last_error = VFipf_vol_set.last_error = 0xA;
    }

    err = VFiPFVOL_CheckForRead(p_vol);
    if (err != 0) {
        VFipf_vol_set.last_error = err;
        p_vol->last_error = err;

        return err;
    }

    err = VFiPFDIR_p_fsfirst(p_vol, p_path_str, attr, p_dta);
    if (err != 0) {
        p_dta->p_vol = NULL;
        VFipf_vol_set.last_error = err;
        p_vol->last_error = err;
    }

    return err;
}

s32 VFiPFDIR_fsnext(struct PF_DTA* p_dta) {
    PF_VOLUME* p_vol;
    s32 err;

    if (p_dta == NULL) {
        return VFipf_vol_set.last_error = 0xA;
    }

    p_vol = p_dta->p_vol;
    if (p_vol == NULL) {
        return VFipf_vol_set.last_error = 0xA;
    }

    err = VFiPFVOL_CheckForRead(p_vol);
    if (err != 0) {
        VFipf_vol_set.last_error = err;
        p_vol->last_error = err;

        return err;
    }

    err = VFiPFDIR_p_fsnext(p_dta);
    if (err != 0) {
        VFipf_vol_set.last_error = err;
        p_vol->last_error = err;
    }

    return err;
}

s32 VFiPFDIR_mkdir(PF_STR* p_path_str) {
    PF_VOLUME* p_vol;
    s32 err;

    if (p_path_str == NULL) {
        return VFipf_vol_set.last_error = 10;
    }

    if (PF_IS_PATH_NULL(p_path_str, 1, 0)) {
        return VFipf_vol_set.last_error = 10;
    }

    p_vol = VFiPFPATH_GetVolumeFromPath(p_path_str);
    if (p_vol == NULL) {
        return VFipf_vol_set.last_error = 9;
    }

    err = VFiPFVOL_CheckForWrite(p_vol);
    if (err != 0) {
        VFipf_vol_set.last_error = err;
        p_vol->last_error = err;

        return err;
    }

    p_vol->cache.signature = NULL;

    err = VFiPFDIR_p_mkdir(p_vol, p_path_str, 0, NULL);
    if (err != 0) {
        VFipf_vol_set.last_error = err;
        p_vol->last_error = err;
    } else {
        err = VFiPFCACHE_FlushFATCache(p_vol);
        if (err != 0) {
            VFipf_vol_set.last_error = err;
            p_vol->last_error = err;
        } else {
            err = VFiPFCACHE_FlushDataCacheSpecific(p_vol, NULL);
            if (err != 0) {
                VFipf_vol_set.last_error = err;
                p_vol->last_error = err;
            }
        }
    }

    p_vol->cache.signature = NULL;

    return err;
}
