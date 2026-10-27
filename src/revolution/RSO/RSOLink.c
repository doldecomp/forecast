#include <cstring>
#include <revolution/OS.h>
#include <revolution/RSO.h>

// Section indices of an RSO module
#define RSO_SECTION_INIT 1
#define RSO_SECTION_TEXT 2
#define RSO_SECTION_CTORS 3
#define RSO_SECTION_DTORS 4
#define RSO_SECTION_RODATA 5
#define RSO_SECTION_DATA 6
#define RSO_SECTION_BSS 7
#define RSO_SECTION_SDATA 8
#define RSO_SECTION_SDATA2 9
#define RSO_SECTION_EMB_SDATA0 10
#define RSO_SECTION_SBSS 11
#define RSO_SECTION_SBSS2 12
#define RSO_SECTION_EMB_SBSS0 13
#define RSO_SECTION_EXTABINDEX 241

// PowerPC ELF relocation types
#define R_PPC_NONE 0
#define R_PPC_ADDR32 1
#define R_PPC_ADDR24 2
#define R_PPC_ADDR16 3
#define R_PPC_ADDR16_LO 4
#define R_PPC_ADDR16_HI 5
#define R_PPC_ADDR16_HA 6
#define R_PPC_ADDR14 7
#define R_PPC_ADDR14_BRTAKEN 8
#define R_PPC_ADDR14_BRNTAKEN 9
#define R_PPC_REL24 10
#define R_PPC_REL14 11
#define R_PPC_REL14_BRTAKEN 12
#define R_PPC_REL14_BRNTAKEN 13
#define R_PPC_EMB_SDA21 109

#define ELF32_R_SYM(info) ((info) >> 8)
#define ELF32_R_TYPE(info) ((u8)(info))

// The list of linked modules lives in the OS globals
#define __RSOObjectInfoList (*(RSOObjectList*)0x80003168)

#define RSO_SECTION_INFO(rso) ((RSOSectionInfo*)(rso)->info.sectionInfoOffset)

static void RSOUnresolveImportSymbolAll(RSOObjectHeader* rso);
static int FindExportIndex(const RSOObjectHeader* rso, const char* name);
static void RSORelocate(RSORel* rel, int index, u32 offset);
static void RSORelocateSmallDataSection(RSOObjectHeader* rsoImp, int impIndex,
                                        const RSOObjectHeader* rsoExp);

#pragma push
#pragma dont_inline on
void RSONotifyModuleLoaded(RSOObjectHeader* moduleHeader) {}

void RSONotifyPreRSOLink(RSOObjectHeader* rsoImp, const RSOObjectHeader* rsoExp) {}

void RSONotifyPostRSOLink(RSOObjectHeader* rsoImp, const RSOObjectHeader* rsoExp) {}
#pragma pop

