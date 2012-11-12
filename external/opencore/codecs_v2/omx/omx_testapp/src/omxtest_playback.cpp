/* ------------------------------------------------------------------
 * Copyright (C) 1998-2010 PacketVideo
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 * -------------------------------------------------------------------
 */
#include "omxdectest.h"
#include "oscl_mem.h"

#include <fcntl.h>
#include <sys/ioctl.h>
#include <err.h>

#include <linux/ioctl.h>
#include <linux/msm_audio.h>

#define ENABLE_ROUTING 1
#define DISABLE_ROUTING 0

#define E_IOCTL -1


OMX_BOOL OmxDecTestPlayback::PrepareAudioComponent()
{
    OMX_ERRORTYPE Err;
    iInputParameters.inPtr = NULL;
    iInputParameters.cComponentRole = NULL;
    iInputParameters.inBytes = 0;

    OMX_U32 ii;
    OMX_S32 ReadCount;
    OMX_S32 Size;
    OMX_S32 TempTimeStamp;
    OMX_BOOL Status = OMX_TRUE;

    iOutputParameters = (AudioOMXConfigParserOutputs*) oscl_malloc(sizeof(AudioOMXConfigParserOutputs));

    if (NULL != iRole)
    {
        //Determine the component first & then get the handle
        OMX_U32 NumComps = 0;
        OMX_STRING* pCompOfRole;

        // call once to find out the number of components that can fit the role
        Err = OMX_MasterGetComponentsOfRole(iRole, &NumComps, NULL);

        pCompOfRole = (OMX_STRING*) oscl_malloc(NumComps * sizeof(OMX_STRING));

        for (ii = 0; ii < NumComps; ii++)
        {
            pCompOfRole[ii] = (OMX_STRING) oscl_malloc(PV_OMX_MAX_COMPONENT_NAME_LENGTH * sizeof(OMX_U8));
            if (NULL == pCompOfRole[ii])
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "OmxComponentDecTest::PrepareAudioComponent() - ERROR allocating memoy to pCompOfRole for index %d", ii));
                ipAppPriv->Handle = NULL;

                oscl_free(pCompOfRole);
                pCompOfRole = NULL;
                return OMX_FALSE;
            }
        }

        // call 2nd time to get the component names
        Err = OMX_MasterGetComponentsOfRole(iRole, &NumComps, (OMX_U8**) pCompOfRole);

        for (ii = 0; ii < NumComps; ii++)
        {
            iInputParameters.cComponentName = pCompOfRole[ii];

            if (OMX_TRUE == Status)
            {
                // try to create component
                Err = OMX_MasterGetHandle(&ipAppPriv->Handle, (OMX_STRING) pCompOfRole[ii], (OMX_PTR) this, iCallbacks->getCallbackStruct());
                // if successful, no need to continue
                if ((OMX_ErrorNone == Err) && (NULL != ipAppPriv->Handle))
                {
                    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG, (0, "OmxComponentDecTest::PrepareAudioComponent() - Got Handle for the component %s", pCompOfRole[ii]));
                }
                else
                {
                    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "OmxComponentDecTest::PrepareAudioComponent() - ERROR, Cannot get component %s handle, try another if possible", pCompOfRole[ii]));
                    continue;
                }

                OMX_BOOL Result;
                Result = SetAudioParameters();

                if (OMX_FALSE == Result)
                {
                    OMX_MasterFreeHandle(ipAppPriv->Handle);
                    ipAppPriv->Handle = NULL;
                    continue;
                }

                // found a component, and it passed all tests. no need to continue.
                break;
            }
            else
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG, (0, "OmxComponentDecTest::PrepareAudioComponent() - Error returned by OMX_MasterConfigParser API"));
                ipAppPriv->Handle = NULL;
            }
        }

        // whether successful or not, need to free CompOfRoles
        for (ii = 0; ii < NumComps; ii++)
        {
            oscl_free(pCompOfRole[ii]);
            pCompOfRole[ii] = NULL;
        }
        oscl_free(pCompOfRole);
        pCompOfRole = NULL;

        // whether successful or not, need to free InputParameters
        if (iInputParameters.inPtr)
        {
            oscl_free(iInputParameters.inPtr);
            iInputParameters.inPtr = NULL;
        }

        // check if there was a problem
        if ((Err != OMX_ErrorNone) || (NULL == ipAppPriv->Handle))
        {
            return OMX_FALSE;
        }
    }
    return OMX_TRUE;
}

