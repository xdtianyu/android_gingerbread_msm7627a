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
#include "pvmf_qcpffparser_node.h"
#include "pvmf_qcpffparser_defs.h"
#include "qcp_parsernode_tunables.h"
#include "pvmf_qcpffparser_port.h"
#include "qcpfileparser.h"
#include "media_clock_converter.h"
#include "pv_gau.h"
#include "pvlogger.h"
#include "oscl_error_codes.h"
#include "pvmf_fileformat_events.h"
#include "pvmf_basic_errorinfomessage.h"
#include "pvmf_errorinfomessage_extension.h"
#include "pvmf_media_cmd.h"
#include "pvmf_media_msg_format_ids.h"
#include "pv_mime_string_utils.h"
#include "oscl_snprintf.h"
#include "pvmf_local_data_source.h"
#include "pvmi_kvp_util.h"
#include "pvmf_qcpffparser_events.h"
#include "oscl_exclusive_ptr.h"
#include "pvmf_source_context_data.h"

static const char PVQCP_ALL_METADATA_KEY[] = "all";
static const char PVQCPMETADATA_DURATION_KEY[] = "duration";
static const char PVQCPMETADATA_NUMTRACKS_KEY[] = "num-tracks";
static const char PVQCPMETADATA_TRACKINFO_BITRATE_KEY[] = "track-info/bit-rate";
static const char PVQCPMETADATA_TRACKINFO_AUDIO_FORMAT_KEY[] = "track-info/audio/format";
static const char PVQCPMETADATA_CLIP_TYPE_KEY[] = "clip-type";
static const char PVQCPMETADATA_RANDOM_ACCESS_DENIED_KEY[] = "random-access-denied";
static const char PVQCPMETADATA_SEMICOLON[] = ";";
static const char PVQCPMETADATA_TIMESCALE[] = "timescale=";
static const char PVQCPMETADATA_INDEX0[] = "index=0";

#define QCP_SAMPLE_DURATION 20



PVMFQCPFFParserNode::PVMFQCPFFParserNode(int32 aPriority) :
        OsclTimerObject(aPriority, "PVQCPFFParserNode"),
        iOutPort(NULL),
        iLogger(NULL),
        iQCPParser(NULL),
        iExtensionRefCount(0)
{
    iFileHandle                = NULL;
    iLogger                    = NULL;
    iDataPathLogger            = NULL;
    iClockLogger               = NULL;
    iDownloadComplete		   = false;

    iFileSizeLastConvertedToTime = 0;
    iLastNPTCalcInConvertSizeToTime = 0;

    iExtensionRefCount         = 0;
    iUseCPMPluginRegistry      = false;

    iCPM                       = NULL;
    iCPMSessionID              = 0xFFFFFFFF;
    iCPMContentType            = PVMF_CPM_CONTENT_FORMAT_UNKNOWN;
    iCPMContentAccessFactory   = NULL;
    iCPMInitCmdId              = 0;
    iCPMOpenSessionCmdId       = 0;
    iCPMRegisterContentCmdId   = 0;
    iCPMGetLicenseInterfaceCmdId = 0;
    iCPMRequestUsageId         = 0;
    iCPMUsageCompleteCmdId     = 0;
    iCPMCloseSessionCmdId      = 0;
    iCPMResetCmdId             = 0;
    iCPMCancelGetLicenseCmdId  = 0;
    iRequestedUsage.key        = NULL;
    iApprovedUsage.key         = NULL;
    iAuthorizationDataKvp.key  = NULL;
    iCPMMetaDataExtensionInterface = NULL;
    iCPMGetMetaDataKeysCmdId       = 0;
    iCPMGetMetaDataValuesCmdId     = 0;
    iQCPParserNodeMetadataValueCount = 0;

    iDownloadProgressInterface = NULL;
    iDownloadFileSize          = 0;
    iQCPHeaderSize             = QCP_HEADER_SIZE;
    iDataStreamInterface       = NULL;
    iDataStreamFactory         = NULL;
    iDataStreamReadCapacityObserver = NULL;
    iAutoPaused                = false;

    iStreamID                  = 0;

    oSourceIsCurrent           = false;
    iInterfaceState = EPVMFNodeCreated;

    iUseCPMPluginRegistry = false;
    iFileHandle = NULL;

    iCountToClaculateRDATimeInterval = 1;
    int32 err;
    OSCL_TRY(err,

             //Create the input command queue.  Use a reserve to avoid lots of dynamic memory allocation.
             iInputCommands.Construct(PVMF_QCPFFPARSER_NODE_COMMAND_ID_START, PVMF_QCPFFPARSER_NODE_COMMAND_VECTOR_RESERVE);

             //Create the "current command" queue.  It will only contain one
             //command at a time, so use a reserve of 1.
             iCurrentCommand.Construct(0, 1);
             iCancelCommand.Construct(0, 1);

             iSelectedTrackList.reserve(1);

             //Set the node capability data.
             //This node can support an unlimited number of ports.
             iCapability.iCanSupportMultipleInputPorts = false;
             iCapability.iCanSupportMultipleOutputPorts = false;
             iCapability.iHasMaxNumberOfPorts = true;
             iCapability.iMaxNumberOfPorts = 1;
             iCapability.iOutputFormatCapability.push_back(PVMF_MIME_QCPFF);
             iCapability.iOutputFormatCapability.push_back(PVMF_MIME_QCELP);
             iCapability.iOutputFormatCapability.push_back(PVMF_MIME_EVRC);
            );

    if (err != OsclErrNone)
    {
        //if a leave happened, cleanup and re-throw the error
        iInputCommands.clear();
        iCurrentCommand.clear();
        iCancelCommand.clear();
        iCapability.iInputFormatCapability.clear();
        iCapability.iOutputFormatCapability.clear();
        OSCL_CLEANUP_BASE_CLASS(PVMFNodeInterface);
        OSCL_CLEANUP_BASE_CLASS(OsclTimerObject);
        OSCL_LEAVE(err);
    }

    Construct();
}

PVMFQCPFFParserNode::~PVMFQCPFFParserNode()
{
    if (iRequestedUsage.key)
    {
        OSCL_ARRAY_DELETE(iRequestedUsage.key);
        iRequestedUsage.key = NULL;
    }
    if (iApprovedUsage.key)
    {
        OSCL_ARRAY_DELETE(iApprovedUsage.key);
        iApprovedUsage.key = NULL;
    }
    if (iAuthorizationDataKvp.key)
    {
        OSCL_ARRAY_DELETE(iAuthorizationDataKvp.key);
        iAuthorizationDataKvp.key = NULL;
    }

    if (iCPM != NULL)
    {
        iCPM->ThreadLogoff();
        PVMFCPMFactory::DestroyContentPolicyManager(iCPM);
        iCPM = NULL;
    }
    if (iDownloadProgressInterface != NULL)
    {
        iDownloadProgressInterface->cancelResumeNotification();
    }
    //Cleanup commands
    //The command queues are self-deleting, but we want to
    //notify the observer of unprocessed commands.
    while (!iCurrentCommand.empty())
    {
        CommandComplete(iCurrentCommand, iCurrentCommand.front(), PVMFFailure);
    }
    while (!iCancelCommand.empty())
    {
        CommandComplete(iCancelCommand, iCancelCommand.front(), PVMFFailure);
    }
    while (!iInputCommands.empty())
    {
        CommandComplete(iInputCommands, iInputCommands.front(), PVMFFailure);
    }
    if (iExtensionRefCount > 0)
    {
        OSCL_ASSERT(false);
    }
    Cancel();

    //Cleanup allocated ports
    ReleaseAllPorts();
    CleanupFileSource();
    iFileServer.Close();
}

PVMFStatus PVMFQCPFFParserNode::ThreadLogon()
{
    PVMF_QCPPARSERNODE_LOGSTACKTRACE((0, "PVMFQCPParserNode::ThreadLogon() Called"));
    if (iInterfaceState == EPVMFNodeCreated)
    {
        if (!IsAdded())
        {
            AddToScheduler();
        }
        iLogger = PVLogger::GetLoggerObject("PVMFQCPParserNode");
        iDataPathLogger = PVLogger::GetLoggerObject("datapath.sourcenode.qcpparsernode");
        iClockLogger = PVLogger::GetLoggerObject("clock");
        iFileServer.Connect();
        SetState(EPVMFNodeIdle);
        return PVMFSuccess;
    }
    PVMF_QCPPARSERNODE_LOGERROR((0, "PVMFQCPParserNode::ThreadLogon() - Invalid State"));
    return PVMFErrInvalidState;
}

PVMFStatus PVMFQCPFFParserNode::GetCapability(PVMFNodeCapability& aNodeCapability)
{

    PVMF_QCPPARSERNODE_LOGSTACKTRACE((0, "PVMFQCPParserNode::GetCapability() called"));
    // TODO: Return the appropriate format capability
    aNodeCapability = iCapability;
    return PVMFSuccess;
}


PVMFPortIter* PVMFQCPFFParserNode::GetPorts(const PVMFPortFilter* aFilter)
{
    PVMF_QCPPARSERNODE_LOGSTACKTRACE((0, "PVMFASFParserNode::GetPorts() called"));
    OSCL_UNUSED_ARG(aFilter);
    PVMF_QCPPARSERNODE_LOGERROR((0, "PVMFASFParserNode::GetPorts() Not Implemented"));
    // TODO: Return the currently available ports
    return NULL;
}

PVMFCommandId PVMFQCPFFParserNode::QueryUUID(PVMFSessionId s, const PvmfMimeString& aMimeType,
        Oscl_Vector<PVUuid, OsclMemAllocator>& aUuids,
        bool aExactUuidsOnly, const OsclAny* aContext)
{
    PVMF_QCPPARSERNODE_LOGSTACKTRACE((0, "PVMFQCPParserNode::QueryUUID called"));
    PVMFQCPFFNodeCommand cmd;
    cmd.PVMFQCPFFNodeCommandBase::Construct(s,
                                            PVMF_QCP_PARSER_NODE_QUERYUUID,
                                            aMimeType,
                                            aUuids,
                                            aExactUuidsOnly,
                                            aContext);
    return QueueCommandL(cmd);
}

PVMFCommandId PVMFQCPFFParserNode::QueryInterface(PVMFSessionId s, const PVUuid& aUuid,
        PVInterface*& aInterfacePtr,
        const OsclAny* aContext)
{
    PVMF_QCPPARSERNODE_LOGSTACKTRACE((0, "PVMFQCPParserNode::QueryInterface called"));
    PVMFQCPFFNodeCommand cmd;
    cmd.PVMFQCPFFNodeCommandBase::Construct(s,
                                            PVMF_QCP_PARSER_NODE_QUERYINTERFACE,
                                            aUuid,
                                            aInterfacePtr,
                                            aContext);
    return QueueCommandL(cmd);
}

PVMFCommandId PVMFQCPFFParserNode::RequestPort(PVMFSessionId s, int32 aPortTag, const PvmfMimeString* aPortConfig, const OsclAny* aContext)
{
    PVMF_QCPPARSERNODE_LOGSTACKTRACE((0, "PVMFQCPParserNode::RequestPort called"));
    PVMFQCPFFNodeCommand cmd;
    cmd.PVMFQCPFFNodeCommandBase::Construct(s,
                                            PVMF_QCP_PARSER_NODE_REQUESTPORT,
                                            aPortTag,
                                            aPortConfig,
                                            aContext);
    return QueueCommandL(cmd);
}

PVMFCommandId PVMFQCPFFParserNode::ReleasePort(PVMFSessionId s, PVMFPortInterface& aPort, const OsclAny* aContext)
{
    PVMF_QCPPARSERNODE_LOGSTACKTRACE((0, "PVMFQCPParserNode::ReleasePort called"));
    PVMFQCPFFNodeCommand cmd;
    cmd.PVMFQCPFFNodeCommandBase::Construct(s, PVMF_QCP_PARSER_NODE_RELEASEPORT, aPort, aContext);
    return QueueCommandL(cmd);
}

PVMFCommandId PVMFQCPFFParserNode::Init(PVMFSessionId s, const OsclAny* aContext)
{
    PVMF_QCPPARSERNODE_LOGSTACKTRACE((0, "PVMFQCPParserNode::Init called"));
    PVMFQCPFFNodeCommand cmd;
    cmd.PVMFQCPFFNodeCommandBase::Construct(s, PVMF_QCP_PARSER_NODE_INIT, aContext);
    return QueueCommandL(cmd);
}

PVMFCommandId PVMFQCPFFParserNode::Prepare(PVMFSessionId s, const OsclAny* aContext)
{
    PVMF_QCPPARSERNODE_LOGSTACKTRACE((0, "PVMFQCPParserNode::Prepare called"));
    PVMFQCPFFNodeCommand cmd;
    cmd.PVMFQCPFFNodeCommandBase::Construct(s, PVMF_QCP_PARSER_NODE_PREPARE, aContext);
    return QueueCommandL(cmd);
}

PVMFCommandId PVMFQCPFFParserNode::Start(PVMFSessionId s, const OsclAny* aContext)
{
    PVMF_QCPPARSERNODE_LOGSTACKTRACE((0, "PVMFQCPParserNode::Start called"));
    PVMFQCPFFNodeCommand cmd;
    cmd.PVMFQCPFFNodeCommandBase::Construct(s, PVMF_QCP_PARSER_NODE_START, aContext);
    return QueueCommandL(cmd);
}

PVMFCommandId PVMFQCPFFParserNode::Stop(PVMFSessionId s, const OsclAny* aContext)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_MLDBG, iLogger, PVLOGMSG_INFO, (0, "PVMFQCPFFParserNode:Stop"));
    PVMFQCPFFNodeCommand cmd;
    cmd.PVMFQCPFFNodeCommandBase::Construct(s, PVMF_QCP_PARSER_NODE_STOP, aContext);
    return QueueCommandL(cmd);
}

PVMFCommandId PVMFQCPFFParserNode::Pause(PVMFSessionId s, const OsclAny* aContext)
{
    PVMF_QCPPARSERNODE_LOGSTACKTRACE((0, "PVMFQCPParserNode::Stop called"));
    PVMFQCPFFNodeCommand cmd;
    cmd.PVMFQCPFFNodeCommandBase::Construct(s, PVMF_QCP_PARSER_NODE_PAUSE, aContext);
    return QueueCommandL(cmd);
}

PVMFCommandId PVMFQCPFFParserNode::Flush(PVMFSessionId s, const OsclAny* aContext)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_MLDBG, iLogger, PVLOGMSG_INFO, (0, "PVMFQCPFFParserNode:Flush"));
    PVMFQCPFFNodeCommand cmd;
    cmd.PVMFQCPFFNodeCommandBase::Construct(s, PVMF_QCP_PARSER_NODE_FLUSH, aContext);
    return QueueCommandL(cmd);
}

PVMFCommandId PVMFQCPFFParserNode::Reset(PVMFSessionId s, const OsclAny* aContext)
{
    PVMF_QCPPARSERNODE_LOGSTACKTRACE((0, "PVMFQCPParserNode::Flush called"));
    PVMFQCPFFNodeCommand cmd;
    cmd.PVMFQCPFFNodeCommandBase::Construct(s, PVMF_QCP_PARSER_NODE_RESET, aContext);
    return QueueCommandL(cmd);
}

PVMFCommandId PVMFQCPFFParserNode::CancelAllCommands(PVMFSessionId s, const OsclAny* aContext)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_MLDBG, iLogger, PVLOGMSG_INFO, (0, "PVMFQCPFFParserNode:CancelAllCommands"));
    PVMFQCPFFNodeCommand cmd;
    cmd.PVMFQCPFFNodeCommandBase::Construct(s, PVMF_QCP_PARSER_NODE_CANCELALLCOMMANDS, aContext);
    return QueueCommandL(cmd);
}

PVMFCommandId PVMFQCPFFParserNode::CancelCommand(PVMFSessionId s, PVMFCommandId aCmdId, const OsclAny* aContext)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_MLDBG, iLogger, PVLOGMSG_INFO, (0, "PVMFQCPFFParserNode:CancelCommand"));
    PVMFQCPFFNodeCommand cmd;
    cmd.PVMFQCPFFNodeCommandBase::Construct(s, PVMF_QCP_PARSER_NODE_CANCELCOMMAND, aCmdId, aContext);
    return QueueCommandL(cmd);
}

void PVMFQCPFFParserNode::Construct()
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFQCPFFParserNode::Construct()"));
    iFileServer.Connect();
    iAvailableMetadataKeys.reserve(4);
    iAvailableMetadataKeys.clear();
}

void PVMFQCPFFParserNode::Run()
{
    if (!iInputCommands.empty())
    {
        if (ProcessCommand())
        {
            /*
             * note: need to check the state before re-scheduling
             * since the node could have been reset in the ProcessCommand
             * call.
             */
            if (iInterfaceState != EPVMFNodeCreated)
            {
                RunIfNotReady();
            }
            return;
        }
    }
    // Send outgoing messages
    if (iInterfaceState == EPVMFNodeStarted || FlushPending())
    {
        PVQCPFFNodeTrackPortInfo* trackPortInfoPtr = NULL;

        if (!GetTrackInfo(iOutPort, trackPortInfoPtr))
        {
            PVMF_QCPPARSERNODE_LOGERROR((0, "PVQCPParserNode::Run: Error - GetTrackInfo failed"));
            return;
        }

        ProcessPortActivity(trackPortInfoPtr);

        if (CheckForPortRescheduling())
        {
            /*
             * Re-schedule since there is atleast one port that needs processing
             */
            RunIfNotReady();
        }
    }

    if (FlushPending()
            && iOutPort
            && iOutPort->OutgoingMsgQueueSize() == 0)
    {
        SetState(EPVMFNodePrepared);
        iOutPort->ResumeInput();
        CommandComplete(iCurrentCommand, iCurrentCommand.front(), PVMFSuccess);
    }
}

PVMFStatus  PVMFQCPFFParserNode::ProcessOutgoingMsg(PVQCPFFNodeTrackPortInfo* aTrackInfoPtr)
{
    /*
     * Called by the AO to process one message off the outgoing
     * message queue for the given port.  This routine will
     * try to send the data to the connected port.
    */
    PVMF_QCPPARSERNODE_LOGSTACKTRACE((0, "PVMFQCPParserNode::ProcessOutgoingMsg() Called aPort=0x%x", aTrackInfoPtr->iPort));
    PVMFStatus status = aTrackInfoPtr->iPort->Send();
    if (status == PVMFErrBusy)
    {
        /* Connected port is busy */
        aTrackInfoPtr->oProcessOutgoingMessages = false;
        PVMF_QCPPARSERNODE_LOGDATATRAFFIC((0, "PVMFQCPParserNode::ProcessOutgoingMsg() Connected port is in busy state"));
    }
    else if (status != PVMFSuccess)
    {
        PVMF_QCPPARSERNODE_LOGERROR((0, "PVMFQCPParserNode::ProcessOutgoingMsg() - aTrackInfoPtr->iPort->Send() Failed"));
    }
    return status;
}

