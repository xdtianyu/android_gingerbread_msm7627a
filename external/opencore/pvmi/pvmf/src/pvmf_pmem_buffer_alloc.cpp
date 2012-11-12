/* ------------------------------------------------------------------
 * Copyright (C) 1998-2009 PacketVideo
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

#include "pvmf_pmem_buffer_alloc.h"
#include "pvlogger.h"

//#define LOG_NDEBUG 0
#ifdef ANDROID
#define LOG_TAG "PMEMBufferAlloc"
#include <utils/Log.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>

PVMFPMemBufferAlloc::PVMFPMemBufferAlloc() :
nNumBufferAllocated(0)
{
    iBuffersAllocQueueLock.Create();
}

PVMFPMemBufferAlloc::~PVMFPMemBufferAlloc()
{
#ifdef ANDROID
    LOGE("PVMFPMemBufferAlloc::~PVMFPMemBufferAlloc with the num buff as %d", nNumBufferAllocated);
#endif
    // Not all the buffers is properly cleaned.
    if ( nNumBufferAllocated != 0 ) {
        // cleanup all the memory
        cleanup();
    }

    iBuffersAllocQueueLock.Close();
}

OsclAny* PVMFPMemBufferAlloc::allocate(int32 nSize, int32 *pmem_fd)
{
    int32 pmemfd = -1;
    void  *pmem_buf = NULL;


    // 1. Open the pmem_audio
    pmemfd = open("/dev/pmem_audio", O_RDWR);

    if ( pmemfd < 0 ) {
#ifdef ANDROID
        LOGE("PVMFPMemBufferAlloc::allocate failed to open pmem_audio");
#endif
        *pmem_fd = -1;
        return pmem_buf;
    }

    // 2. MMAP to get the virtual address
    pmem_buf = mmap(0, nSize, PROT_READ | PROT_WRITE, MAP_SHARED, pmemfd, 0);

    if ( NULL == pmem_buf ) {
#ifdef ANDROID
        LOGE("PVMFPMemBufferAlloc::allocate failed to mmap");
#endif
        *pmem_fd = -1;
        return pmem_buf;
    }

    // 3. Store this information for internal mapping / maintanence
    BuffersAllocated buf((OsclAny*) pmem_buf, nSize, pmemfd);

    // 4. Lock the queue, insert the item , update the count and unlock
    iBuffersAllocQueueLock.Lock();

    iBuffersAllocQueue.push_back(buf);

    nNumBufferAllocated++;
    iBuffersAllocQueueLock.Unlock();

    // 5. Send the pmem fd information
    *pmem_fd = pmemfd;
#ifdef ANDROID
    LOGE("PVMFPMemBufferAlloc::allocate calling with required size %d", nSize);
    LOGE("The PMEM that is allocated is %d and buffer is %x", pmemfd, pmem_buf);
    LOGE("The queue size is %d and num buff allocated is %d", iBuffersAllocQueue.size(), nNumBufferAllocated);
#endif
    // 6. Return the virtual address
    return(OsclAny*)pmem_buf;
}

void PVMFPMemBufferAlloc::deallocate(OsclAny *ptr, int32 pmem_fd)
{
    iBuffersAllocQueueLock.Lock();

    for ( int32 i = (iBuffersAllocQueue.size() - 1); i >= 0; i-- ) {

        // 1. if the buffer address match, delete from the queue
        if ( (iBuffersAllocQueue[i].iPMem_buf == ptr) ||
             (iBuffersAllocQueue[i].iPMem_fd == pmem_fd) ) {

            munmap(iBuffersAllocQueue[i].iPMem_buf, iBuffersAllocQueue[i].iPMem_bufsize);

            // Close the PMEM fd
            close(iBuffersAllocQueue[i].iPMem_fd);

            // Remove the pmem info from the queue
            iBuffersAllocQueue.erase(&iBuffersAllocQueue[i]);

            nNumBufferAllocated--;
        }
    }
#ifdef ANDROID
    LOGE("Inside deallocate and the queue size is set to %d with numbuf as %d", iBuffersAllocQueue.size(), nNumBufferAllocated);
#endif
    iBuffersAllocQueueLock.Unlock();
}

void PVMFPMemBufferAlloc::cleanup()
{
    iBuffersAllocQueueLock.Lock();

    for ( int32 i = (iBuffersAllocQueue.size() - 1); i >= 0; i-- ) {

        munmap(iBuffersAllocQueue[i].iPMem_buf, iBuffersAllocQueue[i].iPMem_bufsize);

        // Close the PMEM fd
        close(iBuffersAllocQueue[i].iPMem_fd);

        // Remove the pmem info from the queue
        iBuffersAllocQueue.erase(&iBuffersAllocQueue[i]);

        nNumBufferAllocated--;
    }

    iBuffersAllocQueueLock.Unlock();
}
