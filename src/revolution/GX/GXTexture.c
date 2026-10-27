#include <revolution/GX.h>

#include <cstring>

// BP register IDs of each texture map's registers (maps 0-3, then 4-7).
static u8 GXTexMode0Ids[GX_MAX_TEXMAP] = {0x80, 0x81, 0x82, 0x83,
                                          0xA0, 0xA1, 0xA2, 0xA3};
static u8 GXTexMode1Ids[GX_MAX_TEXMAP] = {0x84, 0x85, 0x86, 0x87,
                                          0xA4, 0xA5, 0xA6, 0xA7};
static u8 GXTexImage0Ids[GX_MAX_TEXMAP] = {0x88, 0x89, 0x8A, 0x8B,
                                           0xA8, 0xA9, 0xAA, 0xAB};
static u8 GXTexImage1Ids[GX_MAX_TEXMAP] = {0x8C, 0x8D, 0x8E, 0x8F,
                                           0xAC, 0xAD, 0xAE, 0xAF};
static u8 GXTexImage2Ids[GX_MAX_TEXMAP] = {0x90, 0x91, 0x92, 0x93,
                                           0xB0, 0xB1, 0xB2, 0xB3};
static u8 GXTexImage3Ids[GX_MAX_TEXMAP] = {0x94, 0x95, 0x96, 0x97,
                                           0xB4, 0xB5, 0xB6, 0xB7};
static u8 GXTexTlutIds[GX_MAX_TEXMAP] = {0x98, 0x99, 0x9A, 0x9B,
                                         0xB8, 0xB9, 0xBA, 0xBB};

// GXTexFilter <-> hardware min filter encoding.
static u8 GX2HWFiltConv[] = {0, 4, 1, 5, 2, 6};
static u8 HW2GXFiltConv[] = {0, 2, 4, 0, 1, 3, 5, 0};

void __GetImageTileCount(GXTexFmt fmt, u16 width, u16 height, u32* rowTiles,
                         u32* colTiles, u32* cmpTiles) {
    u32 tileWidthShift, tileHeightShift;

    // Tiles are 32 bytes: 8x8 at 4 bpp, 8x4 at 8 bpp, 4x4 at 16/32 bpp.
    switch (fmt) {
    case GX_TF_I4:
    case GX_TF_C4:
    case GX_TF_CMPR:
    case GX_CTF_R4:
    case GX_CTF_Z4:
        tileWidthShift = 3;
        tileHeightShift = 3;
        break;
    case GX_TF_I8:
    case GX_TF_IA4:
    case GX_TF_C8:
    case GX_TF_Z8:
    case GX_CTF_RA4:
    case GX_CTF_A8:
    case GX_CTF_R8:
    case GX_CTF_G8:
    case GX_CTF_B8:
    case GX_CTF_Z8M:
    case GX_CTF_Z8L:
        tileWidthShift = 3;
        tileHeightShift = 2;
        break;
    case GX_TF_IA8:
    case GX_TF_RGB565:
    case GX_TF_RGB5A3:
    case GX_TF_RGBA8:
    case GX_TF_C14X2:
    case GX_TF_Z16:
    case GX_TF_Z24X8:
    case GX_CTF_RA8:
    case GX_CTF_RG8:
    case GX_CTF_GB8:
    case GX_CTF_Z16L:
        tileWidthShift = 2;
        tileHeightShift = 2;
        break;
    default:
        tileWidthShift = tileHeightShift = 0;
        break;
    }

    if (width == 0) {
        width = 1;
    }
    if (height == 0) {
        height = 1;
    }

    *rowTiles = (width + (1 << tileWidthShift) - 1) >> tileWidthShift;
    *colTiles = (height + (1 << tileHeightShift) - 1) >> tileHeightShift;
    // 32-bit formats store each tile as two 32-byte halves.
    *cmpTiles = (fmt == GX_TF_RGBA8 || fmt == GX_TF_Z24X8) ? 2 : 1;
}

