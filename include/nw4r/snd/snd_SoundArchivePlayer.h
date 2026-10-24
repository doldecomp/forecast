#ifndef NW4R_SND_SOUND_ARCHIVE_PLAYER_H
#define NW4R_SND_SOUND_ARCHIVE_PLAYER_H
#include "nw4r/snd/snd_SoundArchiveLoader.h"
#include <nw4r/types_nw4r.h>

#include <nw4r/snd/snd_DisposeCallback.h>
#include <nw4r/snd/snd_MmlParser.h>
#include <nw4r/snd/snd_MmlSeqTrackAllocator.h>
#include <nw4r/snd/snd_NoteOnCallback.h>
#include <nw4r/snd/snd_SeqSound.h>
#include <nw4r/snd/snd_SoundArchive.h>
#include <nw4r/snd/snd_SoundInstanceManager.h>
#include <nw4r/snd/snd_SoundStartable.h>
#include <nw4r/snd/snd_StrmChannel.h>
#include <nw4r/snd/snd_StrmSound.h>
#include <nw4r/snd/snd_Util.h>
#include <nw4r/snd/snd_WaveSound.h>
#include <nw4r/snd/snd_WsdPlayer.h>

namespace nw4r {
namespace snd {

// Forward declarations
class SoundMemoryAllocatable;
class SoundPlayer;

namespace detail {
class SeqTrackAllocator;
} // namespace detail

/******************************************************************************
 *
 * SoundArchivePlayer_FileManager
 *
 ******************************************************************************/
class SoundArchivePlayer_FileManager {
public:
    virtual const void* GetFileAddress(u32 id) = 0;         // at 0x8
    virtual const void* GetFileWaveDataAddress(u32 id) = 0; // at 0x8
};

/******************************************************************************
 *
 * SoundArchivePlayer
 *
 ******************************************************************************/
class SoundArchivePlayer : public detail::DisposeCallback,
                           public SoundStartable {
public:
    SoundArchivePlayer();
    virtual ~SoundArchivePlayer(); // at 0x8

    virtual void InvalidateData(const void* pStart,
                                const void* pEnd); // at 0xC

    virtual void InvalidateWaveData(const void* pStart,
                                    const void* pEnd); // at 0x10

    virtual StartResult
    detail_SetupSound(SoundHandle* pHandle, u32 id,
                      detail::BasicSound::AmbientArgInfo* pArgInfo,
                      detail::ExternalSoundPlayer* pPlayer, bool hold,
                      const StartInfo* pStartInfo); // at 0x28

    virtual u32 detail_ConvertLabelStringToSoundId(const char* pLabel) {
        return mSoundArchive->ConvertLabelStringToSoundId(pLabel);
    } // at 0x2C

    bool IsAvailable() const;

    bool Setup(const SoundArchive* pArchive, void* pMramBuffer,
               u32 mramBufferSize, void* pStrmBuffer, u32 strmBufferSize);

    void Shutdown();

    u32 GetRequiredMemSize(const SoundArchive* pArchive);
    u32 GetRequiredStrmBufferSize(const SoundArchive* pArchive);

    void Update();

    const SoundArchive& GetSoundArchive() const;

    SoundPlayer& GetSoundPlayer(u32 idx);
    SoundPlayer& GetSoundPlayer(int idx) {
        return GetSoundPlayer(static_cast<u32>(idx));
    }

    const void* detail_GetFileAddress(u32 id) const;
    const void* detail_GetFileWaveDataAddress(u32 id) const;

    const void* GetGroupAddress(u32 id) const;
    void SetGroupAddress(u32 id, const void* pAddr);

    const void* GetGroupWaveDataAddress(u32 id) const;
    void SetGroupWaveDataAddress(u32 id, const void* pAddr);

    bool LoadGroup(u32 id, SoundMemoryAllocatable* pAllocatable, u32 blockSize);
    bool LoadGroup(const char* pLabel, SoundMemoryAllocatable* pAllocatable,
                   u32 blockSize);

    bool LoadGroup(int id, SoundMemoryAllocatable* pAllocatable,
                   u32 blockSize) {
        return LoadGroup(static_cast<u32>(id), pAllocatable, blockSize);
    }
    bool LoadGroup(unsigned int id, SoundMemoryAllocatable* pAllocatable,
                   u32 blockSize) {
        return LoadGroup(static_cast<u32>(id), pAllocatable, blockSize);
    }

    u32 GetSoundPlayerCount() const {
        return mSoundPlayerCount;
    }

    u32 GetFreeSeqSoundCount() const {
        return mSeqSoundInstanceManager.GetFreeCount();
    }
    u32 GetFreeStrmSoundCount() const {
        return mStrmSoundInstanceManager.GetFreeCount();
    }
    u32 GetFreeWaveSoundCount() const {
        return mWaveSoundInstanceManager.GetFreeCount();
    }

private:
    struct Group {
        const void* address;         // at 0x0
        const void* waveDataAddress; // at 0x4
    };

    typedef detail::Util::Table<Group> GroupTable;

    /******************************************************************************
     * SeqLoader
     ******************************************************************************/
    class SeqLoader : public detail::SeqSound::SeqLoader {
    public:
        explicit SeqLoader(SoundArchivePlayer& rPlayer)
            : mSoundArchivePlayer(rPlayer) {
            OSInitMutex(&mMutex);
        }

        virtual int LoadAsync(detail::SeqSound::SeqLoadCallback pCallback,
                              void* pCallbackArg, u32 userData); // at 0xC
        virtual void Cancel(u32 userData);                       // at 0x10

    private:
        SoundArchivePlayer& mSoundArchivePlayer; // at 0x4
        OSMutex mMutex;                          // at 0x8
    };

    /******************************************************************************
     * SeqNoteOnCallback
     ******************************************************************************/
    class SeqNoteOnCallback : public detail::NoteOnCallback {
    public:
        explicit SeqNoteOnCallback(const SoundArchivePlayer& rPlayer)
            : mSoundArchivePlayer(rPlayer) {}

        virtual detail::Channel*
        NoteOn(detail::SeqPlayer* pPlayer, int bankNo,
               const detail::NoteOnInfo& rInfo); // at 0xC

    private:
        const SoundArchivePlayer& mSoundArchivePlayer; // at 0x4
    };

    /******************************************************************************
     * StrmLoader
     ******************************************************************************/
    class StrmLoader : public detail::StrmPlayer::StrmLoader {
    public:
        explicit StrmLoader(SoundArchivePlayer& rPlayer)
            : mSoundArchivePlayer(rPlayer) {
            OSInitMutex(&mMutex);
        }

        virtual detail::StrmPlayer::LoadResult
        LoadHeader(HeaderCallback pCallback, void* pCallbackArg,
                   detail::StrmPlayer* pPlayer, u32 userData); // at 0xC

        virtual detail::StrmPlayer::LoadResult
        LoadData(void* pBuffer, u32 size, s32 offset, int channels,
                 u32 blockSize, u32 blockHeaderOffset, bool needUpdateAdpcmLoop,
                 detail::StrmPlayer::StrmDataLoadCallback* pCallback,
                 detail::StrmPlayer* pPlayer, u32 userData); // at 0x10

        virtual void Cancel(detail::StrmPlayer* pPlayer,
                            u32 userData); // at 0x14

    private:
        SoundArchivePlayer& mSoundArchivePlayer; // at 0x4
        OSMutex mMutex;                          // at 0x8
    };

    /******************************************************************************
     * WsdCallback
     ******************************************************************************/
    class WsdCallback : public detail::WsdPlayer::WsdCallback {
    public:
        explicit WsdCallback(const SoundArchivePlayer& rPlayer)
            : mSoundArchivePlayer(rPlayer) {}

        virtual bool GetWaveSoundData(detail::WaveSoundInfo* pSoundInfo,
                                      detail::WaveSoundNoteInfo* pNoteInfo,
                                      detail::WaveData* pWaveData,
                                      const void* pWsdData, int index,
                                      int noteIndex,
                                      u32 callbackArg) const; // at 0xC

    private:
        const SoundArchivePlayer& mSoundArchivePlayer; // at 0x4
    };

    /******************************************************************************
     * SeqLoadTask
     ******************************************************************************/
    class SeqLoadTask : public detail::Task {
    public:
        SeqLoadTask(u32 taskId, const SoundArchive* pArchive, u32 fileId,
                    s32 dataOffset, SoundMemoryAllocatable* pAllocatable,
                    detail::SeqSound::SeqLoadCallback pCallback,
                    void* pCallbackArg, OSMutex* pMutex)
            : detail::Task(taskId),
              mSoundArchive(pArchive),
              mFileId(fileId),
              mDataOffset(dataOffset),
              mAllocatable(pAllocatable),
              mCallback(pCallback),
              mCallbackArg(pCallbackArg),
              mMutex(pMutex) {}

        virtual void Execute(); // at 0xC
        virtual void Cancel();  // at 0x10

    private:
        void SetLoader(detail::SoundArchiveLoader* pLoader) {
            ut::detail::AutoLock<OSMutex> lock(*mMutex);
            mLoader = pLoader;
        }

        void Notify(bool success, const void* pBase, s32 offset) {
            if (mCallback != NULL) {
                mCallback(success, pBase, offset, mCallbackArg);
            }
        }

    private:
        detail::SoundArchiveLoader* mLoader;         // at 0x10
        const SoundArchive* mSoundArchive;           // at 0x14
        u32 mFileId;                                 // at 0x18
        s32 mDataOffset;                             // at 0x1C
        SoundMemoryAllocatable* mAllocatable;        // at 0x20
        detail::SeqSound::SeqLoadCallback mCallback; // at 0x24
        void* mCallbackArg;                          // at 0x28
        OSMutex* mMutex;                             // at 0x2C
    };

    /******************************************************************************
     * StrmHeaderLoadTask
     ******************************************************************************/
    class StrmHeaderLoadTask : public detail::Task {
    public:
        typedef detail::StrmPlayer::StrmLoader::HeaderCallback HeaderCallback;

    public:
        StrmHeaderLoadTask(u32 taskId, const SoundArchive* pArchive, u32 fileId,
                           HeaderCallback pCallback, void* pCallbackArg,
                           OSMutex* pMutex)
            : detail::Task(taskId),
              mFileStream(NULL),
              mSoundArchive(pArchive),
              mFileId(fileId),
              mCallback(pCallback),
              mCallbackArg(pCallbackArg),
              mMutex(pMutex) {}

        virtual void Execute(); // at 0xC
        virtual void Cancel();  // at 0x10

    private:
        void Notify(bool success, const detail::StrmPlayer::StrmHeader* pHeader) {
            if (mCallback != NULL) {
                mCallback(success, pHeader, mCallbackArg);
            }
        }

    private:
        ut::FileStream* mFileStream;       // at 0x10
        const SoundArchive* mSoundArchive; // at 0x14
        u32 mFileId;                       // at 0x18
        HeaderCallback mCallback;          // at 0x1C
        void* mCallbackArg;                // at 0x20
        OSMutex* mMutex;                   // at 0x24
    };

    /******************************************************************************
     * StrmDataLoadTask
     ******************************************************************************/
    class StrmDataLoadTask : public detail::Task {
    public:
        StrmDataLoadTask(void* pBuffer, u32 size, s32 offset, int channels,
                         u32 blockSize, u32 blockHeaderOffset,
                         bool needUpdateAdpcmLoop,
                         detail::StrmPlayer::StrmDataLoadCallback* pCallback,
                         const SoundArchive* pArchive, u32 fileId, u32 taskId,
                         OSMutex* pMutex) DECOMP_DONT_INLINE;

        virtual void Execute(); // at 0xC
        virtual void Cancel();  // at 0x10

    private:
        void Notify(bool success) {
            if (mCallback != NULL) {
                mCallback->OnLoad(success);
            }
        }

    private:
        detail::StrmPlayer::StrmDataLoadCallback* mCallback; // at 0x10
        ut::FileStream* mFileStream;                         // at 0x14
        const SoundArchive* mSoundArchive;                   // at 0x18
        u32 mFileId;                                         // at 0x1C
        void* mBuffer;                                       // at 0x20
        u32 mSize;                                           // at 0x24
        s32 mOffset;                                         // at 0x28
        int mChannels;                                       // at 0x2C
        u32 mBlockSize;                                      // at 0x30
        u32 mBlockHeaderOffset;                              // at 0x34
        bool mNeedUpdateAdpcmLoop;                           // at 0x38
        OSMutex* mMutex;                                     // at 0x3C
    };

private:
    static const int FILE_STREAM_BUFFER_SIZE = 512;

private:
    bool SetupMram(const SoundArchive* pArchive, void* pBuffer, u32 bufferSize);

    detail::PlayerHeap* CreatePlayerHeap(void* pBuffer, u32 bufferSize);

    bool SetupSoundPlayer(const SoundArchive* pArchive, void** ppBuffer,
                          void* pEnd);

    bool CreateGroupAddressTable(const SoundArchive* pArchive, void** ppBuffer,
                                 void* pEnd);

    bool SetupSeqSound(const SoundArchive* pArchive, int sounds,
                       void** ppBuffer, void* pEnd);
    bool SetupWaveSound(const SoundArchive* pArchive, int sounds,
                        void** ppBuffer, void* pEnd);
    bool SetupStrmSound(const SoundArchive* pArchive, int sounds,
                        void** ppBuffer, void* pEnd);
    bool SetupSeqTrack(const SoundArchive* pArchive, int tracks,
                       void** ppBuffer, void* pEnd);
    bool SetupStrmBuffer(const SoundArchive* pArchive, void* pBuffer,
                         u32 bufferSize);

    StartResult PrepareSeqImpl(detail::SeqSound* pSound,
                               const SoundArchive::SoundInfo* pSndInfo,
                               const SoundArchive::SeqSoundInfo* pSeqInfo,
                               int voices);

    inline StartResult
    PrepareStrmImpl(detail::StrmSound* pSound,
                    const SoundArchive::SoundInfo* pSndInfo,
                    const SoundArchive::StrmSoundInfo* pStrmInfo,
                    SoundStartable::StartInfo::StartOffsetType startType,
                    int startOffset, int voices);

    inline StartResult
    PrepareWaveSoundImpl(detail::WaveSound* pSound,
                         const SoundArchive::SoundInfo* pSndInfo,
                         const SoundArchive::WaveSoundInfo* pWsdInfo,
                         int voices);

private:
    const SoundArchive* mSoundArchive; // at 0x10
    GroupTable* mGroupTable;           // at 0x14

    SeqLoader mSeqLoader;           // at 0x18
    SeqNoteOnCallback mSeqCallback; // at 0x38
    WsdCallback mWsdCallback;       // at 0x40
    StrmLoader mStrmLoader;         // at 0x48

    detail::SeqTrackAllocator* mSeqTrackAllocator; // at 0x68
    SoundArchivePlayer_FileManager* mFileManager;  // at 0x6C

    u32 mSoundPlayerCount;      // at 0x70
    SoundPlayer* mSoundPlayers; // at 0x74

    detail::SoundInstanceManager<detail::SeqSound>
        mSeqSoundInstanceManager; // at 0x78

    detail::SoundInstanceManager<detail::StrmSound>
        mStrmSoundInstanceManager; // at 0x88

    detail::SoundInstanceManager<detail::WaveSound>
        mWaveSoundInstanceManager; // at 0x98

    detail::StrmBufferPool mStrmBufferPool;             // at 0xA8
    detail::MmlParser mMmlParser;                       // at 0xC0
    detail::MmlSeqTrackAllocator mMmlSeqTrackAllocator; // at 0xC4

    void* mSetupBufferAddress; // at 0xD0
    u32 mSetupBufferSize;      // at 0xD4
};

} // namespace snd
} // namespace nw4r

#endif
