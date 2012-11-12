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

//#define LOG_NDEBUG 0
#define LOG_TAG "audio_inputA2DP"
#include <utils/Log.h>
#include <utils/threads.h>

#include "oscl_base.h"

#include "android_audio_inputFMA2DP.h"
#include "pv_mime_string_utils.h"
#include "oscl_dll.h"

#include <media/AudioRecord.h>
#include <media/mediarecorder.h>
#include <sys/prctl.h>

using namespace android;

// TODO: get buffer size from AudioFlinger
static int kBufferSize = 2048;

// Define entry point for this DLL
OSCL_DLL_ENTRY_POINT_DEFAULT()
#define BUFFER_POOL_SIZE 10

AndroidAudioInputA2DP::AndroidAudioInputA2DP(uint32 audioSource)
    : OsclTimerObject(OsclActiveObject::EPriorityNominal, "AndroidAudioInputA2DP"),
    iCmdIdCounter(0),
    iPeer(NULL),
    iThreadLoggedOn(false),
    iAudioNumChannels(DEFAULT_AUDIO_NUMBER_OF_CHANNELS),
    iAudioSamplingRate(DEFAULT_AUDIO_SAMPLING_RATE),
    iAudioSource(audioSource),
    iWriteCompleteAO(NULL),
    iMediaBufferMemPool(NULL),
    iState(STATE_IDLE),
    iAudioThreadStarted(false),
    iAuthorClock(NULL),
    iClockNotificationsInf(NULL)
{
    LOGV("AndroidAudioInputA2DP constructor %p", this);
    // semaphore used to communicate between this  mio and the audio output thread
    iAudioThreadSem = new OsclSemaphore();
    iAudioThreadSem->Create(0);
    iAudioThreadTermSem = new OsclSemaphore();
    iAudioThreadTermSem->Create(0);

    //semaphore to communicate between audio thread and BT thread.
    iAudioA2DPThreadSem = new OsclSemaphore();
    iAudioA2DPThreadSem->Create(0);
    iAudioA2DPThreadTermSem = new OsclSemaphore();
    iAudioA2DPThreadTermSem->Create(0);


    iAudioThreadStartLock = new Mutex();
    iAudioThreadStartCV = new Condition();

    //Locks for new queue
    iOSSBTRequestQueueLock.Create();
    iOSSBTResponseQueueLock.Create();
    bIsA2DPEnabled = false;
    mpAudioTrack = 0;
    AudioFlinger = NULL;
    AudioFlingerClient = NULL;

    {
        iAudioFormat=PVMF_MIME_FORMAT_UNKNOWN;
        iExitAudioThread=false;
        iExitA2DPThread=false;
        // Setting up the default audio source type
        iBufferForceWrite = 0;
        iCommandResponseQueue.reserve(11);

        iOSSBTRequestQueue.reserve(10); //New BT queue
        iOSSBTResponseQueue.reserve(10);
        iObserver=NULL;
    }
        const sp<IAudioFlinger>& af = AndroidAudioInputA2DP::get_audio_flinger();

        if ( af == 0 ) {
            return;
        }
}

////////////////////////////////////////////////////////////////////////////
AndroidAudioInputA2DP::~AndroidAudioInputA2DP()
{
    LOGV("AndroidAudioInputA2DP destructor %p", this);

    if(iWriteCompleteAO)
    {
        OSCL_DELETE(iWriteCompleteAO);
        iWriteCompleteAO = NULL;
    }

    if(iMediaBufferMemPool)
    {
        OSCL_DELETE(iMediaBufferMemPool);
        iMediaBufferMemPool = NULL;
    }


    iOSSBTRequestQueueLock.Close();
    iOSSBTResponseQueueLock.Close();

    iAudioThreadSem->Close();
    delete iAudioThreadSem;
    iAudioThreadTermSem->Close();
    delete iAudioThreadTermSem;

    iAudioA2DPThreadSem->Close();
    delete iAudioA2DPThreadSem;
    iAudioA2DPThreadTermSem->Close();
    delete iAudioA2DPThreadTermSem;

    delete iAudioThreadStartLock;
    delete iAudioThreadStartCV;


    if ( AudioFlinger != NULL ) {
        if ( AudioFlingerClient != NULL ) {
            LOGV("DeRegister the client from AF further notifications");
            //if ( true == AudioFlinger->deregisterClient(AudioFlingerClient) ) {
                LOGV("Client De-Register SUCCESS");
            //}

            LOGV("Cleaning up reference to AudioFlinger");
            AudioFlinger.clear();
            AudioFlinger = NULL;

            LOGV("Delete the AudioFlinger Client interface");
            AudioFlingerClient.clear();
            AudioFlingerClient = NULL;
        }
    }
    if (mpAudioTrack) {
        LOGV("Delete Track: %p\n", mpAudioTrack);
        delete mpAudioTrack;
    }

}


// establish binder interface to AudioFlinger service
const sp<IAudioFlinger>& AndroidAudioInputA2DP::get_audio_flinger()
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
            AudioFlingerClient = new AudioFlingerAudioInputA2DPClient(this);
        }

        binder->linkToDeath(AudioFlingerClient);
        AudioFlinger = interface_cast<IAudioFlinger>(binder);
    }
    LOGE_IF(AudioFlinger==0, "no AudioFlinger!?");

    return AudioFlinger;
}


AndroidAudioInputA2DP::AudioFlingerAudioInputA2DPClient::AudioFlingerAudioInputA2DPClient(void *obj)
{
    LOGV("AndroidAudioInputA2DP::AudioFlingerAudioInputA2DPClient::AudioFlingerAudioInputA2DPClient");
    pBaseClass = (AndroidAudioInputA2DP*)obj;
}


