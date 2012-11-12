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
// -*- c++ -*-
// = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =

//                 Q C P    F I L E    P A R S E R

// = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =


/**
 *  @file qcpfileparser.h
 *  @brief This file defines the raw GSM-QCP file parser.
 */

#ifndef QCPFILEPARSER_H_INCLUDED
#define QCPFILEPARSER_H_INCLUDED

//----------------------------------------------------------------------------
// INCLUDES
//----------------------------------------------------------------------------

#ifndef OSCL_BASE_H_INCLUDED
#include "oscl_base.h"
#endif

#ifndef OSCL_STRING_H_INCLUDED
#include "oscl_string.h"
#endif

#ifndef OSCL_FILE_IO_H_INCLUDED
#include "oscl_file_io.h"
#endif

#ifndef OSCL_MEM_H_INCLUDED
#include "oscl_mem.h"
#endif

#ifndef OSCL_VECTOR_H_INCLUDED
#include "oscl_vector.h"
#endif

#ifndef PV_GAU_H
#include "pv_gau.h"
#endif
#ifndef PVFILE_H
#include "pvfile.h"
#endif

#ifndef PVLOGGER_H_INCLUDED
#include "pvlogger.h"
#endif

#define PVMF_QCPPARSER_LOGDIAGNOSTICS(m) PVLOGGER_LOGMSG(PVLOGMSG_INST_MLDBG,iDiagnosticLogger,PVLOGMSG_INFO,m);
#define PVMF_QCPPARSER_LOGERROR(m) PVLOGGER_LOGMSG(PVLOGMSG_INST_REL,iLogger,PVLOGMSG_ERR,m);
#define PVMF_QCPPARSER_LOGDEBUG(m) PVLOGGER_LOGMSG(PVLOGMSG_INST_REL,iLogger,PVLOGMSG_DEBUG,m);

//----------------------------------------------------------------------------
// CONSTANTS
//----------------------------------------------------------------------------

#define MAX_NUM_PACKED_INPUT_BYTES  36 /* Max number of packed input bytes     */
#define MAX_NUM_FRAMES_PER_BUFF     64 /* Max number of frames queued per call */
#define NUM_BITS_PER_BYTE            8 /* There are 8 bits in a byte           */
#define TIME_STAMP_PER_FRAME        20 //160 /* Time stamp value per frame of QCP */

#define QCP_HEADER_SIZE         194
#define QCP_CBR_HEADER_SIZE     178
#define QCP_VBR_HEADER_SIZE     194

#define QCP_RIFF_CHUNK_LENGTH   12
#define QCP_FMT_CHUNK_LENGTH    10
#define QCP_CODEC_INFO_LENGTH   148
#define QCP_VRAT_CHUNK_LENGTH   16
#define QCP_DATA_CHUNK_LENGTH   8

#define QCP_QCELP_13K_PKT_SIZE  36
#define QCP_EVRC_PKT_SIZE       24

#define QCP_FMT1_CHUNK_LENGTH   10

/* Bit format supported by the parser */
enum TQCPFormat
{
    QCP_FORMAT_EVRC,   // EVRC
    QCP_FORMAT_QCELP,  // QCELP
    QCP_FORMAT_UNKNOWN // Unknown
};

typedef struct
{
    unsigned int data1;
    unsigned short data2;
    unsigned short data3;
    char data4[8];
} GUID;

typedef struct
{
    char riff[4];
    unsigned int s_riff;
    char qlcm[4];
}RIFFChunk;

typedef struct
{
    char fmt[4];
    unsigned int s_fmt;
    char mjr;
    char mnr;
} FmtChunk1;

typedef struct
{
    /* UNIQUE ID of the codec */
    GUID codec_id;

    /* Codec Info */
    unsigned short ver;
    char name[80];
    unsigned short abps;    /* average bits per sec of the codec */
    unsigned short bytes_per_pkt;
    unsigned short samp_per_block;
    unsigned short samp_per_sec;
    unsigned short bits_per_samp;

    /* Rate Header fmt info */
    unsigned int   vr_num_of_rates;
    unsigned short vr_bytes_per_pkt[8];

    unsigned int rvd2[5];
} CodecInfo;

