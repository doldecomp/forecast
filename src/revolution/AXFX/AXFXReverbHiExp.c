#include <revolution/AXFX.h>
#include <revolution/OS.h>

#include <cmath>
#include <cstring>

static BOOL __AllocDelayLine(AXFX_REVERBHI_EXP* fx);
static void __BzeroDelayLines(AXFX_REVERBHI_EXP* fx);
static void __FreeDelayLine(AXFX_REVERBHI_EXP* fx);
static BOOL __InitParams(AXFX_REVERBHI_EXP* fx);

static u32 __EarlySizeTable[8][3] = {
    {157, 479, 829},   {317, 809, 1117},  {479, 941, 1487},   {641, 1259, 1949},
    {797, 1667, 2579}, {967, 1901, 2903}, {1123, 2179, 3413}, {1279, 2477, 3889},
};

static f32 __EarlyCoefTable[8][3] = {
    {0.4f, -1.0f, 0.3f}, {0.5f, -0.95f, 0.3f}, {0.6f, -0.9f, 0.3f}, {0.75f, -0.85f, 0.3f},
    {-0.9f, 0.8f, 0.3f}, {-1.0f, 0.7f, 0.3f},  {-1.0f, 0.7f, 0.3f}, {-1.0f, 0.7f, 0.3f},
};

// Comb (3), allpass (2) and last allpass (3) lengths per fused mode.
// The last row holds the maximum of each column.
static u32 __FilterSizeTable[7][8] = {
    {1789, 1999, 2333, 433, 149, 47, 73, 67}, {149, 293, 449, 251, 103, 47, 73, 67},
    {947, 1361, 1531, 433, 137, 47, 73, 67},  {1279, 1531, 1973, 509, 149, 47, 73, 67},
    {1531, 1847, 2297, 563, 179, 47, 73, 67}, {1823, 2357, 2693, 571, 137, 47, 73, 67},
    {1823, 2357, 2693, 571, 179, 47, 73, 67},
};

u32 AXFXReverbHiExpGetMemSize(AXFX_REVERBHI_EXP* fx) {
    u32 i;
    s32 size = __EarlySizeTable[7][2] + (s32)(32000.0f * fx->preDelayTimeMax);

    for (i = 0; i < 3; i++) {
        size += __FilterSizeTable[6][i];
    }

    for (i = 0; i < 2; i++) {
        size += __FilterSizeTable[6][i + 3];
    }

    size *= 3;

    for (i = 0; i < 3; i++) {
        size += __FilterSizeTable[6][i + 5];
    }

    return size * sizeof(f32);
}

BOOL AXFXReverbHiExpInit(AXFX_REVERBHI_EXP* fx) {
    u32 i;
    BOOL result = TRUE;
    BOOL enabled = OSDisableInterrupts();

    fx->active = 1;

    if (fx->preDelayTimeMax < 0.0f) {
        AXFXReverbHiExpShutdown(fx);
        OSRestoreInterrupts(enabled);
        return FALSE;
    }

    fx->earlyMaxLength = __EarlySizeTable[7][2];
    fx->preDelayMaxLength = (u32)(32000.0f * fx->preDelayTimeMax);

    for (i = 0; i < 3; i++) {
        fx->combMaxLength[i] = __FilterSizeTable[6][i];
    }

    for (i = 0; i < 2; i++) {
        fx->allpassMaxLength[i] = __FilterSizeTable[6][i + 3];
    }

    for (i = 0; i < 3; i++) {
        fx->lastAllpassMaxLength[i] = __FilterSizeTable[6][i + 5];
    }

    result = __AllocDelayLine(fx);
    if (!result) {
        AXFXReverbHiExpShutdown(fx);
        OSRestoreInterrupts(enabled);
        return FALSE;
    }

    __BzeroDelayLines(fx);

    result = __InitParams(fx);
    if (!result) {
        AXFXReverbHiExpShutdown(fx);
        OSRestoreInterrupts(enabled);
        return FALSE;
    }

    fx->active &= ~1;
    OSRestoreInterrupts(enabled);
    return TRUE;
}

