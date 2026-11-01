// The channel's sound system: the main sound archive and its player, the HOME
// Menu sound archive, a reverb on AUX C and a voice effect on AUX B.
// The original file is sound_manager.cpp (the name in its panic messages).
#include <nw4r/math.h>
#include <nw4r/snd.h>

#include <channel/SoundManager.h>

#include <revolution/AI.h>
#include <revolution/AX.h>
#include <revolution/OS.h>
#include <revolution/SC/scapi.h>

using namespace nw4r;

// ---------------------------------------------------------------------------
// Voice effect
// ---------------------------------------------------------------------------

// Samples the AUX callback hands over per channel and call (3 ms at 32 kHz)
#define VOICE_FRAME_LENGTH 96
// Calls of history kept per channel; positions wrap with % VOICE_HISTORY_FRAMES
#define VOICE_HISTORY_FRAMES 64
#define VOICE_HISTORY_LENGTH (VOICE_FRAME_LENGTH * VOICE_HISTORY_FRAMES)
// One period of the chorus delay modulation: 30 calls
#define VOICE_LFO_LENGTH (VOICE_FRAME_LENGTH * 30)

// Fixed-point numbers in this effect have 12 fractional bits.
#define FIX_ONE 0x1000

// Reworks the voices on AUX B: an echo, half speed, a chorus or a radio
// sound, optionally shifted up by a fifth.
class VoiceEffect : public snd::FxBase {
public:
    enum Mode {
        MODE_OFF,
        MODE_ECHO,
        MODE_HALF_SPEED,
        MODE_CHORUS,
        MODE_RADIO,
    };

    VoiceEffect();
    virtual ~VoiceEffect() {}

    virtual void UpdateBuffer(int channels, void** buffers, u32 size, snd::SampleFormat format,
                              f32 sampleRate, snd::OutputMode mode);

private:
    inline void Echo(s32** out);
    void HalfSpeed(s32** out);
    inline void Chorus(s32** out);
    void Radio(s32** out);
    void ShiftUp(s32** out);

    // The history sample `pos` samples into the call `frame` calls from now
    // (both may be negative or larger than a call).
    s32* At(s32* history, s32 frame, s32 pos) {
        frame += pos / VOICE_FRAME_LENGTH;
        pos %= VOICE_FRAME_LENGTH;
        if (pos < 0) {
            frame--;
            pos += VOICE_FRAME_LENGTH;
        }
        frame = (mFrame + frame + VOICE_HISTORY_FRAMES) % VOICE_HISTORY_FRAMES;
        return &history[frame * VOICE_FRAME_LENGTH + pos];
    }

    inline void Gather(s32 (*history)[VOICE_HISTORY_LENGTH], s32 ch, s32 count, s32* dst, s32 start, s32 pos);

public:
    bool mEnabled;                           // at 0xC
    bool mShiftUp;                           // at 0xD
    s32 mMode;                               // at 0x10
    s32 mLastMode;                           // at 0x14
    s32 mFrame;                              // at 0x18
    s32 mInput[2][VOICE_HISTORY_LENGTH];     // at 0x1C
    s32 mOutput[2][VOICE_HISTORY_LENGTH];    // at 0xC01C
    s32 mEchoBand[11];                       // at 0x1801C
    s32 mRadioBand[21];                      // at 0x18048
    s32 mSlowWindow[VOICE_HISTORY_LENGTH];   // at 0x1809C
    s32 mChorusLfo[VOICE_LFO_LENGTH];        // at 0x1E09C
    s32 mShiftWindow[VOICE_HISTORY_LENGTH];  // at 0x20D9C
    s32 mShiftLowPass[11];                   // at 0x26D9C
    u32 mBusyTicks;                          // at 0x26DC8
    s32 mUnk26DCC[2];                        // at 0x26DCC
    s32 mUnk26DD4[2];                        // at 0x26DD4
};

static void BuildWindow(s32* table, s32 length, s32 shape);
static void CopyWords(const s32* src, s32* dst, s32 count);

