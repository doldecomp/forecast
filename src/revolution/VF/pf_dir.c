#include <revolution/VF.h>
#include <revolution/VF/pf_cache.h>
#include <revolution/VF/pf_dir.h>
#include <revolution/VF/pf_entry.h>
#include <revolution/VF/pf_fat.h>
#include <revolution/VF/pf_file.h>
#include <revolution/VF/pf_path.h>
#include <revolution/VF/pf_sector.h>
#include <revolution/VF/pf_str.h>
#include <revolution/VF/pf_clib.h>
#include <revolution/VF/pf_w_clib.h>
#include <revolution/VF/pf_volume.h>

#pragma ppc_no_fp_blockmove on

extern PF_VOLUME_SET VFipf_vol_set;



s32 VFiPFDIR_p_mkdir(PF_VOLUME* p_vol, PF_STR* p_path, u32 mode, struct PF_DTA* p_dta) {
    u32 is_found;
    PF_CACHE_PAGE* p_page;
    u32 sector;
    u32 success_size;
    s32 pozIdx;
    u32 next_chain[2];
    PF_STR dir_path;
    PF_STR filename;
    PF_FAT_HINT hint;
    u8 buf[32];
    PF_FFD ffd;
    PF_ENT_ITER iter;
    s8 dest[0x200];
    PF_DIR_ENT ent;
    PF_DIR_ENT ent2;
    u8 num_entry_LFNs;
    u32 offset;
    u16 cnt;
    u8 sum;
    u32 position;
    u32 i;
    u32 OldSector;
    u32 cluster;
    s32 err;
    num_entry_LFNs = 0;
    sector = 0;

    err = VFiPFPATH_SplitPath(p_path, &dir_path, &filename);
    if (err != 0) {
        return err;
    }

    err = VFiPFENT_ITER_GetEntryOfPath(&iter, &ent, p_vol, &dir_path, 1);
    if (err != 0) {
        return err;
    }

    if ((ent.attr & 0x10) == 0) {
        return 0x14;
    }

    cnt = VFiPFSTR_StrNumChar(&filename, 1);
    if ((u16)cnt > 0xFF) {
        return 1;
    }

    if ((u16)cnt + ent.path_len > 259) {
        return 2;
    }

    if (VFiPFSTR_GetCodeMode(&filename) == 2) {
        VFiPFPATH_transformFromUnicodeToNormal(dest, (const u16*)VFiPFSTR_GetStrPos(&filename, 1));
    }
    VFiPFSTR_SetLocalStr(&filename, dest);

    if (p_dta != NULL) {
        p_dta->p_file = NULL;
        p_dta->p_dir = NULL;
        p_dta->parent_start_cluster = ent.start_cluster;
        p_dta->p_vol = ent.p_vol;
        p_dta->attr = 0x10;
        if (VFiPFSTR_GetCodeMode(&filename) == 1) {
            VFiPFPATH_SetSearchPattern(p_dta->reg_exp, NULL, &filename);
            p_dta->status |= 1;
        } else {
            VFiPFPATH_SetSearchPattern(p_dta->reg_exp, (u16*)((u8*)p_dta + 0x444), &filename);
            p_dta->status |= 2;
        }
    }

    VFiPFFAT_InitFFD(&ffd, &hint, p_vol, &ent.start_cluster);
    iter.ffd = ffd;
    VFiPFFAT_ResetFFD(&iter.ffd, &ent.start_cluster);
    err = VFiPFENT_ITER_IteratorInitialize(&iter, 0);
    if (err != 0) {
        return err;
    }

    VFipf_memcpy(&ent2, &ent, sizeof(PF_DIR_ENT));
    err = VFiPFENT_ITER_FindEntry(&iter, &ent2, &filename, 0x10, 0, &is_found, 1);
    if (err != 0) {
        return err;
    }

    if (is_found != 0) {
        return 8;
    }

    err = VFiPFPATH_parseShortName(ent2.short_name, &filename);
    if (err != 0 && ent2.short_name[0] == 0) {
        return 1;
    }
    if (err != 0) {
        err = VFiPFENT_AdjustSFN(&ent, ent2.short_name);
        if (err != 0) {
            return err;
        }
        if (VFiPFSTR_GetCodeMode(&filename) == 1) {
            cnt = VFiPFPATH_transformInUnicode(ent2.long_name, (const s8*)VFiPFSTR_GetStrPos(&filename, 1));
        } else {
            VFipf_w_strcpy(ent2.long_name, (const u16*)VFiPFSTR_GetStrPos(&filename, 1));
        }
    } else {
        ent2.long_name[0] = 0;
    }

    ent2.start_cluster = 0;
    ent2.file_size = 0;
    ent2.p_vol = p_vol;
    ent2.small_letter_flag = 0;
    ent2.attr = 0x10;

    ent2.create_time_ms = VFiPFENT_getcurrentDateTimeForEnt(&ent2.create_date, &ent2.create_time);
    offset = ent2.entry_offset;
    ent2.access_date = ent2.create_date;
    ent2.modify_time = ent2.create_time;
    ent2.modify_date = ent2.create_date;

    if (ent2.long_name[0] != 0 && (ent2.small_letter_flag & 0x18) == 0) {
        num_entry_LFNs = (u32)cnt / 13 + ((u32)cnt % 13 != 0);
        err = VFiPFENT_allocateEntryPos(&ent2, num_entry_LFNs + 1, &ffd, next_chain, &filename, (u32*)&pozIdx);
        if (err != 0) {
            return err;
        }
        if ((VFipf_vol_set.setting & 2) == 2) {
            VFiPFPATH_AdjustExtShortName(ent2.short_name, pozIdx);
        }
    } else {
        err = VFiPFENT_allocateEntryPos(&ent2, 1, &ffd, next_chain, &filename, (u32*)&pozIdx);
        if (err != 0) {
            return err;
        }
    }

    VFiPFFAT_ResetFFD(&ffd, &ent2.start_cluster);
    err = VFiPFFAT_GetSectorSpecified(&ffd, 0, 1, &sector);
    if (err != 0) {
        return err;
    }
    if (sector == 0xFFFFFFFF) {
        return 6;
    }

    cluster = ffd.p_hint->cluster;
    if (p_vol->bpb.fat_type == FAT_32) {
        if (*iter.ffd.p_start_cluster == p_vol->bpb.root_dir_cluster && offset == 0 && cluster == p_vol->bpb.root_dir_cluster) {
            iter.ffd = ffd;
            err = VFiPFENT_ITER_IteratorInitialize(&iter, num_entry_LFNs);
            if (err != 0) {
                return err;
            }
            if (iter.buf[0] == 0) {
                VFiPFFAT_ResetFFD(&ffd, &ent2.start_cluster);
                *ffd.p_start_cluster = 0;
                ffd.p_hint->cluster = 0;
                cluster = p_vol->bpb.root_dir_cluster + 1;
                err = VFiPFFAT_GetSectorSpecified(&ffd, 0, 1, &sector);
                if (err != 0) {
                    return err;
                }
            }
        }
    }

    err = VFiPFCACHE_AllocateDataPage(p_vol, 0xFFFFFFFF, &p_page);
    if (err != 0) {
        return err;
    }
    VFipf_memset(p_page->p_buf, 0, p_vol->bpb.bytes_per_sector);

    {
        for (offset = p_vol->bpb.sectors_per_cluster; offset != 0; ) {
            if (offset == 1) {
                VFiPFENT_setDotEntry((s8*)p_page->p_buf);
                VFiPFENT_storeEntryNumericFieldsToBuf(p_page->p_buf, &ent2);
                VFiPFENT_setDotDotEntry((s8*)p_page->p_buf + 32);
                if (ent.entry_sector == 0xFFFFFFFF) {
                    ent2.start_cluster = 0;
                } else {
                    ent2.start_cluster = ent.start_cluster;
                }
                VFiPFENT_storeEntryNumericFieldsToBuf(p_page->p_buf + 32, &ent2);
            }
            err = VFiPFSEC_WriteData(p_vol, p_page->p_buf, sector + offset - 1, 0, p_vol->bpb.bytes_per_sector, &success_size, 0);
            if (err != 0) {
                break;
            }
            if (success_size != p_vol->bpb.bytes_per_sector) {
                err = 17;
                break;
            }
            offset--;
        }
    }
    VFiPFCACHE_FreeDataPage(p_vol, p_page);
    if (err != 0) {
        return err;
    }

    ent2.num_entry_LFNs = num_entry_LFNs;
    OldSector = ent2.entry_sector;
    sum = VFiPFENT_CalcCheckSum(&ent2);
    if ((ent2.small_letter_flag & 0x18) == 0) {
        position = (u32)next_chain;

        for (i = num_entry_LFNs; i >= 1; i--) {
            VFiPFENT_storeLFNEntryFieldsToBuf(buf, &ent2, i, sum, i == num_entry_LFNs);
            err = VFiPFSEC_WriteData(p_vol, buf, OldSector, ent2.entry_offset, 32, &success_size, 0);
            if (err != 0) {
                return err;
            }
            if (success_size != 32) {
                return 17;
            }
            ent2.entry_offset += 32;
            if (ent2.entry_offset >= p_vol->bpb.bytes_per_sector) {
                ent2.entry_offset = 0;
                OldSector = *(u32*)position;
                position += 4;
            }
        }
        ent2.entry_sector = OldSector;
    }

    ent2.start_cluster = cluster;
    err = VFiPFENT_updateEntry(&ent2, 0);
    if (err != 0) {
        return err;
    }

    if (p_dta != NULL) {
        p_dta->parent_pos = pozIdx + 1;
        VFipf_strcpy((s8*)p_dta->FileName, ent2.short_name);
        if ((p_dta->status & 2) == 2) {
            VFiPFPATH_transformInUnicode((u16*)((u8*)p_dta + 0x64C), (s8*)p_dta->FileName);
        }
        if (ent2.long_name[0] != 0) {
            if ((VFipf_vol_set.setting & 2) == 2) {
                VFipf_vol_set.setting = (VFipf_vol_set.setting & ~3) | 1;
                VFiPFPATH_transformFromUnicodeToNormal((s8*)p_dta->LongName, ent2.long_name);
                VFipf_vol_set.setting = (VFipf_vol_set.setting & ~3) | 2;
            } else {
                VFiPFPATH_transformFromUnicodeToNormal((s8*)p_dta->LongName, ent2.long_name);
            }
            if ((p_dta->status & 2) == 2) {
                VFipf_w_strcpy((u16*)((u8*)p_dta + 0x666), ent2.long_name);
            }
            p_dta->check_sum = sum;
            p_dta->ordinal = 1;
        } else {
            p_dta->LongName[0] = 0;
            if ((p_dta->status & 2) == 2) {
                *(u16*)((u8*)p_dta + 0x666) = 0;
            }
            p_dta->ordinal = 0;
            p_dta->check_sum = 0;
        }
        p_dta->FileSize = ent2.file_size;
        p_dta->Time = ent2.modify_time;
        p_dta->Date = ent2.modify_date;
        p_dta->Attribute = ent2.attr;
        p_dta->num_entry_LFNs = ent2.num_entry_LFNs;
    }

    return 0;
}

