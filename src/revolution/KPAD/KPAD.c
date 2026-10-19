#include <revolution/KPAD/KPAD.h>
#include <revolution/OS.h>
#include <revolution/version.h>

#include <cmath>
#include <cstring>

RVL_LIB_VERSION(KPAD, "Jun 28 2007", "02:03:26", "0x4199_60831");

static f32 sUnitScale = 1.0f;
static Vec2 sDownVec = {0.0f, -1.0f};
static Vec2 sRightVec = {1.0f, 0.0f};
static f32 sSensorHeight = 0.2f;
static f32 sHoriGain = 0.05f;
static f32 sHoriThreshold = 0.07f;
static f32 sHoriSmooth = 0.06f;
static u16 sHoriSteadyCount = 100;
static f32 sDpdMargin = 0.05f;
static f32 sDpdSensitivity = 3.0f;
static f32 sDpdUnit = 0.04f;
static f32 sObjLimitA = 0.9f;
static f32 sObjLimitB = 0.9f;
static f32 sObjLimitC = 0.9f;
static f32 sObjLimitD = 0.7f;
static f32 sObjLimitE = 0.1f;
static s32 sFsStickMin = 15;
static s32 sFsStickMax = 71;
static s32 sClStickMin = 60;
static s32 sClStickMax = 308;
static s32 sClTrigMin = 30;
static s32 sClTrigMax = 180;
static f32 sAccLimit = 3.4f;
static f32 sExAccLimit = 2.1f;
static f32 sTiltDegrees = 24.0f;

// DPD mode and data format wanted for each device (core, freestyle, classic)
// without and with the pointer.
static u8 sDpdModes[][2] = {{0, 1}, {3, 2}, {0, 4}, {1, 5}, {0, 7}, {1, 8}};

// Uninitialised statics are laid out last-defined-first in .sbss.
static f32 sUnk824;
static f32 sUnk820;
static Vec2 sZeroVec;
static s32 sStickCircle; // nonzero: round the stick range instead of a square
static Vec2 sDpdOrigin;

typedef void (*KPADSamplingCallback)(s32 chan);

// DPD object as seen by the pointer tracking.
typedef struct KPADDpdObj {
    Vec2 pos;    // at 0x0
    s8 flags;
    s8 synthetic;
    u8 UNK_0xA[2];
} KPADDpdObj;

// One WPAD report, plus the data format it was read in.
typedef struct KPADSample {
    union {
        WPADStatus core;
        WPADFSStatus fs;
        WPADCLStatus cl;
    } u;                // at 0x00
    u8 format;          // at 0x36
    u8 UNK_0x37;
} KPADSample;

// Channel state. The first 0x84 bytes are the KPADStatus handed out by
// KPADRead; the rest is working state. Only the fields the functions in this
// file touch are named; everything else is reserved so the block keeps its
// 0x528-byte size.
typedef struct KPADControlBlock {
    KPADStatus status;       // at 0x00
    f32 posPlayRadius;       // at 0x84
    f32 posSensitivity;      // at 0x88
    f32 horiPlayRadius;      // at 0x8C
    f32 horiSensitivity;     // at 0x90
    f32 distPlayRadius;      // at 0x94
    f32 distSensitivity;     // at 0x98
    f32 accPlayRadius;       // at 0x9C
    f32 accSensitivity;      // at 0xA0
    f32 unitScale;           // at 0xA4
    Vec2 downVec;            // at 0xA8
    Vec2 rightVec;           // at 0xB0
    Vec2 dpdOrigin;          // at 0xB8
    f32 originScale;         // at 0xC0
    KPADDpdObj dpdObj[6];    // at 0xC4
    s16 validObjCount;       // at 0x10C
    u8 sampleIndex;          // at 0x10E
    u8 sampleCount;          // at 0x10F
    KPADSample samples[KPAD_MAX_SAMPLES]; // at 0x110
    f32 objDistance;         // at 0x490
    Vec2 objDir;             // at 0x494
    f32 objAngle;            // at 0x49C
    f32 stableObjDistance;   // at 0x4A0
    Vec accRaw;              // at 0x4A4
    Vec2 UNK_0x4B0;          // at 0x4B0
    Vec2 horizon;            // at 0x4B8
    Vec2 horizonSmooth;      // at 0x4C0
    u16 steadyCount;         // at 0x4C8
    u8 stableFrames;         // at 0x4CA
    u8 UNK_0x4CB;
    u16 repeatCount;         // at 0x4CC
    u16 repeatNext;          // at 0x4CE
    u16 repeatDelay;         // at 0x4D0
    u16 repeatPulse;         // at 0x4D2
    u16 repeatCountCl;       // at 0x4D4
    u16 repeatNextCl;        // at 0x4D6
    KPADCallback controlDpdCB; // at 0x4D8
    f32 accGain[3];          // at 0x4DC
    f32 exAccGain[3];        // at 0x4E8
    f32 winMinX;           // at 0x4F4
    f32 winMinY;           // at 0x4F8
    f32 winMaxX;           // at 0x4FC
    f32 winMaxY;           // at 0x500
    f32 invDpdUnit;          // at 0x504
    f32 negInvDpdUnit;       // at 0x508
    f32 horiThresholdSq;     // at 0x50C
    f32 UNK_0x510;           // at 0x510
    f32 UNK_0x514;           // at 0x514
    KPADSamplingCallback samplingCB; // at 0x518
    u8 readBusy;             // at 0x51C
    u8 resetPending;         // at 0x51D
    u8 devChanged;           // at 0x51E
    u8 dpdCtrlBusy;          // at 0x51F
    u8 UNK_0x520;            // at 0x520
    u8 dpdMode;              // at 0x521
    u8 dpdOffNotified;       // at 0x522
    u8 dpdOnNotified;        // at 0x523
    u8 dpdEnableX;           // at 0x524
    u8 dpdEnableY;           // at 0x525
    u8 tiltEnabled;          // at 0x526
    u8 UNK_0x527;
} KPADControlBlock;

static KPADControlBlock _kpadcb[KPAD_MAX_CONTROLLERS];
static Mtx sTiltMtx;

void reset_kpad(KPADControlBlock* p);
void controlDpdCallback(s32 chan, s32 result);
void calc_button_repeat(KPADControlBlock* p, u8 devType, s32 dt);
void calc_acc_horizon(KPADControlBlock* p);
void calc_acc_vertical(KPADControlBlock* p);
void calc_acc(KPADControlBlock* p, KPADSample* pSample);
s8 select_2obj_first(KPADControlBlock* p);
s8 select_2obj_continue(KPADControlBlock* p);
s8 select_1obj_first(KPADControlBlock* p);
s8 select_1obj_continue(KPADControlBlock* p);
void calc_dpd_variable(KPADControlBlock* p, s8 valid);
void read_kpad_dpd(KPADControlBlock* p, KPADSample* pSample);
void clamp_stick_circle(Vec2* pOut, s32 x, s32 y, s32 min, s32 max);
void clamp_stick_cross(Vec2* pOut, s32 x, s32 y, s32 min, s32 max);
void read_kpad_stick(KPADControlBlock* p, KPADSample* pSample);
void kpad_sampling_callback(s32 chan);
static DECOMP_INLINE void KPADReset(void);

