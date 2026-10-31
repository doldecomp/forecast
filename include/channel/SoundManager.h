#include "types.h"

void fn_80041884(void* data, void* heap, void *(*alloc_func)(u32), void (*mem_func)(void *));
void fn_800418A8(u32 arg1, void* data, u32 arg3, void* heap, u32 arg5, void* (*alloc_func)(u32), void (*mem_func)(void*));
void fn_80041C38();
void fn_80041F54();
void fn_800420E4(u8 sound_mode);

typedef void *(*SoundAllocFunc)(u32);
typedef void (*SoundMemFunc)(void*);
