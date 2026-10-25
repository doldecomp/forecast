#ifndef RVL_SDK_VI_VI3IN1_H
#define RVL_SDK_VI_VI3IN1_H
#include <types.h>
#ifdef __cplusplus
extern "C" {
#endif

/**
 * Settings of the video encoder ("3-in-1" VDAC), sent over I2C
 */

typedef u8 VIBool;
#define VI_FALSE ((VIBool)0)
#define VI_TRUE ((VIBool)1)
#define VI_DISABLE ((VIBool)0)
#define VI_ENABLE ((VIBool)1)

typedef enum VITiming {
    VI_TMG_GAME,
    VI_TMG_DVD
} VITiming;

typedef enum VIVideo {
    VI_VMODE_NTSC,
    VI_VMODE_MPAL,
    VI_VMODE_PAL,
    VI_VMODE_RGB
} VIVideo;

typedef enum VIACPType {
    VI_ACP_OFF = 1,
    VI_ACP_TYPE1,
    VI_ACP_TYPE2,
    VI_ACP_TYPE3
} VIACPType;

typedef enum VIOverDrive {
    VI_ODV_L1,
    VI_ODV_L2,
    VI_ODV_L3,
    VI_ODV_L4,
    VI_ODV_L5,
    VI_ODV_L6
} VIOverDrive;

// Gamma 0.1 to 3.0
typedef enum VIGamma {
    VI_GM_0_1 = 1,
    VI_GM_0_2,
    VI_GM_0_3,
    VI_GM_0_4,
    VI_GM_0_5,
    VI_GM_0_6,
    VI_GM_0_7,
    VI_GM_0_8,
    VI_GM_0_9,
    VI_GM_1_0,
    VI_GM_1_1,
    VI_GM_1_2,
    VI_GM_1_3,
    VI_GM_1_4,
    VI_GM_1_5,
    VI_GM_1_6,
    VI_GM_1_7,
    VI_GM_1_8,
    VI_GM_1_9,
    VI_GM_2_0,
    VI_GM_2_1,
    VI_GM_2_2,
    VI_GM_2_3,
    VI_GM_2_4,
    VI_GM_2_5,
    VI_GM_2_6,
    VI_GM_2_7,
    VI_GM_2_8,
    VI_GM_2_9,
    VI_GM_3_0
} VIGamma;

typedef struct VIGammaObj {
    u16 a[6];    // at 0x0
    u8 yin[7];   // at 0xC
    u16 yout[7]; // at 0x14
} VIGammaObj;

typedef struct VIMacroVisionObj {
    u8 m[26]; // at 0x0
} VIMacroVisionObj;

extern VIMacroVisionObj VINtscACPType1;
extern VIMacroVisionObj VINtscACPType2;
extern VIMacroVisionObj VINtscACPType3;
extern VIMacroVisionObj VIPalACPType1;
extern VIMacroVisionObj VIPalACPType2;
extern VIMacroVisionObj VIPalACPType3;
extern VIMacroVisionObj VIEurgb60ACPType1;
extern VIMacroVisionObj VIEurgb60ACPType2;
extern VIMacroVisionObj VIEurgb60ACPType3;
extern VIMacroVisionObj VIMpalACPType1;
extern VIMacroVisionObj VIMpalACPType2;
extern VIMacroVisionObj VIMpalACPType3;
extern VIMacroVisionObj VIProgressiveACPType;
extern VIMacroVisionObj VIZeroACPType;

// Bits of Vdac_Flag_Changed: settings that the next retrace sends to the VDAC
#define VI_VDAC_CHANGED_CGMS (1 << 0)
#define VI_VDAC_CHANGED_WSS (1 << 1)
#define VI_VDAC_CHANGED_CC (1 << 2)
#define VI_VDAC_CHANGED_MACROVISION (1 << 3)
#define VI_VDAC_CHANGED_GAMMA (1 << 4)
#define VI_VDAC_CHANGED_TRAP_FILTER (1 << 5)
#define VI_VDAC_CHANGED_RGB_OVERDRIVE (1 << 6)
#define VI_VDAC_CHANGED_RGB_MODE (1 << 7)

extern volatile u32 Vdac_Flag_Changed;

void __VISetYUVSEL(VIBool outsel);
void __VISetTiming(VITiming timing);
void __VISetFilter4EURGB60(VIBool enable);
void __VISetVBICtrl(VIBool cgms, VIBool wss, VIBool cc);

void __VISetCGMS(void);
void __VISetCGMSClear(void);

void __VISetWSS(void);
void VISetWSS(u8 gp1, u8 gp2, u8 gp3, u8 gp4);

void __VISetClosedCaption(void);
void VISetClosedCaption(u8 cc1, u8 cc2, u8 cc3, u8 cc4);

void __VISetMacrovisionImm(VIMacroVisionObj* mparam);
void __VISetMacrovision(void);

void __VISetGammaImm(VIGammaObj* gamma);
void __VISetGamma(void);

void __VISetTrapFilter(void);
void __VISetTrapFilterImm(VIBool filter);
void VISetTrapFilter(VIBool filter);

void __VISetRGBOverDrive(void);
void VISetRGBOverDrive(VIOverDrive level);

void VISetRGBModeImm(void);
void __VISetRGBModeImm(void);
void __VISetVolume(u8 volumeL, u8 volumeR);

void __VISetRevolutionModeSimple(void);

#ifdef __cplusplus
}
#endif
#endif
