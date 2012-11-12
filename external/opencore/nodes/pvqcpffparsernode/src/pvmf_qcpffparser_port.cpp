/* ------------------------------------------------------------------
 * Copyright (C) 1998-2009 PacketVideo
 * Copyright (c) 2009, Code Aurora Forum. All rights reserved.
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
#include "pvmf_qcpffparser_port.h"
#include "pvmf_qcpffparser_node.h"
#include "pvmf_qcpffparser_defs.h"
#include "pv_mime_string_utils.h"

PVMFQCPFFParserOutPort::PVMFQCPFFParserOutPort(int32 aTag, PVMFNodeInterface* aNode)
        : PvmfPortBaseImpl(aTag, aNode, "QcpFFParOut(Audio)")
{
    iQCPParserNode = OSCL_STATIC_CAST(PVMFQCPFFParserNode*, aNode);
    Construct();
}
void PVMFQCPFFParserOutPort::Construct()
{
    iLogger = PVLogger::GetLoggerObject("PVMFQCPParserOutPort");
    oscl_memset(&iStats, 0, sizeof(PvmfPortBaseImplStats));
    iNumFramesGenerated = 0;
    iNumFramesConsumed = 0;
}


PVMFQCPFFParserOutPort::~PVMFQCPFFParserOutPort()
{
    Disconnect();
    ClearMsgQueues();
}


bool PVMFQCPFFParserOutPort::IsFormatSupported(PVMFFormatType aFmt)
{
    bool formatSupported = false;
    if (aFmt == PVMF_MIME_QCELP || aFmt == PVMF_MIME_EVRC)
    {
        formatSupported = true;
    }
    return formatSupported;
}

void PVMFQCPFFParserOutPort::FormatUpdated()
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_MLDBG, iLogger, PVLOGMSG_INFO
                    , (0, "PVMFQCPFFParserOutPort::FormatUpdated "));
}

PVMFStatus PVMFQCPFFParserOutPort::Connect(PVMFPortInterface* aPort)
{
    PVMF_QCPPARSERNODE_LOGINFO((0, "PVMFQCPParserOutPort::Connect: aPort=0x%x", aPort));

    if (!aPort)
    {
        PVMF_QCPPARSERNODE_LOGERROR((0, "PVMFQCPParserOutPort::Connect: Error - Connecting to invalid port"));
        return PVMFErrArgument;
    }

    if (iConnectedPort)
    {
        PVMF_QCPPARSERNODE_LOGERROR((0, "PVMFQCPParserOutPort::Connect: Error - Already connected"));
        return PVMFFailure;
    }

    OsclAny* temp = NULL;
    aPort->QueryInterface(PVMI_CAPABILITY_AND_CONFIG_PVUUID, temp);
    PvmiCapabilityAndConfig *config = OSCL_STATIC_CAST(PvmiCapabilityAndConfig*, temp);

    if (config != NULL)
    {
        if (!(pvmiSetPortFormatSpecificInfoSync(config, PVMF_FORMAT_SPECIFIC_INFO_KEY)))
        {
            PVMF_QCPPARSERNODE_LOGERROR((0, "PVMFQCPParserOutPort::Connect: Error - Unable To Send Format Specific Info To Peer"));
            return PVMFFailure;
        }

        if (!(pvmiSetPortFormatSpecificInfoSync(config, PVMF_DATAPATH_PORT_MAX_NUM_MEDIA_MSGS_KEY)))
        {
            PVMF_QCPPARSERNODE_LOGERROR((0, "PVMFQCPParserOutPort::Connect: Error - Unable To Send Max Num Media Msg Key To Peer"));
            return PVMFFailure;
        }
    }

    /*
     * Automatically connect the peer.
     */
    if (aPort->PeerConnect(this) != PVMFSuccess)
    {
        PVMF_QCPPARSERNODE_LOGERROR((0, "PVMFQCPParserOutPort::Connect: Error - Peer Connect failed"));
        return PVMFFailure;
    }

    iConnectedPort = aPort;

    PortActivity(PVMF_PORT_ACTIVITY_CONNECT);
    return PVMFSuccess;
}

