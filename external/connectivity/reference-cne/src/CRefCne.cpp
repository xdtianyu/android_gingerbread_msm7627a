/**----------------------------------------------------------------------------
  @file CRefCne.cpp


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
 * Preprocessor Definitions and Constants
 * -------------------------------------------------------------------------*/
#define TWO_RADIOS_ARE_CONNECTED    2
#define ONE_RADIO_IS_CONNECTED      1
#define ALL_RADIOS_ARE_DISCONNECTED 0

/*----------------------------------------------------------------------------
 * Include Files
 * -------------------------------------------------------------------------*/
#include "cne.h"
#include "CRefCne.h"
#include "CRefCneRadio.h"
#include "RefCneDefs.h"

/*----------------------------------------------------------------------------
 * Static Member declarations
 * -------------------------------------------------------------------------*/
CRefCne* CRefCne::m_sInstancePtr = NULL;
cne_rat_type CRefCne::m_siPrefNetwork = CNE_RAT_WLAN;

/*----------------------------------------------------------------------------
 * Type Declarations
 * -------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
 * Class Definitions
 * -------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------
 * FUNCTION      getInstance

 * DESCRIPTION   The user of this class will call this function to get an
                 instance of the class. All other public functions will be
                 called on this instance

 * DEPENDENCIES  None

 * RETURN VALUE  an instance of CRefCne class

 * SIDE EFFECTS  None
 *--------------------------------------------------------------------------*/
CRefCne* CRefCne::getInstance
(
)
{
  if (m_sInstancePtr == NULL)
  {
    m_sInstancePtr = new CRefCne;
  }
  return(m_sInstancePtr);
}
/*----------------------------------------------------------------------------
 * FUNCTION      Constructor

 * DESCRIPTION   Creates the RefCne object & initializes members appropriately

 * DEPENDENCIES  None

 * RETURN VALUE  an instance of CRefCne class

 * SIDE EFFECTS  RefCne object is created
 *--------------------------------------------------------------------------*/
CRefCne::CRefCne ()
{
  RCNE_MSG_INFO("In reference CNE constructor");
  m_iNumActiveNetworks = NULL;
  RefCneWifi = new  CRefCneRadio(CNE_RAT_WLAN);
  RefCneWwan = new  CRefCneRadio(CNE_RAT_WWAN);
  RCNE_MSG_INFO("Reference CNE constructed");
}
/*----------------------------------------------------------------------------
 * FUNCTION      RefCneCmdHdlr

 * DESCRIPTION   This the master command handler which calls specific handler
                 to handle a particular command sent by the daemon

 * DEPENDENCIES  None

 * RETURN VALUE  None

 * SIDE EFFECTS  None
 *--------------------------------------------------------------------------*/
void CRefCne::RefCneCmdHdlr
(
  int cmd,
  int cmd_len,
  void* pCmdDataPtr
)
{
  cmd = (cne_cmd_enum_type) cmd;
  CRefCne* myself = getInstance();
  switch (cmd)
  {
    case CNE_NOTIFY_DEFAULT_NW_PREF_CMD:
      {
        RCNE_MSG_INFO("Command hdlr: Notify default"
                      " network pref cmd called [%d]",cmd);
        ref_cne_ret_enum_type ret = myself->SetPrefNetCmd(pCmdDataPtr);
        if (ret != REF_CNE_RET_OK)
        {
          break;
        }
        myself->ProcessStateChange();
      }
      break;
    case CNE_REQUEST_UPDATE_WLAN_INFO_CMD:
      {
        RCNE_MSG_INFO("Command hdlr: Update Wifi info cmd called [%d]",cmd);
        ref_cne_ret_enum_type ret = myself->UpdateWlanInfoCmd(pCmdDataPtr);
        if (ret != REF_CNE_RET_OK)
        {
          break;
        }
	myself->ProcessStateChange();
      }
      break;
    case CNE_REQUEST_UPDATE_WWAN_INFO_CMD:
      {
        RCNE_MSG_INFO("Command hdlr: Update WWAN info cmd called [%d]",cmd);
        ref_cne_ret_enum_type ret = myself->UpdateWwanInfoCmd(pCmdDataPtr);
        if (ret != REF_CNE_RET_OK)
        {
          break;
        }
	myself->ProcessStateChange();
      }
      break;
    default:
      {
        RCNE_MSG_ERROR("Command hdlr: unrecognized cmd [%d] recvd",cmd);
      }
  }

}
/*----------------------------------------------------------------------------
 * FUNCTION      ProcessStateChange

 * DESCRIPTION   Processess the change of state of the connectivity engine
                 after the command received from the daemon is processed

 * DEPENDENCIES  None

 * RETURN VALUE  None

 * SIDE EFFECTS  None
 *--------------------------------------------------------------------------*/
