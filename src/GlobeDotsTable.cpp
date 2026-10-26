// The globe's location dots (see GlobeDots.cpp). The tables are extracted from
// the DOL at build time (config.yml), so the source only includes them.
#include <channel/GlobeDots.h>

extern "C" {
const u8 gGlobeDotPositions[GLOBE_DOT_COUNT * 4] = {
#include "assets/globe_dot_positions.inc"
};

const u8 gGlobeDotSizes[GLOBE_DOT_COUNT] = {
#include "assets/globe_dot_sizes.inc"
};

const u8 gGlobeDotColorIndices[GLOBE_DOT_COUNT] = {
#include "assets/globe_dot_color_indices.inc"
};

const u8 gGlobeDotColors[GLOBE_DOT_COUNT * 3] = {
#include "assets/globe_dot_colors.inc"
};
}