PVMFStatus PVMFQCPFFParserNode::DoGetMetadataKeys(PVMFQCPFFNodeCommand& aCmd)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFQCPFFParserNode::DoGetNodeMetadataKeys() In"));

    /* Get Metadata keys from CPM for protected content only */
    if ((iCPMMetaDataExtensionInterface != NULL))

    {
        GetCPMMetaDataKeys();
        return PVMFPending;
    }
    return (CompleteGetMetadataKeys(aCmd));
}

PVMFStatus
PVMFQCPFFParserNode::CompleteGetMetadataKeys(PVMFQCPFFNodeCommand& aCmd)
{
    PVMF_QCPPARSERNODE_LOGSTACKTRACE((0, "PVMFQCPParserNode::CompleteGetMetadataKeys Called"));
    PVMFMetadataList* keylistptr = NULL;
    uint32 starting_index;
    int32 max_entries;
    char* query_key;

    aCmd.PVMFQCPFFNodeCommand::Parse(keylistptr, starting_index, max_entries, query_key);
    if (keylistptr == NULL)
    {
        return PVMFErrArgument;
    }

    if ((starting_index > (iAvailableMetadataKeys.size() - 1)) || max_entries == 0)
    {
        return PVMFErrArgument;
    }

    uint32 num_entries = 0;
    int32 num_added = 0;
    uint32 lcv = 0;
    for (lcv = 0; lcv < iAvailableMetadataKeys.size(); lcv++)
    {
        if (query_key == NULL)
        {
            ++num_entries;
            if (num_entries > starting_index)
            {
                // Past the starting index so copy the key
                PVMFStatus status = PushValueToList(iAvailableMetadataKeys, keylistptr, lcv);
                if (PVMFErrNoMemory == status)
                {
                    return status;
                }
                num_added++;
            }
        }
        else
        {
            // Check if the key matches the query key
            if (pv_mime_strcmp(iAvailableMetadataKeys[lcv].get_cstr(), query_key) >= 0)
            {
                // This key is counted
                ++num_entries;
                if (num_entries > starting_index)
                {
                    // Past the starting index so copy the key
                    PVMFStatus status = PushValueToList(iAvailableMetadataKeys, keylistptr, lcv);
                    if (PVMFErrNoMemory == status)
                    {
                        return status;
                    }
                    num_added++;
                }
            }
        }

        // Check if max number of entries have been copied
        if (max_entries > 0 && num_added >= max_entries)
        {
            break;
        }
    }
    for (lcv = 0; lcv < iCPMMetadataKeys.size(); lcv++)
    {
        if (query_key == NULL)
        {
            /* No query key so this key is counted */
            ++num_entries;
            if (num_entries > (uint32)starting_index)
            {
                /* Past the starting index so copy the key */

                PVMFStatus status = PushValueToList(iCPMMetadataKeys, keylistptr, lcv);
                if (PVMFErrNoMemory == status)
                {
                    return status;
                }
                num_added++;
            }
        }
        else
        {
            /* Check if the key matches the query key */
            if (pv_mime_strcmp(iCPMMetadataKeys[lcv].get_cstr(), query_key) >= 0)
            {
                ++num_entries;
                if (num_entries > (uint32)starting_index)
                {
                    /* Past the starting index so copy the key */

                    PVMFStatus status = PushValueToList(iCPMMetadataKeys, keylistptr, lcv);
                    if (PVMFErrNoMemory == status)
                    {
                        return status;
                    }
                    num_added++;
                }
            }
        }
        /* Check if max number of entries have been copied */
        if ((max_entries > 0) && (num_added >= max_entries))
        {
            break;
        }
    }




    return PVMFSuccess;
}

PVMFStatus PVMFQCPFFParserNode::DoGetMetadataValues(PVMFQCPFFNodeCommand& aCmd)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFQCPFFParserNode::DoGetMetadataValues() In"));

    // File must be parsed
    if (!iQCPParser)
    {
        return PVMFErrInvalidState;
    }

    PVMFMetadataList* keylistptr_in = NULL;
    PVMFMetadataList* keylistptr = NULL;
    Oscl_Vector<PvmiKvp, OsclMemAllocator>* valuelistptr = NULL;
    uint32 starting_index;
    int32 max_entries;

    aCmd.PVMFQCPFFNodeCommand::Parse(keylistptr_in, valuelistptr, starting_index, max_entries);

    if (keylistptr_in == NULL || valuelistptr == NULL)
    {
        return PVMFErrArgument;
    }

    keylistptr = keylistptr_in;
    //If numkeys is one, just check to see if the request
    //is for ALL metadata
    if (keylistptr_in->size() == 1)
    {
        if (oscl_strncmp((*keylistptr)[0].get_cstr(),
                         PVQCP_ALL_METADATA_KEY,
                         oscl_strlen(PVQCP_ALL_METADATA_KEY)) == 0)
        {
            //use the complete metadata key list
            keylistptr = &iAvailableMetadataKeys;
        }
    }

    uint32 numkeys = keylistptr->size();

    if (starting_index > (numkeys - 1) || numkeys == 0 || max_entries == 0)
    {
        // Don't do anything
        return PVMFErrArgument;
    }

    uint32 numvalentries = 0;
    int32 numentriesadded = 0;
    for (uint32 lcv = 0; lcv < numkeys; lcv++)
    {
        int32 leavecode = 0;
        PvmiKvp KeyVal;
        KeyVal.key = NULL;

        if (oscl_strcmp((*keylistptr)[lcv].get_cstr(), PVQCPMETADATA_DURATION_KEY) == 0 &&
                iQCPFileInfo.iDuration > 0)
        {
            // Movie Duration
            // Increment the counter for the number of values found so far
            ++numvalentries;

            // Create a value entry if past the starting index
            if (numvalentries > starting_index)
            {
                char timescalestr[20];
                oscl_snprintf(timescalestr, 20, ";%s%d", PVQCPMETADATA_TIMESCALE, iQCPFileInfo.iTimescale);
                timescalestr[19] = '\0';
                uint32 duration = Oscl_Int64_Utils::get_uint64_lower32(iQCPFileInfo.iDuration);
                int32 retval =
                    PVMFCreateKVPUtils::CreateKVPForUInt32Value(KeyVal,
                            PVQCPMETADATA_DURATION_KEY,
                            duration,
                            timescalestr);
                if (retval != PVMFSuccess && retval != PVMFErrArgument)
                {
                    break;
                }
            }
        }
        else if (oscl_strcmp((*keylistptr)[lcv].get_cstr(), PVQCPMETADATA_NUMTRACKS_KEY) == 0)
        {
            // Number of tracks
            // Increment the counter for the number of values found so far
            ++numvalentries;
            // Create a value entry if past the starting index
            if (numvalentries > starting_index)
            {
                uint32 numtracks = 1;
                PVMFStatus retval = PVMFCreateKVPUtils::CreateKVPForUInt32Value(KeyVal, PVQCPMETADATA_NUMTRACKS_KEY, numtracks);
                if (retval != PVMFSuccess && retval != PVMFErrArgument)
                {
                    break;
                }
            }
        }
        else if ((oscl_strcmp((*keylistptr)[lcv].get_cstr(), PVQCPMETADATA_TRACKINFO_BITRATE_KEY) == 0) &&
                 iQCPFileInfo.iBitrate > 0)
        {
            // Bitrate
            // Increment the counter for the number of values found so far
            ++numvalentries;
            int32 retval = 0;
            // Create a value entry if past the starting index
            if (numvalentries > starting_index)
            {
                char indexparam[16];
                oscl_snprintf(indexparam, 16, ";%s", PVQCPMETADATA_INDEX0);
                indexparam[15] = '\0';
                uint32 bitrate = iQCPFileInfo.iBitrate;
                retval = PVMFCreateKVPUtils::CreateKVPForUInt32Value(KeyVal, PVQCPMETADATA_TRACKINFO_BITRATE_KEY, bitrate, indexparam);
            }
            if (retval != PVMFSuccess && retval != PVMFErrArgument)
            {
                break;
            }

        }
        else if (oscl_strcmp((*keylistptr)[lcv].get_cstr(), PVQCPMETADATA_RANDOM_ACCESS_DENIED_KEY) == 0)
        {
            /*
             * Random Access
             * Increment the counter for the number of values found so far
             */
            ++numvalentries;

            /* Create a value entry if past the starting index */
            if (numvalentries > (uint32)starting_index)
            {
                bool random_access_denied = false;

                PVMFStatus retval =
                    PVMFCreateKVPUtils::CreateKVPForBoolValue(KeyVal,
                            PVQCPMETADATA_RANDOM_ACCESS_DENIED_KEY,
                            random_access_denied,
                            NULL);
                if (retval != PVMFSuccess && retval != PVMFErrArgument)
                {
                    break;
                }
            }
        }
        else if (oscl_strncmp((*keylistptr)[lcv].get_cstr(), PVQCPMETADATA_CLIP_TYPE_KEY, oscl_strlen(PVQCPMETADATA_CLIP_TYPE_KEY)) == 0)
        {
            /*
             * Clip Type
             * Increment the counter for the number of values found so far
             */
            ++numvalentries;

            /* Create a value entry if past the starting index */
            if (numvalentries > (uint32)starting_index)
            {
                uint32 len = 0;
                char* clipType = NULL;
                {
                    len = oscl_strlen("local");
                    clipType = OSCL_ARRAY_NEW(char, len + 1);
                    oscl_memset(clipType, 0, len + 1);
                    oscl_strncpy(clipType, ("local"), len);
                }

                PVMFStatus retval =
                    PVMFCreateKVPUtils::CreateKVPForCharStringValue(KeyVal,
                            PVQCPMETADATA_CLIP_TYPE_KEY,
                            clipType);

                OSCL_ARRAY_DELETE(clipType);
                if (retval != PVMFSuccess && retval != PVMFErrArgument)
                {
                    break;
                }

            }
        }
        else if ((oscl_strcmp((*keylistptr)[lcv].get_cstr(), PVQCPMETADATA_TRACKINFO_AUDIO_FORMAT_KEY) == 0) &&
                 iQCPFileInfo.iQcpFormat != QCP_FORMAT_UNKNOWN)
        {
            // Format
            // Increment the counter for the number of values found so far
            ++numvalentries;
            int32 retval = 0;

            // Create a value entry if past the starting index
            if (numvalentries > starting_index)
            {
                char indexparam[16];
                oscl_snprintf(indexparam, 16, ";%s", PVQCPMETADATA_INDEX0);
                indexparam[15] = '\0';

                switch (iQCPFileInfo.iQcpFormat)
                {
                    case QCP_FORMAT_QCELP:
                        retval = PVMFCreateKVPUtils::CreateKVPForCharStringValue(KeyVal, PVQCPMETADATA_TRACKINFO_AUDIO_FORMAT_KEY, _STRLIT_CHAR(PVMF_MIME_QCELP), indexparam);
                        break;

                    case QCP_FORMAT_EVRC:
                        retval = PVMFCreateKVPUtils::CreateKVPForCharStringValue(KeyVal, PVQCPMETADATA_TRACKINFO_AUDIO_FORMAT_KEY, _STRLIT_CHAR(PVMF_MIME_EVRC), indexparam);
                        break;

                    case QCP_FORMAT_UNKNOWN:
                    default:
                        // Should not enter here
                        OSCL_ASSERT(false);
                        break;
                }
                if (retval != PVMFSuccess && retval != PVMFErrArgument)
                {
                    break;
                }
            }
        }

        if (KeyVal.key != NULL)
        {
            // Add the entry to the list
            leavecode = PushBackKeyVal(valuelistptr, KeyVal);
            if (leavecode != 0)
            {
                switch (GetValTypeFromKeyString(KeyVal.key))
                {
                    case PVMI_KVPVALTYPE_CHARPTR:
                        if (KeyVal.value.pChar_value != NULL)
                        {
                            OSCL_ARRAY_DELETE(KeyVal.value.pChar_value);
                            KeyVal.value.pChar_value = NULL;
                        }
                        break;

                    default:
                        // Add more case statements if other value types are returned
                        break;
                }

                OSCL_ARRAY_DELETE(KeyVal.key);
                KeyVal.key = NULL;
            }
            else
            {
                // Increment the counter for number of value entries added to the list
                ++numentriesadded;
            }

            // Check if the max number of value entries were added
            if (max_entries > 0 && numentriesadded >= max_entries)
            {
                // Maximum number of values added so break out of the loop
                break;
            }
        }
    }

    iQCPParserNodeMetadataValueCount = (*valuelistptr).size();

    if ((iCPMMetaDataExtensionInterface != NULL))

    {
        iCPMGetMetaDataValuesCmdId =
            iCPMMetaDataExtensionInterface->GetNodeMetadataValues(iCPMSessionID,
                    (*keylistptr_in),
                    (*valuelistptr),
                    0);
        return PVMFPending;
    }
    return PVMFSuccess;
}

PVMFStatus PVMFQCPFFParserNode::DoSetDataSourcePosition(PVMFQCPFFNodeCommand& aCmd)
{
    //file must be parsed
    if (!iQCPParser)
    {
        return PVMFErrInvalidState;
    }

    if (iSelectedTrackList.size() == 0)
    {
        return PVMFErrInvalidState;
    }

    uint32 targetNPT = 0;
    uint32* actualNPT = NULL;
    uint32* actualMediaDataTS = NULL;
    bool seektosyncpoint = false;
    uint32 streamID = 0;

    aCmd.PVMFQCPFFNodeCommand::Parse(targetNPT, actualNPT, actualMediaDataTS, seektosyncpoint, streamID);

    Oscl_Vector<PVQCPFFNodeTrackPortInfo, PVMFQCPParserNodeAllocator>::iterator it;
    for (it = iSelectedTrackList.begin(); it != iSelectedTrackList.end(); it++)
    {
        it->iSendBOS = true;
    }

    //save the stream id for next media segment
    iStreamID = streamID;

    *actualNPT = 0;
    *actualMediaDataTS = 0;


    // Peek the next sample to get the duration of the last sample
    uint32 timestamp;
    int32 result = iQCPParser->PeekNextTimestamp(&timestamp);
    if (result != QcpBitstreamObject::EVERYTHING_OK)
    {
        return PVMFErrResource;
    }

    // get media data TS (should be equal to iContinuousTimeStamp)
    uint32 millisecTS = iSelectedTrackList[0].iClockConverter->get_converted_ts(1000);
    *actualMediaDataTS = millisecTS;

    // see if targetNPT is greater or equal than clip duration.
    uint32 durationms = 0;
    uint32 duration = durationms = Oscl_Int64_Utils::get_uint64_lower32(iQCPFileInfo.iDuration);
    uint32 timescale = iQCPFileInfo.iTimescale;
    if (timescale > 0 && timescale != 1000)
    {
        // Convert to milliseconds
        MediaClockConverter mcc(timescale);
        mcc.update_clock(duration);
        durationms = mcc.get_converted_ts(1000);
    }
    if (targetNPT >= durationms)
    {
        // report EOS for the track.
        for (uint32 i = 0; i < iSelectedTrackList.size(); ++i)
        {
            iSelectedTrackList[i].iSeqNum = 0;
            iSelectedTrackList[i].oEOSReached = true;
            iSelectedTrackList[i].oQueueOutgoingMessages = true;
            iSelectedTrackList[i].oEOSSent = false;
        }
        result = iQCPParser->ResetPlayback(0);
        if (result != QcpBitstreamObject::EVERYTHING_OK)
        {
            return PVMFErrResource;
        }

        *actualNPT = durationms;
        return PVMFSuccess;
    }


    // Reposition
    // If new position is past the end of clip, QCP FF should set the position to the last frame
    result = iQCPParser->ResetPlayback(targetNPT);
    if (result != QcpBitstreamObject::EVERYTHING_OK)
    {
        if (QcpBitstreamObject::END_OF_FILE == result)
        {
            for (uint32 i = 0; i < iSelectedTrackList.size(); ++i)
            {
                iSelectedTrackList[i].iSeqNum = 0;
                iSelectedTrackList[i].oEOSReached = true;
                iSelectedTrackList[i].oQueueOutgoingMessages = true;
                iSelectedTrackList[i].oEOSSent = false;
            }
            result = iQCPParser->ResetPlayback(0);
            if (result != QcpBitstreamObject::EVERYTHING_OK)
            {
                return PVMFErrResource;
            }

            *actualNPT = result;
            return PVMFSuccess;
        }
	else if (QcpBitstreamObject::DATA_INSUFFICIENT == result)
	{
	    // This condition could mean 2 things
	    // 1) End Of File reached for a local content
	    // 2) Insufficient data condition met for PDL use-case
	    // For 1 treat it as End of File and send End of Track
	    // For 2 we dont support reposition until the clip is fully downloaded,
	    // if the clip is fully downloaded and then we get Insufficient data condition
	    // treat it as End Of File.
	    if (iDownloadProgressInterface != NULL)
	    {
		// Check if the file is completely Downloaded or not
		if (!iDownloadComplete)
		{
		    // File not downloaded completely, return not Supported
		    return PVMFErrNotSupported;
		}
	    }

	    // Here either file is completely downlaoded if doing PDL or it is a local file,
	    // treat it as End of File in both cases
	    for (uint32 i = 0; i < iSelectedTrackList.size(); ++i)
	    {
		iSelectedTrackList[i].iSeqNum = 0;
		iSelectedTrackList[i].oEOSReached = true;
		iSelectedTrackList[i].oQueueOutgoingMessages = true;
		iSelectedTrackList[i].oEOSSent = false;
	    }
	    result = iQCPParser->ResetPlayback(0);
	    if (result != QcpBitstreamObject::EVERYTHING_OK)
	    {
		return PVMFErrResource;
	    }

	    *actualNPT = result;
	    return PVMFSuccess;
	}
        else
        {
            return PVMFErrResource;
        }
    }

    //Peek new position to get the actual new timestamp
    uint32 newtimestamp;
    result = iQCPParser->PeekNextTimestamp(&newtimestamp);
    if (result != QcpBitstreamObject::EVERYTHING_OK)
    {
        return PVMFErrResource;
    }
    *actualNPT = newtimestamp;


    ResetAllTracks();
    return PVMFSuccess;
}


PVMFStatus PVMFQCPFFParserNode::DoQueryDataSourcePosition(PVMFQCPFFNodeCommand& aCmd)
{
    //file must be parsed
    if (!iQCPParser)
    {
        return PVMFErrInvalidState;
    }

    if (iSelectedTrackList.size() == 0)
    {
        return PVMFErrInvalidState;
    }

    uint32 targetNPT = 0;
    uint32* actualNPT = NULL;
    bool seektosyncpoint = false;

    aCmd.PVMFQCPFFNodeCommand::Parse(targetNPT, actualNPT, seektosyncpoint);
    if (actualNPT == NULL)
    {
        return PVMFErrArgument;
    }

    // Query
    // If new position is past the end of clip, QCP FF should set the position to the last frame
    *actualNPT = iQCPParser->SeekPointFromTimestamp(targetNPT);

    return PVMFSuccess;
}

PVMFStatus PVMFQCPFFParserNode::DoSetDataSourceRate(PVMFQCPFFNodeCommand& aCmd)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFQCPFFParserNode::DoSetDataSourceRate() In"));
    OSCL_UNUSED_ARG(aCmd);
    return PVMFSuccess;
}