s32 VFiPFDIR_p_fsnext(struct PF_DTA* p_dta) {
    PF_DIR_ENT ent;
    PF_FFD ffd;
    PF_FAT_HINT hint;
    PF_STR pattern;
    PF_DIR_ENT* p_found_ent;
    u32 lpos;
    u32 ppos;
    s32 err;

    lpos = 0;
    ppos = 0;

    if ((p_dta->status & 1) == 1) {
        err = VFiPFSTR_InitStr(&pattern, (const s8*)p_dta->reg_exp, 1);
    } else {
        err = VFiPFSTR_InitStr(&pattern, (const s8*)((u8*)p_dta + 0x444), 2);
    }
    if (err != 0) {
        return err;
    }

    VFiPFSTR_SetLocalStr(&pattern, (s8*)p_dta->reg_exp);

    while (ppos < 999999) {
        if (p_dta->parent_pos >= 999999) {
            return 10;
        }

        ent.start_cluster = p_dta->parent_start_cluster;
        VFiPFFAT_InitFFD(&ffd, &hint, p_dta->p_vol, &ent.start_cluster);

        err = VFiPFENT_findEntryPos(&ffd, &ent, p_dta->parent_pos, &pattern, p_dta->attr, 0, &lpos, &ppos);
        if (err != 0) {
            return err;
        }

        p_dta->parent_pos = ppos + 1;
        if (ppos == 999999) {
            return 3;
        }
        break;
    }

    VFipf_strcpy((s8*)p_dta->FileName, ent.short_name);
    if ((p_dta->status & 2) == 2) {
        VFiPFPATH_transformInUnicode((u16*)((u8*)p_dta + 0x64C), (s8*)p_dta->FileName);
    }

    if (ent.long_name[0] != 0) {
        if ((VFipf_vol_set.setting & 2) == 2) {
            VFipf_vol_set.setting = (VFipf_vol_set.setting & ~3) | 1;
            VFiPFPATH_transformFromUnicodeToNormal((s8*)p_dta->LongName, ent.long_name);
            VFipf_vol_set.setting = (VFipf_vol_set.setting & ~3) | 2;
        } else {
            VFiPFPATH_transformFromUnicodeToNormal((s8*)p_dta->LongName, ent.long_name);
        }
        if ((p_dta->status & 2) == 2) {
            VFipf_w_strcpy((u16*)((u8*)p_dta + 0x666), ent.long_name);
        }
    } else {
        p_dta->LongName[0] = 0;
        if ((p_dta->status & 2) == 2) {
            *(u16*)((u8*)p_dta + 0x666) = 0;
        }
    }

    p_found_ent = NULL;
    if ((ent.attr & 0x10) != 0 && p_dta->p_vol->num_opened_directories != 0) {
        u32 i;
        p_found_ent = NULL;
        for (i = 0; i < 3; i++) {
            if ((ent.p_vol->sdds[i].stat & 1) && (ent.p_vol->sdds[i].stat & 2) &&
                ent.p_vol == ent.p_vol->sdds[i].dir_entry.p_vol &&
                ent.entry_sector == ent.p_vol->sdds[i].dir_entry.entry_sector &&
                ent.entry_offset == ent.p_vol->sdds[i].dir_entry.entry_offset) {
                p_found_ent = &ent.p_vol->sdds[i].dir_entry;
            }
        }
    } else if ((ent.attr & 0x10) == 0 && (ent.attr & 8) == 0) {
        if (p_dta->p_vol->num_opened_files != 0) {
            VFiPFFILE_GetOpenedFile(&ent, &p_found_ent);
        }
    }

    if (p_found_ent == NULL) {
        p_found_ent = &ent;
    }

    p_dta->FileSize = p_found_ent->file_size;
    p_dta->Time = p_found_ent->modify_time;
    p_dta->Date = p_found_ent->modify_date;
    p_dta->Attribute = p_found_ent->attr;
    if (p_dta->Attribute == 0) {
        p_dta->Attribute = 0x40;
    }
    p_dta->num_entry_LFNs = p_found_ent->num_entry_LFNs;
    p_dta->ordinal = p_found_ent->ordinal;
    p_dta->check_sum = p_found_ent->check_sum;

    return 0;
}