// ---------------------------------------------------------------------------
// Sound archives
// ---------------------------------------------------------------------------

// Keeps track of one sound handle the game watches.
struct HandleWatch {
    HandleWatch() {
        Reset(NULL);
    }

    void Reset(snd::SoundHandle* handle) {
        mHandle = handle;
        mUnk1 = false;
        mUnk0 = false;
    }

    bool mUnk0;                // at 0x0
    bool mUnk1;                // at 0x1
    snd::SoundHandle* mHandle; // at 0x4
};

// A volume that moves towards its target by a fixed step every frame.
struct VolumeRamp {
    VolumeRamp(f32 target) {
        mStep = 1.0f / 60.0f;
        mValue = 1.0f;
        mTarget = target;
        mActive = false;
    }

    void Update() {
        f32 goal = mActive ? mTarget : 1.0f;
        if (goal != mValue) {
            if (goal > mValue) {
                mValue += mStep;
                if (mValue > goal) {
                    mValue = goal;
                }
            } else {
                mValue -= mStep;
                if (mValue < goal) {
                    mValue = goal;
                }
            }
        }
    }

    f32 mStep;    // at 0x0
    f32 mValue;   // at 0x4
    f32 mTarget;  // at 0x8
    bool mActive; // at 0xC
};

// The HOME Menu's own sound archive and player.
class HomeMenuSound {
public:
    HomeMenuSound()
        : mReady(false),
          mArchive(NULL),
          mPlayer(NULL),
          mHandle(NULL),
          mHeap(NULL),
          mPlayerWork(NULL),
          mStreamWork(NULL),
          mHeapWork(NULL),
          mHeaderWork(NULL),
          mLabelWork(NULL) {}

    void Setup(bool inMemory, const void* data, const char* path);
    void Teardown();

    bool mReady;                      // at 0x0
    bool mUnk1;                       // at 0x1
    snd::SoundArchive* mArchive;      // at 0x4
    snd::SoundArchivePlayer* mPlayer; // at 0x8
    snd::SoundHandle* mHandle;        // at 0xC
    snd::SoundHeap* mHeap;            // at 0x10
    void* mPlayerWork;                // at 0x14
    void* mStreamWork;                // at 0x18
    void* mHeapWork;                  // at 0x1C
    void* mHeaderWork;                // at 0x20
    void* mLabelWork;                 // at 0x24
};

// Sound heap sizes when the archives are streamed from the disc
#define MAIN_HEAP_SIZE 0x1F4000
#define HOME_MENU_HEAP_SIZE 0x7D0000

static const snd::FxReverbHi::ReverbHiParam sAuxReverbParam = {0.1f, 2.1f, 0.6f, 0.4f, 0.1f, 0.2f};

static u8 sOutputMode = SC_SND_STEREO;

static bool sArchiveInMemory;
static bool sReady;
static SoundAllocFunc sAllocate;
static SoundFreeFunc sRelease;
static snd::SoundArchive* sMainArchive;
static snd::SoundArchivePlayer* sMainPlayer;
static snd::SoundHandle* sEffectHandle;
static snd::SoundHeap* sMainHeap;
static void* sPlayerWork;
static void* sStreamWork;
static void* sHeapWork;
static void* sReverbWork;
static void* sHeaderWork;
static void* sLabelWork;
static snd::FxReverbHi* sAuxReverb;
static VoiceEffect* sVoiceEffect;
static HandleWatch sHandleWatch;
static s32 sNoiseState;

static VolumeRamp sVolumeRamps[3] = {VolumeRamp(0.0f), VolumeRamp(0.0f), VolumeRamp(0.5f)};
static HomeMenuSound sHomeMenuSound;

static inline void ApplyOutputMode(u8 scMode) {
    switch (scMode) {
    case SC_SND_MONO:
        snd::detail::AxManager::GetInstance().SetOutputMode(snd::OUTPUT_MODE_MONO);
        break;
    case SC_SND_STEREO:
    case SC_SND_SURROUND:
    default:
        snd::detail::AxManager::GetInstance().SetOutputMode(snd::OUTPUT_MODE_STEREO);
        break;
    }
}