bool PVMFQCPFFParserNode::SendEndOfTrackCommand(PVQCPFFNodeTrackPortInfo& aTrackPortInfo)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFQCPFFParserNode::SendEndOfTrackCommand() "));

    PVMFSharedMediaCmdPtr sharedMediaCmdPtr = PVMFMediaCmd::createMediaCmd();
    sharedMediaCmdPtr->setFormatID(PVMF_MEDIA_CMD_EOS_FORMAT_ID);

    sharedMediaCmdPtr->setStreamID(iStreamID);

    uint32 timestamp = Oscl_Int64_Utils::get_uint64_lower32(aTrackPortInfo.iContinuousTimeStamp);

    sharedMediaCmdPtr->setTimestamp(timestamp);
    sharedMediaCmdPtr->setSeqNum(aTrackPortInfo.iSeqNum++);

    PVMFSharedMediaMsgPtr mediaMsgOut;
    convertToPVMFMediaCmdMsg(mediaMsgOut, sharedMediaCmdPtr);

    if (aTrackPortInfo.iPort->QueueOutgoingMsg(mediaMsgOut) != PVMFSuccess)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFQCPFFParserNode::SendEndOfTrackCommand() Failed-- Busy "));
        return false;
    }
    aTrackPortInfo.oQueueOutgoingMessages = false;
    aTrackPortInfo.oProcessOutgoingMessages = true;

    return true;
}

void PVMFQCPFFParserNode::HandlePortActivity(const PVMFPortActivity &aActivity)
{

    switch (aActivity.iType)
    {
        case PVMF_PORT_ACTIVITY_OUTGOING_MSG:
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                            (0, "PVMFQCPFFParserNode::PortActivity: Outgoing Msg"));
            RunIfNotReady();
            break;

        case PVMF_PORT_ACTIVITY_INCOMING_MSG:
            break;

        case PVMF_PORT_ACTIVITY_CONNECT:
            break;

        case PVMF_PORT_ACTIVITY_DISCONNECT:
            //nothing needed.

        case PVMF_PORT_ACTIVITY_CONNECTED_PORT_READY:
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                            (0, "PVMFQCPFFParserNode::PortActivity: Connected port ready"));
            //This message is send by destination port to notify that the earlier Send
            //call that failed due to its busy status can be resumed now.
            if (iOutPort
                    && iOutPort->OutgoingMsgQueueSize() > 0)
            {
                RunIfNotReady();
            }
            break;

        case PVMF_PORT_ACTIVITY_OUTGOING_QUEUE_READY:
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                            (0, "PVMFQCPFFParserNode::PortActivity: Outgoing Queue ready"));
            //this message is sent by the OutgoingQueue when it recovers from
            //the queue full status
            RunIfNotReady();
            break;

        default:
            break;
    }
}

bool PVMFQCPFFParserNode::ProcessCommand()
{
    //This call will process the first node command in the input queue.
    //Can't do anything when an asynchronous cancel is in progress-- just
    //need to wait on completion.
    if (!iCancelCommand.empty())
        return false;

    //If a command is in progress, only a hi-pri command can interrupt it.
    if (!iCurrentCommand.empty()  && !iInputCommands.front().hipri() && iInputCommands.front().iCmd != PVMF_QCP_PARSER_NODE_CMD_CANCEL_GET_LICENSE)
    {
        return false;
    }

    //The newest or highest pri command is in the front of the queue.
    OSCL_ASSERT(!iInputCommands.empty());
    PVMFQCPFFNodeCommand& aCmd = iInputCommands.front();

    PVMFStatus cmdstatus;
    OsclAny* eventdata = NULL;
    if (aCmd.hipri())
    {
        switch (aCmd.iCmd)
        {
            case PVMF_QCP_PARSER_NODE_CANCELALLCOMMANDS:
                DoCancelAllCommands(aCmd);
                break;

            case PVMF_QCP_PARSER_NODE_CANCELCOMMAND:
                DoCancelCommand(aCmd);
                break;

            default:
                CommandComplete(iInputCommands, aCmd, PVMFErrNotSupported);
                break;
        }

        //If completion is pending, move the command from
        //the input queue to the cancel queue.
        //This is necessary since the input queue could get
        //rearranged by new commands coming in.
    }
    else
    {
        //Process the normal pri commands.
        switch (aCmd.iCmd)
        {
            case PVMF_QCP_PARSER_NODE_QUERYUUID:
                DoQueryUuid(aCmd);
                break;

            case PVMF_QCP_PARSER_NODE_QUERYINTERFACE:
                DoQueryInterface(aCmd);
                break;

            case PVMF_QCP_PARSER_NODE_REQUESTPORT:
            {
                PVMFPortInterface*port;
                DoRequestPort(aCmd, port);
                eventdata = (OsclAny*)port;
            }
            break;

            case PVMF_QCP_PARSER_NODE_RELEASEPORT:
                DoReleasePort(aCmd);
                break;

            case PVMF_QCP_PARSER_NODE_INIT:
                cmdstatus = DoInit(aCmd);
                if (cmdstatus != PVMFPending)
                {
                    CommandComplete(iInputCommands, aCmd, cmdstatus);
                }
                else
                {
                    MoveCmdToCurrentQueue(aCmd);
                }
                break;

            case PVMF_QCP_PARSER_NODE_PREPARE:
                DoPrepare(aCmd);
                break;

            case PVMF_QCP_PARSER_NODE_START:
                DoStart(aCmd);
                break;

            case PVMF_QCP_PARSER_NODE_STOP:
                DoStop(aCmd);
                break;

            case PVMF_QCP_PARSER_NODE_FLUSH:
                DoFlush(aCmd);
                break;

            case PVMF_QCP_PARSER_NODE_PAUSE:
                DoPause(aCmd);
                break;

            case PVMF_QCP_PARSER_NODE_RESET:
                DoReset(aCmd);
                break;

            case PVMF_QCP_PARSER_NODE_GETNODEMETADATAKEYS:
            {
                cmdstatus = DoGetMetadataKeys(aCmd);
                if (cmdstatus != PVMFPending)
                {
                    CommandComplete(iInputCommands, aCmd, cmdstatus);
                }
                else
                {
                    MoveCmdToCurrentQueue(aCmd);
                }
            }
            break;

            case PVMF_QCP_PARSER_NODE_GETNODEMETADATAVALUES:
            {
                cmdstatus = DoGetMetadataValues(aCmd);
                if (cmdstatus != PVMFPending)
                {
                    CommandComplete(iInputCommands, aCmd, cmdstatus);
                }
                else
                {
                    MoveCmdToCurrentQueue(aCmd);
                }
            }
            break;

            case PVMF_QCP_PARSER_NODE_SET_DATASOURCE_POSITION:
            {
                cmdstatus = DoSetDataSourcePosition(aCmd);
                if (cmdstatus != PVMFPending)
                {
                    CommandComplete(iInputCommands, aCmd, cmdstatus);
                }
                else
                {
                    MoveCmdToCurrentQueue(aCmd);
                }
            }
            break;

            case PVMF_QCP_PARSER_NODE_QUERY_DATASOURCE_POSITION:
            {
                cmdstatus = DoQueryDataSourcePosition(aCmd);
                if (cmdstatus != PVMFPending)
                {
                    CommandComplete(iInputCommands, aCmd, cmdstatus);
                }
                else
                {
                    MoveCmdToCurrentQueue(aCmd);
                }
            }
            break;

            case PVMF_QCP_PARSER_NODE_SET_DATASOURCE_RATE:
            {
                PVMFStatus status = DoSetDataSourceRate(aCmd);
                CommandComplete(iInputCommands, aCmd, status);
            }
            break;

            case PVMF_QCP_PARSER_NODE_GET_LICENSE_W:
            {
                PVMFStatus status = DoGetLicense(aCmd, true);
                if (status == PVMFPending)
                {
                    MoveCmdToCurrentQueue(aCmd);
                }
                else
                {
                    CommandComplete(iInputCommands, aCmd, status);
                }
            }
            break;

            case PVMF_QCP_PARSER_NODE_GET_LICENSE:
            {
                PVMFStatus status = DoGetLicense(aCmd);
                if (status == PVMFPending)
                {
                    MoveCmdToCurrentQueue(aCmd);
                }
                else
                {
                    CommandComplete(iInputCommands, aCmd, status);
                }
            }
            break;

            case PVMF_QCP_PARSER_NODE_CMD_CANCEL_GET_LICENSE:
                cmdstatus = DoCancelGetLicense(aCmd);
                switch (cmdstatus)
                {
                    case PVMFPending:
                        MoveCmdToCancelQueue(aCmd);
                        //wait on CPM callback.
                        break;
                    default:
                        CommandComplete(iInputCommands, aCmd, cmdstatus);
                        break;
                }
                break;

            default:
                OSCL_ASSERT(false);
                CommandComplete(iInputCommands, aCmd, PVMFFailure);
                break;
        }
    }
    return true;
}

void PVMFQCPFFParserNode::SetState(TPVMFNodeInterfaceState s)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_MLDBG, iLogger, PVLOGMSG_INFO, (0, "PVMFQCPFFParserNode:SetState"));
    PVMFNodeInterface::SetState(s);
}

void PVMFQCPFFParserNode::ReportErrorEvent(PVMFEventType aEventType, OsclAny* aEventData, PVUuid* aEventUUID, int32* aEventCode)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_MLDBG, iLogger, PVLOGMSG_INFO, (0, "PVMFQCPFFParserNode:ReportErrorEvent Type %d Data %d"
                    , aEventType, aEventData));

    if (aEventUUID && aEventCode)
    {
        PVMFBasicErrorInfoMessage* eventmsg;
        PVMF_QCP_PARSER_NODE_NEW(NULL,
                                 PVMFBasicErrorInfoMessage,
                                 (*aEventCode, *aEventUUID, NULL),
                                 eventmsg);
        PVMFAsyncEvent asyncevent(PVMFErrorEvent,
                                  aEventType,
                                  NULL,
                                  OSCL_STATIC_CAST(PVInterface*, eventmsg),
                                  aEventData,
                                  NULL,
                                  0);
        PVMFNodeInterface::ReportErrorEvent(asyncevent);
        eventmsg->removeRef();
    }
    else
    {
        PVMFNodeInterface::ReportErrorEvent(aEventType, aEventData);
    }
    /* Transition the node to an error state */
    iInterfaceState = EPVMFNodeError;
}

void PVMFQCPFFParserNode::ReportInfoEvent(PVMFEventType aEventType, OsclAny* aEventData, PVUuid* aEventUUID, int32* aEventCode)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_MLDBG, iLogger, PVLOGMSG_INFO, (0, "PVMFQCPFFParserNode:ReportInfoEvent Type %d Data %d"
                    , aEventType, aEventData));

    if (aEventUUID && aEventCode)
    {
        PVMFBasicErrorInfoMessage* eventmsg;
        PVMF_QCP_PARSER_NODE_NEW(NULL,
                                 PVMFBasicErrorInfoMessage,
                                 (*aEventCode, *aEventUUID, NULL),
                                 eventmsg);
        PVMFAsyncEvent asyncevent(PVMFInfoEvent,
                                  aEventType,
                                  NULL,
                                  OSCL_STATIC_CAST(PVInterface*, eventmsg),
                                  aEventData,
                                  NULL,
                                  0);
        PVMFNodeInterface::ReportInfoEvent(asyncevent);
        eventmsg->removeRef();
    }
    else
    {
        PVMFNodeInterface::ReportInfoEvent(aEventType, aEventData);
    }
}

void PVMFQCPFFParserNode::DoQueryUuid(PVMFQCPFFNodeCommand& aCmd)
{
    OSCL_String* mimetype;
    Oscl_Vector<PVUuid, OsclMemAllocator> *uuidvec;
    bool exactmatch;
    aCmd.PVMFQCPFFNodeCommandBase::Parse(mimetype, uuidvec, exactmatch);

    if (*mimetype == PVMF_DATA_SOURCE_INIT_INTERFACE_MIMETYPE)
    {
        PVUuid uuid(PVMF_DATA_SOURCE_INIT_INTERFACE_UUID);
        uuidvec->push_back(uuid);
    }
    else if (*mimetype == PVMF_TRACK_SELECTION_INTERFACE_MIMETYPE)
    {
        PVUuid uuid(PVMF_TRACK_SELECTION_INTERFACE_UUID);
        uuidvec->push_back(uuid);
    }
    else if (*mimetype == PVMF_DATA_SOURCE_PLAYBACK_CONTROL_INTERFACE_MIMETYPE)
    {
        PVUuid uuid(PvmfDataSourcePlaybackControlUuid);
        uuidvec->push_back(uuid);
    }
    else if (*mimetype == PVMF_META_DATA_EXTENSION_INTERFACE_MIMETYPE)
    {
        PVUuid uuid(KPVMFMetadataExtensionUuid);
        uuidvec->push_back(uuid);
    }

    CommandComplete(iInputCommands, aCmd, PVMFSuccess);
    return;
}

void PVMFQCPFFParserNode::DoQueryInterface(PVMFQCPFFNodeCommand&  aCmd)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "PVMFQCPFFParserNode::DoQueryInterface"));

    PVUuid* uuid;
    PVInterface** ptr;
    aCmd.PVMFQCPFFNodeCommandBase::Parse(uuid, ptr);

    if (queryInterface(*uuid, *ptr))
    {
        (*ptr)->addRef();
        CommandComplete(iInputCommands, aCmd, PVMFSuccess);
    }
    else
    {
        *ptr = NULL;
        CommandComplete(iInputCommands, aCmd, PVMFErrNotSupported);
    }
    return;
}

PVMFStatus PVMFQCPFFParserNode::DoInit(PVMFQCPFFNodeCommand& aCmd)
{
    OSCL_UNUSED_ARG(aCmd);
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFQCPFFParserNode::DoInitNode() In"));

    if (iInterfaceState != EPVMFNodeIdle)
    {
        return PVMFErrInvalidState;
    }
    if (iCPM)
    {
        /*
         * Go thru CPM commands before parsing the file in case
         * of a new source file.
         * - Init CPM
         * - Open Session
         * - Register Content
         * - Get Content Type
         * - Approve Usage
         * In case the source file has already been parsed skip to
         * - Approve Usage
         */
        if (oSourceIsCurrent == false)
        {
            InitCPM();
        }
        else
        {
            RequestUsage();
        }
        return PVMFPending;
    }
    else
    {
        if (CheckForQCPHeaderAvailability() == PVMFSuccess)
        {
            ParseQCPFile();
            SetState(EPVMFNodeInitialized);
            return PVMFSuccess;
        }
    }
    return PVMFSuccess;
}

PVMFStatus PVMFQCPFFParserNode::ParseQCPFile()
{
    iQCPParser = OSCL_NEW(CQCPFileParser, ());
    if (!iQCPParser)
    {
        return PVMFErrNoMemory;
    }

    PVMFDataStreamFactory* dsFactory = iCPMContentAccessFactory;
    bool calcDuration = true;
    if ((dsFactory == NULL) && (iDataStreamFactory != NULL))
    {
        dsFactory = iDataStreamFactory;
        calcDuration = false;
    }

    if (iQCPParser->InitQCPFile(iSourceURL, calcDuration, &iFileServer, dsFactory, iFileHandle, iCountToClaculateRDATimeInterval))
    {
        iAvailableMetadataKeys.clear();
        if (iQCPParser->RetrieveFileInfo(iQCPFileInfo))
        {
            PVMFStatus status = InitMetaData();
            if (status == PVMFSuccess)
            {
                return PVMFSuccess;
            }
            else
            {
                CleanupFileSource();

                PVMF_QCPPARSERNODE_LOGERROR((0, "PVMFQCPParserNode::ParseQCPFile() - InitMetaData Failed"));

                CommandComplete(iCurrentCommand,
                                iCurrentCommand.front(),
                                status
                               );
            }

        }
        else
        {
            return PVMFErrResource;
        }
    }
    else
    {
        //cleanup if failure
        OSCL_DELETE(iQCPParser);
        iQCPParser = NULL;
        return PVMFErrResource;
    }
    return PVMFSuccess;
}

void PVMFQCPFFParserNode::DoPrepare(PVMFQCPFFNodeCommand& aCmd)
{
    if (iInterfaceState != EPVMFNodeInitialized)
    {
        CommandComplete(iInputCommands, aCmd, PVMFErrInvalidState);
        return;
    }
    SetState(EPVMFNodePrepared);
    CommandComplete(iInputCommands, aCmd, PVMFSuccess);
    return;
}

void PVMFQCPFFParserNode::DoStart(PVMFQCPFFNodeCommand& aCmd)
{
    if (iInterfaceState != EPVMFNodePrepared &&
            iInterfaceState != EPVMFNodePaused)
    {
        CommandComplete(iInputCommands, aCmd, PVMFErrInvalidState);
        return;
    }
    SetState(EPVMFNodeStarted);
    CommandComplete(iInputCommands, aCmd, PVMFSuccess);
    return;
}

void PVMFQCPFFParserNode::DoStop(PVMFQCPFFNodeCommand& aCmd)
{
    iStreamID = 0;

    if (iInterfaceState != EPVMFNodeStarted &&
            iInterfaceState != EPVMFNodePaused)
    {
        CommandComplete(iInputCommands, aCmd, PVMFErrInvalidState);
        return;
    }
    if (iDataStreamInterface != NULL)
    {
        PVInterface* iFace = OSCL_STATIC_CAST(PVInterface*, iDataStreamInterface);
        PVUuid uuid = PVMIDataStreamSyncInterfaceUuid;
        iDataStreamFactory->DestroyPVMFCPMPluginAccessInterface(uuid, iFace);
        iDataStreamInterface = NULL;
    }
    // stop and reset position to beginning
    ResetAllTracks();

    // Reset the QCP FF to beginning
    if (iQCPParser)
    {
        iQCPParser->ResetPlayback(0);
    }

    //clear msg queue
    if (iOutPort)
    {
        iOutPort->ClearMsgQueues();
    }
    SetState(EPVMFNodePrepared);
    CommandComplete(iInputCommands, aCmd, PVMFSuccess);
    return;
}

void PVMFQCPFFParserNode::DoPause(PVMFQCPFFNodeCommand& aCmd)
{
    if (iInterfaceState != EPVMFNodeStarted)
    {
        CommandComplete(iInputCommands, aCmd, PVMFErrInvalidState);
        return;
    }
    SetState(EPVMFNodePaused);
    CommandComplete(iInputCommands, aCmd, PVMFSuccess);
    return;

}

void PVMFQCPFFParserNode::DoFlush(PVMFQCPFFNodeCommand& aCmd)
{

    if (iInterfaceState != EPVMFNodeStarted &&
            iInterfaceState != EPVMFNodePaused)
    {
        CommandComplete(iInputCommands, aCmd, PVMFErrInvalidState);
        return;
    }

    /*
     * the flush is asynchronous.  move the command from
     * the input command queue to the current command, where
     * it will remain until the flush completes.
     */
    MoveCmdToCurrentQueue(aCmd);
    return;

}

bool PVMFQCPFFParserNode::FlushPending()
{
    return (iCurrentCommand.size() > 0
            && iCurrentCommand.front().iCmd == PVMF_QCP_PARSER_NODE_FLUSH);
}

