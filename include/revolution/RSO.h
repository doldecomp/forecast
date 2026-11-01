// Based on rso.h from the Super Mario Galaxy decompilation
// (SMGCommunity/Petari, libs/RVL_SDK/include/revolution/rso.h, CC0),
// reduced to what this channel's RSO build has.
#ifndef RVL_SDK_PUBLIC_RSO_H
#define RVL_SDK_PUBLIC_RSO_H
#include <types.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef struct RSOObjectInfo RSOObjectInfo;

typedef struct RSOObjectLink {
    RSOObjectInfo* mNext;  // 0x00
    RSOObjectInfo* mPrev;  // 0x04
} RSOObjectLink;

// Head and tail of the list of linked modules
typedef struct RSOObjectList {
    RSOObjectInfo* mHead;  // 0x00
    RSOObjectInfo* mTail;  // 0x04
} RSOObjectList;

struct RSOObjectInfo {
    RSOObjectLink mLink;     // 0x00
    u32 mNumSections;        // 0x08
    u32 mSectionInfoOffset;  // 0x0C
    u32 mNameOffset;         // 0x10
    u32 mNameSize;           // 0x14
    u32 mVersion;            // 0x18
};

typedef struct RSOSymbolHeader {
    u32 mTableOffset;   // 0x00
    u32 mTableSize;     // 0x04
    u32 mStringOffset;  // 0x08
} RSOSymbolHeader;

typedef struct RSOObjectHeader {
    RSOObjectInfo mInfo;       // 0x00
    u32 mBssSize;              // 0x1C
    u8 mPrologSection;         // 0x20
    u8 mEpilogSection;         // 0x21
    u8 mUnresolvedSection;     // 0x22
    u8 mBssSection;            // 0x23
    u32 mProlog;               // 0x24
    u32 mEpilog;               // 0x28
    u32 mUnresolved;           // 0x2C
    u32 mInternalRelOffset;    // 0x30
    u32 mInternalRelSize;      // 0x34
    u32 mExternalRelOffset;    // 0x38
    u32 mExternalRelSize;      // 0x3C
    RSOSymbolHeader mExpHeader;  // 0x40
    RSOSymbolHeader mImpHeader;  // 0x4C
} RSOObjectHeader;

typedef struct RSOSectionInfo {
    u32 mOffset;  // 0x00
    u32 mSize;    // 0x04
} RSOSectionInfo;

typedef struct RSOExportTable {
    u32 strOffset;  // 0x00
    u32 value;      // 0x04
    u32 section;    // 0x08
    u32 hash;       // 0x0C
} RSOExportTable;

typedef struct RSOImportTable {
    u32 strOffset;  // 0x00
    u32 value;      // 0x04
    u32 relOffset;  // 0x08
} RSOImportTable;

BOOL RSOLinkInit(void* staticRso);
BOOL RSOSetBss(void* newRso, void* bss);
BOOL RSOStaticLocateObject(void* newModule);

void* RSOFindExportSymbol(const RSOObjectHeader* rso, const char* name);
RSOExportTable* RSOFindSymbol(const RSOObjectHeader* rso, const char* name);
u32 RSOGetSectionSize(const RSOObjectHeader* rso, int fixLevel);

#ifdef __cplusplus
}
#endif
#endif
