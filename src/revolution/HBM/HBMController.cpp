#include <nw4r_compat/cmath>
#include <nw4r_compat/cstdarg>
#include <nw4r_compat/cstdio>
#include <nw4r_compat/cstring>
#include <nw4r_compat/cwchar>

// RVL_SDK Home Button Menu - controller.
//
// homebutton::Controller wraps one Wiimote: it owns the pointer and button state,
// the rumble, and the alarms that mute the speaker for a while when the signal
// gets weak or a sound has played for too long. The speaker itself lives in
// HBMRemoteSpk.cpp.

#include <revolution/HBM/HBM.h>

namespace homebutton {

bool Controller::sPlayerFlags[WPAD_MAX_CONTROLLERS];
OSAlarm Controller::sAlarm[WPAD_MAX_CONTROLLERS];
OSAlarm Controller::sAlarmSoundOff[WPAD_MAX_CONTROLLERS];
Controller* Controller::sThis[WPAD_MAX_CONTROLLERS];

void Controller::wpadConnectCallback(s32 chan, s32 result) {
    switch (result) {
    case WPAD_ERR_OK:
        if (!sThis[chan]->mCallbackFlag) {
            sThis[chan]->mOldExtensionCallback = WPADSetExtensionCallback(chan, wpadExtensionCallback);
            sThis[chan]->mCallbackFlag = true;
        }
        WPADControlSpeaker(chan, WPAD_SPEAKER_OFF, NULL);
        break;
    case WPAD_ERR_NO_CONTROLLER:
        WPADSetExtensionCallback(chan, sThis[chan]->mOldExtensionCallback);
        sThis[chan]->mOldExtensionCallback = NULL;
        sThis[chan]->mCallbackFlag = false;
        sThis[chan]->mCheckSoundTimeFlag = false;
        sThis[chan]->mCheckSoundIntervalFlag = false;
        break;
    }

    if (sThis[chan]->mOldConnectCallback != NULL) {
        sThis[chan]->mOldConnectCallback(chan, result);
    }
}

void Controller::wpadExtensionCallback(s32 chan, s32 result) {
    switch (result) {
    case WPAD_DEV_UNKNOWN:
        sThis[chan]->soundOff(1000);
        break;
    }

    if (sThis[chan]->mOldExtensionCallback != NULL) {
        sThis[chan]->mOldExtensionCallback(chan, result);
    }
}

void Controller::soundOnCallback(OSAlarm* alarm, OSContext* context) {
    s32 index = (s32)OSGetAlarmUserData(alarm);
    Controller* controller = sThis[index];
    s32 chan = controller->mHBController.chan;

    if (WPADIsSpeakerEnabled(chan)) {
        WPADControlSpeaker(chan, WPAD_SPEAKER_UNMUTE, NULL);
    }
    controller->mSoundOffFlag = false;
}

Controller::Controller(int chan, RemoteSpk* spk) {
    mHBController.chan = chan;
    mHBController.rumble = false;
    mHBController.spVol = 1.0f;
    mRemoteSpk = spk;
    mOldConnectCallback = NULL;
    mOldExtensionCallback = NULL;
    mCallbackFlag = false;
    mSoundOffFlag = false;
    mMotorEnabled = true;

    if (chan < WPAD_MAX_CONTROLLERS) {
        sPlayerFlags[chan] = false;
        OSCreateAlarm(&sAlarm[chan]);
        OSCreateAlarm(&sAlarmSoundOff[chan]);
        sThis[chan] = this;
    }
}

Controller::~Controller() {
    OSCancelAlarm(&sAlarm[mHBController.chan]);
    OSCancelAlarm(&sAlarmSoundOff[mHBController.chan]);
}

void Controller::initCallback() {
    s32 type;

    mOldConnectCallback = WPADSetConnectCallback(mHBController.chan, wpadConnectCallback);
    mMotorEnabled = true;

    switch (WPADProbe(mHBController.chan, &type)) {
    case WPAD_ERR_OK:
        mOldExtensionCallback = WPADSetExtensionCallback(mHBController.chan, wpadExtensionCallback);
        mCallbackFlag = true;
        break;
    case WPAD_ERR_NO_CONTROLLER:
        mCallbackFlag = false;
        break;
    }
}

void Controller::clearCallback() {
    WPADSetConnectCallback(mHBController.chan, mOldConnectCallback);
    mOldConnectCallback = NULL;
    WPADSetExtensionCallback(mHBController.chan, mOldExtensionCallback);
    mOldExtensionCallback = NULL;
}

void Controller::setKpad(const HBMKPadData* data, bool updatePos) {
    if (data->kpad == NULL) {
        return;
    }

    if (updatePos) {
        if (data->kpad->dev_type == WPAD_DEV_CLASSIC && data->use_devices == WPAD_DEV_CLASSIC) {
            mHBController.x = data->pos.x;
            mHBController.y = data->pos.y;
        } else {
            mHBController.x = data->kpad->pos.x;
            mHBController.y = data->kpad->pos.y;
        }
    }

    KPADStatus* kpad = data->kpad;
    mHBController.trig = kpad->trig;
    mHBController.hold = kpad->hold;
    mHBController.release = kpad->release;

    if (kpad->dev_type == WPAD_DEV_CLASSIC && data->use_devices == WPAD_DEV_CLASSIC) {
        u32 hold = kpad->ex_status.cl.hold;
        u32 trig = kpad->ex_status.cl.trig;
        u32 release = kpad->ex_status.cl.release;

        if (hold & WPAD_BUTTON_CL_A) {
            mHBController.hold |= WPAD_BUTTON_A;
        }
        if (trig & WPAD_BUTTON_CL_A) {
            mHBController.trig |= WPAD_BUTTON_A;
        }
        if (release & WPAD_BUTTON_CL_A) {
            mHBController.release |= WPAD_BUTTON_A;
        }

        if (hold & WPAD_BUTTON_CL_PLUS) {
            mHBController.hold |= WPAD_BUTTON_PLUS;
        }
        if (trig & WPAD_BUTTON_CL_PLUS) {
            mHBController.trig |= WPAD_BUTTON_PLUS;
        }
        if (release & WPAD_BUTTON_CL_PLUS) {
            mHBController.release |= WPAD_BUTTON_PLUS;
        }

        if (hold & WPAD_BUTTON_CL_MINUS) {
            mHBController.hold |= WPAD_BUTTON_MINUS;
        }
        if (trig & WPAD_BUTTON_CL_MINUS) {
            mHBController.trig |= WPAD_BUTTON_MINUS;
        }
        if (release & WPAD_BUTTON_CL_MINUS) {
            mHBController.release |= WPAD_BUTTON_MINUS;
        }

        if (hold & WPAD_BUTTON_CL_HOME) {
            mHBController.hold |= WPAD_BUTTON_HOME;
        }
        if (trig & WPAD_BUTTON_CL_HOME) {
            mHBController.trig |= WPAD_BUTTON_HOME;
        }
        if (release & WPAD_BUTTON_CL_HOME) {
            mHBController.release |= WPAD_BUTTON_HOME;
        }
    }
}

void Controller::resetCounters() {
    mHBController.trig = 0;
    mHBController.hold = 0;
    mHBController.release = 0;
}

void Controller::resetVolumeLimits() {
    mHBController.x = -10000.0f;
    mHBController.y = -10000.0f;
}

void Controller::connectSpeaker() {
    mRemoteSpk->Connect(mHBController.chan);
}

void Controller::noOp0() {}

void Controller::setVolume(f32 volume) {
    mHBController.spVol = volume;
}

void Controller::playSound(nw4r::snd::SoundArchivePlayer* player, int id) {
    if (mSoundOffFlag) {
        return;
    }

    f32 vol = mHBController.spVol;
    mRemoteSpk->Play(mHBController.chan, id, 10.0f * vol);
    if (WPADIsSpeakerEnabled(mHBController.chan)) {
        if (!mCheckSoundTimeFlag) {
            mPlaySoundTime = OSGetTime();
        }
        mCheckSoundTimeFlag = true;
        mCheckSoundIntervalFlag = false;
    }
}

bool Controller::isPlayingSoundId(int soundId) const {
    return !mRemoteSpk->isPlaying(mHBController.chan) ? false : mRemoteSpk->isPlayingId(mHBController.chan, soundId);
}

void Controller::clearSoundFlags() {
    mCheckSoundTimeFlag = false;
    mCheckSoundIntervalFlag = false;
}

void Controller::updateSound() {
    s32 chan = mHBController.chan;

    if (!mRemoteSpk->isPlaying(chan)) {
        if (mCheckSoundTimeFlag) {
            if (!mCheckSoundIntervalFlag) {
                mStopSoundTime = OSGetTime();
                mCheckSoundIntervalFlag = true;
            } else if (OS_TICKS_TO_MSEC((u32)OSGetTime() - (u32)mStopSoundTime) >= 1000) {
                mCheckSoundTimeFlag = false;
                mCheckSoundIntervalFlag = false;
            }
        }
    } else {
        if (mCheckSoundTimeFlag) {
            mCheckSoundIntervalFlag = false;
            if (OS_TICKS_TO_MSEC((u32)OSGetTime() - (u32)mPlaySoundTime) >= 480000) {
                mCheckSoundTimeFlag = false;
                mCheckSoundIntervalFlag = false;
                soundOff(1000);
                return;
            }
        }

        if (!mSoundOffFlag && WPADGetRadioSensitivity(chan) <= 85) {
            soundOff(1000);
        }
    }
}

bool Controller::isPlayReady() const {
    return mRemoteSpk->isPlayReady(mHBController.chan);
}

HBController* Controller::getController() {
    return &mHBController;
}

void Controller::startMotor() {
    if (mHBController.chan >= WPAD_MAX_CONTROLLERS) {
        return;
    }

    if (!mRemoteSpk->isPlaying(mHBController.chan) && mMotorEnabled) {
        mHBController.rumble = true;
        WPADControlMotor(mHBController.chan, WPAD_MOTOR_RUMBLE);
    }
}

void Controller::stopMotor() {
    if (mHBController.chan >= WPAD_MAX_CONTROLLERS) {
        return;
    }

    if (mHBController.rumble) {
        mHBController.rumble = false;
        WPADControlMotor(mHBController.chan, WPAD_MOTOR_STOP);
    }
}

s32 Controller::getInfoAsync(WPADInfo* info) {
    if (mHBController.chan >= WPAD_MAX_CONTROLLERS) {
        return WPAD_ERR_BUSY;
    }

    if (mRemoteSpk->isPlaying(mHBController.chan) || mHBController.rumble) {
        return WPAD_ERR_BUSY;
    }

    return WPADGetInfoAsync(mHBController.chan, info, setPlayerFlag);
}

// WPADGetInfoAsync callback: records that the controller answered.
void setPlayerFlag(s32 chan, s32 result) {
    if (result != WPAD_ERR_OK) {
        return;
    }

    if (chan >= WPAD_MAX_CONTROLLERS) {
        return;
    }

    Controller::sPlayerFlags[chan] = true;
}

bool Controller::getPlayerFlag() const {
    if (mHBController.chan >= WPAD_MAX_CONTROLLERS) {
        return 0;
    }

    return sPlayerFlags[mHBController.chan];
}

void Controller::clearPlayerFlag() {
    if (mHBController.chan >= WPAD_MAX_CONTROLLERS) {
        return;
    }

    sPlayerFlags[mHBController.chan] = false;
}

}  // namespace homebutton