static DECOMP_INLINE void KPADSetBtnRepeat(s32 chan, f32 delaySec,
                                           f32 pulseSec) {
    KPADControlBlock* p = &_kpadcb[chan];

    if (pulseSec) {
        p->repeatDelay = (u16)(s32)(delaySec + 0.5f);
        p->repeatPulse = (u16)(s32)(pulseSec + 0.5f);
    } else {
        p->repeatDelay = 40000;
        p->repeatPulse = 0;
    }

    p->repeatCount = 0;
    p->repeatNext = p->repeatDelay;
    p->repeatCountCl = 0;
    p->repeatNextCl = p->repeatDelay;
}

void KPADSetPosParam(s32 chan, f32 playRadius, f32 sensitivity) {
    KPADControlBlock* p = &_kpadcb[chan];

    // Keep the unit's first .sdata2 entries in the target order without an
    // extra function symbol.
    (void)0.0f;
    (void)0.5f;
    (void)0.383864f;

    p->posPlayRadius = playRadius;
    p->posSensitivity = sensitivity;
}

static DECOMP_INLINE void KPADSetHoriParam(s32 chan, f32 playRadius,
                                           f32 sensitivity) {
    KPADControlBlock* p = &_kpadcb[chan];

    p->horiPlayRadius = playRadius;
    p->horiSensitivity = sensitivity;
}

void KPADSetDistParam(s32 chan, f32 playRadius, f32 sensitivity) {
    KPADControlBlock* p = &_kpadcb[chan];

    p->distPlayRadius = playRadius;
    p->distSensitivity = sensitivity;
}

static DECOMP_INLINE void KPADSetAccParam(s32 chan, f32 playRadius,
                                          f32 sensitivity) {
    KPADControlBlock* p = &_kpadcb[chan];

    p->accPlayRadius = playRadius;
    p->accSensitivity = sensitivity;
}

// Sets the distance between the sensor bar's two light clusters, in metres.
static DECOMP_INLINE void KPADSetObjInterval(f32 interval) {
    BOOL enabled = OSDisableInterrupts();

    sSensorHeight = interval;
    sUnk820 = sUnk824 = interval / 0.383864f;

    OSRestoreInterrupts(enabled);
}

void reset_kpad(KPADControlBlock* p) {
    KPADDpdObj* obj;

    p->resetPending = 0;

    p->winMinX = -1.0f + sDpdMargin;
    p->winMaxX = 1.0f - sDpdMargin;
    p->winMinY = -0.75f + sDpdMargin;
    p->winMaxY = 0.75f - sDpdMargin;
    p->invDpdUnit = 1.0f / sDpdUnit;
    p->negInvDpdUnit = -1.0f / sDpdUnit;
    p->horiThresholdSq = sHoriThreshold * sHoriThreshold;
    p->UNK_0x514 = sUnk824;
    p->UNK_0x510 = sUnk820;

    p->status.release = 0;
    p->status.trig = 0;
    p->status.hold = 0;
    p->repeatCount = 0;
    p->repeatNext = p->repeatDelay;
    p->status.dpd_valid_fg = 0;
    p->stableFrames = 0;

    p->status.pos = p->status.vec = sZeroVec;
    p->horizon.x = 1.0f;
    p->horizon.y = 0.0f;
    p->status.speed = 0.0f;
    p->UNK_0x4B0.x = 1.0f;
    p->status.horizon.x = 1.0f;
    p->UNK_0x4B0.y = 0.0f;
    p->status.horizon.y = 0.0f;
    p->status.hori_vec = sZeroVec;
    p->status.acc.z = 0.0f;
    p->status.acc.x = 0.0f;
    p->status.acc.y = -1.0f;
    p->status.hori_speed = 0.0f;
    p->status.acc_vertical.x = 1.0f;
    p->status.acc_vertical.y = 0.0f;
    p->status.dist = p->unitScale;
    p->status.dist_speed = 0.0f;
    p->status.dist_vec = 0.0f;
    p->objAngle = p->unitScale;
    p->objDistance = p->stableObjDistance = p->UNK_0x510 / p->unitScale;
    p->objDir = p->rightVec;
    p->status.acc_value = 1.0f;
    p->status.acc_speed = 0.0f;
    p->accRaw = p->status.acc;
    p->horizonSmooth = p->horizon;
    p->steadyCount = sHoriSteadyCount;
    p->validObjCount = 0;

    obj = &p->dpdObj[3];
    do {
        obj->flags = -1;
    } while (--obj >= &p->dpdObj[0]);

    obj = &p->dpdObj[5];
    do {
        obj->flags = -1;
    } while (--obj >= &p->dpdObj[4]);

    p->sampleCount = 0;
    p->devChanged = 1;
}

// Maps a position from the unit area described by pArea onto the screen.
// pArea holds the area's bounds as {x, top, ?, bottom} in its 1st and 3rd
// entries, pPos is the position to map and scale stretches the result.
void KPADGetProjectionPos(Vec2* pOut, const Vec2* pPos,
                          const f32* pArea, f32 scale) {
    f32 ratio = 1.2f;
    f32 half = 0.5f;
    f32 height = pArea[3] - pArea[1];
    f32 size = height * half;
    f32 x = ratio * (pPos->x * size);
    f32 y = ratio * (pPos->y * size);

    pOut->x = x * (0.908 * scale);
    pOut->y = y;
}

void KPADEnableDPD(s32 chan) {
    KPADControlBlock* p = &_kpadcb[chan];

    p->dpdEnableX = TRUE;
    p->dpdEnableY = TRUE;
}

void calc_button_repeat(KPADControlBlock* p, u8 devType, s32 dt) {
    if (p->status.trig != 0 || p->status.release != 0) {
        p->repeatCount = 0;
        p->repeatNext = p->repeatDelay;

        if (p->status.trig != 0 && p->repeatPulse != 0) {
            p->status.hold |= 0x80000000;
        }
    } else if (p->status.hold != 0) {
        p->repeatCount += dt;

        if (p->repeatCount >= 40000) {
            p->repeatCount -= 40000;
        }

        if (p->repeatCount >= p->repeatNext) {
            p->status.hold |= 0x80000000;
            p->repeatNext += p->repeatPulse;

            if (p->repeatCount >= 20000) {
                p->repeatCount -= 20000;
                p->repeatNext -= 20000;
            }
        }
    }

    if (devType == 2) {
        if (p->status.ex_status.cl.trig != 0 ||
            p->status.ex_status.cl.release != 0) {
            p->repeatCountCl = 0;
            p->repeatNextCl = p->repeatDelay;

            if (p->status.ex_status.cl.trig != 0 && p->repeatPulse != 0) {
                p->status.ex_status.cl.hold |= 0x80000000;
            }
        } else if (p->status.ex_status.cl.hold != 0) {
            p->repeatCountCl += dt;

            if (p->repeatCountCl >= 40000) {
                p->repeatCountCl -= 40000;
            }

            if (p->repeatCountCl >= p->repeatNextCl) {
                p->status.ex_status.cl.hold |= 0x80000000;
                p->repeatNextCl += p->repeatPulse;

                if (p->repeatCountCl >= 20000) {
                    p->repeatCountCl -= 20000;
                    p->repeatNextCl -= 20000;
                }
            }
        }
    }
}