OMX_BOOL OmxDecTestPlayback::SetAudioParameters()
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "OmxDecTestPlayback::SetAudioParameters In"));

    OMX_ERRORTYPE Err;
    uint32 NumPorts;
    uint32 ii;

    AudioOMXConfigParserOutputs *pOutputParameters;
    pOutputParameters = (AudioOMXConfigParserOutputs *)iOutputParameters;

    //This will initialize the size and version of the iPortInit structure
    INIT_GETPARAMETER_STRUCT(OMX_PORT_PARAM_TYPE, iPortInit);

    Err = OMX_GetParameter(ipAppPriv->Handle, OMX_IndexParamAudioInit, &iPortInit);
    NumPorts = iPortInit.nPorts;

    iInputPortIndex = 0;
    iOutputPortIndex = 1;

    // now get input parameters
    INIT_GETPARAMETER_STRUCT(OMX_PARAM_PORTDEFINITIONTYPE, iParamPort);

    //Input port
    iParamPort.nPortIndex = iInputPortIndex;
    Err = OMX_GetParameter(ipAppPriv->Handle, OMX_IndexParamPortDefinition, &iParamPort);
    if (Err != OMX_ErrorNone)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                        (0, "OmxDecTestPlayback::SetAudioParameters() - ERROR, Problem negotiating input port params %d, OUT", iInputPortIndex));
        return OMX_FALSE;
    }

    // preset the number of input buffers
    iInBufferCount = iParamPort.nBufferCountActual;  // use the value provided by component
    iInBufferSize = iParamPort.nBufferSize;

    if (0 == oscl_strcmp(iFormat, "AMR"))
    {
        iParamPort.nBufferCountActual = iParamPort.nBufferCountMin + 3;
    }
    else if (0 == oscl_strcmp(iFormat, "AAC"))
    {
        iParamPort.nBufferCountActual = iParamPort.nBufferCountMin + 5;
    }

    iInBufferCount = iParamPort.nBufferCountActual;
    Err = OMX_SetParameter(ipAppPriv->Handle, OMX_IndexParamPortDefinition, &iParamPort);
    if (Err != OMX_ErrorNone)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                        (0, "OmxDecTestPlayback::SetAudioParameters() - ERROR, Problem negotiating input port params %d, OUT", iInputPortIndex));
        return OMX_FALSE;return OMX_FALSE;
    }

    //Port 1 for output port
    INIT_GETPARAMETER_STRUCT(OMX_PARAM_PORTDEFINITIONTYPE, iParamPort);
    iParamPort.nPortIndex = iOutputPortIndex;

    Err = OMX_GetParameter(ipAppPriv->Handle, OMX_IndexParamPortDefinition, &iParamPort);
    if (Err != OMX_ErrorNone)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                        (0, "OmxDecTestPlayback::SetAudioParameters() - ERROR, Problem negotiating output port params %d, OUT", iOutputPortIndex));
        return OMX_FALSE;
    }
    // set number of output buffers and the size
    iOutBufferCount = iParamPort.nBufferCountActual;
    iParamPort.nBufferCountActual = iOutBufferCount;
    iOutBufferSize = iParamPort.nBufferSize;

    if (0 == oscl_strcmp(iFormat, "AMR"))
    {
        iParamPort.nBufferCountActual = iParamPort.nBufferCountMin + 1;
    }
    if (0 == oscl_strcmp(iFormat, "AAC"))
    {
        iParamPort.nBufferCountActual = iParamPort.nBufferCountMin + 3;
    }
    iParamPort.nBufferCountActual = iParamPort.nBufferCountMin + 1;
    iOutBufferCount = iParamPort.nBufferCountActual;

    // finalize setting output port parameters
    Err = OMX_SetParameter(ipAppPriv->Handle, OMX_IndexParamPortDefinition, &iParamPort);
    if (Err != OMX_ErrorNone)
    {
       PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                        (0, "OmxDecTestPlayback::SetAudioParameters() - ERROR, Problem negotiating output port params %d, OUT", iOutputPortIndex));
       return OMX_FALSE;
    }

    // Codec specific info set/get: SamplingRate, formats etc.
    if (0 == oscl_strcmp(iFormat, "AMR"))
    {
        OMX_AUDIO_PARAM_AMRTYPE Audio_Amr_Param;
        Audio_Amr_Param.nPortIndex = iInputPortIndex;

        INIT_GETPARAMETER_STRUCT(OMX_AUDIO_PARAM_AACPROFILETYPE, Audio_Amr_Param);

        OMX_GetParameter(ipAppPriv->Handle, OMX_IndexParamAudioAmr, &Audio_Amr_Param);

        Audio_Amr_Param.nPortIndex   = iInputPortIndex;
        Audio_Amr_Param.nChannels    = iNumberOfChannels;
        Audio_Amr_Param.nBitRate	 = iPCMSamplingRate;
        Audio_Amr_Param.eAMRFrameFormat = (OMX_AUDIO_AMRFRAMEFORMATTYPE)iFileType;
        Audio_Amr_Param.eAMRBandMode = (OMX_AUDIO_AMRBANDMODETYPE)iBandMode;

        Err = OMX_SetParameter(ipAppPriv->Handle,OMX_IndexParamAudioAmr,&Audio_Amr_Param);

    }
    else if (0 == oscl_strcmp(iFormat, "AAC"))
    {
        X_AUDIO_PARAM_AACPROFILETYPE Audio_Aac_Param;
        dio_Aac_Param.nPortIndex = iInputPortIndex;

        IT_GETPARAMETER_STRUCT(OMX_AUDIO_PARAM_AACPROFILETYPE, Audio_Aac_Param);

        dio_Aac_Param.nPortIndex   =  iInputPortIndex;
        dio_Aac_Param.nChannels    =  iNumberOfChannels;
        dio_Aac_Param.nBitRate     =  iPCMSamplingRate;
        Audio_Aac_Param.nSampleRate  =  iPCMSamplingRate;
        Audio_Aac_Param.eChannelMode =  OMX_AUDIO_ChannelModeStereo;
        Audio_Aac_Param.eAACProfile = OMX_AUDIO_AACObjectLC;
        Audio_Aac_Param.eAACStreamFormat =  OMX_AUDIO_AACStreamFormatMP2ADTS;

        Err = OMX_SetParameter(ipAppPriv->Handle,OMX_IndexParamAudioAac,&Audio_Aac_Param);
    }

    if (Err != OMX_ErrorNone)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                        (0, "OmxDecTestPlayback::SetAudioParameters() - ERROR, Problem setting Audio parametersfor output port, OUT", iOutputPortIndex));
        return OMX_FALSE;
    }

    return OMX_TRUE;
}


