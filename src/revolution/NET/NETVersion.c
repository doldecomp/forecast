#include <revolution/NET.h>
#include <revolution/version.h>

REX_PPC_LIB_VERSION(NET, "2.0.4.0", "RevoEX-2.0PR4", "070628174553");

// Nothing in the channel calls this, but the DOL keeps it.
#pragma push
#pragma force_active on
const char* NETGetRexPPCVersionPrintable(void) {
    return NETRexPPCVersionPrintableString;
}
#pragma pop
