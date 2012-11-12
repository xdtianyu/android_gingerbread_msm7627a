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
//
//                       Q C P   P A R S E R
//
// = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =


/**
 *  @file qcpparser.cpp
 *  @brief This file contains the implementation of the actual QCP
 *  file parser.
 */
/***********************************************************************
 * Include Files
 ***********************************************************************/
#include "qcpparser.h"

#include "oscl_mem.h"
#include "oscl_stdstring.h"
#include "oscl_utf8conv.h"

/***********************************************************************
 * Constant Defines
 ***********************************************************************/

/***********************************************************************
 * Global constant definitions
 ***********************************************************************/
extern GUID codec_id[3];

/***********************************************************************
 * FUNCTION:    Constructor
 ***********************************************************************/
QCPParser::QCPParser(PVFile* aFileHandle)
{
    fp = aFileHandle;
    // initialize all member variables
    iLocalFileSize = 0;
    iLocalFileSizeSet = false;
    qcpFormatType = QCP_FORMAT_UNKNOWN;
    iNumberOfFrames = 0;
    // duration values from various sources
    iClipDurationInMsec = 0;

    oscl_memset(&iQCPHeaderInfo, 0, sizeof(iQCPHeaderInfo));
}


/***********************************************************************
 * FUNCTION:    Destructor
 ***********************************************************************/
QCPParser::~QCPParser()
{
    // The File Pointer is only used. FileHandles are opened and closed
    // as required.
    fp = NULL;
    iClipDurationInMsec = 0;
    iLocalFileSize = 0;
    iNumberOfFrames = 0;
    qcpFormatType = QCP_FORMAT_UNKNOWN;

    oscl_memset(&iQCPHeaderInfo, 0, sizeof(iQCPHeaderInfo));
}

/***********************************************************************
 * FUNCTION:    ParseQCPFile
 * DESCRIPTION: Used in Media scanner to scan the given format.
 *		This function MUST be called after the Constructor and before
 *				any other public function is called. Otherwise the object's
 *				member data will be uninitialized.
 ***********************************************************************/
QCPErrorType QCPParser::ParseQCPFile(PVFile * fpUsed)
{
    QCPErrorType errCode = QCP_SUCCESS;
    //init members
    fp = fpUsed;

    iLocalFileSize = 0;
    iLocalFileSizeSet = false;
    iNumberOfFrames = 0;

    oscl_memset(&iQCPHeaderInfo, 0, sizeof(iQCPHeaderInfo));

    fp->Seek(0, Oscl_File::SEEKSET);

    if (!iLocalFileSizeSet)
    {
        // seek to the end of the cfile and get the file size
        int res = fp->Seek(0, Oscl_File::SEEKEND);
        if (res == 0)   //success
        {
            iLocalFileSize = fp->Tell();
            iLocalFileSizeSet = true;
            if (iLocalFileSize == 0)
            {
                return QCP_END_OF_FILE;
            }
        }
        else
        {
            iLocalFileSize = 0;
            return QCP_ERROR_UNKNOWN;
        }
    }

    int32 err = fp->Seek(0, Oscl_File::SEEKSET);
    if (0 != err)
    {
        return QCP_ERROR_UNKNOWN;
    }

    /**
     * If we don't find a valid QCP Header point we will attempt recovery.
     * return err code
     **/
    errCode = DecodeQCPHeader(fp, iQCPHeaderInfo);
    if (QCP_SUCCESS != errCode)
    {
        return errCode;
    }

    return QCP_SUCCESS;
}

/***********************************************************************
 *	Function : IsQcpFile
 *	Purpose  : Used in QCP recognizer to recognize the QCP format.
 *		   Verifies whether the file passed in is a possibly
 *			   valid qcp clip
 *  	Input	 : aFile, file to check
 * 	Output	 : None
 *	Return   : error code
 ***********************************************************************/
