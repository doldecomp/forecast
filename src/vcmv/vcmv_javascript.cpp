#include <types.h>
#include <cstring>
#include "vcmv.h"
#include "www.h"
#include "vcmv_www.h"

// File-local static objects
static ESPluginDef sJsPlugin;

static ESPluginApi* sJsPluginApi;
static s32 sScrollDirection;
static s32 sScrollAmount;
static s32 sWriteFlagValue;
bool sSelectSoundPlayed;
static s32 sCurrentParam1;
static const char* sCurrentParam2;
static ESValue* sCurrentParam3;
bool sFocusSoundPlayed;

static const char sJsExtName[] = "vcJavaScriptExt";
static const char* sJsExtNames[] = {sJsExtName};

static inline BOOL vcmvJsCallMethod(const char* param1, s32* param2, void* param3, s32 param4, bool do_force = false) {
    s32 sp8;

    if (strcmp(param1, sCurrentParam2) != 0) {
        return false;
    }

    if (do_force) {
        *param2 = sJsPluginApi->registerClass(sCurrentParam1, 0, 0, param3, param3, "", 0, &sp8);
    } else {
        *param2 = sJsPluginApi->registerClass(param4, 0, 0, param3, param3, "", 0, &sp8);
    }

    if (*param2 >= 0) {
        sCurrentParam3->type = 0;
        sCurrentParam3->value_int = sp8;
    }

    return true;
}

bool vcmvJsTrue(void) { return true; }

void vcmvJsInit(void) {
    sJsPlugin.extNames = sJsExtNames;
    sJsPlugin.reserved04 = 0;
    sJsPlugin.initFunc = (void*)vcmvJsPluginInit;
    sJsPlugin.reserved0C = 0;
    sJsPlugin.reserved10 = 0;
    sJsPlugin.reserved14 = 0;
    sJsPlugin.reserved18 = 0;
    sJsPlugin.checkFunc = (void*)vcmvJsTrue;
    pWWWAddJSPlugin("vcJavaScriptExt.dll", &sJsPlugin, &sJsPluginApi, (void*)vcmvJsTrue, (void*)vcmvJsPluginInit);
    sScrollDirection = 0;
}

s32 vcmvJsPluginInit(s32 param1, const char* param2, ESValue* param3) {
    s32 sp8;

    if (strcmp(param2, "vcJavaScriptExt") == 0) {
        if (sJsPluginApi->registerClass(param1, 0, 0, NULL, (void*)vcmvJsPluginCallback, "", 0, &sp8) < 0) {
            return 10;
        }

        param3->type = 0;
        param3->value_int = sp8;
        return 7;
    }

    return 8;
}

s32 vcmvJsPluginCallback(void* param1, s32 param2, int param3, void* param4, ESValue* param5) {
    s32* sp8;

    if (param3 != 0) {
        return 19;
    }

    if (sJsPluginApi->registerMethod(param2, (void*)vcmvJsMethodDispatcher, 0, 0, &sp8) < 0) {
        return 18;
    }

    param5->type = 0;
    param5->ptr = sp8;
    *sp8 = 0;
    return 16;
}

s32 vcmvJsMethodDispatcher(s32 param1, const char* param2, ESValue* param3) {
    s32 var_r31;
    u8 var_r29;

    var_r29 = 0;
    sCurrentParam2 = param2;
    sCurrentParam1 = param1;
    sCurrentParam3 = param3;

    switch (*param2) {
        case 'F':
            var_r29 |= vcmvJsCallMethod("FadeIn", &var_r31, (void*)vcmvJsFadeIn, param1, false);
            var_r29 |= vcmvJsCallMethod("FocusSound", &var_r31, (void*)vcmvJsFocusSound, sCurrentParam1, true);
            break;
        case 'L':
            var_r29 |= vcmvJsCallMethod("LeftScroll", &var_r31, (void*)vcmvJsLeftScroll, param1, false);
            break;
        case 'R':
            var_r29 |= vcmvJsCallMethod("RightScroll", &var_r31, (void*)vcmvJsRightScroll, param1, false);
            break;
        case 'S':
            var_r29 |= vcmvJsCallMethod("SelectSound", &var_r31, (void*)vcmvJsSelectSound, param1, false);
            break;
        case 'W':
            var_r29 |= vcmvJsCallMethod("WriteFlag", &var_r31, (void*)vcmvJsWriteFlag, param1, false);
            break;
        case 'b':
            if (strcmp(param2, "bNIMSGUI") == 0) {
                param3->type = 2;
                param3->value_double = (double)(u32)vcmvNimsGuiFlag;
                return 6;
            }
            if (strcmp(param2, "bSMSGUI") == 0) {
                param3->type = 2;
                s32 val = 0;
                if (wwwAspectRatio != 0 && vcmvPageCurrentWidth != vcmvPageTargetWidth) {
                    val = 1;
                }
                param3->value_double = (double)val;
                return 6;
            }
            break;
        default:
            break;
    }

    if (var_r29 == 0) {
        return 8;
    }

    if (var_r31 < 0) {
        return 10;
    }

    return 7;
}

s32 vcmvJsFadeIn(s32 param1, s32 param2, s32 param3, ESValue* param4) {
    sScrollAmount = 0;

    if (param3 == 1) {
        sScrollAmount = param4->value_double;
    }

    sScrollDirection = 0;
    return 17;
}

s32 vcmvJsFocusSound(s32 param1, s32 param2, s32 param3, ESValue* param4) {
    if (!vcmvIsPageFading && vcmvFadeTimer == vcmvFadeDuration && !vcmvIsTransitioning) {
        sFocusSoundPlayed = true;
        VCMVPlaySound(0);
    }

    return 17;
}

s32 vcmvJsLeftScroll(s32 param1, s32 param2, s32 param3, ESValue* param4) {
    sScrollAmount = 0;

    if (param3 == 1) {
        sScrollAmount = param4->value_double;
    }

    sScrollDirection = 1;
    VCMVPlaySound(1);
    return 17;
}

s32 vcmvJsRightScroll(s32 param1, s32 param2, s32 param3, ESValue* param4) {
    sScrollAmount = 0;

    if (param3 == 1) {
        sScrollAmount = param4->value_double;
    }

    sScrollDirection = 2;
    VCMVPlaySound(1);
    return 17;
}

s32 vcmvJsSelectSound(s32 param1, s32 param2, s32 param3, ESValue* param4) {
    sSelectSoundPlayed = true;
    return 17;
}

s32 vcmvJsWriteFlag(s32 param1, s32 param2, s32 param3, ESValue* param4) {
    sWriteFlagValue = 0;

    if (param3 == 1) {
        sWriteFlagValue = param4->value_double;
    }

    return 17;
}

s32 vcmvJsGetScrollDirection(void) { return sScrollDirection; }

s32 vcmvJsGetScrollAmount(void) { return sScrollAmount; }

void vcmvJsResetScroll(void) {
    sScrollDirection = 0;
    sScrollAmount = 0;
}