PVMFStatus PVMFQCPFFParserOutPort::getParametersSync(PvmiMIOSession aSession,
        PvmiKeyType aIdentifier,
        PvmiKvp*& aParameters,
        int& num_parameter_elements,
        PvmiCapabilityContext aContext)
{
    OSCL_UNUSED_ARG(aSession);
    OSCL_UNUSED_ARG(aIdentifier);
    OSCL_UNUSED_ARG(aContext);
    PVMF_QCPPARSERNODE_LOGINFO((0, "PVMFQCPParserOutPort::getParametersSync: aSession=0x%x, aIdentifier=%s, aParameters=0x%x, num_parameters_elements=%d, aContext=0x%x",
                                aSession, aIdentifier, aParameters, num_parameter_elements, aContext));

    num_parameter_elements = 0;
    if (pv_mime_strcmp(aIdentifier, PVMF_FORMAT_SPECIFIC_INFO_KEY) == 0)
    {
        if (!pvmiGetPortFormatSpecificInfoSync(PVMF_FORMAT_SPECIFIC_INFO_KEY, aParameters))
        {
            return PVMFFailure;
        }
    }

    else if (pv_mime_strcmp(aIdentifier, PVMF_DATAPATH_PORT_MAX_NUM_MEDIA_MSGS_KEY) == 0)
    {
        if (!pvmiGetPortFormatSpecificInfoSync(PVMF_DATAPATH_PORT_MAX_NUM_MEDIA_MSGS_KEY, aParameters))
        {
            return PVMFFailure;
        }
    }
    num_parameter_elements = 1;
    return PVMFSuccess;
}


PVMFStatus PVMFQCPFFParserOutPort::releaseParameters(PvmiMIOSession aSession,
        PvmiKvp* aParameters,
        int num_elements)
{
    OSCL_UNUSED_ARG(aSession);
    OSCL_UNUSED_ARG(num_elements);

    PVMF_QCPPARSERNODE_LOGINFO((0, "PVMFQCPParserOutPort::releaseParameters: aSession=0x%x, aParameters=0x%x, num_elements=%d",
                                aSession, aParameters, num_elements));

    if (pv_mime_strcmp(aParameters->key, PVMF_FORMAT_SPECIFIC_INFO_KEY) == 0)
    {
        OsclMemAllocator alloc;
        alloc.deallocate((OsclAny*)(aParameters->key));
        return PVMFSuccess;
    }
    return PVMFErrNotSupported;
}

void PVMFQCPFFParserOutPort::setParametersSync(PvmiMIOSession aSession,
        PvmiKvp* aParameters,
        int num_elements,
        PvmiKvp * & aRet_kvp)
{
    OSCL_UNUSED_ARG(aSession);
    OSCL_UNUSED_ARG(aParameters);
    OSCL_UNUSED_ARG(aRet_kvp);
    OSCL_UNUSED_ARG(num_elements);

    PVMF_QCPPARSERNODE_LOGINFO((0, "PVMFQCPParserOutPort::getParametersSync: aSession=0x%x, aParameters=0x%x, num_elements=%d, aRet_kvp=0x%x",
                                aSession, aParameters, num_elements, aRet_kvp));

}

PVMFStatus PVMFQCPFFParserOutPort::verifyParametersSync(PvmiMIOSession aSession,
        PvmiKvp* aParameters,
        int num_elements)
{
    OSCL_UNUSED_ARG(aSession);
    OSCL_UNUSED_ARG(aParameters);
    OSCL_UNUSED_ARG(num_elements);

    PVMF_QCPPARSERNODE_LOGINFO((0, "PVMFQCPParserOutPort::verifyParametersSync: aSession=0x%x, aParameters=0x%x, num_elements=%d",
                                aSession, aParameters, num_elements));

    return PVMFErrNotSupported;
}


