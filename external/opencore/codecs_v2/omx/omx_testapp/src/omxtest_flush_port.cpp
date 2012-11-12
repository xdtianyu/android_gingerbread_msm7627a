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

#include "omxdectest_basic_test.h"
#include "oscl_mem.h"

#define TEST_NUM_BUFFERS_TO_PROCESS 15

/*
 * Active Object class's Run () function
 * Control all the states of AO & sends openmax API's to the component
 */

void OmxDecTestFlushPort::Run()
{
    switch (iState)
    {
        case StateUnLoaded:
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "OmxDecTestFlushPort::Run() - StateUnLoaded IN"));
            OMX_ERRORTYPE Err;
            OMX_BOOL Status;

            if (!iCallbacks->initCallbacks())
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "OmxDecTestFlushPort::Run() - ERROR initCallbacks failed, OUT"));
                StopOnError();
                break;
            }

            ipAppPriv = (AppPrivateType*) oscl_malloc(sizeof(AppPrivateType));
            CHECK_MEM(ipAppPriv, "Component_Handle");
            ipAppPriv->Handle = NULL;

            //Allocate bitstream buffer for AVC component
            if (0 == oscl_strcmp(iFormat, "H264"))
            {
                ipAVCBSO = OSCL_NEW(AVCBitstreamObject, (ipInputFile));
                CHECK_MEM(ipAVCBSO, "Bitstream_Buffer");
            }

            //Allocate bitstream buffer for MPEG4/H263 component
            if (0 == oscl_strcmp(iFormat, "M4V") || 0 == oscl_strcmp(iFormat, "H263"))
            {
                ipBitstreamBuffer = (OMX_U8*) oscl_malloc(BIT_BUFF_SIZE);
                CHECK_MEM(ipBitstreamBuffer, "Bitstream_Buffer")
                ipBitstreamBufferPointer = ipBitstreamBuffer;
            }

            //Allocate bitstream buffer for MP3 component
            if (0 == oscl_strcmp(iFormat, "MP3"))
            {
                ipMp3Bitstream = OSCL_NEW(Mp3BitstreamObject, (ipInputFile));
                CHECK_MEM(ipMp3Bitstream, "Bitstream_Buffer");
            }

            //This should be the first call to the component to load it.
            Err = OMX_MasterInit();
            CHECK_ERROR(Err, "OMX_MasterInit");
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG, (0, "OmxDecTestFlushPort::Run() - OMX_MasterInit done"));

            Status = PrepareComponent();

            if (OMX_FALSE == Status)
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG, (0, "OmxDecTestFlushPort::Run() Error while loading component OUT"));
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
                                (0, "OmxDecTestFlushPort::Run() - Error, ThreadSafe Callback Handler initialization failed, OUT"));

                iState = StateUnLoaded;
                OsclExecScheduler* sched = OsclExecScheduler::Current();
                sched->StopScheduler();
            }