static BOOL LocateObject(void* newModule, void* bss, RSOFixedLevel fixedLevel) {
    u32 i;
    RSOSectionInfo* si;
    RSOObjectHeader* moduleHeader;
    RSORel* intRel;
    RSORel* extRel;
    u32 a_max;

    moduleHeader = (RSOObjectHeader*)newModule;
    moduleHeader->bssSection = 0;
    moduleHeader->info.sectionInfoOffset += (u32)newModule;
    moduleHeader->info.nameOffset += (u32)newModule;
    moduleHeader->internalRelOffset += (u32)newModule;
    moduleHeader->externalRelOffset += (u32)newModule;
    moduleHeader->expHeader.tableOffset += (u32)newModule;
    moduleHeader->expHeader.stringOffset += (u32)newModule;
    moduleHeader->impHeader.tableOffset += (u32)newModule;
    moduleHeader->impHeader.stringOffset += (u32)newModule;

    for (i = RSO_SECTION_INIT; i < moduleHeader->info.numSections; i++) {
        si = &RSO_SECTION_INFO(moduleHeader)[i];
        if (si->offset != 0) {
            si->offset += (u32)newModule;
        } else if (si->size != 0) {
            moduleHeader->bssSection = i;
            si->offset = (u32)bss;
        }
    }

    if (moduleHeader->prologSection != 0) {
        moduleHeader->prolog += RSO_SECTION_INFO(moduleHeader)[moduleHeader->prologSection].offset;
    }
    if (moduleHeader->epilogSection != 0) {
        moduleHeader->epilog += RSO_SECTION_INFO(moduleHeader)[moduleHeader->epilogSection].offset;
    }
    if (moduleHeader->unresolvedSection != 0) {
        moduleHeader->unresolved +=
            RSO_SECTION_INFO(moduleHeader)[moduleHeader->unresolvedSection].offset;
    }

    a_max = moduleHeader->internalRelSize / sizeof(RSORel);
    for (i = 0; i < a_max; i++) {
        intRel = &((RSORel*)moduleHeader->internalRelOffset)[i];
        intRel->offset += (u32)newModule;
        RSORelocate(intRel, 0, RSO_SECTION_INFO(moduleHeader)[ELF32_R_SYM(intRel->info)].offset);
    }

    if (fixedLevel >= RSO_FL_INTERNAL) {
        moduleHeader->internalRelSize = 0;
    }

    a_max = moduleHeader->externalRelSize / sizeof(RSORel);
    for (i = 0; i < a_max; i++) {
        extRel = &((RSORel*)moduleHeader->externalRelOffset)[i];
        extRel->offset += (u32)newModule;
    }

    RSOUnresolveImportSymbolAll(moduleHeader);

    if (fixedLevel <= RSO_FL_INTERNAL) {
        memset(bss, 0, moduleHeader->bssSize);
    }

    RSONotifyModuleLoaded(moduleHeader);
    return TRUE;
}

BOOL RSOStaticLocateObject(void* newModule) {
    RSOObjectHeader* moduleHeader;
    u32 i;
    u32 a_max;
    RSOSectionInfo* si;
    RSORel* extRel;

    moduleHeader = (RSOObjectHeader*)newModule;
    moduleHeader->bssSection = 0;
    moduleHeader->info.sectionInfoOffset += (u32)moduleHeader;
    moduleHeader->info.nameOffset += (u32)moduleHeader;
    moduleHeader->internalRelOffset += (u32)moduleHeader;
    moduleHeader->externalRelOffset += (u32)moduleHeader;
    moduleHeader->expHeader.tableOffset += (u32)moduleHeader;
    moduleHeader->expHeader.stringOffset += (u32)moduleHeader;
    moduleHeader->impHeader.tableOffset += (u32)moduleHeader;
    moduleHeader->impHeader.stringOffset += (u32)moduleHeader;

    for (i = RSO_SECTION_INIT; i < moduleHeader->info.numSections; i++) {
        si = &RSO_SECTION_INFO(moduleHeader)[i];

        switch (i) {
        case RSO_SECTION_INIT:
            si->offset = (u32)_f_init;
            break;
        case RSO_SECTION_TEXT:
            si->offset = (u32)_f_text;
            break;
        case RSO_SECTION_CTORS:
            if (si->size != 0) {
                OSReport("Warrning! .ctors section[%d]! size=%x\n", i, si->size);
            }
            si->offset = 0;
            break;
        case RSO_SECTION_DTORS:
            if (si->size != 0) {
                OSReport("Warrning! .dtors section[%d]! size=%x\n", i, si->size);
            }
            si->offset = 0;
            break;
        case RSO_SECTION_RODATA:
            si->offset = (u32)_f_rodata;
            break;
        case RSO_SECTION_DATA:
            si->offset = (u32)_f_data;
            break;
        case RSO_SECTION_BSS:
            si->offset = (u32)_f_bss;
            break;
        case RSO_SECTION_SDATA:
            si->offset = (u32)_f_sdata;
            break;
        case RSO_SECTION_SBSS:
            si->offset = (u32)_f_sbss;
            break;
        case RSO_SECTION_SDATA2:
            si->offset = (u32)_f_sdata2;
            break;
        case RSO_SECTION_SBSS2:
            si->offset = (u32)_f_sbss2;
            break;
        case RSO_SECTION_EMB_SDATA0:
            si->offset = 0;
            break;
        case RSO_SECTION_EMB_SBSS0:
            si->offset = 0;
            break;
        default:
            if (si->size != 0) {
                OSReport("Warrning! unknown section[%d]! size=%x\n", i, si->size);
            }
            si->offset = 0;
            break;
        }
    }

    a_max = moduleHeader->externalRelSize / sizeof(RSORel);
    for (i = 0; i < a_max; i++) {
        extRel = &((RSORel*)moduleHeader->externalRelOffset)[i];
        extRel->offset += (u32)moduleHeader;
    }

    return TRUE;
}