typedef struct
{
    FmtChunk1 fmt1;
    CodecInfo qpl_info;
} FmtChunk;

typedef struct
{
    unsigned char vrat[4];
    unsigned int s_vrat;
    unsigned int v_rate;
    unsigned int size_in_pkts;
} VratChunk;

typedef struct
{
    unsigned char data[4];
    unsigned int s_data;
} DataChunk;

typedef struct
{
    RIFFChunk riff;
    FmtChunk fmt;
    VratChunk vrat_chunk;
    DataChunk data_chunk;
} QCPHeaderInfo;

typedef struct
{
    int32 iBitrate;
    int32 iTimescale;
    int32 iDuration;
    int32 iFileSize;
    int32 iQcpFormat;
} TPVQcpFileInfo;

//----------------------------------------------------------------------------
// FORWARD CLASS DECLARATIONS
//----------------------------------------------------------------------------


/**
 *  @brief The QcpBitstreamObject Class is the class used by the QCP parser to
 *  manipulate the bitstream read from the file.
 */
class QcpBitstreamObject
{
    public:
        enum
        {
            MAIN_BUFF_SIZE = 8192,
            SECOND_BUFF_SIZE = 36,

            // error types for GetNextBundledAccessUnits(),
            // the definition is consistent with MP4_ERROR_CODE in iSucceedFail.h
            MISC_ERROR = -2,
            READ_ERROR = -1,
            EVERYTHING_OK = 0,
            END_OF_FILE = 62,
            DATA_INSUFFICIENT = 141
        };

        /**
        * @brief Constructor
        *
        * @param pFile Pointer to file pointer containing bitstream
        * @returns None
        */

        QcpBitstreamObject(PVLogger *aLogger, PVFile* pFile = NULL)
        {
            oscl_memset(this, 0, sizeof(QcpBitstreamObject));
            iLogger = aLogger;
            init(pFile);
            iBuffer = OSCL_ARRAY_NEW(uint8, QcpBitstreamObject::MAIN_BUFF_SIZE + QcpBitstreamObject::SECOND_BUFF_SIZE);
            if (iBuffer)
            {
                iStatus = true;
            }
            else
            {
                iStatus = false;
            }
        }

        /**
        * @brief Destructor
        *
        * @param None
        * @returns None
        */
        ~QcpBitstreamObject()
        {
            /*init();*/
            if (iBuffer)
            {
                OSCL_ARRAY_DELETE(iBuffer);
                iBuffer = NULL;
            }
        }

        /**
        * @brief Returns current bitstream status
        *
        * @param None
        * @returns true=Bitstream instantiated; false=no bitstream
        */
        inline bool get()
        {
            return iStatus;
        }

        /**
        * @brief Re-positions the file pointer. Specially used in ResetPlayback()
        *
        * @param filePos Position in file to move to.
        * @returns None
        */
        int32 reset(int32 filePos = 0);

        /**
        * @brief Retrieves clip information: file size, format(QCELP or EVRC) and bitrate
        *
        * @param fileSize Size of file
        * @param format QCP format
        * @param bitrate Bitrate of file
        * @returns Result of operation: EVERYTHING_OK, READ_FAILED, etc.
        */
        int32  getFileInfo(int32& fileSize, int32& format, int32& bitrate);

        /**
        * @brief Retrieves one frame data plus frame type, used in getNextBundledAccessUnits().
        *
        * @param frameBuffer Buffer containing frame read
        * @param frame_type Frame type of frame
        * @param bHeaderIncluded Indicates whether to include header or not in buffer
        * @returns Result of operation: EVERYTHING_OK, READ_FAILED, etc.
        */
        int32  getNextFrame(uint8* frameBuffer, uint8& frame_size, bool bHeaderIncluded = false);