void PVMFQCPFFParserNode::DoReset(PVMFQCPFFNodeCommand& aCmd)
{

    PVMF_QCPPARSERNODE_LOGSTACKTRACE((0, "PVMFQCPParserNode::DoReset() Called"));

    if (iDownloadProgressInterface != NULL)
    {
        iDownloadProgressInterface->cancelResumeNotification();
    }
    MoveCmdToCurrentQueue(aCmd);
    if (iFileHandle != NULL)
    {
        /* Indicates that the init was successfull */
        if ((iCPM))
        {
            SendUsageComplete();
        }
        else
        {
            CompleteReset();
        }
    }
    else
    {
        /*
         * Reset without init completing, so just reset the parser node,
         * no CPM stuff necessary
         */
        CompleteReset();
    }
}

void
PVMFQCPFFParserNode::MoveCmdToCurrentQueue(PVMFQCPFFNodeCommand& aCmd)
{
    int32 err;
    OSCL_TRY(err, iCurrentCommand.StoreL(aCmd););
    if (err != OsclErrNone)
    {
        CommandComplete(iInputCommands, aCmd, PVMFErrNoMemory);
        return;
    }
    iInputCommands.Erase(&aCmd);
    return;
}
void
PVMFQCPFFParserNode::MoveCmdToCancelQueue(PVMFQCPFFNodeCommand& aCmd)
{
    /*
     * note: the StoreL cannot fail since the queue is never more than 1 deep
     * and we reserved space.
     */
    iCancelCommand.StoreL(aCmd);
    iInputCommands.Erase(&aCmd);
}

void PVMFQCPFFParserNode::DoCancelAllCommands(PVMFQCPFFNodeCommand& aCmd)
{
    while (!iCurrentCommand.empty())
    {
        MoveCmdToCancelQueue(aCmd);
    }

    //next cancel all queued commands
    //start at element 1 since this cancel command is element 0.
    while (iInputCommands.size() > 1)
    {
        CommandComplete(iInputCommands, iInputCommands[1], PVMFErrCancelled);
    }

    //finally, report cancel complete.
    CommandComplete(iInputCommands, iInputCommands[0], PVMFSuccess);
    return;

}

void PVMFQCPFFParserNode::DoCancelCommand(PVMFQCPFFNodeCommand& aCmd)
{
    PVMFCommandId id;
    aCmd.PVMFQCPFFNodeCommandBase::Parse(id);
    {
        PVMFQCPFFNodeCommand* cmd = iCurrentCommand.FindById(id);
        if (cmd)
        {
            //cancel the queued command

            MoveCmdToCancelQueue(*cmd);
            CommandComplete(iInputCommands, aCmd, PVMFSuccess);
            return;
        }
    }

    {
        PVMFQCPFFNodeCommand* cmd = iInputCommands.FindById(id, 1);
        if (cmd)
        {
            //cancel the queued command
            CommandComplete(iInputCommands, *cmd, PVMFErrCancelled);

            CommandComplete(iInputCommands, aCmd, PVMFSuccess);
            return;
        }
    }
    CommandComplete(iInputCommands, aCmd, PVMFErrArgument);
    return;
}

void PVMFQCPFFParserNode::DoRequestPort(PVMFQCPFFNodeCommand& aCmd, PVMFPortInterface*&aPort)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFQCPFFParserNode::DoRequestPort() In"));
    aPort = NULL;

    if ((iInterfaceState != EPVMFNodePrepared) || (!iQCPParser))
    {
        PVMF_QCPPARSERNODE_LOGERROR((0, "PVMFASFParserNode::DoRequestPort() - Invalid State"));
        CommandComplete(iInputCommands, aCmd, PVMFErrInvalidState);
        return;
    }

    int32 tag = 0;
    OSCL_String* mime_string;
    aCmd.PVMFQCPFFNodeCommandBase::Parse(tag, mime_string);

    if (tag != PVMF_QCPFFPARSER_NODE_PORT_TYPE_SOURCE)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_ERR,
                        (0, "PVMFQCPFFParserNode::DoRequestPort: Error - Invalid port tag"));
        CommandComplete(iInputCommands, aCmd, PVMFFailure);
        return;
    }

    if (iOutPort)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                        (0, "PVMFQCPFFParserNode::DoRequestPort: Error - port already exists"));
        CommandComplete(iInputCommands, aCmd, PVMFFailure);
        return;
    }

    if ((int32)aCmd.iParam1 == PVMF_QCPFFPARSER_NODE_PORT_TYPE_SOURCE)
    {

        iOutPort = OSCL_NEW(PVMFQCPFFParserOutPort, (PVMF_QCPFFPARSER_NODE_PORT_TYPE_SOURCE, this));
        if (!iOutPort)
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                            (0, "PVMFQCPFFParserNode::DoRequestPort: Error - no memory"));
            CommandComplete(iInputCommands, aCmd, PVMFErrNoMemory);
            return;
        }
        if (mime_string)
        {
            PVMFFormatType fmt = mime_string->get_str();
            if (!iOutPort->IsFormatSupported(fmt))
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                                (0, "PVMFQCPFFParserNode::DoRequestPort: Error - format not supported"));
                OSCL_DELETE(iOutPort);
                iOutPort = NULL;
                CommandComplete(iInputCommands, aCmd, PVMFFailure);
                return;
            }
        }

        MediaClockConverter* clockconv = NULL;
        OsclMemPoolFixedChunkAllocator* trackdatamempool = NULL;
        PVMFSimpleMediaBufferCombinedAlloc* mediadataimplalloc = NULL;
        PVMFMemPoolFixedChunkAllocator* mediadatamempool = NULL;
        int32 leavecode = 0;
        OSCL_TRY(leavecode,
                 clockconv = OSCL_NEW(MediaClockConverter, (iQCPFileInfo.iTimescale));
                 trackdatamempool = OSCL_NEW(OsclMemPoolFixedChunkAllocator, (PVQCPFF_MEDIADATA_POOLNUM));
                 mediadataimplalloc = OSCL_NEW(PVMFSimpleMediaBufferCombinedAlloc, (trackdatamempool));
                 mediadatamempool = OSCL_NEW(PVMFMemPoolFixedChunkAllocator, ("QcpFFPar", PVQCPFF_MEDIADATA_POOLNUM, PVQCPFF_MEDIADATA_CHUNKSIZE));
                );

        if (leavecode || !clockconv || !trackdatamempool || !mediadataimplalloc || !mediadatamempool)
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                            (0, "PVMFQCPFFParserNode::DoRequestPort: Error - unable to create clockconv, trackdatamempool, mediadataimplalloc, and mediadatamempool"));
            if (iOutPort)
            {
                OSCL_DELETE(iOutPort);
                iOutPort = NULL;
            }
            if (clockconv)
            {
                OSCL_DELETE(clockconv);
            }
            if (trackdatamempool)
            {
                OSCL_DELETE(trackdatamempool);
            }
            if (mediadataimplalloc)
            {
                OSCL_DELETE(mediadataimplalloc);
            }
            if (mediadatamempool)
            {
                OSCL_DELETE(mediadatamempool);
            }

            CommandComplete(iInputCommands, aCmd, PVMFErrNoMemory);
            return;
        }

        mediadatamempool->enablenullpointerreturn();

        PVQCPFFNodeTrackPortInfo trackportinfo;

        trackportinfo.iTrackId = 0;  // Only support 1 channel so far
        trackportinfo.iTag = PVMF_QCPFFPARSER_NODE_PORT_TYPE_SOURCE;
        trackportinfo.iPort = iOutPort;

        trackportinfo.iClockConverter = clockconv;
        trackportinfo.iTrackDataMemoryPool = trackdatamempool;
        trackportinfo.iMediaDataImplAlloc = mediadataimplalloc;
        trackportinfo.iMediaDataMemPool = mediadatamempool;

        aPort = iOutPort;

        OsclMemPoolResizableAllocator* trackDataResizableMemPool = NULL;
        trackportinfo.iResizableDataMemoryPoolSize = PVMF_QCP_PARSER_NODE_MAX_AUDIO_DATA_MEM_POOL_SIZE;
        PVMF_QCP_PARSER_NODE_NEW(NULL,
                                 OsclMemPoolResizableAllocator,
                                 (trackportinfo.iResizableDataMemoryPoolSize,
                                  PVMF_QCP_PARSER_NODE_DATA_MEM_POOL_GROWTH_LIMIT),
                                 trackDataResizableMemPool);

        PVUuid eventuuid = PVMFQCPParserNodeEventTypesUUID;
        int32   errcode = PVMFQCPFFParserErrTrackMediaMsgAllocatorCreationFailed;

        PVMFResizableSimpleMediaMsgAlloc* resizableSimpleMediaDataImplAlloc = NULL;
        OsclExclusivePtr<PVMFResizableSimpleMediaMsgAlloc> resizableSimpleMediaDataImplAllocAutoPtr;
        PVMF_QCP_PARSER_NODE_NEW(NULL,
                                 PVMFResizableSimpleMediaMsgAlloc,
                                 (trackDataResizableMemPool),
                                 resizableSimpleMediaDataImplAlloc);

        if (trackDataResizableMemPool == NULL)
        {
            PVMF_QCPPARSERNODE_LOGERROR((0, "PVMFQCPParserNode::DoRequestPort() - trackDataResizableMemPool Alloc Failed"));
            CommandComplete(iInputCommands,
                            aCmd,
                            PVMFErrNoMemory,
                            NULL,
                            &eventuuid,
                            &errcode);
            return;
        }

        trackDataResizableMemPool->enablenullpointerreturn();

        trackportinfo.iResizableSimpleMediaMsgAlloc = resizableSimpleMediaDataImplAlloc;
        trackportinfo.iResizableDataMemoryPool = trackDataResizableMemPool;
        trackportinfo.iNode = this;
        uint8* typeSpecificInfoBuff = iQCPParser->getCodecSpecificInfo();
        uint32 typeSpecificDataLength = MAX_NUM_PACKED_INPUT_BYTES;
        if ((int32)typeSpecificDataLength > 0)
        {
            OsclMemAllocDestructDealloc<uint8> my_alloc;
            OsclRefCounter* my_refcnt;
            uint aligned_refcnt_size =
                oscl_mem_aligned_size(sizeof(OsclRefCounterSA< OsclMemAllocDestructDealloc<uint8> >));
            uint aligned_type_specific_info_size =
                oscl_mem_aligned_size(typeSpecificDataLength);
            uint8* my_ptr = NULL;
            int32 errcode = 0;
            OSCL_TRY(errcode,
                     my_ptr = (uint8*) my_alloc.ALLOCATE(aligned_refcnt_size + aligned_type_specific_info_size));

            if (errcode != OsclErrNone)
            {
                PVMF_QCPPARSERNODE_LOGERROR((0, "PVMFQCPParserNode::PopulateTrackInfoVec - Unable to Allocate Memory"));
            }

            my_refcnt = OSCL_PLACEMENT_NEW(my_ptr, OsclRefCounterSA< OsclMemAllocDestructDealloc<uint8> >(my_ptr));
            my_ptr += aligned_refcnt_size;

            OsclMemoryFragment memfrag;
            memfrag.len = typeSpecificDataLength;
            memfrag.ptr = typeSpecificInfoBuff;

            OsclRefCounterMemFrag tmpRefcntMemFrag(memfrag, my_refcnt, memfrag.len);
            trackportinfo.iFormatSpecificConfig = tmpRefcntMemFrag;
        }

        iSelectedTrackList.push_back(trackportinfo);


        CommandComplete(iInputCommands, aCmd, PVMFSuccess, (OsclAny*)aPort);
        return;
    }
    else
    {
        // don't support other types yet
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                        (0, "PVMFQCPFFParserNode::DoRequestPort: Error - type not supported"));
        CommandComplete(iInputCommands, aCmd, PVMFFailure);
        return;
    }

}

void PVMFQCPFFParserNode::DoReleasePort(PVMFQCPFFNodeCommand& aCmd)
{

    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "PVMFQCPFFParserNode::DoReleasePort"));

    // search for the matching port address
    // disconnect it, if needed
    // cleanup the buffers associated with it
    // delete the port
    // set the address to NULL

    // Remove the selected track from the track list
    for (uint32 i = 0;i < iSelectedTrackList.size();i++)
    {
        if (iSelectedTrackList[i].iPort == aCmd.iParam1)
        {
            // Found the element. So erase it
            iSelectedTrackList[i].iMediaData.Unbind();
            OSCL_DELETE(((PVMFQCPFFParserOutPort*)iSelectedTrackList[i].iPort));
            iSelectedTrackList[i].iPort = NULL;
            iOutPort = NULL;
            if (iSelectedTrackList[i].iClockConverter)
            {
                OSCL_DELETE(iSelectedTrackList[i].iClockConverter);
            }
            if (iSelectedTrackList[i].iTrackDataMemoryPool)
            {
                iSelectedTrackList[i].iTrackDataMemoryPool->removeRef();
                iSelectedTrackList[i].iTrackDataMemoryPool = NULL;
            }
            if (iSelectedTrackList[i].iMediaDataImplAlloc)
            {
                OSCL_DELETE(iSelectedTrackList[i].iMediaDataImplAlloc);
            }
            if (iSelectedTrackList[i].iMediaDataMemPool)
            {
                iSelectedTrackList[i].iMediaDataMemPool->CancelFreeChunkAvailableCallback();
                iSelectedTrackList[i].iMediaDataMemPool->removeRef();
                iSelectedTrackList[i].iMediaDataMemPool = NULL;
            }

            if (iSelectedTrackList[i].iResizableSimpleMediaMsgAlloc != NULL)
            {
                PVMF_QCP_PARSER_NODE_DELETE(NULL,
                                            PVMFResizableSimpleMediaMsgAlloc,
                                            iSelectedTrackList[i].iResizableSimpleMediaMsgAlloc);
                iSelectedTrackList[i].iResizableSimpleMediaMsgAlloc = NULL;
            }
            if (iSelectedTrackList[i].iResizableDataMemoryPool != NULL)
            {
                iSelectedTrackList[i].iResizableDataMemoryPool->removeRef();
                iSelectedTrackList[i].iResizableDataMemoryPool = NULL;
            }
            iSelectedTrackList.erase(&iSelectedTrackList[i]);
            CommandComplete(iInputCommands, aCmd, PVMFSuccess);
            return;
        }
    }

    //if we get here the track was not found
    CommandComplete(iInputCommands, aCmd, PVMFErrBadHandle);
    return;

}

void PVMFQCPFFParserNode::ResetAllTracks()
{
    for (uint32 i = 0; i < iSelectedTrackList.size(); ++i)
    {
        iSelectedTrackList[i].iMediaData.Unbind();
        iSelectedTrackList[i].iSeqNum = 0;
        iSelectedTrackList[i].iFirstFrame = true;

        iSelectedTrackList[i].oEOSSent = false;
        iSelectedTrackList[i].oEOSReached = false;
        iSelectedTrackList[i].oQueueOutgoingMessages = true;
    }
}

bool PVMFQCPFFParserNode::ReleaseAllPorts()
{
    while (!iSelectedTrackList.empty())
    {
        iSelectedTrackList[0].iPort->Disconnect();
        iSelectedTrackList[0].iMediaData.Unbind();
        OSCL_DELETE(((PVMFQCPFFParserOutPort*)iSelectedTrackList[0].iPort));
        if (iSelectedTrackList[0].iClockConverter)
        {
            OSCL_DELETE(iSelectedTrackList[0].iClockConverter);
        }
        if (iSelectedTrackList[0].iTrackDataMemoryPool)
        {
            iSelectedTrackList[0].iTrackDataMemoryPool->removeRef();
            iSelectedTrackList[0].iTrackDataMemoryPool = NULL;
        }
        if (iSelectedTrackList[0].iMediaDataImplAlloc)
        {
            OSCL_DELETE(iSelectedTrackList[0].iMediaDataImplAlloc);
        }
        if (iSelectedTrackList[0].iMediaDataMemPool)
        {
            iSelectedTrackList[0].iMediaDataMemPool->CancelFreeChunkAvailableCallback();
            iSelectedTrackList[0].iMediaDataMemPool->removeRef();
            iSelectedTrackList[0].iMediaDataMemPool = NULL;
        }
        iOutPort = NULL;

        if (iSelectedTrackList[0].iResizableSimpleMediaMsgAlloc != NULL)
        {
            PVMF_QCP_PARSER_NODE_DELETE(NULL,
                                        PVMFResizableSimpleMediaMsgAlloc,
                                        iSelectedTrackList[0].iResizableSimpleMediaMsgAlloc);
            iSelectedTrackList[0].iResizableSimpleMediaMsgAlloc = NULL;
        }
        if (iSelectedTrackList[0].iResizableDataMemoryPool != NULL)
        {
            iSelectedTrackList[0].iResizableDataMemoryPool->removeRef();
            iSelectedTrackList[0].iResizableDataMemoryPool = NULL;
        }
        iSelectedTrackList.erase(iSelectedTrackList.begin());
    }
    return true;
}

void PVMFQCPFFParserNode::CleanupFileSource()
{
    iAvailableMetadataKeys.clear();

    if (iQCPParser)
    {
        OSCL_DELETE(iQCPParser);
    }
    iQCPParser = NULL;

    iUseCPMPluginRegistry = false;
    iCPMSourceData.iFileHandle = NULL;
    iQCPParserNodeMetadataValueCount = 0;

    if (iCPMContentAccessFactory != NULL)
    {
        iCPMContentAccessFactory->removeRef();
        iCPMContentAccessFactory = NULL;
    }
    if (iDataStreamFactory != NULL)
    {
        iDataStreamFactory->removeRef();
        iDataStreamFactory = NULL;
    }
    iCPMContentType = PVMF_CPM_CONTENT_FORMAT_UNKNOWN;
    iPreviewMode = false;
    oSourceIsCurrent = false;
    if (iFileHandle)
    {
        OSCL_DELETE(iFileHandle);
    }
    iFileHandle = NULL;
}

void PVMFQCPFFParserNode::CommandComplete(PVMFQCPFFNodeCmdQ& aCmdQ, PVMFQCPFFNodeCommand& aCmd, PVMFStatus aStatus, PVInterface*aExtMsg, OsclAny* aEventData)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_MLDBG, iLogger, PVLOGMSG_INFO, (0, "PVMFQCPFFParserNode:CommandComplete Id %d Cmd %d Status %d Context %d Data %d"
                    , aCmd.iId, aCmd.iCmd, aStatus, aCmd.iContext, aEventData));

    PVInterface* extif = NULL;
    PVMFBasicErrorInfoMessage* errormsg = NULL;
    if (aExtMsg)
    {
        extif = aExtMsg;
    }

    PVMFCmdResp resp(aCmd.iId, aCmd.iContext, aStatus, extif, aEventData);
    PVMFSessionId session = aCmd.iSession;

    /* Erase the command from the queue. */
    aCmdQ.Erase(&aCmd);

    /* Report completion to the session observer.*/
    ReportCmdCompleteEvent(session, resp);

    if (errormsg)
    {
        errormsg->removeRef();
    }
}

