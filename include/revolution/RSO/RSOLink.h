#ifndef RVL_SDK_RSO_LINK_H
#define RVL_SDK_RSO_LINK_H
#include <types.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef u32 RSOHash;

// How much of a module's relocation data may be discarded after linking
typedef enum {
    RSO_FL_NON = 0,
    RSO_FL_INTERNAL,
    RSO_FL_EXTERNAL,
} RSOFixedLevel;

typedef struct RSOObjectInfo RSOObjectInfo;

typedef struct RSOObjectLink {
    RSOObjectInfo* next; // at 0x0
    RSOObjectInfo* prev; // at 0x4
} RSOObjectLink;

typedef struct RSOObjectList {
    RSOObjectInfo* head; // at 0x0
    RSOObjectInfo* tail; // at 0x4
} RSOObjectList;

struct RSOObjectInfo {
    RSOObjectLink link;    // at 0x0
    u32 numSections;       // at 0x8
    u32 sectionInfoOffset; // at 0xC
    u32 nameOffset;        // at 0x10
    u32 nameSize;          // at 0x14
    u32 version;           // at 0x18
};

typedef struct RSOSymbolHeader {
    u32 tableOffset;  // at 0x0
    u32 tableSize;    // at 0x4
    u32 stringOffset; // at 0x8
} RSOSymbolHeader;

typedef struct RSOObjectHeader {
    RSOObjectInfo info;    // at 0x0
    u32 bssSize;           // at 0x1C
    u8 prologSection;      // at 0x20
    u8 epilogSection;      // at 0x21
    u8 unresolvedSection;  // at 0x22
    u8 bssSection;         // at 0x23
    u32 prolog;            // at 0x24
    u32 epilog;            // at 0x28
    u32 unresolved;        // at 0x2C
    u32 internalRelOffset; // at 0x30
    u32 internalRelSize;   // at 0x34
    u32 externalRelOffset; // at 0x38
    u32 externalRelSize;   // at 0x3C
    RSOSymbolHeader expHeader; // at 0x40
    RSOSymbolHeader impHeader; // at 0x4C
} RSOObjectHeader;

typedef struct RSOSectionInfo {
    u32 offset; // at 0x0
    u32 size;   // at 0x4
} RSOSectionInfo;

typedef struct RSOImportTable {
    u32 strOffset; // at 0x0
    u32 value;     // at 0x4
    u32 relOffset; // at 0x8
} RSOImportTable;

typedef struct RSOExportTable {
    u32 strOffset; // at 0x0
    u32 value;     // at 0x4
    u32 section;   // at 0x8
    RSOHash hash;  // at 0xC
} RSOExportTable;

typedef struct RSORel {
    u32 offset; // at 0x0
    u32 info;   // at 0x4
    u32 addend; // at 0x8
} RSORel;

void RSONotifyModuleLoaded(RSOObjectHeader* moduleHeader);
void RSONotifyPreRSOLink(RSOObjectHeader* rsoImp, const RSOObjectHeader* rsoExp);
void RSONotifyPostRSOLink(RSOObjectHeader* rsoImp, const RSOObjectHeader* rsoExp);

BOOL RSOStaticLocateObject(void* newModule);

int RSOLink(RSOObjectHeader* rsoImp, const RSOObjectHeader* rsoExp);

void* RSOFindExportSymbolAddr(const RSOObjectHeader* rso, const char* name);
RSOExportTable* RSOFindExportSymbol(const RSOObjectHeader* rso, const char* name);

BOOL RSOListInit(void* staticRso);
BOOL RSOLinkList(void* newRso, void* bss);

u32 RSOGetFixedSize(const RSOObjectHeader* rso, RSOFixedLevel fixedLevel);

#ifdef __cplusplus
}
#endif
#endif