void AndroidAudioInputA2DP::AudioFlingerAudioInputA2DPClient::binderDied(const wp<IBinder>& who) {
    Mutex::Autolock _l(pBaseClass->AudioFlingerLock);

    pBaseClass->AudioFlinger.clear();
    LOGW("AudioFlinger server died!");
}

void AndroidAudioInputA2DP::AudioFlingerAudioInputA2DPClient::ioConfigChanged(int event, int ioHandle, void *param2) {
    LOGV("ioConfigChanged() event %d", event);

    if ( event != AudioSystem::OUTPUT_OPENED ) {
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

                        LOGV("ioConfigChanged:: A2DP Disabled");
                    }
                } else {
                    if ( !pBaseClass->bIsA2DPEnabled ) {

                        pBaseClass->bIsA2DPEnabled = true;
                        // This is used by both A2DP and Hardware flush tracking mechanism

                        LOGV("ioConfigChanged:: A2DP Enabled");

                    }
                }
            }
            break;
    }

    LOGV("ioConfigChanged Out");
}

////////////////////////////////////////////////////////////////////////////
PVMFStatus AndroidAudioInputA2DP::connect(PvmiMIOSession& aSession, PvmiMIOObserver* aObserver)
{
    LOGV("connect");

    if(!aObserver)
    {
        LOGE("connect: aObserver is NULL");
        return PVMFFailure;
    }

    int32 err = 0;
    OSCL_TRY(err, iObservers.push_back(aObserver));
    OSCL_FIRST_CATCH_ANY(err, return PVMFErrNoMemory);
    aSession = (PvmiMIOSession)(iObservers.size() - 1); // Session ID is the index of observer in the vector
    return PVMFSuccess;
}

////////////////////////////////////////////////////////////////////////////
PVMFStatus AndroidAudioInputA2DP::disconnect(PvmiMIOSession aSession)
{
    LOGV("disconnect");
    uint32 index = (uint32)aSession;
    if(index >= iObservers.size())
    {
        LOGE("disconnect: Invalid session ID: %d", index);
        return PVMFFailure;
    }

    iObservers.erase(iObservers.begin()+index);
    return PVMFSuccess;
}

////////////////////////////////////////////////////////////////////////////
PvmiMediaTransfer* AndroidAudioInputA2DP::createMediaTransfer(PvmiMIOSession& aSession,
        PvmiKvp* read_formats,
        int32 read_flags,
        PvmiKvp* write_formats,
        int32 write_flags)
{
    LOGV("createMediaTransfer %p", this);
    OSCL_UNUSED_ARG(read_formats);
    OSCL_UNUSED_ARG(read_flags);
    OSCL_UNUSED_ARG(write_formats);
    OSCL_UNUSED_ARG(write_flags);

    uint32 index = (uint32)aSession;
    if(index >= iObservers.size())
    {
        LOGE("Invalid sessions ID: index %d, size %d", index, iObservers.size());
        // Invalid session ID
        OSCL_LEAVE(OsclErrArgument);
        return NULL;
    }

    iWriteCompleteAO = OSCL_NEW(AndroidAudioInputThreadSafeCallbackAO,(this,5));

    return (PvmiMediaTransfer*)this;
}

////////////////////////////////////////////////////////////////////////////
void AndroidAudioInputA2DP::deleteMediaTransfer(PvmiMIOSession& aSession,
        PvmiMediaTransfer* media_transfer)
{
    LOGV("deleteMediaTransfer %p", this);
    uint32 index = (uint32)aSession;
    if(!media_transfer || index >= iObservers.size())
    {
        LOGE("Invalid sessions ID: index %d, size %d", index, iObservers.size());
        // Invalid session ID
        OSCL_LEAVE(OsclErrArgument);
    }
}

////////////////////////////////////////////////////////////////////////////
PVMFCommandId AndroidAudioInputA2DP::QueryUUID(const PvmfMimeString& aMimeType,
        Oscl_Vector<PVUuid, OsclMemAllocator>& aUuids,
        bool aExactUuidsOnly,
        const OsclAny* aContext)
{
    OSCL_UNUSED_ARG(aMimeType);
    OSCL_UNUSED_ARG(aExactUuidsOnly);

    int32 err = 0;
    OSCL_TRY(err, aUuids.push_back(PVMI_CAPABILITY_AND_CONFIG_PVUUID););
    OSCL_FIRST_CATCH_ANY(err, OSCL_LEAVE(OsclErrNoMemory););

    return AddCmdToQueue(AI_CMD_QUERY_UUID, aContext);
}

////////////////////////////////////////////////////////////////////////////
PVMFCommandId AndroidAudioInputA2DP::QueryInterface(const PVUuid& aUuid,
        PVInterface*& aInterfacePtr,
        const OsclAny* aContext)
{
    if(aUuid == PVMI_CAPABILITY_AND_CONFIG_PVUUID)
    {
        PvmiCapabilityAndConfig* myInterface = OSCL_STATIC_CAST(PvmiCapabilityAndConfig*,this);
        aInterfacePtr = OSCL_STATIC_CAST(PVInterface*, myInterface);
    }
    else
    {
        aInterfacePtr = NULL;
    }

    return AddCmdToQueue(AI_CMD_QUERY_INTERFACE, aContext, (OsclAny*)&aInterfacePtr);
}