void calc_acc_horizon(KPADControlBlock* p) {
    f32 mag;
    f32 ax;
    f32 ay;
    f32 nx;
    f32 ny;
    f32 len;
    f32 sx;
    f32 sy;
    f32 dx;
    f32 dy;

    mag = sqrt(p->accRaw.x * p->accRaw.x + p->accRaw.y * p->accRaw.y);

    if (mag == 0.0f || mag >= 2.0f) {
        return;
    }

    ax = p->accRaw.x / mag;
    ay = p->accRaw.y / mag;

    if (mag > 1.0f) {
        mag = 2.0f - mag;
    }

    mag *= mag * sHoriGain;

    nx = p->horizon.x + mag * ((p->downVec.x * ax + p->downVec.y * ay) - p->horizon.x);
    ny = p->horizon.y + mag * ((p->downVec.y * ax - p->downVec.x * ay) - p->horizon.y);

    len = sqrt(nx * nx + ny * ny);

    if (len != 0.0f) {
        p->horizon.x = nx / len;
        p->horizon.y = ny / len;

        p->horizonSmooth.x = p->horizonSmooth.x + sHoriSmooth * (p->horizon.x - p->horizonSmooth.x);
        dx = p->horizon.x - p->horizonSmooth.x;
        p->horizonSmooth.y = p->horizonSmooth.y + sHoriSmooth * (p->horizon.y - p->horizonSmooth.y);
        dy = p->horizon.y - p->horizonSmooth.y;

        if (dx * dx + dy * dy <= p->horiThresholdSq) {
            if (p->steadyCount != 0) {
                p->steadyCount--;
            }
        } else {
            p->steadyCount = sHoriSteadyCount;
        }
    }
}

void calc_acc_vertical(KPADControlBlock* p) {
    f32 nx;
    f32 ny;
    f32 xy;
    f32 horiz;
    f32 vert;
    f32 mag;
    f32 len;
    f32 hn;

    xy = p->accRaw.x * p->accRaw.x + p->accRaw.y * p->accRaw.y;
    horiz = sqrt(xy);
    vert = -p->accRaw.z;
    mag = sqrt(xy + vert * vert);

    if (mag == 0.0f || mag >= 2.0f) {
        return;
    }

    hn = horiz / mag;
    vert /= mag;

    if (mag > 1.0f) {
        mag = 2.0f - mag;
    }

    mag *= mag * sHoriGain;

    nx = p->status.acc_vertical.x + mag * (hn - p->status.acc_vertical.x);
    ny = p->status.acc_vertical.y + mag * (vert - p->status.acc_vertical.y);

    len = sqrt(nx * nx + ny * ny);

    if (len != 0.0f) {
        p->status.acc_vertical.x = nx / len;
        p->status.acc_vertical.y = ny / len;
    }
}

static f32 clamp_acc(f32 value, f32 limit) {
    if (value < 0.0f) {
        if (value < -limit)
            return -limit;
    } else {
        if (value > limit)
            return limit;
    }
    return value;
}

// Moves one acceleration component towards its newest sample, more slowly the
// closer it already is.
static void smooth_acc(KPADControlBlock* p, f32* pValue, f32 target) {
    f32 f, diff;

    diff = target - *pValue;

    if (diff < 0.0f) {
        f = -diff;
    } else {
        f = diff;
    }

    if (f >= p->accPlayRadius) {
        f = 1.0f;
    } else {
        f /= p->accPlayRadius;
        f *= f;
        f *= f;
    }
    f *= p->accSensitivity;
    *pValue += f * diff;
}

// Converts the accelerometer values of the newest report, of the Wii Remote
// and of the Nunchuk, into smoothed accelerations.
void calc_acc(KPADControlBlock* p, KPADSample* pSample) {
    KPADStatus* sp = &p->status;
    Vec ex;
    Vec old;

    switch (pSample->format) {
    case WPAD_FMT_CORE_BTN_ACC:
    case WPAD_FMT_CORE_BTN_ACC_DPD:
    case WPAD_FMT_FS_BTN_ACC:
    case WPAD_FMT_FS_BTN_ACC_DPD:
    case WPAD_FMT_CLASSIC_BTN_ACC:
    case WPAD_FMT_CLASSIC_BTN_ACC_DPD:
        break;

    default:
        return;
    }

    p->accRaw.x = clamp_acc((f32)(s32)-pSample->u.core.accX * p->accGain[0], sAccLimit);
    p->accRaw.y = clamp_acc((f32)(s32)-pSample->u.core.accZ * p->accGain[2], sAccLimit);
    p->accRaw.z = clamp_acc((f32)(s32)pSample->u.core.accY * p->accGain[1], sAccLimit);

    old = sp->acc;

    smooth_acc(p, &sp->acc.x, p->accRaw.x);
    smooth_acc(p, &sp->acc.y, p->accRaw.y);
    smooth_acc(p, &sp->acc.z, p->accRaw.z);
    sp->acc_value = sqrt(sp->acc.x * sp->acc.x + sp->acc.y * sp->acc.y + sp->acc.z * sp->acc.z);

    old.x -= sp->acc.x;
    old.y -= sp->acc.y;
    old.z -= sp->acc.z;
    sp->acc_speed = sqrt(old.x * old.x + old.y * old.y + old.z * old.z);
    calc_acc_horizon(p);
    calc_acc_vertical(p);
    if (pSample->u.fs.err != WPAD_ERR_OK || pSample->u.fs.dev != WPAD_DEV_FREESTYLE ||
        (pSample->format != WPAD_FMT_FS_BTN_ACC && pSample->format != WPAD_FMT_FS_BTN_ACC_DPD)) {
        return;
    }

    ex.x = clamp_acc((f32)(s32)-pSample->u.fs.fsAccX * p->exAccGain[0], sExAccLimit);
    ex.y = clamp_acc((f32)(s32)-pSample->u.fs.fsAccZ * p->exAccGain[2], sExAccLimit);
    ex.z = clamp_acc((f32)(s32)pSample->u.fs.fsAccY * p->exAccGain[1], sExAccLimit);

    if (p->tiltEnabled) {
        PSMTXMultVec(sTiltMtx, &ex, &ex);
    }

    old = sp->ex_status.fs.acc;

    smooth_acc(p, &sp->ex_status.fs.acc.x, ex.x);
    smooth_acc(p, &sp->ex_status.fs.acc.y, ex.y);
    smooth_acc(p, &sp->ex_status.fs.acc.z, ex.z);
    sp->ex_status.fs.acc_value = sqrt(sp->ex_status.fs.acc.x * sp->ex_status.fs.acc.x + sp->ex_status.fs.acc.y * sp->ex_status.fs.acc.y +
                                      sp->ex_status.fs.acc.z * sp->ex_status.fs.acc.z);

    old.x -= sp->ex_status.fs.acc.x;
    old.y -= sp->ex_status.fs.acc.y;
    old.z -= sp->ex_status.fs.acc.z;
    sp->ex_status.fs.acc_speed = sqrt(old.x * old.x + old.y * old.y + old.z * old.z);
}