void CRefCne::ProcessStateChange
(
)
{
  RCNE_MSG_INFO("PSC:BEGIN processing state change");
  m_iNumActiveNetworks = 0;
  cne_rat_type  myPrefNet = GetPreferredNetwork();
  /* Check if the preferred network is set, if not then phone is
   * in boot up process, so do nothing */
  if ((myPrefNet != CNE_RAT_WLAN) && (myPrefNet!= CNE_RAT_WWAN))
  {
    RCNE_MSG_ERROR("Preferred network unset; waiting until it is set");
    return;
  }

  CRefCneRadio* pref;
  CRefCneRadio* nonpref;
  if (myPrefNet == CNE_RAT_WLAN)
  {
    RCNE_MSG_INFO("PSC: Preferred RAT is Wifi, non-preferred RAT is WWAN");
    pref = RefCneWifi;
    nonpref = RefCneWwan;
  } else
  {
    RCNE_MSG_INFO("PSC: Preferred RAT is WWAN, non-preferred RAT is Wifi");
    pref = RefCneWwan;
    nonpref = RefCneWifi;
  }
  if (RefCneWifi->bIsDataConnected() == TRUE )
  {
    ++m_iNumActiveNetworks;
    RCNE_MSG_INFO("PSC: Wifi is in connected state");
  }
  if (RefCneWwan->bIsDataConnected() == TRUE )
  {
    ++m_iNumActiveNetworks;
    RCNE_MSG_INFO("PSC: WWAN is in connected state");
  }
  switch (m_iNumActiveNetworks)
  {
    case TWO_RADIOS_ARE_CONNECTED:
      /**
       *  If both Radios are up turn off the non-preferred network
       */
      {
        RCNE_MSG_INFO("PSC: both radios are up; checking connect/ disconnect"
                      " request status");
	switch (nonpref->iIsConActionPending())
	{
          case REF_CNE_NET_PENDING_CONNECT:
            /**
             * Check for power-up or out of coverage scenarios
             */
            {
              RCNE_MSG_INFO("PSC: device was in start up or perhaps out of"
                             " range for all networks and now it's in range");
              RCNE_MSG_DEBUG("PSC: turning off non-pref network...");
              nonpref->TurnOff();
              nonpref->SetPending(REF_CNE_NET_PENDING_DISCONNECT);
            }
            break;
          case REF_CNE_NET_PENDING_DISCONNECT:
            {
              RCNE_MSG_DEBUG("PSC: non-pref network is in pending disconnect"
                             " state; waiting for disconnect event");
            }
            break;
          default:
            /**
             * Check for special cases if both networks are up
             */
            {
              RCNE_MSG_DEBUG("PSC: no pending request found for non-pref net,"
                             " checking for pref net request status");
              if (pref->iIsConActionPending() == REF_CNE_NET_PENDING_CONNECT)
              {
                RCNE_MSG_DEBUG("PSC: pref network [%d]  is now available,"
                               " turning off non-pref network",myPrefNet);
                pref->ClearPending();
              }
              else
              {
                RCNE_MSG_DEBUG("PSC: unexpected bringup of non-pref network"
                               "  -- special case ?");
	      }
	      nonpref->TurnOff();
              nonpref->SetPending(REF_CNE_NET_PENDING_DISCONNECT);
            }
        }
      }
      break;
    case ONE_RADIO_IS_CONNECTED:
      /**
       * If only one network is up, check if it is the preferred one,
       * if not then turn on the preferred network
       */
      {
        if (pref->bIsDataConnected() == FALSE)
        {
          RCNE_MSG_INFO("PSC: non pref network is up; requesting"
                         " pref net connection");
          if (pref->iIsConActionPending() == REF_CNE_NET_PENDING_CONNECT)
          {
            RCNE_MSG_DEBUG("PSC: pref net is in pending connect state");
          }
          else
          {
            pref->TurnOn();
            pref->SetPending(REF_CNE_NET_PENDING_CONNECT);
          }
          if (nonpref->iIsConActionPending() == REF_CNE_NET_PENDING_CONNECT)
          {
            nonpref->ClearPending();
          }
        }
        else
        {
          RCNE_MSG_INFO("PSC: Preferred radio is connected");
          if (pref->iIsConActionPending() == REF_CNE_NET_PENDING_CONNECT)
          {
            pref->ClearPending();
          }
          if (nonpref->iIsConActionPending() == REF_CNE_NET_PENDING_DISCONNECT)
          {
            nonpref->ClearPending();
          }
        }
      }
	  break;
    case ALL_RADIOS_ARE_DISCONNECTED:
      /**
       * If both networks are disconnected then try to bring up
       * both networks
       */
      {
        RCNE_MSG_WARN("All radios are disconnected; trying to reconnect");
        if (pref->iIsConActionPending() == REF_CNE_NET_NOT_PENDING)
        {
          pref->TurnOn();
          pref->SetPending(REF_CNE_NET_PENDING_CONNECT);
        }
        else
        {
          RCNE_MSG_DEBUG("PSC: pref net is in pending connect state");
        }
        if (nonpref->iIsConActionPending() == REF_CNE_NET_NOT_PENDING)
        {
          nonpref->TurnOn();
          nonpref->SetPending(REF_CNE_NET_PENDING_CONNECT);
        }
        else
        {
          RCNE_MSG_DEBUG("PSC: non-pref net is in pending connect state");
        }
      }
      break;
    default:
      {
        RCNE_MSG_ERROR("PSC: number of active networks is invalid");
        //ASSERT(0);
      }
  }
}
/*----------------------------------------------------------------------------
 * FUNCTION      UpdateWlanInfoCmd

 * DESCRIPTION   The command handler for UpdateWlanInfo notification

 * DEPENDENCIES  None

 * RETURN VALUE  ref_cne_ret_enum_type

 * SIDE EFFECTS  None
 *--------------------------------------------------------------------------*/
