/* ------------------------------------------------------------------
 * Copyright (C) 2009 PacketVideo
 * Copyright (c) 2010, Code Aurora Forum. All rights reserved.
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

#include <utils/Log.h>
#include <camera/CameraParameters.h>
#include <utils/Errors.h>
#include <media/mediarecorder.h>
#include <surfaceflinger/ISurface.h>
#include <camera/ICamera.h>
#include <camera/Camera.h>

#include "android_camera_input_threadsafe_callbacks.h"
#include "android_camera_input.h"
#define LOG_TAG "android_camera_input_threadsafe_callbacks"

namespace android {

AndroidCameraInputThreadSafeCallbackAO::AndroidCameraInputThreadSafeCallbackAO(void* aObserver,
                                                                               uint32 aDepth,
                                                                               const char* aAOname,
                                                                               int32 aPriority)
:ThreadSafeCallbackAO(aObserver, aDepth, aAOname, aPriority)
{

}


AndroidCameraInputThreadSafeCallbackAO::~AndroidCameraInputThreadSafeCallbackAO()
{

}


OsclReturnCode AndroidCameraInputThreadSafeCallbackAO::ProcessEvent(OsclAny* EventData)
{
    if (iObserver != NULL)
    {
        AndroidCameraInput* ptr = (AndroidCameraInput*) iObserver;
        if(ptr->IsAdded())
        {
            ptr->RunIfNotReady();
        }
    }
    return OsclSuccess;
}

#if PROCESS_MULTIPLE_EVENTS_IN_CALLBACK
void AndroidCameraInputThreadSafeCallbackAO::Run()
{
    OsclAny *param;
    OsclReturnCode status = OsclSuccess;
    // Process multiple events in one attempt
    do
    {
        param = Dequeue(status);
        if((status == OsclSuccess) || (status == OsclPending))
        {
            ProcessEvent(param);
        }
    }while(status == OsclSuccess);
}

OsclAny* AndroidCameraInputThreadSafeCallbackAO::Dequeue(OsclReturnCode &status)
{
    OsclAny *param;
    OsclProcStatus::eOsclProcError sema_status;

    status = OsclSuccess;

    Mutex.Lock();
    if(Q->NumElem == 0)
    {
        status = OsclFailure;
        Mutex.Unlock();
        return NULL;
    }

    param = (Q->pFirst[Q->index_out]).pData;
    Q->index_out++;
    if(Q->index_out == Q->MaxNumElements)
        Q->index_out = 0;
    Q->NumElem--;
    if(Q->NumElem == 0)
    {
        // Call PendForExec() only when there are no more elements in the queue
        // Else, continue in the do-while loop
        PendForExec();
        status = OsclPending;
    }
    Mutex.Unlock();

    sema_status = RemoteThreadCtrlSema.Signal();
    if(sema_status != OsclProcStatus::SUCCESS_ERROR)
    {
        status = OsclFailure;
        return NULL;
    }
    return param;
}
#endif

}; // namespace android

