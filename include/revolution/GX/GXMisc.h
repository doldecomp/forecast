#ifndef RVL_SDK_GX_MISC_H
#define RVL_SDK_GX_MISC_H
#include <types.h>

#include <revolution/GX/GXTypes.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef void (*GXDrawDoneCallback)(void);
typedef void (*GXDrawSyncCallback)(u16 token);

void GXSetMisc(GXMiscToken token, u32 val);
void GXFlush(void);
void GXResetWriteGatherPipe(void);

void GXAbortFrame(void);

void GXSetDrawSync(u16 token);

void GXSetDrawDone(void);
void GXWaitDrawDone(void);
void GXDrawDone(void);
void GXPixModeSync(void);

void GXPokeAlphaMode(GXCompare func, u8 threshold);
void GXPokeAlphaRead(GXAlphaReadMode mode);
void GXPokeAlphaUpdate(GXBool update);
void GXPokeBlendMode(GXBlendMode type, GXBlendFactor src, GXBlendFactor dst,
                     GXLogicOp op);
void GXPokeColorUpdate(GXBool update);
void GXPokeDstAlpha(GXBool enable, u8 alpha);
void GXPokeDither(GXBool dither);
void GXPokeZMode(GXBool compareEnable, GXCompare func, GXBool updateEnable);

GXDrawSyncCallback GXSetDrawSyncCallback(GXDrawSyncCallback);
GXDrawDoneCallback GXSetDrawDoneCallback(GXDrawDoneCallback);

void __GXAbort(void);
void __GXPEInit(void);

#ifdef __cplusplus
}
#endif
#endif