// Picks the pair of DPD objects that best matches the two ends of the
// sensor bar, ordered so the bar points to the right. Returns 2 when a pair is
// found.
static f32 calc_horizon(KPADControlBlock* p, Vec2* p1, Vec2* p2, Vec2* pHori) {
    f32 inv, len, vx, vy;

    vx = p2->x - p1->x;
    vy = p2->y - p1->y;
    len = sqrt(vx * vx + vy * vy);
    inv = 1.0f / len;
    vx *= inv;
    vy *= inv;

    pHori->x = p->rightVec.x * vx + p->rightVec.y * vy;
    pHori->y = p->rightVec.y * vx - p->rightVec.x * vy;

    return p->UNK_0x510 * inv;
}

s8 select_2obj_first(KPADControlBlock* p) {
    KPADDpdObj *a, *b, *sel1, *sel2;
    Vec2 hori;
    f32 t, best = sObjLimitA;

    a = &p->dpdObj[0];
    do {
        if (a->flags != 0) {
            continue;
        }

        b = a + 1;
        do {
            if (b->flags != 0) {
                continue;
            }

            t = calc_horizon(p, &a->pos, &b->pos, &hori);

            if (t <= p->UNK_0x514 || t >= sDpdSensitivity) {
                continue;
            }

            t = p->horizon.x * hori.x + p->horizon.y * hori.y;
            if (t < 0.0f) {
                if (-t > best) {
                    best = -t;
                    sel1 = b;
                    sel2 = a;
                }
            } else {
                if (t > best) {
                    best = t;
                    sel1 = a;
                    sel2 = b;
                }
            }
        } while (++b <= &p->dpdObj[3]);
    } while (++a < &p->dpdObj[3]);

    if (best == sObjLimitA) {
        return 0;
    }

    p->dpdObj[4] = *sel1;
    p->dpdObj[5] = *sel2;

    return 2;
}

// Like select_2obj_first, but scores each pair by how well it continues the
// previous distance and direction. Returns 2 when a pair is found.
s8 select_2obj_continue(KPADControlBlock* p) {
    KPADDpdObj* a;
    KPADDpdObj* b;
    KPADDpdObj* sel1;
    KPADDpdObj* sel2;
    Vec2 v;
    f32 score;
    f32 dx;
    f32 dy;
    f32 best;
    f32 len;
    f32 dot;
    int flip;

    best = 2.0f;

    a = &p->dpdObj[0];
    do {
        if (a->flags == 0) {
            b = a + 1;
            do {
                if (b->flags != 0) {
                    continue;
                }

                dx = b->pos.x - a->pos.x;
                dy = b->pos.y - a->pos.y;
                len = sqrt(dx * dx + dy * dy);
                score = 1.0f / len;
                v.x = dx * score;
                v.y = dy * score;
                score *= p->UNK_0x510;

                if (score <= p->UNK_0x514 || score >= sDpdSensitivity) {
                    continue;
                }

                score -= p->objAngle;

                if (score < 0.0f) {
                    score *= p->negInvDpdUnit;
                } else {
                    score *= p->invDpdUnit;
                }

                if (score >= 1.0f) {
                    continue;
                }

                dot = p->objDir.x * v.x + p->objDir.y * v.y;

                if (dot < 0.0f) {
                    dot = -dot;
                    flip = 1;
                } else {
                    flip = 0;
                }

                if (dot <= sObjLimitB) {
                    continue;
                }

                score += (1.0f - dot) / (1.0f - sObjLimitB);

                if (score < best) {
                    best = score;

                    if (flip) {
                        sel1 = b;
                        sel2 = a;
                    } else {
                        sel1 = a;
                        sel2 = b;
                    }
                }
            } while (++b <= &p->dpdObj[3]);
        }
    } while (++a < &p->dpdObj[3]);

    if (best == 2.0f) {
        return 0;
    }

    p->dpdObj[4] = *sel1;
    p->dpdObj[5] = *sel2;
    return 2;
}

// Picks the DPD object whose bar ends both lie in the pointer window; used
// for the first frame with a single visible object. Returns -1 once a pair is
// found.
s8 select_1obj_first(KPADControlBlock* p) {
    KPADDpdObj* obj;
    Vec2 a;
    Vec2 b;
    f32 dx;
    f32 dy;

    dx = p->rightVec.x * p->horizon.x + p->rightVec.y * p->horizon.y;
    dy = p->rightVec.y * p->horizon.x - p->rightVec.x * p->horizon.y;
    dx *= p->stableObjDistance;
    dy *= p->stableObjDistance;

    obj = p->dpdObj;
    do {
        if (obj->flags == 0) {
            a.x = obj->pos.x - dx;
            a.y = obj->pos.y - dy;
            b.x = obj->pos.x + dx;
            b.y = obj->pos.y + dy;

            if (a.x <= p->winMinX || a.x >= p->winMaxX || a.y <= p->winMinY ||
                a.y >= p->winMaxY) {
                if (b.x > p->winMinX && b.x < p->winMaxX && b.y > p->winMinY &&
                    b.y < p->winMaxY) {
                    p->dpdObj[5] = *obj;
                    p->dpdObj[4].pos = a;
                    p->dpdObj[4].flags = 0;
                    p->dpdObj[4].synthetic = -1;
                    return -1;
                }
            } else if (b.x <= p->winMinX || b.x >= p->winMaxX ||
                       b.y <= p->winMinY || b.y >= p->winMaxY) {
                p->dpdObj[4] = *obj;
                p->dpdObj[5].pos = b;
                p->dpdObj[5].flags = 0;
                p->dpdObj[5].synthetic = -1;
                return -1;
            }
        }
    } while (++obj < &p->dpdObj[4]);

    return 0;
}