OMX_BOOL OmxDecTestPlayback::OpenPCMDriver()
{
    struct msm_audio_config pcmConfig;
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "OmxDecTestPlayback::OpenPCMDriver() - IN"));

    iPCMDriverFD = open("/dev/msm_pcm_out", O_RDWR);
    if (iPCMDriverFD < 0)
    {
       PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                        (0, "OmxDecTestPlayback::OpenPCMDriver() - ERROR, Cannot open msm_pcm_out, OUT"));
       printf("[ERROR] OmxDecTestPlayback::OpenPCMDriver() -  Cannot open msm_pcm_out device\n");
       return OMX_FALSE;
    }

    // Set sampling rate and num of channels
    if (E_IOCTL == ioctl(iPCMDriverFD, AUDIO_GET_CONFIG, &pcmConfig))
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                        (0, "OmxDecTestPlayback::OpenPCMDriver() - ERROR, getting Audio config, OUT"));
        return OMX_FALSE;
    }

    pcmConfig.sample_rate   = iPCMSamplingRate;
    pcmConfig.channel_count = iNumberOfChannels;

    if (E_IOCTL == ioctl(iPCMDriverFD, AUDIO_SET_CONFIG, &pcmConfig))
    {
       PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                        (0, "OmxDecTestPlayback::OpenPCMDriver() - ERROR, setting Audio config, OUT"));
       return OMX_FALSE;
    }

    if (E_IOCTL == ioctl(iPCMDriverFD, AUDIO_GET_CONFIG, &pcmConfig))
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                        (0, "OmxDecTestPlayback::OpenPCMDriver() - ERROR, getting Audio config, OUT"));
        return OMX_FALSE;
    }

    if (E_IOCTL == ioctl(iPCMDriverFD, AUDIO_GET_SESSION_ID, &iSessionID))
    {
         PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                        (0, "OmxDecTestPlayback::OpenPCMDriver() - ERROR, getting Audio session id, OUT"));
         return OMX_FALSE;
    }

    // open msm_mixer
    iControl = msm_mixer_open("/dev/snd/controlC0", 0);
    if (iControl < 0) {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                        (0, "OmxDecTestPlayback::OpenPCMDriver() - ERROR, Cannot open msm_mixer, OUT"));
        printf("[ERROR] OmxDecTestPlayback::OpenPCMDriver()- Cannot open msm_mixer device\n");
        return OMX_FALSE;
    }

    // set device to stereo receiver and get device id
    strncpy(iDeviceStr,"speaker_stereo_rx",strlen("speaker_stereo_rx"));
    iDeviceID = msm_get_device(iDeviceStr);

    if (msm_en_device(iDeviceID, 1))
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                        (0, "OmxDecTestPlayback::OpenPCMDriver() - ERROR, Cannot enable audio device, OUT"));
        printf("[ERROR] OmxDecTestPlayback::OpenPCMDriver() - Cannot enable audio device\n");
        return OMX_FALSE;
    }

    if (msm_route_stream(1, iSessionID, iDeviceID, 1))
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                        (0, "OmxDecTestPlayback::OpenPCMDriver() - ERROR, Cannot set stream routing, OUT"));
        printf("[ERROR] OmxDecTestPlayback::OpenPCMDriver() - Cannot set stream routing\n");
        return OMX_FALSE;
    }

    //  send AUDIO_START command
    if (E_IOCTL == ioctl(iPCMDriverFD, AUDIO_START, 0) )
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                        (0, "OmxDecTestPlayback::OpenPCMDriver() - ERROR, starting Audio, OUT"));
        return OMX_FALSE;
    }

    return OMX_TRUE;
}

