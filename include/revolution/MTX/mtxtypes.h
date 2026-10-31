#ifndef RVL_SDK_MTX_TYPES_H
#define RVL_SDK_MTX_TYPES_H
#include <types.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef f32 Mtx[3][4];
typedef f32 (*MtxPtr)[4];
typedef f32 Mtx44[4][4];

typedef struct Vec {
    f32 x, y, z;
} Vec, *VecPtr, Point3d, *Point3dPtr;

typedef struct Vec2 {
    f32 x, y;
} Vec2;

typedef struct Quaternion {
    f32 x, y, z, w;
} Quaternion;

#define MTXDegToRad(d) (d * 0.01745329252f)
#define MTXRadToDeg(r) (r * 57.29577951f)

#ifdef __cplusplus
}
#endif
#endif
