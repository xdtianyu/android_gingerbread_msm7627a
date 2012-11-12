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

//#define LOG_NDEBUG 0
#define LOG_TAG "AudioLPADecode"
#include <utils/Log.h>
#include "android_audio_lpadecode.h"

#include <sys/prctl.h>
#include <sys/resource.h>
#include <utils/threads.h>
#include <media/AudioTrack.h>
#include <linux/time.h>
#include <linux/unistd.h>

#include <errno.h>

#include <sys/ioctl.h>

using namespace android;

// TODO: dynamic buffer count based on sample rate and # channels
static const int kNumOutputBuffers = 4;

// maximum allowed clock drift before correction
static const int32 kMaxClockDriftInMsecs = 25;    // should be tight enough for reasonable sync
static const int32 kMaxClockCorrection = 100;     // maximum clock correction per update

#define ANDROID_AUDIO_LPADEC_TIMERID 1
#define ANDROID_AUDIO_LPADEC_SUSPEND_TIMEOUT 5

/*
/ Audio LPA decode MIO component
/
/ Implementation for LPA decode.
/ the buffers have been successfully written, they are returned through
/ another message queue to the MIO and from there back to the engine.
/ This separation is necessary because most of the PV API is not
/ thread-safe.
*/
OSCL_EXPORT_REF AndroidAudioLPADecode::AndroidAudioLPADecode() :
AndroidAudioMIO("AndroidAudioLPADecode"),
iExitAudioThread(false),
iReturnBuffers(false),
iA2DPThreadCreatedAndMIOConfigured(false),
bEOS(false),
iExitA2DPThread(false),
iActiveTiming(NULL),
iEventThreadCreated(false),
bSuspendEventRxed(false),
iEventExitThread(false),
sessionId(-1),
iHwState(STATE_HW_INITIALIZED),
nBytesConsumed(0),
nBytesWritten(0),
bIsAudioRouted(false),
iTimeoutTimer(NULL)
{
    LOGV("constructor");
    iClockTimeOfWriting_ns = 0;
    iInputFrameSizeInBytes = 0;
    afd = -1;
    bIsA2DPEnabled = false;
    AudioFlinger = NULL;
    AudioFlingerClient = NULL;

    LOGV("Opening pcm_dec driver");
    afd = open("/dev/msm_pcm_lp_dec", O_WRONLY | O_NONBLOCK);
    if ( afd < 0 ) {
        LOGE("pcm_dec: cannot open pcm_dec device and the error is %d", errno);
        return;
    } else {
        LOGV("pcm_dec: pcm_dec Driver opened");

        const sp<IAudioFlinger>& af = AndroidAudioLPADecode::get_audio_flinger();

        if ( af == 0 ) {
            LOGE("Cannot obtain AF, A2DP will not work and hence failing LPA Decode");
            close(afd);
            afd = -1;
            return;
        }
    }

    // semaphore used to communicate between this  mio and the audio tunnel thread
    iAudioThreadSem = new OsclSemaphore();
    iAudioThreadSem->Create(0);
    iAudioThreadTermSem = new OsclSemaphore();
    iAudioThreadTermSem->Create(0);
    iAudioThreadCreatedSem = new OsclSemaphore();
    iAudioThreadCreatedSem->Create(0);

    // semaphore used to communicate between this  mio and the decoder thread
    iEventThreadSem = new OsclSemaphore();
    iEventThreadSem->Create(0);
    iEventThreadTermSem = new OsclSemaphore();
    iEventThreadTermSem->Create(0);

    // semaphore used for A2DP thread
    iA2DPThreadSem = new OsclSemaphore();
    iA2DPThreadSem->Create(0);
    iA2DPThreadTermSem = new OsclSemaphore();
    iA2DPThreadTermSem->Create(0);
    iA2DPThreadReturnSem = new OsclSemaphore();
    iA2DPThreadReturnSem->Create(0);
    iA2DPThreadCreatedSem = new OsclSemaphore();
    iA2DPThreadCreatedSem->Create(0);

    // locks to access the queues by this mio and by the audio tunnel thread
    iOSSRequestQueueLock.Create();
    iOSSRequestQueue.reserve(iWriteResponseQueue.capacity());

    iOSSResponseQueueLock.Create();
    iOSSResponseQueue.reserve(iWriteResponseQueue.capacity());

    iOSSBufferSwapQueueLock.Create();
    iOSSBufferSwapQueue.reserve(iOSSResponseQueue.capacity());

    iPmemInfoQueueLock.Create();
    iDeviceSwitchLock.Create();

    // create active timing object
    OsclMemAllocator alloc;
    OsclAny*ptr=alloc.allocate(sizeof(AndroidAudioMIOActiveTimingSupport));
    if ( ptr ) {
        iActiveTiming=new(ptr)AndroidAudioMIOActiveTimingSupport(kMaxClockDriftInMsecs, kMaxClockCorrection);
        iActiveTiming->setThreadSemaphore(iAudioThreadSem);
    }

    LOGV("AudioFlinger Client registration");
    AndroidAudioLPADecode::AudioFlinger->registerClient(AndroidAudioLPADecode::AudioFlingerClient);

    // Initialize the OSCL timer for timeouts
    iTimeoutTimer = OSCL_NEW(OsclTimer<OsclMemAllocator>, ("lpadecode_timeout"));
    iTimeoutTimer->SetObserver(this);
    iTimeoutTimer->SetFrequency(1); // 1 second Frequency
    // Cancel all timers - This should be invoked only when we hit the Pause state.
    iTimeoutTimer->Clear(); // Make sure to clear all the timers.
}

int AndroidAudioLPADecode::initCheck()
{
    if ( afd >= 0 ) {
        LOGV("pcm_dec / LPA SUCCESS");
        return PVMFSuccess;
    } else {
        LOGE("pcm_dec / LPA setup failed");
        return PVMFFailure;
    }
}

OSCL_EXPORT_REF AndroidAudioLPADecode::~AndroidAudioLPADecode()
{
    LOGV("AndroidAudioLPADecode::destructor");

    // make sure tunnel thread has exited
    RequestAndWaitForThreadExit();

    // make sure the event thread also has exited if it has not done before.
    RequestAndWaitForEventThreadExit();

    RequestAndWaitForA2DPThreadExit();

    if ( afd >= 0 ) {
        iPmemInfoQueueLock.Lock();

        struct msm_audio_pmem_info pmem_info;

        for ( int32 i = (iPmemInfoQueue.size() - 1); i >= 0; i-- ) {
            pmem_info.vaddr = iPmemInfoQueue[i].iPMem_buf;
            pmem_info.fd = iPmemInfoQueue[i].iPMem_fd;
            ioctl(afd, AUDIO_DEREGISTER_PMEM, &pmem_info);
            iPmemInfoQueue.erase(&iPmemInfoQueue[i]);
        }

        iPmemInfoQueueLock.Unlock();

        close(afd);
        afd = -1;
        iHwState = STATE_HW_INITIALIZED;

        if ( bIsAudioRouted ) {

            // Close the session
            mAudioSink->closeSession();
            bIsAudioRouted = false;
        }

        // clean up some thread interface objects
        iAudioThreadSem->Close();
        delete iAudioThreadSem;
        iAudioThreadTermSem->Close();
        delete iAudioThreadTermSem;
        iAudioThreadCreatedSem->Close();
        delete iAudioThreadCreatedSem;

        iEventThreadSem->Close();
        delete iEventThreadSem;
        iEventThreadTermSem->Close();
        delete iEventThreadTermSem;

        // clean up some thread interface objects
        iA2DPThreadSem->Close();
        delete iA2DPThreadSem;
        iA2DPThreadTermSem->Close();
        delete iA2DPThreadTermSem;
        iA2DPThreadCreatedSem->Close();
        delete iA2DPThreadCreatedSem;
        iA2DPThreadReturnSem->Close();
        delete iA2DPThreadReturnSem;

        iOSSRequestQueueLock.Close();
        iOSSResponseQueueLock.Close();
        iPmemInfoQueueLock.Close();
        iDeviceSwitchLock.Close();
        iOSSBufferSwapQueueLock.Close();
    }

    // cleanup active timing object
    if ( iActiveTiming ) {
        iActiveTiming->~AndroidAudioMIOActiveTimingSupport();
        OsclMemAllocator alloc;
        alloc.deallocate(iActiveTiming);
    }

    // Shutdown and destroy the timer
    if ( iTimeoutTimer ) {
        iTimeoutTimer->Clear();
        OSCL_DELETE(iTimeoutTimer);
    }

    if ( AudioFlinger != NULL ) {
        if ( AudioFlingerClient != NULL ) {
            LOGV("DeRegister the client from AF further notifications");
            if ( true == AudioFlinger->deregisterClient(AudioFlingerClient) ) {
                LOGV("Client De-Register SUCCESS");
            }

            LOGV("Cleaning up reference to AudioFlinger");
            AudioFlinger.clear();
            AudioFlinger = NULL;

            LOGV("Delete the AudioFlinger Client interface");
            AudioFlingerClient.clear();
            AudioFlingerClient = NULL;
        }
    }

    LOGV("AndroidAudioLPADecode::destructor out");
}

// establish binder interface to AudioFlinger service
const sp<IAudioFlinger>& AndroidAudioLPADecode::get_audio_flinger()
{
    Mutex::Autolock _l(AudioFlingerLock);

    if ( AudioFlinger.get() == 0 ) {
        sp<IServiceManager> sm = defaultServiceManager();
        sp<IBinder> binder;
        do {
            binder = sm->getService(String16("media.audio_flinger"));
            if ( binder != 0 )
                break;
            LOGW("AudioFlinger not published, waiting...");
            usleep(500000); // 0.5 s
        } while ( true );
        if ( AudioFlingerClient == NULL ) {
            AudioFlingerClient = new AudioFlingerLPAdecodeClient(this);
        }

        binder->linkToDeath(AudioFlingerClient);
        AudioFlinger = interface_cast<IAudioFlinger>(binder);
    }
    LOGE_IF(AudioFlinger==0, "no AudioFlinger!?");

    return AudioFlinger;
}