BOOL AXFXReverbHiExpSettings(AXFX_REVERBHI_EXP* fx) {
    BOOL result = TRUE;
    BOOL enabled = OSDisableInterrupts();

    fx->active |= 1;
    AXFXReverbHiExpShutdown(fx);

    result = AXFXReverbHiExpInit(fx);
    if (!result) {
        AXFXReverbHiExpShutdown(fx);
        OSRestoreInterrupts(enabled);
        return FALSE;
    }

    fx->active |= 2;
    fx->active &= ~1;
    OSRestoreInterrupts(enabled);
    return TRUE;
}

void AXFXReverbHiExpShutdown(AXFX_REVERBHI_EXP* fx) {
    BOOL enabled = OSDisableInterrupts();

    fx->active |= 1;
    __FreeDelayLine(fx);

    OSRestoreInterrupts(enabled);
}

void AXFXReverbHiExpCallback(AXFX_BUFFERUPDATE* update, AXFX_REVERBHI_EXP* fx) {
    u32 samp, ch, i;
    s32* input[3];
    f32 output[3];
    f32 fusedOut[3];
    s32* busIn[3];
    s32* busOut[3];
    f32 data;
    f32* earlyLine;
    f32 earlyOut;
    f32* preDelayLine;
    f32 preDelayOut;
    f32* combLine;
    f32 combOut;
    f32* allpassLine;
    f32 allpassOut;
    f32 filterOut;
    f32 allpassIn;
    f32 lpfOut;
    f32 allpassCoef;
    f32 lpfCoef1;
    f32 lpfCoef2;
    f32 fusedGain;
    f32 crosstalkGain;
    f32 crosstalkL;
    f32 crosstalkR;
    f32 crosstalkS;

    if (fx->active) {
        fx->active &= ~2;
        return;
    }

    input[0] = update->left;
    input[1] = update->right;
    input[2] = update->surround;

    if (fx->busIn != NULL) {
        busIn[0] = fx->busIn->left;
        busIn[1] = fx->busIn->right;
        busIn[2] = fx->busIn->surround;
    }

    if (fx->busOut != NULL) {
        busOut[0] = fx->busOut->left;
        busOut[1] = fx->busOut->right;
        busOut[2] = fx->busOut->surround;
    }

    lpfCoef1 = 1.0f - fx->lpfCoef;
    lpfCoef2 = fx->lpfCoef;
    allpassCoef = fx->allpassCoef;
    fusedGain = 0.6f * fx->fusedGain;
    crosstalkGain = 0.5f * fx->crosstalk;

    for (samp = 0; samp < 96; samp++) {
        for (ch = 0; ch < 3; ch++) {
            if (fx->busIn != NULL) {
                data = *input[ch] + *busIn[ch]++;
            } else {
                data = *input[ch];
            }

            // Unlike later SDK versions, the input is never written to the early line.
            earlyLine = fx->earlyLine[ch];
            earlyOut = fx->earlyCoef[0] * earlyLine[fx->earlyPos[0]] +
                       fx->earlyCoef[1] * earlyLine[fx->earlyPos[1]] +
                       fx->earlyCoef[2] * earlyLine[fx->earlyPos[2]];

            if (fx->preDelayLength != 0) {
                preDelayLine = fx->preDelayLine[ch];
                preDelayOut = preDelayLine[fx->preDelayPos];
                preDelayLine[fx->preDelayPos] = data;
            } else {
                preDelayOut = data;
            }

            filterOut = 0.0f;
            for (i = 0; i < 3; i++) {
                combLine = fx->combLine[ch][i];
                combOut = combLine[fx->combPos[i]];
                combLine[fx->combPos[i]] = preDelayOut + combOut * fx->combCoef[i];
                filterOut += combOut;
            }

            for (i = 0; i < 2; i++) {
                allpassLine = fx->allpassLine[ch][i];
                allpassOut = allpassLine[fx->allpassPos[i]];
                allpassIn = filterOut + allpassOut * allpassCoef;
                allpassLine[fx->allpassPos[i]] = allpassIn;
                filterOut = allpassOut - allpassIn * allpassCoef;
            }

            lpfOut = lpfCoef1 * filterOut + lpfCoef2 * fx->lastLpfOut[ch];
            fx->lastLpfOut[ch] = lpfOut;

            allpassLine = fx->lastAllpassLine[ch];
            allpassOut = allpassLine[fx->lastAllpassPos[ch]];
            allpassIn = lpfOut + allpassOut * allpassCoef;
            allpassLine[fx->lastAllpassPos[ch]] = allpassIn;
            fusedOut[ch] = allpassOut - allpassIn * allpassCoef;

            if (++fx->lastAllpassPos[ch] >= fx->lastAllpassLength[ch]) {
                fx->lastAllpassPos[ch] = 0;
            }

            fusedOut[ch] *= fusedGain;
            fusedOut[ch] += earlyOut;
        }

        crosstalkL = fusedOut[1] + fusedOut[2];
        crosstalkR = fusedOut[0] + fusedOut[2];
        crosstalkS = fusedOut[0] + fusedOut[1];

        output[0] = fusedOut[0] + crosstalkL * crosstalkGain;
        output[1] = fusedOut[1] + crosstalkR * crosstalkGain;
        output[2] = fusedOut[2] + crosstalkS * crosstalkGain;

        *input[0]++ = output[0] * fx->outGain;
        *input[1]++ = output[1] * fx->outGain;
        *input[2]++ = output[2] * fx->outGain;

        if (fx->busOut != NULL) {
            *busOut[0]++ = output[0] * fx->sendGain;
            *busOut[1]++ = output[1] * fx->sendGain;
            *busOut[2]++ = output[2] * fx->sendGain;
        }

        for (i = 0; i < 3; i++) {
            if (++fx->earlyPos[i] >= fx->earlyLength) {
                fx->earlyPos[i] = 0;
            }
        }

        if (fx->preDelayLength != 0) {
            if (++fx->preDelayPos >= fx->preDelayLength) {
                fx->preDelayPos = 0;
            }
        }

        for (i = 0; i < 3; i++) {
            if (++fx->combPos[i] >= fx->combLength[i]) {
                fx->combPos[i] = 0;
            }
        }

        for (i = 0; i < 2; i++) {
            if (++fx->allpassPos[i] >= fx->allpassLength[i]) {
                fx->allpassPos[i] = 0;
            }
        }
    }
}