////////////////////////////////////////////////////////////////////////////
PVMFCommandId AndroidAudioInputA2DP::Init(const OsclAny* aContext)
{
    LOGV("Init");
    if(iState != STATE_IDLE)
    {
        LOGE("Init: Invalid state (%d)", iState);
        OSCL_LEAVE(OsclErrInvalidState);
        return -1;
    }

    return AddCmdToQueue(AI_CMD_INIT, aContext);
}


////////////////////////////////////////////////////////////////////////////
PVMFCommandId AndroidAudioInputA2DP::Start(const OsclAny* aContext)
{
    LOGV("Start");
    if(iState != STATE_INITIALIZED && iState != STATE_PAUSED)
    {
        LOGE("Start: Invalid state (%d)", iState);
        OSCL_LEAVE(OsclErrInvalidState);
        return -1;
    }

    return AddCmdToQueue(AI_CMD_START, aContext);
}

////////////////////////////////////////////////////////////////////////////
PVMFCommandId AndroidAudioInputA2DP::Pause(const OsclAny* aContext)
{
    LOGV("Pause");
    if(iState != STATE_STARTED)
    {
        LOGE("Pause: Invalid state (%d)", iState);
        OSCL_LEAVE(OsclErrInvalidState);
        return -1;
    }

    return AddCmdToQueue(AI_CMD_PAUSE, aContext);
}

////////////////////////////////////////////////////////////////////////////
PVMFCommandId AndroidAudioInputA2DP::Flush(const OsclAny* aContext)
{
    LOGV("Flush");
    if(iState != STATE_STARTED || iState != STATE_PAUSED)
    {
        LOGE("Flush: Invalid state (%d)", iState);
        OSCL_LEAVE(OsclErrInvalidState);
        return -1;
    }

    return AddCmdToQueue(AI_CMD_FLUSH, aContext);
}

////////////////////////////////////////////////////////////////////////////
PVMFCommandId AndroidAudioInputA2DP::Reset(const OsclAny* aContext)
{
    LOGV("Reset");
    return AddCmdToQueue(AI_CMD_RESET, aContext);
}

////////////////////////////////////////////////////////////////////////////
PVMFCommandId AndroidAudioInputA2DP::DiscardData(PVMFTimestamp aTimestamp, const OsclAny* aContext)
{
    OSCL_UNUSED_ARG(aContext);
    OSCL_UNUSED_ARG(aTimestamp);
    OSCL_LEAVE(OsclErrNotSupported);
    return -1;
}

PVMFCommandId AndroidAudioInputA2DP::DiscardData(const OsclAny* aContext)
{
    OSCL_UNUSED_ARG(aContext);
    OSCL_LEAVE(OsclErrNotSupported);
    return -1;
}


////////////////////////////////////////////////////////////////////////////
PVMFCommandId AndroidAudioInputA2DP::Stop(const OsclAny* aContext)
{
    LOGV("Stop %p", this);
    if(iState != STATE_STARTED && iState != STATE_PAUSED)
    {
        LOGE("Stop: Invalid state (%d)", iState);
        OSCL_LEAVE(OsclErrInvalidState);
        return -1;
    }

    return AddCmdToQueue(AI_CMD_STOP, aContext);
}

////////////////////////////////////////////////////////////////////////////
void AndroidAudioInputA2DP::ThreadLogon()
{
    LOGV("ThreadLogon %p", this);
    if(!iThreadLoggedOn)
    {
        AddToScheduler();
        iThreadLoggedOn = true;
    }
}

////////////////////////////////////////////////////////////////////////////
void AndroidAudioInputA2DP::ThreadLogoff()
{
    LOGV("ThreadLogoff");
    if(iThreadLoggedOn)
    {
        RemoveFromScheduler();
        iThreadLoggedOn = false;
    }
}

////////////////////////////////////////////////////////////////////////////
PVMFCommandId AndroidAudioInputA2DP::CancelAllCommands( const OsclAny* aContext)
{
    OSCL_UNUSED_ARG(aContext);
    OSCL_LEAVE(OsclErrNotSupported);
    return -1;
}

////////////////////////////////////////////////////////////////////////////
PVMFCommandId AndroidAudioInputA2DP::CancelCommand( PVMFCommandId aCmdId, const OsclAny* aContext)
{
    OSCL_UNUSED_ARG(aCmdId);
    OSCL_UNUSED_ARG(aContext);
    OSCL_LEAVE(OsclErrNotSupported);
    return -1;
}

////////////////////////////////////////////////////////////////////////////
void AndroidAudioInputA2DP::setObserver(PvmiConfigAndCapabilityCmdObserver* aObserver)
{
    OSCL_UNUSED_ARG(aObserver);
}