AndroidAudioLPADecode::AudioFlingerLPAdecodeClient::AudioFlingerLPAdecodeClient(void *obj)
{
    LOGV("AndroidAudioLPADecode::AudioFlingerLPAdecodeClient::AudioFlingerLPAdecodeClient");
    pBaseClass = (AndroidAudioLPADecode*)obj;
}


void AndroidAudioLPADecode::AudioFlingerLPAdecodeClient::binderDied(const wp<IBinder>& who) {
    Mutex::Autolock _l(pBaseClass->AudioFlingerLock);

    pBaseClass->AudioFlinger.clear();
    LOGW("AudioFlinger server died!");
}

void AndroidAudioLPADecode::AudioFlingerLPAdecodeClient::ioConfigChanged(int event, int ioHandle, void *param2) {
    LOGV("ioConfigChanged() event %d", event);

    if ( event != AudioSystem::A2DP_OUTPUT_STATE ) {
        return;
    }

    switch ( event ) {
        case AudioSystem::A2DP_OUTPUT_STATE:
            {
                LOGV("ioConfigChanged() A2DP_OUTPUT_STATE iohandle is %d with A2DPEnabled in %d", ioHandle, pBaseClass->bIsA2DPEnabled);

                if ( -1 == ioHandle ) {
                    if ( pBaseClass->bIsA2DPEnabled ) {
                        pBaseClass->bIsA2DPEnabled = false;
                        // This is used by both A2DP and Hardware flush tracking mechanism
                        pBaseClass->iFlushPending = false;
                        LOGV("ioConfigChanged:: A2DP Disabled");
                    }
                } else {
                    if ( !pBaseClass->bIsA2DPEnabled ) {
                        pBaseClass->iDeviceSwitchLock.Lock();
                        pBaseClass->bIsA2DPEnabled = true;
                        // This is used by both A2DP and Hardware flush tracking mechanism
                        pBaseClass->iFlushPending = false;
                        LOGV("ioConfigChanged:: A2DP Enabled");
                        pBaseClass->HandleA2DPswitch();
                    }
                }
            }
            break;
    }

    LOGV("ioConfigChanged Out");
}

void AndroidAudioLPADecode::HandleA2DPswitch()
{
    struct msm_audio_stats stats;

    LOGV("Inside HandleA2DPswitch with A2DPEnabled set to %d, Hardware state set to %d", bIsA2DPEnabled, iHwState);

    if ( bIsA2DPEnabled ) {

        // 1. If hardware output is already enabled
        if ( iHwState != STATE_HW_INITIALIZED ) {
            // 1.1 Put the Hardware to paused state
            if ( iHwState == STATE_HW_STARTED ) {

                if ( ioctl(afd, AUDIO_PAUSE, 1) < 0 ) {
                    LOGE("AUDIO PAUSE failed");
                } else {
                    iHwState = STATE_HW_PAUSED;
                }
            }

            if ( iHwState != STATE_HW_STOPPED ) {
                // 1.2 Get the Byte count that is consumed
                if ( ioctl(afd, AUDIO_GET_STATS, &stats) < 0 ) {
                    LOGE("AUDIO_GET_STATUS failed");
                } else {
                    nBytesConsumed = 0;
                    if(stats.unused[0] > 0) {
                        nBytesConsumed = stats.unused[0];
                        nBytesConsumed <<= 32;
                    }
                    nBytesConsumed |= stats.byte_count;
                    LOGV("Number of bytes consumed by DSP is %llu", nBytesConsumed);
                }
            }

            // 1.4 Check for Bytes Consumed
            if ( nBytesConsumed == 0 || iHwState == STATE_HW_STOPPED) {
                LOGV("DSP did not consume any data. Start A2DP for processing data");
                iActiveTiming->setThreadSemaphore(iA2DPThreadSem);
                iA2DPThreadSem->Signal();
                iDeviceSwitchLock.Unlock();
            } else {
                LOGV("Issue Flush to the decoder");
                // 1.3 Flush the decoder
                ioctl(afd, AUDIO_FLUSH, 0);
            }
        }
        // If this is during the startup / bootup
        else if ( iHwState == STATE_HW_INITIALIZED ) {
            iActiveTiming->setThreadSemaphore(iA2DPThreadSem);
            iDeviceSwitchLock.Unlock();
            LOGV("Setting A2DP as active timing object");
        }

        LOGV("AndroidAudioLPADecode::HandleA2DPswitch OUT");
        return;
    }
}

PVMFCommandId AndroidAudioLPADecode::QueryInterface(const PVUuid& aUuid, PVInterface*& aInterfacePtr, const OsclAny* aContext)
{
    LOGV("QueryInterface in");
    // check for active timing extension
    if ( iActiveTiming && (aUuid == PvmiClockExtensionInterfaceUuid) ) {
        PvmiClockExtensionInterface* myInterface = OSCL_STATIC_CAST(PvmiClockExtensionInterface*,iActiveTiming);
        aInterfacePtr = OSCL_STATIC_CAST(PVInterface*, myInterface);
        return QueueCmdResponse(PVMFSuccess, aContext);
    }

    // pass to base class
    else return AndroidAudioMIO::QueryInterface(aUuid, aInterfacePtr, aContext);
}

PVMFCommandId AndroidAudioLPADecode::QueryUUID(const PvmfMimeString& aMimeType,
                                               Oscl_Vector<PVUuid, OsclMemAllocator>& aUuids,
                                               bool aExactUuidsOnly, const OsclAny* aContext)
{
    LOGV("QueryUUID in");
    int32 err;
    OSCL_TRY(err,
             aUuids.push_back(PVMI_CAPABILITY_AND_CONFIG_PVUUID);
            if (iActiveTiming){
            PVUuid uuid;
            iActiveTiming->queryUuid(uuid);
            aUuids.push_back(uuid);
            }
            );
    return QueueCmdResponse(err == OsclErrNone ? PVMFSuccess : PVMFFailure, aContext);
}

PVMFCommandId AndroidAudioLPADecode::Pause(const OsclAny* aContext)
{
    LOGV("AndroidAudioLPADecode::Pause (%p)", aContext);

    if ( !bIsA2DPEnabled ) {
        if ( ( iState == STATE_MIO_STARTED ) && (iHwState == STATE_HW_STARTED) ) {
            LOGV("AndroidAudioLPADecode::Pause - Pause driver");
            ioctl(afd, AUDIO_PAUSE, 1);
            iHwState = STATE_HW_PAUSED;
            LOGV("The state of Hardware is set to %d", iHwState);
        }

        // This is checked explicity because, the Hardware can also be put to
        // Pause state after EOS occurs
        if ( iHwState ==  STATE_HW_PAUSED ) {

            LOGV("Pausing the session");
            mAudioSink->pauseSession();

            LOGV("Start a Timer to honor TCXO shutdown");
            // Start a timer - To put the device into TCXO shutdown
            iTimeoutTimer->Request(ANDROID_AUDIO_LPADEC_TIMERID, 0, ANDROID_AUDIO_LPADEC_SUSPEND_TIMEOUT, this, false);
        }
    }
    return AndroidAudioMIO::Pause(aContext);
}

PVMFCommandId AndroidAudioLPADecode::Start(const OsclAny* aContext)
{
    LOGV("AndroidAudioLPADecode::Start (%p)", aContext);
    LOGV("The bIsA2DPEnabled is set to %d and iState is %d and bSuspendEventRxed is %d", bIsA2DPEnabled, iState, bSuspendEventRxed);
    if ( !bIsA2DPEnabled && iAudioThreadCreatedAndMIOConfigured ) {
        if ( iState == STATE_MIO_PAUSED ) {
            if ( !bSuspendEventRxed && iHwState == STATE_HW_PAUSED ) {

                LOGV("AndroidAudioLPADecode::Start - Resuming Session");
                mAudioSink->resumeSession();

                LOGV("AndroidAudioLPADecode::Start - Resuming Driver");
                ioctl(afd, AUDIO_PAUSE, 0);
            } else {
                // Get the session id and register with HAL
                unsigned short decId;

                LOGV("AndroidAudioLPADecode::Start - Starting driversuspend");
                if ( !bIsAudioRouted ) {

                    if ( ioctl(afd, AUDIO_GET_SESSION_ID, &decId) == -1 ) {
                        LOGE("AUDIO_GET_SESSION_ID FAILED\n");
                    } else {
                        sessionId = (int)decId;
                        LOGV("AUDIO_GET_SESSION_ID success : decId = %d", decId);
                    }

                    LOGV("Opening a routing session for audio playback: sessionId = %d", sessionId);
                    status_t ret = mAudioSink->openSession(AudioSystem::PCM_16_BIT, sessionId);

                    if ( ret ) {
                        LOGE("Opening a routing session failed");
                        close(afd);
                        afd = -1;
                        iHwState = STATE_HW_INITIALIZED;
                    } else {
                        LOGV("AudioSink Opened a session(%d)",sessionId);
                        bIsAudioRouted = true;
                    }
                }

                ioctl(afd, AUDIO_START,0);
                bSuspendEventRxed = false;
            }

            iHwState = STATE_HW_STARTED;
            LOGV("The state of Hardware is set to %d", iHwState);

            LOGV("Cancel the timer that is set for TCXO shutdown");
            iTimeoutTimer->Cancel(ANDROID_AUDIO_LPADEC_TIMERID);

            LOGV("Wake up the AudioThread from sleep");
            iAudioThreadSem->Signal();
        }
    }
    return AndroidAudioMIO::Start(aContext);
}

PVMFCommandId AndroidAudioLPADecode::Stop(const OsclAny* aContext)
{
    LOGV("AndroidAudioLPADecode Stop (%p)", aContext);
    // return all buffer by writecomplete
    returnAllBuffers();
    return AndroidAudioMIO::Stop(aContext);
}