void PVMFQCPFFParserNode::CommandComplete(PVMFQCPFFNodeCmdQ& aCmdQ,
        PVMFQCPFFNodeCommand& aCmd,
        PVMFStatus aStatus,
        OsclAny* aEventData,
        PVUuid* aEventUUID,
        int32* aEventCode,
        PVInterface* aExtMsg)
{
    PVMF_QCPPARSERNODE_LOGSTACKTRACE((0, "PVMFQCPParserNode::CommandComplete() In Id %d Cmd %d Status %d Context %d Data %d",
                                      aCmd.iId, aCmd.iCmd, aStatus, aCmd.iContext, aEventData));

    PVInterface* extif = NULL;
    PVMFBasicErrorInfoMessage* errormsg = NULL;
    if (aExtMsg)
    {
        extif = aExtMsg;
    }
    else if (aEventUUID && aEventCode)
    {
        errormsg =
            OSCL_NEW(PVMFBasicErrorInfoMessage, (*aEventCode, *aEventUUID, NULL));
        extif = OSCL_STATIC_CAST(PVInterface*, errormsg);
    }

    PVMFCmdResp resp(aCmd.iId, aCmd.iContext, aStatus, extif, aEventData);
    PVMFSessionId session = aCmd.iSession;

    /* Erase the command from the queue. */
    if (!aCmdQ.empty())
    {
        aCmdQ.Erase(&aCmd);
    }

    /* Report completion to the session observer.*/
    ReportCmdCompleteEvent(session, resp);

    if (errormsg)
    {
        errormsg->removeRef();
    }
}

PVMFCommandId PVMFQCPFFParserNode::QueueCommandL(PVMFQCPFFNodeCommand& aCmd)
{
    if (IsAdded())
    {
        PVMFCommandId id;
        id = iInputCommands.AddL(aCmd);
        /* Wakeup the AO */
        RunIfNotReady();
        return id;
    }
    OSCL_LEAVE(OsclErrInvalidState);
    return -1;
}

void PVMFQCPFFParserNode::addRef()
{
    ++iExtensionRefCount;
}

void PVMFQCPFFParserNode::removeRef()
{
    --iExtensionRefCount;
}

PVMFStatus PVMFQCPFFParserNode::QueryInterfaceSync(PVMFSessionId aSession,
        const PVUuid& aUuid,
        PVInterface*& aInterfacePtr)
{
    OSCL_UNUSED_ARG(aSession);
    aInterfacePtr = NULL;
    if (queryInterface(aUuid, aInterfacePtr))
    {
        aInterfacePtr->addRef();
        return PVMFSuccess;
    }
    return PVMFErrNotSupported;
}

bool PVMFQCPFFParserNode::queryInterface(const PVUuid& uuid, PVInterface*& iface)
{
    if (uuid == PVMF_DATA_SOURCE_INIT_INTERFACE_UUID)
    {
        PVMFDataSourceInitializationExtensionInterface* myInterface = OSCL_STATIC_CAST(PVMFDataSourceInitializationExtensionInterface*, this);
        iface = OSCL_STATIC_CAST(PVInterface*, myInterface);
    }
    else if (uuid == PVMF_TRACK_SELECTION_INTERFACE_UUID)
    {
        PVMFTrackSelectionExtensionInterface* myInterface = OSCL_STATIC_CAST(PVMFTrackSelectionExtensionInterface*, this);
        iface = OSCL_STATIC_CAST(PVInterface*, myInterface);
    }
    else if (uuid == KPVMFMetadataExtensionUuid)
    {
        PVMFMetadataExtensionInterface* myInterface = OSCL_STATIC_CAST(PVMFMetadataExtensionInterface*, this);
        iface = OSCL_STATIC_CAST(PVInterface*, myInterface);
    }
    else if (uuid == PvmfDataSourcePlaybackControlUuid)
    {
        PvmfDataSourcePlaybackControlInterface* myInterface = OSCL_STATIC_CAST(PvmfDataSourcePlaybackControlInterface*, this);
        iface = OSCL_STATIC_CAST(PVInterface*, myInterface);
    }
    else if (uuid == PVMIDatastreamuserInterfaceUuid)
    {
        PVMIDatastreamuserInterface* myInterface = OSCL_STATIC_CAST(PVMIDatastreamuserInterface*, this);
        iface = OSCL_STATIC_CAST(PVInterface*, myInterface);
    }
    else if (uuid == PVMF_FF_PROGDOWNLOAD_SUPPORT_INTERFACE_UUID)
    {
        PVMFFormatProgDownloadSupportInterface* myInterface = OSCL_STATIC_CAST(PVMFFormatProgDownloadSupportInterface*, this);
        iface = OSCL_STATIC_CAST(PVInterface*, myInterface);
    }
    else if (uuid == PVMFCPMPluginLicenseInterfaceUuid)
    {
        PVMFCPMPluginLicenseInterface* myInterface = OSCL_STATIC_CAST(PVMFCPMPluginLicenseInterface*, this);
        iface = OSCL_STATIC_CAST(PVInterface*, myInterface);
    }
    else
    {
        return false;
    }
    return true;
}


PVMFStatus PVMFQCPFFParserNode::SetSourceInitializationData(OSCL_wString& aSourceURL, PVMFFormatType& aSourceFormat, OsclAny* aSourceData)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFQCPFFParserNode::SetSourceInitializationData() called"));
    if (aSourceFormat == PVMF_MIME_QCPFF)
    {
        /* Clean up any previous sources */
        CleanupFileSource();

        iSourceFormat = aSourceFormat;
        iSourceURL = aSourceURL;
        if (aSourceData)
        {
            // Old context object? query for local datasource availability
            PVInterface* pvInterface =
                OSCL_STATIC_CAST(PVInterface*, aSourceData);

            PVInterface* localDataSrc = NULL;
            PVUuid localDataSrcUuid(PVMF_LOCAL_DATASOURCE_UUID);

            if (pvInterface->queryInterface(localDataSrcUuid, localDataSrc))
            {
                PVMFLocalDataSource* context =
                    OSCL_STATIC_CAST(PVMFLocalDataSource*, localDataSrc);

                iPreviewMode = context->iPreviewMode;
                if (context->iFileHandle)
                {

                    PVMF_QCP_PARSER_NODE_NEW(NULL,
                                             OsclFileHandle,
                                             (*(context->iFileHandle)),
                                             iFileHandle);

                    iCPMSourceData.iFileHandle = iFileHandle;
                }
                iCPMSourceData.iPreviewMode = iPreviewMode;
                iCPMSourceData.iIntent = context->iIntent;

            }
            else
            {
                // New context object ?
                PVInterface* sourceDataContext = NULL;
                PVInterface* commonDataContext = NULL;
                PVUuid sourceContextUuid(PVMF_SOURCE_CONTEXT_DATA_UUID);
                PVUuid commonContextUuid(PVMF_SOURCE_CONTEXT_DATA_COMMON_UUID);
                if (pvInterface->queryInterface(sourceContextUuid, sourceDataContext) &&
                        sourceDataContext->queryInterface(commonContextUuid, commonDataContext))
                {
                    PVMFSourceContextDataCommon* context =
                        OSCL_STATIC_CAST(PVMFSourceContextDataCommon*, commonDataContext);

                    iPreviewMode = context->iPreviewMode;
                    if (context->iFileHandle)
                    {

                        PVMF_QCP_PARSER_NODE_NEW(NULL,
                                                 OsclFileHandle,
                                                 (*(context->iFileHandle)),
                                                 iFileHandle);

                        iCPMSourceData.iFileHandle = iFileHandle;
                    }
                    iCPMSourceData.iPreviewMode = iPreviewMode;
                    iCPMSourceData.iIntent = context->iIntent;
                }
            }
        }
        /*
         * create a CPM object here...
         */
        iUseCPMPluginRegistry = true;
        {
            //cleanup any prior instance
            if (iCPM)
            {
                iCPM->ThreadLogoff();
                PVMFCPMFactory::DestroyContentPolicyManager(iCPM);
                iCPM = NULL;
            }
            iCPM = PVMFCPMFactory::CreateContentPolicyManager(*this);
            //thread logon may leave if there are no plugins
            int32 err;
            OSCL_TRY(err, iCPM->ThreadLogon(););
            OSCL_FIRST_CATCH_ANY(err,
                                 iCPM->ThreadLogoff();
                                 PVMFCPMFactory::DestroyContentPolicyManager(iCPM);
                                 iCPM = NULL;
                                 iUseCPMPluginRegistry = false;
                                );
        }
        return PVMFSuccess;
    }
    PVMF_QCPPARSERNODE_LOGERROR((0, "PVMFQCPParserNode::SetSourceInitializationData - Unsupported Format"));
    return PVMFFailure;
}

PVMFStatus PVMFQCPFFParserNode::SetClientPlayBackClock(PVMFMediaClock* aClientClock)
{
    OSCL_UNUSED_ARG(aClientClock);
    return PVMFSuccess;
}

PVMFStatus PVMFQCPFFParserNode::SetEstimatedServerClock(PVMFMediaClock* aClientClock)
{
    OSCL_UNUSED_ARG(aClientClock);
    return PVMFSuccess;
}

PVMFStatus PVMFQCPFFParserNode::GetMediaPresentationInfo(PVMFMediaPresentationInfo& aInfo)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFQCPFFParserNode::GetMediaPresentationInfo() called"));

    if (!iQCPParser)
    {
        return PVMFFailure;
    }

    aInfo.setDurationValue(iQCPFileInfo.iDuration);
    // Current version of AAC parser is limited to 1 channel
    PVMFTrackInfo tmpTrackInfo;
    tmpTrackInfo.setPortTag(PVMF_QCPFFPARSER_NODE_PORT_TYPE_SOURCE);
    tmpTrackInfo.setTrackID(0);
    TPVQcpFileInfo qcpinfo;
    if (!iQCPParser->RetrieveFileInfo(qcpinfo)) return PVMFErrNotSupported;

    switch (qcpinfo.iQcpFormat)
    {
            // Supported formats
        case QCP_FORMAT_QCELP: // QCELP
        case QCP_FORMAT_EVRC: // EVRC
            break;

            // Everything else is not supported
        case QCP_FORMAT_UNKNOWN:
        default:
            return PVMFErrNotSupported;
    }
    tmpTrackInfo.setTrackBitRate(qcpinfo.iBitrate);
    tmpTrackInfo.setTrackDurationTimeScale((uint64)qcpinfo.iTimescale);
    tmpTrackInfo.setTrackDurationValue(qcpinfo.iDuration);
    OSCL_FastString mime_type = _STRLIT_CHAR(PVMF_MIME_QCPFF);
    if (qcpinfo.iQcpFormat == QCP_FORMAT_QCELP)
    {
        mime_type = _STRLIT_CHAR(PVMF_MIME_QCELP);
    }
    else if (QCP_FORMAT_EVRC == qcpinfo.iQcpFormat)
        mime_type = _STRLIT_CHAR(PVMF_MIME_EVRC);


    tmpTrackInfo.setTrackMimeType(mime_type);
    aInfo.addTrackInfo(tmpTrackInfo);
    return PVMFSuccess;
}


PVMFStatus PVMFQCPFFParserNode::SelectTracks(PVMFMediaPresentationInfo& aInfo)
{
    OSCL_UNUSED_ARG(aInfo);
    return PVMFSuccess;
}

uint32 PVMFQCPFFParserNode::GetNumMetadataKeys(char* aQueryKeyString)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFQCPFFParserNode::GetNumMetadataKeys() called"));

    uint32 num_entries = 0;

    if (aQueryKeyString == NULL)
    {
        // No query key so just return all the available keys
        num_entries = iAvailableMetadataKeys.size();
    }
    else
    {
        // Determine the number of metadata keys based on the query key string provided
        for (uint32 i = 0; i < iAvailableMetadataKeys.size(); i++)
        {
            // Check if the key matches the query key
            if (pv_mime_strcmp(iAvailableMetadataKeys[i].get_cstr(), aQueryKeyString) >= 0)
            {
                num_entries++;
            }
        }
    }
    for (uint32 i = 0; i < iCPMMetadataKeys.size(); i++)
    {
        if (pv_mime_strcmp(iCPMMetadataKeys[i].get_cstr(),
                           aQueryKeyString) >= 0)
        {
            num_entries++;
        }
    }

    if ((iCPMMetaDataExtensionInterface != NULL))
    {
        num_entries +=
            iCPMMetaDataExtensionInterface->GetNumMetadataKeys(aQueryKeyString);
    }
    return num_entries;
}

uint32 PVMFQCPFFParserNode::GetNumMetadataValues(PVMFMetadataList& aKeyList)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFQCPFFParserNode::GetNumMetadataValues() called"));

    uint32 numkeys = aKeyList.size();
    if (!iQCPParser || numkeys == 0)
    {
        return 0;
    }

    // Count the number of metadata value entries based on the key list provided
    uint32 numvalentries = 0;
    for (uint32 lcv = 0; lcv < numkeys; lcv++)
    {
        if (oscl_strcmp(aKeyList[lcv].get_cstr(), PVQCPMETADATA_DURATION_KEY) == 0 &&
                iQCPFileInfo.iDuration > 0)
        {
            // Movie Duration
            ++numvalentries;
        }
        else if (oscl_strcmp(aKeyList[lcv].get_cstr(), PVQCPMETADATA_NUMTRACKS_KEY) == 0)
        {
            // Number of tracks
            ++numvalentries;
        }
        else if ((oscl_strcmp(aKeyList[lcv].get_cstr(), PVQCPMETADATA_TRACKINFO_BITRATE_KEY) == 0) &&
                 iQCPFileInfo.iBitrate > 0)
        {
            // Bitrate
            ++numvalentries;
        }
        else if ((oscl_strcmp(aKeyList[lcv].get_cstr(), PVQCPMETADATA_TRACKINFO_AUDIO_FORMAT_KEY) == 0) &&
                 iQCPFileInfo.iQcpFormat != QCP_FORMAT_UNKNOWN)
        {
            // Format
            ++numvalentries;
        }
        else if (oscl_strcmp(aKeyList[lcv].get_cstr(), PVQCPMETADATA_RANDOM_ACCESS_DENIED_KEY) == 0)
        {
            /*
             * Random Access
             * Increment the counter for the number of values found so far
             */
            ++numvalentries;
        }
        else if (oscl_strcmp(aKeyList[lcv].get_cstr(), PVQCPMETADATA_CLIP_TYPE_KEY) == 0)
        {
            /*
             * clip-type
             * Increment the counter for the number of values found so far
             */
            ++numvalentries;
        }


    }

    if ((iCPMMetaDataExtensionInterface != NULL))
    {
        numvalentries +=
            iCPMMetaDataExtensionInterface->GetNumMetadataValues(aKeyList);
    }
    return numvalentries;
}

PVMFCommandId PVMFQCPFFParserNode::GetNodeMetadataKeys(PVMFSessionId aSessionId, PVMFMetadataList& aKeyList
        , uint32 aStartingKeyIndex, int32 aMaxKeyEntries, char* aQueryKeyString, const OsclAny* aContext)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFQCPFFParserNode::GetNodeMetadataKeys() called"));

    PVMFQCPFFNodeCommand cmd;
    cmd.PVMFQCPFFNodeCommand::Construct(aSessionId, PVMF_QCP_PARSER_NODE_GETNODEMETADATAKEYS, aKeyList, aStartingKeyIndex, aMaxKeyEntries, aQueryKeyString, aContext);
    return QueueCommandL(cmd);
}

PVMFCommandId PVMFQCPFFParserNode::GetNodeMetadataValues(PVMFSessionId aSessionId, PVMFMetadataList& aKeyList, Oscl_Vector<PvmiKvp, OsclMemAllocator>& aValueList
        , uint32 aStartingValueIndex, int32 aMaxValueEntries, const OsclAny* aContext)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFQCPFFParserNode::GetNodeMetadataValue() called"));

    PVMFQCPFFNodeCommand cmd;
    cmd.PVMFQCPFFNodeCommand::Construct(aSessionId, PVMF_QCP_PARSER_NODE_GETNODEMETADATAVALUES, aKeyList, aValueList, aStartingValueIndex, aMaxValueEntries, aContext);
    return QueueCommandL(cmd);
}

PVMFStatus PVMFQCPFFParserNode::ReleaseNodeMetadataKeys(PVMFMetadataList& , uint32 , uint32)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFQCPFFParserNode::ReleaseNodeMetadataKeys() called"));
    return PVMFSuccess;
}

PVMFStatus PVMFQCPFFParserNode::ReleaseNodeMetadataValues(Oscl_Vector<PvmiKvp, OsclMemAllocator>& aValueList, uint32 start, uint32 end)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFQCPFFParserNode::ReleaseNodeMetadataValues() called"));

    if (start > end || aValueList.size() == 0)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMFQCPFFParserNode::ReleaseNodeMetadataValues() Invalid start/end index"));
        return PVMFErrArgument;
    }

    end = OSCL_MIN(aValueList.size(), iQCPParserNodeMetadataValueCount);

    for (uint32 i = start; i < end; i++)
    {
        if (aValueList[i].key != NULL)
        {
            switch (GetValTypeFromKeyString(aValueList[i].key))
            {
                case PVMI_KVPVALTYPE_CHARPTR:
                    if (aValueList[i].value.pChar_value != NULL)
                    {
                        OSCL_ARRAY_DELETE(aValueList[i].value.pChar_value);
                        aValueList[i].value.pChar_value = NULL;
                    }
                    break;

                case PVMI_KVPVALTYPE_UINT32:
                case PVMI_KVPVALTYPE_UINT8:
                    // No memory to free for these valtypes
                    break;

                default:
                    // Should not get a value that wasn't created from here
                    break;
            }

            OSCL_ARRAY_DELETE(aValueList[i].key);
            aValueList[i].key = NULL;
        }
    }

    return PVMFSuccess;
}

PVMFCommandId PVMFQCPFFParserNode::SetDataSourcePosition(PVMFSessionId aSessionId
        , PVMFTimestamp aTargetNPT
        , PVMFTimestamp& aActualNPT
        , PVMFTimestamp& aActualMediaDataTS
        , bool aSeekToSyncPoint
        , uint32 aStreamID
        , OsclAny* aContext)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "PVMFQCPFFParserNode::SetDataSourcePosition: aTargetNPT=%d, aSeekToSyncPoint=%d, aContext=0x%x",
                     aTargetNPT, aSeekToSyncPoint, aContext));

    PVMFQCPFFNodeCommand cmd;
    cmd.PVMFQCPFFNodeCommand::Construct(aSessionId, PVMF_QCP_PARSER_NODE_SET_DATASOURCE_POSITION, aTargetNPT, aActualNPT,
                                        aActualMediaDataTS, aSeekToSyncPoint, aStreamID, aContext);
    return QueueCommandL(cmd);
}

PVMFCommandId PVMFQCPFFParserNode::QueryDataSourcePosition(PVMFSessionId aSessionId
        , PVMFTimestamp aTargetNPT
        , PVMFTimestamp& aActualNPT
        , bool aSeekToSyncPoint
        , OsclAny* aContext)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "PVMFQCPFFParserNode::QueryDataSourcePosition: aTargetNPT=%d, aSeekToSyncPoint=%d, aContext=0x%x",
                     aTargetNPT, aSeekToSyncPoint, aContext));

    PVMFQCPFFNodeCommand cmd;
    cmd.PVMFQCPFFNodeCommand::Construct(aSessionId, PVMF_QCP_PARSER_NODE_QUERY_DATASOURCE_POSITION, aTargetNPT, aActualNPT,
                                        aSeekToSyncPoint, aContext);
    return QueueCommandL(cmd);
}

