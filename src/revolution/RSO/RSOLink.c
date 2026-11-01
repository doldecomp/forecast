// Based on RSOLink.c from the Super Mario Galaxy decompilation
// (SMGCommunity/Petari, src/RVL_SDK/rso/RSOLink.c, CC0).
// This channel's build has fewer functions (no far-jump linking), and some of
// them are exported under the names the channel's own code uses.
#include <revolution/OS.h>
#include <revolution/RSO.h>

#include <cstring>

typedef struct RSORel {
    u32 offset;
    u32 info;
    u32 addend;
} RSORel;

// Head and tail of the module list live in low memory
#define RSO_STATIC_LIST ((RSOObjectList*)0x80003168)

// Hooks for a debugger; empty in retail builds
void RSONotifyModuleLoaded(RSOObjectHeader* rso) DECOMP_DONT_INLINE {}

void RSONotifyPreRSOLink(RSOObjectHeader* rsoImp, const RSOObjectHeader* rsoExp) DECOMP_DONT_INLINE {}

void RSONotifyPostRSOLink(RSOObjectHeader* rsoImp, const RSOObjectHeader* rsoExp) DECOMP_DONT_INLINE {}

static void RSOUnresolveImportSymbolAll(RSOObjectHeader* rso);
void RSORelocate(RSORel* rel, int index, u32 offset);
void RSORelocateSmallDataSection(RSOObjectHeader* rsoImp, int impIndex, RSOObjectHeader* rsoExp);
int FindExportIndex(const RSOObjectHeader* rso, const char* name);

// Turns the offsets of a freshly loaded module into addresses, applies its
// internal relocations and points all of its imports at the unresolved handler
int LocateObject(void* newModule, void* bss, int fixLevel) {
    u32 i;
    RSOSectionInfo* si;
    RSOObjectHeader* moduleHeader;
    RSORel* intRel;
    u32 count;

    moduleHeader = (RSOObjectHeader*)newModule;
    moduleHeader->mBssSection = 0;
    moduleHeader->mInfo.mSectionInfoOffset += (u32)newModule;
    moduleHeader->mInfo.mNameOffset += (u32)newModule;
    moduleHeader->mInternalRelOffset += (u32)newModule;
    moduleHeader->mExternalRelOffset += (u32)newModule;
    moduleHeader->mExpHeader.mTableOffset += (u32)newModule;
    moduleHeader->mExpHeader.mStringOffset += (u32)newModule;
    moduleHeader->mImpHeader.mTableOffset += (u32)newModule;
    moduleHeader->mImpHeader.mStringOffset += (u32)newModule;

    for (i = 1; i < moduleHeader->mInfo.mNumSections; i++) {
        si = &((RSOSectionInfo*)moduleHeader->mInfo.mSectionInfoOffset)[i];
        if (si->mOffset != 0) {
            si->mOffset += (u32)newModule;
        } else if (si->mSize != 0) {
            moduleHeader->mBssSection = i;
            si->mOffset = (u32)bss;
        }
    }

    if (moduleHeader->mPrologSection != 0) {
        moduleHeader->mProlog +=
            ((RSOSectionInfo*)moduleHeader->mInfo.mSectionInfoOffset)[moduleHeader->mPrologSection].mOffset;
    }
    if (moduleHeader->mEpilogSection != 0) {
        moduleHeader->mEpilog +=
            ((RSOSectionInfo*)moduleHeader->mInfo.mSectionInfoOffset)[moduleHeader->mEpilogSection].mOffset;
    }
    if (moduleHeader->mUnresolvedSection != 0) {
        moduleHeader->mUnresolved +=
            ((RSOSectionInfo*)moduleHeader->mInfo.mSectionInfoOffset)[moduleHeader->mUnresolvedSection].mOffset;
    }

    count = moduleHeader->mInternalRelSize / sizeof(RSORel);
    for (i = 0; i < count; i++) {
        intRel = &((RSORel*)moduleHeader->mInternalRelOffset)[i];
        intRel->offset += (u32)newModule;
        RSORelocate(intRel, 0, ((RSOSectionInfo*)moduleHeader->mInfo.mSectionInfoOffset)[intRel->info >> 8].mOffset);
    }

    if (fixLevel >= 1) {
        moduleHeader->mInternalRelSize = 0;
    }

    count = moduleHeader->mExternalRelSize / sizeof(RSORel);
    for (i = 0; i < count; i++) {
        ((RSORel*)moduleHeader->mExternalRelOffset)[i].offset += (u32)newModule;
    }

    RSOUnresolveImportSymbolAll(moduleHeader);

    if (fixLevel <= 1) {
        memset(bss, 0, moduleHeader->mBssSize);
    }

    RSONotifyModuleLoaded(moduleHeader);
    return 1;
}