OMX_ERRORTYPE OmxDecTestPlayback::FillBufferDone(OMX_OUT OMX_HANDLETYPE aComponent,
      OMX_OUT OMX_PTR aAppData,
      OMX_OUT OMX_BUFFERHEADERTYPE* aBuffer)
{
    OSCL_UNUSED_ARG(aComponent);
    OSCL_UNUSED_ARG(aAppData);

    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "OmxDecTestPlayback::FillBufferDone() - IN"));
    //Check the validity of buffer
    if (NULL == aBuffer || OMX_DirOutput != aBuffer->nOutputPortIndex)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                        (0, "OmxDecTestPlayback::FillBufferDone() - ERROR, Invalid buffer found in callback, OUT"));
        iState = StateError;
        RunIfNotReady();
        return OMX_ErrorBadParameter;
    }

    OMX_U8* pOutputBuffer;
    OMX_U32 ii = 0;

    pOutputBuffer = (OMX_U8*)(aBuffer->pBuffer);

   //Output buffer has been freed by component & can now be passed again in FillThisBuffer call
    if (NULL != aBuffer)
    {
        if (0 != aBuffer->nFilledLen)
        {
        // write decoded data to PCM driver

      if (write(iPCMDriverFD, aBuffer->pBuffer, aBuffer->nFilledLen) != (ssize_t)(aBuffer->nFilledLen))
      {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                            (0, "OmxDecTestPlayback::FillBufferDone() - ERROR, Write to PCM driver failed, OUT"));
            iState = StateError;
            RunIfNotReady();
            return OMX_ErrorBadParameter;
      }
      PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                                (0, "OmxDecTestPlayback::FillBufferDone() - Frame %i written to PCM driver successfully", iFramesWritten));

    if (WriteOutput(pOutputBuffer, aBuffer->nFilledLen))
            {
                /* Clear the output buffer after writing to file,
                 * so that there is no chance of getting it written again while flushing port */
                aBuffer->nFilledLen = 0;

                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                                (0, "OmxDecTestPlayback::FillBufferDone() - Frame %i processed Timestamp %d", iFramesWritten, aBuffer->nTimeStamp));

                iFramesWritten++;
            }
            else
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                                (0, "OmxDecTestPlayback::FillBufferDone() - ERROR, Failed to write output to file"));
            }
        }
    }

    while (((OMX_U32) ipOutBuffer[ii] != (OMX_U32) aBuffer) &&
            (ii < iOutBufferCount))
    {
        ii++;
    }

    if (iOutBufferCount != ii)
    {
        //If the buffer is already returned and then a callback comes for same buffer
        //report an error and stop
        if (OMX_TRUE == ipOutReleased[ii])
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                            (0, "OmxDecTestPlayback::FillBufferDone() - ERROR, Same bufer with index %d returned twice", ii));

            iState = StateError;
            RunIfNotReady();
        }

        ipOutReleased[ii] = OMX_TRUE;
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                        (0, "OmxDecTestBase::FillBufferDone() - Output buffer with index %d marked free", ii));
    }
    else
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                        (0, "OmxDecTestPlayback::FillBufferDone() - ERROR, Invalid buffer found in callback, cannot mark it free"));
        iState = StateError;
        RunIfNotReady();
    }

    if (0 >= iPendingCommands)
    {
        RunIfNotReady();
    }

    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "OmxDecTestPlayback::FillBufferDone() - OUT"));
    return OMX_ErrorNone;
}

/*
 * Active Object class's Run () function
 * Control all the states of AO & sends API's to the component
 */
void OmxDecTestPlayback::Run()
{
    switch (iState)
    {
        case StateUnLoaded:
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "OmxDecTestPlayback::Run() - StateUnLoaded IN"));

            OMX_ERRORTYPE Err;
            OMX_BOOL Status;

            OMX_BOOL AudioFormat = OMX_FALSE;
            if ( (0 == oscl_strcmp(iFormat, "MP3"))
                || (0 == oscl_strcmp(iFormat, "AAC"))
                || (0 == oscl_strcmp(iFormat, "AMR")))
            {
                    AudioFormat = OMX_TRUE;
            }
            if (OMX_FALSE == AudioFormat)
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_DEBUG,
                (0, "OmxDecTestPlayback::Run() - This test is designe donly for Audio formats OUT"));
                iState = StateUnLoaded;
                OsclExecScheduler* sched = OsclExecScheduler::Current();
                sched->StopScheduler();
                break;
            }
            if (!iCallbacks->initCallbacks())
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "OmxDecTestPlayback::RunLL() - ERROR initCallbacks failed, OUT"));
                StopOnError();
                break;
            }

            ipAppPriv = (AppPrivateType*) oscl_malloc(sizeof(AppPrivateType));
            CHECK_MEM(ipAppPriv, "Component_Handle");
            ipAppPriv->Handle = NULL;


            //This should be the first call to the component to load it.
            Err = OMX_MasterInit();
            CHECK_ERROR(Err, "OMX_MasterInit");

            //Allocate bitstream buffer for MP3 component
            if (0 == oscl_strcmp(iFormat, "MP3"))
            {
                ipMp3Bitstream = OSCL_NEW(Mp3BitstreamObject, (ipInputFile));
                CHECK_MEM(ipMp3Bitstream, "Bitstream_Buffer");

                Status = PrepareComponent();
            }
            else
            {
                Status = PrepareAudioComponent();
            }

            if (OMX_FALSE == Status)
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG, (0, "OmxDecTestPlayback::Run() Error while loading component OUT"));
                iState = StateError;

                if (iInputParameters.inPtr)
                {
                    oscl_free(iInputParameters.inPtr);
                    iInputParameters.inPtr = NULL;
                }

                RunIfNotReady();
                break;
            }

#if PROXY_INTERFACE
            ipThreadSafeHandlerEventHandler = OSCL_NEW(OmxEventHandlerThreadSafeCallbackAO, (this, EVENT_HANDLER_QUEUE_DEPTH, "EventHandlerAO"));
            ipThreadSafeHandlerEmptyBufferDone = OSCL_NEW(OmxEmptyBufferDoneThreadSafeCallbackAO, (this, iInBufferCount, "EmptyBufferDoneAO"));
            ipThreadSafeHandlerFillBufferDone = OSCL_NEW(OmxFillBufferDoneThreadSafeCallbackAO, (this, iOutBufferCount, "FillBufferDoneAO"));

            if ((NULL == ipThreadSafeHandlerEventHandler) ||
                    (NULL == ipThreadSafeHandlerEmptyBufferDone) ||
                    (NULL == ipThreadSafeHandlerFillBufferDone))
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                                (0, "OmxDecTestPlayback::Run() - Error ThreadSafe Callback Handler initialization failed, OUT"));

                iState = StateUnLoaded;
                OsclExecScheduler* sched = OsclExecScheduler::Current();
                sched->StopScheduler();
            }