PVMFCommandId PVMFQCPFFParserNode::QueryDataSourcePosition(PVMFSessionId aSessionId
        , PVMFTimestamp aTargetNPT
        , PVMFTimestamp& aSeekPointBeforeTargetNPT
        , PVMFTimestamp& aSeekPointAfterTargetNPT
        , OsclAny* aContext
        , bool aSeekToSyncPoint)
{
    OSCL_UNUSED_ARG(aSeekPointAfterTargetNPT);
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE,
                    (0, "PVMFQCPFFParserNode::QueryDataSourcePosition: aTargetNPT=%d, aSeekToSyncPoint=%d, aContext=0x%x",
                     aTargetNPT, aSeekToSyncPoint, aContext));

    PVMFQCPFFNodeCommand cmd;
    // Construct not changed,aSeekPointBeforeTargetNPT has replace aActualtNPT
    cmd.PVMFQCPFFNodeCommand::Construct(aSessionId, PVMF_QCP_PARSER_NODE_QUERY_DATASOURCE_POSITION, aTargetNPT, aSeekPointBeforeTargetNPT,
                                        aSeekToSyncPoint, aContext);
    return QueueCommandL(cmd);
}

PVMFCommandId PVMFQCPFFParserNode::SetDataSourceRate(PVMFSessionId aSessionId
        , int32 aRate
        , PVMFTimebase* aTimebase
        , OsclAny* aContext)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFQCPFFParserNode::SetDataSourceRate() called"));

    PVMFQCPFFNodeCommand cmd;
    cmd.PVMFQCPFFNodeCommand::Construct(aSessionId, PVMF_QCP_PARSER_NODE_SET_DATASOURCE_RATE, aRate, aTimebase, aContext);
    return QueueCommandL(cmd);
}


PVMFStatus PVMFQCPFFParserNode::CheckForQCPHeaderAvailability()
{
    if (iDataStreamInterface != NULL)
    {
        /*
         * First check if we have minimum number of bytes to recognize
         * the file and determine the header size.
         */
        uint32 currCapacity = 0;
        iDataStreamInterface->QueryReadCapacity(iDataStreamSessionID,
                                                currCapacity);

        if (currCapacity <  QCP_MIN_DATA_SIZE_FOR_RECOGNITION)
        {
            iRequestReadCapacityNotificationID =
                iDataStreamInterface->RequestReadCapacityNotification(iDataStreamSessionID,
                        *this,
                        QCP_MIN_DATA_SIZE_FOR_RECOGNITION);
            return PVMFPending;
        }


        uint32 headerSize32 =
            Oscl_Int64_Utils::get_uint64_lower32(iQCPHeaderSize);

        if (currCapacity < headerSize32)
        {
            iRequestReadCapacityNotificationID =
                iDataStreamInterface->RequestReadCapacityNotification(iDataStreamSessionID,
                        *this,
                        headerSize32);
            return PVMFPending;
        }
    }
    return PVMFSuccess;
}


bool PVMFQCPFFParserNode::GetTrackInfo(PVMFPortInterface* aPort,
                                       PVQCPFFNodeTrackPortInfo*& aTrackInfoPtr)
{
    Oscl_Vector<PVQCPFFNodeTrackPortInfo, PVMFQCPParserNodeAllocator>::iterator it;
    for (it = iSelectedTrackList.begin(); it != iSelectedTrackList.end(); it++)
    {
        if (it->iPort == aPort)
        {
            aTrackInfoPtr = it;
            return true;
        }
    }
    return false;
}

bool PVMFQCPFFParserNode::GetTrackInfo(int32 aTrackID,
                                       PVQCPFFNodeTrackPortInfo*& aTrackInfoPtr)
{
    Oscl_Vector<PVQCPFFNodeTrackPortInfo, PVMFQCPParserNodeAllocator>::iterator it;
    for (it = iSelectedTrackList.begin(); it != iSelectedTrackList.end(); it++)
    {
        if (it->iTrackId == aTrackID)
        {
            aTrackInfoPtr = it;
            return true;
        }
    }
    return false;
}


bool PVMFQCPFFParserNode::ProcessPortActivity(PVQCPFFNodeTrackPortInfo* aTrackInfoPtr)
{
    /*
     * called by the AO to process a port activity message
     */
    PVMF_QCPPARSERNODE_LOGSTACKTRACE((0, "PVMFQCPParserNode::ProcessPortActivity() Called"));

    PVMFStatus status;
    if (aTrackInfoPtr->oQueueOutgoingMessages)
    {
        status = QueueMediaSample(aTrackInfoPtr);

        if ((status != PVMFErrBusy) &&
                (status != PVMFSuccess) &&
                (status != PVMFErrInvalidState))
        {
            PVMF_QCPPARSERNODE_LOGERROR((0, "PVMFQCPParserNode::ProcessPortActivity() QueueMediaSample Failed - Err=%d", status));
            return false;
        }
        if (iAutoPaused == true)
        {
            aTrackInfoPtr->oQueueOutgoingMessages = false;
            PVMF_QCPPARSERNODE_LOGDATATRAFFIC((0, "PVMFQCPParserNode::QueueMediaSample() - Auto Paused"));
            return PVMFErrBusy;
        }
        if (aTrackInfoPtr->iPort->IsOutgoingQueueBusy())
        {
            aTrackInfoPtr->oQueueOutgoingMessages = false;
            PVMF_QCPPARSERNODE_LOGDATATRAFFIC((0, "PVMFQCPParserNode::QueueMediaSample() Port Outgoing Queue Busy"));
            return PVMFErrBusy;
        }

    }
    if (aTrackInfoPtr->oProcessOutgoingMessages)
    {
        if (aTrackInfoPtr->iPort->OutgoingMsgQueueSize() > 0)
        {
            status = ProcessOutgoingMsg(aTrackInfoPtr);
            /*
             * Report any unexpected failure in port processing...
             * (the InvalidState error happens when port input is suspended,
             * so don't report it.)
             */
            if ((status != PVMFErrBusy) &&
                    (status != PVMFSuccess) &&
                    (status != PVMFErrInvalidState))
            {
                PVMF_QCPPARSERNODE_LOGERROR((0, "PVMFASFParserNode::ProcessPortActivity() ProcessOutgoingMsg Failed - Err=%d", status));
                ReportErrorEvent(PVMFErrPortProcessing);
            }
        }
        else
        {
            /* Nothing to send - wait for more data */
            aTrackInfoPtr->oProcessOutgoingMessages = false;
        }
    }
    return true;
}

bool PVMFQCPFFParserNode::CheckForPortRescheduling()
{
    PVQCPFFNodeTrackPortInfo* trackInfoPtr = NULL;
    if (!GetTrackInfo(iOutPort, trackInfoPtr))
    {
        PVMF_QCPPARSERNODE_LOGERROR((0, "PVMFQCPParserNode::CheckForPortRescheduling: Error - GetPortContainer failed"));
        return false;
    }

    if ((trackInfoPtr->oProcessOutgoingMessages) ||
            (trackInfoPtr->oQueueOutgoingMessages))
    {
        /*
         * Found a port that has outstanding activity and
         * is not busy.
         */
        return true;
    }
    /*
     * No port processing needed - either all port activity queues are empty
     * or the ports are backed up due to flow control.
     */
    return false;
}

PVMFStatus PVMFQCPFFParserNode::InitMetaData()
{
    if (iQCPFileInfo.iFileSize > 0)
    {
        // Populate the metadata key vector based on info available
        PushToAvailableMetadataKeysList(PVQCPMETADATA_NUMTRACKS_KEY);
        if (iQCPFileInfo.iDuration > 0)
        {
            PushToAvailableMetadataKeysList(PVQCPMETADATA_DURATION_KEY);

        }
        if (iQCPFileInfo.iBitrate > 0)
        {
            PushToAvailableMetadataKeysList(PVQCPMETADATA_TRACKINFO_BITRATE_KEY);

        }
        if (iQCPFileInfo.iQcpFormat != QCP_FORMAT_UNKNOWN)
        {
            PushToAvailableMetadataKeysList(PVQCPMETADATA_TRACKINFO_AUDIO_FORMAT_KEY);

        }
        PushToAvailableMetadataKeysList(PVQCPMETADATA_RANDOM_ACCESS_DENIED_KEY);
        PushToAvailableMetadataKeysList(PVQCPMETADATA_CLIP_TYPE_KEY);

        //set clip duration on download progress interface
        //applicable to PDL sessions
        {
            if ((iDownloadProgressInterface != NULL) && (iQCPFileInfo.iDuration != 0))
            {
                iDownloadProgressInterface->setClipDuration(OSCL_CONST_CAST(uint32, iQCPFileInfo.iDuration));
            }
        }

        return PVMFSuccess;
    }
    else
        return PVMFFailure;

}

void PVMFQCPFFParserNode::PushToAvailableMetadataKeysList(const char* aKeystr, char* aOptionalParam)
{
    if (aKeystr == NULL)
    {
        return;
    }
    int32 leavecode = 0;
    if (aOptionalParam)
    {
        OSCL_TRY(leavecode, iAvailableMetadataKeys.push_front(aKeystr);
                 iAvailableMetadataKeys[0] += aOptionalParam;);
    }
    else
    {
        OSCL_TRY(leavecode, iAvailableMetadataKeys.push_front(aKeystr));
    }
}

void PVMFQCPFFParserNode::InitCPM()
{
    iCPMInitCmdId = iCPM->Init();
}

void PVMFQCPFFParserNode::OpenCPMSession()
{
    iCPMOpenSessionCmdId = iCPM->OpenSession(iCPMSessionID);
}

void PVMFQCPFFParserNode::CPMRegisterContent()
{
    iCPMRegisterContentCmdId = iCPM->RegisterContent(iCPMSessionID,
                               iSourceURL,
                               iSourceFormat,
                               (OsclAny*) & iCPMSourceData);
}

void PVMFQCPFFParserNode::GetCPMLicenseInterface()
{
    iCPMLicenseInterfacePVI = NULL;
    iCPMGetLicenseInterfaceCmdId =
        iCPM->QueryInterface(iCPMSessionID,
                             PVMFCPMPluginLicenseInterfaceUuid,
                             iCPMLicenseInterfacePVI);
}

bool PVMFQCPFFParserNode::GetCPMContentAccessFactory()
{
    PVMFStatus status = iCPM->GetContentAccessFactory(iCPMSessionID,
                        iCPMContentAccessFactory);
    if (status != PVMFSuccess)
    {
        return false;
    }
    return true;
}

bool PVMFQCPFFParserNode::GetCPMMetaDataExtensionInterface()
{
    PVInterface* temp = NULL;
    bool retVal =
        iCPM->queryInterface(KPVMFMetadataExtensionUuid, temp);
    iCPMMetaDataExtensionInterface = OSCL_STATIC_CAST(PVMFMetadataExtensionInterface*, temp);
    return retVal;
}

void PVMFQCPFFParserNode::RequestUsage()
{
    PopulateDRMInfo();

    if (iDataStreamReadCapacityObserver != NULL)
    {
        iCPMContentAccessFactory->SetStreamReadCapacityObserver(iDataStreamReadCapacityObserver);
    }

    iCPMRequestUsageId = iCPM->ApproveUsage(iCPMSessionID,
                                            iRequestedUsage,
                                            iApprovedUsage,
                                            iAuthorizationDataKvp,
                                            iUsageID,
                                            iCPMContentAccessFactory);

    oSourceIsCurrent = true;
}

void PVMFQCPFFParserNode::PopulateDRMInfo()
{
    if (iRequestedUsage.key)
    {
        OSCL_ARRAY_DELETE(iRequestedUsage.key);
        iRequestedUsage.key = NULL;
    }

    if (iApprovedUsage.key)
    {
        OSCL_ARRAY_DELETE(iApprovedUsage.key);
        iApprovedUsage.key = NULL;
    }

    if (iAuthorizationDataKvp.key)
    {
        OSCL_ARRAY_DELETE(iAuthorizationDataKvp.key);
        iAuthorizationDataKvp.key = NULL;
    }

    if ((iCPMContentType == PVMF_CPM_FORMAT_OMA1) ||
            (iCPMContentType == PVMF_CPM_FORMAT_AUTHORIZE_BEFORE_ACCESS))
    {
        int32 UseKeyLen = oscl_strlen(_STRLIT_CHAR(PVMF_CPM_REQUEST_USE_KEY_STRING));
        int32 AuthKeyLen = oscl_strlen(_STRLIT_CHAR(PVMF_CPM_AUTHORIZATION_DATA_KEY_STRING));
        int32 leavecode = 0;

        OSCL_TRY(leavecode,
                 iRequestedUsage.key = OSCL_ARRAY_NEW(char, UseKeyLen + 1);
                 iApprovedUsage.key = OSCL_ARRAY_NEW(char, UseKeyLen + 1);
                 iAuthorizationDataKvp.key = OSCL_ARRAY_NEW(char, AuthKeyLen + 1);
                );
        if (leavecode || !iRequestedUsage.key || !iApprovedUsage.key || !iAuthorizationDataKvp.key)
        {
            if (iRequestedUsage.key)
            {
                OSCL_ARRAY_DELETE(iRequestedUsage.key);
                iRequestedUsage.key = NULL;
            }
            if (iApprovedUsage.key)
            {
                OSCL_ARRAY_DELETE(iApprovedUsage.key);
                iApprovedUsage.key = NULL;
            }
            if (iAuthorizationDataKvp.key)
            {
                OSCL_ARRAY_DELETE(iAuthorizationDataKvp.key);
                iAuthorizationDataKvp.key = NULL;
            }

            return;
        }

        oscl_strncpy(iRequestedUsage.key,
                     _STRLIT_CHAR(PVMF_CPM_REQUEST_USE_KEY_STRING),
                     UseKeyLen);
        iRequestedUsage.key[UseKeyLen] = 0;
        iRequestedUsage.length = 0;
        iRequestedUsage.capacity = 0;
        if (iPreviewMode)
        {
            iRequestedUsage.value.uint32_value =
                (BITMASK_PVMF_CPM_DRM_INTENT_PREVIEW |
                 BITMASK_PVMF_CPM_DRM_INTENT_PAUSE |
                 BITMASK_PVMF_CPM_DRM_INTENT_SEEK_FORWARD |
                 BITMASK_PVMF_CPM_DRM_INTENT_SEEK_BACK);
        }
        else
        {
            iRequestedUsage.value.uint32_value =
                (BITMASK_PVMF_CPM_DRM_INTENT_PLAY |
                 BITMASK_PVMF_CPM_DRM_INTENT_PAUSE |
                 BITMASK_PVMF_CPM_DRM_INTENT_SEEK_FORWARD |
                 BITMASK_PVMF_CPM_DRM_INTENT_SEEK_BACK);
        }
        oscl_strncpy(iApprovedUsage.key,
                     _STRLIT_CHAR(PVMF_CPM_REQUEST_USE_KEY_STRING),
                     UseKeyLen);
        iApprovedUsage.key[UseKeyLen] = 0;
        iApprovedUsage.length = 0;
        iApprovedUsage.capacity = 0;
        iApprovedUsage.value.uint32_value = 0;

        oscl_strncpy(iAuthorizationDataKvp.key,
                     _STRLIT_CHAR(PVMF_CPM_AUTHORIZATION_DATA_KEY_STRING),
                     AuthKeyLen);
        iAuthorizationDataKvp.key[AuthKeyLen] = 0;
        iAuthorizationDataKvp.length = 0;
        iAuthorizationDataKvp.capacity = 0;
        iAuthorizationDataKvp.value.pUint8_value = NULL;
    }
    else
    {
        //Error
        OSCL_ASSERT(false);
    }
}

void PVMFQCPFFParserNode::SendUsageComplete()
{
    iCPMUsageCompleteCmdId = iCPM->UsageComplete(iCPMSessionID, iUsageID);
}

void PVMFQCPFFParserNode::CloseCPMSession()
{
    iCPMCloseSessionCmdId = iCPM->CloseSession(iCPMSessionID);
}

void PVMFQCPFFParserNode::ResetCPM()
{
    iCPMResetCmdId = iCPM->Reset();
}

void PVMFQCPFFParserNode::GetCPMMetaDataKeys()
{
    if (iCPMMetaDataExtensionInterface != NULL)
    {
        iCPMMetadataKeys.clear();
        iCPMGetMetaDataKeysCmdId =
            iCPMMetaDataExtensionInterface->GetNodeMetadataKeys(iCPMSessionID,
                    iCPMMetadataKeys,
                    0,
                    PVMF_QCP_PARSER_NODE_MAX_CPM_METADATA_KEYS);
    }
}

