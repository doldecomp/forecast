#include <nw4r_compat/cmath>
#include <nw4r_compat/cstdarg>
#include <nw4r_compat/cstdio>
#include <nw4r_compat/cstring>
#include <nw4r_compat/cwchar>

#include <revolution/OS.h>
#include <revolution/WPAD.h>
#include <types.h>

// RVL_SDK Home Button Menu - remote speaker.
//
// homebutton::RemoteSpk streams the menu sounds to the four Wiimote speakers:
// a periodic alarm encodes 40 samples per channel with WENC and sends them.

#include <revolution/HBM/HBM.h>

namespace homebutton {

RemoteSpk* RemoteSpk::spInstance;

static void MakeVolumeData(const s16* src, s16* dst, int vol, u32 size) {
    u32 encSize = size > 40 ? 40 : size;
    int i;
    for (i = 0; i < encSize; i++) {
        *dst++ = *src++ * vol / 10;
    }
    if (size <= 40) {
        for (i = 0; i < 40 - size; i++) {
            *dst++ = 0;
        }
    }
}

void RemoteSpk::UpdateSpeaker(OSAlarm* alarm, OSContext* context) {
    s16 pcm[40];
    u8 adpcm[20];

    if (GetInstance() == NULL) {
        return;
    }

    RemoteSpkChanInfo* pinfo = GetInstance()->info;
    for (int i = 0; i < WPAD_MAX_CONTROLLERS; i++, pinfo++) {
        if (pinfo->in_pcm == NULL || !WPADIsSpeakerEnabled(i)) {
            continue;
        }

        BOOL enabled = OSDisableInterrupts();
        if (WPADCanSendStreamData(i)) {
            MakeVolumeData(pinfo->in_pcm, pcm, pinfo->vol, (u32)pinfo->length >> 1);
            WENCGetEncodeData(&pinfo->wencinfo, pinfo->first ? 0 : 1, pcm, 40, adpcm);
            WPADSendStreamData(i, adpcm, 20);
            pinfo->first = false;
            pinfo->cannotSendCnt = 0;
            pinfo->in_pcm += 40;
            pinfo->length -= 80;
            if (pinfo->length <= 0) {
                pinfo->seId = -1;
                pinfo->in_pcm = NULL;
            }
        } else {
            pinfo->cannotSendCnt++;
            if (pinfo->cannotSendCnt > 300) {
                pinfo->in_pcm = NULL;
            }
        }
        OSRestoreInterrupts(enabled);
    }
}

void RemoteSpk::ClearPcm() {
    RemoteSpk* pThis = GetInstance();
    for (int i = 0; i < WPAD_MAX_CONTROLLERS; i++) {
        pThis->info[i].seId = -1;
        pThis->info[i].in_pcm = NULL;
    }
}

RemoteSpk::RemoteSpk(void* spkSeBuf) {
    SetInstance(this);

    if (spkSeBuf != NULL) {
        available = ARCInitHandle(spkSeBuf, &handle);
    } else {
        available = false;
    }

    OSCreateAlarm(&speakerAlarm);
    for (int i = 0; i < WPAD_MAX_CONTROLLERS; i++) {
        OSCreateAlarm(&info[i].alarm);
        info[i].in_pcm = NULL;
        info[i].seId = -1;
        info[i].first = true;
        info[i].playReady = true;
    }
}

RemoteSpk::~RemoteSpk() {
    SetInstance(NULL);
    available = false;
    OSCancelAlarm(&speakerAlarm);
    for (int i = 0; i < WPAD_MAX_CONTROLLERS; i++) {
        OSCancelAlarm(&info[i].alarm);
    }
}

void RemoteSpk::Start() {
    if (!available) {
        return;
    }

    OSCreateAlarm(&speakerAlarm);
    OSSetPeriodicAlarm(&speakerAlarm, OSGetTime(), OS_NSEC_TO_TICKS(6666667), UpdateSpeaker);
}

void RemoteSpk::cancelAlarm() {
    OSCancelAlarm(&speakerAlarm);
}

void RemoteSpk::DelaySpeakerOnCallback(OSAlarm* alarm, OSContext* context) {
    s32 chan = (s32)OSGetAlarmUserData(alarm);
    WPADControlSpeaker(chan, WPAD_SPEAKER_ON, SpeakerOnCallback);
}

void RemoteSpk::SpeakerOnCallback(s32 chan, s32 result) {
    RemoteSpk* pThis = GetInstance();
    if (pThis == NULL) {
        return;
    }

    switch (result) {
    case WPAD_ERR_OK:
        pThis->info[chan].first = true;
        WPADControlSpeaker(chan, WPAD_SPEAKER_PLAY, SpeakerPlayCallback);
        break;
    case WPAD_ERR_BUSY:
        OSSetAlarmUserData(&pThis->info[chan].alarm, (void*)chan);
        OSCancelAlarm(&pThis->info[chan].alarm);
        OSSetAlarm(&pThis->info[chan].alarm, OS_MSEC_TO_TICKS(50), DelaySpeakerOnCallback);
        break;
    case WPAD_ERR_NO_CONTROLLER:
        break;
    }
}

void RemoteSpk::DelaySpeakerPlayCallback(OSAlarm* alarm, OSContext* context) {
    s32 chan = (s32)OSGetAlarmUserData(alarm);
    WPADControlSpeaker(chan, WPAD_SPEAKER_PLAY, SpeakerPlayCallback);
}

void RemoteSpk::SpeakerPlayCallback(s32 chan, s32 result) {
    RemoteSpk* pThis = GetInstance();
    if (pThis == NULL) {
        return;
    }

    switch (result) {
    case WPAD_ERR_OK:
        pThis->info[chan].playReady = true;
        break;
    case WPAD_ERR_NO_CONTROLLER:
        pThis->info[chan].playReady = false;
        break;
    case WPAD_ERR_BUSY:
        OSSetAlarmUserData(&pThis->info[chan].alarm, (void*)chan);
        OSCancelAlarm(&pThis->info[chan].alarm);
        OSSetAlarm(&pThis->info[chan].alarm, OS_MSEC_TO_TICKS(50), DelaySpeakerPlayCallback);
        break;
    }
}

void RemoteSpk::Connect(s32 chan) {
    if (!available) {
        return;
    }

    WPADControlSpeaker(chan, WPAD_SPEAKER_ON, SpeakerOnCallback);
    memset(&info[chan].wencinfo, 0, sizeof(WENCInfo));
    info[chan].first = true;
    info[chan].playReady = false;
}

static inline void GetPCMFromSeID(ARCHandle* handle, int seId, const s16*& wave, s32& length) {
    ARCFileInfo af;
    ARCFastOpen(handle, seId, &af);
    wave = (const s16*)ARCGetStartAddrInMem(&af);
    length = ARCGetLength(&af);
    ARCClose(&af);
}

void RemoteSpk::Play(s32 chan, int seId, s8 vol) {
    if (!available) {
        return;
    }

    const s16* wave;
    s32 length;
    GetPCMFromSeID(&handle, seId, wave, length);
    info[chan].cannotSendCnt = 0;
    info[chan].seId = seId;
    info[chan].length = length;
    info[chan].vol = vol;
    info[chan].in_pcm = wave;
}

bool RemoteSpk::isPlaying(s32 chan) const {
    return info[chan].in_pcm != NULL;
}

bool RemoteSpk::isPlayingId(s32 chan, int seId) const {
    if (info[chan].in_pcm != NULL && info[chan].seId == seId) {
        return true;
    }

    return false;
}

bool RemoteSpk::isPlayReady(s32 chan) const {
    return info[chan].playReady;
}

}  // namespace homebutton