bool
PVMFQCPFFParserOutPort::pvmiSetPortFormatSpecificInfoSync(PvmiCapabilityAndConfig *aPort,
        const char* aFormatValType)
{
    /*
     * Create PvmiKvp for capability settings
     */
    PVQCPFFNodeTrackPortInfo* trackInfoPtr = NULL;
    if (!(iQCPParserNode->GetTrackInfo((OSCL_STATIC_CAST(PVMFPortInterface*, this)), trackInfoPtr)))
    {
        return false;
    }
    if (pv_mime_strcmp(aFormatValType, PVMF_FORMAT_SPECIFIC_INFO_KEY) == 0)
    {
        if (trackInfoPtr->iFormatSpecificConfig.getMemFragSize() > 0)
        {
            OsclMemAllocator alloc;
            PvmiKvp kvp;
            kvp.key = NULL;
            kvp.length = oscl_strlen(aFormatValType) + 1; // +1 for \0
            kvp.key = (PvmiKeyType)alloc.ALLOCATE(kvp.length);
            if (kvp.key == NULL)
            {
                return false;
            }
            oscl_strncpy(kvp.key, aFormatValType, kvp.length);

            kvp.value.key_specific_value = (OsclAny*)(trackInfoPtr->iFormatSpecificConfig.getMemFragPtr());
            kvp.capacity = trackInfoPtr->iFormatSpecificConfig.getMemFragSize();
            PvmiKvp* retKvp = NULL; // for return value
            int32 err;
            OSCL_TRY(err, aPort->setParametersSync(NULL, &kvp, 1, retKvp););
            /* ignore the error for now */
            alloc.deallocate((OsclAny*)(kvp.key));
        }
        return true;
    }
    else if (pv_mime_strcmp(aFormatValType, PVMF_DATAPATH_PORT_MAX_NUM_MEDIA_MSGS_KEY) == 0)
    {
        OsclMemAllocator alloc;
        PvmiKvp kvp;
        kvp.key = NULL;
        kvp.length = oscl_strlen(aFormatValType) + 1; // +1 for \0
        kvp.key = (PvmiKeyType)alloc.ALLOCATE(kvp.length);
        if (kvp.key == NULL)
        {
            return false;
        }
        oscl_strncpy(kvp.key, aFormatValType, kvp.length);

        kvp.value.uint32_value = PVMF_QCP_PARSER_NODE_MAX_NUM_OUTSTANDING_MEDIA_MSGS;
        PvmiKvp* retKvp = NULL; // for return value
        int32 err;
        OSCL_TRY(err, aPort->setParametersSync(NULL, &kvp, 1, retKvp););
        /* ignore the error for now */
        alloc.deallocate((OsclAny*)(kvp.key));
        return true;
    }
    return false;
}

bool
PVMFQCPFFParserOutPort::pvmiGetPortFormatSpecificInfoSync(const char* aFormatValType,
        PvmiKvp*& aKvp)
{
    /*
     * Create PvmiKvp for capability settings
     */
    PVQCPFFNodeTrackPortInfo* trackInfoPtr = NULL;
    if (!(iQCPParserNode->GetTrackInfo((OSCL_STATIC_CAST(PVMFPortInterface*, this)), trackInfoPtr)))
    {
        return false;
    }
    if (pv_mime_strcmp(aFormatValType, PVMF_FORMAT_SPECIFIC_INFO_KEY) == 0)
    {
        if (trackInfoPtr->iFormatSpecificConfig.getMemFragSize() > 0)
        {
            OsclMemAllocator alloc;
            aKvp->key = NULL;
            aKvp->length = oscl_strlen(aFormatValType) + 1; // +1 for \0
            aKvp->key = (PvmiKeyType)alloc.ALLOCATE(aKvp->length);
            if (aKvp->key == NULL)
            {
                return false;
            }
            oscl_strncpy(aKvp->key, aFormatValType, aKvp->length);

            aKvp->value.key_specific_value = (OsclAny*)(trackInfoPtr->iFormatSpecificConfig.getMemFragPtr());
            aKvp->capacity = trackInfoPtr->iFormatSpecificConfig.getMemFragSize();
        }
        return true;
    }
    else if (pv_mime_strcmp(aFormatValType, PVMF_DATAPATH_PORT_MAX_NUM_MEDIA_MSGS_KEY) == 0)
    {
        OsclMemAllocator alloc;
        aKvp->key = NULL;
        aKvp->length = oscl_strlen(aFormatValType) + 1; // +1 for \0
        aKvp->key = (PvmiKeyType)alloc.ALLOCATE(aKvp->length);
        if (aKvp->key == NULL)
        {
            return false;
        }
        oscl_strncpy(aKvp->key, aFormatValType, aKvp->length);
        aKvp->value.uint32_value = PVMF_QCP_PARSER_NODE_MAX_NUM_OUTSTANDING_MEDIA_MSGS;
        return true;
    }
    return false;
}