static void SetupSoundManager(bool inMemory, const void* data, const char* path, const void* hbmData,
                              const char* hbmPath, SoundAllocFunc alloc, SoundFreeFunc free);

void InitSoundManager(const void* archive, const void* hbmArchive, SoundAllocFunc alloc,
                      SoundFreeFunc free) {
    SetupSoundManager(true, archive, NULL, hbmArchive, NULL, alloc, free);
}

static void SetupSoundManager(bool inMemory, const void* data, const char* path, const void* hbmData,
                              const char* hbmPath, SoundAllocFunc alloc, SoundFreeFunc free) {
    if (sReady) {
        return;
    }

    sReady = true;
    sArchiveInMemory = inMemory;
    sAllocate = alloc;
    sRelease = free;

    snd::DvdSoundArchive* dvdArchive = NULL;
    snd::MemorySoundArchive* memArchive = NULL;

    if (inMemory) {
        sMainArchive = memArchive = new snd::MemorySoundArchive();
    } else {
        sMainArchive = dvdArchive = new snd::DvdSoundArchive();
        sMainHeap = new snd::SoundHeap();
    }

    sMainPlayer = new snd::SoundArchivePlayer();
    sEffectHandle = new snd::SoundHandle();
    sAuxReverb = new snd::FxReverbHi();

    if (!AICheckInit()) {
        AIInit(NULL);
        AXInit();
    }

    snd::SoundSystem::InitSoundSystem(4, 3);

    if (sArchiveInMemory) {
        memArchive->Setup(data);
    } else {
        if (!dvdArchive->Open(path)) {
            OSPanic("sound_manager.cpp", 413, "Cannot open Sound Archive File");
        }

        u32 size = dvdArchive->GetHeaderSize();
        sHeaderWork = sAllocate(size);
        dvdArchive->LoadHeader(sHeaderWork, size);

        size = dvdArchive->GetLabelStringDataSize();
        sLabelWork = sAllocate(size);
        dvdArchive->LoadLabelStringData(sLabelWork, size);
    }

    u32 playerSize = sMainPlayer->GetRequiredMemSize(sMainArchive);
    u32 streamSize = sMainPlayer->GetRequiredStrmBufferSize(sMainArchive);
    sPlayerWork = sAllocate(playerSize);
    sStreamWork = sAllocate(streamSize);
    sMainPlayer->Setup(sMainArchive, sPlayerWork, playerSize, sStreamWork, streamSize);

    if (!sArchiveInMemory) {
        sHeapWork = sAllocate(MAIN_HEAP_SIZE);
        sMainHeap->Create(sHeapWork, MAIN_HEAP_SIZE);
        sMainPlayer->LoadGroup(0, sMainHeap, 0);
    }

    sAuxReverb->SetParam(sAuxReverbParam);
    u32 reverbSize = sAuxReverb->GetRequiredMemSize();
    sReverbWork = sAllocate(reverbSize);
    sAuxReverb->AssignWorkBuffer(sReverbWork, reverbSize);
    snd::detail::AxManager::GetInstance().AppendEffect(snd::AUX_C, sAuxReverb);

    VoiceEffect* voice = new VoiceEffect();
    sVoiceEffect = voice;
    snd::detail::AxManager::GetInstance().AppendEffect(snd::AUX_B, voice);

    if ((inMemory && hbmData != NULL) || (!inMemory && hbmPath != NULL)) {
        sHomeMenuSound.Setup(inMemory, hbmData, hbmPath);
    }

    ApplyOutputMode(sOutputMode);

    sVolumeRamps[0].mActive = false;
    sHomeMenuSound.mUnk1 = false;
    sVolumeRamps[1].mActive = false;
}