static BOOL __AllocDelayLine(AXFX_REVERBHI_EXP* fx) {
    u32 ch, i;

    for (ch = 0; ch < 3; ch++) {
        fx->earlyLine[ch] = __AXFXAlloc(fx->earlyMaxLength * sizeof(f32));
        if (fx->earlyLine[ch] == NULL) {
            return FALSE;
        }

        if (fx->preDelayMaxLength != 0) {
            fx->preDelayLine[ch] = __AXFXAlloc(fx->preDelayMaxLength * sizeof(f32));
            if (fx->preDelayLine[ch] == NULL) {
                return FALSE;
            }
        } else {
            fx->preDelayLine[ch] = NULL;
        }

        for (i = 0; i < 3; i++) {
            fx->combLine[ch][i] = __AXFXAlloc(fx->combMaxLength[i] * sizeof(f32));
            if (fx->combLine[ch][i] == NULL) {
                return FALSE;
            }
        }

        for (i = 0; i < 2; i++) {
            fx->allpassLine[ch][i] = __AXFXAlloc(fx->allpassMaxLength[i] * sizeof(f32));
            if (fx->allpassLine[ch][i] == NULL) {
                return FALSE;
            }
        }

        fx->lastAllpassLine[ch] = __AXFXAlloc(fx->lastAllpassMaxLength[ch] * sizeof(f32));
        if (fx->lastAllpassLine[ch] == NULL) {
            return FALSE;
        }
    }

    return TRUE;
}

static void __BzeroDelayLines(AXFX_REVERBHI_EXP* fx) {
    u32 ch, i;

    for (ch = 0; ch < 3; ch++) {
        if (fx->earlyLine[ch] != NULL) {
            memset(fx->earlyLine[ch], 0, fx->earlyMaxLength * sizeof(f32));
        }

        if (fx->preDelayLine[ch] != NULL) {
            memset(fx->preDelayLine[ch], 0, fx->preDelayMaxLength * sizeof(f32));
        }

        for (i = 0; i < 3; i++) {
            if (fx->combLine[ch][i] != NULL) {
                memset(fx->combLine[ch][i], 0, fx->combMaxLength[i] * sizeof(f32));
            }
        }

        for (i = 0; i < 2; i++) {
            if (fx->allpassLine[ch][i] != NULL) {
                memset(fx->allpassLine[ch][i], 0, fx->allpassMaxLength[i] * sizeof(f32));
            }
        }

        if (fx->lastAllpassLine[ch] != NULL) {
            memset(fx->lastAllpassLine[ch], 0, fx->lastAllpassMaxLength[ch] * sizeof(f32));
        }
    }
}