void GXInitTexObj(GXTexObj* obj, void* image, u16 width, u16 height,
                  GXTexFmt format, GXTexWrapMode wrapS, GXTexWrapMode wrapT,
                  GXBool mipmap) {
    u32 maxLod;
    u32 tileWidthShift, tileHeightShift;
    GXTexObjImpl* impl = (GXTexObjImpl*)obj;

    memset(impl, 0, sizeof(GXTexObjImpl));

    GX_SET_REG_FIELD(impl->mode0, 2, 0, wrapS);
    GX_SET_REG_FIELD(impl->mode0, 2, 2, wrapT);
    GX_SET_REG_FIELD(impl->mode0, 1, 4, GX_LINEAR);

    if (mipmap) {
        impl->flags |= GX_TEXOBJ_MIPMAP;

        if (format == GX_TF_C4 || format == GX_TF_C8 ||
            format == GX_TF_C14X2) {
            GX_SET_REG_FIELD(impl->mode0, 3, 5, 5);
        } else {
            GX_SET_REG_FIELD(impl->mode0, 3, 5, 6);
        }

        if (width > height) {
            maxLod = 31 - __cntlzw(width);
        } else {
            maxLod = 31 - __cntlzw(height);
        }

        GX_SET_REG_FIELD(impl->mode1, 8, 8, (u8)(16.0f * maxLod));
    } else {
        GX_SET_REG_FIELD(impl->mode0, 3, 5, 4);
    }

    impl->format = format;
    GX_SET_REG_FIELD(impl->image0, 10, 0, width - 1);
    GX_SET_REG_FIELD(impl->image0, 10, 10, height - 1);
    GX_SET_REG_FIELD(impl->image0, 4, 20, format & 0xF);
    GX_SET_REG_FIELD(impl->image3, 24, 0, ((u32)image & 0x3FFFFFFF) >> 5);

    switch (format & 0xF) {
    case GX_TF_I4:
    case GX_TF_C4:
        impl->loadFormat = 1;
        tileWidthShift = 3;
        tileHeightShift = 3;
        break;
    case GX_TF_I8:
    case GX_TF_IA4:
    case GX_TF_C8:
        impl->loadFormat = 2;
        tileWidthShift = 3;
        tileHeightShift = 2;
        break;
    case GX_TF_IA8:
    case GX_TF_RGB565:
    case GX_TF_RGB5A3:
    case GX_TF_C14X2:
        impl->loadFormat = 2;
        tileWidthShift = 2;
        tileHeightShift = 2;
        break;
    case GX_TF_RGBA8:
        impl->loadFormat = 3;
        tileWidthShift = 2;
        tileHeightShift = 2;
        break;
    case GX_TF_CMPR:
        impl->loadFormat = 0;
        tileWidthShift = 3;
        tileHeightShift = 3;
        break;
    default:
        impl->loadFormat = 2;
        tileWidthShift = 2;
        tileHeightShift = 2;
        break;
    }

    impl->loadCount =
        ((width + (1 << tileWidthShift) - 1) >> tileWidthShift) *
        ((height + (1 << tileHeightShift) - 1) >> tileHeightShift);
    impl->loadCount &= 0x7FFF;

    impl->flags |= GX_TEXOBJ_DIRECT;
}

void GXInitTexObjCI(GXTexObj* obj, void* image, u16 width, u16 height,
                    GXTexFmt format, GXTexWrapMode wrapS,
                    GXTexWrapMode wrapT, GXBool mipmap, u32 tlutName) {
    GXTexObjImpl* impl = (GXTexObjImpl*)obj;

    GXInitTexObj(obj, image, width, height, format, wrapS, wrapT, mipmap);
    impl->flags &= ~GX_TEXOBJ_DIRECT;
    impl->tlutName = tlutName;
}

