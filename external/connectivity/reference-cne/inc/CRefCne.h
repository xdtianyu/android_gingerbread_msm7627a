#ifndef REF_CNE_H
#define REF_CNE_H

/**----------------------------------------------------------------------------
  @file REF_CNE.h


-----------------------------------------------------------------------------*/

/* Copyright (c) 2010, Code Aurora Forum. All rights reserved.
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
#include "RefCneDefs.h"
#include "CRefCneRadio.h"


/*----------------------------------------------------------------------------
 * Preprocessor Definitions and Constants
 * -------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
 * Type Declarations
 * -------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
 * Class Definitions
 * -------------------------------------------------------------------------*/
class CRefCne
{
public:
  /**
    @brief Returns an instance of the CneSpm class.

    The user of this class will call this function to get an
    instance of the class. All other public functions will be
    called  on this instance

   @param  None
   @see    None
   @return  An instance of the CneSpm class is returned.
  */
  static CRefCne* getInstance ();
  static void RefCneCmdHdlr
    (
    int ,
    int ,
    void*
    );

private:
  /* Wlan notification command format */
  typedef struct _Wlan {
    int type;
    int status;
    int rssi;
    char ssid[32];
  } refCneWlanInfoCmdFmt;

  /* Wwan notification command format */
  typedef struct _Wwan {
    int type;
    int status;
    int rssi;
    int roaming;
  } refCneWwanInfoCmdFmt;

  CRefCne();
  ~CRefCne();
  static CRefCne* m_sInstancePtr;
  int m_iNumActiveNetworks;
  static cne_rat_type m_siPrefNetwork;
  CRefCneRadio* RefCneWifi;
  CRefCneRadio* RefCneWwan;


  ref_cne_ret_enum_type UpdateWlanInfoCmd
  (
    void*
  );

  ref_cne_ret_enum_type UpdateWwanInfoCmd
  (
    void*
  );

  ref_cne_ret_enum_type SetPrefNetCmd
  (
    void *
  );
  void SetPreferredNetwork
  (
    cne_rat_type *
  );
  cne_rat_type GetPreferredNetwork();
  void ProcessStateChange();
};

#endif /* REF_CNE_H */
