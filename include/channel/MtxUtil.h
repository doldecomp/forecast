#ifndef CHANNEL_MTX_UTIL_H
#define CHANNEL_MTX_UTIL_H
#include <types.h>
#include <revolution/MTX.h>

// Concatenates a rotation about one axis (in degrees) onto the matrix
void RotateX(Mtx m, f32 degrees);
void RotateY(Mtx m, f32 degrees);
void RotateZ(Mtx m, f32 degrees);

// Sets the matrix to a rotation about Z (in degrees)
void SetRotateZ(Mtx m, f32 degrees);

// Transforms a position by a 4x4 projection matrix, giving x, y, z and w
void MultVec4(f32* out, const Mtx44 m, const Vec* v);

// Mtx with an (empty) destructor.  gModelMtx is one: defining it in MtxUtil.cpp makes the
// compiler emit the file's static initializer, which registers the destructor (the
// destructor itself ended up in GlobeView.cpp).  Users work on the .m member.
class Mtx34 {
public:
    ~Mtx34();

    Mtx m;
};

extern Mtx34 gModelMtx;

#endif