PVMFCommandId AndroidAudioLPADecode::Reset(const OsclAny* aContext)
{
    LOGV("AndroidAudioLPADecode Reset (%p)", aContext);
    // return all buffer by writecomplete
    returnAllBuffers();
    // request tunnel thread to exit
    RequestAndWaitForThreadExit();
    RequestAndWaitForEventThreadExit();
    RequestAndWaitForA2DPThreadExit();

    return AndroidAudioMIO::Reset(aContext);
}

void AndroidAudioLPADecode::cancelCommand(PVMFCommandId command_id)
{
    LOGV("cancelCommand (%u) RequestQ size %d", command_id,iOSSRequestQueue.size());
    iOSSRequestQueueLock.Lock();
    for ( uint32 i = 0; i < iOSSRequestQueue.size(); i++ ) {
        if ( iOSSRequestQueue[i].iCmdId == command_id ) {
            iDataQueued -= iOSSRequestQueue[i].iDataLen;
            if ( iPeer )
                iPeer->writeComplete(PVMFSuccess, iOSSRequestQueue[i].iCmdId, (OsclAny*)iOSSRequestQueue[i].iContext);
            iOSSRequestQueue.erase(&iOSSRequestQueue[i]);
            break;
        }
    }
    iOSSRequestQueueLock.Unlock();
    LOGV("cancelCommand data queued = %u", iDataQueued);

    ProcessWriteResponseQueue();
}

void AndroidAudioLPADecode::returnAllBuffers()
{
    iOSSRequestQueueLock.Lock();
    while ( iOSSRequestQueue.size() ) {
        iDataQueued -= iOSSRequestQueue[0].iDataLen;
        if ( iPeer )
            iPeer->writeComplete(PVMFSuccess, iOSSRequestQueue[0].iCmdId, (OsclAny*)iOSSRequestQueue[0].iContext);
        LOGV("The buffer that is returned is returnAllBuffers %d", iOSSRequestQueue[0].iCmdId);
        iOSSRequestQueue.erase(&iOSSRequestQueue[0]);
    }
    iOSSRequestQueueLock.Unlock();
    LOGV("returnAllBuffers data queued = %u", iDataQueued);

    if ( !bIsA2DPEnabled ) {
        if ( iAudioThreadSem && iAudioThreadCreatedAndMIOConfigured ) {  // Should we call flush first, before returning the buffers?
            LOGV("AndroidAudioLPADecode::returnAllBuffers::Flush the driver to return all the buffers");
            iReturnBuffers = true;
            ioctl(afd, AUDIO_FLUSH, 0);
        }
    } else {
        if ( iA2DPThreadSem && iA2DPThreadCreatedAndMIOConfigured ) {
            iReturnBuffers = true;
            LOGV("Signalling A2DP Thread to wake up*****************");
            iA2DPThreadSem->Signal();
            while ( iA2DPThreadReturnSem->Wait() != OsclProcStatus::SUCCESS_ERROR )
                ;
            LOGV("return buffers signal completed");
        }
    }
}


PVMFCommandId AndroidAudioLPADecode::DiscardData(PVMFTimestamp aTimestamp, const OsclAny* aContext)
{
    LOGV("DiscardData timestamp(%u) RequestQ size %d", aTimestamp,iOSSRequestQueue.size());

    if ( iActiveTiming ) {
        LOGV("Force clock update");
        iActiveTiming->ForceClockUpdate();
    }

    bool sched = false;
    PVMFCommandId audcmdid;
    const OsclAny* context;
    PVMFTimestamp timestamp;

    if ( afd >= 0 && !bIsA2DPEnabled ) {
        LOGV("AndroidAudioLPADecode::DiscardData:: Calling audio flush: in state");
        iFlushPending = true;
        ioctl(afd, AUDIO_FLUSH, 0);
    }

    // This is to ensure that the first buffer is not flushed without writing
    if ( (iState != STATE_MIO_INITIALIZED) &&
         (aTimestamp != 0) ) {
        // the OSSRequest queue should be drained
        // all the buffers in them should be returned to the engine
        // writeComplete cannot be called from here
        // thus the best way is to queue the buffers onto the write response queue
        // and then call RunIfNotReady
        iOSSRequestQueueLock.Lock();
        for ( int32 i = (iOSSRequestQueue.size() - 1); i >= 0; i-- ) {
            audcmdid = iOSSRequestQueue[i].iCmdId;
            context = iOSSRequestQueue[i].iContext;
            timestamp = iOSSRequestQueue[i].iTimestamp;
            iDataQueued -= iOSSRequestQueue[i].iDataLen;
            LOGV("discard buffer (%d) context(%p) timestamp(%u) Datalen(%d)", audcmdid,context, timestamp,iOSSRequestQueue[i].iDataLen);
            iOSSRequestQueue.erase(&iOSSRequestQueue[i]);
            sched = true;

            WriteResponse resp(PVMFSuccess, audcmdid, context, timestamp);
            iWriteResponseQueueLock.Lock();
            iWriteResponseQueue.push_back(resp);
            iWriteResponseQueueLock.Unlock();
        }

        iOSSRequestQueueLock.Unlock();
    }

    if ( bIsA2DPEnabled ) {
        LOGV("DiscardData data queued = %u, setting flush pending", iDataQueued);
        iFlushPending=true;
        mAudioSink->pause();
        // wakeup the audio thread: There is a chance of audio thread waiting in pause
        // state and possibly with a partial buffer
        iA2DPThreadSem->Signal();
    }

    if ( sched )
        RunIfNotReady();

    return AndroidAudioMIO::DiscardData(aTimestamp, aContext);
}

void AndroidAudioLPADecode::RequestAndWaitForThreadExit()
{
    LOGV("AndroidAudioLPADecode::RequestAndWaitForThreadExit In");
    if ( iAudioThreadSem && iAudioThreadCreatedAndMIOConfigured ) {
        LOGV("signal thread for exit");
        iExitAudioThread = true;
        iAudioThreadSem->Signal();
        while ( iAudioThreadTermSem->Wait() != OsclProcStatus::SUCCESS_ERROR )
            ;
        LOGV("RequestAndWaitForThreadExit:: Thread term signal received");
        iAudioThreadCreatedAndMIOConfigured = false;
    } else {
        LOGV("Returned since not started");
    }
}


void AndroidAudioLPADecode::RequestAndWaitForEventThreadExit()
{
    LOGV("AndroidAudioLPADecode::RequestAndWaitForEventThreadExit In");
    if ( iEventThreadSem && iEventThreadCreated ) {
        iEventExitThread = true;
        LOGV("Calling AUDIO_ABORT_GET_EVENT to unblock from the driver");
        ioctl(afd, AUDIO_ABORT_GET_EVENT, 0);
        LOGV("Event Thread for exit");
        iEventThreadSem->Signal();
        while ( iEventThreadTermSem->Wait() != OsclProcStatus::SUCCESS_ERROR )
            ;
        LOGV("Event thread term signal received");
        iEventThreadCreated = false;
    } else {
        LOGV("returned from RequestAndWaitForEventThreadExit since not started");
    }
}

void AndroidAudioLPADecode::RequestAndWaitForA2DPThreadExit()
{
    LOGV("RequestAndWaitForA2DPThreadExit In");
    if ( iA2DPThreadSem && iA2DPThreadCreatedAndMIOConfigured ) {
        LOGV("signal A2DP thread for exit");
        iExitA2DPThread = true;
        iA2DPThreadSem->Signal();
        while ( iA2DPThreadTermSem->Wait() != OsclProcStatus::SUCCESS_ERROR )
            ;
        LOGV("thread term signal received");
        iA2DPThreadCreatedAndMIOConfigured = false;
    } else {
        LOGV("Returned since not started from RequestAndWaitForA2DPThreadExit");
    }
}

void AndroidAudioLPADecode::setParametersSync(PvmiMIOSession aSession, PvmiKvp* aParameters,
                                              int num_elements, PvmiKvp * & aRet_kvp)
{
    LOGV("AndroidAudioLPADecode setParametersSync In");
    AndroidAudioMIO::setParametersSync(aSession, aParameters, num_elements, aRet_kvp);

    // initialize thread when we have enough information
    if ( iAudioSamplingRateValid && iAudioNumChannelsValid && iAudioFormat != PVMF_MIME_FORMAT_UNKNOWN ) {
        OsclThread AudioOutput_Thread, PCMDec_Thread, A2DP_Thread;
        iExitAudioThread = false;
        iExitA2DPThread = false;
        iReturnBuffers = false;

        LOGV("start audio thread");
        OsclProcStatus::eOsclProcError ret = AudioOutput_Thread.Create((TOsclThreadFuncPtr)start_audout_thread_func,
                                                                       0, (TOsclThreadFuncArg)this, Start_on_creation);

        //Don't signal the MIO node that the configuration is complete until the driver latency has been set
        while ( iAudioThreadCreatedSem->Wait() != OsclProcStatus::SUCCESS_ERROR )
            ;

        LOGV("start A2DP thread");
        OsclProcStatus::eOsclProcError ret1 = A2DP_Thread.Create((TOsclThreadFuncPtr)start_a2dp_thread_func,
                                                                 0, (TOsclThreadFuncArg)this, Start_on_creation);

        while ( iA2DPThreadCreatedSem->Wait() != OsclProcStatus::SUCCESS_ERROR )
            ;

        iEventThreadCreated = false;
        iEventExitThread = false;
        //iEventThreadCreated = true;

        LOGV("start Event thread");
        OsclProcStatus::eOsclProcError ret2 = PCMDec_Thread.Create((TOsclThreadFuncPtr)start_event_thread_func,
                                                                   0, (TOsclThreadFuncArg)this, Start_on_creation);

        if ( OsclProcStatus::SUCCESS_ERROR == ret2 ) {
            iEventThreadCreated = true;
        } else {
            iEventExitThread = false;
            LOGE("event PVMFErrResourceConfiguration to peer");
            iObserver->ReportErrorEvent(PVMFErrResourceConfiguration);
            return;
        }

        if ( (  (OsclProcStatus::SUCCESS_ERROR == ret) & (iAudioThreadCreatedAndMIOConfigured = true) ) &&
             (  (OsclProcStatus::SUCCESS_ERROR == ret1) & (iA2DPThreadCreatedAndMIOConfigured = true) ) ) {
            if ( iObserver ) {
                LOGV("event PVMFMIOConfigurationComplete to peer");
                iObserver->ReportInfoEvent(PVMFMIOConfigurationComplete);
            }
        } else {

            if ( iObserver ) {
                LOGE("event PVMFErrResourceConfiguration to peer");
                iObserver->ReportErrorEvent(PVMFErrResourceConfiguration);
            }
        }
    }
    LOGV("AndroidAudioLPADecode setParametersSync out");
}