void GXInitTexObjLOD(GXTexObj* obj, GXTexFilter minFilter,
                     GXTexFilter magFilter, f32 minLod, f32 maxLod,
                     f32 lodBias, GXBool biasClamp, GXBool edgeLod,
                     GXAnisotropy maxAniso) {
    GXTexObjImpl* impl = (GXTexObjImpl*)obj;
    u8 bias;
    u8 minLodFixed, maxLodFixed;

    // The LOD bias is s2.5 fixed point.
    if (lodBias < -4.0f) {
        lodBias = -4.0f;
    } else if (lodBias >= 4.0f) {
        lodBias = 3.99f;
    }
    bias = 32.0f * lodBias;

    GX_SET_REG_FIELD(impl->mode0, 8, 9, bias);
    GX_SET_REG_FIELD(impl->mode0, 1, 4, magFilter == GX_LINEAR ? 1 : 0);
    GX_SET_REG_FIELD(impl->mode0, 3, 5, GX2HWFiltConv[minFilter]);
    GX_SET_REG_FIELD(impl->mode0, 1, 8, edgeLod ? 0 : 1);
    GX_SET_REG_FIELD(impl->mode0, 1, 17, 0);
    GX_SET_REG_FIELD(impl->mode0, 1, 18, 0);
    GX_SET_REG_FIELD(impl->mode0, 2, 19, maxAniso);
    GX_SET_REG_FIELD(impl->mode0, 1, 21, biasClamp);

    // The LOD range is u4.4 fixed point, up to LOD 10.
    if (minLod < 0.0f) {
        minLod = 0.0f;
    } else if (minLod > 10.0f) {
        minLod = 10.0f;
    }
    minLodFixed = 16.0f * minLod;

    if (maxLod < 0.0f) {
        maxLod = 0.0f;
    } else if (maxLod > 10.0f) {
        maxLod = 10.0f;
    }
    maxLodFixed = 16.0f * maxLod;

    GX_SET_REG_FIELD(impl->mode1, 8, 0, minLodFixed);
    GX_SET_REG_FIELD(impl->mode1, 8, 8, maxLodFixed);
}

void GXInitTexObjWrapMode(GXTexObj* obj, GXTexWrapMode wrapS,
                          GXTexWrapMode wrapT) {
    GXTexObjImpl* impl = (GXTexObjImpl*)obj;
    GX_SET_REG_FIELD(impl->mode0, 2, 0, wrapS);
    GX_SET_REG_FIELD(impl->mode0, 2, 2, wrapT);
}

void GXInitTexObjTlut(GXTexObj* obj, u32 tlutName) {
    GXTexObjImpl* impl = (GXTexObjImpl*)obj;
    impl->tlutName = tlutName;
}

void GXInitTexObjUserData(GXTexObj* obj, void* userData) {
    GXTexObjImpl* impl = (GXTexObjImpl*)obj;
    impl->userData = userData;
}

void* GXGetTexObjUserData(const GXTexObj* obj) {
    const GXTexObjImpl* impl = (const GXTexObjImpl*)obj;
    return impl->userData;
}

u16 GXGetTexObjWidth(const GXTexObj* obj) {
    const GXTexObjImpl* impl = (const GXTexObjImpl*)obj;
    return GX_GET_REG_FIELD(impl->image0, 10, 0) + 1;
}

u16 GXGetTexObjHeight(const GXTexObj* obj) {
    const GXTexObjImpl* impl = (const GXTexObjImpl*)obj;
    return GX_GET_REG_FIELD(impl->image0, 10, 10) + 1;
}

GXTexFmt GXGetTexObjFmt(const GXTexObj* obj) {
    const GXTexObjImpl* impl = (const GXTexObjImpl*)obj;
    return impl->format;
}

GXTexWrapMode GXGetTexObjWrapS(GXTexObj* obj) {
    const GXTexObjImpl* impl = (const GXTexObjImpl*)obj;
    return (GXTexWrapMode)GX_GET_REG_FIELD(impl->mode0, 2, 0);
}