// Same for the static module (the DOL itself): its sections are already in place,
// at the linker's _f_* symbols from <revolution/OS/__ppc_eabi_init.h>
BOOL RSOStaticLocateObject(void* newModule) {
    RSOObjectHeader* moduleHeader;
    u32 i;
    u32 count;
    RSOSectionInfo* si;

    moduleHeader = (RSOObjectHeader*)newModule;
    moduleHeader->mBssSection = 0;
    moduleHeader->mInfo.mSectionInfoOffset += (u32)moduleHeader;
    moduleHeader->mInfo.mNameOffset += (u32)moduleHeader;
    moduleHeader->mInternalRelOffset += (u32)moduleHeader;
    moduleHeader->mExternalRelOffset += (u32)moduleHeader;
    moduleHeader->mExpHeader.mTableOffset += (u32)moduleHeader;
    moduleHeader->mExpHeader.mStringOffset += (u32)moduleHeader;
    moduleHeader->mImpHeader.mTableOffset += (u32)moduleHeader;
    moduleHeader->mImpHeader.mStringOffset += (u32)moduleHeader;

    for (i = 1; i < moduleHeader->mInfo.mNumSections; i++) {
        si = &((RSOSectionInfo*)moduleHeader->mInfo.mSectionInfoOffset)[i];

        switch (i) {
        case 1:
            si->mOffset = (u32)_f_init;
            break;
        case 2:
            si->mOffset = (u32)_f_text;
            break;
        case 3:
            if (si->mSize != 0) {
                OSReport("Warrning! .ctors section[%d]! size=%x\n", i, si->mSize);
            }
            si->mOffset = 0;
            break;
        case 4:
            if (si->mSize != 0) {
                OSReport("Warrning! .dtors section[%d]! size=%x\n", i, si->mSize);
            }
            si->mOffset = 0;
            break;
        case 5:
            si->mOffset = (u32)_f_rodata;
            break;
        case 6:
            si->mOffset = (u32)_f_data;
            break;
        case 7:
            si->mOffset = (u32)_f_bss;
            break;
        case 8:
            si->mOffset = (u32)_f_sdata;
            break;
        case 11:
            si->mOffset = (u32)_f_sbss;
            break;
        case 9:
            si->mOffset = (u32)_f_sdata2;
            break;
        case 12:
            si->mOffset = (u32)_f_sbss2;
            break;
        case 10:
            si->mOffset = 0;
            break;
        case 13:
            si->mOffset = 0;
            break;
        default:
            if (si->mSize != 0) {
                OSReport("Warrning! unknown section[%d]! size=%x\n", i, si->mSize);
            }
            si->mOffset = 0;
            break;
        }
    }

    count = moduleHeader->mExternalRelSize / sizeof(RSORel);
    for (i = 0; i < count; i++) {
        ((RSORel*)moduleHeader->mExternalRelOffset)[i].offset += (u32)moduleHeader;
    }

    return 1;
}

static RSOImportTable* RSOGetImport(const RSOSymbolHeader* imp) {
    return (RSOImportTable*)imp->mTableOffset;
}

static char* RSOGetImportSymbolName(const RSOSymbolHeader* imp, u32 index) {
    RSOImportTable* impTab = &RSOGetImport(imp)[index];
    return (char*)impTab->strOffset + imp->mStringOffset;
}

static int RSOGetNumImportSymbols(const RSOSymbolHeader* imp) {
    return imp->mTableSize / sizeof(RSOImportTable);
}

// Patches every relocation of one import with the import's current value
static void RSORelocateImportSymbol(RSOObjectHeader* rso, RSOImportTable* impTab, int impIndex) {
    RSORel* rel = (RSORel*)(rso->mExternalRelOffset + impTab->relOffset);

    while ((rel->info >> 8) == (u32)impIndex) {
        RSORelocate(rel, 0, impTab->value);
        rel++;
    }
}