static RSOImportTable* RSOGetImport(const RSOSymbolHeader* imp) {
    return (RSOImportTable*)imp->tableOffset;
}

static char* RSOGetImportSymbolName(const RSOSymbolHeader* imp, u32 index) {
    RSOImportTable* impTab = &RSOGetImport(imp)[index];
    return (char*)(impTab->strOffset + imp->stringOffset);
}

static int RSOGetNumImportSymbols(const RSOSymbolHeader* imp) {
    return imp->tableSize / sizeof(RSOImportTable);
}

static void RSORelocateImportSymbol(RSOObjectHeader* rso, RSOImportTable* impTab, int impIndex) {
    RSORel* rel = (RSORel*)(rso->externalRelOffset + impTab->relOffset);

    while (ELF32_R_SYM(rel->info) == (u32)impIndex) {
        RSORelocate(rel, 0, impTab->value);
        rel++;
    }
}

static void RSOResolveImportSymbol(RSOObjectHeader* rsoImp, int index, void* addr) {
    RSOImportTable* impTab = &RSOGetImport(&rsoImp->impHeader)[index];
    impTab->value = (u32)addr;
    RSORelocateImportSymbol(rsoImp, impTab, index);
}

static void RSOUnresolveImportSymbol(RSOObjectHeader* rsoImp, int index) {
    RSOImportTable* impTab = &RSOGetImport(&rsoImp->impHeader)[index];
    impTab->value = rsoImp->unresolved;
    RSORelocateImportSymbol(rsoImp, impTab, index);
}

static void RSOUnresolveImportSymbolAll(RSOObjectHeader* rso) {
    int i;
    int s_max = RSOGetNumImportSymbols(&rso->impHeader);

    for (i = 0; i < s_max; i++) {
        RSOUnresolveImportSymbol(rso, i);
    }
}

int RSOLink(RSOObjectHeader* rsoImp, const RSOObjectHeader* rsoExp) {
    RSOSymbolHeader* imp;
    int i, count, s_max;
    const char* impName;
    void* addr;
    RSOImportTable* impTab;

    imp = &rsoImp->impHeader;
    count = 0;
    s_max = RSOGetNumImportSymbols(&rsoImp->impHeader);
    impTab = (RSOImportTable*)imp->tableOffset;

    RSONotifyPreRSOLink(rsoImp, rsoExp);

    for (i = 0; i < s_max; i++, impTab++) {
        impName = (const char*)(impTab->strOffset + imp->stringOffset);

        if (impName != NULL) {
            RSORelocateSmallDataSection(rsoImp, i, rsoExp);
            addr = RSOFindExportSymbolAddr(rsoExp, impName);

            if (addr != NULL) {
                RSOResolveImportSymbol(rsoImp, i, addr);
                count++;
            }
        }
    }

    RSONotifyPostRSOLink(rsoImp, rsoExp);
    return count;
}