////////////////////////////////////////////////////////////////////////////
PVMFStatus AndroidAudioInputA2DP::getParametersSync(PvmiMIOSession session,
        PvmiKeyType identifier,
        PvmiKvp*& parameters,
        int& num_parameter_elements,
        PvmiCapabilityContext context)
{
    LOGV("getParametersSync(%s)", identifier);
    OSCL_UNUSED_ARG(session);
    OSCL_UNUSED_ARG(context);

    parameters = NULL;
    num_parameter_elements = 0;
    PVMFStatus status = PVMFFailure;

    if( pv_mime_strcmp(identifier, OUTPUT_FORMATS_CAP_QUERY) == 0 ||
            pv_mime_strcmp(identifier, OUTPUT_FORMATS_CUR_QUERY) == 0)
    {
        // No. of Supported audio format types
        num_parameter_elements = 1;
        status = AllocateKvp(parameters, (PvmiKeyType)OUTPUT_FORMATS_VALTYPE, num_parameter_elements);
        if(status != PVMFSuccess)
        {
            LOGE("AndroidAudioInputA2DP::getParametersSync() OUTPUT_FORMATS_VALTYPE AllocateKvp failed");
            return status;
        }
        else
        {
            // Supported audio format types
            parameters[0].value.pChar_value = (char*)PVMF_MIME_PCM16;
        }
    }
    else if(pv_mime_strcmp(identifier, OUTPUT_TIMESCALE_CUR_QUERY) == 0)
    {
        num_parameter_elements = 1;
        status = AllocateKvp(parameters, (PvmiKeyType)OUTPUT_TIMESCALE_CUR_VALUE, num_parameter_elements);
        if(status != PVMFSuccess)
        {
            LOGE("AndroidAudioInputA2DP::getParametersSync() OUTPUT_TIMESCALE_CUR_VALUE AllocateKvp failed");
            return status;
        }

        // XXX is it okay to hardcode this as the timescale?
        parameters[0].value.uint32_value = 1000;
    }
    else if (pv_mime_strcmp(identifier, AUDIO_OUTPUT_SAMPLING_RATE_CUR_QUERY) == 0)
    {
        num_parameter_elements = 1;
        status = AllocateKvp(parameters, (PvmiKeyType)AUDIO_OUTPUT_SAMPLING_RATE_CUR_QUERY, num_parameter_elements);
        if (status != PVMFSuccess)
        {
            LOGE("AndroidAudioInputA2DP::getParametersSync() AUDIO_OUTPUT_SAMPLING_RATE_CUR_QUERY AllocateKvp failed");
            return status;
        }

        parameters[0].value.uint32_value = iAudioSamplingRate;
    }
    else if (pv_mime_strcmp(identifier, AUDIO_OUTPUT_NUM_CHANNELS_CUR_QUERY) == 0)
    {
        num_parameter_elements = 1;
        status = AllocateKvp(parameters, (PvmiKeyType)AUDIO_OUTPUT_NUM_CHANNELS_CUR_QUERY, num_parameter_elements);
        if (status != PVMFSuccess)
        {
            LOGE("AndroidAudioInputA2DP::getParametersSync() AUDIO_OUTPUT_NUM_CHANNELS_CUR_QUERY AllocateKvp failed");
            return status;
        }
        parameters[0].value.uint32_value = iAudioNumChannels;
    }

    return status;
}

////////////////////////////////////////////////////////////////////////////
PVMFStatus AndroidAudioInputA2DP::releaseParameters(PvmiMIOSession session,
        PvmiKvp* parameters,
        int num_elements)
{
    OSCL_UNUSED_ARG(session);
    OSCL_UNUSED_ARG(num_elements);

    if(parameters)
    {
        iAlloc.deallocate((OsclAny*)parameters);
        return PVMFSuccess;
    }
    else
    {
        LOGE("Attempt to release NULL parameters");
        return PVMFFailure;
    }
}

////////////////////////////////////////////////////////////////////////////
void AndroidAudioInputA2DP::createContext(PvmiMIOSession session, PvmiCapabilityContext& context)
{
    OSCL_UNUSED_ARG(session);
    OSCL_UNUSED_ARG(context);
}

////////////////////////////////////////////////////////////////////////////
void AndroidAudioInputA2DP::setContextParameters(PvmiMIOSession session,
        PvmiCapabilityContext& context,
        PvmiKvp* parameters, int num_parameter_elements)
{
    OSCL_UNUSED_ARG(session);
    OSCL_UNUSED_ARG(context);
    OSCL_UNUSED_ARG(parameters);
    OSCL_UNUSED_ARG(num_parameter_elements);
}

////////////////////////////////////////////////////////////////////////////
void AndroidAudioInputA2DP::DeleteContext(PvmiMIOSession session, PvmiCapabilityContext& context)
{
    OSCL_UNUSED_ARG(session);
    OSCL_UNUSED_ARG(context);
}

////////////////////////////////////////////////////////////////////////////
void AndroidAudioInputA2DP::setParametersSync(PvmiMIOSession session, PvmiKvp* parameters,
        int num_elements, PvmiKvp*& ret_kvp)
{
    LOGV("setParametersSync");
    OSCL_UNUSED_ARG(session);
    PVMFStatus status = PVMFSuccess;
    ret_kvp = NULL;

    for(int32 i = 0; i < num_elements; i++)
    {
        status = VerifyAndSetParameter(&(parameters[i]), true);
        if(status != PVMFSuccess)
        {
            LOGE("VerifyAndSetParameter failed");
            ret_kvp = &(parameters[i]);
            OSCL_LEAVE(OsclErrArgument);
        }
    }
}

////////////////////////////////////////////////////////////////////////////
PVMFCommandId AndroidAudioInputA2DP::setParametersAsync(PvmiMIOSession session,
        PvmiKvp* parameters,
        int num_elements,
        PvmiKvp*& ret_kvp,
        OsclAny* context)
{
    OSCL_UNUSED_ARG(session);
    OSCL_UNUSED_ARG(parameters);
    OSCL_UNUSED_ARG(num_elements);
    OSCL_UNUSED_ARG(ret_kvp);
    OSCL_UNUSED_ARG(context);
    OSCL_LEAVE(OsclErrNotSupported);
    return -1;
}

////////////////////////////////////////////////////////////////////////////
uint32 AndroidAudioInputA2DP::getCapabilityMetric (PvmiMIOSession session)
{
    OSCL_UNUSED_ARG(session);
    return 0;
}

////////////////////////////////////////////////////////////////////////////
PVMFStatus AndroidAudioInputA2DP::verifyParametersSync(PvmiMIOSession session,
        PvmiKvp* parameters, int num_elements)
{
    OSCL_UNUSED_ARG(session);
    OSCL_UNUSED_ARG(parameters);
    OSCL_UNUSED_ARG(num_elements);
    return PVMFErrNotSupported;
}

