/*
**
** Copyright 2008, The Android Open Source Project
** Copyright (c) 2010, Code Aurora Forum. All rights reserved.
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/

#ifndef ANDROID_AUDIO_LPADECODE_H
#define ANDROID_AUDIO_LPADECODE_H

#include <utils/Errors.h>

#include "android_audio_mio.h"
#include "utils/Timers.h"

#include <linux/msm_audio.h>
#include <media/IAudioFlinger.h>
#include <binder/IServiceManager.h>
#include <utils/List.h>

#ifndef OSCL_TIMER_H_INCLUDED
#include "oscl_timer.h"
#endif

using namespace android;

class AndroidAudioLPADecode : public AndroidAudioMIO,
    public OsclTimerObserver
{
public:
    OSCL_IMPORT_REF AndroidAudioLPADecode();
    OSCL_IMPORT_REF ~AndroidAudioLPADecode();

    virtual PVMFCommandId QueryUUID(const PvmfMimeString& aMimeType, Oscl_Vector<PVUuid,
                                    OsclMemAllocator>& aUuids, bool aExactUuidsOnly=false,
                                    const OsclAny* aContext=NULL);

    virtual PVMFCommandId QueryInterface(const PVUuid& aUuid, PVInterface*& aInterfacePtr,
                                         const OsclAny* aContext=NULL);

    virtual void setParametersSync(PvmiMIOSession aSession, PvmiKvp* aParameters,
                                   int num_elements, PvmiKvp * & aRet_kvp);

    virtual PVMFCommandId DiscardData(PVMFTimestamp aTimestamp=0, const OsclAny* aContext=NULL);

    virtual void cancelCommand(PVMFCommandId aCmdId);
    virtual PVMFCommandId Pause(const OsclAny* aContext=NULL);
    virtual PVMFCommandId Start(const OsclAny* aContext=NULL);
    virtual PVMFCommandId Stop(const OsclAny* aContext=NULL);
    virtual PVMFCommandId Reset(const OsclAny* aContext=NULL);

/* Result of constructing the AudioTrack. This must be checked
 * before using any AudioTrack API (except for set()), using
 * an uninitialized AudioTrack produces undefined results.
 * See set() method above for possible return codes.
*/
    virtual int initCheck();

// helper function to obtain AudioFlinger service handle
    const sp<IAudioFlinger>& get_audio_flinger();

private:

// Audio request handling
// this mio queues the requests, the audio thread dequeues and processes them
    class OSSRequest {
    public:
        OSSRequest(uint8* data, uint32 len, PVMFCommandId id, const OsclAny* ctx, const PVMFTimestamp& ts, int32 pmem_fd) :
        iData(data), iDataLen(len), iCmdId(id), iContext(ctx), iTimestamp(ts), pmemfd(pmem_fd)
        {}

        uint8* iData;
        uint32 iDataLen;
        PVMFCommandId iCmdId;
        const OsclAny* iContext;
        PVMFTimestamp iTimestamp;
        int32 pmemfd;
    };

    virtual void writeAudioLPABuffer(uint8* aData, uint32 aDataLen, PVMFCommandId cmdId,
                                     OsclAny* aContext, PVMFTimestamp aTimestamp, int32 pmem_fd);

    virtual void Run();

    virtual void returnAllBuffers();

    void RequestAndWaitForThreadExit();

    //Function prototypes and Semaphores for pcm_dec driver event thread. this thread will wait for Write_dones from pcm_dec driver;
    void RequestAndWaitForEventThreadExit();
    void RequestAndWaitForA2DPThreadExit(); // A2DP

    void HandleA2DPswitch();

    // From OsclTimerObserver
    void TimeoutOccurred(int32 timerID, int32 timeoutInfo);

    // OsclTimer for timeouts
    OsclTimer<OsclMemAllocator>* iTimeoutTimer;

    static int start_audout_thread_func(TOsclThreadFuncArg arg);
    int audout_thread_func();

    // semaphores used to communicate with the audio thread
    OsclSemaphore* iAudioThreadSem;
    OsclSemaphore* iAudioThreadTermSem;
    OsclSemaphore* iAudioThreadCreatedSem;

    volatile bool iExitAudioThread;
    volatile bool iReturnBuffers;

    static int start_event_thread_func(TOsclThreadFuncArg arg);
    int event_thread_func();

    // semaphores used to communicate with the audio Tunnel thread
    OsclSemaphore* iEventThreadSem;
    OsclSemaphore* iEventThreadTermSem;

    bool iEventThreadCreated;
    volatile bool iEventExitThread;

    static int start_a2dp_thread_func(TOsclThreadFuncArg arg);
    int a2dp_thread_func();

    // semaphores used to communicate with the audio thread
    OsclSemaphore* iA2DPThreadSem;
    OsclSemaphore* iA2DPThreadTermSem;
    OsclSemaphore* iA2DPThreadReturnSem;
    OsclSemaphore* iA2DPThreadCreatedSem;
    bool iA2DPThreadCreatedAndMIOConfigured;

    volatile bool iExitA2DPThread;

    // active timing
    AndroidAudioMIOActiveTimingSupport* iActiveTiming;

    // oss request queue, needs to use lock mechanism to access
    Oscl_Vector<OSSRequest,OsclMemAllocator> iOSSRequestQueue, iOSSResponseQueue, iOSSBufferSwapQueue;

    // lock used to access the oss request and response queue
    OsclMutex iOSSRequestQueueLock,iOSSResponseQueueLock, iOSSBufferSwapQueueLock, iDeviceSwitchLock ;

    // number of bytes in an input frame
    int iInputFrameSizeInBytes;

    // EOS reached
    bool bEOS;

    // PCM decoder fd
    int afd;

    // This class will hold the list of PMEM buffers for reg / de-reg from driver.
    class PmemInfo {
    public:
        PmemInfo(int32 fd, OsclAny *buf) :
        iPMem_fd(fd), iPMem_buf(buf)
        {}
        int32 iPMem_fd;
        OsclAny* iPMem_buf;
    };

    Oscl_Vector<PmemInfo,OsclMemAllocator> iPmemInfoQueue;
    OsclMutex iPmemInfoQueueLock;

    // Suspend / Resume handling
    bool   bSuspendEventRxed; // Suspend received from driver.

    // Wallclock time in nano secs to find the interval between write calls done to device.
    nsecs_t     iClockTimeOfWriting_ns;

    // Session ID for the decoder
    int   sessionId;

    // Number of Bytes actually consumed
    uint64 nBytesConsumed;

    // Number of Bytes written
    uint64 nBytesWritten;

    // Hardware State
    enum HWState {
        STATE_HW_INITIALIZED,
        STATE_HW_STARTED,
        STATE_HW_PAUSED,
        STATE_HW_STOPPED
    };
    HWState iHwState;

    volatile bool bIsA2DPEnabled, bIsAudioRouted; // bIsAudioRouted is used for opensession / closesession use cases.

    class AudioFlingerLPAdecodeClient: public IBinder::DeathRecipient, public BnAudioFlingerClient {
    public:
        AudioFlingerLPAdecodeClient(void *obj);

        AndroidAudioLPADecode *pBaseClass;
        // DeathRecipient
        virtual void binderDied(const wp<IBinder>& who);

        // IAudioFlingerClient

        // indicate a change in the configuration of an output or input: keeps the cached
        // values for output/input parameters upto date in client process
        virtual void ioConfigChanged(int event, int ioHandle, void *param2);

        friend class AndroidAudioLPADecode;
    };

    sp<AudioFlingerLPAdecodeClient> AudioFlingerClient;
    friend class AudioFlingerLPAdecodeClient;
    Mutex AudioFlingerLock;
    sp<IAudioFlinger> AudioFlinger;
};

#endif // ANDROID_AUDIO_LPADECODE_H