ref_cne_ret_enum_type CRefCne::UpdateWlanInfoCmd
(
  void* pWifiCmdData
)
{
  RCNE_MSG_DEBUG("UWLICH: Wlan update info cmd handler called");
  if (pWifiCmdData == NULL)
  {
    RCNE_MSG_ERROR("UWLICH: Cmd data ptr is Null, bailing out...");
    return(REF_CNE_RET_ERROR);
  }
  refCneWlanInfoCmdFmt *WlanInfoCmd;
  WlanInfoCmd  =   (refCneWlanInfoCmdFmt *) pWifiCmdData;
  RCNE_MSG_INFO("UWLICH: WLAN INFO data is:  rssi=%d, status=%d", WlanInfoCmd->rssi, WlanInfoCmd->status);
  if (WlanInfoCmd->status == -10)
  {
    RCNE_MSG_ERROR("UWLICH: Invalid WLAN status received");
    return(REF_CNE_RET_ERROR);
  }
  RCNE_MSG_DEBUG("UWLICH: WLAN info status is valid, will update status");
  RefCneWifi->UpdateStatus(WlanInfoCmd->status);
  RCNE_MSG_DEBUG("UWLICH: WLAN info status updated");
  RCNE_MSG_INFO("UWLICH: handled Wlan update info cmd");
  return(REF_CNE_RET_OK);
}
/*----------------------------------------------------------------------------
 * FUNCTION      UpdateWwanInfoCmd

 * DESCRIPTION   The command handler for UpdateWwanInfo notification

 * DEPENDENCIES  None

 * RETURN VALUE  ref_cne_ret_enum_type

 * SIDE EFFECTS  None
 *--------------------------------------------------------------------------*/