static int RSOGetNumExportSymbols(const RSOSymbolHeader* exp) {
    return exp->tableSize / sizeof(RSOExportTable);
}

static RSOHash RSOGetHash(const char* symbolName) {
    RSOHash h = 0;
    RSOHash g;

    while (*symbolName != '\0') {
        h = (h << 4) + *symbolName++;
        g = h & 0xF0000000;
        if (g != 0) {
            h ^= g >> 24;
        }
        h &= ~g;
    }

    return h;
}

static void* RSOGetExportSymbolAddr(const RSOObjectHeader* rso, int index) {
    RSOExportTable* expTab = &((RSOExportTable*)rso->expHeader.tableOffset)[index];
    RSOSectionInfo* expSec = &RSO_SECTION_INFO(rso)[expTab->section];
    return (void*)(expSec->offset + expTab->value);
}

static char* RSOGetExportSymbolName(const RSOSymbolHeader* exp, int index) {
    RSOExportTable* expTab = (RSOExportTable*)exp->tableOffset + index;
    return (char*)(expTab->strOffset + exp->stringOffset);
}

static int FindExportIndex(const RSOObjectHeader* rso, const char* name) {
    RSOHash a_hash = RSOGetHash(name);
    u32 i;
    const char* expName;
    int s_max = RSOGetNumExportSymbols(&rso->expHeader);
    RSOExportTable* expTab = (RSOExportTable*)rso->expHeader.tableOffset;
    RSOExportTable* a_expTab;
    int a_top = 0;
    int a_last = s_max - 1;
    int a_idx = -1;

    if (s_max <= 0) {
        return -1;
    }

    // Binary search on the sorted hashes, then compare the names of the entries sharing that hash
    while (a_idx == -1) {
        i = (a_top + a_last) >> 1;
        a_expTab = &expTab[i];

        if (a_hash > a_expTab->hash) {
            if (a_top == (int)i) {
                a_idx = a_last;
            } else {
                a_top = i;
            }
        } else if (a_hash < a_expTab->hash) {
            if (a_top == (int)i) {
                a_idx = a_top;
            } else {
                a_last = i;
            }
        } else {
            a_idx = i;
        }
    }

    a_expTab = &expTab[a_idx];
    if (a_hash != a_expTab->hash) {
        return -1;
    }

    expName = RSOGetExportSymbolName(&rso->expHeader, a_idx);
    if (strcmp(name, expName) == 0) {
        return a_idx;
    }

    for (i = a_idx + 1; (int)i <= a_last; i++) {
        a_expTab = &expTab[i];
        if (a_hash == a_expTab->hash) {
            expName = RSOGetExportSymbolName(&rso->expHeader, i);
            if (strcmp(name, expName) == 0) {
                return i;
            }
        } else {
            i = a_last + 1;
        }
    }

    for (i = a_idx - 1; (int)i >= a_top; i--) {
        a_expTab = &expTab[i];
        if (a_hash == a_expTab->hash) {
            expName = RSOGetExportSymbolName(&rso->expHeader, i);
            if (strcmp(name, expName) == 0) {
                return i;
            }
        } else {
            return -1;
        }
    }

    return -1;
}

void* RSOFindExportSymbolAddr(const RSOObjectHeader* rso, const char* name) {
    int a_idx = FindExportIndex(rso, name);

    if (a_idx == -1) {
        return NULL;
    } else {
        return RSOGetExportSymbolAddr(rso, a_idx);
    }
}

RSOExportTable* RSOFindExportSymbol(const RSOObjectHeader* rso, const char* name) {
    int a_idx = FindExportIndex(rso, name);

    if (a_idx == -1) {
        return NULL;
    } else {
        return &((RSOExportTable*)rso->expHeader.tableOffset)[a_idx];
    }
}