GXTexWrapMode GXGetTexObjWrapT(GXTexObj* obj) {
    const GXTexObjImpl* impl = (const GXTexObjImpl*)obj;
    return (GXTexWrapMode)GX_GET_REG_FIELD(impl->mode0, 2, 2);
}

GXBool GXGetTexObjMipMap(const GXTexObj* obj) {
    const GXTexObjImpl* impl = (const GXTexObjImpl*)obj;
    return impl->flags & GX_TEXOBJ_MIPMAP;
}

void GXGetTexObjLODAll(const GXTexObj* obj, GXTexFilter* minFilter,
                       GXTexFilter* magFilter, f32* minLod, f32* maxLod,
                       f32* lodBias, GXBool* biasClamp, GXBool* edgeLod,
                       GXAnisotropy* maxAniso) {
    const GXTexObjImpl* impl = (const GXTexObjImpl*)obj;
    s16 bias;

    *minFilter = (GXTexFilter)HW2GXFiltConv[GX_GET_REG_FIELD(impl->mode0, 3, 5)];
    *magFilter = (GXTexFilter)GX_GET_REG_FIELD(impl->mode0, 1, 4);
    *minLod = (u8)impl->mode1 / 16.0f;
    *maxLod = (u32)GX_GET_REG_FIELD(impl->mode1, 8, 8) / 16.0f;
    bias = (s32)GX_GET_REG_FIELD(impl->mode0, 8, 9);
    *lodBias = (s8)bias / 32.0f;
    *biasClamp = (u32)GX_GET_REG_FIELD(impl->mode0, 1, 21);
    *edgeLod = !GX_GET_REG_FIELD(impl->mode0, 1, 8);
    *maxAniso = (GXAnisotropy)GX_GET_REG_FIELD(impl->mode0, 2, 19);
}

void GXLoadTexObjPreLoaded(const GXTexObj* obj, const GXTexRegion* region,
                           GXTexMapID id) {
    GXTexObjImpl* impl = (GXTexObjImpl*)obj;
    GXTexRegionImpl* regionImpl = (GXTexRegionImpl*)region;
    GXTlutRegionImpl* tlutRegion;

    GX_SET_REG_FIELD(impl->mode0, 8, 24, GXTexMode0Ids[id]);
    GX_SET_REG_FIELD(impl->mode1, 8, 24, GXTexMode1Ids[id]);
    GX_SET_REG_FIELD(impl->image0, 8, 24, GXTexImage0Ids[id]);
    GX_SET_REG_FIELD(regionImpl->image1, 8, 24, GXTexImage1Ids[id]);
    GX_SET_REG_FIELD(regionImpl->image2, 8, 24, GXTexImage2Ids[id]);
    GX_SET_REG_FIELD(impl->image3, 8, 24, GXTexImage3Ids[id]);

    GX_BP_LOAD_REG(impl->mode0);
    GX_BP_LOAD_REG(impl->mode1);
    GX_BP_LOAD_REG(impl->image0);
    GX_BP_LOAD_REG(regionImpl->image1);
    GX_BP_LOAD_REG(regionImpl->image2);
    GX_BP_LOAD_REG(impl->image3);

    // Color-indexed textures also need their TLUT bound to this map.
    if (!(impl->flags & GX_TEXOBJ_DIRECT)) {
        tlutRegion =
            (GXTlutRegionImpl*)gxdt->tlutRegionCallback(impl->tlutName);
        GX_SET_REG_FIELD(tlutRegion->tlutObj.tlut, 8, 24, GXTexTlutIds[id]);
        GX_BP_LOAD_REG(tlutRegion->tlutObj.tlut);
    }

    gxdt->tImage0[id] = impl->image0;
    gxdt->tMode0[id] = impl->mode0;
    gxdt->gxDirtyFlags |= GX_DIRTY_SU_TEX;
    gxdt->lastWriteWasXF = FALSE;
}