void AndroidAudioLPADecode::Run()
{
    // if running, update clock
    if ( (iState == STATE_MIO_STARTED) && iInputFrameSizeInBytes ) {
        uint32 msecsQueued = iDataQueued / iInputFrameSizeInBytes * iActiveTiming->msecsPerFrame();
        LOGV("%u msecs of data queued, %u bytes of data queued", msecsQueued,iDataQueued);
        iActiveTiming->UpdateClock();
    }
    AndroidAudioMIO::Run();
}

void AndroidAudioLPADecode::writeAudioLPABuffer(uint8* aData, uint32 aDataLen, PVMFCommandId cmdId,
                                                OsclAny* aContext, PVMFTimestamp aTimestamp, int32 pmem_fd)
{
    // queue up buffer and signal audio thread to process it
    LOGV("writeAudioBuffer :: DataLen(%d), cmdId(%d), Context(%p), Timestamp (%d), PMEMFd (%d)",aDataLen, cmdId, aContext, aTimestamp, pmem_fd);
    OSSRequest req(aData, aDataLen, cmdId, aContext, aTimestamp, pmem_fd);
    iOSSRequestQueueLock.Lock();
    iOSSRequestQueue.push_back(req);
    iDataQueued += aDataLen;
    iOSSRequestQueueLock.Unlock();

    // wake up the audio tunnel thread to process this buffer only if clock has started running
    if ( iActiveTiming->clockState() == PVMFMediaClock::RUNNING ) {
        LOGV("clock is ticking signal thread for data");

        iDeviceSwitchLock.Lock();

        if ( !bIsA2DPEnabled ) {

            if ( iFlushPending ) {
                LOGV("Flush was pending... This can come here, if DiscardData is called, where there was no buffer to flush");
                iFlushPending = false;
                // Ensure to set to 0, since after A2DP switch, flush can be issued.
                // Then the nBytesConsumed should be set to 0, before h/w resume.
                nBytesConsumed = 0;
            }

            // If EOS has occured - the decoder is put to pause state.
            // But incase of continuous seek / we see that the EOS is sent
            // in middle of seek and hence once the user resumes, the playback does not start.
            // Ensure to reset the h/w state to resume, if write buffer is called after EOS.
            if ( bEOS && iHwState == STATE_HW_PAUSED && aDataLen ) {
                LOGV("AndroidAudioLPADecode::writeAudioLPABuffer - Resuming Driver");
                ioctl(afd, AUDIO_PAUSE, 0);

                iHwState = STATE_HW_STARTED;
                LOGV("The state of Hardware is set to %d", iHwState);

                LOGV("Cancel the timer that is set for TCXO shutdown");
                iTimeoutTimer->Cancel(ANDROID_AUDIO_LPADEC_TIMERID);
            }

            iAudioThreadSem->Signal();
            LOGV("writeAudioLPABuffer::Waking up AudioThread from sleep");
        } else {
            iA2DPThreadSem->Signal();
            LOGV("writeAudioLPABuffer::Waking up A2DPThread from sleep");
        }

        iDeviceSwitchLock.Unlock();
    }

    LOGV("Returning from writeAudioLPABuffer");
}

void AndroidAudioLPADecode::TimeoutOccurred(int32 timerID, int32 /*timeoutInfo*/)
{
    if ( timerID == ANDROID_AUDIO_LPADEC_TIMERID ) {
        LOGE("AndroidAudioLPADecode::TimeoutOccurred with the ID ANDROID_AUDIO_LPADEC_TIMERID");

        if ( iHwState == STATE_HW_PAUSED ) {

            LOGE("AndroidAudioLPADecode::TimeoutOccurred :: EOS not occured");
            // Expiration due to Non - EOS state, hence just stop the decoder.
            struct msm_audio_stats stats;

            // If H/W is used for rendering
            if ( ( iState == STATE_MIO_PAUSED ) ||
                 ( iHwState == STATE_HW_PAUSED && bIsA2DPEnabled ) ) {

                // 1. Get the Byte count that is consumed
                if ( ioctl(afd, AUDIO_GET_STATS, &stats)  < 0 ) {
                    LOGE("AUDIO_GET_STATUS failed");
                } else {
                    nBytesConsumed = 0;
                    if(stats.unused[0] > 0) {
                        nBytesConsumed = stats.unused[0];
                        nBytesConsumed <<= 32;
                    }
                    nBytesConsumed |= stats.byte_count;
                    LOGV("Number of bytes consumed by DSP is %llu", nBytesConsumed);
                }

                // Set the Suspension to true
                bSuspendEventRxed = true;

                iHwState = STATE_HW_STOPPED;
                LOGV("The state of Hardware is set to %d", iHwState);

                if ( bIsAudioRouted ) {
                    // 3. Close the session
                    mAudioSink->closeSession();
                    bIsAudioRouted = false;
                }
                LOGV("AUDIO_STOP");
                // 4. Call AUDIO_STOP on the Driver.
                if ( ioctl(afd, AUDIO_STOP, 0) < 0 ) {
                    LOGE("AUDIO_STOP failed");
                }

            }
        }

        // Cancel the timer
        LOGV("Cancelling the ANDROID_AUDIO_LPADEC_TIMERID");
        iTimeoutTimer->Cancel(ANDROID_AUDIO_LPADEC_TIMERID);
    } else {
        LOGE("This ID is not supported and hence ideally this should not come here");
    }
}

//------------------------------------------------------------------------
// audio thread
//

#undef LOG_TAG
#define LOG_TAG "LPADecode_audiothread"

// this is the audio tunnel thread
// used to send data to the linux audio tunnel device
// communicates with the audio MIO via a semaphore, a request queue and a response queue
/*static*/ int AndroidAudioLPADecode::start_audout_thread_func(TOsclThreadFuncArg arg)
{
    LOGV("start_audout_thread_func in");
    AndroidAudioLPADecode *obj = (AndroidAudioLPADecode *)arg;
    prctl(PR_SET_NAME, (unsigned long) "audio out", 0, 0, 0);
    int err = obj->audout_thread_func();
    LOGV("start_audout_thread_func out return code %d",err);
    return err;
}

