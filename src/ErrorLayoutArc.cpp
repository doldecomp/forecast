// The error screen's layout archive (error_system.brlyt and its textures),
// linked into the DOL so errors can be shown without reading the NAND.
#include <types.h>

extern "C" {
u8 gErrorLayoutArc[] ATTRIBUTE_ALIGN(32) = {
#include "assets/error_layout.arc.inc"
};
}