////////////////////////////////////////////////////////////////////////////
bool AndroidAudioInputA2DP::setAudioSamplingRate(int32 iSamplingRate)
{
    LOGV("AndroidAudioInputA2DP::setAudioSamplingRate( %d )", iSamplingRate);

    if (iSamplingRate == 0)
    {
        // Setting sampling rate to zero will cause a crash
        LOGE("AndroidAudioInputA2DP::setAudioSamplingRate() invalid sampling rate.  Return false.");
        return false;
    }

    iAudioSamplingRate = iSamplingRate;
    LOGV("AndroidAudioInputA2DP::setAudioSamplingRate() iAudioSamplingRate %d set", iAudioSamplingRate);
    return true;
}
////////////////////////////////////////////////////////////////////////////
bool AndroidAudioInputA2DP::setAudioNumChannels(int32 iNumChannels)
{
    LOGV("AndroidAudioInputA2DP::setAudioNumChannels( %d )", iNumChannels);

    iAudioNumChannels = iNumChannels;
    LOGV("AndroidAudioInputA2DP::setAudioNumChannels() iAudioNumChannels %d set", iAudioNumChannels);
    return true;
}
////////////////////////////////////////////////////////////////////////////
bool AndroidAudioInputA2DP::setAudioFormatType(char *iAudioFormat)
{
	LOGV("In AndroidAudioInputA2DP::setAudioFormatType");
	if(iAudioSource != AUDIO_SOURCE_FM_RX_A2DP)
	{
		LOGE("Returning failure because the format type does not support this input source %x", iAudioSource);
		return false;
	}
	return true;
}

////////////////////////////////////////////////////////////////////////////
bool AndroidAudioInputA2DP::setAudioSource(uint32 iSource)
{
    iAudioSource = iSource;
    LOGV("AndroidAudioInputA2DP::setAudioSource() iAudioSource %d set", iAudioSource);
    return true;
}

////////////////////////////////////////////////////////////////////////////
//                            Private methods
////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////
void AndroidAudioInputA2DP::Run()
{
    if(!iCmdQueue.empty())
    {
        AndroidAudioInputCmd cmd = iCmdQueue[0];
        iCmdQueue.erase(iCmdQueue.begin());

        switch(cmd.iType)
        {

        case AI_CMD_INIT:
            DoRequestCompleted(cmd, DoInit());
            break;

        case AI_CMD_START:
            DoRequestCompleted(cmd, DoStart());
            break;

        case AI_CMD_RESET:
            DoRequestCompleted(cmd, DoReset());
            break;

        case AI_CMD_STOP:
            DoRequestCompleted(cmd, DoStop());
            break;

        case AI_CMD_QUERY_UUID:
        case AI_CMD_QUERY_INTERFACE:
            DoRequestCompleted(cmd, PVMFSuccess);
            break;

        case AI_CMD_CANCEL_ALL_COMMANDS:
        case AI_CMD_CANCEL_COMMAND:
            DoRequestCompleted(cmd, PVMFFailure);
            break;

        default:
            break;
        }
    }

    if ((iState == STATE_STARTED) && (iStartCmd.iType == AI_CMD_START)) {
            LOGV("First frame received, Complete the start");
            // First frame is received now complete Start
            DoRequestCompleted(iStartCmd, PVMFSuccess);

            // Set the iStartCmd type to be invalid now
            iStartCmd.iType = AI_INVALID_CMD;
	}
    if(!iCmdQueue.empty())
    {
        // Run again if there are more things to process
        RunIfNotReady();
    }
}

////////////////////////////////////////////////////////////////////////////
PVMFCommandId AndroidAudioInputA2DP::AddCmdToQueue(AndroidAudioInputCmdType aType,
        const OsclAny* aContext, OsclAny* aData1)
{
    if(aType == AI_DATA_WRITE_EVENT)
        OSCL_LEAVE(OsclErrArgument);

    AndroidAudioInputCmd cmd;
    cmd.iType = aType;
    cmd.iContext = OSCL_STATIC_CAST(OsclAny*, aContext);
    cmd.iData1 = aData1;
    cmd.iId = iCmdIdCounter;
    ++iCmdIdCounter;
    iCmdQueue.push_back(cmd);
    RunIfNotReady();
    return cmd.iId;
}

////////////////////////////////////////////////////////////////////////////
void AndroidAudioInputA2DP::AddDataEventToQueue(uint32 aMicroSecondsToEvent)
{
    // If not added to the scheduler just return
    if (!IsAdded())
        return;
    AndroidAudioInputCmd cmd;
    cmd.iType = AI_DATA_WRITE_EVENT;
    iCmdQueue.push_back(cmd);
    RunIfNotReady(aMicroSecondsToEvent);
}

////////////////////////////////////////////////////////////////////////////
void AndroidAudioInputA2DP::DoRequestCompleted(const AndroidAudioInputCmd& aCmd, PVMFStatus aStatus, OsclAny* aEventData)
{
    LOGV("DoRequestCompleted(%d, %d) this %p", aCmd.iId, aStatus, this);
    if ((aStatus == PVMFPending) && (aCmd.iType == AI_CMD_START)) {
        LOGV("Start is pending, Return here, wait for success or failure");
        // Copy the command
        iStartCmd = aCmd;
        return;
    }
    PVMFCmdResp response(aCmd.iId, aCmd.iContext, aStatus, aEventData);

    for(uint32 i = 0; i < iObservers.size(); i++)
        iObservers[i]->RequestCompleted(response);
}

