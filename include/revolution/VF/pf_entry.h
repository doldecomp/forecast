#ifndef RVL_SDK_VF_PF_ENTRY_H
#define RVL_SDK_VF_PF_ENTRY_H
#include "revolution/VF/vf_struct.h"
#include <types.h>
#ifdef __cplusplus
extern "C" {
#endif

static u32 VFiPFENT_compareAttr(u8 attr, u8 attr_required, u8 attr_unwanted);
static s32 VFiPFENT_compareEntryName(PF_DIR_ENT* p_ent, PF_STR* p_pattern, u8 attr);
static s32 VFiPFENT_getEntry(PF_DIR_ENT* p_ent, PF_ENT_ITER* p_iter, PF_STR* p_pattern, u8 attr_required, u8 attr_unwanted, u32* logical_index);
static s32 VFiPFENT_searchEmptyTailSFN(PF_FFD* p_ffd, u32 tail_index, const s8* pattern, u32* p_tail_bit);
static s32 VFiPFENT_findEmptyTailSFN(PF_DIR_ENT* p_ent_containig_dir, const s8* name, u32* p_tails);
void VFiPFENT_setDotEntry(s8* sfn);
void VFiPFENT_setDotDotEntry(s8* sfn);
u8 VFiPFENT_CalcCheckSum(PF_DIR_ENT* p_ent);
void VFiPFENT_LoadShortNameFromBuf(PF_DIR_ENT* p_ent, const u8* buf);
void VFiPFENT_loadEntryNumericFieldsFromBuf(PF_DIR_ENT* p_ent, const u8* buf);
void VFiPFENT_storeEntryNumericFieldsToBuf(u8* buf, const struct PF_DIR_ENT* p_ent);
s32 VFiPFENT_LoadLFNEntryFieldsFromBuf(PF_DIR_ENT* p_ent, const u8* buf);
void VFiPFENT_storeLFNEntryFieldsToBuf(u8* buf, PF_DIR_ENT* p_ent, u8 ord, u8 sum, u32 is_last);
s32 VFiPFENT_findEntryPos(PF_FFD* p_ffd, PF_DIR_ENT* p_ent, u32 index_search_from, PF_STR* p_pattern, u8 attr_required, u8 attr_unwanted, u32* p_lpos, u32* p_ppos);
s32 VFiPFENT_findEntry(PF_FFD* p_ffd, PF_DIR_ENT* p_ent, u32 index_search_from, PF_STR* p_pattern, u8 attr_required, u8 attr_unwanted);
s32 VFiPFENT_allocateEntryPos(PF_DIR_ENT* p_ent, u8 num_entries, PF_FFD* p_ffd, u32* p_next_chain, PF_STR* p_filename, u32* p_pos);
s32 VFiPFENT_allocateEntry(PF_DIR_ENT* p_ent, u8 num_entries, PF_FFD* p_ffd, u32* p_next_chain, PF_STR* p_filename);
s32 VFiPFENT_GetRootDir(PF_VOLUME* p_vol, PF_DIR_ENT* p_ent);
s32 VFiPFENT_MakeRootDir(PF_VOLUME* p_vol);
s32 VFiPFENT_updateEntry(PF_DIR_ENT* p_ent, u32 flag);
s32 VFiPFENT_AdjustSFN(PF_DIR_ENT* p_ent, s8* p_short_name);
s32 VFiPFENT_RemoveEntry(PF_DIR_ENT* p_ent, PF_ENT_ITER* p_iter);
u8 VFiPFENT_getcurrentDateTimeForEnt(u16* p_date, u16* p_time);

#ifdef __cplusplus
}
#endif
#endif