// Keeps tracking with one visible object: pairs the closest object with one
// of the previous end points and rebuilds the other one from the bar
// direction. Returns 0 when nothing is close enough.
s8 select_1obj_continue(KPADControlBlock* p) {
    KPADDpdObj* slot;
    KPADDpdObj* obj;
    KPADDpdObj* bestSlot;
    KPADDpdObj* bestObj;
    f32 best;
    f32 dx;
    f32 dy;
    f32 d;

    best = sObjLimitE * sObjLimitE;

    slot = &p->dpdObj[4];
    do {
        if (slot->flags == 0 && slot->synthetic == 0) {
            obj = &p->dpdObj[0];
            do {
                if (obj->flags == 0) {
                    dx = slot->pos.x - obj->pos.x;
                    dy = slot->pos.y - obj->pos.y;
                    d = dx * dx + dy * dy;

                    if (d < best) {
                        best = d;
                        bestSlot = slot;
                        bestObj = obj;
                    }
                }
            } while (++obj < &p->dpdObj[4]);
        }
    } while (++slot < &p->dpdObj[6]);

    if (best == sObjLimitE * sObjLimitE) {
        return 0;
    }

    *bestSlot = *bestObj;

    p->objDir.x = p->rightVec.x * p->horizon.x + p->rightVec.y * p->horizon.y;
    p->objDir.y = p->rightVec.y * p->horizon.x - p->rightVec.x * p->horizon.y;
    dx = p->objDistance * p->objDir.x;
    dy = p->objDistance * p->objDir.y;

    if (bestSlot == &p->dpdObj[4]) {
        p->dpdObj[5].pos.x = bestSlot->pos.x + dx;
        p->dpdObj[5].pos.y = bestSlot->pos.y + dy;
        p->dpdObj[5].flags = 0;
        p->dpdObj[5].synthetic = -1;
    } else {
        p->dpdObj[4].pos.x = bestSlot->pos.x - dx;
        p->dpdObj[4].pos.y = bestSlot->pos.y - dy;
        p->dpdObj[4].flags = 0;
        p->dpdObj[4].synthetic = -1;
    }

    if (p->status.dpd_valid_fg < 0) {
        return -1;
    }

    return 1;
}

// Smooths the pointer position, the horizon and the distance from the DPD
// objects of the newest report.
void calc_dpd_variable(KPADControlBlock* p, s8 valid) {
    KPADStatus* sp = &p->status;
    f32 f1, f2, dist;
    Vec2 pos, vec;

    if (valid == 0) {
        sp->dpd_valid_fg = 0;
        return;
    }

    pos.x = p->rightVec.x * p->objDir.x + p->rightVec.y * p->objDir.y;
    pos.y = p->rightVec.y * p->objDir.x - p->rightVec.x * p->objDir.y;

    if (sp->dpd_valid_fg == 0) {
        sp->horizon = pos;
        sp->hori_vec = sZeroVec;
        sp->hori_speed = 0.0f;
    } else {
        vec.x = pos.x - sp->horizon.x;
        vec.y = pos.y - sp->horizon.y;
        f1 = sqrt(vec.x * vec.x + vec.y * vec.y);

        if (f1 >= p->horiPlayRadius) {
            f2 = 1.0f;
        } else {
            f2 = f1 / p->horiPlayRadius;
            f2 *= f2;
            f2 *= f2;
        }
        f2 *= p->horiSensitivity;
        vec.x = f2 * vec.x + sp->horizon.x;
        vec.y = f2 * vec.y + sp->horizon.y;
        f1 = sqrt(vec.x * vec.x + vec.y * vec.y);
        vec.x /= f1;
        vec.y /= f1;

        sp->hori_vec.x = vec.x - sp->horizon.x;
        sp->hori_vec.y = vec.y - sp->horizon.y;
        sp->hori_speed = sqrt(sp->hori_vec.x * sp->hori_vec.x + sp->hori_vec.y * sp->hori_vec.y);

        sp->horizon = vec;
    }

    dist = p->UNK_0x510 / p->objDistance;

    if (sp->dpd_valid_fg == 0) {
        sp->dist = dist;
        sp->dist_vec = 0.0f;
        sp->dist_speed = 0.0f;
    } else {
        f2 = dist - sp->dist;
        if (f2 < 0.0f) {
            f1 = -f2;
        } else {
            f1 = f2;
        }

        if (f1 >= p->distPlayRadius) {
            f1 = 1.0f;
        } else {
            f1 /= p->distPlayRadius;
            f1 *= f1;
            f1 *= f1;
        }
        f1 *= p->distSensitivity;

        sp->dist_vec = f1 * f2;
        if (sp->dist_vec < 0.0f) {
            sp->dist_speed = -sp->dist_vec;
        } else {
            sp->dist_speed = sp->dist_vec;
        }

        sp->dist += sp->dist_vec;
    }

    pos.x = (p->dpdObj[4].pos.x + p->dpdObj[5].pos.x) * 0.5f;
    pos.y = (p->dpdObj[4].pos.y + p->dpdObj[5].pos.y) * 0.5f;

    f1 = p->objDir.x * p->rightVec.x + p->objDir.y * p->rightVec.y;
    f2 = -p->objDir.y * p->rightVec.x + p->objDir.x * p->rightVec.y;
    vec.x = f1 * pos.x - f2 * pos.y;
    vec.y = f2 * pos.x + f1 * pos.y;

    vec.x = (p->dpdOrigin.x - vec.x) * p->originScale;
    vec.y = (p->dpdOrigin.y - vec.y) * p->originScale;

    pos.x = -p->downVec.y * vec.x + p->downVec.x * vec.y;
    pos.y = -p->downVec.x * vec.x - p->downVec.y * vec.y;
    if (sp->dpd_valid_fg == 0) {
        sp->pos = pos;
        sp->vec = sZeroVec;
        sp->speed = 0.0f;
    } else {
        vec.x = pos.x - sp->pos.x;
        vec.y = pos.y - sp->pos.y;
        f1 = sqrt(vec.x * vec.x + vec.y * vec.y);

        if (f1 >= p->posPlayRadius) {
            f2 = 1.0f;
        } else {
            f2 = f1 / p->posPlayRadius;
            f2 *= f2;
            f2 *= f2;
        }
        f2 *= p->posSensitivity;

        sp->vec.x = f2 * vec.x;
        sp->vec.y = f2 * vec.y;
        sp->speed = sqrt(sp->vec.x * sp->vec.x + sp->vec.y * sp->vec.y);

        sp->pos.x += sp->vec.x;
        sp->pos.y += sp->vec.y;
    }

    sp->dpd_valid_fg = valid;
}