////////////////////////////////////////////////////////////////////////////
PVMFStatus AndroidAudioInputA2DP::DoInit()
{
    LOGV("DoInit");
    // Create memory pool for the media data, using the maximum frame size found earlier
    int32 err = 0;
    OSCL_TRY(err,
        if(iMediaBufferMemPool)
        {
            OSCL_DELETE(iMediaBufferMemPool);
            iMediaBufferMemPool = NULL;
        }
        iMediaBufferMemPool = OSCL_NEW(OsclMemPoolFixedChunkAllocator, (10));
        if(!iMediaBufferMemPool) {
            LOGV("AndroidAudioInputA2DP::DoInit() unable to create memory pool.  Return PVMFErrNoMemory.");
            OSCL_LEAVE(OsclErrNoMemory);
        }
    );
    OSCL_FIRST_CATCH_ANY(err, return PVMFErrNoMemory);
    allocateBufferPool();
    iState = STATE_INITIALIZED;
    return PVMFSuccess;
}

////////////////////////////////////////////////////////////////////////////
PVMFStatus AndroidAudioInputA2DP::DoStart()
{
    LOGV("DoStart");

    // Set the clock state observer
    if (iAuthorClock) {
        iAuthorClock->ConstructMediaClockNotificationsInterface(iClockNotificationsInf, *this);
        if (iClockNotificationsInf == NULL) {
             return PVMFErrNoMemory;
        }
        iClockNotificationsInf->SetClockStateObserver(*this);
    }
    iAudioThreadStartLock->lock();
    iAudioThreadStarted = false;

    /* Make the thread joinable */
    OsclProcStatus::eOsclProcError ret = AudioInput_Thread.Create(
            (TOsclThreadFuncPtr)start_audin_thread_func, 0,
            (TOsclThreadFuncArg)this, Start_on_creation, true);

    if ( OsclProcStatus::SUCCESS_ERROR != ret)
    { // thread creation failed
        LOGE("Failed to create thread (%d)", ret);
        iAudioThreadStartLock->unlock();
        return PVMFFailure;
    }
    /*A2DP thread*/
    OsclProcStatus::eOsclProcError ret2 = AudioA2DP_Thread.Create(
            (TOsclThreadFuncPtr)start_auda2dp_thread_func, 0,
            (TOsclThreadFuncArg)this, Start_on_creation, true);
    if ( OsclProcStatus::SUCCESS_ERROR != ret2)
    { // thread creation failed
        LOGE("Failed to create A2DP thread (%d)", ret);
        iAudioThreadStartLock->unlock();
        return PVMFFailure;
    }

    // wait for thread to set up AudioRecord
    while (!iAudioThreadStarted)
        iAudioThreadStartCV->wait(*iAudioThreadStartLock);

    status_t startResult = iAudioThreadStartResult;
    iAudioThreadStartLock->unlock();

    if (startResult != NO_ERROR)
        return PVMFFailure; // thread failed to set up AudioRecord

    iState = STATE_STARTED;
    AddDataEventToQueue(0);
    return PVMFSuccess;
}

////////////////////////////////////////////////////////////////////////////
int AndroidAudioInputA2DP::start_audin_thread_func(TOsclThreadFuncArg arg)
{
    prctl(PR_SET_NAME, (unsigned long) "audio in", 0, 0, 0);
    sp<AndroidAudioInputA2DP> obj =  (AndroidAudioInputA2DP *)arg;
    return obj->audin_thread_func();
}

////////////////////////////////////////////////////////////////////////////
PVMFStatus AndroidAudioInputA2DP::DoReset()
{
    LOGV("DoReset");
    if(iAudioThreadStarted ){
        iAudioThreadSem->Signal();
        iAudioThreadTermSem->Wait();
        int exitCode = 0;
        OsclProcStatus::eOsclProcError ret = AudioInput_Thread.Terminate((OsclAny*)exitCode);
        /* No need to check for exitCode, as it is an unused argument in Terminate() */
        if ( OsclProcStatus::SUCCESS_ERROR != ret)
        {
            LOGE("Failed to terminate the thread : audio in");
        }
        iAudioThreadStarted = false;
        iAudioA2DPThreadSem->Signal();
        iAudioA2DPThreadTermSem->Wait();
        exitCode = 0;
        ret = AudioA2DP_Thread.Terminate((OsclAny*)exitCode);

        if ( OsclProcStatus::SUCCESS_ERROR != ret)
        {
            LOGE("Failed to terminate the thread : audio in");
        }
    }
    while(!iOSSBTRequestQueue.empty())
    {
    OSSRequest FMDATA(iOSSBTRequestQueue[0].iData,kBufferSize,-1,0,0);
          iMediaBufferMemPool->deallocate(FMDATA.iData);
        iOSSBTRequestQueue.erase(&iOSSBTRequestQueue[0]);
    }

    while(!iOSSBTResponseQueue.empty())
    {
        OSSRequest FMDATA(iOSSBTRequestQueue[0].iData,kBufferSize,-1,0,0);
        iMediaBufferMemPool->deallocate(FMDATA.iData);
        iOSSBTResponseQueue.erase(&iOSSBTResponseQueue[0]);
    }
    return PVMFSuccess;
}