void ExitSoundManager() {
    if (!sReady) {
        return;
    }

    sReady = false;

    snd::detail::AxManager::GetInstance().ClearEffect(snd::AUX_C, 0);
    snd::detail::AxManager::GetInstance().ClearEffect(snd::AUX_B, 0);

    delete sVoiceEffect;
    sVoiceEffect = NULL;

    sHomeMenuSound.Teardown();
    sMainPlayer->Shutdown();

    if (!sArchiveInMemory) {
        sMainHeap->Clear();
    }

    if (sMainArchive != NULL) {
        delete sMainArchive;
        sMainArchive = NULL;
    }
    if (sMainPlayer != NULL) {
        delete sMainPlayer;
        sMainPlayer = NULL;
    }
    if (sEffectHandle != NULL) {
        delete sEffectHandle;
        sEffectHandle = NULL;
    }
    if (sMainHeap != NULL) {
        delete sMainHeap;
        sMainHeap = NULL;
    }
    if (sAuxReverb != NULL) {
        delete sAuxReverb;
        sAuxReverb = NULL;
    }

    if (sPlayerWork != NULL) {
        sRelease(sPlayerWork);
        sPlayerWork = NULL;
    }
    if (sStreamWork != NULL) {
        sRelease(sStreamWork);
        sStreamWork = NULL;
    }
    if (sHeapWork != NULL) {
        sRelease(sHeapWork);
        sHeapWork = NULL;
    }
    if (sReverbWork != NULL) {
        sRelease(sReverbWork);
        sReverbWork = NULL;
    }
    if (sHeaderWork != NULL) {
        sRelease(sHeaderWork);
        sHeaderWork = NULL;
    }
    if (sLabelWork != NULL) {
        sRelease(sLabelWork);
        sLabelWork = NULL;
    }
}

void UpdateSoundManager() {
    sVolumeRamps[0].Update();
    sVolumeRamps[1].Update();
    sVolumeRamps[2].Update();

    sMainPlayer->Update();

    if (sHomeMenuSound.mReady) {
        sHomeMenuSound.mPlayer->Update();
    }
}

void SetSoundManagerOutputMode(u8 scMode) {
    sOutputMode = scMode;
    ApplyOutputMode(scMode);
}

void HomeMenuSound::Setup(bool inMemory, const void* data, const char* path) {
    if (mReady) {
        return;
    }

    mReady = true;

    snd::DvdSoundArchive* dvdArchive = NULL;
    snd::MemorySoundArchive* memArchive = NULL;

    if (inMemory) {
        mArchive = memArchive = new snd::MemorySoundArchive();
    } else {
        mArchive = dvdArchive = new snd::DvdSoundArchive();
        mHeap = new snd::SoundHeap();
    }

    mPlayer = new snd::SoundArchivePlayer();
    mHandle = new snd::SoundHandle();

    if (inMemory) {
        memArchive->Setup(data);
    } else {
        if (!dvdArchive->Open(path)) {
            OSPanic("sound_manager.cpp", 639, "Cannot open HBM Sound Archive File");
        }

        u32 size = dvdArchive->GetHeaderSize();
        mHeaderWork = sAllocate(size);
        dvdArchive->LoadHeader(mHeaderWork, size);

        size = dvdArchive->GetLabelStringDataSize();
        mLabelWork = sAllocate(size);
        dvdArchive->LoadLabelStringData(mLabelWork, size);
    }

    u32 playerSize = mPlayer->GetRequiredMemSize(mArchive);
    u32 streamSize = mPlayer->GetRequiredStrmBufferSize(mArchive);
    mPlayerWork = sAllocate(playerSize);
    mStreamWork = sAllocate(streamSize);
    mPlayer->Setup(mArchive, mPlayerWork, playerSize, mStreamWork, streamSize);

    if (!inMemory) {
        mHeapWork = sAllocate(HOME_MENU_HEAP_SIZE);
        mHeap->Create(mHeapWork, HOME_MENU_HEAP_SIZE);
        mPlayer->LoadGroup(0, mHeap, 0);
    }
}