void GXLoadTexObj(const GXTexObj* obj, GXTexMapID id) {
    GXTexRegion* region = gxdt->texRegionCallback(obj, id);
    GXLoadTexObjPreLoaded(obj, region, id);
}

void GXInitTlutObj(GXTlutObj* obj, void* table, GXTlutFmt format,
                   u16 numEntries) {
    GXTlutObjImpl* impl = (GXTlutObjImpl*)obj;

    impl->tlut = 0;
    GX_SET_REG_FIELD(impl->tlut, 2, 10, format);
    GX_SET_REG_FIELD(impl->loadTlut0, 24, 0, ((u32)table & 0x3FFFFFFF) >> 5);
    GX_SET_REG_FIELD(impl->loadTlut0, 8, 24, GX_BP_REG_TMEMTLUTSRC);
    impl->numEntries = numEntries;
}

void GXLoadTlut(const GXTlutObj* obj, u32 tlutName) {
    GXTlutObjImpl* impl = (GXTlutObjImpl*)obj;
    GXTlutRegionImpl* region =
        (GXTlutRegionImpl*)gxdt->tlutRegionCallback(tlutName);
    u32 tmemOffset;

    __GXFlushTextureState();
    GX_BP_LOAD_REG(impl->loadTlut0);
    GX_BP_LOAD_REG(region->loadTlut1);
    __GXFlushTextureState();

    tmemOffset = region->loadTlut1 & 0x3FF;
    GX_SET_REG_FIELD(impl->tlut, 10, 0, tmemOffset);
    region->tlutObj = *impl;
}

void GXInitTexCacheRegion(GXTexRegion* region, GXBool is32bMipmap,
                          u32 tmemEven, GXTexCacheSize sizeEven, u32 tmemOdd,
                          GXTexCacheSize sizeOdd) {
    GXTexRegionImpl* impl = (GXTexRegionImpl*)region;
    u32 widthExp2;

    switch (sizeEven) {
    case GX_TEXCACHE_32K:
        widthExp2 = 3;
        break;
    case GX_TEXCACHE_128K:
        widthExp2 = 4;
        break;
    case GX_TEXCACHE_512K:
        widthExp2 = 5;
        break;
    }

    impl->image1 = 0;
    GX_SET_REG_FIELD(impl->image1, 15, 0, tmemEven >> 5);
    GX_SET_REG_FIELD(impl->image1, 3, 15, widthExp2);
    GX_SET_REG_FIELD(impl->image1, 3, 18, widthExp2);
    GX_SET_REG_FIELD(impl->image1, 1, 21, 0);

    switch (sizeOdd) {
    case GX_TEXCACHE_32K:
        widthExp2 = 3;
        break;
    case GX_TEXCACHE_128K:
        widthExp2 = 4;
        break;
    case GX_TEXCACHE_512K:
        widthExp2 = 5;
        break;
    case GX_TEXCACHE_NONE:
        widthExp2 = 0;
        break;
    }

    impl->image2 = 0;
    GX_SET_REG_FIELD(impl->image2, 15, 0, tmemOdd >> 5);
    GX_SET_REG_FIELD(impl->image2, 3, 15, widthExp2);
    GX_SET_REG_FIELD(impl->image2, 3, 18, widthExp2);

    impl->is32bMipmap = is32bMipmap;
    impl->isCached = TRUE;
}

void GXInitTlutRegion(GXTlutRegion* region, u32 tmemAddr, GXTlutSize size) {
    GXTlutRegionImpl* impl = (GXTlutRegionImpl*)region;

    // TLUTs live in the upper half of TMEM, addressed in 512-byte units.
    impl->loadTlut1 = 0;
    tmemAddr -= 0x80000;
    GX_SET_REG_FIELD(impl->loadTlut1, 10, 0, tmemAddr >> 9);
    GX_SET_REG_FIELD(impl->loadTlut1, 11, 10, size);
    GX_SET_REG_FIELD(impl->loadTlut1, 8, 24, GX_BP_REG_TMEMTLUTDST);
}