static void RSOResolveImportSymbol(RSOObjectHeader* rsoImp, int index, void* addr) {
    RSOImportTable* impTab = &RSOGetImport(&rsoImp->mImpHeader)[index];
    impTab->value = (u32)addr;
    RSORelocateImportSymbol(rsoImp, impTab, index);
}

// Points an import at the module's "unresolved" handler
static void RSOUnresolveImportSymbol(RSOObjectHeader* rsoImp, int index) {
    RSOImportTable* impTab = &RSOGetImport(&rsoImp->mImpHeader)[index];
    impTab->value = rsoImp->mUnresolved;
    RSORelocateImportSymbol(rsoImp, impTab, index);
}

static void RSOUnresolveImportSymbolAll(RSOObjectHeader* rso) {
    int i;
    int count = RSOGetNumImportSymbols(&rso->mImpHeader);

    for (i = 0; i < count; i++) {
        RSOUnresolveImportSymbol(rso, i);
    }
}

// Resolves the imports of rsoImp that rsoExp exports; returns how many were resolved
int RSOLink(RSOObjectHeader* rsoImp, const RSOObjectHeader* rsoExp) {
    RSOSymbolHeader* imp;
    int i, resolved, count;
    const char* impName;
    void* addr;
    RSOImportTable* impTab;

    imp = &rsoImp->mImpHeader;
    resolved = 0;
    count = RSOGetNumImportSymbols(&rsoImp->mImpHeader);
    impTab = (RSOImportTable*)imp->mTableOffset;

    RSONotifyPreRSOLink(rsoImp, rsoExp);

    for (i = 0; i < count; i++, impTab++) {
        impName = (const char*)(impTab->strOffset + imp->mStringOffset);

        if (impName != NULL) {
            RSORelocateSmallDataSection(rsoImp, i, (RSOObjectHeader*)rsoExp);
            addr = RSOFindExportSymbol(rsoExp, impName);

            if (addr != NULL) {
                RSOResolveImportSymbol(rsoImp, i, addr);
                resolved++;
            }
        }
    }

    RSONotifyPostRSOLink(rsoImp, rsoExp);
    return resolved;
}

static int RSOGetNumExportSymbols(const RSOSymbolHeader* exp) {
    return exp->mTableSize / sizeof(RSOExportTable);
}

