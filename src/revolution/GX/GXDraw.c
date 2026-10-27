#include <cmath>
// The SDK was built against MSL's math headers, which tell MWCC how errno is
// accessed. Without that, the compiler assumes cos/sin can change any local
// whose address was taken (ttype/tex0 below) and reloads or spills it.
#include <__internal/__errno.h>
#include <revolution/GX.h>

// +1 for null terminator
static GXVtxDescList vcd[GX_VA_MAX_ATTR + 1];
static GXVtxAttrFmtList vat[GX_VA_MAX_ATTR + 1];

static void GetVertState(void) {
    GXGetVtxDescv(vcd);
    GXGetVtxAttrFmtv(GX_VTXFMT3, vat);

    GXClearVtxDesc();
    GXSetVtxDesc(GX_VA_POS, GX_DIRECT);
    GXSetVtxDesc(GX_VA_NRM, GX_DIRECT);
    GXSetVtxAttrFmt(GX_VTXFMT3, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
    GXSetVtxAttrFmt(GX_VTXFMT3, GX_VA_NRM, GX_NRM_XYZ, GX_F32, 0);
}

static void RestoreVertState(void) {
    GXSetVtxDescv(vcd);
    GXSetVtxAttrFmtv(GX_VTXFMT3, vat);
}

void GXDrawCylinder(u8 sides) {
    // Unit-circle vertices
    f32 vx[100];
    f32 vy[100];

    f32 z, zn;
    f32 sectorAngle;
    s32 i;

    // Radius in both directions
    z = 1.0f;
    zn = -z;

    GetVertState();

    // Calculate vertices
    for (i = 0; i <= sides; i++) {
        sectorAngle = 2.0f * i * M_PI / sides;
        vx[i] = cosf(sectorAngle);
        vy[i] = sinf(sectorAngle);
    }

    GXBegin(GX_TRIANGLESTRIP, GX_VTXFMT3, (sides + 1) * 2);
    {
        for (i = 0; i <= sides; i++) {
            GXPosition3f32(vx[i], vy[i], zn);
            GXPosition3f32(vx[i], vy[i], 0.0f);
            GXPosition3f32(vx[i], vy[i], z);
            GXPosition3f32(vx[i], vy[i], 0.0f);
        }
    }
    GXEnd();

    GXBegin(GX_TRIANGLEFAN, GX_VTXFMT3, sides + 2);
    {
        GXPosition3f32(0.0f, 0.0f, z);
        GXPosition3f32(0.0f, 0.0f, 1.0f);

        for (i = 0; i <= sides; i++) {

            GXPosition3f32(vx[i], -vy[i], z);
            GXPosition3f32(0.0f, 0.0f, 1.0f);
        }
    }
    GXEnd();

    GXBegin(GX_TRIANGLEFAN, GX_VTXFMT3, sides + 2);
    {
        GXPosition3f32(0.0f, 0.0f, zn);
        GXPosition3f32(0.0f, 0.0f, -1.0f);

        for (i = 0; i <= sides; i++) {
            GXPosition3f32(vx[i], vy[i], zn);
            GXPosition3f32(0.0f, 0.0f, -1.0f);
        }
    }
    GXEnd();

    RestoreVertState();
}
// https://github.com/doldecomp/melee/blob/43c7de326a8192cac8eccd0af8272933e16a4e7d/libs/dolphin/src/dolphin/gx/GXDraw.c#L160
void GXDrawTorus(f32 rc, u8 numc, u8 numt)
{
    GXAttrType ttype;
    s32 i, j, k;
    f32 s, t;
    f32 x, y, z;
    f32 twopi = 6.2831855f;
    f32 rt;

    rt = 1.0f - rc;
    GXGetVtxDesc(GX_VA_TEX0, &ttype);
    GetVertState();
    if (ttype != GX_NONE) {
        GXSetVtxDesc(GX_VA_TEX0, GX_DIRECT);
        GXSetVtxAttrFmt(GX_VTXFMT3, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);
    }
    for (i = 0; i < numc; i++) {
        GXBegin(GX_TRIANGLESTRIP, GX_VTXFMT3, (numt + 1) * 2);
        for (j = 0; j <= numt; j++) {
            for (k = 1; k >= 0; k--) {
                s = (i + k) % numc;
                t = j % numt;
                x = (rt - rc * cosf(s * twopi / numc)) *
                    cosf(t * twopi / numt);
                y = (rt - rc * cosf(s * twopi / numc)) *
                    sinf(t * twopi / numt);
                z = rc * sinf(s * twopi / numc);
                GXPosition3f32(x, y, z);
                x = -cosf(t * twopi / numt) * cosf(s * twopi / numc);
                y = -sinf(t * twopi / numt) * cosf(s * twopi / numc);
                z = sinf(s * twopi / numc);
                GXNormal3f32(x, y, z);
                if (ttype != GX_NONE) {
                    GXTexCoord2f32((i + k) / (f32) numc, j / (f32) numt);
                }
            }
        }
        GXEnd();
    }
    RestoreVertState();
}

void GXDrawSphere(u32 stacks, u32 sectors) {
    GXAttrType tex0;
    f32 radius;
    f32 stackStep, sectorStep;
    f32 stackAngle, stackAngleNext;
    f32 now_xy, next_xy;
    f32 now_z, next_z;
    f32 sectorAngle;
    f32 cosv, sinv;
    int i, j;

    stackStep = M_PI / stacks;
    sectorStep = 2 * M_PI / sectors;
    radius = 1.0f;

    // Check texcoord attributes
    GXGetVtxDesc(GX_VA_TEX0, &tex0);

    GetVertState();

    // Setup texcoord VAT/VCD if enabled
    if (tex0 != GX_NONE) {
        GXSetVtxDesc(GX_VA_TEX0, GX_DIRECT);
        GXSetVtxAttrFmt(GX_VTXFMT3, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);
    }

    for (i = 0; i < (int)stacks; i++) {
        stackAngle = i * stackStep;
        stackAngleNext = stackAngle + stackStep;

        now_xy = radius * sinf(stackAngle);
        next_xy = radius * sinf(stackAngleNext);
        now_z = radius * cosf(stackAngle);
        next_z = radius * cosf(stackAngleNext);

        GXBegin(GX_TRIANGLESTRIP, GX_VTXFMT3, (sectors + 1) * 2);
        {
            for (j = 0; j <= (int)sectors; j++) {
                sectorAngle = j * sectorStep;
                cosv = cosf(sectorAngle);
                sinv = sinf(sectorAngle);

                // Vertex 2 position
                GXPosition3f32(cosv * next_xy, sinv * next_xy, next_z);
                // Vertex 2 normal
                GXPosition3f32(cosv * next_xy / radius, sinv * next_xy / radius,
                               next_z / radius);

                // Vertex 2 texcoord (S,T)
                if (tex0 != GX_NONE) {
                    GXTexCoord2f32((f32)j / sectors, (f32)(i + 1) / stacks);
                }

                // Vertex 1 position
                GXPosition3f32(cosv * now_xy, sinv * now_xy, now_z);
                // Vertex 1 normal
                GXPosition3f32(cosv * now_xy / radius, sinv * now_xy / radius,
                               now_z / radius);

                // Vertex 1 texcoord (S,T)
                if (tex0 != GX_NONE) {
                    GXTexCoord2f32((f32)j / sectors, (f32)i / stacks);
                }
            }
        }
        GXEnd();
    }

    RestoreVertState();
}

static void GXDrawCubeFace(f32 nx, f32 ny, f32 nz, f32 tx, f32 ty, f32 tz,
                           f32 bx, f32 by, f32 bz, GXAttrType binormal,
                           GXAttrType texture)
{
    GXPosition3f32(0.57735026f * (nx + tx + bx), 0.57735026f * (ny + ty + by),
                   0.57735026f * (nz + tz + bz));
    GXNormal3f32(nx, ny, nz);
    if (binormal != GX_NONE) {
        GXNormal3f32(tx, ty, tz);
        GXNormal3f32(bx, by, bz);
    }
    if (texture != GX_NONE) {
        GXTexCoord2s8(1, 1);
    }
    GXPosition3f32(0.57735026f * (nx - tx + bx), 0.57735026f * (ny - ty + by),
                   0.57735026f * (nz - tz + bz));
    GXNormal3f32(nx, ny, nz);
    if (binormal != GX_NONE) {
        GXNormal3f32(tx, ty, tz);
        GXNormal3f32(bx, by, bz);
    }
    if (texture != GX_NONE) {
        GXTexCoord2s8(0, 1);
    }
    GXPosition3f32(0.57735026f * (nx - tx - bx), 0.57735026f * (ny - ty - by),
                   0.57735026f * (nz - tz - bz));
    GXNormal3f32(nx, ny, nz);
    if (binormal != GX_NONE) {
        GXNormal3f32(tx, ty, tz);
        GXNormal3f32(bx, by, bz);
    }
    if (texture != GX_NONE) {
        GXTexCoord2s8(0, 0);
    }
    GXPosition3f32(0.57735026f * (nx + tx - bx), 0.57735026f * (ny + ty - by),
                   0.57735026f * (nz + tz - bz));
    GXNormal3f32(nx, ny, nz);
    if (binormal != GX_NONE) {
        GXNormal3f32(tx, ty, tz);
        GXNormal3f32(bx, by, bz);
    }
    if (texture != GX_NONE) {
        GXTexCoord2s8(1, 0);
    }
}

void GXDrawCube(void)
{
    GXAttrType ntype;
    GXAttrType ttype;

    GXGetVtxDesc(GX_VA_NBT, &ntype);
    GXGetVtxDesc(GX_VA_TEX0, &ttype);
    GetVertState();
    if (ntype != GX_NONE) {
        GXSetVtxDesc(GX_VA_NBT, GX_DIRECT);
        GXSetVtxAttrFmt(GX_VTXFMT3, GX_VA_NBT, GX_TEX_ST, GX_RGBA6, 0);
    }
    if (ttype != GX_NONE) {
        GXSetVtxDesc(GX_VA_TEX0, GX_DIRECT);
        GXSetVtxAttrFmt(GX_VTXFMT3, GX_VA_TEX0, GX_TEX_ST, GX_RGB8, 0);
    }

    GXBegin(GX_QUADS, GX_VTXFMT3, 24);
    GXDrawCubeFace(-1.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f,
                   ntype, ttype);
    GXDrawCubeFace(1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, -1.0f,
                   ntype, ttype);
    GXDrawCubeFace(0.0f, -1.0f, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f,
                   ntype, ttype);
    GXDrawCubeFace(0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, -1.0f, 0.0f, 0.0f,
                   ntype, ttype);
    GXDrawCubeFace(0.0f, 0.0f, -1.0f, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f, 0.0f,
                   ntype, ttype);
    GXDrawCubeFace(0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f,
                   ntype, ttype);
    GXEnd();

    RestoreVertState();
}