static void RSORelocate(RSORel* rel, int index, u32 offset) {
    u32* p;
    u32 x;
    u32 y;
    RSORel* targetRel = &rel[index];

    p = (u32*)targetRel->offset;

    switch (ELF32_R_TYPE(rel->info)) {
    case R_PPC_NONE:
        break;
    case R_PPC_ADDR32:
        *p = offset + rel->addend;
        break;
    case R_PPC_ADDR24:
        *p = (*p & 0xFC000003) | ((offset + rel->addend) & 0x03FFFFFC);
        break;
    case R_PPC_ADDR16:
        *(u16*)p = (u16)(offset + rel->addend);
        break;
    case R_PPC_ADDR16_LO:
        *(u16*)p = (u16)(offset + rel->addend);
        break;
    case R_PPC_ADDR16_HI:
        *(u16*)p = (u16)((offset + rel->addend) >> 16);
        break;
    case R_PPC_ADDR16_HA:
        x = offset + rel->addend;
        *(u16*)p = (u16)((x >> 16) + ((x >> 15) & 1));
        break;
    case R_PPC_ADDR14:
    case R_PPC_ADDR14_BRTAKEN:
    case R_PPC_ADDR14_BRNTAKEN:
        *p = (*p & 0xFFFF0003) | ((offset + rel->addend) & 0xFFFC);
        break;
    case R_PPC_REL24:
        x = offset + rel->addend - (u32)p;
        *p = (*p & 0xFC000003) | (x & 0x03FFFFFC);
        // If the target is out of branch range, store a displacement of -4 instead
        x = offset + rel->addend;
        y = ((u32)p & 0xFC000003) | ((((u32)p & 0x03FFFFFC) + (*p & 0x03FFFFFC)) & 0x03FFFFFC);
        if (x != 0 && x != y) {
            *p = (*p & 0xFC000003) | 0x03FFFFFC;
        }
        break;
    case R_PPC_REL14:
    case R_PPC_REL14_BRTAKEN:
    case R_PPC_REL14_BRNTAKEN:
        *p = (*p & 0xFFFF0003) | (((offset + rel->addend) - (u32)p) & 0xFFFC);
        break;
    case R_PPC_EMB_SDA21:
        // Handled by RSORelocateSmallDataSection
        break;
    default:
        OSReport("OSLink: unknown relocation type %3d\n", ELF32_R_TYPE(rel->info));
        break;
    }

    DCFlushRange(p, 32);
    ICInvalidateRange(p, 32);
}

static void RSORelocateSmallDataSection(RSOObjectHeader* rsoImp, int impIndex,
                                        const RSOObjectHeader* rsoExp) {
    u8* p;
    u32 base;
    const char* impName;
    RSOImportTable* impTab;
    RSOExportTable* expTab;
    RSOExportTable* baseTab;
    RSORel* rel;
    RSOSectionInfo* si;

    impTab = &RSOGetImport(&rsoImp->impHeader)[impIndex];
    impName = RSOGetImportSymbolName(&rsoImp->impHeader, impIndex);
    rel = (RSORel*)(rsoImp->externalRelOffset + impTab->relOffset);

    while (ELF32_R_SYM(rel->info) == (u32)impIndex) {
        if (ELF32_R_TYPE(rel->info) == R_PPC_EMB_SDA21) {
            // Point at the instruction's second byte, which holds the rA field
            p = (u8*)rel->offset;
            switch ((u32)p & 3) {
            case 0:
                p += 1;
                break;
            case 2:
                p -= 1;
                break;
            case 3:
                p -= 2;
                break;
            }

            expTab = RSOFindExportSymbol(rsoExp, impName);
            if (expTab != NULL) {
                switch (expTab->section) {
                case RSO_SECTION_SDATA:
                case RSO_SECTION_SBSS:
                    *p = (*p & ~0x1F) | 13;
                    baseTab = RSOFindExportSymbol(rsoExp, "_SDA_BASE_");
                    if (baseTab == NULL) {
                        rel++;
                        continue;
                    }
                    base = baseTab->value;
                    break;
                case RSO_SECTION_SDATA2:
                case RSO_SECTION_SBSS2:
                    *p = (*p & ~0x1F) | 2;
                    baseTab = RSOFindExportSymbol(rsoExp, "_SDA2_BASE_");
                    if (baseTab == NULL) {
                        rel++;
                        continue;
                    }
                    base = baseTab->value;
                    break;
                case RSO_SECTION_EMB_SDATA0:
                case RSO_SECTION_EMB_SBSS0:
                    base = 0;
                    *p &= ~0x1F;
                    break;
                case RSO_SECTION_EXTABINDEX:
                    OSReport("ERROR: incorrect R_PPC_EMB_SDA21 data.\n");
                    break;
                default:
                    OSReport("ERROR: incorrect R_PPC_EMB_SDA21 data.\n");
                    break;
                }

                si = &RSO_SECTION_INFO(rsoExp)[expTab->section];
                *(u16*)(p + 1) = (u16)((si->offset + expTab->value + rel->addend) - base);
                DCFlushRange(p, 32);
                ICInvalidateRange(p, 32);
            }
        }

        rel++;
    }
}

