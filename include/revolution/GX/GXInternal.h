#ifndef RVL_SDK_GX_INTERNAL_H
#define RVL_SDK_GX_INTERNAL_H
#include <types.h>

#include <revolution/GX/GXTypes.h>
#ifdef __cplusplus
extern "C" {
#endif

/**
 * GX internal structures.
 *
 * Wouldn't be necessary if the public ones didn't include padding; but they do,
 * so there has to be different structure definitions.
 *
 * These internal structures are implemented like the RFL ones since we don't
 * have DWARF info for most GX structures.
 */

/**
 * Declare a public structure from the corresponding internal structure.
 * (Implementation size is included to require that such a structure already
 * exists.)
 */
#define GX_PUBLIC_STRUCT_DECL(name, size)                                      \
    typedef struct _##name {                                                   \
        u32 dummy[((size) - sizeof(name##Impl) + sizeof(name##Impl)) /         \
                  sizeof(u32)];                                                \
    } name;

typedef struct _GXFifoObjImpl {
    void* base;      // at 0x0
    void* end;       // at 0x4
    u32 size;        // at 0x8
    u32 hiWatermark; // at 0xC
    u32 loWatermark; // at 0x10
    void* readPtr;   // at 0x14
    void* writePtr;  // at 0x18
    s32 count;       // at 0x1C
    u8 wrap;         // at 0x20
    GXBool bindCPU;  // at 0x21
    GXBool bindGP;   // at 0x22
} GXFifoObjImpl;

typedef struct _GXLightObjImpl {
    char UNK_0x0[0xC];
    GXColor color; // at 0xC
    f32 aa;        // at 0x10
    f32 ab;        // at 0x14
    f32 ac;        // at 0x18
    f32 ka;        // at 0x1C
    f32 kb;        // at 0x20
    f32 kc;        // at 0x24
    f32 posX;      // at 0x28
    f32 posY;      // at 0x2C
    f32 posZ;      // at 0x30
    f32 dirX;      // at 0x34
    f32 dirY;      // at 0x38
    f32 dirZ;      // at 0x3C
} GXLightObjImpl;

typedef struct _GXTexObjImpl {
    u32 mode0;       // at 0x0
    u32 mode1;       // at 0x4
    u32 image0;      // at 0x8
    u32 image3;      // at 0xC
    void* userData;  // at 0x10
    GXTexFmt format; // at 0x14
    u32 tlutName;    // at 0x18
    u16 loadCount;   // at 0x1C
    u8 loadFormat;   // at 0x1E
    u8 flags;        // at 0x1F
} GXTexObjImpl;

// GXTexObjImpl::flags
#define GX_TEXOBJ_MIPMAP (1 << 0)
#define GX_TEXOBJ_DIRECT (1 << 1) // not color-indexed (no TLUT)

typedef struct _GXTlutObjImpl {
    u32 tlut;       // at 0x0
    u32 loadTlut0;  // at 0x4
    u16 numEntries; // at 0x8
} GXTlutObjImpl;

typedef struct _GXTexRegionImpl {
    u32 image1;       // at 0x0
    u32 image2;       // at 0x4
    char UNK_0x8[0xC - 0x8];
    GXBool is32bMipmap; // at 0xC
    GXBool isCached;    // at 0xD
} GXTexRegionImpl;

typedef struct _GXTlutRegionImpl {
    u32 loadTlut1;         // at 0x0
    GXTlutObjImpl tlutObj; // at 0x4
} GXTlutRegionImpl;

#ifdef __cplusplus
}
#endif
#endif