static void __FreeDelayLine(AXFX_REVERBHI_EXP* fx) {
    u32 ch, i;

    for (ch = 0; ch < 3; ch++) {
        if (fx->earlyLine[ch] != NULL) {
            __AXFXFree(fx->earlyLine[ch]);
            fx->earlyLine[ch] = NULL;
        }

        if (fx->preDelayLine[ch] != NULL) {
            __AXFXFree(fx->preDelayLine[ch]);
            fx->preDelayLine[ch] = NULL;
        }

        for (i = 0; i < 3; i++) {
            if (fx->combLine[ch][i] != NULL) {
                __AXFXFree(fx->combLine[ch][i]);
                fx->combLine[ch][i] = NULL;
            }
        }

        for (i = 0; i < 2; i++) {
            if (fx->allpassLine[ch][i] != NULL) {
                __AXFXFree(fx->allpassLine[ch][i]);
                fx->allpassLine[ch][i] = NULL;
            }
        }

        if (fx->lastAllpassLine[ch] != NULL) {
            __AXFXFree(fx->lastAllpassLine[ch]);
            fx->lastAllpassLine[ch] = NULL;
        }
    }
}

static BOOL __InitParams(AXFX_REVERBHI_EXP* fx) {
    u32 ch, i;
    f32 exponent;

    if (fx->earlyMode >= 8) {
        return FALSE;
    }

    if (fx->preDelayTime < 0.0f || fx->preDelayTime > fx->preDelayTimeMax) {
        return FALSE;
    }

    if (fx->fusedMode >= 6) {
        return FALSE;
    }

    if (fx->fusedTime < 0.0f) {
        return FALSE;
    }

    if (fx->coloration < 0.0f || fx->coloration > 1.0f) {
        return FALSE;
    }

    if (fx->damping < 0.0f || fx->damping > 1.0f) {
        return FALSE;
    }

    if (fx->crosstalk < 0.0f || fx->crosstalk > 1.0f) {
        return FALSE;
    }

    if (fx->earlyGain < 0.0f || fx->earlyGain > 1.0f) {
        return FALSE;
    }

    if (fx->fusedGain < 0.0f || fx->fusedGain > 1.0f) {
        return FALSE;
    }

    if (fx->outGain < 0.0f || fx->outGain > 1.0f) {
        return FALSE;
    }

    if (fx->sendGain < 0.0f || fx->sendGain > 1.0f) {
        return FALSE;
    }

    fx->earlyLength = __EarlySizeTable[fx->earlyMode][2];
    for (i = 0; i < 3; i++) {
        fx->earlyPos[i] = fx->earlyLength - __EarlySizeTable[fx->earlyMode][i];
        fx->earlyCoef[i] = 0.6f * (fx->earlyGain * __EarlyCoefTable[fx->earlyMode][i]);
    }

    fx->preDelayPos = 0;
    fx->preDelayLength = (u32)(32000.0f * fx->preDelayTime);

    for (i = 0; i < 3; i++) {
        fx->combPos[i] = 0;
        fx->combLength[i] = __FilterSizeTable[fx->fusedMode][i];
        // The exponent comes first, which puts -3.0f before 10.0 in .sdata2.
        exponent = (-3.0f * fx->combLength[i]) / (32000.0f * fx->fusedTime);
        fx->combCoef[i] = pow(10.0, exponent);
    }

    for (i = 0; i < 2; i++) {
        fx->allpassPos[i] = 0;
        fx->allpassLength[i] = __FilterSizeTable[fx->fusedMode][i + 3];
    }

    for (ch = 0; ch < 3; ch++) {
        fx->lastAllpassPos[ch] = 0;
        fx->lastAllpassLength[ch] = __FilterSizeTable[fx->fusedMode][ch + 5];
    }

    fx->allpassCoef = fx->coloration;
    fx->lpfCoef = 1.0f - fx->damping;
    if (fx->lpfCoef > 0.95f) {
        fx->lpfCoef = 0.95f;
    }

    for (ch = 0; ch < 3; ch++) {
        fx->lastLpfOut[ch] = 0.0f;
    }

    return TRUE;
}