s32 VFiPFDIR_p_fsfirst(PF_VOLUME* p_vol, PF_STR* p_path, u8 attr, struct PF_DTA* p_dta) {
    PF_STR dir_path;
    PF_STR filename;
    PF_ENT_ITER iter;
    PF_DIR_ENT ent;
    s32 err;

    err = VFiPFPATH_SplitPathWildcard(p_path, &dir_path, &filename);
    if (err != 0) {
        return err;
    }

    err = VFiPFENT_ITER_GetEntryOfPattern(&iter, &ent, p_vol, &dir_path);
    if (err != 0) {
        return err;
    }

    if ((ent.attr & 0x10) == 0) {
        return 0x14;
    }

    p_dta->parent_start_cluster = ent.start_cluster;
    p_dta->parent_pos = 0;
    p_dta->p_vol = ent.p_vol;
    p_dta->attr = attr;
    p_dta->status = 0;

    if (VFiPFSTR_GetCodeMode(&filename) == 1) {
        VFiPFPATH_SetSearchPattern((s8*)p_dta->reg_exp, NULL, &filename);
        p_dta->status |= 1;
    } else {
        VFiPFPATH_SetSearchPattern((s8*)p_dta->reg_exp, (u16*)((u8*)p_dta + 0x444), &filename);
        p_dta->status |= 2;
    }

    return VFiPFDIR_p_fsnext(p_dta);
}

static void VFiPFDIR_FinalizeSDD(struct PF_SDD* p_sdd) {
    p_sdd->stat = 0;
    VFiPFFAT_FinalizeFFD(&p_sdd->ffd);
}

static void VFiPFDIR_FinalizeUDD(struct PF_DIR* p_dir) {
    p_dir->stat &= ~1u;
}

void VFiPFDIR_FinalizeAllDirs(struct PF_VOLUME* p_vol) {
    u16 i;

    for (i = 0; i < 3u; ++i) {
        VFiPFDIR_FinalizeSDD(&p_vol->sdds[i]);
    }
    for (i = 0; i < 3u; ++i) {
        VFiPFDIR_FinalizeUDD(&p_vol->udds[i]);
    }
    p_vol->num_opened_directories = 0;
}
