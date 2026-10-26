#ifndef CHANNEL_GLOBE_DOTS_H
#define CHANNEL_GLOBE_DOTS_H
#include <types.h>
#include <revolution/MTX.h>

#define GLOBE_DOT_COUNT 9096

// The dot tables (GlobeDotsTable.cpp)
extern "C" {
extern const u8 gGlobeDotPositions[GLOBE_DOT_COUNT * 4]; // u16 longitude, u16 latitude per dot
extern const u8 gGlobeDotSizes[GLOBE_DOT_COUNT];
extern const u8 gGlobeDotColorIndices[GLOBE_DOT_COUNT];
extern const u8 gGlobeDotColors[GLOBE_DOT_COUNT * 3];   // RGB8, indexed by gGlobeDotColorIndices
}

// Textured dots on the globe, one triangle per location. They fade out while the globe is moving.
class GlobeDots {
public:
    GlobeDots();
    ~GlobeDots();

    void ResetAlpha();
    void UpdateAlpha(f32 speedX, f32 speedY);
    void Draw();

    Vec mVerts[GLOBE_DOT_COUNT * 3]; // at 0x0
    u8 mAlpha;                       // at 0x4FF20
};

#endif
