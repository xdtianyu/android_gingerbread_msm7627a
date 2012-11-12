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

#define MAX_INSTANCE 3

#define CHECK_ERROR_INSTANCE_TEST(e, s) \
    if (OMX_ErrorNone != (e))\
    {\
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "OmxDecTestMultipleInstance - %s Error, Stop the test case", s));\
        iTestStatus = OMX_FALSE;\
        iState = StateStop;\
        RunIfNotReady();\
        break;\
    }

#define CHECK_MEM_INSTANCE_TEST(m, s) \
    if (NULL == (m))\
    {\
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "OmxDecTestMultipleInstance - %s Error, Memory Allocation Failed", s));\
        iTestStatus = OMX_FALSE;\
        iState = StateStop;\
        RunIfNotReady();\
        break;\
    }


/*
 * Active Object class's Run () function
 * Control all the states of AO & sends API's to the component
 */
void OmxDecTestMultipleInstance::Run()
{
    switch (iState)
    {
        case StateUnLoaded:
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "OmxDecTestMultipleInstance::Run() - StateUnLoaded IN"));

            OMX_ERRORTYPE Err;
            OMX_U32 ii;
            OMX_U32 NumComps = 0;
            OMX_STRING* pCompOfRole = NULL;
            OMX_BOOL IsRoleSupported = OMX_FALSE;
            OMX_HANDLETYPE LocalCompHandle[MAX_INSTANCE];

            if (!iCallbacks->initCallbacks())
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "OmxDecTestMultipleInstance::Run() - ERROR initCallbacks failed, OUT"));
                iTestStatus = OMX_FALSE;
                iState = StateStop;
                RunIfNotReady();
                break;
            }

            ipAppPriv = (AppPrivateType*) oscl_malloc(sizeof(AppPrivateType));
            CHECK_MEM_INSTANCE_TEST(ipAppPriv, "Component_Handle");
            ipAppPriv->Handle = NULL;

            //This should be the first call to the component to load it.
            Err = OMX_MasterInit();
            CHECK_ERROR_INSTANCE_TEST(Err, "OMX_MasterInit");
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG, (0, "OmxDecTestMultipleInstance::Run() - OMX_MasterInit done"));


            if (NULL != iName)
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                                (0, "OmxDecTestMultipleInstance::Run() - Finding out whether the component %s supports the correct role", iName));

                //Given the component name, verify whether it supports correct role or not
                Err = OMX_MasterGetRolesOfComponent(iName, &NumComps, NULL);

                if (OMX_ErrorNone != Err || NumComps < 1)
                {
                    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "OmxDecTestMultipleInstance::Run() - ERROR, No role is supported by the given component"));
                    iTestStatus = OMX_FALSE;
                    ipAppPriv->Handle = NULL;
                    iState = StateStop;
                    RunIfNotReady();
                    break;
                }

                pCompOfRole = (OMX_STRING*) oscl_malloc(NumComps * sizeof(OMX_STRING));
                CHECK_MEM_INSTANCE_TEST(pCompOfRole, "ComponentRoleArray");

                for (ii = 0; ii < NumComps; ii++)
                {
                    pCompOfRole[ii] = (OMX_STRING) oscl_malloc(PV_OMX_MAX_COMPONENT_NAME_LENGTH * sizeof(OMX_U8));
                    CHECK_MEM_INSTANCE_TEST(pCompOfRole[ii], "ComponentRoleArray");
                }

                if (OMX_FALSE == iTestStatus)
                {
                    break;
                }

                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                                (0, "OmxDecTestMultipleInstance::Run() - Find out the roles supported now"));

                Err = OMX_MasterGetRolesOfComponent(iName, &NumComps, (OMX_U8**) pCompOfRole);
                CHECK_ERROR_INSTANCE_TEST(Err, "GetRolesOfComponent");

                for (ii = 0; ii < NumComps; ii++)
                {
                    if (NULL != oscl_strstr(pCompOfRole[ii], iRole))
                    {
                        IsRoleSupported = OMX_TRUE;
                        break;
                    }
                }

                if (OMX_FALSE == IsRoleSupported)
                {
                    iTestStatus = OMX_FALSE;
                    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                                    (0, "OmxDecTestMultipleInstance::Run() - ERROR, Required Component Role not supported!"));
                    iState = StateStop;
                    RunIfNotReady();
                    break;
                }

                Err = OMX_MasterGetHandle(&ipAppPriv->Handle, iName, (OMX_PTR) this , iCallbacks->getCallbackStruct());

                // check if there was a problem
                CHECK_ERROR_INSTANCE_TEST(Err, "GetHandle");
                CHECK_MEM_INSTANCE_TEST(ipAppPriv->Handle, "ComponentHandle");

                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG, (0, "OmxDecTestMultipleInstance::Run() - Got first handle for the component %s", iName));

                //Now try loading and unloading the same component multiple times
                //to test dynamic loading/unloading
                OMX_S32 jj;

                for (jj = 0; jj < MAX_INSTANCE; jj++)
                {
                    LocalCompHandle[jj] = NULL;
                    Err = OMX_MasterGetHandle(&LocalCompHandle[jj], iName, (OMX_PTR) this , iCallbacks->getCallbackStruct());
                    CHECK_ERROR_INSTANCE_TEST(Err, "GetHandle");
                    CHECK_MEM_INSTANCE_TEST(LocalCompHandle[jj], "ComponentHandle");

                    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG, (0, "OmxDecTestMultipleInstance::Run() - Got another Handle number %d for the component %s", (jj + 2), iName));
                }

                for (jj = MAX_INSTANCE - 1; jj >= 0; jj--)
                {
                    Err = OMX_MasterFreeHandle(LocalCompHandle[jj]);
                    CHECK_ERROR_INSTANCE_TEST(Err, "FreeHandle");
                    LocalCompHandle[jj] = NULL;

                    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG, (0, "OmxDecTestMultipleInstance::Run() - Free Handle number %d for the component %s", (jj + 2), iName));
                }

                //Free the first allocated component handle
                Err = OMX_MasterFreeHandle(ipAppPriv->Handle);
                CHECK_ERROR_INSTANCE_TEST(Err, "FreeHandle_ipAppPriv");
                ipAppPriv->Handle = NULL;

                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG, (0, "OmxDecTestMultipleInstance::Run() - Free last handle for the component %s", iName));

                //Instantiate the first component back
                Err = OMX_MasterGetHandle(&ipAppPriv->Handle, iName, (OMX_PTR) this , iCallbacks->getCallbackStruct());
                CHECK_ERROR_INSTANCE_TEST(Err, "GetHandle");
                CHECK_MEM_INSTANCE_TEST(ipAppPriv->Handle, "ComponentHandle");

                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG, (0, "OmxDecTestMultipleInstance::Run() - Got again Handle for the component %s", iName));

            }
            else if (NULL != iRole)
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG, (0, "OmxDecTestMultipleInstance::Run() - Finding out the components that can support the role %s", iRole));

                //Given the role, determine the component first & then get the handle
                // Call once to find out the number of components that can fit the role
                Err = OMX_MasterGetComponentsOfRole(iRole, &NumComps, NULL);

                if (OMX_ErrorNone != Err || NumComps < 1)
                {
                    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "OmxDecTestMultipleInstance::Run() - ERROR, No component can handle the specified role %s", iRole));
                    iTestStatus = OMX_FALSE;
                    ipAppPriv->Handle = NULL;
                    iState = StateStop;
                    RunIfNotReady();
                    break;
                }

                pCompOfRole = (OMX_STRING*) oscl_malloc(NumComps * sizeof(OMX_STRING));
                CHECK_MEM_INSTANCE_TEST(pCompOfRole, "ComponentRoleArray");

                for (ii = 0; ii < NumComps; ii++)
                {
                    pCompOfRole[ii] = (OMX_STRING) oscl_malloc(PV_OMX_MAX_COMPONENT_NAME_LENGTH * sizeof(OMX_U8));
                    CHECK_MEM_INSTANCE_TEST(pCompOfRole[ii], "ComponentRoleArray");
                }

                if (OMX_FALSE == iTestStatus)
                {
                    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR,
                                    (0, "OmxDecTestMultipleInstance::Run() - Error occured in this state, StateUnLoaded OUT"));
                    break;
                }

                // call 2nd time to get the component names
                Err = OMX_MasterGetComponentsOfRole(iRole, &NumComps, (OMX_U8**) pCompOfRole);
                CHECK_ERROR_INSTANCE_TEST(Err, "GetComponentsOfRole");

                for (ii = 0; ii < NumComps; ii++)
                {
                    // try to create component
                    Err = OMX_MasterGetHandle(&ipAppPriv->Handle, (OMX_STRING) pCompOfRole[ii], (OMX_PTR) this, iCallbacks->getCallbackStruct());
                    // if successful, no need to continue
                    if ((OMX_ErrorNone == Err) && (NULL != ipAppPriv->Handle))
                    {
                        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG, (0, "OmxDecTestMultipleInstance::Run() - Got first Handle for the component %s", pCompOfRole[ii]));
                        break;
                    }
                    else
                    {
                        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "OmxDecTestMultipleInstance::Run() - ERROR, Cannot get component %s handle, try another if possible", pCompOfRole[ii]));
                    }
                }

                CHECK_ERROR_INSTANCE_TEST(Err, "GetHandle");
                CHECK_MEM_INSTANCE_TEST(ipAppPriv->Handle, "ComponentHandle");


                //Now try loading and unloading the same component multiple times
                //to test dynamic loading/unloading
                OMX_S32 jj;
                for (jj = 0; jj < MAX_INSTANCE; jj++)
                {
                    LocalCompHandle[jj] = NULL;
                    Err = OMX_MasterGetHandle(&LocalCompHandle[jj], (OMX_STRING) pCompOfRole[ii], (OMX_PTR) this , iCallbacks->getCallbackStruct());
                    CHECK_ERROR_INSTANCE_TEST(Err, "GetHandle");
                    CHECK_MEM_INSTANCE_TEST(LocalCompHandle[jj], "ComponentHandle");

                    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG, (0, "OmxDecTestMultipleInstance::Run() - Got another Component Handle number %d for the component %s", (jj + 2), pCompOfRole[ii]));
                }

                for (jj = MAX_INSTANCE - 1; jj >= 0; jj--)
                {
                    Err = OMX_MasterFreeHandle(LocalCompHandle[jj]);
                    CHECK_ERROR_INSTANCE_TEST(Err, "FreeHandle");
                    LocalCompHandle[jj] = NULL;

                    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG, (0, "OmxDecTestMultipleInstance::Run() - Free Handle number %d for the component %s", (jj + 2), pCompOfRole[ii]));
                }

                //Free the first allocated component handle
                Err = OMX_MasterFreeHandle(ipAppPriv->Handle);
                CHECK_ERROR_INSTANCE_TEST(Err, "FreeHandle_ipAppPriv");
                ipAppPriv->Handle = NULL;

                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG, (0, "OmxDecTestMultipleInstance::Run() - Free last Handle for the component %s", pCompOfRole[ii]));

                //Instantiate the first component back
                Err = OMX_MasterGetHandle(&ipAppPriv->Handle, (OMX_STRING) pCompOfRole[ii], (OMX_PTR) this , iCallbacks->getCallbackStruct());
                CHECK_ERROR_INSTANCE_TEST(Err, "GetHandle");
                CHECK_MEM_INSTANCE_TEST(ipAppPriv->Handle, "ComponentHandle");

                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG, (0, "OmxDecTestMultipleInstance::Run() - Got again Handle for the component %s", pCompOfRole[ii]));
            }

            //Free the role of component arrays
            if (pCompOfRole)
            {
                for (ii = 0; ii < NumComps; ii++)
                {
                    oscl_free(pCompOfRole[ii]);
                    pCompOfRole[ii] = NULL;
                }
                oscl_free(pCompOfRole);
                pCompOfRole = NULL;
            }

            iState = StateStop;
            RunIfNotReady();
        }
        break;

        /********* FREE THE HANDLE & CLOSE FILES FOR THE COMPONENT ********/
        case StateStop:
        {
            OMX_U8 TestName[] = "MULTIPLE_INSTANCE_TEST";
            OMX_ERRORTYPE Err = OMX_ErrorNone;

            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "OmxDecTestMultipleInstance::Run() - StateStop IN"));

            if (ipAppPriv)
            {
                if (ipAppPriv->Handle)
                {
                    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                                    (0, "OmxDecTestMultipleInstance::Run() - Free the Component Handle"));

                    Err = OMX_MasterFreeHandle(ipAppPriv->Handle);
                    if (OMX_ErrorNone != Err)
                    {
                        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "OmxDecTestMultipleInstance::Run() - FreeHandle Error"));
                        iTestStatus = OMX_FALSE;
                    }
                }
            }

            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                            (0, "OmxDecTestMultipleInstance::Run() - De-initialize the omx component"));

            Err = OMX_MasterDeinit();
            if (OMX_ErrorNone != Err)
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "OmxDecTestMultipleInstance::Run() - OMX_MasterDeinit Error"));
                iTestStatus = OMX_FALSE;
            }

            if (ipAppPriv)
            {
                oscl_free(ipAppPriv);
                ipAppPriv = NULL;
            }

            if (OMX_FALSE == iTestStatus)
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                                (0, "OmxDecTestMultipleInstance::Run() - %s: Fail", TestName));
#ifdef PRINT_RESULT
                fprintf(iConsOutFile, "%s: Fail \n", TestName);
                OMX_DEC_TEST(false);
                iTestCase->TestCompleted();
#endif

            }
            else
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                                (0, "OmxDecTestMultipleInstance::Run() - %s: Success", TestName));
#ifdef PRINT_RESULT
                fprintf(iConsOutFile, "%s: Success \n", TestName);
                OMX_DEC_TEST(true);
                iTestCase->TestCompleted();
#endif
            }

            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "OmxDecTestMultipleInstance::Run() - StateStop OUT"));

            iState = StateUnLoaded;
            OsclExecScheduler* sched = OsclExecScheduler::Current();
            sched->StopScheduler();
        }
        break;

        default:
        {
            break;
        }
    }
    return;
}
