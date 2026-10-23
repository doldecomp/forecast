#ifndef RVL_SDK_SC_SCAPI_H
#define RVL_SDK_SC_SCAPI_H
#include <types.h>

#include <revolution/BTE.h>
#ifdef __cplusplus
extern "C" {
#endif

#define SC_MAX_DEV_ENTRY_FOR_STD 10
#define SC_MAX_DEV_ENTRY_FOR_SMP 6
#define SC_MAX_DEV_ENTRY (SC_MAX_DEV_ENTRY_FOR_STD + SC_MAX_DEV_ENTRY_FOR_SMP)
#define SC_MAX_DEV_ENTRY_FOR_CMP 6

typedef enum { SC_ASPECT_STD, SC_ASPECT_WIDE } SCAspectRatio;

typedef enum { SC_EURGB_50_HZ, SC_EURGB_60_HZ } SCEuRgb60Mode;

typedef enum {
    SC_LANG_JP,
    SC_LANG_EN,
    SC_LANG_DE,
    SC_LANG_FR,
    SC_LANG_SP,
    SC_LANG_IT,
    SC_LANG_NL,
    SC_LANG_ZH_S,
    SC_LANG_ZH_T,
    SC_LANG_KR,
} SCLanguage;

typedef enum { SC_INTERLACED, SC_PROGRESSIVE } SCProgressiveMode;

typedef enum { SC_MOTOR_OFF, SC_MOTOR_ON } SCMotorMode;

typedef enum { SC_SND_MONO, SC_SND_STEREO, SC_SND_SURROUND } SCSoundMode;

typedef enum { SC_SENSOR_BAR_BOTTOM, SC_SENSOR_BAR_TOP } SCSensorBarPos;

typedef struct SCIdleModeInfo {
    u8 wc24;      // at 0x0
    u8 slotLight; // at 0x1
} SCIdleModeInfo;

typedef struct SCDevInfo {
    char devName[20]; // at 0x0
    char at_0x14[1];
    char UNK_0x15[0xB];
    LINK_KEY linkKey; // at 0x20
    char UNK_0x30[0x10];
} SCDevInfo;

typedef struct SCBtDeviceInfo {
    BD_ADDR addr;   // at 0x0
    SCDevInfo info; // at 0x6
} SCBtDeviceInfo;

typedef struct SCBtDeviceInfoArray {
    u8 numRegist; // at 0x0
    union {
        struct {
            SCBtDeviceInfo regist[SC_MAX_DEV_ENTRY_FOR_STD]; // at 0x1
            SCBtDeviceInfo active[SC_MAX_DEV_ENTRY_FOR_SMP]; // at 0x2BD
        };

        SCBtDeviceInfo devices[SC_MAX_DEV_ENTRY];
    };
} SCBtDeviceInfoArray;

typedef struct SCBtCmpDevInfo {
    BD_ADDR addr;     // at 0x0
    SCDevInfo info;   // at 0x6
    LINK_KEY linkKey; // at 0x46
} SCBtCmpDevInfo;

typedef struct SCBtCmpDevInfoArray {
    u8 num;                                           // at 0x0
    SCBtCmpDevInfo devices[SC_MAX_DEV_ENTRY_FOR_CMP]; // at 0x1
} SCBtCmpDevInfoArray;

#define SC_PARENTAL_PASSWORD_LENGTH 4
#define SC_PARENTAL_SECRET_ANSWER_LENGTH 32

typedef struct SCParentalControlsInfo {
    u8 enable;                                          // at 0x0
    u8 org;                                             // at 0x1
    u8 rating;                                          // at 0x2
    char password[SC_PARENTAL_PASSWORD_LENGTH];         // at 0x3
    u8 secretQuestion;                                  // at 0x7
    u16 secretAnswer[SC_PARENTAL_SECRET_ANSWER_LENGTH]; // at 0x8
    u16 secretAnswerLength;                             // at 0x48
} SCParentalControlsInfo;

#define SC_PARENTAL_FLAG_ENABLED (1 << 7)

#define SC_NET_RESTRICTIONS_NONE 0
#define SC_NET_RESTRICTIONS_OPERA (1 << 0)
#define SC_NET_RESTRICTIONS_MSG_BOARD (1 << 1)
#define SC_NET_RESTRICTIONS_SHOPPING (1 << 2)

#define SC_WC_FLAGS_DISABLED 0
#define SC_WC_FLAGS_ENABLED 1

#define SC_SIMPLE_ADDRESS_ID_COUNTRY 24
#define SC_SIMPLE_ADDRESS_ID_REGION 16
#define SC_SIMPLE_ADDRESS_ID_CITY 0

typedef struct SCSimpleAddress {
    u32 id;                   // 0x00
    u16 countryName[16][64];  // 0x04
    u16 regionName[16][64];   // 0x804
    u16 latitude;             // 0x1004
    u16 longitude;            // 0x1006
} SCSimpleAddress;

u8 SCGetAspectRatio(void);
s8 SCGetDisplayOffsetH(void);
u8 SCGetEuRgb60Mode(void);
BOOL SCGetIdleMode(SCIdleModeInfo* mode);
u8 SCGetLanguage(void);
BOOL SCSetLanguage(u8 language);
BOOL SCGetParentalControl(SCParentalControlsInfo* pcInfo);
u8 SCGetProgressiveMode(void);
u8 SCGetScreenSaverMode(void);
u8 SCGetSoundMode(void);
u32 SCGetCounterBias(void);
void SCGetBtDeviceInfoArray(SCBtDeviceInfoArray* info);
BOOL SCSetBtDeviceInfoArray(const SCBtDeviceInfoArray* info);
void SCGetBtCmpDevInfoArray(SCBtCmpDevInfoArray* info);
BOOL SCSetBtCmpDevInfoArray(const SCBtCmpDevInfoArray* info);
u32 SCGetBtDpdSensibility(void);
u8 SCGetWpadMotorMode(void);
BOOL SCSetWpadMotorMode(u8 mode);
u8 SCGetWpadSensorBarPosition(void);
u8 SCGetWpadSpeakerVolume(void);
BOOL SCSetWpadSpeakerVolume(u8 vol);
u32 SCGetSimpleAddressID(void);
BOOL SCGetSimpleAddressData(SCSimpleAddress* address);
u32 SCGetNetContentRestrictions(void);
BOOL SCGetEULA(void);
u32 SCGetWCFlags(void);

#ifdef __cplusplus
}
#endif
#endif
