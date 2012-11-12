#ifndef REF_CNE_RADIO_H
  #define REF_CNE_RADIO_H

/**----------------------------------------------------------------------------
  @file REF_CNE_RADIO.h


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

/*----------------------------------------------------------------------------
 * Preprocessor Definitions and Constants
 * -------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
 * Type Declarations
 * -------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
 * Class Definitions
 * -------------------------------------------------------------------------*/
class CRefCneRadio
{
public:
  /**
  * @brief Returns an instance of the RefCneRadio class.

    The user of this class will call this function to get an
    instance of the class. All other public functions will be
    called  on this instance

   @param  None
   @see    None
  *@return  An instance of the RCneRadio class is returned.
  **/

  //Constructor
  CRefCneRadio
  (
    cne_rat_type
  );

  //Destructor
  ~CRefCneRadio(){};

  // returns if True if status is connected, else false
  bool bIsDataConnected ();

  int iIsConActionPending ();

  void ClearPending ();

  bool bIsConStateChanged ();

  void UpdateStatus
  (
    int
  );

  //Turn ON Radio
  void TurnOn ();

  //Turn OFF Radio
  void TurnOff ();

  void SetPending
  (
    ref_cne_net_con_req_enum_type
  );

private:

  //private member variables
  cne_network_state_enum_type m_iNetState;
  cne_rat_type m_iMyRatType;
  ref_cne_net_con_req_enum_type m_iRequestState;
  ref_cne_net_con_status_enum_type m_iNetConState;
  ref_cne_net_con_status_enum_type m_iPrevNetConState;

  /* Impicit Contructor which cannot be explicitly called */
  CRefCneRadio();
  CRefCneRadio(const CRefCneRadio& radio);
};
#endif /* REF_CNE_RADIO_H */