void GXInvalidateTexAll(void) {
    __GXFlushTextureState();
    // Invalidate both TMEM halves.
    GX_BP_LOAD_REG(GX_BP_REG_TMEMTEXINVALIDATE << GX_BP_OPCODE_SHIFT | 0x1000);
    GX_BP_LOAD_REG(GX_BP_REG_TMEMTEXINVALIDATE << GX_BP_OPCODE_SHIFT | 0x1100);
    __GXFlushTextureState();
}

GXTexRegionCallback GXSetTexRegionCallback(GXTexRegionCallback callback) {
    GXTexRegionCallback old = gxdt->texRegionCallback;
    gxdt->texRegionCallback = callback;
    return old;
}

GXTlutRegionCallback GXSetTlutRegionCallback(GXTlutRegionCallback callback) {
    GXTlutRegionCallback old = gxdt->tlutRegionCallback;
    gxdt->tlutRegionCallback = callback;
    return old;
}

void __SetSURegs(u32 texMap, u32 texCoord) {
    u32 width, height;
    GXBool biasS, biasT;

    // Texture coordinate scale: the size of the texture bound to the map.
    width = GX_GET_REG_FIELD(gxdt->tImage0[texMap], 10, 0);
    height = GX_GET_REG_FIELD(gxdt->tImage0[texMap], 10, 10);
    GX_SET_REG_FIELD(gxdt->suTs0[texCoord], 16, 0, width);
    GX_SET_REG_FIELD(gxdt->suTs1[texCoord], 16, 0, height);

    // Repeating textures get the coordinate bias.
    biasS = GX_GET_REG_FIELD(gxdt->tMode0[texMap], 2, 0) == GX_REPEAT;
    biasT = GX_GET_REG_FIELD(gxdt->tMode0[texMap], 2, 2) == GX_REPEAT;
    GX_SET_REG_FIELD(gxdt->suTs0[texCoord], 1, 16, biasS);
    GX_SET_REG_FIELD(gxdt->suTs1[texCoord], 1, 16, biasT);

    GX_BP_LOAD_REG(gxdt->suTs0[texCoord]);
    GX_BP_LOAD_REG(gxdt->suTs1[texCoord]);
    gxdt->lastWriteWasXF = FALSE;
}

// The original calls __SetSURegs out of line here.
#pragma push
#pragma dont_inline on
void __GXSetSUTexRegs(void) {
    u32 numStages;
    u32 numIndStages;
    u32 i;
    u32 map;
    u32 texMap;
    u32 texCoord;
    u32* tref;

    // Texture coordinates whose scale the user sets manually are skipped.
    if (gxdt->tcsManEnab == 0xFF) {
        return;
    }

    numStages = GX_GET_REG_FIELD(gxdt->genMode, 4, 10) + 1;
    numIndStages = GX_GET_REG_FIELD(gxdt->genMode, 3, 16);

    for (i = 0; i < numIndStages; i++) {
        switch (i) {
        case 0:
            texMap = GX_GET_REG_FIELD(gxdt->ras1_iref, 3, 0);
            texCoord = GX_GET_REG_FIELD(gxdt->ras1_iref, 3, 3);
            break;
        case 1:
            texMap = GX_GET_REG_FIELD(gxdt->ras1_iref, 3, 6);
            texCoord = GX_GET_REG_FIELD(gxdt->ras1_iref, 3, 9);
            break;
        case 2:
            texMap = GX_GET_REG_FIELD(gxdt->ras1_iref, 3, 12);
            texCoord = GX_GET_REG_FIELD(gxdt->ras1_iref, 3, 15);
            break;
        case 3:
            texMap = GX_GET_REG_FIELD(gxdt->ras1_iref, 3, 18);
            texCoord = GX_GET_REG_FIELD(gxdt->ras1_iref, 3, 21);
            break;
        }

        if (!(gxdt->tcsManEnab & (1 << texCoord))) {
            __SetSURegs(texMap, texCoord);
        }
    }

    for (i = 0; i < numStages; i++) {
        tref = &gxdt->tref[i / 2];
        map = gxdt->texmapId[i];
        texMap = map & ~GX_TEX_DISABLE;

        if (i & 1) {
            texCoord = GX_GET_REG_FIELD(*tref, 3, 15);
        } else {
            texCoord = GX_GET_REG_FIELD(*tref, 3, 3);
        }

        if (texMap != GX_TEXMAP_NULL &&
            !(gxdt->tcsManEnab & (1 << texCoord)) &&
            (gxdt->tevTcEnab & (1 << i))) {
            __SetSURegs(texMap, texCoord);
        }
    }
}