#endif

            Status = OpenPCMDriver();

            if (OMX_FALSE == Status)
            {
                iState = StateError;
                RunIfNotReady();
                break;
            }

            iState = StateLoaded;
            RunIfNotReady();
        }
        break;

        case StateLoaded:
        {
            OMX_ERRORTYPE Err;
            OMX_U32 ii;

            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "OmxDecTestPlayback::Run() - StateLoaded IN"));

            // allocate memory for ipInBuffer
            ipInBuffer = (OMX_BUFFERHEADERTYPE**) oscl_malloc(sizeof(OMX_BUFFERHEADERTYPE*) * iInBufferCount);
            CHECK_MEM(ipInBuffer, "InputBufferHeader");

            ipInputAvail = (OMX_BOOL*) oscl_malloc(sizeof(OMX_BOOL) * iInBufferCount);
            CHECK_MEM(ipInputAvail, "InputBufferFlag");

            /* Initialize all the buffers to NULL */
            for (ii = 0; ii < iInBufferCount; ii++)
            {
                ipInBuffer[ii] = NULL;
            }

            //allocate memory for output buffer
            ipOutBuffer = (OMX_BUFFERHEADERTYPE**) oscl_malloc(sizeof(OMX_BUFFERHEADERTYPE*) * iOutBufferCount);
            CHECK_MEM(ipOutBuffer, "OutputBuffer");

            ipOutReleased = (OMX_BOOL*) oscl_malloc(sizeof(OMX_BOOL) * iOutBufferCount);
            CHECK_MEM(ipOutReleased, "OutputBufferFlag");

            /* Initialize all the buffers to NULL */
            for (ii = 0; ii < iOutBufferCount; ii++)
            {
                ipOutBuffer[ii] = NULL;
            }

            Err = OMX_SendCommand(ipAppPriv->Handle, OMX_CommandStateSet, OMX_StateIdle, NULL);
            CHECK_ERROR(Err, "SendCommand Loaded->Idle");

            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                            (0, "OmxDecTestPlayback::Run() - Sent State Transition Command from Loaded->Idle"));

            iPendingCommands = 1;

            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                            (0, "OmxDecTestPlayback::Run() - Allocating %d input and %d output buffers", iInBufferCount, iOutBufferCount));

            //These calls are required because the control of in & out buffer should be with the testapp.
            for (ii = 0; ii < iInBufferCount; ii++)
            {
                Err = OMX_AllocateBuffer(ipAppPriv->Handle, &ipInBuffer[ii], iInputPortIndex, NULL, iInBufferSize);
                CHECK_ERROR(Err, "AllocateBuffer_Input");

                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                                (0, "OmxDecTestPlayback::Run() - Called AllocateBuffer for buffer index %d on port %d", ii, iInputPortIndex));

                ipInputAvail[ii] = OMX_TRUE;
                ipInBuffer[ii]->nInputPortIndex = iInputPortIndex;
            }

            if (StateError == iState)
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                                (0, "OmxDecTestPlayback::Run() - AllocateBuffer Error, StateLoaded OUT"));
                RunIfNotReady();
                break;
            }

            for (ii = 0; ii < iOutBufferCount; ii++)
            {
                Err = OMX_AllocateBuffer(ipAppPriv->Handle, &ipOutBuffer[ii], iOutputPortIndex, NULL, iOutBufferSize);
                CHECK_ERROR(Err, "AllocateBuffer_Output");

                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                                (0, "OmxDecTestPlayback::Run() - Called AllocateBuffer for buffer index %d on port %d", ii, iOutputPortIndex));

                ipOutReleased[ii] = OMX_TRUE;

                ipOutBuffer[ii]->nOutputPortIndex = iOutputPortIndex;
                ipOutBuffer[ii]->nInputPortIndex = 0;
            }
            if (StateError == iState)
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                                (0, "OmxDecTestPlayback::Run() - AllocateBuffer Error, StateLoaded OUT"));
                RunIfNotReady();
                break;
            }

            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "OmxDecTestPlayback::Run() - StateLoaded OUT, Moving to next state"));

        }
        break;

        case StateIdle:
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "OmxDecTestPlayback::Run() - StateIdle IN"));
            OMX_ERRORTYPE Err = OMX_ErrorNone;

            /*Send an output buffer before dynamic reconfig */
            Err = OMX_FillThisBuffer(ipAppPriv->Handle, ipOutBuffer[0]);
            CHECK_ERROR(Err, "FillThisBuffer");

            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                            (0, "OmxDecTestPlayback::Run() - FillThisBuffer command called for initiating dynamic port reconfiguration"));

            ipOutReleased[0] = OMX_FALSE;

            Err = OMX_SendCommand(ipAppPriv->Handle, OMX_CommandStateSet, OMX_StateExecuting, NULL);
            CHECK_ERROR(Err, "SendCommand Idle->Executing");

            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                            (0, "OmxDecTestPlayback::Run() - Sent State Transition Command from Idle->Executing"));

            iPendingCommands = 1;

            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "OmxDecTestPlayback::Run() - StateIdle OUT"));
        }
        break;

        case StateDecodeHeader:
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                            (0, "OmxDecTestPlayback::Run() - StateDecodeHeader IN, Sending configuration input buffers to the component to start dynamic port reconfiguration"));
            if ( 0 == strcmp(iFormat, "MP3"))
            {
                if (!iFlagDecodeHeader) {
                  (*this.*pGetInputFrame)();
                  iFlagDecodeHeader = OMX_TRUE;
                }
            }
            iState = StateExecuting;
            RunIfNotReady();

            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "OmxDecTestPlayback::Run() - StateDecodeHeader OUT"));
        }
        break;

        case StateDisablePort:
        {
            OMX_ERRORTYPE Err = OMX_ErrorNone;
            OMX_U32 ii;

            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "OmxDecTestPlayback::Run() - StateDisablePort IN"));

            if (!iDisableRun)
            {
                if (!iFlagDisablePort)
                {
                    Err = OMX_SendCommand(ipAppPriv->Handle, OMX_CommandPortDisable, iOutputPortIndex, NULL);
                    CHECK_ERROR(Err, "SendCommand_PortDisable");

                    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                                    (0, "OmxDecTestPlayback::Run() - Sent Command for OMX_CommandPortDisable on port %d as a part of dynamic port reconfiguration", iOutputPortIndex));

                    iPendingCommands = 1;
                    iFlagDisablePort = OMX_TRUE;
                    RunIfNotReady();
                }
                else
                {
                    //Wait for all the buffers to be returned on output port before freeing them
                    //This wait is required because of the queueing delay in all the Callbacks
                    for (ii = 0; ii < iOutBufferCount; ii++)
                    {
                        if (OMX_FALSE == ipOutReleased[ii])
                        {
                            break;
                        }
                    }

                    if (ii != iOutBufferCount)
                    {
                        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                                        (0, "OmxDecTestPlayback::Run() - Not all the output buffers returned by component yet, wait for it"));
                        RunIfNotReady();
                        break;
                    }

                    for (ii = 0; ii < iOutBufferCount; ii++)
                    {
                        if (ipOutBuffer[ii])
                        {
                            Err = OMX_FreeBuffer(ipAppPriv->Handle, iOutputPortIndex, ipOutBuffer[ii]);
                            CHECK_ERROR(Err, "FreeBuffer_Output_DynamicReconfig");
                            ipOutBuffer[ii] = NULL;

                            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                                            (0, "OmxDecTestPlayback::Run() - Called FreeBuffer for buffer index %d on port %d", ii, iOutputPortIndex));
                        }
                    }

                    if (ipOutBuffer)
                    {
                        oscl_free(ipOutBuffer);
                        ipOutBuffer = NULL;
                    }

                    if (ipOutReleased)
                    {
                        oscl_free(ipOutReleased);
                        ipOutReleased = NULL;
                    }

                    if (StateError == iState)
                    {
                        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                                        (0, "OmxDecTestPlayback::Run() - Error occured in this state, StateDisablePort OUT"));
                        RunIfNotReady();
                        break;
                    }
                    iDisableRun = OMX_TRUE;
                }
            }

            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "OmxDecTestPlayback::Run() - StateDisablePort OUT"));
        }
        break;

        case StateDynamicReconfig:
        {
            OMX_BOOL Status = OMX_TRUE;

            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "OmxDecTestPlayback::Run() - StateDynamicReconfig IN"));

            Status = HandlePortReEnable();

            if (OMX_FALSE == Status)
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                                (0, "OmxDecTestPlayback::Run() - Error occured in this state, StateDynamicReconfig OUT"));
                iState = StateError;
                RunIfNotReady();
                break;
            }

            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "OmxDecTestPlayback::Run() - StateDynamicReconfig OUT"));
        }
        break;

        case StateExecuting:
        {
            OMX_U32 Index;
            OMX_BOOL MoreOutput;

            OMX_ERRORTYPE Err = OMX_ErrorNone;

            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "OmxDecTestPlayback::Run() - StateExecuting IN"));

            MoreOutput = OMX_TRUE;
            while (MoreOutput)
            {
                Index = 0;
                while (OMX_FALSE == ipOutReleased[Index] && Index < iOutBufferCount)
                {
                    Index++;
                }

                if (Index != iOutBufferCount)
                {
                    //This call is being made only once per frame
                    Err = OMX_FillThisBuffer(ipAppPriv->Handle, ipOutBuffer[Index]);
                    CHECK_ERROR(Err, "FillThisBuffer");
                    //Make this flag OMX_TRUE till u receive the callback for output buffer free
                    ipOutReleased[Index] = OMX_FALSE;
                    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                                    (0, "OmxDecTestPlayback::Run() - FillThisBuffer command called for output buffer index %d", Index));
                }
                else
                {
                    MoreOutput = OMX_FALSE;;
                }
            }

            if (!iStopProcessingInput || (OMX_ErrorInsufficientResources == iStatusExecuting))
            {
                // find available input buffer
                Index = 0;
                while (OMX_FALSE == ipInputAvail[Index] && Index < iInBufferCount)
                {
                    Index++;
                }

                if (Index != iInBufferCount)
                {
                    if (0 == strcmp(iFormat, "MP3")) {
                   iStatusExecuting = (*this.*pGetInputFrame)();
                }
                else
                {
                    iStatusExecuting = ReadBuffer(Index);
                }
            }
            }
            else
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                                (0, "OmxDecTestPlayback::Run() - Finished sending all the input buffers, Do not send the EOS buffer to the component"));

                iState = StateStopping;

            }

            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "OmxDecTestPlayback::Run() - StateExecuting OUT"));

            RunIfNotReady();
        }
        break;

        case StateStopping:
        {
            OMX_ERRORTYPE Err = OMX_ErrorNone;

            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "OmxDecTestPlayback::Run() - StateStopping IN"));

            //stop execution by state transition to Idle state.
            if (!iFlagStopping)
            {
                Err = OMX_SendCommand(ipAppPriv->Handle, OMX_CommandStateSet, OMX_StateIdle, NULL);
                CHECK_ERROR(Err, "SendCommand Executing->Idle");

                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                                (0, "OmxDecTestPlayback::Run() - Sent State Transition Command from Executing->Idle"));

                iPendingCommands = 1;
                iFlagStopping = OMX_TRUE;
            }

            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "OmxDecTestPlayback::Run() - StateStopping OUT"));
        }
        break;

        case StateCleanUp:
        {
            OMX_U32 ii;
            OMX_ERRORTYPE Err = OMX_ErrorNone;
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "OmxDecTestPlayback::Run() - StateCleanUp IN"));

            if (!iFlagCleanUp)
            {
                //Added a check here to verify whether all the ip/op buffers are returned back by the component
                //in case of Executing->Idle state transition

                if (OMX_FALSE == VerifyAllBuffersReturned())
                {
                    // not all buffers have been returned yet, reschedule
                    RunIfNotReady();
                    break;
                }

                //Destroy the component by state transition to Loaded state
                Err = OMX_SendCommand(ipAppPriv->Handle, OMX_CommandStateSet, OMX_StateLoaded, NULL);
                CHECK_ERROR(Err, "SendCommand Idle->Loaded");

                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                                (0, "OmxDecTestPlayback::Run() - Sent State Transition Command from Idle->Loaded"));

                iPendingCommands = 1;

                if (ipInBuffer)
                {
                    for (ii = 0; ii < iInBufferCount; ii++)
                    {
                        if (ipInBuffer[ii])
                        {
                            Err = OMX_FreeBuffer(ipAppPriv->Handle, iInputPortIndex, ipInBuffer[ii]);
                            CHECK_ERROR(Err, "FreeBuffer_Input");
                            ipInBuffer[ii] = NULL;

                            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                                            (0, "OmxDecTestPlayback::Run() - Called FreeBuffer for buffer index %d on port %d", ii, iInputPortIndex));
                        }
                    }

                    oscl_free(ipInBuffer);
                    ipInBuffer = NULL;
                }

                if (ipInputAvail)
                {
                    oscl_free(ipInputAvail);
                    ipInputAvail = NULL;
                }


                if (ipOutBuffer)
                {
                    for (ii = 0; ii < iOutBufferCount; ii++)
                    {
                        if (ipOutBuffer[ii])
                        {
                            Err = OMX_FreeBuffer(ipAppPriv->Handle, iOutputPortIndex, ipOutBuffer[ii]);
                            CHECK_ERROR(Err, "FreeBuffer_Output");
                            ipOutBuffer[ii] = NULL;

                            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                                            (0, "OmxDecTestPlayback::Run() - Called FreeBuffer for buffer index %d on port %d", ii, iOutputPortIndex));
                        }
                    }
                    oscl_free(ipOutBuffer);
                    ipOutBuffer = NULL;
                }

                if (ipOutReleased)
                {
                    oscl_free(ipOutReleased);
                    ipOutReleased = NULL;
                }
                iFlagCleanUp = OMX_TRUE;
            }

            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "OmxDecTestPlayback::Run() - StateCleanUp OUT"));

        }
        break;


        /********* FREE THE HANDLE & CLOSE FILES FOR THE COMPONENT ********/
        case StateStop:
        {
            OMX_U8 TestName[] = "PLAYBACK_TEST";
            OMX_ERRORTYPE Err = OMX_ErrorNone;

            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "OmxDecTestPlayback::Run() - StateStop IN"));

            if (ipAppPriv)
            {
                if (ipAppPriv->Handle)
                {
                    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                                    (0, "OmxDecTestPlayback::Run() - Free the Component Handle"));

                    Err = OMX_MasterFreeHandle(ipAppPriv->Handle);
                    if (OMX_ErrorNone != Err)
                    {
                        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "OmxDecTestPlayback::Run() - FreeHandle Error"));
                        iTestStatus = OMX_FALSE;
                    }
                }
            }

            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                            (0, "OmxDecTestPlayback::Run() - De-initialize the omx component"));

            Err = OMX_MasterDeinit();
            if (OMX_ErrorNone != Err)
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "OmxDecTestPlayback::Run() - OMX_MasterDeinit Error"));
                iTestStatus = OMX_FALSE;
            }


            if (0 == oscl_strcmp(iFormat, "MP3"))
            {
                if (ipMp3Bitstream)
                {
                    OSCL_DELETE(ipMp3Bitstream);
                    ipMp3Bitstream = NULL;
                }
            }

            if (iOutputParameters)
            {
                oscl_free(iOutputParameters);
                iOutputParameters = NULL;
            }

            if (0 <= iPCMDriverFD)
            {
                if (E_IOCTL == ioctl(iPCMDriverFD, AUDIO_STOP, 0))
                {
                    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                        (0, "OmxDecTestPlayback::OpenPCMDriver() - ERROR, stopping Audio session"));
                }
                if (msm_route_stream(1, iSessionID, iDeviceID, DISABLE_ROUTING))
                {
                    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                        (0, "OmxDecTestPlayback::OpenPCMDriver() - ERROR,reseting stream routing"));
                }

                close(iPCMDriverFD);
                iPCMDriverFD = -1;
            }

            if (ipAppPriv)
            {
                oscl_free(ipAppPriv);
                ipAppPriv = NULL;
            }