        /**
        * @brief Undo getNextFrame in case gau buffer overflow occurs when getting next frame
        *
        * @param offset Number of bytes to move back from current position in file
        * @returns None
        */
        void undoGetNextFrame(int32 offset)
        {
            iPos -= offset;
        }

        int32 iBitRate;
        int32 iSamplingRate;
        int32 iFrameLengthInBytes;
        int32 iDuration;
        int32 iNumOfVBRs;
        int32 iNumberOfFrames;
        int32 iFileSizeToDecode;
        uint8 iBitrates[8];
        uint8 iPacketSizes[8];

    private:

        /**
        * @brief Initialization
        *
        * @param pFile File pointer
        * @returns None
        */
        inline void init(PVFile* pFile = NULL)
        {
            iFileSize = 0;
            iBytesRead = 0;
            iBytesProcessed = 0;
            ipQCPFile = pFile;
            iActual_size = iMax_size = QcpBitstreamObject::MAIN_BUFF_SIZE;
            iPos = QcpBitstreamObject::MAIN_BUFF_SIZE + QcpBitstreamObject::SECOND_BUFF_SIZE;
            iQcpFormat = iFrame_type = 0;
            iInitFilePos = 0;

            if (ipQCPFile)
            {
                ipQCPFile->Seek(0, Oscl_File::SEEKSET);
            }
        }

        /**
        * @brief Reads data from bitstream, this is the only function to read data from file
        *
        * @param None
        * @returns Result of operation: EVERYTHING_OK, READ_FAILED, etc.
        */
        int32 refill();

        /**
        * @brief Parses the QCP bitstream header: and get format(QCELP or EVRC)
        *
        * @param None
        * @returns Result of operation: EVERYTHING_OK, READ_FAILED, etc.
        */
        int32 parseQCPHeader();

        /**
         * @brief Gets the updated file size
         *
         * @param None
         * @returns Result of operation: true/false.
         */
        bool UpdateFileSize();

        QCPHeaderInfo iQCPHeaderInfo;

    private:
        int32 iPos;             // pointer for iBuffer[]
        int32 iActual_size;     // number of bytes read from a file once <= max_size
        int32 iMax_size;        // max_size = bitstreamStruc::MAIN_BUFF_SIZE
        int32 iBytesRead;       // (cumulative) number of bytes read from a file so far
        int32 iBytesProcessed;
        int32 iFileSize;        // file size of the ipQCPFile
        int32 iQcpFormat;       // 0 : EVRC ; 1 : QCELP; 2 : Unrecognized format
        uint32 iInitFilePos;
        int32 iFrame_type;      // frame type got from the very first frame

        uint8 *iBuffer;
        PVFile* ipQCPFile; // bitstream file
        bool iStatus;
        PVLogger *iLogger;

        /**
        * @brief Checks for the valide QCP header
        *
        * @param fmt
        * @param qpl_info
        * @returns error type.
        */
        int32 IsValidQCPHeader(FmtChunk1 *fmt,
                               CodecInfo *qpl_info);

        /**
        * @brief Checks valide Codec IDs
        *
        * @param codec_id
        * @param codec_id_ref
        * @returns error type.
        */
        int32 IsValidCodecID(GUID *codec_id,
                             GUID *codec_id_ref);

        uint32 GetDuration();

};

/**
 *  @brief The CQCPFileParser Class is the class that will construct and maintain
 *  all the necessary data structures to be able to render a valid QCP file
 *  to disk.
 *
 *  This class supports the following QCP file format specs: QCELP & EVRC
 */
class PVMFCPMPluginAccessInterfaceFactory;
class CQCPFileParser
{
    public:
        typedef OsclMemAllocator alloc_type;

        /**
        * @brief Constructor
        *
        * @param None
        * @returns None
        */
        OSCL_IMPORT_REF  CQCPFileParser();

        /**
        * @brief Destructor
        *
        * @param None
        * @returns None
        */
        OSCL_IMPORT_REF ~CQCPFileParser();

