#include <channel/MtxUtil.h>
#include <nw4r/math.h>

extern const f32 sDegToRad;
extern const f32 sDegToFIdx;

void TranslateMtx(nw4r::math::MTX34* m, f32 x, f32 y, f32 z) {
    Mtx trans;
    PSMTXTrans(trans, x, y, z);
    PSMTXConcat(trans, m->mtx, m->mtx);
}

void RotateX(Mtx m, f32 degrees) {
    Mtx rot;
    PSMTXRotRad(rot, 'x', sDegToRad * degrees);
    PSMTXConcat(rot, m, m);
}

void RotateY(Mtx m, f32 degrees) {
    Mtx rot;
    PSMTXRotRad(rot, 'y', sDegToRad * degrees);
    PSMTXConcat(rot, m, m);
}

void RotateZ(Mtx m, f32 degrees) {
    Mtx rot;
    PSMTXRotRad(rot, 'z', sDegToRad * degrees);
    PSMTXConcat(rot, m, m);
}

void RotateMtxDeg(nw4r::math::MTX34* m, f32 x, f32 y, f32 z) {
    nw4r::math::MTX34 rot;
    nw4r::math::MTX34RotXYZFIdx(&rot, sDegToFIdx * x, sDegToFIdx * y, sDegToFIdx * z);
    PSMTXConcat(rot.mtx, m->mtx, m->mtx);
}

void SetRotateZ(Mtx m, f32 degrees) {
    PSMTXRotRad(m, 'z', sDegToRad * degrees);
}