void HomeMenuSound::Teardown() {
    if (!mReady) {
        return;
    }

    mReady = false;
    mPlayer->Shutdown();

    if (!sArchiveInMemory) {
        mHeap->Clear();
    }

    if (mArchive != NULL) {
        delete mArchive;
        mArchive = NULL;
    }
    if (mPlayer != NULL) {
        delete mPlayer;
        mPlayer = NULL;
    }
    if (mHandle != NULL) {
        delete mHandle;
        mHandle = NULL;
    }
    if (mHeap != NULL) {
        delete mHeap;
        mHeap = NULL;
    }

    if (mPlayerWork != NULL) {
        sRelease(mPlayerWork);
        mPlayerWork = NULL;
    }
    if (mStreamWork != NULL) {
        sRelease(mStreamWork);
        mStreamWork = NULL;
    }
    if (mHeapWork != NULL) {
        sRelease(mHeapWork);
        mHeapWork = NULL;
    }
    if (mHeaderWork != NULL) {
        sRelease(mHeaderWork);
        mHeaderWork = NULL;
    }
    if (mLabelWork != NULL) {
        sRelease(mLabelWork);
        mLabelWork = NULL;
    }
}

// ---------------------------------------------------------------------------
// Sound handles
// ---------------------------------------------------------------------------

static inline void StartWatched(snd::SoundHandle* handle, u32 id) {
    sMainPlayer->StartSound(handle, id);
    if (handle == sHandleWatch.mHandle) {
        sHandleWatch.Reset(handle);
    }
}

// Sets the volume; sequences below 0.05 also get all their tracks muted.
static inline void SetVolumeAndMute(snd::SoundHandle* handle, f32 volume) {
    handle->SetVolume(volume, 0);
    if (sMainArchive->GetSoundType(handle->GetId()) == snd::SOUND_TYPE_SEQ) {
        snd::SeqSoundHandle seq(handle);
        bool mute = volume < 0.05f;
        seq.SetTrackMute(0xFFFFFFFF, mute);
    }
}

void PlaySE(s32 id) {
    snd::SoundHandle* handle = sEffectHandle;
    StartWatched(handle, id);
    SetVolumeAndMute(handle, 1.0f);
    handle->SetPitch(1.0f);
    handle->SetPan(0.0f);
}

void PlayLoopSE(s32 id, f32 volume, f32 pitch, f32 pan, f32 unused) {
    snd::SoundHandle* handle = sEffectHandle;
    StartWatched(handle, id);
    SetVolumeAndMute(handle, volume);
    handle->SetPitch(pitch);
    handle->SetPan(pan);
}

void StartSound(snd::SoundHandle* handle, u32 id) {
    StartWatched(handle, id);
}

void StopSound(snd::SoundHandle* handle, s32 fadeFrames) {
    handle->Stop(fadeFrames);
}

void PauseSound(snd::SoundHandle* handle, bool pause, s32 fadeFrames) {
    handle->Pause(pause, fadeFrames);
}

bool IsSoundPaused(snd::SoundHandle* handle) {
    return handle->IsAttachedSound() && handle->detail_GetAttachedSound()->IsPause();
}

void SetSoundVolume(snd::SoundHandle* handle, f32 volume) {
    SetVolumeAndMute(handle, volume);
}

void SetSoundPitch(snd::SoundHandle* handle, f32 pitch) {
    handle->SetPitch(pitch);
}

void SetSoundPan(snd::SoundHandle* handle, f32 pan) {
    handle->SetPan(pan);
}

bool IsSoundPlaying(snd::SoundHandle* handle) {
    return handle->IsAttachedSound();
}

// ---------------------------------------------------------------------------
// Voice effect
// ---------------------------------------------------------------------------

// sin(angle) in 12-bit fixed point; 0x1000 is pi.
static inline s32 SinQ12(s32 angle) {
    return 4096.0f * math::SinIdx(angle << 3);
}

// Fills taps (numTaps, odd) with a band-pass FIR between the angular
// frequencies lo and hi (0x1000 = pi): (sin(hi k) - sin(lo k)) / (pi k).
static inline void MakeBandPass(s32* taps, s32 numTaps, s32 lo, s32 hi) {
    s32 half = numTaps / 2;
    taps[half] = hi - lo;
    for (s32 k = 1; k <= half; k++) {
        s32 tap = (SinQ12(hi * k) - SinQ12(lo * k)) * FIX_ONE / (k * 0x3243);
        taps[half - k] = tap;
        taps[half + k] = tap;
    }
}