// Converts the DPD objects of the newest report, marks the ones that fall
// outside the pointer window or repeat another one, and picks the pair (or
// single object) the pointer is derived from.
void read_kpad_dpd(KPADControlBlock* p, KPADSample* pSample) {
    DPDObject* src;
    KPADDpdObj* b;
    KPADDpdObj* a;
    KPADDpdObj* first;
    KPADDpdObj* dst;
    KPADDpdObj* last;
    s8 sel;
    f32 inv;
    f32 len;
    f32 dy;
    f32 dx;

    if (pSample->format == WPAD_FMT_CORE_BTN_ACC_DPD ||
        pSample->format == WPAD_FMT_FS_BTN_ACC_DPD ||
        pSample->format == WPAD_FMT_CLASSIC_BTN_ACC_DPD) {
        src = &pSample->u.core.obj[3];
        dst = &p->dpdObj[3];
        do {
            if (src->size != 0) {
                dst->pos.x = 0.001953125f * src->x - 0.99902344f;
                dst->pos.y = 0.001953125f * src->y - 0.74902344f;
                dst->flags = 0;
                dst->synthetic = 0;
            } else {
                dst->flags = -1;
            }

            dst--;
            src--;
        } while (dst >= &p->dpdObj[0]);
    } else {
        dst = &p->dpdObj[3];
        do {
            dst->flags = -1;
        } while (--dst >= &p->dpdObj[0]);
    }

    last = &p->dpdObj[3];
    first = &p->dpdObj[0];
    dst = last;
    do {
        if (dst->flags >= 0 &&
            (dst->pos.x <= p->winMinX || dst->pos.x >= p->winMaxX ||
             dst->pos.y <= p->winMinY || dst->pos.y >= p->winMaxY)) {
            dst->flags |= 1;
        }
    } while (--dst >= first);

    a = first;
    do {
        if (a->flags == 0) {
            b = a + 1;
            do {
                if (b->flags == 0 && a->pos.x == b->pos.x &&
                    a->pos.y == b->pos.y) {
                    b->flags |= 2;
                }
            } while (++b <= last);
        }
    } while (++a < last);

    dst = &p->dpdObj[3];
    p->validObjCount = 0;
    do {
        if (dst->flags == 0) {
            p->validObjCount++;
        }
    } while (--dst >= &p->dpdObj[0]);

    if (p->status.acc_vertical.x <= sObjLimitD) {
        goto none;
    }

    {
        s8 mode = p->status.dpd_valid_fg;

        if (mode == 2 || mode == -2) {
            if (p->validObjCount >= 2) {
                sel = select_2obj_continue(p);
                if (sel != 0) {
                    goto found;
                }
            }

            if (p->validObjCount >= 1) {
                sel = select_1obj_continue(p);
                if (sel != 0) {
                    goto found;
                }
            }
        } else if (mode == 1 || mode == -1) {
            if (p->validObjCount >= 2) {
                sel = select_2obj_first(p);
                if (sel != 0) {
                    goto found;
                }
            }

            if (p->validObjCount >= 1) {
                sel = select_1obj_continue(p);
                if (sel != 0) {
                    goto found;
                }
            }
        } else {
            if (p->validObjCount >= 2) {
                sel = select_2obj_first(p);
                if (sel != 0) {
                    goto found;
                }
            }

            if (p->validObjCount == 1) {
                sel = select_1obj_first(p);
                if (sel != 0) {
                    goto found;
                }
            }
        }
    }

none:
    sel = 0;
found:
    if (sel != 0) {
        dx = p->dpdObj[5].pos.x - p->dpdObj[4].pos.x;
        dy = p->dpdObj[5].pos.y - p->dpdObj[4].pos.y;
        len = sqrt(dx * dx + dy * dy);
        inv = 1.0f / len;
        p->objDistance = len;
        dx *= inv;
        dy *= inv;
        p->objDir.x = dx;
        p->objAngle = p->UNK_0x510 * inv;
        p->objDir.y = dy;
        p->UNK_0x4B0.x = p->rightVec.x * dx + p->rightVec.y * dy;
        p->UNK_0x4B0.y = p->rightVec.y * dx - p->rightVec.x * dy;

        if (p->steadyCount == 0) {
            if (p->UNK_0x4B0.x * p->horizon.x + p->UNK_0x4B0.y * p->horizon.y <=
                sObjLimitC) {
                sel = 0;
                p->dpdObj[5].flags = 1;
                p->dpdObj[4].flags = 1;
            }
        }

        if (p->status.dpd_valid_fg == 2 && sel == 2) {
            if (p->stableFrames == 200) {
                p->stableObjDistance = p->objDistance;
            } else {
                p->stableFrames++;
            }
        } else {
            p->stableFrames = 0;
        }
    } else {
        p->stableFrames = 0;
    }

    calc_dpd_variable(p, sel);
}

// Maps a stick position to a unit-circle position, with a dead zone up to
// min and full deflection from max on.
void clamp_stick_circle(Vec2* pOut, s32 x, s32 y, s32 min, s32 max) {
    f32 fx = x;
    f32 fy = y;
    f32 fmin = min;
    f32 fmax = max;
    f32 len;
    f32 scale;

    len = sqrt(fx * fx + fy * fy);

    if (len <= fmin) {
        pOut->x = pOut->y = 0.0f;
    } else if (len >= fmax) {
        pOut->x = fx / len;
        pOut->y = fy / len;
    } else {
        scale = (len - fmin) / (fmax - fmin) / len;
        pOut->x = fx * scale;
        pOut->y = fy * scale;
    }
}

// Maps a stick position to a square, with a dead zone up to min and full
// deflection from max on, then pulls corners back onto the unit circle.
void clamp_stick_cross(Vec2* pOut, s32 x, s32 y, s32 min, s32 max) {
    f32 len;

    if (x < 0) {
        if (-x <= min) {
            pOut->x = 0.0f;
        } else if (-x >= max) {
            pOut->x = 1.0f;
        } else {
            pOut->x = (f32)(-(x + min)) / (f32)(max - min);
        }

        pOut->x = -pOut->x;
    } else if (x <= min) {
        pOut->x = 0.0f;
    } else if (x >= max) {
        pOut->x = 1.0f;
    } else {
        pOut->x = (f32)(x - min) / (f32)(max - min);
    }

    if (y < 0) {
        if (-y <= min) {
            pOut->y = 0.0f;
        } else if (-y >= max) {
            pOut->y = 1.0f;
        } else {
            pOut->y = (f32)(-(y + min)) / (f32)(max - min);
        }

        pOut->y = -pOut->y;
    } else if (y <= min) {
        pOut->y = 0.0f;
    } else if (y >= max) {
        pOut->y = 1.0f;
    } else {
        pOut->y = (f32)(y - min) / (f32)(max - min);
    }

    if (pOut->x * pOut->x + pOut->y * pOut->y > 1.0f) {
        len = sqrt(pOut->x * pOut->x + pOut->y * pOut->y);
        pOut->x = pOut->x / len;
        pOut->y = pOut->y / len;
    }
}

// Converts the extension controller part of a report into stick/trigger
// positions in the channel's extension status.
static inline void calc_trigger(f32* pOut, s32 value, s32 min, s32 max) {
    if (value <= min) {
        *pOut = 0.0f;
    } else if (value >= max) {
        *pOut = 1.0f;
    } else {
        *pOut = (f32)(value - min) / (f32)(max - min);
    }
}

