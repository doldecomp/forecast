#include "revolution/AX/AXAlloc.h"
#include <revolution/AX.h>
#include <revolution/OS.h>

const char* __AXVersion =
    "<< RVL_SDK - AX \trelease build: May  8 2007 12:54:39 (0x4199_60831) >>";

static BOOL __init = FALSE;

void AXInit(void) {
    AXInitEx(0);
}

inline void AXInitEx(u32 mode) {
    if (!__init) {
        OSRegisterVersion(__AXVersion);

        __AXInitVoiceStacks();
        __AXVPBInit();
        __AXSPBInit();
        __AXAuxInit();
        __AXClInit();
        __AXOutInit(mode);

        __init = TRUE;
    }
}
