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

//                       Q C P   P A R S E R

// = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =


/**
 *  @file qcpparser.h
 *  @brief This include file contains the definitions for the actual QCP
 *  file parser.
 */

#ifndef QCPPARSER_H_INCLUDED
#define QCPPARSER_H_INCLUDED


//----------------------------------------------------------------------
// Include Files
//----------------------------------------------------------------------
#ifndef OSCL_BASE_H_INCLUDED
#include "oscl_base.h"
#endif
#ifndef OSCL_FILE_IO_H_INCLUDED
#include "oscl_file_io.h"
#endif
#ifndef OSCL_MEDIA_DATA_H
#include "oscl_media_data.h"
#endif
#ifndef PVFILE_H_INCLUDED
#include "pvfile.h"
#endif
#ifndef IQCPFF_H_INCLUDED
#include "iqcpff.h"
#endif
#ifndef PV_GAU_H_
#include "pv_gau.h"
#endif
#ifndef PV_ID3_PARCOM_H_INCLUDED
#include "pv_id3_parcom.h"
#endif
#ifndef QCP_UTILS_H_INCLUDED
#include "qcputils.h"
#endif
#ifndef __MEDIA_CLOCK_CONVERTER_H
#include "media_clock_converter.h"
#endif
#ifndef PVLOGGER_H_INCLUDED
#include "pvlogger.h"
#endif

//----------------------------------------------------------------------
// Global Type Declarations
//----------------------------------------------------------------------

//----------------------------------------------------------------------
// Global Constant Declarations
//----------------------------------------------------------------------

#define QCP_HEADER_SIZE         170
#define QCP_CBR_HEADER_SIZE     170
#define QCP_VBR_HEADER_SIZE     186

#define QCP_RIFF_CHUNK_LENGTH   12
#define QCP_FMT_CHUNK_LENGTH    10
#define QCP_CODEC_INFO_LENGTH   148
#define QCP_VRAT_CHUNK_LENGTH   16
#define QCP_DATA_CHUNK_LENGTH   8

#define QCP_QCELP_13K_PKT_SIZE  36
#define QCP_EVRC_PKT_SIZE       24

#define QCP_FMT1_CHUNK_LENGTH   10

#define TIME_STAMP_PER_FRAME    20 // 160/8000 = 20ms

//======================================================================
//  CLASS DEFINITIONS and FUNCTION DECLARATIONS
//======================================================================

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

/**
 *  @brief The QCPParser Class is the class that parses the
 *  QCP file.
 */

class QCPParser
{

    public:
        /**
        * @brief Constructor.
        *
        * @param Filehandle
        * @returns None
        */
        QCPParser(PVFile* aFileHandle = NULL);

        /**
        * @brief Destructor.
        *
        * @param None
        * @returns None
        */
        ~QCPParser();

        /**
        * @brief Parses the MetaData (beginning or end) and positions
        * the file pointer at the first audio frame.
        *
        * @param fpUsed Pointer to file
        * @param enableCRC, CRC check flag
        * @returns error type.
        */
        QCPErrorType	ParseQCPFile(PVFile * fpUsed);

        /**
        * @brief Checks the file is valid qcp clip or not
        *
        * @param fpUsed Pointer to file
        * @param filename Name of file
        * @returns error type.
        */
        QCPErrorType IsQcpFile(QCP_FF_FILE* aFile);

        /**
        * @brief Returns the duration of the file.
        *
        * @param None
        * @returns Clip duration
        */
        uint32  GetDuration();

    private:
	//duration related values
        uint32 iClipDurationInMsec;

    protected:
        int32 iLocalFileSize;
        bool iLocalFileSizeSet;
        PVFile * fp;
    public:
        /*scan file related */
        QCPHeaderInfo qcpHeaderInfo;
        QCPFormatType qcpFormatType;

    private:

        QCPErrorType DecodeQCPHeader(PVFile *fpUsed,
                                     QCPHeaderInfo &aQCPHeaderInfo);

        /**
        * @brief Checks for the valide QCP header
        *
        * @param fmt
        * @param qpl_info
        * @returns error type.
        */
        QCPErrorType IsValidQCPHeader(FmtChunk1 *fmt,
                                      CodecInfo *qpl_info);

        /**
        * @brief Checks valide Codec IDs
        *
        * @param codec_id
        * @param codec_id_ref
        * @returns error type.
        */
        QCPErrorType IsValidCodecID(GUID *codec_id,
                                    GUID *codec_id_ref);

        /**
        * @brief Reads in byte data, taking the most significant byte first.
        *
        * @param fp Pointer to file to read from
        * @param length Number of bytes to read
        * @param data Data read
        * @returns True if read is successful; False otherwise
        */
        bool readByteData(PVFile *fp, uint32 length, uint8 *data, uint32* numbytes = NULL);

        int32 iNumberOfFrames;
        QCPHeaderInfo iQCPHeaderInfo;
};

#endif // #ifdef QCPPARSER_H_INCLUDED
