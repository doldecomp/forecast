#ifndef RVL_SDK_HBM_HBM_H
#define RVL_SDK_HBM_HBM_H
#include <types.h>
#include <revolution/ARC.h>
#include <revolution/OS.h>
#include <revolution/WENC.h>
#include <revolution/KPAD.h>
#include <revolution/WPAD.h>
#include <nw4r/snd/snd_SoundHandle.h>

namespace nw4r {
namespace snd {
class SoundArchivePlayer;
}
}  // namespace nw4r

// Controller input handed to the menu each frame.
struct HBMKPadData {
    KPADStatus* kpad;  // at 0x0
    Vec2 pos;          // at 0x4
    u32 use_devices;   // at 0xC
};

// Input for all four controllers, as handed to HBMCalc.
struct HBMControllerData {
    HBMKPadData wiiCon[4];  // at 0x00
};

// RVL_SDK Home Button Menu - controller and remote speaker interfaces.
// The rest of the library (menu, gui, sound) is not recovered yet.

namespace homebutton {

// Per-channel speaker state of RemoteSpk (0x68 bytes).
struct RemoteSpkChanInfo {
    OSAlarm alarm;          // at 0x00
    WENCInfo wencinfo;      // at 0x30
    const s16* in_pcm;      // at 0x50
    s32 length;             // at 0x54, bytes left
    s32 seId;               // at 0x58
    bool first;             // at 0x5C
    s8 vol;                 // at 0x5D
    s8 cannotSendCnt;       // at 0x5E
    u8 unk5F[0x03];         // at 0x5F
    u8 playReady;           // at 0x62
    u8 unk63[0x05];         // at 0x63
};

// Streams the menu sound effects to the four Wiimote speakers.
class RemoteSpk {
public:
    // MWCC puts the vtable pointer after the members declared before the first
    // virtual function, so these come first (vtable at 0x1F0).
    RemoteSpkChanInfo info[4];  // at 0x000
    OSAlarm speakerAlarm;       // at 0x1A0
    ARCHandle handle;           // at 0x1D0
    bool available;             // at 0x1EC
    // vtable at 0x1F0

    static RemoteSpk* spInstance;

    RemoteSpk(void* spkSeBuf);
    virtual ~RemoteSpk();

    static void SetInstance(RemoteSpk* instance) {
        spInstance = instance;
    }
    static RemoteSpk* GetInstance() {
        return spInstance;
    }

    void Start();
    void cancelAlarm();
    void ClearPcm();
    void Connect(s32 chan);
    void Play(s32 chan, int seId, s8 vol);
    bool isPlaying(s32 chan) const;
    bool isPlayingId(s32 chan, int seId) const;
    bool isPlayReady(s32 chan) const;

    static void UpdateSpeaker(OSAlarm* alarm, OSContext* context);
    static void SpeakerOnCallback(s32 chan, s32 result);
    static void DelaySpeakerOnCallback(OSAlarm* alarm, OSContext* context);
    static void SpeakerPlayCallback(s32 chan, s32 result);
    static void DelaySpeakerPlayCallback(OSAlarm* alarm, OSContext* context);

};

// Public state of one controller as the menu sees it (0x20 bytes).
struct HBController {
    s32 chan;     // at 0x00
    f32 spVol;    // at 0x04
    f32 x;        // at 0x08
    f32 y;        // at 0x0C
    u32 trig;     // at 0x10
    u32 hold;     // at 0x14
    u32 release;  // at 0x18
    bool rumble;  // at 0x1C
};

// Wraps one Wiimote: pointer and buttons, rumble, and the speaker mute timers.
class Controller {
public:
    Controller(int chan, RemoteSpk* spk);
    ~Controller();

    void initCallback();
    void clearCallback();
    void setKpad(const HBMKPadData* data, bool updatePos);
    void resetCounters();
    void resetVolumeLimits();
    void connectSpeaker();
    void noOp0();
    void setVolume(f32 volume);
    void playSound(nw4r::snd::SoundArchivePlayer* player, int id);
    bool isPlayingSoundId(int soundId) const;
    void clearSoundFlags();
    void updateSound();
    bool isPlayReady() const;
    HBController* getController();
    void startMotor();
    void stopMotor();
    s32 getInfoAsync(WPADInfo* info);
    bool getPlayerFlag() const;
    void clearPlayerFlag();

    static void wpadConnectCallback(s32 chan, s32 result);
    static void wpadExtensionCallback(s32 chan, s32 result);
    static void soundOnCallback(OSAlarm* alarm, OSContext* context);

    // Mutes the speaker, and unmutes it again after the given time.
    void soundOff(s32 msec) {
        s32 chan = mHBController.chan;
        if (WPADIsSpeakerEnabled(chan)) {
            WPADControlSpeaker(chan, WPAD_SPEAKER_MUTE, NULL);
            OSSetAlarmUserData(&sAlarmSoundOff[chan], (void*)chan);
            OSCancelAlarm(&sAlarmSoundOff[chan]);
            OSSetAlarm(&sAlarmSoundOff[chan], OS_MSEC_TO_TICKS(msec), soundOnCallback);
            mSoundOffFlag = true;
        }
    }

    HBController mHBController;                   // at 0x00
    nw4r::snd::SoundHandle mSoundHandle;          // at 0x20
    RemoteSpk* mRemoteSpk;                        // at 0x24
    WPADConnectCallback mOldConnectCallback;      // at 0x28
    WPADExtensionCallback mOldExtensionCallback;  // at 0x2C
    s64 mPlaySoundTime;                           // at 0x30
    s64 mStopSoundTime;                           // at 0x38
    bool mCallbackFlag;                           // at 0x40
    bool mSoundOffFlag;                           // at 0x41
    bool mCheckSoundTimeFlag;                     // at 0x42
    bool mCheckSoundIntervalFlag;                 // at 0x43
    bool mMotorEnabled;                           // at 0x44

    static bool sPlayerFlags[WPAD_MAX_CONTROLLERS];
    static OSAlarm sAlarm[WPAD_MAX_CONTROLLERS];
    static OSAlarm sAlarmSoundOff[WPAD_MAX_CONTROLLERS];
    static Controller* sThis[WPAD_MAX_CONTROLLERS];
};

void setPlayerFlag(s32 chan, s32 result);

}  // namespace homebutton

#endif