// Sets count samples of dst to zero.
static inline void Clear(s32* dst, s32 count) {
    for (int i = 0; i < count; i++) {
        dst[i] = 0;
    }
}

// dst = dst * gainDst + src * gainSrc for count samples, with 1.0 = FIX_ONE
static inline void Mix(s32* dst, s32 gainDst, const s32* src, s32 gainSrc, s32 count) {
    for (int i = 0; i < count; i++) {
        dst[i] = (dst[i] * gainDst + src[i] * gainSrc) / FIX_ONE;
    }
}

// The smaller of a and b
static inline s32 Min(s32 a, s32 b) {
    return a <= b ? a : b;
}

// The value at x on the line through (x0, y0) and (x1, y1).
static inline s32 Lerp(s32 x, s32 x0, s32 y0, s32 x1, s32 y1) {
    if (x1 == x0) {
        return y0;
    }
    return y0 + (s32)((s64)(x - x0) * (y1 - y0) / (x1 - x0));
}

// The u16 angle of step i of n steps per turn
static inline u16 StepAngle(s32 i, s32 n) {
    return Lerp(i, 0, 0, n, 0x10000);
}

VoiceEffect::VoiceEffect() {
    mShiftUp = false;
    mEnabled = false;
    mMode = MODE_OFF;
    mLastMode = -1;
    mFrame = 0;

    for (int ch = 0; ch < 2; ch++) {
        Clear(mInput[ch], VOICE_HISTORY_LENGTH);
        Clear(mOutput[ch], VOICE_HISTORY_LENGTH);
        mUnk26DCC[ch] = 0;
        mUnk26DD4[ch] = 0;
    }

    mBusyTicks = 0;

    MakeBandPass(mEchoBand, 11, 0x100, 0xE00);
    MakeBandPass(mRadioBand, 21, 0x100, 0x366);
    MakeBandPass(mShiftLowPass, 11, 0, 0x900);

    // The chorus delay swings between -22 and 22 samples.
    for (int i = 0; i < VOICE_LFO_LENGTH; i++) {
        mChorusLfo[i] = (s32)(4096.0f * math::SinIdx(StepAngle(i, VOICE_LFO_LENGTH))) * 22 / FIX_ONE;
    }

    BuildWindow(mSlowWindow, VOICE_HISTORY_LENGTH, 0);
    BuildWindow(mShiftWindow, VOICE_HISTORY_LENGTH, 0);
}

// Copies count history samples to dst, starting `pos` samples into the call
// `start` calls from now, one call at a time.
void VoiceEffect::Gather(s32 (*history)[VOICE_HISTORY_LENGTH], s32 ch, s32 count, s32* dst, s32 start, s32 pos) {
    s32 frame = start + pos / VOICE_FRAME_LENGTH;
    pos %= VOICE_FRAME_LENGTH;
    if (pos < 0) {
        frame--;
        pos += VOICE_FRAME_LENGTH;
    }

    while (count > 0) {
        s32 n = Min(count, VOICE_FRAME_LENGTH - pos);
        CopyWords(At(history[ch], frame, pos), dst, n);
        dst += n;
        count -= n;
        pos = 0;
        frame++;
    }
}

// dst[i] = sum of taps[k] * src[i - k] for the 96 samples of a call; src
// has numTaps - 1 earlier samples in front of it.
static inline void Filter(s32* dst, const s32* src, const s32* taps, s32 numTaps) {
    for (s32 i = VOICE_FRAME_LENGTH - 1; i >= 0; i--) {
        s32 sum = 0;
        for (s32 k = 0; k < numTaps; k++) {
            sum += taps[k] * src[i - k];
        }
        dst[i] = sum / FIX_ONE;
    }
}