// ELF symbol hash; the export table is sorted by it
static u32 RSOGetHash(const char* symbolName) {
    u32 h = 0;
    u32 g;

    while (*symbolName != 0) {
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
    RSOExportTable* expTab = &((RSOExportTable*)rso->mExpHeader.mTableOffset)[index];
    RSOSectionInfo* expSec = &((RSOSectionInfo*)rso->mInfo.mSectionInfoOffset)[expTab->section];
    return (void*)(expSec->mOffset + expTab->value);
}

static char* RSOGetExportSymbolName(const RSOSymbolHeader* exp, int index) {
    RSOExportTable* expTab = (RSOExportTable*)exp->mTableOffset + index;
    return (char*)expTab->strOffset + exp->mStringOffset;
}

// Binary search of the export table by hash, then a linear scan over equal hashes
int FindExportIndex(const RSOObjectHeader* rso, const char* name) {
    u32 hash = RSOGetHash(name);
    u32 i;
    const char* expName;
    int count = RSOGetNumExportSymbols(&rso->mExpHeader);
    RSOExportTable* expTab = (RSOExportTable*)rso->mExpHeader.mTableOffset;
    RSOExportTable* entry;
    int top = 0;
    int last = count - 1;
    int index = -1;

    if (count <= 0) {
        return -1;
    }

    while (index == -1) {
        i = (top + last) >> 1;
        entry = &expTab[i];

        if (hash > entry->hash) {
            if (top == (int)i) {
                index = last;
            } else {
                top = i;
            }
        } else if (hash < entry->hash) {
            if (top == (int)i) {
                index = top;
            } else {
                last = i;
            }
        } else {
            index = i;
        }
    }

    entry = &expTab[index];
    if (hash != entry->hash) {
        return -1;
    }

    expName = RSOGetExportSymbolName(&rso->mExpHeader, index);
    if (!strcmp(name, expName)) {
        return index;
    }

    for (i = index + 1; (int)i <= last; i++) {
        entry = &expTab[i];
        if (hash == entry->hash) {
            expName = RSOGetExportSymbolName(&rso->mExpHeader, i);
            if (!strcmp(name, expName)) {
                return i;
            }
        } else {
            i = last + 1;
        }
    }

    for (i = index - 1; (int)i >= top; i--) {
        entry = &expTab[i];
        if (hash == entry->hash) {
            expName = RSOGetExportSymbolName(&rso->mExpHeader, i);
            if (!strcmp(name, expName)) {
                return i;
            }
        } else {
            return -1;
        }
    }

    return -1;
}

// Returns the address of an exported symbol, or NULL
void* RSOFindExportSymbol(const RSOObjectHeader* rso, const char* name) {
    int index = FindExportIndex(rso, name);

    if (index == -1) {
        return NULL;
    } else {
        return RSOGetExportSymbolAddr(rso, index);
    }
}

// Returns the export table entry of a symbol, or NULL
RSOExportTable* RSOFindSymbol(const RSOObjectHeader* rso, const char* name) {
    int index = FindExportIndex(rso, name);

    if (index == -1) {
        return NULL;
    } else {
        return &((RSOExportTable*)rso->mExpHeader.mTableOffset)[index];
    }
}

void RSORelocate(RSORel* rel, int index, u32 offset) {
    u32* p;
    u32 x;
    u32 y;
    RSORel* targetRel = &rel[index];

    p = (u32*)targetRel->offset;

    switch ((u8)rel->info) {
    case 0:
        break;
    case 1:
        *p = offset + rel->addend;
        break;
    case 2:
        *p = (*p & 0xFC000003) | ((offset + rel->addend) & 0x03FFFFFC);
        break;
    case 3:
        *(u16*)p = (u16)(offset + rel->addend);
        break;
    case 4:
        *(u16*)p = (u16)(offset + rel->addend);
        break;
    case 5:
        *(u16*)p = (u16)((offset + rel->addend) >> 16);
        break;
    case 6:
        x = offset + rel->addend;
        *(u16*)p = (u16)((x >> 16) + ((x >> 15) & 1));
        break;
    case 7:
    case 8:
    case 9:
        *p = (*p & 0xFFFF0003) | ((offset + rel->addend) & 0xFFFC);
        break;
    case 10:
        x = offset + rel->addend - (u32)p;
        *p = (*p & 0xFC000003) | (x & 0x03FFFFFC);
        x = offset + rel->addend;
        y = ((u32)p & 0xFC000003) | ((((u32)p & 0x03FFFFFC) + (*p & 0x03FFFFFC)) & 0x03FFFFFC);
        if (x != 0 && x != y) {
            *p = (*p & 0xFC000003) | 0x03FFFFFC;
        }
        break;
    case 11:
    case 12:
    case 13:
        *p = (*p & 0xFFFF0003) | (((offset + rel->addend) - (u32)p) & 0xFFFC);
        break;
    case 109:
        break;
    default:
        OSReport("OSLink: unknown relocation type %3d\n", (u8)rel->info);
        break;
    }

    DCFlushRange(p, 0x20);
    ICInvalidateRange(p, 0x20);
}

// Rewrites the base register and offset of R_PPC_EMB_SDA21 relocations
void RSORelocateSmallDataSection(RSOObjectHeader* rsoImp, int impIndex, RSOObjectHeader* rsoExp) {
    u8* p;
    u32 base;
    const char* impName;
    RSOImportTable* impTab;
    RSOExportTable* expTab;
    RSOExportTable* baseTab;
    RSORel* rel;
    RSOSectionInfo* si;

    impTab = &RSOGetImport(&rsoImp->mImpHeader)[impIndex];
    impName = RSOGetImportSymbolName(&rsoImp->mImpHeader, impIndex);
    rel = (RSORel*)((u8*)rsoImp->mExternalRelOffset + impTab->relOffset);

    while ((rel->info >> 8) == (u32)impIndex) {
        if ((u8)rel->info == 109) {
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

            expTab = RSOFindSymbol(rsoExp, impName);
            if (expTab != NULL) {
                switch (expTab->section) {
                case 8:
                case 11:
                    *p = (*p & ~0x1F) | 0xD;
                    baseTab = RSOFindSymbol(rsoExp, "_SDA_BASE_");
                    if (baseTab == NULL) {
                        rel++;
                        continue;
                    }
                    base = baseTab->value;
                    break;
                case 9:
                case 12:
                    *p = (*p & ~0x1F) | 2;
                    baseTab = RSOFindSymbol(rsoExp, "_SDA2_BASE_");
                    if (baseTab == NULL) {
                        rel++;
                        continue;
                    }
                    base = baseTab->value;
                    break;
                case 10:
                case 13:
                    base = 0;
                    *p &= 0xE0;
                    break;
                case 0xF1:
                    OSReport("ERROR: incorrect R_PPC_EMB_SDA21 data.\n");
                    break;
                default:
                    OSReport("ERROR: incorrect R_PPC_EMB_SDA21 data.\n");
                    break;
                }

                si = &((RSOSectionInfo*)rsoExp->mInfo.mSectionInfoOffset)[expTab->section];
                *(u16*)(p + 1) = (u16)((si->mOffset + expTab->value + rel->addend) - base);
                DCFlushRange(p, 0x20);
                ICInvalidateRange(p, 0x20);
            }
        }

        rel++;
    }
}

// Starts the module list with the static module (the DOL itself)
BOOL RSOLinkInit(void* staticRso) {
    int result;
    RSOObjectInfo* prev;

    RSO_STATIC_LIST->mTail = NULL;
    RSO_STATIC_LIST->mHead = NULL;

    result = RSOStaticLocateObject(staticRso);

    prev = RSO_STATIC_LIST->mTail;
    if (prev == NULL) {
        RSO_STATIC_LIST->mHead = (RSOObjectInfo*)staticRso;
    } else {
        prev->mLink.mNext = (RSOObjectInfo*)staticRso;
    }

    ((RSOObjectInfo*)staticRso)->mLink.mPrev = prev;
    ((RSOObjectInfo*)staticRso)->mLink.mNext = NULL;
    RSO_STATIC_LIST->mTail = (RSOObjectInfo*)staticRso;

    return result;
}

// Locates a new module, links it both ways with every listed module and adds it to the list
int LinkList(void* newRso, void* bss, int fixLevel) {
    int result;
    RSOObjectHeader* rso;
    RSOObjectInfo* target;
    RSOObjectInfo* prev;

    result = LocateObject(newRso, bss, fixLevel);
    if (result == 0) {
        return 0;
    }

    target = RSO_STATIC_LIST->mHead;
    while (target != NULL) {
        RSOLink((RSOObjectHeader*)newRso, (RSOObjectHeader*)target);
        RSOLink((RSOObjectHeader*)target, (RSOObjectHeader*)newRso);
        target = target->mLink.mNext;
    }

    prev = RSO_STATIC_LIST->mTail;
    if (prev == NULL) {
        RSO_STATIC_LIST->mHead = (RSOObjectInfo*)newRso;
    } else {
        prev->mLink.mNext = (RSOObjectInfo*)newRso;
    }

    ((RSOObjectInfo*)newRso)->mLink.mPrev = prev;
    ((RSOObjectInfo*)newRso)->mLink.mNext = NULL;
    RSO_STATIC_LIST->mTail = (RSOObjectInfo*)newRso;

    // Fully fixed: the bss is cleared here and the link tables are dropped
    if (fixLevel >= 2) {
        rso = (RSOObjectHeader*)newRso;
        memset(bss, 0, rso->mBssSize);
        rso->mExternalRelSize = 0;
        rso->mImpHeader.mTableSize = 0;
    }

    return 1;
}

BOOL RSOSetBss(void* newRso, void* bss) {
    return LinkList(newRso, bss, 0);
}

// Size the module file can be cut down to once it is linked with a fix level:
// up to its internal relocations at level 1, up to its external relocations at
// level 2. 0 means it cannot be cut (level 0).
u32 RSOGetSectionSize(const RSOObjectHeader* rso, int fixLevel) {
    switch (fixLevel) {
    case 0:
        return 0;
    case 1:
        return rso->mInternalRelOffset;
    case 2:
        return rso->mExternalRelOffset;
    }

    return 0;
}
