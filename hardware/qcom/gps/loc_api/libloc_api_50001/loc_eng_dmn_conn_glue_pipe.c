/* Copyright (c) 2011, Code Aurora Forum. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of Code Aurora Forum, Inc. nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */
#include <string.h>
#include <unistd.h>
#include <errno.h>

// #include <linux/stat.h>
#include <fcntl.h>
// #include <linux/types.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <loc_eng_dbg.h>

#include "loc_eng_dmn_conn_glue_pipe.h"
#include "loc_eng_dbg.h"

/*===========================================================================
FUNCTION    loc_eng_dmn_conn_glue_pipeget

DESCRIPTION
   create a named pipe.

   pipe_name - pipe name path
   mode - mode

DEPENDENCIES
   None

RETURN VALUE
   0: success or negative value for failure

SIDE EFFECTS
   N/A

===========================================================================*/
int loc_eng_dmn_conn_glue_pipeget(const char * pipe_name, int mode)
{
    int fd;
    int result;

    LOC_LOGD("%s:%d] %s, mode = %d\n", __func__, __LINE__, pipe_name, mode);
    result = mkfifo(pipe_name, 0666);

    if ((result == -1) && (errno != EEXIST)) {
        LOC_LOGE("%s:%d] failed: %s\n", __func__, __LINE__, strerror(errno));
        return result;
    }

    fd = open(pipe_name, mode);
    if (fd <= 0)
    {
        LOC_LOGE("%s:%d] failed: %s\n", __func__, __LINE__, strerror(errno));
    }
    LOC_LOGD("%s:%d] fd = %d, %s\n", __func__, __LINE__, fd, pipe_name);
    return fd;
}

/*===========================================================================
FUNCTION    loc_eng_dmn_conn_glue_piperemove

DESCRIPTION
   remove a pipe

    pipe_name - pipe name path
    fd - fd for the pipe

DEPENDENCIES
   None

RETURN VALUE
   0: success

SIDE EFFECTS
   N/A

===========================================================================*/
int loc_eng_dmn_conn_glue_piperemove(const char * pipe_name, int fd)
{
    close(fd);
    if (pipe_name) unlink(pipe_name);
    LOC_LOGD("%s:%d] fd = %d, %s\n", __func__, __LINE__, fd, pipe_name);
    return 0;
}

/*===========================================================================
FUNCTION    loc_eng_dmn_conn_glue_pipewrite

DESCRIPTION
   write to a pipe

   fd - fd of a pipe
   buf - buffer for the data to write
   sz - size of the data in buffer

DEPENDENCIES
   None

RETURN VALUE
   number of bytes written or negative value for failure

SIDE EFFECTS
   N/A

===========================================================================*/
int loc_eng_dmn_conn_glue_pipewrite(int fd, const void * buf, size_t sz)
{
    int result;

    LOC_LOGD("%s:%d] fd = %d, buf = 0x%lx, size = %d\n", __func__, __LINE__, fd, (long) buf, (int) sz);

    result = write(fd, buf, sz);

    /* @todo check for non EINTR & EAGAIN, shall not do select again, select_tut Law 7) */

    LOC_LOGD("%s:%d] fd = %d, result = %d\n", __func__, __LINE__, fd, result);
    return result;
}

/*===========================================================================
FUNCTION    loc_eng_dmn_conn_glue_piperead

DESCRIPTION
   read from a pipe

   fd - fd for the pipe
   buf - buffer to hold the data read from pipe
   sz - size of the buffer

DEPENDENCIES
   None

RETURN VALUE
   number of bytes read from pipe or negative value for failure

SIDE EFFECTS
   N/A

===========================================================================*/
int loc_eng_dmn_conn_glue_piperead(int fd, void * buf, size_t sz)
{
    int len;

    LOC_LOGD("%s:%d] fd = %d, buf = 0x%lx, size = %d\n", __func__, __LINE__, fd, (long) buf, (int) sz);

    len = read(fd, buf, sz);

    /* @todo check for non EINTR & EAGAIN, shall not do select again, select_tut Law 7) */

    LOC_LOGD("%s:%d] fd = %d, len = %d\n", __func__, __LINE__, fd, len);
    return len;
}

/*===========================================================================
FUNCTION    loc_eng_dmn_conn_glue_pipeunblock

DESCRIPTION
   unblock a pipe

   fd - fd for the pipe

DEPENDENCIES
   None

RETURN VALUE
   0 for success or negative value for failure

SIDE EFFECTS
   N/A

===========================================================================*/
int loc_eng_dmn_conn_glue_pipeunblock(int fd)
{
    int result;
    struct flock flock_v;
    LOC_LOGD("%s:%d]\n", __func__, __LINE__);
//    result = fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NDELAY);
    flock_v.l_type = F_UNLCK;
    flock_v.l_len = 32;
    result = fcntl(fd, F_SETLK, &flock_v);
    if (result < 0) {
        LOC_LOGE("%s:%d] fcntl failure, %s\n", __func__, __LINE__, strerror(errno));
    }

    return result;
}