int AndroidAudioLPADecode::audout_thread_func()
{
    enum {
        IDLE, STOPPING, STOPPED, STARTED, PAUSED
    } state = IDLE;
    int64_t lastClock = 0;
    struct msm_audio_config config;
    float msecsPerFrame = 0;
    int outputFrameSizeInBytes = 0;

    LOGV("audout_thread_func");

#if defined(HAVE_SCHED_SETSCHEDULER) && defined(HAVE_GETTID)
    setpriority(PRIO_PROCESS, gettid(), ANDROID_PRIORITY_AUDIO);
#endif

    if ( iAudioNumChannelsValid == false || iAudioSamplingRateValid == false || iAudioFormat == PVMF_MIME_FORMAT_UNKNOWN ) {
        LOGE("channel count or sample rate is invalid");
        iAudioThreadCreatedAndMIOConfigured = false;
        iAudioThreadCreatedSem->Signal();
        return -1;
    }

    if ( afd >= 0 ) {
        if ( ioctl(afd, AUDIO_GET_CONFIG, &config) < 0 ) {
            LOGE("could not get config");
            close(afd);
            afd = -1;
            iAudioThreadCreatedAndMIOConfigured = false;
            iAudioThreadCreatedSem->Signal();
            iHwState = STATE_HW_INITIALIZED;
            return -1;
        }

        config.sample_rate = iAudioSamplingRate;
        config.channel_count =  iAudioNumChannels;
        LOGV(" in initate_play, sample_rate=%d and channel count \n", iAudioSamplingRate, iAudioNumChannels);
        if ( ioctl(afd, AUDIO_SET_CONFIG, &config) < 0 ) {
            LOGE("could not set config");
            close(afd);
            afd = -1;
            iAudioThreadCreatedAndMIOConfigured = false;
            iAudioThreadCreatedSem->Signal();
            iHwState = STATE_HW_INITIALIZED;
            return -1;
        }
    }

    uint32 latency = 96;
    iActiveTiming->setDriverLatency(latency);
    iAudioThreadCreatedAndMIOConfigured = true;
    iAudioThreadCreatedSem->Signal();

    // this must be set after iActiveTiming->setFrameRate to prevent race
    // condition in Run()
    iInputFrameSizeInBytes = outputFrameSizeInBytes;

    // buffer management
    uint32 bytesToWrite;
    uint8* data = 0;
    uint32 len = 0;
    PVMFCommandId cmdid = 0;
    const OsclAny* context = 0;
    PVMFTimestamp timestamp = 0;
    int32         pmem_fd = -1;

    // wait for signal from MIO thread
    LOGV("wait for signal");
    iAudioThreadSem->Wait();
    LOGV("ready to work");

    // This is possible when the A2DP thread was only active and hardware was never used
    if ( iExitAudioThread ) {
        LOGE("Exiting from the Init part, since this thread was never used");
        iClockTimeOfWriting_ns = 0;
        iAudioThreadTermSem->Signal();
        return 0;
    }

    if ( iState == STATE_MIO_PAUSED ) {
        LOGV("Thread started in Pause state, Wait until the state is moved to executing");
        iAudioThreadSem->Wait();
    }

    if ( ( afd >= 0 )  && ( iState != STATE_MIO_PAUSED ) ) {

        int nRetVal = 0;

        // Get the session id and register with HAL
        unsigned short decId;

        if ( ioctl(afd, AUDIO_GET_SESSION_ID, &decId) == -1 ) {
            LOGE("AUDIO_GET_SESSION_ID FAILED\n");
            nRetVal = -1;
        } else {
            sessionId = (int)decId;
            LOGV("AUDIO_GET_SESSION_ID success : decId = %d", decId);
        }

        LOGV("Opening a routing session for audio playback: sessionId = %d", sessionId);
        status_t ret = mAudioSink->openSession(AudioSystem::PCM_16_BIT, sessionId);

        if ( ret ) {
            LOGE("Opening a routing session failed");
            close(afd);
            afd = -1;
            iHwState = STATE_HW_INITIALIZED;
            nRetVal = -1;
        } else {
            LOGV("AudioSink Opened a session(%d)",sessionId);
            bIsAudioRouted = true;

            if ( ioctl(afd, AUDIO_START, 0) < 0 ) {
                LOGE("Failed to start the driver");
                close(afd);
                afd = -1;
                nRetVal = -1;
            } else {
                LOGV("pcm_dec: AUDIO_START Successful");
                iHwState = STATE_HW_STARTED;
            }
        }

        if ( -1 == nRetVal ) {
            while ( 1 ) {

                LOGE("Waiting for a kill for this thread");
                iAudioThreadSem->Wait();

                if ( iExitAudioThread ) {
                    iAudioThreadTermSem->Signal();
                    break;
                }
                continue;
            }
        }
    }

    while ( 1 ) {
        // if paused, stop the output track
        switch ( iActiveTiming->clockState() ) {
            case PVMFMediaClock::RUNNING:
                // start output
                if ( ( state != STARTED ) || (iHwState != STATE_HW_STARTED) ) {
                    if ( ( iDataQueued || len ) && (iHwState == STATE_HW_STARTED) ) {
                        LOGV("state is set to STARTED");
                        state = STARTED;
                    } else {

                        iOSSRequestQueueLock.Lock();
                        // There are instances where the EOS arrives twice. This will ensure that the
                        // 2nd EOS is also service to goto End of Data (PV).
                        if ( !iOSSRequestQueue[0].iDataLen && !iOSSRequestQueue.empty() ) {
                            LOGV("There is some data that needs to be de-queued");
                            iOSSRequestQueueLock.Unlock();
                            break;
                        } else {
                            iOSSRequestQueueLock.Unlock();
                            LOGV("clock running and no data queued - don't start");
                            iAudioThreadSem->Wait();
                            LOGV("Wake up the audio_out_thread from RUNNING CLOCK State");
                            continue;
                        }
                    }
                } else {
                    LOGV("audio sink already in started state");
                }
                break;
            case PVMFMediaClock::STOPPED:
                LOGV("clock has been stopped...");
                state = STOPPING;
            case PVMFMediaClock::PAUSED:
                LOGV("clock has been paused...");
                state = PAUSED;
                if ( !iExitAudioThread && !iReturnBuffers ) {
                    LOGV("Thread is being put to wait in state %d", state);
                    iAudioThreadSem->Wait();
                    LOGV("Thread is woken up in state %d", state);
                }
                break;
            default:
                break;
        }

        if ( bIsA2DPEnabled ) {

            LOGV("A2DP is on... Sleep");
            len = 0;
            data = 0;

            if ( iExitAudioThread ) {
                LOGV("Send response for command id %d and this is from Hardware sleep wakeup", cmdid);
                if ( len ) sendResponse(cmdid, context, timestamp);
                break;
            }

            iAudioThreadSem->Wait();
            LOGV("AudioThread Woken up from A2DP sleep");

            // If A2DP is disabled and if the user has not put the playback to explicit pause
            if ( (!bIsA2DPEnabled) && (iState != STATE_MIO_PAUSED) ) {

                if ( iHwState == STATE_HW_STOPPED ) {

                    LOGV("Starting Audio from A2DP switch");

                    // Get the session id and register with HAL
                    unsigned short decId;

                    if ( ioctl(afd, AUDIO_GET_SESSION_ID, &decId) == -1 ) {
                        LOGE("AUDIO_GET_SESSION_ID FAILED\n");
                    } else {
                        sessionId = (int)decId;
                        LOGV("AUDIO_GET_SESSION_ID success : decId = %d", decId);
                    }

                    LOGV("Opening a routing session for audio playback: sessionId = %d", sessionId);
                    status_t ret = mAudioSink->openSession(AudioSystem::PCM_16_BIT, sessionId);

                    if ( ret ) {
                        LOGE("Opening a routing session failed");
                        close(afd);
                        afd = -1;
                        iHwState = STATE_HW_INITIALIZED;
                    } else {
                        LOGV("AudioSink Opened a session(%d)",sessionId);
                        bIsAudioRouted = true;
                    }

                    LOGV("Hardware in Stopped state, Start the driver");
                    if ( ioctl (afd, AUDIO_START, 0) < 0 ) {
                        LOGE("Start failed");
                    }

                    state = STARTED;
                    iHwState = STATE_HW_STARTED;

                    LOGV("Cancel the timer that is set for TCXO shutdown");
                    iTimeoutTimer->Cancel(ANDROID_AUDIO_LPADEC_TIMERID);
                }
            }

            continue;
        }

        if ( ( iState == STATE_MIO_PAUSED ) && !bEOS && !iReturnBuffers ) {
            LOGV("In paused state, so dont dequeue the buffer, wait until state transisiotn happens");
            continue;
        }

        // if out of data, check the request queue
        if ( len == 0 ) { // Event thread to control data thread.
            //LOGV("no playable data, Request Q size %d",iOSSRequestQueue.size());
            iOSSRequestQueueLock.Lock();
            bool empty = iOSSRequestQueue.empty();
            if ( !empty ) {
                data = iOSSRequestQueue[0].iData;
                len = iOSSRequestQueue[0].iDataLen;
                cmdid = iOSSRequestQueue[0].iCmdId;
                context = iOSSRequestQueue[0].iContext;
                timestamp = iOSSRequestQueue[0].iTimestamp;
                pmem_fd = iOSSRequestQueue[0].pmemfd;
                iDataQueued -= len;
                if ( len ) {
                    iOSSRequestQueue.erase(&iOSSRequestQueue[0]);
                }

                LOGV("Dequed buffer buffer command id (%d), timestamp = %u data queued = %u, len is %u and pmemfd is %d", cmdid, timestamp,iDataQueued, len, pmem_fd);

                if ( bEOS && len ) {
                    bEOS = false;
                }
            }
            iOSSRequestQueueLock.Unlock();

            // if queue is empty, wait for more work
            // FIXME: Why do end up here so many times when stopping?
            if ( empty && !iExitAudioThread && !iReturnBuffers ) {
                LOGV("queue is empty, wait for more work");
                iAudioThreadSem->Wait();
            }

            // empty buffer means "End-Of-Stream" - send response to MIO
            else if ( len == 0 && !bEOS ) {
                LOGV("EOS");
                bEOS = true;
                if ( iHwState != STATE_HW_PAUSED ) {
                    LOGV("Calling fsync");
                    if ( !iExitAudioThread && (fsync(afd) < 0) && iHwState == STATE_HW_STOPPED )
                    {
                        LOGV("Fsync failed because the h/w is stopped in Fsync");
                        bEOS = false;
                        continue;
                    }
                    else {

                        iOSSRequestQueueLock.Lock();
                        bool empty = iOSSRequestQueue.empty();

                        // This is to ensure that the flush does not remove this from Queue and new buffer
                        // is not getting flushed.
                        if ( !empty && iOSSRequestQueue[0].iCmdId == cmdid ) {
                            iOSSRequestQueue.erase(&iOSSRequestQueue[0]);

                            LOGV("Out Of Fsycn and sending response to cmdid %d", cmdid);
                            sendResponse(cmdid, context, timestamp);
                        }
                        iOSSRequestQueueLock.Unlock();
                    }
                }
                state = STOPPED;

                if ( !iExitAudioThread ) {
                    nsecs_t interval_nanosec = 0; // Interval between last writetime and EOS processing time in nanosec
                    nsecs_t now = systemTime(SYSTEM_TIME_MONOTONIC);
                    LOGV("now = %lld ,iClockTimeOfWriting_ns = %lld",now, iClockTimeOfWriting_ns);
                    if ( now >= iClockTimeOfWriting_ns ) {
                        interval_nanosec = now - iClockTimeOfWriting_ns;
                    } else { //when timevalue wraps
                        interval_nanosec = 0;
                    }
                    LOGV(" I am early,going for sleep for latency = %u millsec, interval_nanosec = %lld",latency, interval_nanosec);
                    struct timespec requested_time_delay, remaining;
                    requested_time_delay.tv_sec = latency/1000;
                    nsecs_t latency_nanosec = (latency%1000)*1000*1000;
                    if ( interval_nanosec < latency_nanosec ) {
                        requested_time_delay.tv_nsec = latency_nanosec - interval_nanosec;
                        nanosleep (&requested_time_delay, &remaining);
                        LOGV(" Wow, what a great siesta....send response to engine");
                    } else {// interval is greater than latency so no need of sleep
                        LOGV(" No time to sleep :( send response to engine anyways");
                    }
                    iClockTimeOfWriting_ns = 0;
                    sendResponse(cmdid, context, timestamp);

                    LOGV("Putting the Driver to pause state - Incase we need to loopback. Else the driver will be closed.");
                    if ( ioctl(afd, AUDIO_PAUSE, 1) < 0 ) {
                        LOGE("Failed to AUDIO_PAUSE");
                    }
                    iHwState = STATE_HW_PAUSED;
                    LOGV("State of the Hardware is set to %d", iHwState);
                }
            }
            // EOS received twice use case (Super fast forward)
            else if ( bEOS && len == 0 ) {

                iOSSRequestQueueLock.Lock();
                bool empty = iOSSRequestQueue.empty();

                // This is to ensure that the flush does not remove this from Queue and new buffer
                // is not getting flushed.
                if ( !empty && iOSSRequestQueue[0].iCmdId == cmdid ) {
                    iOSSRequestQueue.erase(&iOSSRequestQueue[0]);

                    LOGV("Sending response for the 2nd EOS");
                    sendResponse(cmdid, context, timestamp);
                }
                iOSSRequestQueueLock.Unlock();
            }
        }

        if ( iReturnBuffers ) {
            LOGV("Return buffers from the audio thread due to discard data cmdid %d", cmdid);
            if ( len ) sendResponse(cmdid, context, timestamp);
            iReturnBuffers=false;
            data = 0;
            len = 0;
        }

        // check for exit signal
        if ( iExitAudioThread ) {
            LOGV("Exiting the thread ***, so sending response for cmdid %d", cmdid);
            if ( len ) sendResponse(cmdid, context, timestamp);
            break;
        }

        // data to output?
        if ( len && (state == STARTED) && !iExitAudioThread ) {

            bytesToWrite = len;

            if ( !bIsA2DPEnabled ) {

                bool   bIsPmemRegistered = false;
                struct msm_audio_aio_buf aio_buf_local;
                struct msm_audio_pmem_info pmem_info;

                memset(&aio_buf_local, 0, sizeof(msm_audio_aio_buf));
                memset(&pmem_info, 0, sizeof(msm_audio_pmem_info));

                iPmemInfoQueueLock.Lock();

                for ( int32 i = (iPmemInfoQueue.size() - 1); i >= 0; i-- ) {

                    LOGV("The Fd in the Queue is %d and fd received is %d", iPmemInfoQueue[i].iPMem_fd, pmem_fd);

                    // 1. if the buffer address match, delete from the queue
                    if ( iPmemInfoQueue[i].iPMem_fd == pmem_fd ) {
                        bIsPmemRegistered = true;
                        break;
                    }
                }

                // Check if the FD is already registered, then dont register again
                if ( !bIsPmemRegistered ) {

                    PmemInfo Info(pmem_fd, (OsclAny *)data);

                    // Push this into the queue
                    iPmemInfoQueue.push_back(Info);

                    LOGV("Registering PMEM with fd %d and address as %x", pmem_fd, data);

                    pmem_info.fd = pmem_fd;
                    pmem_info.vaddr = data;

                    if ( ioctl(afd, AUDIO_REGISTER_PMEM, &pmem_info) < 0 ) {
                        LOGE("Registration of PMEM with the Driver failed with fd %d and mememory %x",
                             pmem_info.fd, pmem_info.vaddr);
                    }
                }

                iPmemInfoQueueLock.Unlock();

                // This state can be valid only if:
                // There is a switch from A2DP to hardware decoder
                // Ensure that the Bytes consumed is taken care.
                if ( nBytesConsumed ) {
                    LOGV("Switch from A2DP to hardware");
                    data = data + (bytesToWrite - nBytesConsumed);
                    bytesToWrite = nBytesConsumed;
                    len = bytesToWrite; // reset the actual length
                    iDataQueued = iDataQueued +  bytesToWrite;
                    nBytesConsumed = 0;
                }

                aio_buf_local.buf_addr = data;
                aio_buf_local.buf_len = bytesToWrite;
                aio_buf_local.data_len = bytesToWrite;
                aio_buf_local.private_data = (void*) pmem_fd;

                if ( (bytesToWrite % 2) != 0 ) {
                    LOGV("Increment for even bytes");
                    aio_buf_local.data_len += 1;
                }

                LOGV("The buffer that is written CMDID %d and fd as %d", cmdid, pmem_fd);
                if ( ioctl(afd, AUDIO_ASYNC_WRITE, &aio_buf_local) < 0 ) {
                    LOGV("error on async write\n");
                    break;
                }

                // Put the data into the response queue
                iOSSResponseQueueLock.Lock();
                OSSRequest req(data, len, cmdid, context, timestamp, pmem_fd);
                iOSSResponseQueue.push_back(req);
                iOSSResponseQueueLock.Unlock();

                len -= bytesToWrite;

                iEventThreadSem->Signal(); // Wake up the thread to wait on the event.
            }
        }
    }

    iClockTimeOfWriting_ns = 0;
    iAudioThreadTermSem->Signal();

    return 0;
}

