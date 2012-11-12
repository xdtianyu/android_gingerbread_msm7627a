/*
 * Copyright (c) 2008, The Android Open Source Project
 * Copyright (c) 2010, Code Aurora Forum. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ANDROID_AUDIO_INPUTFMA2DP_H_INCLUDED
#define ANDROID_AUDIO_INPUTFMA2DP_H_INCLUDED

#ifndef OSCL_BASE_H_INCLUDED
#include "oscl_base.h"
#endif
#ifndef OSCLCONFIG_IO_H_INCLUDED
#include "osclconfig_io.h"
#endif
#ifndef OSCL_STRING_H_INCLUDED
#include "oscl_string.h"
#endif
#ifndef OSCL_FILE_IO_H_INCLUDED
#include "oscl_file_io.h"
#endif
#ifndef OSCL_MEM_MEMPOOL_H_INCLUDED
#include "oscl_mem_mempool.h"
#endif
#ifndef OSCL_SCHEDULER_AO_H_INCLUDED
#include "oscl_scheduler_ao.h"
#endif
#ifndef OSCL_VECTOR_H_INCLUDED
#include "oscl_vector.h"
#endif
#ifndef PVMI_MIO_CONTROL_H_INCLUDED
#include "pvmi_mio_control.h"
#endif
#ifndef PVMI_MEDIA_TRANSFER_H_INCLUDED
#include "pvmi_media_transfer.h"
#endif
#ifndef PVMI_CONFIG_AND_CAPABILITY_H_INCLUDED
#include "pvmi_config_and_capability.h"
#endif
#ifndef PVMF_SIMPLE_MEDIA_BUFFER_H_INCLUDED
#include "pvmf_simple_media_buffer.h"
#endif
#include "android_audio_input.h"

#include <utils/RefBase.h>

#ifdef HIDE_MIO_SYMBOLS
#pragma GCC visibility push(hidden)
#endif

#include <media/IAudioFlinger.h>
#include <binder/IServiceManager.h>
#include <media/MediaPlayerInterface.h>
#include <media/AudioTrack.h>

namespace android {


class AndroidAudioInputA2DP;
class Mutex;
class Condition;

class AndroidAudioInputA2DP : public OsclTimerObject,
    public PvmiMIOControl,
    public PvmiCapabilityAndConfig,
    public RefBase,
    public PVMFMediaClockStateObserver
{
public:
    AndroidAudioInputA2DP(uint32 audioSource);
    virtual ~AndroidAudioInputA2DP();

    // Pure virtuals from PvmiMIOControl
    PVMFStatus connect(PvmiMIOSession& aSession, PvmiMIOObserver* aObserver);
    PVMFStatus disconnect(PvmiMIOSession aSession);
    PvmiMediaTransfer* createMediaTransfer(PvmiMIOSession& aSession,
                                                         PvmiKvp* read_formats=NULL,
                                                         int32 read_flags=0,
                                                         PvmiKvp* write_formats=NULL,
                                                         int32 write_flags=0);
    void deleteMediaTransfer(PvmiMIOSession& aSession,
                                           PvmiMediaTransfer* media_transfer);
    PVMFCommandId QueryUUID(const PvmfMimeString& aMimeType,
                                          Oscl_Vector<PVUuid, OsclMemAllocator>& aUuids,
                                          bool aExactUuidsOnly=false,
                                          const OsclAny* aContext=NULL);
    PVMFCommandId QueryInterface(const PVUuid& aUuid,
                                               PVInterface*& aInterfacePtr,
                                               const OsclAny* aContext=NULL);
    PVMFCommandId Init(const OsclAny* aContext=NULL);
    PVMFCommandId Start(const OsclAny* aContext=NULL);
    PVMFCommandId Reset(const OsclAny* aContext=NULL);
    PVMFCommandId Pause(const OsclAny* aContext=NULL);
    PVMFCommandId Flush(const OsclAny* aContext=NULL);
    PVMFCommandId DiscardData(PVMFTimestamp aTimestamp, const OsclAny* aContext=NULL);
    PVMFCommandId DiscardData(const OsclAny* aContext=NULL);
    PVMFCommandId Stop(const OsclAny* aContext=NULL);
    PVMFCommandId CancelCommand(PVMFCommandId aCmdId, const OsclAny* aContext=NULL);
    PVMFCommandId CancelAllCommands(const OsclAny* aContext=NULL);
    void ThreadLogon();
    void ThreadLogoff();
    // Pure virtuals from PvmiCapabilityAndConfig
    void setObserver (PvmiConfigAndCapabilityCmdObserver* aObserver);
    PVMFStatus getParametersSync(PvmiMIOSession aSession, PvmiKeyType aIdentifier,
                                               PvmiKvp*& aParameters, int& num_parameter_elements,
                                               PvmiCapabilityContext aContext);
    PVMFStatus releaseParameters(PvmiMIOSession aSession, PvmiKvp* aParameters,
                                               int num_elements);
    void createContext(PvmiMIOSession aSession, PvmiCapabilityContext& aContext);
    void setContextParameters(PvmiMIOSession aSession, PvmiCapabilityContext& aContext,
                                            PvmiKvp* aParameters, int num_parameter_elements);
    void DeleteContext(PvmiMIOSession aSession, PvmiCapabilityContext& aContext);
    void setParametersSync(PvmiMIOSession aSession, PvmiKvp* aParameters,
                                         int num_elements, PvmiKvp * & aRet_kvp);
    PVMFCommandId setParametersAsync(PvmiMIOSession aSession, PvmiKvp* aParameters,
                                                   int num_elements, PvmiKvp*& aRet_kvp,
                                                   OsclAny* context=NULL);
    uint32 getCapabilityMetric (PvmiMIOSession aSession);
    PVMFStatus verifyParametersSync (PvmiMIOSession aSession,
                                                   PvmiKvp* aParameters, int num_elements);

    int allocateBufferPool();
    // Android-specific stuff
    /* Sets the input sampling rate */
    bool setAudioSamplingRate(int32 iSamplingRate);

    /* Set the input number of channels */
    bool setAudioNumChannels(int32 iNumChannels);

    /* Sets the audio input source */
    bool setAudioSource(uint32 iSource);

    /* From PVMFMediaClockStateObserver and its base*/
    void ClockStateUpdated();
    void NotificationsInterfaceDestroyed();

    // helper function to obtain AudioFlinger service handle
    const sp<IAudioFlinger>& get_audio_flinger();