ref_cne_ret_enum_type CRefCne::UpdateWwanInfoCmd
(
  void* pWwanCmdData
)
{
  RCNE_MSG_DEBUG("UWWICH: Wwan update info cmd handler called");
  if (pWwanCmdData == NULL)
  {
    RCNE_MSG_ERROR("UWWICH: WWAN info data is null, bailing out...");
    return(REF_CNE_RET_ERROR);
  }
  refCneWwanInfoCmdFmt *WwanInfoCmd;
  WwanInfoCmd  =   (refCneWwanInfoCmdFmt *)pWwanCmdData;
  RCNE_MSG_INFO("UWWICH: WWAN info data is: rssi=%d, status=%d", WwanInfoCmd->rssi, WwanInfoCmd->status);
  if (WwanInfoCmd->status == -10)
  {
    RCNE_MSG_ERROR("UWWICH: Invalid WWAN status received");
    return(REF_CNE_RET_ERROR);
  }
  RCNE_MSG_DEBUG("UWWICH: wwan status is valid, now updating status");
  RefCneWwan->UpdateStatus(WwanInfoCmd->status);
  RCNE_MSG_DEBUG("UWWICH: wwan status is updated");
  RCNE_MSG_INFO("UWWICH: handled Wwan update info cmd");
  return(REF_CNE_RET_OK);
}
/*----------------------------------------------------------------------------
 * FUNCTION      SetPrefNetCmd

 * DESCRIPTION   The command handler for set preferred network notification

 * DEPENDENCIES  None

 * RETURN VALUE  ref_cne_ret_enum_type

 * SIDE EFFECTS  None
 *--------------------------------------------------------------------------*/
ref_cne_ret_enum_type CRefCne::SetPrefNetCmd
(
  void* pPrefNetCmdData
)
{
  RCNE_MSG_DEBUG("SPNCH: Set preferred network command handler called");
  if (pPrefNetCmdData == NULL)
  {  
    RCNE_MSG_ERROR("SPNCH: preferred network data is null, bailing out...");
    return(REF_CNE_RET_ERROR);
  }
  cne_rat_type *pPrefNetwork;
  pPrefNetwork = (cne_rat_type *)pPrefNetCmdData;
  if ( (*pPrefNetwork != CNE_RAT_WLAN)&&(*pPrefNetwork != CNE_RAT_WWAN) )
  {
    RCNE_MSG_ERROR("SPNCH: Invalid Network ID [%d] received",*pPrefNetwork);
    return(REF_CNE_RET_ERROR);
  }
  SetPreferredNetwork(pPrefNetwork);
  RCNE_MSG_DEBUG("SPNCH: handled set preferred network cmd");
  return(REF_CNE_RET_OK);
}
/*----------------------------------------------------------------------------
 * FUNCTION      SetPreferredNetwork

 * DESCRIPTION   Sets the desired network as the preferred network

 * DEPENDENCIES  None

 * RETURN VALUE  None

 * SIDE EFFECTS  The default network for the system is changed
 *--------------------------------------------------------------------------*/
void CRefCne::SetPreferredNetwork
(
  cne_rat_type* pNetId
)
{
  (*pNetId) ?
    RCNE_MSG_DEBUG("SPN: setting preferred network to [wlan]") :
    RCNE_MSG_DEBUG("SPN: setting preferred network to [wwan]");

  m_siPrefNetwork = *pNetId;
  return;
}
/*----------------------------------------------------------------------------
 * FUNCTION      GetPreferredNetwork

 * DESCRIPTION   Informs the caller about which network is used as default

 * DEPENDENCIES  None

 * RETURN VALUE  cne_rat_type

 * SIDE EFFECTS  None
 *--------------------------------------------------------------------------*/
cne_rat_type CRefCne::GetPreferredNetwork
(
)
{
  return(m_siPrefNetwork);
}