QCPErrorType QCPParser::IsQcpFile(QCP_FF_FILE* aFile)
{
    QCPErrorType errCode = QCP_SUCCESS;
    iLocalFileSizeSet = true;

    // get the file pointer
    fp = &(aFile->_pvfile);

    // try to retrieve the file size
    int32 err = fp->Seek(0, Oscl_File::SEEKEND);
    if (err == 0)
    {
        iLocalFileSize =  fp->Tell();
        if (iLocalFileSize == 0)
        {
            return QCP_END_OF_FILE;
        }
    }
    else
    {
        iLocalFileSize = 0;
    }

    // seek to the begining position in the file
    fp->Seek(0, Oscl_File::SEEKSET);

    errCode = DecodeQCPHeader(fp, iQCPHeaderInfo);
    if (QCP_SUCCESS != errCode)
    {
        return errCode;
    }

    // qcp header was valid it is an qcp clip
    // return success.
    return QCP_SUCCESS;
}

/***********************************************************************
 * FUNCTION:    DecodeQCPHeader
 * DESCRIPTION: Parse Header Bit fields into a structure
 *                  Validate version ranges and Format specific IDs
 ***********************************************************************/
QCPErrorType QCPParser::DecodeQCPHeader(PVFile *fpUsed,
                                        QCPHeaderInfo &aQCPHeaderInfo)
{
    QCPErrorType errCode = QCP_SUCCESS;
    uint32 cur_pos = 0;
    uint8 pQcpHeaderBuf[QCP_HEADER_SIZE];
    uint8 *pBuf = pQcpHeaderBuf;
    RIFFChunk *riff;
    FmtChunk1 *fmt;
    CodecInfo *qpl_info;
    VratChunk *vrat_chunk;
    DataChunk *data_chunk;

    fp = fpUsed;

    riff = &aQCPHeaderInfo.riff;
    fmt = &aQCPHeaderInfo.fmt.fmt1;
    qpl_info = &aQCPHeaderInfo.fmt.qpl_info;
    vrat_chunk = &aQCPHeaderInfo.vrat_chunk;
    data_chunk = &aQCPHeaderInfo.data_chunk;

    /******* Begin QCP header Parsing *******/

    // Read data from file
    if (!readByteData(fp, QCP_HEADER_SIZE, (uint8 *)pBuf))
    {
        errCode = QCP_INSUFFICIENT_DATA;
        return errCode;
    }

    // RIFF chunk
    oscl_memcpy(riff, pBuf, QCP_RIFF_CHUNK_LENGTH);
    cur_pos += QCP_RIFF_CHUNK_LENGTH;

    // FMT chunk
    oscl_memcpy(fmt, &pBuf[cur_pos], QCP_FMT1_CHUNK_LENGTH);
    cur_pos += QCP_FMT1_CHUNK_LENGTH;

    // QPL (Codec) info
    oscl_memcpy(qpl_info, &pBuf[cur_pos], QCP_CODEC_INFO_LENGTH);
    cur_pos += QCP_CODEC_INFO_LENGTH;

    if (qpl_info->vr_num_of_rates)
    {
        // Read Variable bitrate chunk (Vrat chunk) from file
        if (!readByteData(fp, QCP_VRAT_CHUNK_LENGTH, (uint8 *)pBuf))
        {
            errCode = QCP_INSUFFICIENT_DATA;
            return errCode;
        }

        // Vrat chunk info
        oscl_memcpy(vrat_chunk, pBuf, QCP_VRAT_CHUNK_LENGTH);
    }

    /******* End of QCP header Parsing *******/

    errCode = IsValidQCPHeader(fmt, qpl_info);
    if(QCP_SUCCESS != errCode)
    {
        return errCode;
    }

    // Read DATA chunk from file
    if (!readByteData(fp, QCP_DATA_CHUNK_LENGTH, (uint8 *)pBuf))
    {
        errCode = QCP_INSUFFICIENT_DATA;
        return errCode;
    }

    // Copy DATA chunk info
    oscl_memcpy(data_chunk, pBuf, QCP_DATA_CHUNK_LENGTH);

    if (qpl_info->vr_num_of_rates)
    {
        iNumberOfFrames = vrat_chunk->size_in_pkts;
    }
    else
    {
        iNumberOfFrames = data_chunk->s_data/qpl_info->bytes_per_pkt;
    }

    iClipDurationInMsec = GetDuration();

    return errCode;
}