#undef LOG_TAG
#define LOG_TAG "LPADecode_eventthread"

// Event processing thread , used to receive AIO events from PCMdec HW
// communicates with the audio MIO via a semaphore.
int AndroidAudioLPADecode::start_event_thread_func(TOsclThreadFuncArg arg)
{
    AndroidAudioLPADecode *obj = (AndroidAudioLPADecode *)arg;
    prctl(PR_SET_NAME, (unsigned long) " event thread", 0, 0, 0);
    int err = obj->event_thread_func();
    return err;
}

int AndroidAudioLPADecode::event_thread_func()
{
    struct msm_audio_event pcmdec_event;
    int        rc = 0;

    // These variables are used to store the details about
    // first element that will be delivered once A2DP is acked.
    PVMFCommandId ConsumedBuf_CmdId = -1;
    const OsclAny* Context;
    PVMFTimestamp ConsumedBuf_Timestamp;
    bool bIsRequestQbackedup = false;

#if defined(HAVE_SCHED_SETSCHEDULER) && defined(HAVE_GETTID)
    setpriority(PRIO_PROCESS, gettid(), ANDROID_PRIORITY_AUDIO);
#endif
    // wait for signal from MIO thread
    LOGV("event_thread_func wait for signal \n");
    iEventThreadSem->Wait();
    LOGV("event_thread_func ready to work \n");

    while ( 1 ) {

        if ( iEventExitThread ) {
            LOGV("exitpcmdec thread... breaking out if loop \n");
            break;
        }

        rc = ioctl(afd, AUDIO_GET_EVENT, &pcmdec_event);
        LOGV("pcm dec Event Thread rc = %d and errno is %d",rc, errno);

        if ( (rc < 0) && (errno == ENODEV ) ) {

            LOGV("AUDIO_ABORT_GET_EVENT called. Exit the thread");

            if ( iEventExitThread ) {
                LOGV("exitpcmdec thread... breaking out of event thread loop \n");
                break;
            }

            iEventThreadSem->Wait();
            continue;
        }

        switch ( pcmdec_event.event_type ) {
            case AUDIO_EVENT_WRITE_DONE:
                {
                    LOGV("WRITE_DONE: addr %p len %d and fd is %d\n",
                         pcmdec_event.event_payload.aio_buf.buf_addr,
                         pcmdec_event.event_payload.aio_buf.data_len,
                         (int32) pcmdec_event.event_payload.aio_buf.private_data);

                    iOSSResponseQueueLock.Lock();

                    for ( int32 i = (iOSSResponseQueue.size() - 1); i >= 0; i-- ) {

                        // 1. if the buffer address match, delete from the queue
                        if ( iOSSResponseQueue[i].pmemfd == (int32) pcmdec_event.event_payload.aio_buf.private_data ) {

                            // If A2DP switch happens
                            // If there is a TCXO Shutdown - Re-arrange the buffer.
                            if ( bIsA2DPEnabled || bSuspendEventRxed ) {

                                int32 nBytesRemain = nBytesConsumed - nBytesWritten;

                                LOGV("nBytesConsumed set to %llu,nBytesWritten set to %llu and nBytesRemain set to %d", nBytesConsumed, nBytesWritten, nBytesRemain);

                                // This is the total number of bytes consumed.
                                // This buffer needs be released back, since it is consumed by driver
                                if ( nBytesRemain >= (int32) iOSSResponseQueue[i].iDataLen ) {

                                    LOGV("This buffer cmdid is already rendered by DSP %d", iOSSResponseQueue[i].iCmdId);

                                    // Update the Bytes written to DSP value
                                    //nBytesConsumed = nBytesConsumed - iOSSResponseQueue[i].iDataLen;
                                    nBytesWritten = nBytesWritten + iOSSResponseQueue[i].iDataLen;;

                                    // Update the Number of bytes returned value - When this becomes 0, it means all
                                    // the buffers are returned. Hence send back the response to first buffer.
                                    // Store the first buffer info, so that it can be Acked when all the buffers are
                                    // returned and queue is setup.
                                    ConsumedBuf_CmdId = iOSSResponseQueue[i].iCmdId;
                                    ConsumedBuf_Timestamp = iOSSResponseQueue[i].iTimestamp;
                                    Context = iOSSResponseQueue[i].iContext;
                                    iOSSResponseQueue.erase(&iOSSResponseQueue[i]);
                                    break;
                                } else {
                                    iOSSRequestQueueLock.Lock();

                                    if ( !bIsRequestQbackedup ) {

                                        // If the queue is not empty, then queue up the exisiting buffers
                                        if ( iOSSRequestQueue.size() ) {
                                            LOGV("Have to back up the data and migrate the new data to request queue");
                                            while ( iOSSRequestQueue.size() ) {
                                                LOGV("Data to backup is %d", iOSSRequestQueue[0].iCmdId);
                                                OSSRequest reqBSQ(iOSSRequestQueue[0].iData,
                                                                  iOSSRequestQueue[0].iDataLen,
                                                                  iOSSRequestQueue[0].iCmdId,
                                                                  iOSSRequestQueue[0].iContext,
                                                                  iOSSRequestQueue[0].iTimestamp,
                                                                  iOSSRequestQueue[0].pmemfd);

                                                iOSSBufferSwapQueueLock.Lock();
                                                iOSSBufferSwapQueue.push_back(reqBSQ);
                                                iOSSBufferSwapQueueLock.Unlock();
                                                iOSSRequestQueue.erase(&iOSSRequestQueue[0]);
                                            }
                                        }
                                    }

                                    LOGV("Add cmd id to queue %d", iOSSResponseQueue[i].iCmdId);
                                    // This buffer is partially consumed.
                                    // Queue it again for the A2DP to get consumed.

                                    if ( nBytesRemain > 0 ) {
                                        OSSRequest req(iOSSResponseQueue[i].iData + nBytesRemain,
                                                       iOSSResponseQueue[i].iDataLen - nBytesRemain,
                                                       iOSSResponseQueue[i].iCmdId,
                                                       iOSSResponseQueue[i].iContext,
                                                       iOSSResponseQueue[i].iTimestamp,
                                                       iOSSResponseQueue[i].pmemfd);
                                        iOSSRequestQueue.push_back(req);
                                    } else {
                                        OSSRequest req(iOSSResponseQueue[i].iData,
                                                       iOSSResponseQueue[i].iDataLen,
                                                       iOSSResponseQueue[i].iCmdId,
                                                       iOSSResponseQueue[i].iContext,
                                                       iOSSResponseQueue[i].iTimestamp,
                                                       iOSSResponseQueue[i].pmemfd);
                                        iOSSRequestQueue.push_back(req);
                                    }

                                    LOGV("The size of the request queue is %d", iOSSRequestQueue.size());
                                    iOSSRequestQueueLock.Unlock();

                                    // Update the data queued value to ensure that the timestamp is calculated properly
                                    if ( nBytesRemain > 0 ) {
                                        iDataQueued += iOSSResponseQueue[i].iDataLen - nBytesRemain;

                                        // Update the actual number of bytes written by DSP
                                        nBytesWritten = nBytesWritten + nBytesRemain;
                                    } else {
                                        iDataQueued += iOSSResponseQueue[i].iDataLen;
                                    }

                                    iOSSResponseQueue.erase(&iOSSResponseQueue[i]);

                                    if ( !bIsRequestQbackedup ) {

                                        if ( bIsA2DPEnabled ) {

                                            LOGV("Close the session");
                                            // Close the session
                                            mAudioSink->closeSession();
                                            bIsAudioRouted = false;

                                            // Call AUDIO_STOP on the Driver.
                                            LOGV("Inside A2DP transition and calling AUDIO_STOP");
                                            if ( ioctl(afd, AUDIO_STOP, 0) < 0 ) {
                                                LOGE("AUDIO_STOP failed");
                                            } else {
                                                iHwState = STATE_HW_STOPPED;
                                                LOGV("The state of Hardware is set to %d", iHwState);
                                            }

                                            LOGV("Activating the A2DP Thread");
                                            iActiveTiming->setThreadSemaphore(iA2DPThreadSem);
                                            iA2DPThreadSem->Signal();
                                            iDeviceSwitchLock.Unlock();
                                        }

                                        nBytesConsumed = 0;
                                        bIsRequestQbackedup = true;
                                    }

                                    LOGV("The size of the response queue is %d", iOSSResponseQueue.size());
                                    if ( iOSSResponseQueue.empty() ) {

                                        // back up the data and then send response
                                        LOGV("The size of the backup queue is set to %d", iOSSBufferSwapQueue.size());

                                        if ( !iOSSBufferSwapQueue.empty() ) {
                                            LOGV("Not empty and start the backup with cmd id %d", iOSSBufferSwapQueue[0].iCmdId );
                                            while ( iOSSBufferSwapQueue.size() ) {
                                                OSSRequest reqBSQ(iOSSBufferSwapQueue[0].iData,
                                                                  iOSSBufferSwapQueue[0].iDataLen,
                                                                  iOSSBufferSwapQueue[0].iCmdId,
                                                                  iOSSBufferSwapQueue[0].iContext,
                                                                  iOSSBufferSwapQueue[0].iTimestamp,
                                                                  iOSSBufferSwapQueue[0].pmemfd);

                                                iOSSRequestQueueLock.Lock();
                                                iOSSRequestQueue.push_back(reqBSQ);
                                                iOSSRequestQueueLock.Unlock();
                                                iOSSBufferSwapQueue.erase(&iOSSBufferSwapQueue[0]);
                                            }
                                        }

                                        bIsRequestQbackedup = false;
                                        LOGV("cmdid waiting for response is %d", ConsumedBuf_CmdId);
                                        if ( -1 != ConsumedBuf_CmdId ) {
                                            sendResponse(ConsumedBuf_CmdId, Context, ConsumedBuf_Timestamp);
                                            ConsumedBuf_CmdId = -1;
                                        }

                                        if ( iHwState == STATE_HW_STOPPED ) {
                                            LOGV("If the hardware state is set to 0, then reset the BytesWritten counter");
                                            nBytesWritten = 0;
                                        }
                                    }
                                    break;
                                }
                            } else {
                                if ( !iFlushPending ) {
                                    nBytesWritten = nBytesWritten +  iOSSResponseQueue[i].iDataLen;
                                    LOGV("Number of Bytes actually written to DSP is %llu", nBytesWritten);
                                }

                                LOGV("Sending response to cmdid %d", iOSSResponseQueue[i].iCmdId);
                                sendResponse(iOSSResponseQueue[i].iCmdId, iOSSResponseQueue[i].iContext,
                                             iOSSResponseQueue[i].iTimestamp);
                                iOSSResponseQueue.erase(&iOSSResponseQueue[i]);

                                // If the Flush command is completed
                                if ( (iFlushPending) && (iOSSResponseQueue.size() == 0) ) {
                                    iFlushPending = false;
                                    nBytesWritten = 0;
                                    nBytesConsumed = 0;
                                    LOGV("Flush completed and nBytesWritten is set to 0");
                                }
                                break;
                            }
                        }
                    }

                    iOSSResponseQueueLock.Unlock();
                }
                break;

            case AUDIO_EVENT_SUSPEND:
                {
                    LOGE("AUDIO_EVENT_SUSPEND recieved");
                    struct msm_audio_stats stats;

                    // If H/W is used for rendering
                    if ( ( iState == STATE_MIO_PAUSED && iHwState != STATE_HW_STOPPED ) ||
                         ( iHwState == STATE_HW_PAUSED && bIsA2DPEnabled ) ) {

                        // 1. Get the Byte count that is consumed
                        if ( ioctl(afd, AUDIO_GET_STATS, &stats)  < 0 ) {
                            LOGE("AUDIO_GET_STATUS failed");
                        } else {
                            nBytesConsumed = 0;
                            if(stats.unused[0] > 0) {
                                nBytesConsumed = stats.unused[0];
                                nBytesConsumed <<= 32;
                            }
                            nBytesConsumed |= stats.byte_count;
                            LOGV("Number of bytes consumed by DSP is %llu", nBytesConsumed);
                        }

                        // Set the Suspension to true
                        bSuspendEventRxed = true;

                        iHwState = STATE_HW_STOPPED;
                        LOGV("The state of Hardware is set to %d", iHwState);

                        if ( bIsAudioRouted ) {
                            // 3. Close the session
                            mAudioSink->closeSession();
                            bIsAudioRouted = false;
                        }
                        LOGV("Inside AUDIO_EVENT_SUSPEND and calling AUDIO_STOP");
                        // 4. Call AUDIO_STOP on the Driver.
                        if ( ioctl(afd, AUDIO_STOP, 0) < 0 ) {
                            LOGE("AUDIO_STOP failed");
                        }

                        if ( bIsA2DPEnabled ) {
                            LOGV("A2DP already enabled. Clear the Bytes written count");
                            nBytesWritten = 0;
                        }

                    } else if ( iState == STATE_MIO_STARTED ) {
                        // In Execution, Just update the suspend flag
                        LOGV("AUDIO_EVENT_SUSPEND received in executing state - Igonre it:: For now");
                    }
                }
                break;

            case AUDIO_EVENT_RESUME:
                {
                    LOGV("AUDIO_EVENT_RESUME received");
                }
                break;


            default:
                LOGV("Received Invalid Event from driver \n");
                break;
        }
    }

    LOGV("Event thread exiting");
    iEventThreadTermSem->Signal();

    return 0;
}