void read_kpad_stick(KPADControlBlock* p, KPADSample* pSample) {
    void (*clamp)(Vec2*, s32, s32, s32, s32) = clamp_stick_cross;
    KPADEXStatus* ex = &p->status.ex_status;

    if (sStickCircle) {
        clamp = clamp_stick_circle;
    }

    if (pSample->u.core.dev == WPAD_DEV_FREESTYLE &&
        (u8)(pSample->format + 253) <= 2) {
        if (p->devChanged) {
            p->devChanged = 0;
            ex->fs.stick = sZeroVec;
            ex->fs.acc.z = 0.0f;
            ex->fs.acc.x = 0.0f;
            ex->fs.acc.y = -1.0f;
            ex->fs.acc_value = 1.0f;
            ex->fs.acc_speed = 0.0f;
        }

        clamp(&ex->fs.stick, pSample->u.fs.fsStickX, pSample->u.fs.fsStickY,
              sFsStickMin, sFsStickMax);
    } else if (pSample->u.core.dev == WPAD_DEV_CLASSIC &&
               (u8)(pSample->format + 250) <= 2) {
        if (p->devChanged) {
            p->devChanged = 0;
            ex->cl.lstick = sZeroVec;
            ex->cl.rstick = sZeroVec;
            ex->cl.rtrigger = 0.0f;
            ex->cl.ltrigger = 0.0f;
            ex->cl.release = 0;
            ex->cl.trig = 0;
            ex->cl.hold = 0;
            p->repeatCountCl = 0;
            p->repeatNextCl = p->repeatDelay;
        }

        clamp(&ex->cl.lstick, pSample->u.cl.clLStickX, pSample->u.cl.clLStickY,
              sClStickMin, sClStickMax);
        clamp(&ex->cl.rstick, pSample->u.cl.clRStickX, pSample->u.cl.clRStickY,
              sClStickMin, sClStickMax);

        calc_trigger(&ex->cl.ltrigger, pSample->u.cl.clTriggerL, sClTrigMin, sClTrigMax);

        calc_trigger(&ex->cl.rtrigger, pSample->u.cl.clTriggerR, sClTrigMin, sClTrigMax);
    }
}

// Works out how far the pointer may move from the centre of the sensor bar
// image, from the offset the sensor bar introduces.
static inline void calc_dpd_limit(KPADControlBlock* p) {
    f32 w = 1.0f;
    f32 h = 0.75f;
    f32 len = sqrt(w * w + h * h);

    if (p->dpdOrigin.x < 0.0f) {
        w += p->dpdOrigin.x;
    } else {
        w -= p->dpdOrigin.x;
    }

    if (p->dpdOrigin.y < 0.0f) {
        h += p->dpdOrigin.y;
    } else {
        h -= p->dpdOrigin.y;
    }

    w = w < h ? w : h;
    p->originScale = len / w;
}

static void read_kpad_button(KPADControlBlock* p, u32 devType, u32 count, u32 core, u32 fs, u32 cl) {
    KPADStatus* sp = &p->status;
    KPADEXStatus* ep = &p->status.ex_status;
    u32 old, change;
    u32 clOld, clChange;

    old = sp->hold & 0xFFFF;
    sp->hold = (core & 0x9FFF) | (fs & 0x6000);
    change = sp->hold ^ old;
    sp->trig = change & sp->hold;
    sp->release = change & old;

    if (devType == WPAD_DEV_CLASSIC) {
        clOld = ep->cl.hold & 0xFFFF;
        ep->cl.hold = cl & 0xFFFF;
        clChange = ep->cl.hold ^ clOld;
        ep->cl.trig = clChange & ep->cl.hold;
        ep->cl.release = clChange & clOld;
    }

    calc_button_repeat(p, devType, count);
}

s32 KPADRead(s32 chan, KPADStatus* pSamples, u32 numSamples) {
    KPADControlBlock* p = &_kpadcb[chan];
    KPADSample sample;
    KPADSample* pSample;
    KPADStatus* pDst;
    s32 devType;
    BOOL enabled;
    u32 result;
    u32 count;
    u32 n;
    int k;
    int index;
    u32 btn;
    u32 btnFs;
    u32 btnCl;
    u8 dev;

    result = 0;

    if (WPADGetStatus() != WPAD_LIB_STATUS_3) {
        return 0;
    }

    enabled = OSDisableInterrupts();

    if (p->readBusy != 0) {
        OSRestoreInterrupts(enabled);
        return 0;
    }

    p->readBusy = 1;

    devType = WPADProbe(chan, NULL);

    if (devType == WPAD_ERR_NO_CONTROLLER && p->controlDpdCB != NULL &&
        p->dpdOffNotified && !p->dpdOnNotified) {
        controlDpdCallback(chan, WPAD_ERR_OK);
    }

    OSRestoreInterrupts(enabled);

    if (p->resetPending != 0) {
        p->status.wpad_err = devType;
        reset_kpad(p);
    }

    WPADSetSamplingCallback(chan, kpad_sampling_callback);

    if (p->sampleCount != 0 && pSamples != NULL && numSamples != 0) {
        enabled = OSDisableInterrupts();

        count = p->sampleCount;
        result = count;
        if (count > numSamples) {
            result = numSamples;
        }
        k = result;
        p->sampleCount = 0;
        pDst = &pSamples[k];

        // Hand out the buffered samples oldest-last; the newest one is kept
        // aside and processed first below.
        index = p->sampleIndex - result;
        if (index < 0) {
            index += KPAD_MAX_SAMPLES;
        }

        pDst--;
        while (--k != 0) {
            pDst--;
            *(KPADSample*)pDst = p->samples[index];
            if (++index >= KPAD_MAX_SAMPLES) {
                index = 0;
            }
        }

        sample = p->samples[index];

        OSRestoreInterrupts(enabled);

        {
            WPADAccGravityUnit unit = {1, 1, 1};
            WPADAccGravityUnit unitEx = {1, 1, 1};

            WPADGetAccGravityUnit(chan, WPAD_ACC_GRAVITY_UNIT_CORE, &unit);
            if (unit.x * unit.y * unit.z != 0) {
                p->accGain[0] = 1.0f / unit.x;
                p->accGain[1] = 1.0f / unit.y;
                p->accGain[2] = 1.0f / unit.z;
            } else {
                p->accGain[0] = 0.01f;
                p->accGain[1] = 0.01f;
                p->accGain[2] = 0.01f;
            }

            WPADGetAccGravityUnit(chan, WPAD_ACC_GRAVITY_UNIT_FS, &unitEx);
            if (unitEx.x * unitEx.y * unitEx.z != 0) {
                p->exAccGain[0] = 1.0f / unitEx.x;
                p->exAccGain[1] = 1.0f / unitEx.y;
                p->exAccGain[2] = 1.0f / unitEx.z;
            } else {
                p->exAccGain[0] = 0.005f;
                p->exAccGain[1] = 0.005f;
                p->exAccGain[2] = 0.005f;
            }
        }

        btn = btnFs = btnCl = 0xFFFF;
        dev = WPAD_DEV_NOT_FOUND;
        pDst = &pSamples[result];
        pDst--;
        n = result;
        do {
            pDst--;
            pSample = (n > 1) ? (KPADSample*)pDst : &sample;

            switch (pSample->u.core.err) {
            case WPAD_ERR_OK:
                dev = pSample->u.core.dev;
                if (dev == WPAD_DEV_FREESTYLE) {
                    btnFs = pSample->u.core.button;
                    btnCl = 0;
                } else if (dev == WPAD_DEV_CLASSIC) {
                    btnCl = pSample->u.cl.clButton;
                    btnFs = 0;
                } else {
                    btnCl = 0;
                    btnFs = 0;
                }
                // fallthrough
            case WPAD_ERR_COMMUNICATION_ERROR:
            case WPAD_ERR_CORRUPTED:
                btn = pSample->u.core.button;
                break;
            }
        } while (--n != 0);

        if (btn == 0xFFFF) {
            do {
                memcpy(pSamples, &p->status, sizeof(KPADStatus));
                pSamples++;
            } while (--result != 0);
        } else {

            if (btnFs == 0xFFFF) {
                btnFs = p->status.hold;
            }
            if (btnCl == 0xFFFF) {
                btnCl = p->status.ex_status.cl.hold;
            }

            read_kpad_button(p, dev, count, btn, btnFs, btnCl);

            pDst = &pSamples[result];
            pDst--;
            n = result;
            do {
                pDst--;
                pSample = (n > 1) ? (KPADSample*)pDst : &sample;

                p->status.wpad_err = pSample->u.core.err;
                if (p->status.dev_type != pSample->u.core.dev &&
                    (u8)(pSample->u.core.err + 2) <= 2) {
                    p->status.dev_type = pSample->u.core.dev;
                    p->devChanged = 1;
                }
                p->status.data_format = pSample->format;

                switch (pSample->u.core.err) {
                case WPAD_ERR_OK:
                    read_kpad_stick(p, pSample);
                    // fallthrough
                case WPAD_ERR_CORRUPTED:
                    calc_acc(p, pSample);
                    read_kpad_dpd(p, pSample);
                    break;
                default:
                    p->status.dpd_valid_fg = 0;
                    break;
                }

                pDst[1] = p->status;
            } while (--n != 0);
        }
    }

    p->readBusy = 0;
    return result;
}