/***********************************************************************
 *      Function : IsValidQCPHeader
 *      Purpose  : Validates the QCP header
 *  	Input    :
 *  	Outpu    :
 *      Return   :
 **********************************************************************/
QCPErrorType QCPParser::IsValidQCPHeader(FmtChunk1 *fmt,
                                         CodecInfo *qpl_info)
{
    QCPErrorType errCode = QCP_SUCCESS;

    // EVRC Unique ID & version number validation
    errCode = IsValidCodecID(&qpl_info->codec_id, &codec_id[0]);
    if((QCP_SUCCESS == errCode) &&
       (qpl_info->ver == 1))
    {
        qcpFormatType = QCP_FORMAT_EVRC;
    }
    else
    {	// QCELP Unique ID & version number validation
        errCode = IsValidCodecID(&qpl_info->codec_id, &codec_id[1]);
        if((QCP_SUCCESS == errCode) &&
           ((qpl_info->ver == 1) || (qpl_info->ver == 2)))
        {
            qcpFormatType = QCP_FORMAT_QCELP;
        }
        else
        {
            errCode = IsValidCodecID(&qpl_info->codec_id, &codec_id[2]);
            if((QCP_SUCCESS == errCode) &&
               ((qpl_info->ver == 1) || (qpl_info->ver == 2)))
            {
                qcpFormatType = QCP_FORMAT_QCELP;
            }
            else
            {
                qcpFormatType = QCP_FORMAT_UNKNOWN;
                errCode = QCP_ERROR_UNKNOWN_FORMAT;
            }
        }
    }

    return errCode;
}

/***********************************************************************
 *      Function : IsValidCodecID
 *      Purpose  : Validates the QCP Codec ID
 *  	Input    :
 *  	Output   :
 *      Return   :
 **********************************************************************/
QCPErrorType QCPParser::IsValidCodecID(GUID *codec_id,
                                       GUID *codec_id_ref)
{
    QCPErrorType errCode = QCP_ERROR_UNKNOWN;

    // QCELP & EVRC Unique ID validation
    if ((codec_id->data1    == codec_id_ref->data1) &&
        (codec_id->data2    == codec_id_ref->data2) &&
        (codec_id->data3    == codec_id_ref->data3) &&
        (codec_id->data4[0] == codec_id_ref->data4[0]) &&
        (codec_id->data4[1] == codec_id_ref->data4[1]) &&
        (codec_id->data4[2] == codec_id_ref->data4[2]) &&
        (codec_id->data4[3] == codec_id_ref->data4[3]) &&
        (codec_id->data4[4] == codec_id_ref->data4[4]) &&
        (codec_id->data4[5] == codec_id_ref->data4[5]) &&
        (codec_id->data4[6] == codec_id_ref->data4[6]) &&
        (codec_id->data4[7] == codec_id_ref->data4[7]))
    {
        errCode = QCP_SUCCESS;
    }

    return errCode;
}

/***********************************************************************
 *      Function : GetDuration
 *      Purpose  : Fetches duration of the clip playing
 *                         Duration is returned, by different
 *                         means by the pre-defined priorities
 *  	Input    : 
 *  	Output   : 
 *      Return   : Clip duration
 **********************************************************************/
uint32 QCPParser::GetDuration()
{
    iClipDurationInMsec = iNumberOfFrames*TIME_STAMP_PER_FRAME; // each frame 20ms, 160/8000 = 20ms
    return iClipDurationInMsec;
}

/***********************************************************************
 *      Function : readByteData
 *      Purpose  : Read in byte data and take most significant byte first
 *      Input    : 
 *      Output   : 
 *      Return   : 
 ***********************************************************************/
bool QCPParser::readByteData(PVFile *fp, uint32 length, uint8 *data, uint32 *numbytes)
{
    uint32 bytesRead;
    bytesRead = fp->Read(data, 1, length);

    if (numbytes)
        *numbytes = bytesRead;

    if (bytesRead < (uint32)length) // read byte data failed
    {
        int32 seekback = 0;
        // bytes to seek backwards
        seekback -= (int32) bytesRead;
        fp->Seek(seekback, Oscl_File::SEEKCUR);
        return false;
    }

    return true;
}