// Mixes in the output of 60 calls ago and keeps a band of it.
void VoiceEffect::Echo(s32** out) {
    for (int ch = 0; ch < 2; ch++) {
        s32 dry[VOICE_FRAME_LENGTH + 11];
        s32 wet[VOICE_FRAME_LENGTH + 11];
        Gather(mInput, ch, VOICE_FRAME_LENGTH + 11, dry, 0, -11);
        Gather(mOutput, ch, VOICE_FRAME_LENGTH + 11, wet, -60, -11);

        Mix(dry, FIX_ONE, wet, 0x999, VOICE_FRAME_LENGTH + 11);

        Filter(out[ch], &dry[11], mEchoBand, 11);
    }
}

// Reads the input with a swinging delay and mixes in an old echo.
void VoiceEffect::Chorus(s32** out) {
    for (int ch = 0; ch < 2; ch++) {
        s32* dst = out[ch];
        s32 start = (mFrame % 30) * VOICE_FRAME_LENGTH;
        for (int i = 0; i < VOICE_FRAME_LENGTH; i++) {
            dst[i] = *At(mInput[ch], -2, i + mChorusLfo[start + i]);
        }

        s32* echo = At(mOutput[ch], -60, 0);
        Mix(dst, 0xCCC, echo, 0x333, VOICE_FRAME_LENGTH);
    }
}

void VoiceEffect::UpdateBuffer(int channels, void** buffers, u32 size, snd::SampleFormat format,
                               f32 sampleRate, snd::OutputMode mode) {
    u32 start = OSGetTick();

    s32* out[2];
    out[0] = static_cast<s32*>(buffers[0]);
    out[1] = static_cast<s32*>(buffers[1]);

    for (int ch = 0; ch < 2; ch++) {
        CopyWords(out[ch], At(mInput[ch], 0, 0), VOICE_FRAME_LENGTH);
    }

    if (mEnabled) {
        switch (mMode) {
        case MODE_ECHO:
            Echo(out);
            break;

        case MODE_HALF_SPEED:
            HalfSpeed(out);
            break;

        case MODE_CHORUS:
            Chorus(out);
            break;

        case MODE_RADIO:
            Radio(out);
            break;
        }
    }

    for (int ch = 0; ch < 2; ch++) {
        CopyWords(out[ch], At(mOutput[ch], 0, 0), VOICE_FRAME_LENGTH);
    }

    if (mShiftUp) {
        ShiftUp(out);
    }

    mFrame++;
    mLastMode = mMode;
    mBusyTicks += OSGetTick() - start;
}

// Plays the input at half speed: two overlapping windows of 64 calls that each
// read the history at half the rate.
void VoiceEffect::HalfSpeed(s32** out) {
    for (int ch = 0; ch < 2; ch++) {
        s32 sum[VOICE_FRAME_LENGTH];
        Clear(sum, VOICE_FRAME_LENGTH);

        for (int w = 0; w < 2; w++) {
            int phase = (mFrame + w * VOICE_HISTORY_FRAMES / 2) % VOICE_HISTORY_FRAMES;
            s32 offset = phase * VOICE_FRAME_LENGTH;
            s32* src = At(mInput[ch], -phase, offset / 2);
            for (int i = 0; i < VOICE_FRAME_LENGTH; i++) {
                s32 value;
                if (i == VOICE_FRAME_LENGTH - 1) {
                    // The last sample's neighbour can lie in the next call.
                    value = *At(mInput[ch], -phase, phase * VOICE_FRAME_LENGTH / 2 + i / 2 + 1);
                    value = (src[i / 2] + value) / 2;
                } else {
                    value = (src[i / 2] + src[i / 2 + 1]) / 2;
                }
                sum[i] += value * mSlowWindow[offset + i] / FIX_ONE;
            }
        }

        s32* dst = out[ch];
        for (int i = 0; i < VOICE_FRAME_LENGTH; i++) {
            dst[i] = sum[i] * 0x1333 / FIX_ONE;
        }
    }
}