#endif

            if (StateError != iState)
            {
                iState = StateLoaded;
                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "OmxDecTestFlushPort::Run() - StateUnLoaded OUT, moving to next state"));
            }

            RunIfNotReady();
        }
        break;

        case StateLoaded:
        {
            OMX_ERRORTYPE Err;
            OMX_U32 ii;

            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "OmxDecTestFlushPort::Run() - StateLoaded IN"));
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
                            (0, "OmxDecTestFlushPort::Run() - Sent State Transition Command from Loaded->Idle"));
            iPendingCommands = 1;

            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                            (0, "OmxDecTestFlushPort::Run() - Allocating %d input and %d output buffers", iInBufferCount, iOutBufferCount));
            //These calls are required because the control of in & out buffer should be with the testapp.
            for (ii = 0; ii < iInBufferCount; ii++)
            {
                Err = OMX_AllocateBuffer(ipAppPriv->Handle, &ipInBuffer[ii], iInputPortIndex, NULL, iInBufferSize);
                CHECK_ERROR(Err, "AllocateBuffer_Input");

                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                                (0, "OmxDecTestFlushPort::Run() - Called AllocateBuffer for buffer index %d on port %d", ii, iInputPortIndex));
                ipInputAvail[ii] = OMX_TRUE;
                ipInBuffer[ii]->nInputPortIndex = iInputPortIndex;
            }

            if (StateError == iState)
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                                (0, "OmxDecTestFlushPort::Run() - AllocateBuffer Error, StateLoaded OUT"));
                RunIfNotReady();
                break;
            }

            for (ii = 0; ii < iOutBufferCount; ii++)
            {
                Err = OMX_AllocateBuffer(ipAppPriv->Handle, &ipOutBuffer[ii], iOutputPortIndex, NULL, iOutBufferSize);
                CHECK_ERROR(Err, "AllocateBuffer_Output");

                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                                (0, "OmxDecTestFlushPort::Run() - Called AllocateBuffer for buffer index %d on port %d", ii, iOutputPortIndex));
                ipOutReleased[ii] = OMX_TRUE;

                ipOutBuffer[ii]->nOutputPortIndex = iOutputPortIndex;
                ipOutBuffer[ii]->nInputPortIndex = 0;
            }
            if (StateError == iState)
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                                (0, "OmxDecTestFlushPort::Run() - AllocateBuffer Error, StateLoaded OUT"));
                RunIfNotReady();
                break;
            }
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "OmxDecTestFlushPort::Run() - StateLoaded OUT, Moving to next state"));
        }
        break;

        case StateIdle:
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "OmxDecTestFlushPort::Run() - StateIdle IN"));
            OMX_ERRORTYPE Err = OMX_ErrorNone;

            Err = OMX_FillThisBuffer(ipAppPriv->Handle, ipOutBuffer[0]);
            CHECK_ERROR(Err, "FillThisBuffer");

            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                            (0, "OmxDecTestFlushPort::Run() - FillThisBuffer command called for initiating dynamic port reconfiguration"));

            ipOutReleased[0] = OMX_FALSE;

            Err = OMX_SendCommand(ipAppPriv->Handle, OMX_CommandStateSet, OMX_StateExecuting, NULL);
            CHECK_ERROR(Err, "SendCommand Idle->Executing");

            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                            (0, "OmxDecTestFlushPort::Run() - Sent State Transition Command from Idle->Executing"));
            iPendingCommands = 1;

            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "OmxDecTestFlushPort::Run() - StateIdle OUT"));
        }
        break;

        case StateDecodeHeader:
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                            (0, "OmxDecTestFlushPort::Run() - StateDecodeHeader IN, Sending configuration input buffers to the component to start dynamic port reconfiguration"));

            if (!iFlagDecodeHeader)
            {
                (*this.*pGetInputFrame)();
                //For AAC component , send one more frame apart from the config frame, so that we can receive the callback
                if (0 == oscl_strcmp(iFormat, "AAC") || 0 == oscl_strcmp(iFormat, "AMR"))
                {
                    (*this.*pGetInputFrame)();
                }
                iFlagDecodeHeader = OMX_TRUE;
                iFrameCount++;

                //Proceed to executing state and if Port settings changed callback comes,
                //then do the dynamic port reconfiguration
                iState = StateExecuting;

                RunIfNotReady();
            }
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "OmxDecTestFlushPort::Run() - StateDecodeHeader OUT"));
        }
        break;

        case StateDisablePort:
        {
            OMX_ERRORTYPE Err = OMX_ErrorNone;
            OMX_U32 ii;

            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "OmxDecTestFlushPort::Run() - StateDisablePort IN"));

            if (!iDisableRun)
            {
                if (!iFlagDisablePort)
                {
                    Err = OMX_SendCommand(ipAppPriv->Handle, OMX_CommandPortDisable, iOutputPortIndex, NULL);
                    CHECK_ERROR(Err, "SendCommand_PortDisable");

                    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                                    (0, "OmxDecTestFlushPort::Run() - Sent Command for OMX_CommandPortDisable on port %d as a part of dynamic port reconfiguration", iOutputPortIndex));

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
                                        (0, "OmxDecTestFlushPort::Run() - Not all the output buffers returned by component yet, wait for it"));
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
                                            (0, "OmxDecTestFlushPort::Run() - Called FreeBuffer for buffer index %d on port %d", ii, iOutputPortIndex));
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
                                        (0, "OmxDecTestFlushPort::Run() - Error occured in this state, StateDisablePort OUT"));
                        RunIfNotReady();
                        break;
                    }
                    iDisableRun = OMX_TRUE;
                }
            }

            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "OmxDecTestFlushPort::Run() - StateDisablePort OUT"));
        }
        break;

        case StateDynamicReconfig:
        {
            OMX_BOOL Status = OMX_TRUE;

            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "OmxDecTestFlushPort::Run() - StateDynamicReconfig IN"));

            Status = HandlePortReEnable();
            if (OMX_FALSE == Status)
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                                (0, "OmxDecTestFlushPort::Run() - Error occured in this state, StateDynamicReconfig OUT"));
                iState = StateError;
                RunIfNotReady();
                break;
            }

            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "OmxDecTestFlushPort::Run() - StateDynamicReconfig OUT"));
        }
        break;

        case StateExecuting:
        {
            OMX_U32 Index;
            OMX_BOOL MoreOutput;
            OMX_ERRORTYPE Err = OMX_ErrorNone;

            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "OmxDecTestFlushPort::Run() - StateExecuting IN"));

            //After Processing N number of buffers, send the flush command on both the ports
            if ((iFrameCount > TEST_NUM_BUFFERS_TO_PROCESS) && (OMX_FALSE == iFlushCommandSent))
            {
                Err = OMX_SendCommand(ipAppPriv->Handle, OMX_CommandFlush, OMX_ALL, NULL);
                CHECK_ERROR(Err, "SendCommand OMX_CommandFlush");

                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                                (0, "OmxDecTestFlushPort::Run() - Sending Flush Command on each port"));

                //Expecting 2 callbacks from each port
                iPendingCommands = 2;
                iFlushCommandSent = OMX_TRUE;
            }

            else
            {
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
                        //Reset this till u receive the callback for output buffer free
                        ipOutReleased[Index] = OMX_FALSE;

                        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                                        (0, "OmxDecTestFlushPort::Run() - FillThisBuffer command called for output buffer index %d", Index));
                    }
                    else
                    {
                        MoreOutput = OMX_FALSE;
                    }
                } //while (MoreOutput) loop end here


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
                        iStatusExecuting = (*this.*pGetInputFrame)();
                        iFrameCount++;
                    }
                }
                else if (OMX_FALSE == iEosFlagExecuting)
                {
                    //Only send one successful dummy buffer with flag set to signal EOS
                    Index = 0;
                    while (OMX_FALSE == ipInputAvail[Index] && Index < iInBufferCount)
                    {
                        Index++;
                    }

                    if (Index != iInBufferCount)
                    {
                        ipInBuffer[Index]->nFlags |= OMX_BUFFERFLAG_EOS;
                        ipInBuffer[Index]->nFilledLen = 0;
                        Err = OMX_EmptyThisBuffer(ipAppPriv->Handle, ipInBuffer[Index]);

                        CHECK_ERROR(Err, "EmptyThisBuffer_EOS");

                        ipInputAvail[Index] = OMX_FALSE; // mark unavailable
                        iEosFlagExecuting = OMX_TRUE;

                        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                                        (0, "OmxDecTestFlushPort::Run() - Input buffer sent to the component with OMX_BUFFERFLAG_EOS flag set"));
                    }
                } //else if (OMX_FALSE == iEosFlagExecuting)
                else
                {
                    //nothing to do here
                }

                RunIfNotReady();
            } //else of if ((iFrameCount > TEST_NUM_BUFFERS_TO_PROCESS) && (OMX_FALSE == iFlushCommandSent)) ends here

            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "OmxDecTestFlushPort::Run() - StateExecuting OUT"));

        }
        break;

        case StateIntermediate:
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "OmxDecTestFlushPort::Run() - StateIntermediate IN, for verification if ip/op buffers"));

            /* check whether all the buffers are returned on each port after a flush comand,
             * then change the Client AO state back to executing to resume processing
             */
            if (OMX_TRUE == VerifyAllBuffersReturned())
            {
                // All buffers have returned, change the AO state back to executing to resume processing
                //Note that component is already in Executing state
                iState = StateExecuting;
            }
            else
            {
                // not all buffers have been returned yet, remain in StateIntermediate and reschedule
            }

            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "OmxDecTestFlushPort::Run() - StateIntermediate OUT"));

            RunIfNotReady();
        }
        break;


        case StateStopping:
        {
            OMX_ERRORTYPE Err = OMX_ErrorNone;

            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "OmxDecTestFlushPort::Run() - StateStopping IN"));

            //stop execution by state transition to Idle state.
            if (!iFlagStopping)
            {
                Err = OMX_SendCommand(ipAppPriv->Handle, OMX_CommandStateSet, OMX_StateIdle, NULL);
                CHECK_ERROR(Err, "SendCommand Executing->Idle");

                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                                (0, "OmxDecTestFlushPort::Run() - Sent State Transition Command from Executing->Idle"));

                iPendingCommands = 1;
                iFlagStopping = OMX_TRUE;
            }

            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "OmxDecTestFlushPort::Run() - StateStopping OUT"));
        }
        break;

        case StateCleanUp:
        {
            OMX_U32 ii;
            OMX_ERRORTYPE Err = OMX_ErrorNone;

            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "OmxDecTestFlushPort::Run() - StateCleanUp IN"));


            if (!iFlagCleanUp)
            {
                //Added a check here to verify whether all the ip/op buffers are returned back by the component or not
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
                                (0, "OmxDecTestFlushPort::Run() - Sent State Transition Command from Idle->Loaded"));

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
                                            (0, "OmxDecTestFlushPort::Run() - Called FreeBuffer for buffer index %d on port %d", ii, iInputPortIndex));
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
                                            (0, "OmxDecTestFlushPort::Run() - Called FreeBuffer for buffer index %d on port %d", ii, iOutputPortIndex));
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

            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "OmxDecTestFlushPort::Run() - StateCleanUp OUT"));

        }
        break;


        /********* FREE THE HANDLE & CLOSE FILES FOR THE COMPONENT ********/
        case StateStop:
        {
            OMX_U8 TestName[] = "FLUSH_PORT_TEST";
            OMX_ERRORTYPE Err = OMX_ErrorNone;

            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "OmxDecTestFlushPort::Run() - StateStop IN"));

            if (ipAppPriv)
            {
                if (ipAppPriv->Handle)
                {
                    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                                    (0, "OmxDecTestFlushPort::Run() - Free the Component Handle"));

                    Err = OMX_MasterFreeHandle(ipAppPriv->Handle);
                    if (OMX_ErrorNone != Err)
                    {
                        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "OmxDecTestFlushPort::Run() - FreeHandle Error"));
                        iTestStatus = OMX_FALSE;
                    }
                }
            }

            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                            (0, "OmxDecTestFlushPort::Run() - De-initialize the omx component"));

            Err = OMX_MasterDeinit();
            if (OMX_ErrorNone != Err)
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "OmxDecTestFlushPort::Run() - OMX_MasterDeinit Error"));
                iTestStatus = OMX_FALSE;
            }

            if (0 == oscl_strcmp(iFormat, "H264"))
            {
                if (ipAVCBSO)
                {
                    OSCL_DELETE(ipAVCBSO);
                    ipAVCBSO = NULL;
                }
            }

            if (0 == oscl_strcmp(iFormat, "M4V") || 0 == oscl_strcmp(iFormat, "H263"))
            {
                if (ipBitstreamBuffer)
                {
                    oscl_free(ipBitstreamBufferPointer);
                    ipBitstreamBuffer = NULL;
                    ipBitstreamBufferPointer = NULL;
                }
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
#ifdef PRINT_RESULT
                fprintf(iConsOutFile, "%s: Fail \n", TestName);
                OMX_DEC_TEST(false);
                iTestCase->TestCompleted();
#endif

                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_INFO,
                                (0, "OmxDecTestFlushPort::Run() - %s : Fail", TestName));
            }
            else
            {
#ifdef PRINT_RESULT
                fprintf(iConsOutFile, "%s: Success \n", TestName);
                OMX_DEC_TEST(true);
                iTestCase->TestCompleted();
#endif

                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_INFO,
                                (0, "OmxDecTestFlushPort::Run() - %s : Success", TestName));
            }

            iState = StateUnLoaded;
            OsclExecScheduler* sched = OsclExecScheduler::Current();
            sched->StopScheduler();
        }
        break;

        case StateError:
        {
            //Do all the cleanup's and exit from here
            OMX_U32 ii;

            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "OmxDecTestFlushPort::Run() - StateError IN"));

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
                                        (0, "OmxDecTestFlushPort::Run() - Called FreeBuffer for buffer index %d on port %d", ii, iInputPortIndex));
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
                                        (0, "OmxDecTestFlushPort::Run() - Called FreeBuffer for buffer index %d on port %d", ii, iOutputPortIndex));
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

            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "OmxDecTestFlushPort::Run() - StateError OUT"));

        }
        break;

        default:
        {
            break;
        }
    }
    return ;
}
