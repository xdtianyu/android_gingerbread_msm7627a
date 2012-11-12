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
#ifndef PVMF_QCPFFPARSER_EVENTS_H_INCLUDED
#define PVMF_QCPFFPARSER_EVENTS_H_INCLUDED

/**
 UUID for PV QCP FF parser node error and information event type codes
 **/
#define PVMFQCPParserNodeEventTypesUUID PVUuid(0x5e8ebc60,0x1934,0x11de,0xa0,0x43,0x00,0x02,0xa5,0xd5,0xc5,0x1b)

/**
 * An enumeration of error types from PV QCP FF parser node
 **/
typedef enum
{
    /**
     When QCP FF reports error READ_BITRATE_MUTUAL_EXCLUSION_OBJECT_FAILED
    **/
    PVMFQCPFFParserErrBitRateMutualExclusionObjectFailed = 1024,

    /**
     When QCP FF reports error READ_BITRATE_RECORD_FAILED
    **/
    PVMFQCPFFParserErrBitRateRecordReadFailed,

    /**
     When QCP FF reports error READ_CODEC_ENTRY_FAILED
    **/
    PVMFQCPFFParserErrCodecEntryReadFailed,

    /**
     When QCP FF reports error READ_CODEC_LIST_OBJECT_FAILED
    **/
    PVMFQCPFFParserErrCodecListObjectReadFailed,

    /**
     When QCP FF reports error READ_CONTENT_DESCRIPTION_OBJECT_FAILED
    **/
    PVMFQCPFFParserErrContentDescriptionObjectReadFailed,

    /**
     When QCP FF reports error READ_CONTENT_DESCRIPTOR_FAILED
    **/
    PVMFQCPFFParserErrContentDescriptorReadFailed,

    /**
     When QCP FF reports error READ_DATA_OBJECT_FAILED
    **/
    PVMFQCPFFParserErrDataObjectReadFailed,

    /**
     When QCP FF reports error READ_DATA_PACKET_FAILED
    **/
    PVMFQCPFFParserErrDataPacketReadFailed,

    /**
     When QCP FF reports error INCORRECT_ERROR_CORRECTION_DATA_TYPE
    **/
    PVMFQCPFFParserErrIncorrectErrorCorrectionDataType,

    /**
     When QCP FF reports error OPAQUE_DATA_NOT_SUPPORTED
    **/
    PVMFQCPFFParserErrOpaqueDataNotSupported,

    /**
     When QCP FF reports error READ_DATA_PACKET_PAYLOAD_FAILED
    **/
    PVMFQCPFFParserErrDataPacketPayloadReadFailed,

    /**
     When QCP FF reports error ZERO_OR_NEGATIVE_SIZE
    **/
    PVMFQCPFFParserErrZeroOrNegativeSize,

    /**
     When QCP FF reports error READ_ERROR_CORRECTION_OBJECT_FAILED
    **/
    PVMFQCPFFParserErrErrorCorrectionObjectReadFailed,

    /**
     When QCP FF reports error READ_EXTENDED_CONTENT_DESCRIPTION_OBJECT_FAILED
    **/
    PVMFQCPFFParserErrExtendedContentDescriptionObjectReadFailed,

    /**
     When QCP FF reports error READ_FILE_PROPERTIES_OBJECT_FAILED
    **/
    PVMFQCPFFParserErrFilePropertiesObjectReadFailed,

    /**
     When QCP FF reports error INVALID_FILE_PROPERTIES_OBJECT_SIZE
    **/
    PVMFQCPFFParserErrInvalidFilePropertiesObjectSize,

    /**
     When QCP FF reports error INVALID_DATA_PACKET_COUNT
    **/
    PVMFQCPFFParserErrInvalidDataPacketCount,

    /**
     When QCP FF reports error INVALID_PACKET_SIZE
    **/
    PVMFQCPFFParserErrInvalidDataPacketSize,

    /**
     When QCP FF reports error READ_HEADER_EXTENSION_OBJECT_FAILED
    **/
    PVMFQCPFFParserErrHeaderExtensionObjectReadFailed,

    /**
     When QCP FF reports error RES_VAL_IN_HEADER_EXTENSION_OBJ_INCORRECT
    **/
    PVMFQCPFFParserErrReservedValueInHeaderExtensionObjectIncorrect,

    /**
     When QCP FF reports error READ_HEADER_OBJECT_FAILED
    **/
    PVMFQCPFFParserErrHeaderObjectReadFailed,

    /**
     When QCP FF reports error MANDATORY_HEADER_OBJECTS_MISSING
    **/
    PVMFQCPFFParserErrMandatoryHeaderObjectsMissing,

    /**
     When QCP FF reports error NO_STREAM_OBJECTS_IN_FILE
    **/
    PVMFQCPFFParserErrNoStreamObjectsInFile,

    /**
     When QCP FF reports error RES_VALUE_IN_HDR_OBJECT_INCORRECT
    **/
    PVMFQCPFFParserErrReservedValueInHeaderObjectIncorrect,

    /**
     When QCP FF reports error DUPLICATE_OBJECTS
    **/
    PVMFQCPFFParserErrDuplicateObjects,

    /**
     When QCP FF reports error ZERO_OR_NEGATIVE_OBJECT_SIZE
    **/
    PVMFQCPFFParserErrZeroOrNegativeObjectSize,

    /**
     When QCP FF reports error READ_SCRIPT_COMMAND_OBJECT_FAILED
    **/
    PVMFQCPFFParserErrScriptCommandObjectReadFailed,

    /**
     When QCP FF reports error READ_PADDING_OBJECT_FAILED
    **/
    PVMFQCPFFParserErrPaddingObjectReadFailed,

    /**
     When QCP FF reports error READ_MARKER_FAILED
    **/
    PVMFQCPFFParserErrMarkerReadFailed,

    /**
     When QCP FF reports error READ_MARKER_OBJECT_FAILED
    **/
    PVMFQCPFFParserErrMarkerObjectReadFailed,

    /**
     When QCP FF reports error READ_STREAM_BITRATE_PROPERTIES_OBJECT_FAILED
    **/
    PVMFQCPFFParserErrStreamBitRatePropertiesObjectReadFailed,

    /**
     When QCP FF reports error READ_STREAM_PROPERTIES_OBJECT_FAILED
    **/
    PVMFQCPFFParserErrStreamPropertiesObjectReadFailed,

    /**
     When QCP FF reports error INVALID_STREAM_PROPERTIES_OBJECT_SIZE
    **/
    PVMFQCPFFParserErrInvalidStreamPropertiesObjectSize,

    /**
     When QCP FF reports error INVALID_STREAM_NUMBER
    **/
    PVMFQCPFFParserErrInvalidStreamNumber,

    /**
     When QCP FF reports error READ_SIMPLE_INDEX_OBJECT_FAILED
    **/
    PVMFQCPFFParserErrSimpleIndexObjectReadFailed,

    /**
     When QCP FF reports error READ_INDEX_ENTRY_FAILED
    **/
    PVMFQCPFFParserErrIndexEntryReadFailed,

    /**
     When QCP FF reports error NO_MEDIA_STREAMS
    **/
    PVMFQCPFFParserErrNoMediaStreams,

    /**
     When QCP FF reports error READ_UNKNOWN_OBJECT
    **/
    PVMFQCPFFParserErrReadUnknownObject,

    /**
     When QCP FF reports error ASF_FILE_OPEN_FAILED
    **/
    PVMFQCPFFParserErrFileOpenFailed,

    /**
     When QCP FF reports error ASF_SAMPLE_INCOMPLETE
    **/
    PVMFQCPFFParserErrIncompleteASFSample,

    /**
     When QCP FF reports error PARSE_TYPE_SPECIFIC_DATA_FAILED
    **/
    PVMFQCPFFParserErrParseTypeSpecificDataFailed,

    /**
     When QCP FF reports error END_OF_MEDIA_PACKETS
    **/
    PVMFQCPFFParserErrEndOfMediaPackets,

    /**
     When QCP FF reports error READ_CONTENT_ENCRYPTION_OBJECT_FAILED
    **/
    PVMFQCPFFParserErrContentEncryptionObjectReadFailed,

    /**
     When QCP FF reports error READ_EXTENDED_CONTENT_ENCRYPTION_OBJECT_FAILED
    **/
    PVMFQCPFFParserErrExtendedContentEncryptionObjectReadFailed,

    /**
     When QCP FF reports error READ_INDEX_SPECIFIER_FAILED
    **/
    PVMFQCPFFParserErrIndexSpecifierReadFailed,

    /**
     When QCP FF reports error READ_INDEX_BLOCK_FAILED
    **/
    PVMFQCPFFParserErrIndexBlockReadFailed,

    /**
     When QCP FF reports error READ_INDEX_OBJECT_FAILED
    **/
    PVMFQCPFFParserErrIndexObjectReadFailed,

    PVMFQCPFFParserErrUnableToOpenFile,
    PVMFQCPFFParserErrUnableToRecognizeFile,
    PVMFQCPFFParserErrUnableToCreateASFFileClass,
    PVMFQCPFFParserErrTrackMediaMsgAllocatorCreationFailed,
    PVMFQCPFFParserErrUnableToPopulateTrackInfoList,
    PVMFQCPFFParserErrInitMetaDataFailed,

    /**
     Placeholder for the last PV QCP FF parser error event
     **/
    PVMFQCPFFParserErrLast = 8191
} PVMFQCPFFParserErrorEventType;

/**
 * An enumeration of informational event types from PV ASF FF parser node
 **/
typedef enum
{
    /**
     Placeholder for the last PV ASF FF parser informational event
     **/
    PVMFQCPFFParserInfoLast = 10000

} PVMFQCPFFParserInformationalEventType;

#endif // PVMF_QCPFFPARSER_EVENTS_H_INCLUDED