#undef LOG_TAG
#define LOG_TAG "LPADecode_A2DPthread"

// this is the A2DP output thread
// used to send data to the A2DP
// communicates with the audio MIO via a semaphore, a request queue and a response queue
/*static*/ int AndroidAudioLPADecode::start_a2dp_thread_func(TOsclThreadFuncArg arg)
{
    LOGV("start_a2dp_thread_func in");
    AndroidAudioLPADecode *obj = (AndroidAudioLPADecode *)arg;
    prctl(PR_SET_NAME, (unsigned long) "a2dp thread", 0, 0, 0);
    int err = obj->a2dp_thread_func();
    LOGV("start_a2dp_thread_func out return code %d",err);
    return err;
}

int AndroidAudioLPADecode::a2dp_thread_func()
{
    enum {
        IDLE, STOPPED, STARTED, PAUSED
    } state = IDLE;
    int64_t lastClock = 0;

    // LOGD("audout_thread_func");

#if defined(HAVE_SCHED_SETSCHEDULER) && defined(HAVE_GETTID)
    setpriority(PRIO_PROCESS, gettid(), ANDROID_PRIORITY_AUDIO);
#endif

    if ( iAudioNumChannelsValid == false || iAudioSamplingRateValid == false || iAudioFormat == PVMF_MIME_FORMAT_UNKNOWN ) {
        iA2DPThreadCreatedAndMIOConfigured = false;
        iA2DPThreadCreatedSem->Signal();
        LOGE("channel count or sample rate is invalid");
        return -1;
    }

    iA2DPThreadCreatedAndMIOConfigured = true;
    iA2DPThreadCreatedSem->Signal();

    // wait for signal from MIO thread
    // Stop the Track, until there is a valid start thread.
    LOGV("wait for signal");
    iA2DPThreadSem->Wait();
    LOGV("ready to work");

    // This is possible when the A2DP thread was only active and hardware was never used
    if ( iExitA2DPThread ) {
        LOGE("Exiting from the Init part, since this thread was never used");
        iClockTimeOfWriting_ns = 0;
        iA2DPThreadTermSem->Signal();
        return 0;
    }

    LOGV("Creating AudioTrack object: rate=%d, channels=%d, buffers=%d", iAudioSamplingRate, iAudioNumChannels, kNumOutputBuffers);
    status_t ret = mAudioSink->open(iAudioSamplingRate, iAudioNumChannels, ((iAudioFormat==PVMF_MIME_PCM8)?AudioSystem::PCM_8_BIT:AudioSystem::PCM_16_BIT), kNumOutputBuffers);

    if ( ret != 0 ) {
        iA2DPThreadCreatedAndMIOConfigured = false;
        iA2DPThreadCreatedSem->Signal();
        LOGE("Error creating AudioTrack");
        return -1;
    }

    // calculate timing data
    int outputFrameSizeInBytes = mAudioSink->frameSize();
    float msecsPerFrame = mAudioSink->msecsPerFrame();
    uint32 latency = mAudioSink->latency();
    LOGV("driver latency(%u),outputFrameSizeInBytes(%d),msecsPerFrame(%f),frame count(%d)", latency,outputFrameSizeInBytes,msecsPerFrame,mAudioSink->frameCount());

    // initialize active timing
    iActiveTiming->setFrameRate(msecsPerFrame);
    iActiveTiming->setDriverLatency(latency);
    // this must be set after iActiveTiming->setFrameRate to prevent race
    // condition in Run()
    iInputFrameSizeInBytes = outputFrameSizeInBytes;

    // buffer management
    uint32 bytesAvailInBuffer = 0;
    uint32 bytesToWrite;
    uint32 bytesWritten;
    uint8* data = 0;
    uint32 len = 0;
    PVMFCommandId cmdid = 0;
    const OsclAny* context = 0;
    PVMFTimestamp timestamp = 0;

    // Buffer management to communicate to Hardware thread - incase of context switch
    bool bIsHWActivated = false;
    uint8* dataRendered = 0;
    uint32  nActualBytes = 0; // this is used for switch from A2DP to h/w rendering
    int32 pmem_fd = -1; // pmem info.

    while ( 1 ) {
        // if paused, stop the output track
        switch ( iActiveTiming->clockState() ) {
            case PVMFMediaClock::RUNNING:
                // start output
                if ( state != STARTED ) {
                    if ( iFlushPending ) {
                        LOGV("flush");
                        mAudioSink->flush();
                        iFlushPending = false;
                        bytesAvailInBuffer = 0;
                        iClockTimeOfWriting_ns = 0;
                        // discard partial buffer and send response to MIO
                        if ( data && len ) {
                            LOGV("discard partial buffer and send response to MIO");
                            sendResponse(cmdid, context, timestamp);
                            data = 0;
                            len = 0;
                        }
                    }
                    if ( (iDataQueued || len) && bIsA2DPEnabled ) {
                        LOGV("start");
                        mAudioSink->start();
                        state = STARTED;
                        bIsHWActivated = false;
                    } else {
                        LOGV("clock running and no data queued - don't start track");
                    }
                } else {
                    LOGV("audio sink already in started state");
                }
                break;
            case PVMFMediaClock::STOPPED:
                LOGV("clock has been stopped...");
            case PVMFMediaClock::PAUSED:
                if ( state == STARTED ) {
                    LOGV("pause");
                    mAudioSink->pause();
                }
                state = PAUSED;
                if ( !iExitA2DPThread && !iReturnBuffers ) {
                    if ( iFlushPending ) {
                        LOGV("flush");
                        mAudioSink->flush();
                        iFlushPending = false;
                        bytesAvailInBuffer = 0;
                        iClockTimeOfWriting_ns = 0;
                        // discard partial buffer and send response to MIO
                        if ( data && len ) {
                            LOGV("discard partial buffer and send response to MIO");
                            sendResponse(cmdid, context, timestamp);
                            data = 0;
                            len = 0;
                        }
                    }

                    if ( bIsA2DPEnabled ) {
                        LOGV("wait");
                        iA2DPThreadSem->Wait();
                        LOGV("awake");
                    }
                }
                break;
            default:
                break;
        }

        if ( !bIsA2DPEnabled ) {

            LOGE("A2DP is disabled");

            if ( iExitA2DPThread ) {
                LOGV("Send response for command id %d and this is from A2DP sleep wakeup", cmdid);
                if ( len ) sendResponse(cmdid, context, timestamp);
                break;
            }

            if ( state == STARTED ) {

                LOGV("Pause, Close the Sink and activate Hardware Semaphore");
                // Pause the sink
                mAudioSink->pause();
                state = PAUSED;
            }

            if ( !bIsHWActivated ) {

                if ( (len != 0) && (data != 0) && (nActualBytes != 0) ) {
                    LOGV("Schedule for the Hardware thread to wakeup and read with requeued cmd id %d", cmdid);

                    OSSRequest req(dataRendered, nActualBytes, cmdid, context, timestamp, pmem_fd);
                    iOSSRequestQueueLock.Lock();
                    iOSSRequestQueue.push_front(req);
                    iOSSRequestQueueLock.Unlock();

                    nBytesConsumed = len;
                }

                data = 0;
                len = 0;
                nActualBytes = len;
                // Activate hardware thread
                iActiveTiming->setThreadSemaphore(iAudioThreadSem);
                iAudioThreadSem->Signal();

                LOGV("Activating hardware");
                bIsHWActivated = true;
            }

            iA2DPThreadSem->Wait();
            LOGV("Woken up from Hardware wait state");
            continue;
        }

        // if out of data, check the request queue
        if ( len == 0 ) {
            //LOGV("no playable data, Request Q size %d",iOSSRequestQueue.size());
            iOSSRequestQueueLock.Lock();
            bool empty = iOSSRequestQueue.empty();
            if ( !empty ) {
                data = dataRendered = iOSSRequestQueue[0].iData;
                len = nActualBytes = iOSSRequestQueue[0].iDataLen;
                cmdid = iOSSRequestQueue[0].iCmdId;
                context = iOSSRequestQueue[0].iContext;
                timestamp = iOSSRequestQueue[0].iTimestamp;
                pmem_fd = iOSSRequestQueue[0].pmemfd;
                iDataQueued -= len;
                iOSSRequestQueue.erase(&iOSSRequestQueue[0]);
                LOGV("receive buffer with cmdid (%d), timestamp = %u data queued = %u", cmdid, timestamp,iDataQueued);
            }
            iOSSRequestQueueLock.Unlock();

            // if queue is empty, wait for more work
            // FIXME: Why do end up here so many times when stopping?
            if ( empty && !iExitA2DPThread && !iReturnBuffers ) {
                LOGV("queue is empty, wait for more work");
                iA2DPThreadSem->Wait();
            }

            // empty buffer means "End-Of-Stream" - send response to MIO
            else if ( len == 0 ) {
                LOGV("EOS");
                state = STOPPED;
                mAudioSink->stop();
                if ( !iExitA2DPThread ) {
                    nsecs_t interval_nanosec = 0; // Interval between last writetime and EOS processing time in nanosec
                    nsecs_t now = systemTime(SYSTEM_TIME_MONOTONIC);
                    LOGV("now = %lld ,iClockTimeOfWriting_ns = %lld",now, iClockTimeOfWriting_ns);
                    if ( now >= iClockTimeOfWriting_ns ) {
                        interval_nanosec = now - iClockTimeOfWriting_ns;
                    } else { //when timevalue wraps
                        interval_nanosec = 0;
                    }
                    LOGV(" I am early,going for sleep for latency = %u millsec, interval_nanosec = %lld",latency, interval_nanosec);
                    struct timespec requested_time_delay, remaining;
                    requested_time_delay.tv_sec = latency/1000;
                    nsecs_t latency_nanosec = (latency%1000)*1000*1000;
                    if ( interval_nanosec < latency_nanosec ) {
                        requested_time_delay.tv_nsec = latency_nanosec - interval_nanosec;
                        nanosleep (&requested_time_delay, &remaining);
                        LOGV(" Wow, what a great siesta....send response to engine");
                    } else {// interval is greater than latency so no need of sleep
                        LOGV(" No time to sleep :( send response to engine anyways");
                    }
                    iClockTimeOfWriting_ns = 0;
                    sendResponse(cmdid, context, timestamp);
                }
            }
        }

        if ( iReturnBuffers ) {
            LOGV("Return buffers from the audio thread");
            if (len) sendResponse(cmdid, context, timestamp);
            iReturnBuffers=false;
            data = 0;
            len = 0;
            nActualBytes = 0;
            iA2DPThreadReturnSem->Signal();
        }

        // check for exit signal
        if ( iExitA2DPThread ) {
            LOGV("exit received");
            if (len) sendResponse(cmdid, context, timestamp);
            nActualBytes = 0;
            break;
        }

        // data to output?
        if ( len && (state == STARTED) && !iExitA2DPThread ) {

            // always align to AudioFlinger buffer boundary
            if ( bytesAvailInBuffer == 0 )
                bytesAvailInBuffer = mAudioSink->bufferSize();

            bytesToWrite = bytesAvailInBuffer > len ? len : bytesAvailInBuffer;
            //LOGV("16 bit :: cmdid = %d, len = %u, bytesAvailInBuffer = %u, bytesToWrite = %u", cmdid, len, bytesAvailInBuffer, bytesToWrite);
            bytesWritten = mAudioSink->write(data, bytesToWrite);
            if ( bytesWritten != bytesToWrite ) {
                LOGE("Error writing audio data");
                iA2DPThreadSem->Wait();
            }
            data += bytesWritten;
            len -= bytesWritten;
            iClockTimeOfWriting_ns = systemTime(SYSTEM_TIME_MONOTONIC);


            // count bytes sent
            bytesAvailInBuffer -= bytesWritten;

            // update frame count for latency calculation
            iActiveTiming->incFrameCount(bytesWritten / outputFrameSizeInBytes);
            //LOGV("outputFrameSizeInBytes = %u,bytesWritten = %u,bytesAvailInBuffer = %u", outputFrameSizeInBytes,bytesWritten,bytesAvailInBuffer);
            // if done with buffer - send response to MIO
            if ( data && !len ) {
                LOGV("done with the data cmdid %d, context %p, timestamp %d ",cmdid, context, timestamp);
                sendResponse(cmdid, context, timestamp);
                data = 0;
                nActualBytes = 0;
            }
        }
    } // while loop

    LOGV("stop and delete track");
    mAudioSink->stop();
    iClockTimeOfWriting_ns = 0;

    // LOGD("a2dp_thread_func exit");
    iA2DPThreadTermSem->Signal();

    return 0;
}