// Mixes both channels down, keeps a narrow band, adds crackle and drops the
// low bits, like a small radio.
void VoiceEffect::Radio(s32** out) {
    s32 left[VOICE_FRAME_LENGTH + 21];
    s32 right[VOICE_FRAME_LENGTH + 21];
    s32* mono = &left[21];
    Gather(mInput, 0, VOICE_FRAME_LENGTH + 21, left, 0, -21);
    Gather(mInput, 1, VOICE_FRAME_LENGTH + 21, right, 0, -21);

    Mix(left, FIX_ONE, right, FIX_ONE, VOICE_FRAME_LENGTH + 21);

    Filter(mono, mono, mRadioBand, 21);

    for (int i = 0; i < VOICE_FRAME_LENGTH; i++) {
        if (i % 8 == 0) {
            sNoiseState = sNoiseState * 0x80D + 7;
        }
        s32 crackle = (s64)(sNoiseState & 0xFFF) * 0x19A / FIX_ONE;
        s32 gain = crackle + 0xE66;
        s32 sample = mono[i];
        mono[i] = sample * gain / FIX_ONE;
        // Keep only the top 25 bits.
        ((u32*)mono)[i] &= ~0x7F;
    }

    for (int ch = 0; ch < 2; ch++) {
        CopyWords(mono, out[ch], VOICE_FRAME_LENGTH);
    }
}

static inline BOOL InShiftWindow(s32 i) {
    return i >= 0 && i <= VOICE_HISTORY_LENGTH - 1;
}

// Raises the output by a fifth: two overlapping windows that read the output
// history 1.5 times as fast, then a low-pass.
void VoiceEffect::ShiftUp(s32** out) {
    for (int ch = 0; ch < 2; ch++) {
        s32 sum[VOICE_FRAME_LENGTH + 11];
        Clear(sum, VOICE_FRAME_LENGTH + 11);

        for (int w = 0; w < 2; w++) {
            s32 phase = (mFrame + w * VOICE_HISTORY_FRAMES / 2) % VOICE_HISTORY_FRAMES;
            for (s32 k = -11; k < VOICE_FRAME_LENGTH; k++) {
                s32 n = phase * VOICE_FRAME_LENGTH + k;
                if (InShiftWindow(n)) {
                    s32 fixPos = n * 0x1800;
                    s32 pos = fixPos / FIX_ONE;
                    s32 a = *At(mOutput[ch], -VOICE_HISTORY_FRAMES / 2 - phase, pos);
                    s32 b = *At(mOutput[ch], -VOICE_HISTORY_FRAMES / 2 - phase, pos + 1);
                    s32 value = Lerp(fixPos % FIX_ONE, 0, a, FIX_ONE, b);
                    sum[k + 11] += value * mShiftWindow[n] / FIX_ONE;
                }
            }
        }

        Filter(out[ch], &sum[11], mShiftLowPass, 11);
    }
}

// Fills table with a window of `length` samples (1.0 = 0x1000): a Hamming
// window (shape 0), a Hann window (1) or a triangle (2).
static void BuildWindow(s32* table, s32 length, s32 shape) {
    switch (shape) {
    case 0:
        for (int i = 0; i < length; i++) {
            s32 c = 4096.0f * math::CosIdx(StepAngle(i, length));
            table[i] = -c * 0x75C / FIX_ONE + 0x8A3;
        }
        break;
    case 1:
        for (int i = 0; i < length; i++) {
            s32 c = 4096.0f * math::CosIdx(StepAngle(i, length));
            table[i] = -c / 2 + 0x800;
        }
        break;
    case 2:
        for (int i = 0; i < length; i++) {
            table[i] = i < length / 2 ? Lerp(i, -1, 0, length / 2, FIX_ONE)
                                      : Lerp(i, length / 2, FIX_ONE, length, 0);
        }
        break;
    }
}

// Copies count words; the ranges may overlap.
static void CopyWords(const s32* src, s32* dst, s32 count) {
    s32 offset = (const u8*)src - (u8*)dst;
    if (offset == 0) {
        return;
    }

    if (offset > 0) {
        for (int i = 0; i < count; i++) {
            dst[i] = src[i];
        }
    } else {
        for (int i = count - 1; i >= 0; i--) {
            dst[i] = src[i];
        }
    }
}
