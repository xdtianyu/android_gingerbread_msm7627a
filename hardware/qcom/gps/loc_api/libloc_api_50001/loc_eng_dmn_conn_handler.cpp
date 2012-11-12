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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <loc_eng_dbg.h>

#include "loc_eng_dmn_conn_handler.h"

#ifndef DEBUG_DMN_LOC_API
void loc_eng_if_wakeup(int if_req, unsigned is_supl, unsigned long ipv4_addr, unsigned char * ipv6_addr);
#endif

int loc_eng_dmn_conn_loc_api_server_if_request_handler(struct ctrl_msgbuf *pmsg, int len)
{
    LOC_LOGD("%s:%d]\n", __func__, __LINE__);
#ifndef DEBUG_DMN_LOC_API
    loc_eng_if_wakeup( 1,
                       pmsg->cmsg.cmsg_if_request.is_supl,
                       pmsg->cmsg.cmsg_if_request.ipv4_addr,
                       pmsg->cmsg.cmsg_if_request.ipv6_addr);
#endif
    return 0;
}

int loc_eng_dmn_conn_loc_api_server_if_release_handler(struct ctrl_msgbuf *pmsg, int len)
{
    LOC_LOGD("%s:%d]\n", __func__, __LINE__);
#ifndef DEBUG_DMN_LOC_API
    loc_eng_if_wakeup( 0,
                       pmsg->cmsg.cmsg_if_request.is_supl,
                       pmsg->cmsg.cmsg_if_request.ipv4_addr,
                       pmsg->cmsg.cmsg_if_request.ipv6_addr);
#endif
    return 0;
}