BOOL RSOListInit(void* staticRso) {
    BOOL r;
    RSOObjectInfo* prev;

    __RSOObjectInfoList.head = __RSOObjectInfoList.tail = NULL;

    r = RSOStaticLocateObject(staticRso);

    prev = __RSOObjectInfoList.tail;
    if (prev == NULL) {
        __RSOObjectInfoList.head = (RSOObjectInfo*)staticRso;
    } else {
        prev->link.next = (RSOObjectInfo*)staticRso;
    }
    ((RSOObjectInfo*)staticRso)->link.prev = prev;
    ((RSOObjectInfo*)staticRso)->link.next = NULL;
    __RSOObjectInfoList.tail = (RSOObjectInfo*)staticRso;

    return r;
}

static BOOL LinkList(void* newRso, void* bss, RSOFixedLevel fixedLevel) {
    RSOObjectHeader* a_rsoInfo;
    RSOObjectInfo* a_target;
    RSOObjectInfo* prev;

    if (!LocateObject(newRso, bss, fixedLevel)) {
        return FALSE;
    }

    for (a_target = __RSOObjectInfoList.head; a_target != NULL; a_target = a_target->link.next) {
        RSOLink((RSOObjectHeader*)newRso, (RSOObjectHeader*)a_target);
        RSOLink((RSOObjectHeader*)a_target, (RSOObjectHeader*)newRso);
    }

    prev = __RSOObjectInfoList.tail;
    if (prev == NULL) {
        __RSOObjectInfoList.head = (RSOObjectInfo*)newRso;
    } else {
        prev->link.next = (RSOObjectInfo*)newRso;
    }
    ((RSOObjectInfo*)newRso)->link.prev = prev;
    ((RSOObjectInfo*)newRso)->link.next = NULL;
    __RSOObjectInfoList.tail = (RSOObjectInfo*)newRso;

    if (fixedLevel >= RSO_FL_EXTERNAL) {
        a_rsoInfo = (RSOObjectHeader*)newRso;
        memset(bss, 0, a_rsoInfo->bssSize);
        a_rsoInfo->externalRelSize = 0;
        a_rsoInfo->impHeader.tableSize = 0;
    }

    return TRUE;
}

BOOL RSOLinkList(void* newRso, void* bss) {
    return LinkList(newRso, bss, RSO_FL_NON);
}

u32 RSOGetFixedSize(const RSOObjectHeader* rso, RSOFixedLevel fixedLevel) {
    switch (fixedLevel) {
    case RSO_FL_NON:
        return 0;
    case RSO_FL_INTERNAL:
        return rso->internalRelOffset;
    case RSO_FL_EXTERNAL:
        return rso->externalRelOffset;
    default:
        return 0;
    }
}