void PVMFQCPFFParserNode::CPMCommandCompleted(const PVMFCmdResp& aResponse)
{
    PVMFCommandId id = aResponse.GetCmdId();
    PVMFStatus status = aResponse.GetCmdStatus();
    if (id == iCPMCancelGetLicenseCmdId)
    {
        /*
         * if this command is CancelGetLicense, we will return success or fail here.
         */
        PVMF_QCPPARSERNODE_LOGDATATRAFFIC((0, "PVMFQCPFFParserNode::CPMCommandCompleted -  CPM CancelGetLicense complete"));
        OSCL_ASSERT(!iCancelCommand.empty());
        CommandComplete(iCancelCommand,
                        iCancelCommand.front(),
                        status);
        return;
    }
    //if CPM comes back as PVMFErrNotSupported then by pass rest of the CPM
    //sequence. Fake success here so that node doesnt treat this as an error
    else if (id == iCPMRegisterContentCmdId && status == PVMFErrNotSupported)
    {
        /* Unsupported format - Treat it like unprotected content */
        PVMF_QCPPARSERNODE_LOGINFO((0, "PVMFQCPParserNode::CPMCommandCompleted - Unknown CPM Format - Ignoring CPM"));
        if (CheckForQCPHeaderAvailability() == PVMFSuccess)
        {
            if (ParseQCPFile())
            {
                /* End of Node Init sequence. */
                OSCL_ASSERT(!iCurrentCommand.empty());
                OSCL_ASSERT(iCurrentCommand.front().iCmd == PVMF_QCP_PARSER_NODE_INIT);
                CompleteInit();
            }
        }
        return;
    }

    if (status != PVMFSuccess)
    {
        /*
         * If any command fails, the sequence fails.
         */
        CommandComplete(iCurrentCommand,
                        iCurrentCommand.front(),
                        aResponse.GetCmdStatus(),
                        NULL,
                        NULL,
                        NULL,
                        aResponse.GetEventExtensionInterface());
    }
    else
    {
        /*
         * process the response, and issue the next command in
         * the sequence.
         */

        if (id == iCPMInitCmdId)
        {
            OpenCPMSession();
        }
        else if (id == iCPMOpenSessionCmdId)
        {
            CPMRegisterContent();
        }
        else if (id == iCPMRegisterContentCmdId)
        {
            GetCPMLicenseInterface();
        }
        else if (id == iCPMGetLicenseInterfaceCmdId)
        {
            iCPMLicenseInterface = OSCL_STATIC_CAST(PVMFCPMPluginLicenseInterface*, iCPMLicenseInterfacePVI);
            iCPMLicenseInterfacePVI = NULL;
            iCPMContentType = iCPM->GetCPMContentType(iCPMSessionID);

            if ((iCPMContentType == PVMF_CPM_FORMAT_AUTHORIZE_BEFORE_ACCESS)
                    || (iCPMContentType == PVMF_CPM_FORMAT_OMA1))
            {
                GetCPMContentAccessFactory();
                GetCPMMetaDataExtensionInterface();
                if (CheckForQCPHeaderAvailability() == PVMFSuccess)
                {
                    if (ParseQCPFile())
                    {
                        RequestUsage();
                    }
                }
            }
            else
            {
                /* Unsupported format - Treat it like unprotected content */
                PVMF_QCPPARSERNODE_LOGINFO((0, "PVMFQCPParserNode::CPMCommandCompleted - Unknown CPM Format - Ignoring CPM"));
                if (CheckForQCPHeaderAvailability() == PVMFSuccess)
                {
                    if (ParseQCPFile())
                    {
                        OSCL_ASSERT(!iCurrentCommand.empty());
                        OSCL_ASSERT(iCurrentCommand.front().iCmd == PVMF_QCP_PARSER_NODE_INIT);
                        CompleteInit();
                        /*
                         * if there was any pending cancel, it was waiting on
                         * this command to complete-- so the cancel is now done.
                         */
                        if (!iCancelCommand.empty())
                        {
                            CommandComplete(iCancelCommand,
                                            iCancelCommand.front(),
                                            PVMFSuccess);
                        }
                    }
                }
            }
        }
        else if (id == iCPMRequestUsageId)
        {
            oSourceIsCurrent = false;
            if (aResponse.GetCmdStatus() == PVMFSuccess)
            {
                if (CheckForQCPHeaderAvailability() == PVMFSuccess)
                {
                    if (ParseQCPFile())
                    {
                        OSCL_ASSERT(!iCurrentCommand.empty());
                        OSCL_ASSERT(iCurrentCommand.front().iCmd == PVMF_QCP_PARSER_NODE_INIT);
                        CompleteInit();
                        /*
                         * if there was any pending cancel, it was waiting on
                         * this command to complete-- so the cancel is now done.
                         */
                        if (!iCancelCommand.empty())
                        {
                            CommandComplete(iCancelCommand,
                                            iCancelCommand.front(),
                                            PVMFSuccess);
                        }
                    }
                }
            }
            else
            {
                CompleteInit();
            }
        }
        else if (id == iCPMGetMetaDataKeysCmdId)
        {
            /* End of GetNodeMetaDataKeys */
            PVMFStatus status =
                CompleteGetMetadataKeys(iCurrentCommand.front());
            CommandComplete(iCurrentCommand,
                            iCurrentCommand.front(),
                            status,
                            NULL,
                            NULL,
                            NULL,
                            NULL);
        }
        else if (id == iCPMUsageCompleteCmdId)
        {
            CloseCPMSession();
        }
        else if (id == iCPMCloseSessionCmdId)
        {
            ResetCPM();
        }
        else if (id == iCPMResetCmdId)
        {
            /* End of Node Reset sequence */
            OSCL_ASSERT(!iCurrentCommand.empty());
            OSCL_ASSERT(iCurrentCommand.front().iCmd == PVMF_QCP_PARSER_NODE_RESET);
            CompleteReset();
        }
        else if (id == iCPMGetMetaDataValuesCmdId)
        {
            /* End of GetNodeMetaDataValues */
            OSCL_ASSERT(!iCurrentCommand.empty());
            OSCL_ASSERT(iCurrentCommand.front().iCmd == PVMF_QCP_PARSER_NODE_GETNODEMETADATAVALUES);
            CompleteGetMetaDataValues();
        }
        else if (id == iCPMGetLicenseCmdId)
        {
            CompleteGetLicense();
        }
        else
        {
            /* Unknown cmd - error */
            CommandComplete(iCurrentCommand,
                            iCurrentCommand.front(),
                            PVMFFailure);
        }
    }

    /*
     * if there was any pending cancel, it was waiting on
     * this command to complete-- so the cancel is now done.
     */
    if (!iCancelCommand.empty())
    {
        if (iCancelCommand.front().iCmd != PVMF_QCP_PARSER_NODE_CMD_CANCEL_GET_LICENSE)
        {
            CommandComplete(iCancelCommand,
                            iCancelCommand.front(),
                            PVMFSuccess);
        }
    }
}

void PVMFQCPFFParserNode::PassDatastreamFactory(PVMFDataStreamFactory& aFactory,
        int32 aFactoryTag,
        const PvmfMimeString* aFactoryConfig)
{
    OSCL_UNUSED_ARG(aFactoryTag);
    OSCL_UNUSED_ARG(aFactoryConfig);

    iDataStreamFactory = &aFactory;
    PVUuid uuid = PVMIDataStreamSyncInterfaceUuid;
    PVInterface* iFace =
        iDataStreamFactory->CreatePVMFCPMPluginAccessInterface(uuid);
    if (iFace != NULL)
    {
        iDataStreamInterface = OSCL_STATIC_CAST(PVMIDataStreamSyncInterface*, iFace);
        iDataStreamInterface->OpenSession(iDataStreamSessionID, PVDS_READ_ONLY);
    }
}

void
PVMFQCPFFParserNode::PassDatastreamReadCapacityObserver(PVMFDataStreamReadCapacityObserver* aObserver)
{
    iDataStreamReadCapacityObserver = aObserver;
}

void PVMFQCPFFParserNode::CompleteInit()
{
    PVMF_QCPPARSERNODE_LOGSTACKTRACE((0, "PVMFQCPParserNode::CompleteInit() Called"));
    if (iCPM)
    {

        if ((iCPMContentType == PVMF_CPM_FORMAT_OMA1) ||
                (iCPMContentType == PVMF_CPM_FORMAT_AUTHORIZE_BEFORE_ACCESS))
        {
            if (iApprovedUsage.value.uint32_value !=
                    iRequestedUsage.value.uint32_value)
            {
                CommandComplete(iCurrentCommand,
                                iCurrentCommand.front(),
                                PVMFErrAccessDenied,
                                NULL, NULL, NULL);
                return;
            }
        }
    }
    SetState(EPVMFNodeInitialized);
    CommandComplete(iCurrentCommand,
                    iCurrentCommand.front(),
                    PVMFSuccess);
    return;
}

void PVMFQCPFFParserNode::CompleteGetMetaDataValues()
{
    CommandComplete(iCurrentCommand,
                    iCurrentCommand.front(),
                    PVMFSuccess);
}

void PVMFQCPFFParserNode::setFileSize(const uint32 aFileSize)
{
    iDownloadFileSize = aFileSize;
}

int32 PVMFQCPFFParserNode::convertSizeToTime(uint32 aFileSize, uint32& aNPTInMS)
{
    OSCL_UNUSED_ARG(aFileSize);
    OSCL_UNUSED_ARG(aNPTInMS);
    return -1;
}

void PVMFQCPFFParserNode::setDownloadProgressInterface(PVMFDownloadProgressInterface* aInterface)
{
    if (aInterface == NULL)
    {
        OSCL_ASSERT(false);
    }
    iDownloadProgressInterface = aInterface;
}

void PVMFQCPFFParserNode::DataStreamInformationalEvent(const PVMFAsyncEvent& aEvent)
{
    OSCL_UNUSED_ARG(aEvent);
    OSCL_LEAVE(OsclErrNotSupported);
}

void PVMFQCPFFParserNode::DataStreamErrorEvent(const PVMFAsyncEvent& aEvent)
{
    OSCL_UNUSED_ARG(aEvent);
    OSCL_LEAVE(OsclErrNotSupported);
}

void PVMFQCPFFParserNode::DataStreamCommandCompleted(const PVMFCmdResp& aResponse)
{
    if (aResponse.GetCmdId() == iRequestReadCapacityNotificationID)
    {
        PVMFStatus cmdStatus = aResponse.GetCmdStatus();
        if (cmdStatus == PVMFSuccess)
        {
            if (CheckForQCPHeaderAvailability() == PVMFSuccess)
            {
                if (iCPMContentType == PVMF_CPM_FORMAT_AUTHORIZE_BEFORE_ACCESS)
                {
                    if (ParseQCPFile())
                    {
                        {
                            /* End of Node Init sequence. */
                            OSCL_ASSERT(!iCurrentCommand.empty());
                            OSCL_ASSERT(iCurrentCommand.front().iCmd == PVMF_QCP_PARSER_NODE_INIT);
                            CompleteInit();
                        }
                    }
                }
                else
                {
                    if (ParseQCPFile())
                    {
                        /* End of Node Init sequence. */
                        OSCL_ASSERT(!iCurrentCommand.empty());
                        OSCL_ASSERT(iCurrentCommand.front().iCmd == PVMF_QCP_PARSER_NODE_INIT);
                        CompleteInit();
                    }
                }
            }
        }
        else
        {
            PVMF_QCPPARSERNODE_LOGERROR((0, "PVMFQCPParserNode::DataStreamCommandCompleted() Failed %d", cmdStatus));
            CommandComplete(iCurrentCommand,
                            iCurrentCommand.front(),
                            PVMFErrResource);

        }
    }
    else
    {
        OSCL_ASSERT(false);
    }
}

void PVMFQCPFFParserNode::playResumeNotification(bool aDownloadComplete)
{
    iAutoPaused = false;
    iDownloadComplete = aDownloadComplete;
    PVQCPFFNodeTrackPortInfo* trackInfoPtr = NULL;
    if (!GetTrackInfo(iOutPort, trackInfoPtr))
    {
        PVMF_QCPPARSERNODE_LOGERROR((0, "PVMFASFParserNode::playResumeNotification: Error - GetPortContainer failed"));
        return;
    }
    if (trackInfoPtr->oQueueOutgoingMessages == false)
    {
        trackInfoPtr->oQueueOutgoingMessages = true;
    }

    PVMF_QCPPARSERNODE_LOGERROR((0, "PVMFQCPParserNode::playResumeNotification() - Auto Resume Triggered - FileSize = %d, NPT = %d isDownloadComplete [%d]", iFileSizeLastConvertedToTime, iLastNPTCalcInConvertSizeToTime, iDownloadComplete));
    PVMF_QCPPARSERNODE_LOGDATATRAFFIC((0, "PVMFQCPParserNode::playResumeNotification() - Auto Resume Triggered - FileSize = %d, NPT = %d", iFileSizeLastConvertedToTime, iLastNPTCalcInConvertSizeToTime));
    RunIfNotReady();
}

void PVMFQCPFFParserNode::CompleteReset()
{
    PVMF_QCPPARSERNODE_LOGSTACKTRACE((0, "PVMFQCPParserNode::CompleteReset() Called"));
    /* stop and cleanup */
    ReleaseAllPorts();
    CleanupFileSource();
    SetState(EPVMFNodeIdle);
    CommandComplete(iCurrentCommand, iCurrentCommand.front(), PVMFSuccess);
    return;
}

PVMFStatus PVMFQCPFFParserNode::ThreadLogoff()
{
    PVMF_QCPPARSERNODE_LOGSTACKTRACE((0, "PVMFQCPParserNode::ThreadLogoff() Called"));
    if (iInterfaceState == EPVMFNodeIdle)
    {
        CleanupFileSource();
        iFileServer.Close();

        if (IsAdded())
        {
            RemoveFromScheduler();
        }
        iLogger = NULL;
        iDataPathLogger = NULL;
        iClockLogger = NULL;
        SetState(EPVMFNodeCreated);
        return PVMFSuccess;
    }
    PVMF_QCPPARSERNODE_LOGERROR((0, "PVMFQCPParserNode::ThreadLogoff() - Invalid State"));
    return PVMFErrInvalidState;
}

PVMFStatus PVMFQCPFFParserNode::QueueMediaSample(PVQCPFFNodeTrackPortInfo* aTrackInfoPtr)
{
    if (iAutoPaused == true)
    {
        aTrackInfoPtr->oQueueOutgoingMessages = false;
        PVMF_QCPPARSERNODE_LOGDATATRAFFIC((0, "PVMFQCPParserNode::QueueMediaSample() - Auto Paused"));
        return PVMFErrBusy;
    }
    if (aTrackInfoPtr->iPort->IsOutgoingQueueBusy())
    {
        aTrackInfoPtr->oQueueOutgoingMessages = false;
        PVMF_QCPPARSERNODE_LOGDATATRAFFIC((0, "PVMFQCPParserNode::QueueMediaSample() Port Outgoing Queue Busy"));
        return PVMFErrBusy;
    }
    if (aTrackInfoPtr->oQueueOutgoingMessages)
    {
        PVMFStatus status;
        if (aTrackInfoPtr->iSendBOS == true)
        {
            status = SendBeginOfMediaStreamCommand(aTrackInfoPtr);
            return status;
        }

        if (aTrackInfoPtr->oEOSReached == false)
        {
            PVMFSharedMediaDataPtr mediaDataOut;
            status = RetrieveMediaSample(aTrackInfoPtr, mediaDataOut);
            if (status == PVMFErrBusy)
            {
                PVMF_QCPPARSERNODE_LOGINFO((0, "PVMFQCPParserNode::QueueMediaSample() RetrieveMediaSample - Mem Pools Busy"));
                aTrackInfoPtr->oQueueOutgoingMessages = false;
                if (iAutoPaused == true)
                {
                    PauseAllMediaRetrieval();
                }
                return status;
            }
            else if (status == PVMFSuccess)
            {
                if (aTrackInfoPtr->oEOSReached == false)
                {
                    mediaDataOut->setStreamID(iStreamID);
                    PVMFSharedMediaMsgPtr msgOut;
                    convertToPVMFMediaMsg(msgOut, mediaDataOut);

                    /* For logging purposes */
                    uint32 markerInfo = mediaDataOut->getMarkerInfo();
                    uint32 noRender = 0;
                    uint32 keyFrameBit = 0;
                    if (markerInfo & PVMF_MEDIA_DATA_MARKER_INFO_NO_RENDER_BIT)
                    {
                        noRender = 1;
                    }
                    if (markerInfo & PVMF_MEDIA_DATA_MARKER_INFO_RANDOM_ACCESS_POINT_BIT)
                    {
                        keyFrameBit = 1;
                    }

                    status = aTrackInfoPtr->iPort->QueueOutgoingMsg(msgOut);

                    if (status != PVMFSuccess)
                    {
                        PVMF_QCPPARSERNODE_LOGERROR((0, "PVMFQCPParserNode::QueueMediaSample: Error - QueueOutgoingMsg failed"));
                        ReportErrorEvent(PVMFErrPortProcessing);
                    }
                    /* This flag will get reset to false if the connected port is busy */
                    aTrackInfoPtr->oProcessOutgoingMessages = true;
                    return status;
                }
            }
            else
            {
                PVMF_QCPPARSERNODE_LOGERROR((0, "PVMFQCPParserNode::QueueMediaSample() - Sample Retrieval Failed"));
                ReportErrorEvent(PVMFErrCorrupt);
                return PVMFFailure;
            }
        }
        else
        {
            status = GenerateAndSendEOSCommand(aTrackInfoPtr);
            return status;
        }
    }
    return PVMFSuccess;
}


PVMFStatus PVMFQCPFFParserNode::RetrieveMediaSample(PVQCPFFNodeTrackPortInfo* aTrackInfoPtr,
        PVMFSharedMediaDataPtr& aMediaDataOut)
{

    // Create a data buffer from pool
    int errcode = OsclErrNoResources;
    OsclSharedPtr<PVMFMediaDataImpl> mediaDataImplOut;
    mediaDataImplOut = aTrackInfoPtr->iResizableSimpleMediaMsgAlloc->allocate(MAXTRACKDATASIZE);

    if (mediaDataImplOut.GetRep() != NULL)
    {
        errcode = OsclErrNone;
    }

    OsclMemPoolResizableAllocatorObserver* resizableAllocObs =
        OSCL_STATIC_CAST(OsclMemPoolResizableAllocatorObserver*, aTrackInfoPtr);

    // Enable flag to receive event when next deallocate() is called on pool
    if (errcode != OsclErrNone)
    {
        aTrackInfoPtr->iResizableDataMemoryPool->notifyfreeblockavailable(*resizableAllocObs);
        return PVMFErrBusy;
    }

    // Now create a PVMF media data from pool
    errcode = OsclErrNoResources;
    aMediaDataOut = PVMFMediaData::createMediaData(mediaDataImplOut, aTrackInfoPtr->iMediaDataMemPool);

    if (aMediaDataOut.GetRep() != NULL)
    {
        errcode = OsclErrNone;
    }

    OsclMemPoolFixedChunkAllocatorObserver* fixedChunkObs =
        OSCL_STATIC_CAST(OsclMemPoolFixedChunkAllocatorObserver*, aTrackInfoPtr);

    // Enable flag to receive event when next deallocate() is called on pool
    if (errcode != OsclErrNone)
    {
        aTrackInfoPtr->iMediaDataMemPool->notifyfreechunkavailable(*fixedChunkObs);
        return PVMFErrBusy;
    }


    // Retrieve memory fragment to write to
    OsclRefCounterMemFrag refCtrMemFragOut;
    OsclMemoryFragment memFragOut;
    aMediaDataOut->getMediaFragment(0, refCtrMemFragOut);
    memFragOut.ptr = refCtrMemFragOut.getMemFrag().ptr;

    Oscl_Vector<uint32, OsclMemAllocator> payloadSizeVec;

    uint32 numsamples = NUM_QCP_FRAMES;
    // Set up the GAU structure
    GAU gau;
    gau.numMediaSamples = numsamples;
    gau.buf.num_fragments = 1;
    gau.buf.buf_states[0] = NULL;
    gau.buf.fragments[0].ptr = refCtrMemFragOut.getMemFrag().ptr;
    gau.buf.fragments[0].len = refCtrMemFragOut.getCapacity();

    int32 retval = iQCPParser->GetNextBundledAccessUnits(&numsamples, &gau);
    uint32 actualdatasize = 0;
    for (uint32 i = 0; i < numsamples; ++i)
    {
        actualdatasize += gau.info[i].len;
    }

    if (retval == QcpBitstreamObject::EVERYTHING_OK)
    {
        memFragOut.len = actualdatasize;

        // Set Actual size
        aMediaDataOut->setMediaFragFilledLen(0, actualdatasize);

        // Resize memory fragment
        aTrackInfoPtr->iResizableSimpleMediaMsgAlloc->ResizeMemoryFragment(mediaDataImplOut);


        // set current timestamp to media msg.
        aTrackInfoPtr->iClockConverter->update_clock(Oscl_Int64_Utils::get_uint64_lower32(aTrackInfoPtr->iContinuousTimeStamp));

        uint32 ts32 = Oscl_Int64_Utils::get_uint64_lower32(aTrackInfoPtr->iContinuousTimeStamp);

        aMediaDataOut->setSeqNum(aTrackInfoPtr->iSeqNum);
        aMediaDataOut->setTimestamp(ts32);

        // update ts by adding the data samples
        aTrackInfoPtr->iContinuousTimeStamp += numsamples * QCP_SAMPLE_DURATION; // i.e 20ms


        if (aTrackInfoPtr->iSeqNum == 0)
        {
            aMediaDataOut->setFormatSpecificInfo(aTrackInfoPtr->iFormatSpecificConfig);
        }
        aTrackInfoPtr->iSeqNum += 1;

        // Set M bit to 1 always - ASF FF only outputs complete frames
        uint32 markerInfo = 0;
        markerInfo |= PVMF_MEDIA_DATA_MARKER_INFO_M_BIT;

        // Reset random access point if first frame after repositioning
        if (aTrackInfoPtr->iFirstFrame)
        {
            markerInfo |= PVMF_MEDIA_DATA_MARKER_INFO_RANDOM_ACCESS_POINT_BIT;
            aTrackInfoPtr->iFirstFrame = false;
        }
        mediaDataImplOut->setMarkerInfo(markerInfo);
    }
    else if (retval == QcpBitstreamObject::DATA_INSUFFICIENT)
    {
        payloadSizeVec.clear();
        if (iDownloadProgressInterface != NULL)
        {
            if (iDownloadComplete)
            {
                aTrackInfoPtr->oEOSReached = true;
                return PVMFSuccess;
            }
            iDownloadProgressInterface->requestResumeNotification(aTrackInfoPtr->iContinuousTimeStamp,
                    iDownloadComplete);
            iAutoPaused = true;
            PVMF_QCPPARSERNODE_LOGERROR((0, "PVMFQCPParserNode::RetrieveMediaSample() - Auto Pause Triggered - TS=%d", aTrackInfoPtr->iContinuousTimeStamp));
            PVMF_QCPPARSERNODE_LOGDATATRAFFIC((0, "PVMFQCPParserNode::RetrieveMediaSample() - Auto Pause Triggered - TS=%d", aTrackInfoPtr->iContinuousTimeStamp));
            return PVMFErrBusy;
        }
        else
        {
            // if we recieve Insufficient data for local playback from parser library that means
            // its end of track, so change track state to send end of track.
            aTrackInfoPtr->oEOSReached = true;
        }
    }
    else if (retval == QcpBitstreamObject::END_OF_FILE)
    {
        aTrackInfoPtr->oEOSReached = true;
    }
    else
    {
        PVMF_QCPPARSERNODE_LOGERROR((0, "PVMFQCPParserNode::RetrieveMediaSample() - Sample Retrieval Failed"));
        return PVMFFailure;
    }

    return PVMFSuccess;
}


