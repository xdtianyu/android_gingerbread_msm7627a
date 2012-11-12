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

#ifndef PVMF_PMEM_BUFFER_ALLOC_H_INCLUDED
#define PVMF_PMEM_BUFFER_ALLOC_H_INCLUDED

#ifndef OSCL_BASE_H_INCLUDED
    #include "oscl_base.h"
#endif

#ifndef OSCL_VECTOR_H_INCLUDED
    #include "oscl_vector.h"
#endif

#ifndef OSCL_MEM_H_INCLUDED
    #include "oscl_mem.h"
#endif

#ifndef OSCL_MUTEX_H_INCLUDED
    #include "oscl_mutex.h"
#endif

/**
 * This interface is used to allocate a buffers from pmem.
 */
class PVMFPMemBufferAlloc {
public:

    PVMFPMemBufferAlloc();
    ~PVMFPMemBufferAlloc();

    /**
     * This method allocates a pmem buffer of requested size buffer.
     *
     * @returns a ptr to a pmem buffer and fd.
     * or NULL if there is an error and -1 for fd.
     */
    virtual OsclAny* allocate(int32 nSize, int32 *pmem_fd);

    /**
     * This method deallocates a buffer ptr that was previously
     * allocated through the allocate method.
     *
     * @param ptr is a ptr to the previously allocated buffer to release.
     */
    virtual void deallocate(OsclAny* ptr, int32 pmem_fd);


private:

    /**
     * This method ensures that the interface cleanups all the
     * memory, whichever is not freed by the client.
     * 
     */
    void cleanup();

    // List of PMEM fds and memory allocated
    class BuffersAllocated {
    public:
        BuffersAllocated(OsclAny *buf, int32 nSize, int32 fd) :
        iPMem_buf(buf), iPMem_bufsize(nSize), iPMem_fd(fd)
        {}
        OsclAny* iPMem_buf;
        int32 iPMem_bufsize;
        int32 iPMem_fd;
    };

    Oscl_Vector<BuffersAllocated,OsclMemAllocator> iBuffersAllocQueue;

    // lock used to access the Buffers allocated queue
    OsclMutex iBuffersAllocQueueLock;
    int32 nNumBufferAllocated;
};

#endif