#if PROXY_INTERFACE
            if (ipThreadSafeHandlerEventHandler)
            {
                OSCL_DELETE(ipThreadSafeHandlerEventHandler);
                ipThreadSafeHandlerEventHandler = NULL;
            }

            if (ipThreadSafeHandlerEmptyBufferDone)
            {
                OSCL_DELETE(ipThreadSafeHandlerEmptyBufferDone);
                ipThreadSafeHandlerEmptyBufferDone = NULL;
            }

            if (ipThreadSafeHandlerFillBufferDone)
            {
                OSCL_DELETE(ipThreadSafeHandlerFillBufferDone);
                ipThreadSafeHandlerFillBufferDone = NULL;
            }
#endif

            if (OMX_FALSE == iTestStatus)
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                                (0, "OmxDecTestPlayback::Run() - %s: Fail", TestName));
#ifdef PRINT_RESULT
                fprintf(iConsOutFile, "%s: Fail \n", TestName);
                OMX_DEC_TEST(false);
                iTestCase->TestCompleted();
#endif

            }
            else
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                                (0, "OmxDecTestPlayback::Run() - %s: Success", TestName));
#ifdef PRINT_RESULT
                fprintf(iConsOutFile, "%s: Success \n", TestName);
                OMX_DEC_TEST(true);
                iTestCase->TestCompleted();