void PVMFQCPFFParserNode::PauseAllMediaRetrieval()
{
    PVQCPFFNodeTrackPortInfo* trackInfoPtr = NULL;
    if (!GetTrackInfo(iOutPort, trackInfoPtr))
    {
        PVMF_QCPPARSERNODE_LOGERROR((0, "PVMFQCPParserNode::PauseAllMediaRetrieval: Error - GetPortContainer failed"));
        return;
    }
    trackInfoPtr->oQueueOutgoingMessages = false;

    return;
}

PVMFStatus PVMFQCPFFParserNode:: GenerateAndSendEOSCommand(PVQCPFFNodeTrackPortInfo* aTrackInfoPtr)
{
    PVMF_QCPPARSERNODE_LOGSTACKTRACE((0, "PVMFQCPParserNode::GenerateAndSendEOSCommand Called"));
    if (aTrackInfoPtr->iPort->IsOutgoingQueueBusy() == true)
    {
        /* come back later */
        PVMF_QCPPARSERNODE_LOGDATATRAFFIC((0, "PVMFQCPParserNode::GenerateAndSendEOSCommand: Waiting - Output Queue Busy"));
        return PVMFErrBusy;
    }

    if ((aTrackInfoPtr->oEOSSent == false) && (aTrackInfoPtr->oEOSReached == true))
    {
        PVMFSharedMediaCmdPtr sharedMediaCmdPtr = PVMFMediaCmd::createMediaCmd();

        sharedMediaCmdPtr->setFormatID(PVMF_MEDIA_CMD_EOS_FORMAT_ID);

        sharedMediaCmdPtr->setStreamID(iStreamID);

        sharedMediaCmdPtr->setSeqNum(aTrackInfoPtr->iSeqNum++);

        uint32 ts32 = Oscl_Int64_Utils::get_uint64_lower32(aTrackInfoPtr->iContinuousTimeStamp);
        sharedMediaCmdPtr->setTimestamp(ts32);

        PVMFSharedMediaMsgPtr msg;
        convertToPVMFMediaCmdMsg(msg, sharedMediaCmdPtr);

        PVMFStatus status = aTrackInfoPtr->iPort->QueueOutgoingMsg(msg);
        if (status != PVMFSuccess)
        {
            ReportErrorEvent(PVMFErrPortProcessing, (OsclAny*)(aTrackInfoPtr->iPort));
            PVMF_QCPPARSERNODE_LOGERROR((0, "PVMFQCPParserNode::GenerateAndSendEOSCommand: Error Sending EOS"));
            return status;
        }
        aTrackInfoPtr->oEOSSent = true;
        aTrackInfoPtr->oQueueOutgoingMessages = false;
        aTrackInfoPtr->oProcessOutgoingMessages = true;
        return (status);
    }
    aTrackInfoPtr->oQueueOutgoingMessages = false;
    return PVMFFailure;

}
bool PVMFQCPFFParserNode::RetrieveTrackData(PVQCPFFNodeTrackPortInfo& aTrackPortInfo)
{
    // Create a data buffer from pool
    int errcode = 0;
    OsclSharedPtr<PVMFMediaDataImpl> mediaDataImplOut;
    OSCL_TRY(errcode, mediaDataImplOut = aTrackPortInfo.iMediaDataImplAlloc->allocate(MAXTRACKDATASIZE));

    if (errcode != 0)
    {
        if (errcode == OsclErrNoResources)
        {
            aTrackPortInfo.iTrackDataMemoryPool->notifyfreechunkavailable(aTrackPortInfo);  // Enable flag to receive event when next deallocate() is called on pool
            return false;
        }
        else if (errcode == OsclErrNoMemory)
        {
            // Memory allocation for the pool failed
            ReportErrorEvent(PVMFErrNoMemory, NULL);
            return false;
        }
        else if (errcode == OsclErrArgument)
        {
            // Invalid parameters passed to mempool
            ReportErrorEvent(PVMFErrArgument, NULL);
            return false;
        }
        else
        {
            // General error
            ReportErrorEvent(PVMFFailure, NULL);
            return false;
        }

    }

    // Now create a PVMF media data from pool
    errcode = OsclErrNoResources;
    PVMFSharedMediaDataPtr mediadataout;
    mediadataout = PVMFMediaData::createMediaData(mediaDataImplOut, aTrackPortInfo.iMediaDataMemPool);

    if (mediadataout.GetRep() != NULL)
    {
        errcode = OsclErrNone;
    }
    else
    {
        aTrackPortInfo.iMediaDataMemPool->notifyfreechunkavailable(aTrackPortInfo);     // Enable flag to receive event when next deallocate() is called on pool
        return false;
    }

    // Set the random access point flag if first frame
    if (aTrackPortInfo.iFirstFrame == true)
    {
        uint32 markerInfo = 0;
        markerInfo |= PVMF_MEDIA_DATA_MARKER_INFO_RANDOM_ACCESS_POINT_BIT;
        mediaDataImplOut->setMarkerInfo(markerInfo);
        aTrackPortInfo.iFirstFrame = false;
    }

    // Retrieve memory fragment to write to
    OsclRefCounterMemFrag refCtrMemFragOut;
    mediadataout->getMediaFragment(0, refCtrMemFragOut);

    // Retrieve one bundle of samples from the file format parser
    // Temporary retrieve 32 frames since QCP MDF needs 32 frames
    uint32 numsamples = NUM_QCP_FRAMES;

    // Set up the GAU structure
    GAU gau;
    gau.numMediaSamples = numsamples;
    gau.buf.num_fragments = 1;
    gau.buf.buf_states[0] = NULL;
    gau.buf.fragments[0].ptr = refCtrMemFragOut.getMemFrag().ptr;
    gau.buf.fragments[0].len = refCtrMemFragOut.getCapacity();

    int32 retval = iQCPParser->GetNextBundledAccessUnits(&numsamples, &gau);

    // Determine actual size of the retrieved data by summing each sample length in GAU
    uint32 actualdatasize = 0;
    for (uint32 i = 0; i < numsamples; ++i)
    {
        actualdatasize += gau.info[i].len;
    }

    if (retval == QcpBitstreamObject::EVERYTHING_OK)
    {
        // Set buffer size
        mediadataout->setMediaFragFilledLen(0, actualdatasize);

        // Save the media data in the trackport info
        aTrackPortInfo.iMediaData = mediadataout;

        // Retrieve timestamp and convert to milliseconds


        aTrackPortInfo.iClockConverter->update_clock(Oscl_Int64_Utils::get_uint64_lower32(aTrackPortInfo.iContinuousTimeStamp));

        uint32 timestamp = Oscl_Int64_Utils::get_uint64_lower32(aTrackPortInfo.iContinuousTimeStamp);
        // Set the media data's timestamp
        aTrackPortInfo.iMediaData->setTimestamp(timestamp);


        // compute "next" ts based on the duration of the samples that we obtained
        aTrackPortInfo.iContinuousTimeStamp += numsamples * QCP_SAMPLE_DURATION; // i.e 20ms

        // Set the sequence number
        aTrackPortInfo.iMediaData->setSeqNum(aTrackPortInfo.iSeqNum++);

        return true;
    }
    else if (retval == QcpBitstreamObject::READ_ERROR)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFQCPFFParserNode::RetrieveTrackData() QCP Parser READ_ERROR"));
        PVUuid erruuid = PVMFFileFormatEventTypesUUID;
        int32 errcode = PVMFFFErrFileRead;
        ReportErrorEvent(PVMFErrResource, NULL, &erruuid, &errcode);
        return false;
    }
    else if (retval == QcpBitstreamObject::END_OF_FILE)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFQCPFFParserNode::RetrieveTrackData() QCP Parser End Of File!"));
        if (SendEndOfTrackCommand(aTrackPortInfo))
        {
            // EOS message sent so change state
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "PVMFQCPFFParserNode::RetrieveTrackData() Sending EOS message succeeded"));
            return false;
        }
        else
        {
            // EOS message could not be queued so keep in same state and try again later
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "PVMFQCPFFParserNode::RetrieveTrackData() Sending EOS message failed"));
            return true;
        }
    }
    else
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFQCPFFParserNode::RetrieveTrackData() QCP Parser Unknown Error!"));
        PVUuid erruuid = PVMFFileFormatEventTypesUUID;
        int32 errcode = PVMFFFErrInvalidData;
        ReportErrorEvent(PVMFErrCorrupt, NULL, &erruuid, &errcode);
        return false;
    }
}

PVMFStatus PVMFQCPFFParserNode::SendBeginOfMediaStreamCommand(PVQCPFFNodeTrackPortInfo* aTrackPortInfoPtr)
{
    PVMFSharedMediaCmdPtr sharedMediaCmdPtr = PVMFMediaCmd::createMediaCmd();
    sharedMediaCmdPtr->setFormatID(PVMF_MEDIA_CMD_BOS_FORMAT_ID);

    uint32 timestamp = Oscl_Int64_Utils::get_uint64_lower32(aTrackPortInfoPtr->iContinuousTimeStamp);
    sharedMediaCmdPtr->setTimestamp(timestamp);

    PVMFSharedMediaMsgPtr mediaMsgOut;
    convertToPVMFMediaCmdMsg(mediaMsgOut, sharedMediaCmdPtr);
    mediaMsgOut->setStreamID(iStreamID);

    PVMFStatus status =  aTrackPortInfoPtr->iPort->QueueOutgoingMsg(mediaMsgOut);
    if (status != PVMFSuccess)
    {
        // Output queue is busy, so wait for the output queue being ready
        PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_DEBUG,
                        (0, "PVMFQCPFFParserNode::SendBeginOfMediaStreamCommand: Outgoing queue busy. "));
        return status;
    }
    aTrackPortInfoPtr->iSendBOS = false;
    aTrackPortInfoPtr->oProcessOutgoingMessages = true;
    PVLOGGER_LOGMSG(PVLOGMSG_INST_LLDBG, iLogger, PVLOGMSG_STACK_TRACE, (0, "PVMFQCPFFParserNode::SendBeginOfMediaStreamCommand() BOS Sent StreamId %d ", iStreamID));
    return status;
}

PVMFCommandId
PVMFQCPFFParserNode::GetLicense(PVMFSessionId aSessionId,
                                OSCL_wString& aContentName,
                                OsclAny* aData,
                                uint32 aDataSize,
                                int32 aTimeoutMsec,
                                OsclAny* aContextData)
{
    PVMF_QCPPARSERNODE_LOGDATATRAFFIC((0, "PVMFQCPFFParserNode::GetLicense - Wide called"));
    PVMFQCPFFNodeCommand cmd;
    cmd.PVMFQCPFFNodeCommand::Construct(aSessionId,
                                        PVMF_QCP_PARSER_NODE_GET_LICENSE_W,
                                        aContentName,
                                        aData,
                                        aDataSize,
                                        aTimeoutMsec,
                                        aContextData);
    return QueueCommandL(cmd);
}

PVMFCommandId
PVMFQCPFFParserNode::GetLicense(PVMFSessionId aSessionId,
                                OSCL_String&  aContentName,
                                OsclAny* aData,
                                uint32 aDataSize,
                                int32 aTimeoutMsec,
                                OsclAny* aContextData)
{
    PVMF_QCPPARSERNODE_LOGDATATRAFFIC((0, "PVMFQCPFFParserNode::GetLicense - Wide called"));
    PVMFQCPFFNodeCommand cmd;
    cmd.PVMFQCPFFNodeCommand::Construct(aSessionId,
                                        PVMF_QCP_PARSER_NODE_GET_LICENSE,
                                        aContentName,
                                        aData,
                                        aDataSize,
                                        aTimeoutMsec,
                                        aContextData);
    return QueueCommandL(cmd);
}

PVMFStatus PVMFQCPFFParserNode::DoGetLicense(PVMFQCPFFNodeCommand& aCmd,
        bool aWideCharVersion)
{
    if (iCPMLicenseInterface == NULL)
    {
        return PVMFErrNotSupported;
    }

    if (aWideCharVersion == true)
    {
        OSCL_wString* contentName = NULL;
        OsclAny* data = NULL;
        uint32 dataSize = 0;
        int32 timeoutMsec = 0;
        aCmd.PVMFQCPFFNodeCommand::Parse(contentName,
                                         data,
                                         dataSize,
                                         timeoutMsec);
        iCPMGetLicenseCmdId =
            iCPMLicenseInterface->GetLicense(iCPMSessionID,
                                             *contentName,
                                             data,
                                             dataSize,
                                             timeoutMsec);
    }
    else
    {
        OSCL_String* contentName = NULL;
        OsclAny* data = NULL;
        uint32 dataSize = 0;
        int32 timeoutMsec = 0;
        aCmd.PVMFQCPFFNodeCommand::Parse(contentName,
                                         data,
                                         dataSize,
                                         timeoutMsec);
        iCPMGetLicenseCmdId =
            iCPMLicenseInterface->GetLicense(iCPMSessionID,
                                             *contentName,
                                             data,
                                             dataSize,
                                             timeoutMsec);
    }
    return PVMFPending;
}

void PVMFQCPFFParserNode::CompleteGetLicense()
{
    CommandComplete(iCurrentCommand,
                    iCurrentCommand.front(),
                    PVMFSuccess);
}

PVMFCommandId
PVMFQCPFFParserNode::CancelGetLicense(PVMFSessionId aSessionId, PVMFCommandId aCmdId, OsclAny* aContextData)
{
    PVMF_QCPPARSERNODE_LOGDATATRAFFIC((0, "PVMFQCPFFParserNode::CancelGetLicense - called"));
    PVMFQCPFFNodeCommand cmd;
    cmd.PVMFQCPFFNodeCommandBase::Construct(aSessionId, PVMF_QCP_PARSER_NODE_CMD_CANCEL_GET_LICENSE, aCmdId, aContextData);
    return QueueCommandL(cmd);
}

PVMFStatus PVMFQCPFFParserNode::DoCancelGetLicense(PVMFQCPFFNodeCommand& aCmd)
{
    PVMF_QCPPARSERNODE_LOGDATATRAFFIC((0, "PVMFQCPFFParserNode::DoCancelGetLicense() Called"));
    PVMFStatus status = PVMFErrArgument;

    if (iCPMLicenseInterface == NULL)
    {
        status = PVMFErrNotSupported;
    }
    else
    {
        /* extract the command ID from the parameters.*/
        PVMFCommandId id;
        aCmd.PVMFQCPFFNodeCommandBase::Parse(id);

        /* first check "current" command if any */
        PVMFQCPFFNodeCommand* cmd = iCurrentCommand.FindById(id);
        if (cmd)
        {
            if (cmd->iCmd == PVMF_QCP_PARSER_NODE_GET_LICENSE_W || cmd->iCmd == PVMF_QCP_PARSER_NODE_GET_LICENSE)
            {
                iCPMCancelGetLicenseCmdId =
                    iCPMLicenseInterface->CancelGetLicense(iCPMSessionID, iCPMGetLicenseCmdId);

                /*
                 * the queued commands are all asynchronous commands to the
                 * CPM module. CancelGetLicense can cancel only for GetLicense cmd.
                 * We need to wait CPMCommandCompleted.
                 */
                return PVMFPending;
            }
        }

        /*
         * next check input queue.
         * start at element 1 since this cancel command is element 0.
         */
        cmd = iInputCommands.FindById(id, 1);
        if (cmd)
        {
            if (cmd->iCmd == PVMF_QCP_PARSER_NODE_GET_LICENSE_W || cmd->iCmd == PVMF_QCP_PARSER_NODE_GET_LICENSE)
            {
                /* cancel the queued command */
                CommandComplete(iInputCommands, *cmd, PVMFErrCancelled);
                /* report cancel success */
                return PVMFSuccess;
            }
        }
    }
    /* if we get here the command isn't queued so the cancel fails */
    return status;
}

int32 PVMFQCPFFParserNode::PushBackKeyVal(Oscl_Vector<PvmiKvp, OsclMemAllocator>*& aValueListPtr, PvmiKvp &aKeyVal)
{
    int32 leavecode = 0;
    OSCL_TRY(leavecode, (*aValueListPtr).push_back(aKeyVal));
    return leavecode;
}

PVMFStatus PVMFQCPFFParserNode::PushValueToList(Oscl_Vector<OSCL_HeapString<OsclMemAllocator>, OsclMemAllocator> &aRefMetaDataKeys, PVMFMetadataList *&aKeyListPtr, uint32 aLcv)
{
    int32 leavecode = 0;
    OSCL_TRY(leavecode, aKeyListPtr->push_back(aRefMetaDataKeys[aLcv]));
    OSCL_FIRST_CATCH_ANY(leavecode, PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_ERR, (0, "PVMFQCPFFParserNode::PushValueToList() Memory allocation failure when copying metadata key"));return PVMFErrNoMemory);
    return PVMFSuccess;
}

