#ifndef RVL_SDK_GX_FIFO_H
#define RVL_SDK_GX_FIFO_H
#include <types.h>

#include <revolution/GX/GXInternal.h>

#include <revolution/OS.h>
#ifdef __cplusplus
extern "C" {
#endif

GX_PUBLIC_STRUCT_DECL(GXFifoObj, 128);

typedef void (*GXBreakPtCallback)(void);

void GXInitFifoBase(GXFifoObj* fifo, void* base, u32 size);
void GXInitFifoPtrs(GXFifoObj* fifo, void* readPtr, void* writePtr);
void GXInitFifoLimits(GXFifoObj* fifo, u32 hiWatermark, u32 loWatermark);

void GXGetGPStatus(GXBool* overhi, GXBool* underlow, GXBool* readIdle,
                   GXBool* cmdIdle, GXBool* brkpt);

void GXSetCPUFifo(GXFifoObj*);
void GXSetGPFifo(GXFifoObj*);
BOOL GXGetCPUFifo(GXFifoObj*);
BOOL GXGetGPFifo(GXFifoObj*);

void GXGetFifoPtrs(const GXFifoObj*, void** readPtr, void** writePtr);

u32 GXGetFifoCount(GXFifoObj*);
u8 GXGetFifoWrap(GXFifoObj*);

void GXEnableBreakPt(void* writePtr);
void GXDisableBreakPt(void);

OSThread* GXGetCurrentGXThread(void);

GXBreakPtCallback GXSetBreakPtCallback(GXBreakPtCallback callback);

GXBool CPGPLinkCheck(void);
void __GXFifoInit(void);
GXBool __GXIsGPFifoReady(void);
void __GXCleanGPFifo(void);

#ifdef __cplusplus
}
#endif
#endif