void KPADInit(void) {
    s32 i;
    KPADControlBlock* p;
    u32 idx;

    WPADInit();
    memset(_kpadcb, 0, sizeof(_kpadcb));
    sDpdSensitivity = (f32)(1.0f + (f32)WPADGetDpdSensitivity());

    i = 0;
    do {
        p = &_kpadcb[i];
        p->UNK_0x520 = TRUE;
        p->dpdMode = 0;
        p->status.dev_type = WPAD_DEV_NOT_FOUND;
        p->status.data_format = 0;
        p->unitScale = sUnitScale;
        p->downVec = sDownVec;
        p->rightVec = sRightVec;
        p->dpdOrigin = sDpdOrigin;
        calc_dpd_limit(p);
        p->posPlayRadius = p->horiPlayRadius = p->distPlayRadius = p->accPlayRadius = 0.0f;
        p->posSensitivity = p->horiSensitivity = p->distSensitivity = p->accSensitivity = 1.0f;

        KPADSetBtnRepeat(i, 0.0f, 0.0f);
        KPADEnableDPD(i);
        p->tiltEnabled = FALSE;

        sTiltMtx[0][0] = 1;
        sTiltMtx[0][1] = 0;
        sTiltMtx[0][2] = 0;
        sTiltMtx[0][3] = 0;
        sTiltMtx[1][0] = 0;
        sTiltMtx[1][1] = (f32)cos(0.017453292f * sTiltDegrees);
        sTiltMtx[1][2] = (f32)-sin(0.017453292f * sTiltDegrees);
        sTiltMtx[1][3] = 0;
        sTiltMtx[2][0] = 0;
        sTiltMtx[2][1] = (f32)sin(0.017453292f * sTiltDegrees);
        sTiltMtx[2][2] = (f32)cos(0.017453292f * sTiltDegrees);
        sTiltMtx[2][3] = 0;

        idx = 0;
        do {
            p->samples[idx].u.core.err = WPAD_ERR_NO_CONTROLLER;
        } while (++idx < KPAD_MAX_SAMPLES);
    } while (++i < KPAD_MAX_CONTROLLERS);

    KPADReset();
    OSRegisterVersion(__KPADVersion);
}

static DECOMP_INLINE void KPADReset(void) {
    s32 chan;

    KPADSetObjInterval(sSensorHeight);

    chan = KPAD_MAX_CONTROLLERS - 1;
    do {
        if (WPADGetStatus() == WPAD_LIB_STATUS_3) {
            WPADControlMotor(chan, WPAD_MOTOR_STOP);
        }

        _kpadcb[chan].resetPending = 1;
    } while (--chan >= 0);
}

// Reads the newest report of a channel and keeps its DPD mode and data format
// in line with the attached device.
void kpad_sampling_callback(s32 chan) {
    KPADControlBlock* p;
    u8 dpd;
    f32 offset;
    s32 devType;
    u8 idx;
    int mode;
    KPADSample* pSample;
    u8* pMode;

    p = &_kpadcb[chan];

    if (WPADProbe(chan, &devType) != WPAD_ERR_NO_CONTROLLER) {
        idx = p->sampleIndex;
        if (idx >= KPAD_MAX_SAMPLES) {
            idx = 0;
        }

        pSample = &p->samples[idx];
        WPADRead(chan, &pSample->u.core);
        pSample->format = WPADGetDataFormat(chan);
        p->sampleIndex = idx + 1;

        if (p->sampleCount < KPAD_MAX_SAMPLES) {
            p->sampleCount++;
        }

        if (p->dpdEnableX) {
            if (p->dpdEnableY) {
                if (WPADGetSensorBarPosition() == 1) {
                    offset = 0.2f;
                } else {
                    offset = -0.2f;
                }
            } else {
                offset = 0.0f;
            }

            p->dpdOrigin.x = 0.0f;
            p->dpdOrigin.y = -offset;
            calc_dpd_limit(p);
            p->dpdEnableX = FALSE;
        }

        switch (devType) {
        case WPAD_DEV_CORE:
        case WPAD_DEV_FUTURE:
        case WPAD_DEV_NOT_SUPPORTED:
        case WPAD_DEV_UNKNOWN:
            mode = 0;
            break;
        case WPAD_DEV_FREESTYLE:
            mode = 2;
            break;
        case WPAD_DEV_CLASSIC:
            mode = 4;
            break;
        default:
            goto callback;
        }

        if (p->UNK_0x520) {
            mode++;
        }

        if (WPADIsDpdEnabled(chan)) {
            dpd = p->dpdMode;
        } else {
            dpd = 0;
        }

        pMode = sDpdModes[mode];

        if (dpd != pMode[0]) {
            if (p->controlDpdCB != NULL && !p->dpdOffNotified) {
                p->dpdOffNotified = TRUE;
                p->controlDpdCB(chan, 0);
                p->dpdOnNotified = FALSE;
            }

            if (!p->dpdCtrlBusy) {
                p->dpdCtrlBusy = TRUE;

                if (WPADControlDpd(chan, pMode[0], controlDpdCallback) == 0) {
                    p->dpdMode = pMode[0];
                }
            }
        } else if (pSample->format != pMode[1]) {
            WPADSetDataFormat(chan, pMode[1]);
        }
    }

callback:
    if (p->samplingCB != NULL) {
        p->samplingCB(chan);
    }
}

// Called when WPADControlDpd() has finished switching the pointer on or off.
void controlDpdCallback(s32 chan, s32 result) {
    KPADControlBlock* p = &_kpadcb[chan];

    if (result == WPAD_ERR_OK && p->controlDpdCB != NULL &&
        !p->dpdOnNotified) {
        p->dpdOnNotified = TRUE;
        p->controlDpdCB(chan, 1);
        p->dpdOffNotified = FALSE;
    }

    p->dpdCtrlBusy = FALSE;
}