        /**
        * @brief Opens the specified clip and performs initialization of the parser
        *
        * @param aClip Filename to parse
        * @param aInitParsingEnable Indicates whether to setup random positioning (true)
        * or not (false)
        * @param aFileSession Pointer to opened file server session. Used when opening
        * and reading the file on certain operating systems.
        * @returns true if the init succeeds, else false.
        */
        OSCL_IMPORT_REF bool InitQCPFile(OSCL_wString& aClip, bool aInitParsingEnable = true, Oscl_FileServer* aFileSession = NULL, PVMFCPMPluginAccessInterfaceFactory*aCPM = NULL, OsclFileHandle*aHandle = NULL, uint32 countToClaculateRDATimeInterval = 1);

        /**
        * @brief Retrieves information about the clip such as bit rate, sampling frequency, etc.
        *
        * @param aInfo Storage for information retrieved
        * @returns True if successful, False otherwise.
        */
        OSCL_IMPORT_REF bool RetrieveFileInfo(TPVQcpFileInfo& aInfo);

        /**
        * @brief Resets the parser variables to allow start of playback at a new position
        *
        * @param aStartTime Time where to start playback from
        * @returns Result of operation: EVERYTHING_OK, READ_FAILED, etc.
        */
        OSCL_IMPORT_REF int32  ResetPlayback(int32 aStartTime = 0);

        /**
        * @brief Returns the actual starting timestamp for a specified start time
        *
        * @param aStartTime Time where to start playback from
        * @returns Timestamp corresponding to the actual start position
        */
        OSCL_IMPORT_REF uint32 SeekPointFromTimestamp(uint32 aStartTime = 0);

        /**
        * @brief Attempts to read in the number of QCP frames specified by aNumSamples.
        * It formats the read data to QCELP/EVRC bit order and stores it in the GAU structure.
        *
        * @param aNumSamples Requested number of frames to be read from file
        * @param aGau Frame information structure of type GAU
        * @returns Result of operation: EVERYTHING_OK, READ_FAILED, etc.
        */
        OSCL_IMPORT_REF int32  GetNextBundledAccessUnits(uint32 *aNumSamples, GAU *aGau);

        /**
        * @brief Returns the value of the next timestamp that will be
        *    returned in a call to GetNextBundledAccessUnits.
        *
        * @param aTimestamp returned value of next timestamp
        * @returns Result of operation: EVERYTHING_OK, READ_FAILED, etc.
        */
        OSCL_IMPORT_REF int32  PeekNextTimestamp(uint32 *aTimestamp);

        /**
        * @brief Returns the frame type of the current QCP frame
        *
        * @param aFrameIndex Index to frame
        * @returns Frame type
        */
        OSCL_IMPORT_REF uint8  GetFrameTypeInCurrentBundledAccessUnits(uint32 aFrameIndex);

        OSCL_IMPORT_REF uint8*  getCodecSpecificInfo();

    private:
        PVFile          iQCPFile;
        int32           iQCPDuration;
        int32           iQCPBitRate;
        int32           iQCPFormat;
        int32           iQCPFileSize;
        int32           iTotalNumFramesRead;
        bool            iEndOfFileReached;
        QcpBitstreamObject *ipBSO;
        Oscl_Vector<int32, alloc_type> iRPTable; // table containing sync indexes for repositioning

        /* Decoder input buffer for 1 raw encoded speech frame (QCELP or EVRC) */
        uint8 iQCPFrameBuffer[MAX_NUM_PACKED_INPUT_BYTES];
        uint8 iQCPFrameHeaderBuffer[MAX_NUM_FRAMES_PER_BUFF];
        uint32 iRandomAccessTimeInterval;
        uint32 iCountToClaculateRDATimeInterval;
        bool CalculateDuration(bool aInitParsingEnable, uint32 countToClaculateRDATimeInterval);
        PVLogger*     iLogger;
        PVLogger*     iDiagnosticLogger;
};

#endif //QCPFILEPARSER_H_INCLUDED