////////////////////////////////////////////////////////////////////////////
PVMFStatus AndroidAudioInputA2DP::DoStop()
{
    LOGV("DoStop");
    // Remove and destroy the clock state observer
    RemoveDestroyClockStateObs();
    iExitAudioThread = true;
    iExitA2DPThread = true;
    iState = STATE_STOPPED;
    if(iAudioThreadStarted ){
        iAudioThreadSem->Signal();
        iAudioThreadTermSem->Wait();
        int exitCode = 0;
        OsclProcStatus::eOsclProcError ret = AudioInput_Thread.Terminate((OsclAny*)exitCode);
        /* No need to check for exitCode, as it is an unused argument in Terminate() */
        if ( OsclProcStatus::SUCCESS_ERROR != ret)
        {
            LOGE("Failed to terminate the thread : audio in");
        }
        iAudioThreadStarted = false;
        iAudioA2DPThreadSem->Signal();
        iAudioA2DPThreadTermSem->Wait();
        exitCode = 0;
        ret = AudioA2DP_Thread.Terminate((OsclAny*)exitCode);
        if ( OsclProcStatus::SUCCESS_ERROR != ret)
        {
            LOGE("Failed to terminate the thread : FM A2DP");
        }
    }
    return PVMFSuccess;
}

int AndroidAudioInputA2DP::allocateBufferPool()
{
    LOGV("allocateBufferPool");
    //allocate mem for the audio capture thread
    int32 error = 0;
    uint8* data = NULL;
    iOSSBTRequestQueueLock.Lock();
	for (int i = 0; i < BUFFER_POOL_SIZE; ++i) {
		  OSCL_TRY(error,
		  data = (uint8*)iMediaBufferMemPool->allocate(kBufferSize);
		  );
		 if (error != OsclErrNone || !data )
		 {
			LOGE("Could not allocate Request queue buffer pool");
		 }
		 else{
			LOGV("adding data to request queue");
			OSSRequest reqBSQ(data,kBufferSize,-1,-1,0);
			iOSSBTRequestQueue.push_back(reqBSQ);
		}
	}
	iOSSBTRequestQueueLock.Unlock();
	if (iAudioThreadStarted == true)
	{
		LOGV("Signal to audio thread to read data");
		iAudioThreadSem->Signal();   //signal to audio read thread audin_thread_func
	}
    return 0;
}

////////////////////////////////////////////////////////////////////////////
int AndroidAudioInputA2DP::audin_thread_func() {
    // setup audio record session

    iAudioThreadStartLock->lock();
    LOGV("create Audio thread");
    int32 nFrameSize = sizeof(int16); // Default PCM_16_BIT frame size.
    if(iAudioSource == android::AudioSystem::DEVICE_IN_FM_RX_A2DP){
        iAudioFormatType = android::AudioSystem::PCM_16_BIT;
        kBufferSize = 2048;
    }
    AudioRecord* record;
    record = new AudioRecord(
                     iAudioSource, iAudioSamplingRate,
                     iAudioFormatType,
                     (iAudioNumChannels > 1) ? AudioSystem::CHANNEL_IN_STEREO : AudioSystem::CHANNEL_IN_MONO,
                     4*kBufferSize/iAudioNumChannels/nFrameSize, 0);
    LOGI("AudioRecord created %p, this %p", record, this);
    status_t res = record->initCheck();
    if (res == NO_ERROR)
        res = record->start();

    iAudioThreadStartResult = res;
    iAudioThreadStarted = true;

    iAudioThreadStartCV->signal();
    iAudioThreadStartLock->unlock();

    if (res == NO_ERROR) {
        int numOfBytes = 0;
        while (!iExitAudioThread) {
            iOSSBTRequestQueueLock.Lock();
            if(iOSSBTRequestQueue.empty()){
            iOSSBTRequestQueueLock.Unlock();
            LOGV("no raw food wait");
            iAudioThreadSem->Wait();
            LOGV("signal from main thread");
            continue;
            }
            uint8* data =   iOSSBTRequestQueue[0].iData;
            iOSSBTRequestQueue.erase(&iOSSBTRequestQueue[0]);
            iOSSBTRequestQueueLock.Unlock();
            numOfBytes = record->read(data, kBufferSize);
            LOGV("read %d bytes", numOfBytes);
            if (numOfBytes <= 0)
                break;
            OSSRequest FMDATA(data,numOfBytes,-1,0,0);
            iOSSBTResponseQueueLock.Lock();
            iOSSBTResponseQueue.push_back(FMDATA);
            iOSSBTResponseQueueLock.Unlock();
            iAudioA2DPThreadSem->Signal();
            // Queue the next data event
            OsclAny* P = NULL;
            iWriteCompleteAO->ReceiveEvent(P);
        }
        record->stop();
    }
    LOGV("delete record %p, this %p", record, this);
    delete record;
    iAudioThreadTermSem->Signal();
    return 0;
}

////////////////////////////////////////////////////////////////////////////
PVMFStatus AndroidAudioInputA2DP::AllocateKvp(PvmiKvp*& aKvp, PvmiKeyType aKey, int32 aNumParams)
{
    uint8* buf = NULL;
    uint32 keyLen = oscl_strlen(aKey) + 1;
    int32 err = 0;

    OSCL_TRY(err,
            buf = (uint8*)iAlloc.allocate(aNumParams * (sizeof(PvmiKvp) + keyLen));
            if(!buf) {
                LOGE("Failed to allocate memory for Kvp");
                OSCL_LEAVE(OsclErrNoMemory);
            }
            );
    OSCL_FIRST_CATCH_ANY(err, LOGE("allocation error"); return PVMFErrNoMemory;);

    int32 i = 0;
    PvmiKvp* curKvp = aKvp = new (buf) PvmiKvp;
    buf += sizeof(PvmiKvp);
    for(i = 1; i < aNumParams; i++)
    {
        curKvp += i;
        curKvp = new (buf) PvmiKvp;
        buf += sizeof(PvmiKvp);
    }

    for(i = 0; i < aNumParams; i++)
    {
        aKvp[i].key = (char*)buf;
        oscl_strncpy(aKvp[i].key, aKey, keyLen);
        buf += keyLen;
    }

    return PVMFSuccess;
}

