// Based on NETVersion.c from the Super Mario Galaxy decompilation
// (SMGCommunity/Petari, CC0), with this channel's RevoEX version string.
#include <revolution/NET.h>

// Nothing in the channel calls this, but the original kept it.
#pragma force_active on

char* NETRexPPCVersionPrintableString = "<< REX-PPC 2.0.4.0 (RevoEX-2.0PR4) REL 070628174553 >>";

char* NETGetRexPPCVersionPrintable() {
    return NETRexPPCVersionPrintableString;
}
