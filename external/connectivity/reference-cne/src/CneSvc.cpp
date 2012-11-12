/*============================================================================
  FILE:         CneSvc.cpp

  OVERVIEW:     Provide an overview of the implementation contained in this
                file.  Do not rehash the user-level documentation from the
                header file.  Maintainers, not users of this code will be
                reading this.  Describe anything of interest about the
                algorithms or design, and call attention to anything tricky
                or potentially confusing.

  DEPENDENCIES: If the code in this file has any notable dependencies,
                describe them here.  Any initialization and sequencing
                requirements, or assumptions about the overall state of
                the system belong here.
============================================================================*/

/* Copyright (c) 2010, 2011 Code Aurora Forum. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of Code Aurora nor
 *       the names of its contributors may be used to endorse or promote
 *       products derived from this software without specific prior written
 *       permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NON-INFRINGEMENT ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */


/*----------------------------------------------------------------------------
 * Include Files
 * -------------------------------------------------------------------------*/
#include "cne.h"
#include "CRefCne.h"
#include "RefCneDefs.h"

/*----------------------------------------------------------------------------
 * Preprocessor Definitions and Constants
 * -------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
 * Type Declarations
 * -------------------------------------------------------------------------*/
#ifdef __cplusplus
  extern "C" {
#endif /* __cplusplus */

extern void cnd_regCommandsNotificationCb
(
  int,
  void (*)(int,int,void*),
  int
);

#ifdef __cplusplus
  }
#endif /* __cplusplus */

/*----------------------------------------------------------------------------
 * Global Data Definitions
 * -------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------
 * Static Variable Definitions
 * -------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
 * Static/Class member Function Declarations and Definitions
 * -------------------------------------------------------------------------*/
static cne_messageCbType cnd_sendUnsolicitedMsg; 

/*----------------------------------------------------------------------------
 * Externalized Function Definitions
 * -------------------------------------------------------------------------*/
extern "C" void cne_init
(
  void
)
{
}
/*----------------------------------------------------------------------------
 * FUNCTION      Function Name

 * DESCRIPTION   Function Description

 * DEPENDENCIES

 * RETURN VALUE

 * SIDE EFFECTS
 *--------------------------------------------------------------------------*/
extern "C" void
cne_processCommand
(
  int fd,
  int cmd,
  void *cmd_data,  /* event data depends on the type of event */
  int cmd_len
)
{
  //CRefCne::getInstance()->RefCneCmdHdlr(cmd, cmd_len, cmd_data);
  CRefCne::RefCneCmdHdlr(cmd, cmd_len, cmd_data);
  return;
}
/*----------------------------------------------------------------------------
 * FUNCTION      Function Name

 * DESCRIPTION   Function Description

 * DEPENDENCIES  

 * RETURN VALUE  

 * SIDE EFFECTS  
 *--------------------------------------------------------------------------*/
extern "C" void cne_sendUnsolicitedMsg
(
  int targetFd, 
  int msgType, 
  int dataLen, 
  void *data
)
{

  RCNE_MSG_ERROR("cne_sendUnsolicitedMsg called");
  cnd_sendUnsolicitedMsg(targetFd,
                         msgType,
                         dataLen,
                         data);

  RCNE_MSG_ERROR("cne_sendUnsolicitedMsg called GOT");
}
/*----------------------------------------------------------------------------
 * FUNCTION      Function Name

 * DESCRIPTION   Function Description

 * DEPENDENCIES  

 * RETURN VALUE  

 * SIDE EFFECTS  
 *--------------------------------------------------------------------------*/
extern "C" void 
cne_regMessageCb
(
  cne_messageCbType cbFn
)
{
  cnd_sendUnsolicitedMsg = cbFn;
  return;
}
/*----------------------------------------------------------------------------
 * FUNCTION      Function Name

 * DESCRIPTION   Function Description

 * DEPENDENCIES

 * RETURN VALUE

 * SIDE EFFECTS
 *--------------------------------------------------------------------------*/
int cne_svc_init
(
  void
)
{
  /* create the RefCne obj */
  RCNE_MSG_DEBUG("Reference CNE init called");
  (void) CRefCne::getInstance();
  return CNE_SERVICE_DISABLED;
}