////////////////////////////////////////////////////////////////////////////
PVMFStatus AndroidAudioInputA2DP::VerifyAndSetParameter(PvmiKvp* aKvp, bool aSetParam)
{
    if(!aKvp)
    {
        LOGE("aKvp is a NULL pointer");
        return PVMFFailure;
    }

    if(pv_mime_strcmp(aKvp->key, OUTPUT_FORMATS_VALTYPE) == 0)
    {
        if(pv_mime_strcmp(aKvp->value.pChar_value, PVMF_MIME_PCM16) == 0)
        {
            iAudioFormatType = android::AudioSystem::PCM_16_BIT;
            return PVMFSuccess;
        }
        else
        {
            LOGE("unsupported format (%s) for key %s", aKvp->value.pChar_value, aKvp->key);
            return PVMFFailure;
        }
    }
    else if (pv_mime_strcmp(aKvp->key, PVMF_AUTHORING_CLOCK_KEY) == 0)
    {
        LOGV("AndroidAudioInputA2DP::VerifyAndSetParameter() PVMF_AUTHORING_CLOCK_KEY value %p", aKvp->value.key_specific_value);
        if( (NULL == aKvp->value.key_specific_value) && ( iAuthorClock ) )
        {
            RemoveDestroyClockStateObs();
        }
        iAuthorClock = (PVMFMediaClock*) aKvp->value.key_specific_value;
        return PVMFSuccess;
    }

    LOGE("unsupported parameter: %s", aKvp->key);
    return PVMFFailure;
}


void AndroidAudioInputA2DP::NotificationsInterfaceDestroyed()
{
    iClockNotificationsInf = NULL;
}


void AndroidAudioInputA2DP::ClockStateUpdated()
{
    PVMFMediaClock::PVMFMediaClockState iClockState = iAuthorClock->GetState();
    uint32 currentTime = 0;
    bool tmpbool = false;
    iAuthorClock->GetCurrentTime32(currentTime, tmpbool, PVMF_MEDIA_CLOCK_MSEC);
    LOGV("ClockStateUpdated State %d Current clock value %d", iClockState, currentTime);
}


void AndroidAudioInputA2DP::RemoveDestroyClockStateObs()
{
    if (iAuthorClock != NULL) {
        if (iClockNotificationsInf != NULL) {
            iClockNotificationsInf->RemoveClockStateObserver(*this);
            iAuthorClock->DestroyMediaClockNotificationsInterface(iClockNotificationsInf);
            iClockNotificationsInf = NULL;
        }
    }
}


#undef LOG_TAG
#define LOG_TAG "FMA2DPthread"

////////////////////////////////////////////////////////////////////////////
int AndroidAudioInputA2DP::start_auda2dp_thread_func(TOsclThreadFuncArg arg)
{
    prctl(PR_SET_NAME, (unsigned long) "audio a2dp", 0, 0, 0);
    sp<AndroidAudioInputA2DP> obj =  (AndroidAudioInputA2DP *)arg;
    return obj->auda2dp_thread_func();
}

////////////////////////////////////////////////////////////////////////////
int AndroidAudioInputA2DP::auda2dp_thread_func()
{
    int32 nFrameSize = sizeof(int16);
    kBufferSize = 2048;
    LOGV("Creating AudioTrack object");
    mpAudioTrack= new AudioTrack(
                AudioSystem::FM,
                iAudioSamplingRate,
                android::AudioSystem::PCM_16_BIT,
                (iAudioNumChannels == 2) ? AudioSystem::CHANNEL_OUT_STEREO : AudioSystem::CHANNEL_OUT_MONO,
                4*kBufferSize/nFrameSize);
    if ((mpAudioTrack == 0) ) {
        LOGE("Unable to create audio track");
        delete mpAudioTrack;
        iAudioA2DPThreadTermSem->Signal();
        return 0;
    }
    status_t res = mpAudioTrack->initCheck();
    if (res == NO_ERROR) {
        mpAudioTrack->setVolume(1, 1);
        mpAudioTrack->start();
        while (!iExitA2DPThread) {
            iOSSBTResponseQueueLock.Lock();
            if(iOSSBTResponseQueue.empty() || !iAudioThreadStarted)
            {
                iOSSBTResponseQueueLock.Unlock();
                LOGV("Hungry");
                iAudioA2DPThreadSem->Wait();
                LOGV("turn to eat");
                continue;
            }
            uint8* data = iOSSBTResponseQueue[0].iData;
            size_t noofbytes = iOSSBTResponseQueue[0].iDataLen;
            ssize_t ret = mpAudioTrack->write(data, noofbytes);
            OSSRequest FMDATA(data,noofbytes,-1,0,0);
            iOSSBTResponseQueue.erase(&iOSSBTResponseQueue[0]);
            iOSSBTResponseQueueLock.Unlock();
            iOSSBTRequestQueueLock.Lock();
            if(iOSSBTRequestQueue.empty()) {
                LOGV("auda2dp_thread_func(): Signal to audio thread to read data");
                iAudioThreadSem->Signal();
            }
            iOSSBTRequestQueue.push_back(FMDATA);
            iOSSBTRequestQueueLock.Unlock();
        }
    }
    LOGV("releasing the a2dp thread");
    iAudioA2DPThreadTermSem->Signal();
    return 0;
}