#endif
            }

            if (ipOutputFile)
            {
                fclose(ipOutputFile);
            }

            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "OmxDecTestPlayback::Run() - StateStop OUT"));

            iState = StateUnLoaded;
            OsclExecScheduler* sched = OsclExecScheduler::Current();
            sched->StopScheduler();
        }
        break;

        case StateError:
        {
            //Do all the cleanup's and exit from here
            OMX_U32 ii;

            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "OmxDecTestPlayback::Run() - StateError IN"));

            iTestStatus = OMX_FALSE;

            if (ipInBuffer)
            {
                for (ii = 0; ii < iInBufferCount; ii++)
                {
                    if (ipInBuffer[ii])
                    {
                        OMX_FreeBuffer(ipAppPriv->Handle, iInputPortIndex, ipInBuffer[ii]);
                        ipInBuffer[ii] = NULL;

                        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                                        (0, "OmxDecTestPlayback::Run() - Called FreeBuffer for buffer index %d on port %d", ii, iInputPortIndex));
                    }
                }
                oscl_free(ipInBuffer);
                ipInBuffer = NULL;
            }

            if (ipInputAvail)
            {
                oscl_free(ipInputAvail);
                ipInputAvail = NULL;
            }

            if (ipOutBuffer)
            {
                for (ii = 0; ii < iOutBufferCount; ii++)
                {
                    if (ipOutBuffer[ii])
                    {
                        OMX_FreeBuffer(ipAppPriv->Handle, iOutputPortIndex, ipOutBuffer[ii]);
                        ipOutBuffer[ii] = NULL;

                        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                                        (0, "OmxDecTestPlayback::Run() - Called FreeBuffer for buffer index %d on port %d", ii, iOutputPortIndex));
                    }
                }
                oscl_free(ipOutBuffer);
                ipOutBuffer = NULL;
            }

            if (ipOutReleased)
            {
                oscl_free(ipOutReleased);
                ipOutReleased = NULL;
            }

            iState = StateStop;
            RunIfNotReady();

            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "OmxDecTestPlayback::Run() - StateError OUT"));

        }
        break;


        default:
        {
            break;
        }
    }
    return;
}

OMX_ERRORTYPE OmxDecTestPlayback::ReadBuffer(OMX_U32 Index)
{
    int ReadCount = 0;

     PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "OmxDecTestPlayback::ReadBuffer() - IN"));

    ReadCount = fread(ipInBuffer[Index]->pBuffer, 1, ipInBuffer[Index]->nAllocLen, ipInputFile);

    ipInBuffer[Index]->nFilledLen = ReadCount;

    if (0 == ReadCount)
      ipInBuffer[Index]->nFlags |= OMX_BUFFERFLAG_EOS;

    if (ReadCount < 0)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "OmxComponentDecTest::ReadBuffer() - ERROR reading input file", ii));
        return OMX_ErrorInsufficientResources;
    }

    OMX_ERRORTYPE Status = OMX_EmptyThisBuffer(ipAppPriv->Handle, ipInBuffer[Index]);
    if (OMX_ErrorNone == Status)
    {
        ipInputAvail[Index] = OMX_FALSE; // mark unavailable
        return OMX_ErrorNone;
    }
    return OMX_ErrorNone;
}