private:

    //Audio output request handling
    // this mio queues the requests, the audio output thread dequeues and processes them
    class OSSRequest
    {
    public:
        OSSRequest(uint8* data, uint32 len,int32 ibis,int32 ibs,const OsclAny* ctx)
            :iData(data),iDataLen(len),iA2DPinputstatus(ibis),iBufferstatus(ibs),iContext(ctx)
        {}
        uint8* iData;
        uint32 iDataLen;
        int32 iA2DPinputstatus;
        int32 iBufferstatus;        //to use with enum to check current queued buffer state
        const OsclAny* iContext;
    };

    AndroidAudioInputA2DP();
    void Run();

    int audin_thread_func();
    static int start_audin_thread_func(TOsclThreadFuncArg arg);

    int auda2dp_thread_func();
    static int start_auda2dp_thread_func(TOsclThreadFuncArg arg);
    PVMFCommandId AddCmdToQueue(AndroidAudioInputCmdType aType, const OsclAny* aContext, OsclAny* aData1 = NULL);
    void AddDataEventToQueue(uint32 aMicroSecondsToEvent);
    void DoRequestCompleted(const AndroidAudioInputCmd& aCmd, PVMFStatus aStatus, OsclAny* aEventData=NULL);
    PVMFStatus DoInit();
    PVMFStatus DoStart();
    PVMFStatus DoReset();
    PVMFStatus DoStop();
    PVMFStatus AllocateKvp(PvmiKvp*& aKvp, PvmiKeyType aKey, int32 aNumParams);

    /**
     * Verify one key-value pair parameter against capability of the port and
     * if the aSetParam flag is set, set the value of the parameter corresponding to
     * the key.
     *
     * @param aKvp Key-value pair parameter to be verified
     * @param aSetParam If true, set the value of parameter corresponding to the key.
     * @return PVMFSuccess if parameter is supported, else PVMFFailure
     */
    PVMFStatus VerifyAndSetParameter(PvmiKvp* aKvp, bool aSetParam=false);

    void RemoveDestroyClockStateObs();

    // Command queue
    uint32 iCmdIdCounter;
    Oscl_Vector<AndroidAudioInputCmd, OsclMemAllocator> iCmdQueue;

    // PvmiMIO sessions
    Oscl_Vector<PvmiMIOObserver*, OsclMemAllocator> iObservers;

    PvmiMediaTransfer* iPeer;

    // Thread logon
    bool iThreadLoggedOn;

    // semaphore used to communicate with the audio input thread
    OsclSemaphore* iAudioThreadSem;
    // and another one
    OsclSemaphore* iAudioThreadTermSem;

    //semaphore used to communicate with BT thread
    OsclSemaphore* iAudioA2DPThreadSem;
    OsclSemaphore* iAudioA2DPThreadTermSem;

    volatile bool iExitAudioThread;
    volatile bool iExitA2DPThread;

    PvmiMIOObserver* iObserver;

    //Control command handling.
    class CommandResponse
    {
    public:
        CommandResponse(PVMFStatus s,PVMFCommandId id,const OsclAny* ctx)
            :iStatus(s),iCmdId(id),iContext(ctx)
        {}

        PVMFStatus iStatus;
        PVMFCommandId iCmdId;
        const OsclAny* iContext;
    };
    Oscl_Vector<CommandResponse,OsclMemAllocator> iCommandResponseQueue;

    //Audio parameters.
    PVMFFormatType iAudioFormat;
    int32 iAudioNumChannels;
    int32 iAudioSamplingRate;
    uint32 iAudioSource;
    int32 iFrameSize;

    // Functions specific to this MIO

    //request active object which the audio output thread uses to schedule this timer object to run
    AndroidAudioInputThreadSafeCallbackAO *iWriteCompleteAO;
    Oscl_Vector<OSSRequest,OsclMemAllocator>iOSSBTResponseQueue, iOSSBTRequestQueue;

    //Locks for new request and response queue
    OsclMutex iOSSBTResponseQueueLock,iOSSBTRequestQueueLock;
    // Allocator for simple media data buffer
    OsclMemAllocator iAlloc;
    OsclMemPoolFixedChunkAllocator* iMediaBufferMemPool;

    // State machine
    enum AndroidAudioInputA2DPState
    {
        STATE_IDLE,
        STATE_INITIALIZED,
        STATE_STARTED,
        STATE_FLUSHING,
        STATE_PAUSED,
        STATE_STOPPED
    };

    AndroidAudioInputA2DPState iState;

    // synchronize startup of audio input thread, so we can return an error
    // from DoStart() if something goes wrong
    Mutex *iAudioThreadStartLock;
    Condition *iAudioThreadStartCV;
    volatile status_t iAudioThreadStartResult;
    volatile bool iAudioThreadStarted;

    PVMFMediaClock *iAuthorClock;
    PVMFMediaClockNotificationsInterface *iClockNotificationsInf;
    // This stores the Start cmd when Audio MIO is waiting for
    // first audio frame to be received from the device.
    AndroidAudioInputCmd iStartCmd;
    // Variable to track AudioSource type.
    int iAudioFormatType;
    bool iBufferForceWrite;

    // Audio input thread
    OsclThread AudioInput_Thread;
    OsclThread AudioA2DP_Thread;


    volatile bool bIsA2DPEnabled;

    class AudioFlingerAudioInputA2DPClient: public IBinder::DeathRecipient, public BnAudioFlingerClient {
    public:
        AudioFlingerAudioInputA2DPClient(void *obj);

        AndroidAudioInputA2DP *pBaseClass;
        // DeathRecipient
        virtual void binderDied(const wp<IBinder>& who);

        // IAudioFlingerClient

        // indicate a change in the configuration of an output or input: keeps the cached
        // values for output/input parameters upto date in client process
        virtual void ioConfigChanged(int event, int ioHandle, void *param2);

        friend class AndroidAudioInputA2DP;
    };

    sp<AudioFlingerAudioInputA2DPClient> AudioFlingerClient;
    friend class AudioFlingerAudioInputA2DPClient;
    Mutex AudioFlingerLock;
    sp<IAudioFlinger> AudioFlinger;


public:
    bool setAudioFormatType(char *iAudioFormat);
    AudioTrack *mpAudioTrack;  // Pointer to audio track used for playback

};

}; // namespace android

#ifdef HIDE_MIO_SYMBOLS
#pragma GCC visibility pop
#endif

#endif // ANDROID_AUDIO_INPUTFMA2DP_H_INCLUDED