#pragma pop

void __GXSetTmemConfig(u32 config) {
    // Each register sets one texture map's even/odd TMEM cache region.
    switch (config) {
    case 2:
        GX_BP_LOAD_REG(0x8C0D8000);
        GX_BP_LOAD_REG(0x900DC000);
        GX_BP_LOAD_REG(0x8D0D8800);
        GX_BP_LOAD_REG(0x910DC800);
        GX_BP_LOAD_REG(0x8E0D9000);
        GX_BP_LOAD_REG(0x920DD000);
        GX_BP_LOAD_REG(0x8F0D9800);
        GX_BP_LOAD_REG(0x930DD800);
        GX_BP_LOAD_REG(0xAC0DA000);
        GX_BP_LOAD_REG(0xB00DC400);
        GX_BP_LOAD_REG(0xAD0DA800);
        GX_BP_LOAD_REG(0xB10DCC00);
        GX_BP_LOAD_REG(0xAE0DB000);
        GX_BP_LOAD_REG(0xB20DD400);
        GX_BP_LOAD_REG(0xAF0DB800);
        GX_BP_LOAD_REG(0xB30DDC00);
        break;
    case 1:
        GX_BP_LOAD_REG(0x8C0D8000);
        GX_BP_LOAD_REG(0x900DC000);
        GX_BP_LOAD_REG(0x8D0D8800);
        GX_BP_LOAD_REG(0x910DC800);
        GX_BP_LOAD_REG(0x8E0D9000);
        GX_BP_LOAD_REG(0x920DD000);
        GX_BP_LOAD_REG(0x8F0D9800);
        GX_BP_LOAD_REG(0x930DD800);
        GX_BP_LOAD_REG(0xAC0DA000);
        GX_BP_LOAD_REG(0xB00DE000);
        GX_BP_LOAD_REG(0xAD0DA800);
        GX_BP_LOAD_REG(0xB10DE800);
        GX_BP_LOAD_REG(0xAE0DB000);
        GX_BP_LOAD_REG(0xB20DF000);
        GX_BP_LOAD_REG(0xAF0DB800);
        GX_BP_LOAD_REG(0xB30DF800);
        break;
    case 0:
    default:
        GX_BP_LOAD_REG(0x8C0D8000);
        GX_BP_LOAD_REG(0x900DC000);
        GX_BP_LOAD_REG(0x8D0D8400);
        GX_BP_LOAD_REG(0x910DC400);
        GX_BP_LOAD_REG(0x8E0D8800);
        GX_BP_LOAD_REG(0x920DC800);
        GX_BP_LOAD_REG(0x8F0D8C00);
        GX_BP_LOAD_REG(0x930DCC00);
        GX_BP_LOAD_REG(0xAC0D9000);
        GX_BP_LOAD_REG(0xB00DD000);
        GX_BP_LOAD_REG(0xAD0D9400);
        GX_BP_LOAD_REG(0xB10DD400);
        GX_BP_LOAD_REG(0xAE0D9800);
        GX_BP_LOAD_REG(0xB20DD800);
        GX_BP_LOAD_REG(0xAF0D9C00);
        GX_BP_LOAD_REG(0xB30DDC00);
        break;
    }
}
