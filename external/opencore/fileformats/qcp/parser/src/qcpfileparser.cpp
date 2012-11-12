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
 *  @file qcpfileparser.cpp
 *  @brief This file contains the implementation of the raw GSM-QCP file parser.
 */

//----------------------------------------------------------------------------
// INCLUDES
//----------------------------------------------------------------------------
#include "qcpfileparser.h"

// Use default DLL entry point for Symbian
#include "oscl_dll.h"
OSCL_DLL_ENTRY_POINT_DEFAULT()


/* Codec IDs for QCELP/EVRC */
GUID codec_id[3] =
{
    // EVRC ID
    {0xE689D48D, 0x9076, 0x46b5, {0x91, 0xEF, 0x73, 0x6A, 0x51, 0x00, 0xCE, 0xB4}},
    // QCELP IDs
    {0x5E7F6D41, 0xB115, 0x11D0, {0xBA, 0x91, 0x00, 0x80, 0x5F, 0xB4, 0xB9, 0x7E}},
    {0x5E7F6D42, 0xB115, 0x11D0, {0xBA, 0x91, 0x00, 0x80, 0x5F, 0xB4, 0xB9, 0x7E}},
};

/***********************************************************************
 *      Function : QcpBitstreamObject::reset()
 *      Purpose  : Re-position the file pointer, Specially used in 
 *                 ResetPlayback()
 *      Input    :
 *      Output   :
 *      Return   :
 **********************************************************************/
int32 QcpBitstreamObject::reset(int32 filePos)
{
    iFrame_type = 0;
    iBytesRead  = iInitFilePos + filePos; // set the initial value
    iBytesProcessed = iBytesRead;
    if (ipQCPFile)
    {
        ipQCPFile->Seek(iInitFilePos + filePos, Oscl_File::SEEKSET);
    }
    iPos = QcpBitstreamObject::MAIN_BUFF_SIZE + QcpBitstreamObject::SECOND_BUFF_SIZE;
    return refill();
}

/***********************************************************************
 *      Function : QcpBitstreamObject::refill()
 *      Purpose  : Reads data from bitstream
 *      Input    :
 *      Output   :
 *      Return   :
 **********************************************************************/
int32 QcpBitstreamObject::refill()
{
    PVMF_QCPPARSER_LOGDEBUG((0, "Refill In ipos=%d, iBytesRead=%d, iBytesProcessed=%d, iActualSize=%d, iFileSize=%d", iPos, iBytesRead, iBytesProcessed, iActual_size, iFileSize));

    if (iBytesRead > 0 && iFileSize > 0 && iBytesRead >= iFileSize)
    {
        // if number of bytes read so far exceed the file size,
        // then first update the file size (PDL case).
        if (!UpdateFileSize()) return QcpBitstreamObject::MISC_ERROR;

        //At this point we're within 32 bytes of the end of data.
        //Quit reading data but don't return EOF until all data is processed.
        if (iBytesProcessed < iBytesRead)
        {
            return QcpBitstreamObject::EVERYTHING_OK;
        }
        else
        {
            //there is no more data to read.
            if (iBytesRead > iFileSize || iBytesProcessed > iFileSize)
                return QcpBitstreamObject::DATA_INSUFFICIENT;
            if (iBytesRead == iFileSize || iBytesProcessed == iFileSize)
                return QcpBitstreamObject::EVERYTHING_OK;
        }
    }

    if (!ipQCPFile)
    {
        return QcpBitstreamObject::MISC_ERROR;
    }

    // Get file size at the very first time
    if (iFileSize == 0)
    {
        if (ipQCPFile->Seek(0, Oscl_File::SEEKEND))
        {
            return QcpBitstreamObject::MISC_ERROR;
        }

        iFileSize = ipQCPFile->Tell();

        if (iFileSize <= 0)
        {
            return QcpBitstreamObject::MISC_ERROR;
        }

        if (ipQCPFile->Seek(0, Oscl_File::SEEKSET))
        {
            return QcpBitstreamObject::MISC_ERROR;
        }

        // first-time read, set the initial value of iPos
        iPos = QcpBitstreamObject::SECOND_BUFF_SIZE;
        iBytesProcessed = 0;
    }
    // we are currently positioned at the end of the data buffer.
    else if (iPos == QcpBitstreamObject::MAIN_BUFF_SIZE + QcpBitstreamObject::SECOND_BUFF_SIZE)
    {
        // reset iPos and refill from the beginning of the buffer.
        iPos = QcpBitstreamObject::SECOND_BUFF_SIZE;
    }

    else if (iPos >= iActual_size)
    {
        int32 len = 0;
        // move the remaining stuff to the beginning of iBuffer
        if (iActual_size + QcpBitstreamObject::SECOND_BUFF_SIZE > iPos)
        {
            // we are currently positioned within SECOND_BUFF_SIZE bytes from the end of the buffer.
            len = iActual_size + QcpBitstreamObject::SECOND_BUFF_SIZE - iPos;
        }
        else
        {
            // no leftover data.
            len = 0;
        }

        oscl_memcpy(&iBuffer[QcpBitstreamObject::SECOND_BUFF_SIZE-len], &iBuffer[iPos], len);
        iPos = QcpBitstreamObject::SECOND_BUFF_SIZE - len;

        // update the file size for the PDL scenario where more data has been downloaded
        // into the file but the file size has not been updated yet.
        if (iBytesRead + iMax_size > iFileSize)
        {
            if (!UpdateFileSize()) return QcpBitstreamObject::MISC_ERROR;
        }
    }

    // read data
    if ((iActual_size = ipQCPFile->Read(&iBuffer[QcpBitstreamObject::SECOND_BUFF_SIZE], 1, iMax_size)) == 0)
    {
        return QcpBitstreamObject::READ_ERROR;
    }

    iBytesRead += iActual_size;

    PVMF_QCPPARSER_LOGDEBUG((0, "Refill Out ipos=%d, iBytesRead=%d, iBytesProcessed=%d, iActualSize=%d, iFileSize=%d", iPos, iBytesRead, iBytesProcessed, iActual_size, iFileSize));

    return QcpBitstreamObject::EVERYTHING_OK;
}

/***********************************************************************
 *      Function : QcpBitstreamObject::getNextFrame()
 *      Purpose  : Used to get one frame of QCP data, used in 
 *		   getNextBundledAccessUnits()
 *      Input    :
 *      Output   :
 *      Return   :
 **********************************************************************/
int32 QcpBitstreamObject::getNextFrame(uint8* frameBuffer, uint8& frame_length, bool bHeaderIncluded)
{
    PVMF_QCPPARSER_LOGDEBUG((0, "GetNextFrame In ipos=%d, iBytesRead=%d, iBytesProcessed=%d, iActualSize=%d, iFileSize=%d", iPos, iBytesRead, iBytesProcessed, iActual_size, iFileSize));
    if (!frameBuffer)
    {
        return QcpBitstreamObject::MISC_ERROR;
    }

    int32 ret_value = QcpBitstreamObject::EVERYTHING_OK;

    // Need to refill?
    if (iFileSize == 0 || iPos >= iActual_size)
    {
        ret_value = refill();
        if (ret_value)
        {
            return ret_value;
        }
    }

    /* Decode till the end of Data length, leave any data present after that */
    if (iBytesProcessed >= iFileSizeToDecode)
    {
        return QcpBitstreamObject::END_OF_FILE;
    }

    int32 i, bitrate = 0;
    int32 frame_size = 0;
    uint8 *pBuffer = &iBuffer[iPos];

    if(iNumOfVBRs)
    {
        bitrate = (uint8)pBuffer[0];
        for (i = 0; i < iNumOfVBRs; i++)
        {
            if (bitrate == iBitrates[i])
            {
                frame_size = iPacketSizes[i];
                break;
            }
            else if (i == (iNumOfVBRs-1))
            {	
                return QcpBitstreamObject::MISC_ERROR;
            }
        }
        if (frame_size < 0)
        {
            return QcpBitstreamObject::MISC_ERROR;
        }
        else
        {
            if (bHeaderIncluded)
            {
                oscl_memcpy(frameBuffer, &pBuffer[0], frame_size + 1);   // With frame header
            }
            else
            {
                oscl_memcpy(frameBuffer, &pBuffer[1], frame_size); // NO frame header
            }
        }
        frame_size += 1;
    }
    else
    {
        frame_size = iFrameLengthInBytes;
        oscl_memcpy(frameBuffer, &pBuffer[0], frame_size);
    }

    iPos += frame_size;
    iBytesProcessed += frame_size;
    frame_length = frame_size;

    PVMF_QCPPARSER_LOGDEBUG((0, "GetNextFrame Out ipos=%d, iBytesRead=%d, iBytesProcessed=%d, iActualSize=%d, iFileSize=%d", iPos, iBytesRead, iBytesProcessed, iActual_size, iFileSize));
    return ret_value;
}

/***********************************************************************
 *      Function : QcpBitstreamObject::parseQCPHeader()
 *      Purpose  : Parses QCP file header and get format type(QCELP/EVRC)
 *      Input    :
 *      Output   :
 *      Return   :
 **********************************************************************/
int32 QcpBitstreamObject::parseQCPHeader()
{
    int32 returnValue = reset();
    uint32 i, cur_pos = 0;
    RIFFChunk *riff;
    FmtChunk1 *fmt;
    CodecInfo *qpl_info;
    VratChunk *vrat_chunk;
    DataChunk *data_chunk;

    riff = &iQCPHeaderInfo.riff;
    fmt = &iQCPHeaderInfo.fmt.fmt1;
    qpl_info = &iQCPHeaderInfo.fmt.qpl_info;
    vrat_chunk = &iQCPHeaderInfo.vrat_chunk;
    data_chunk = &iQCPHeaderInfo.data_chunk;

    if (returnValue == QcpBitstreamObject::EVERYTHING_OK)
    {
        iQcpFormat = QCP_FORMAT_UNKNOWN;

        // first time read, we don't use iSecond_buffer
        uint8 *pBuf = &iBuffer[iPos];
	
        if (iActual_size > QCP_HEADER_SIZE)
        {
            // RIFF chunk
            oscl_memcpy(riff, pBuf, QCP_RIFF_CHUNK_LENGTH);
            cur_pos += QCP_RIFF_CHUNK_LENGTH;

            // FMT chunk
            oscl_memcpy(fmt, &pBuf[cur_pos], QCP_FMT1_CHUNK_LENGTH);
            cur_pos += QCP_FMT1_CHUNK_LENGTH;

            // QPL (Codec) info
            oscl_memcpy(qpl_info, &pBuf[cur_pos], QCP_CODEC_INFO_LENGTH);
            cur_pos += QCP_CODEC_INFO_LENGTH;

            // Vrat chunk info
            oscl_memcpy(vrat_chunk, &pBuf[cur_pos], QCP_VRAT_CHUNK_LENGTH);
            cur_pos +=  QCP_VRAT_CHUNK_LENGTH;

            // If parsed chunk is not VRAT chunk, put the offset back
            if(vrat_chunk->vrat[0] != 'v' && vrat_chunk->vrat[1] != 'r' &&
               vrat_chunk->vrat[2] != 'a' && vrat_chunk->vrat[3] != 't'){
                cur_pos -=  QCP_VRAT_CHUNK_LENGTH;
                qpl_info->vr_num_of_rates = 0;
            }

            /******* End of QCP header Parsing *******/

            returnValue = IsValidQCPHeader(fmt, qpl_info);
            if(EVERYTHING_OK != returnValue)
            {
                return returnValue;
            }

            // Read DATA chunk from file
            oscl_memcpy(data_chunk, &pBuf[cur_pos], QCP_DATA_CHUNK_LENGTH);
            cur_pos +=  QCP_DATA_CHUNK_LENGTH;

            if (qpl_info->vr_num_of_rates)
            {
                iNumOfVBRs = qpl_info->vr_num_of_rates;
                iNumberOfFrames = vrat_chunk->size_in_pkts;
                for (i = 0; i < qpl_info->vr_num_of_rates; i++)
                {
                    iBitrates[i] = (qpl_info->vr_bytes_per_pkt[i] >> 8);
                    iPacketSizes[i] = (qpl_info->vr_bytes_per_pkt[i] & 0xFF);
                }
            }
            else
            {
                iNumOfVBRs = 0;
                iNumberOfFrames = data_chunk->s_data/qpl_info->bytes_per_pkt;
            }

            iDuration = GetDuration();

            // Get QCP header parameters
            iBitRate            = qpl_info->abps;
            iSamplingRate       = qpl_info->samp_per_sec;
            iFrameLengthInBytes = qpl_info->bytes_per_pkt;
        }
	
        iInitFilePos = cur_pos;
        iPos += iInitFilePos;
        iBytesProcessed += iInitFilePos;
        iFileSizeToDecode = iInitFilePos + iQCPHeaderInfo.data_chunk.s_data;
    }

    return returnValue;
}

/***********************************************************************
 *      Function : QcpBitstreamObject::IsValidQCPHeader()
 *      Purpose  : Validates the parsed QCP header
 *      Input    :
 *      Output   :
 *      Return   :
 **********************************************************************/
int32 QcpBitstreamObject::IsValidQCPHeader(FmtChunk1 *fmt,
                                           CodecInfo *qpl_info)
{
    int32 returnValue = EVERYTHING_OK;

    // EVRC Unique ID & version number validation
    returnValue = IsValidCodecID(&qpl_info->codec_id, &codec_id[0]);
    if((EVERYTHING_OK == returnValue) &&
       (qpl_info->ver == 1))
    {
        iQcpFormat = QCP_FORMAT_EVRC;
    }
    else
    {	// QCELP Unique ID & version number validation
        returnValue = IsValidCodecID(&qpl_info->codec_id, &codec_id[1]);
        if((EVERYTHING_OK == returnValue) &&
           ((qpl_info->ver == 1) || (qpl_info->ver == 2)))
        {
            iQcpFormat = QCP_FORMAT_QCELP;
        }
        else
        {
            returnValue = IsValidCodecID(&qpl_info->codec_id, &codec_id[2]);
            if((EVERYTHING_OK == returnValue) &&
               ((qpl_info->ver == 1) || (qpl_info->ver == 2)))
            {
                iQcpFormat = QCP_FORMAT_QCELP;
            }
            else
            {
                iQcpFormat = QCP_FORMAT_UNKNOWN;
                returnValue = MISC_ERROR;
            }
        }
    }

    return returnValue;
}

/***********************************************************************
 *      Function : QcpBitstreamObject::IsValidCodecID()
 *      Purpose  : Validates the QCP Codec ID
 *      Input    :
 *      Output   :
 *      Return   :
 **********************************************************************/
int32 QcpBitstreamObject::IsValidCodecID(GUID *codec_id,
                                         GUID *codec_id_ref)
{
    int32 returnValue = MISC_ERROR;

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
        returnValue = EVERYTHING_OK;
    }

    return returnValue;
}

/***********************************************************************
 *	Function : QcpBitstreamObject::GetDuration()
 *	Purpose  : Fetches duration of the clip playing
 *      Input	 : 
 *      Output	 : 
 *	Return   : Clip duration
 **********************************************************************/
uint32 QcpBitstreamObject::GetDuration()
{
    int32 clip_duration = 0;	
    clip_duration = iNumberOfFrames*TIME_STAMP_PER_FRAME; // each frame 20ms, 160/8000 = 20
    return clip_duration;
}

/***********************************************************************
 *      Function : QcpBitstreamObject::getFileInfo()
 *      Purpose  : Get clip information: file size, format(QCLP13K or EVRC) 
 *                 and bitrate
 *      Input    :
 *      Output   :
 *      Return   : 
 **********************************************************************/
int32 QcpBitstreamObject::getFileInfo(int32& fileSize, int32& format, int32& bitrate)
{
    fileSize = format = 0;
    int32 ret_value = QcpBitstreamObject::EVERYTHING_OK;
    if (iFileSize == 0)
    {
        ret_value = parseQCPHeader();
        if (ret_value)
        {
            return ret_value;
        }
    }

    fileSize = iFileSize;
    format = iQcpFormat;
    bitrate = iBitRate;
    return ret_value;
}

//! get the updated file size
bool QcpBitstreamObject::UpdateFileSize()
{
    if (ipQCPFile != NULL)
    {
        uint32 aRemBytes = 0;
        if (ipQCPFile->GetRemainingBytes(aRemBytes))
        {
            uint32 currPos = (uint32)(ipQCPFile->Tell());
            iFileSize = currPos + aRemBytes;
            return true;
        }
    }
    return false;
}

/***********************************************************************
 *      Function : CQCPFileParser::CQCPFileParser()
 *      Purpose  : Constructor for CQCPFileParser class
 *      Input    :
 *      Output   :
 *      Return   :
 **********************************************************************/
OSCL_EXPORT_REF CQCPFileParser::CQCPFileParser(void)
{
    iQCPDuration        = -1;
    iQCPBitRate         = 0;
    iTotalNumFramesRead = 0;
    iQCPFormat          = QCP_FORMAT_UNKNOWN;
    iEndOfFileReached   = false;
    iRandomAccessTimeInterval = 0;
    iCountToClaculateRDATimeInterval = 0;
    iLogger = PVLogger::GetLoggerObject("pvqcp_parser");
    iDiagnosticLogger = PVLogger::GetLoggerObject("playerdiagnostics.pvqcp_parser");

    ipBSO = NULL;
}

/***********************************************************************
 *      Function : CQCPFileParser::~CQCPFileParser
 *      Purpose  : Destructor for CQCPFileParser class
 *      Input    :
 *      Output   :
 *      Return   :
 **********************************************************************/
OSCL_EXPORT_REF CQCPFileParser::~CQCPFileParser(void)
{
    iQCPFile.Close();
    OSCL_DELETE(ipBSO);
    ipBSO = NULL;
}

/***********************************************************************
 *      Function : CQCPFileParser::InitQCPFile()
 *      Purpose  : This function opens the QCP file, checks for QCP format 
 *                 type, calculates the track duration, and sets the QCP 
 *		   bitrate value
 *
 *      Input    : iClip = pointer to the QCP file name to be played 
 *                 of type TPtrC
 *      Output   : 
 *      Return   : returnValue = true if the init succeeds, else false
 **********************************************************************/
OSCL_EXPORT_REF bool CQCPFileParser::InitQCPFile(OSCL_wString& aClip, bool aInitParsingEnable, Oscl_FileServer* aFileSession, PVMFCPMPluginAccessInterfaceFactory*aCPM, OsclFileHandle*aHandle, uint32 countToClaculateRDATimeInterval)
{
    iQCPFile.SetCPM(aCPM);
    iQCPFile.SetFileHandle(aHandle);

    // Open the file (aClip)
    if (iQCPFile.Open(aClip.get_cstr(), (Oscl_File::MODE_READ | Oscl_File::MODE_BINARY), *aFileSession) != 0)
    {
        PVMF_QCPPARSER_LOGERROR((0, "CQCPFileParser::InitQCPFile- File Open failed"));
        return false;
    }

    // create ipBSO
    ipBSO = OSCL_NEW(QcpBitstreamObject, (iLogger, &iQCPFile));
    if (!ipBSO)
    {
        return false;
    }
    if (!ipBSO->get())
    {
        return false; // make sure the memory allocation is going well
    }

    // get file info
    if (ipBSO->getFileInfo(iQCPFileSize, iQCPFormat, iQCPBitRate))
    {
        PVMF_QCPPARSER_LOGERROR((0, "CQCPFileParser::InitQCPFile- getFileInfo failed "));
        return false;
    }

    // Reject unsupported QCP file types
    if ( iQCPFormat != QCP_FORMAT_QCELP &&
         iQCPFormat != QCP_FORMAT_EVRC)
    {
        PVMF_QCPPARSER_LOGERROR((0, "CQCPFileParser::Unsupported QCP type "));
        return false;
    }

    // Determine file duration and set up random positioning table if needed
    CalculateDuration(aInitParsingEnable, countToClaculateRDATimeInterval);
    return true;
}

/***********************************************************************
 *      Function : CalculateDuration()
 *      Purpose  : Fetches duration of the clip playing
 *      Input    :
 *      Output   :
 *      Return   : 
 **********************************************************************/
bool CQCPFileParser::CalculateDuration(bool aInitParsingEnable, uint32 countToClaculateRDATimeInterval)
{
    iCountToClaculateRDATimeInterval = countToClaculateRDATimeInterval;
    uint32 FrameCount = iCountToClaculateRDATimeInterval;
    iRandomAccessTimeInterval = countToClaculateRDATimeInterval * TIME_STAMP_PER_FRAME;

    if (aInitParsingEnable)
    {
        // Go through each frame to calculate QCP file duration.
        int32 status = QcpBitstreamObject::EVERYTHING_OK;
        iQCPDuration = 0;

        int32 error = 0;
        int32 filePos = 0;
	uint8 frame_length = 0;

        OSCL_TRY(error, iRPTable.push_back(filePos));
        OSCL_FIRST_CATCH_ANY(error, return false);

        while (status == QcpBitstreamObject::EVERYTHING_OK)
        {
            // get the next frame

            status = ipBSO->getNextFrame(iQCPFrameBuffer, frame_length); // NO QCP frame header

            if (status == QcpBitstreamObject::EVERYTHING_OK)
            {
                // calculate the number of frames // BX
                iQCPDuration += TIME_STAMP_PER_FRAME;

                // set up the table for randow positioning

                filePos += frame_length;

                error = 0;
                if (!FrameCount)
                {
                    OSCL_TRY(error, iRPTable.push_back(filePos));
                    OSCL_FIRST_CATCH_ANY(error, return false);
                    FrameCount = countToClaculateRDATimeInterval;
                }
            }

            else if (status == QcpBitstreamObject::END_OF_FILE)
            {
                break;
            }

            else
            {
                PVMF_QCPPARSER_LOGERROR((0, "CQCPFileParser::getNextFrame Fails Error Code %d", status));
                if (ipBSO->reset())
                {
                    return false;
                }

                return false;
            }
            FrameCount--;
        }

        ResetPlayback(0);
    }
    return true;
}

/***********************************************************************
 *      Function : CQCPFileParser::RetreiveQCPFileInfo()
 *      Purpose  : This function opens the QCP file, checks for QCP format 
 *                 type, calculates the track duration, and sets the QCP  
 *                 bitrate value
 *      Input    :
 *      Output   :
 *      Return   : false if an error happens, else true
 **********************************************************************/
OSCL_EXPORT_REF bool CQCPFileParser::RetrieveFileInfo(TPVQcpFileInfo& aInfo)
{
    if (iQCPFormat == QCP_FORMAT_UNKNOWN)
    {
        // File is not open and parsed
        return false;
    }

    aInfo.iBitrate = iQCPBitRate;
    aInfo.iTimescale = 1000;
    aInfo.iDuration = iQCPDuration;
    aInfo.iFileSize = iQCPFileSize;
    aInfo.iQcpFormat = iQCPFormat;
    PVMF_QCPPARSER_LOGDIAGNOSTICS((0, "CQCPFileParser::RetrieveFileInfo- duration = %d, bitrate = %d, filesize = %d", iQCPDuration, iQCPBitRate, iQCPFileSize));

    return true;
}

/***********************************************************************
 *      Function : CQCPFileParser::ResetPlayback()
 *      Purpose  : This function sets the file pointer to the location that aStartTime would
 *  		   point to in the file
 *      Input    :
 *      Output   :
 *      Return   : 0 if success, -1 if failure
 **********************************************************************/
OSCL_EXPORT_REF int32 CQCPFileParser::ResetPlayback(int32 aStartTime)
{
    // get file size info, //iQCPFile.Size(fileSize)
    if (iQCPFileSize <= 0)
    {
        if (ipBSO->getFileInfo(iQCPFileSize, iQCPFormat, iQCPBitRate))
        {
            PVMF_QCPPARSER_LOGERROR((0, "CQCPFileParser::Reset Playback Failed"));
            return QcpBitstreamObject::MISC_ERROR;
        }
    }

    iEndOfFileReached = false;
    // initialize "iTotalNumFramesRead"
    // note: +1 means we choose the next frame(ts>=aStartTime)
    iTotalNumFramesRead = aStartTime / TIME_STAMP_PER_FRAME + (aStartTime > 0) * 1;

    uint32 tblIdx = aStartTime / (iRandomAccessTimeInterval);// +(aStartTime>0)*1;
    iTotalNumFramesRead = tblIdx * iCountToClaculateRDATimeInterval;

    PVMF_QCPPARSER_LOGDIAGNOSTICS((0, "CQCPFileParser::resetplayback - TotalNumFramesRead=%d", iTotalNumFramesRead));
    // set new file position
    int32 newPosition = 0;
    if (iTotalNumFramesRead > 0)
    {
        // At the first time, don't do reset
        if (iQCPDuration != 0 && iRPTable.size() == 0)
        {
            newPosition = (iQCPFileSize * aStartTime) / iQCPDuration;
            PVMF_QCPPARSER_LOGDIAGNOSTICS((0, "CQCPFileParser::resetplayback - newPosition=%d", newPosition));
            if (newPosition < 0)
            {
                // if we have no duration information, reset the file position at 0.
                newPosition = 0;
            }
        }
        else if (iRPTable.size() > 0)
        {
            // use the randow positioning table to determine the file position
            if (tblIdx  >= iRPTable.size())
            {
                // Requesting past the end of table so set to (end of table-1)
                // to be at the last sample
                tblIdx = ((int32)iRPTable.size()) - 2;
            }
            newPosition = iRPTable[tblIdx];
        }
    }

    if (newPosition >= 0 && ipBSO->reset(newPosition))
    {
        PVMF_QCPPARSER_LOGERROR((0, "QCPBitstreamObject::refill- Misc Error"));
        return QcpBitstreamObject::MISC_ERROR;
    }
    iEndOfFileReached = false;

    return QcpBitstreamObject::EVERYTHING_OK;
}

/***********************************************************************
 *      Function : CQCPFileParser::SeekPointFromTimestamp()
 *      Purpose  : This function returns the timestamp for an actual 
 *                 position corresponding to the specified start time
 *      Input    :
 *      Output   :
 *      Return   : Timestamp in milliseconds of the actual position
 **********************************************************************/
OSCL_EXPORT_REF uint32 CQCPFileParser::SeekPointFromTimestamp(uint32 aStartTime)
{
    // get file size info, //iQCPFile.Size(fileSize)
    if (iQCPFileSize <= 0)
    {
        if (ipBSO->getFileInfo(iQCPFileSize, iQCPFormat, iQCPBitRate))
        {
            return 0;
        }
    }

    // Determine the frame number corresponding to timestamp
    // note: +1 means we choose the next frame(ts>=aStartTime)
    uint32 startframenum = aStartTime / TIME_STAMP_PER_FRAME + (aStartTime > 0) * 1;

    // Correct the frame number if necessary
    if (startframenum > 0)
    {
        if (iQCPDuration != 0 && iRPTable.size() <= 0)
        {
            // Duration not known and reposition table not available so go to first frame
            startframenum = 0;
        }
        else if (iRPTable.size() > 0)
        {
            if (startframenum >= iRPTable.size())
            {
                // Requesting past the end of table so set to (end of table-1)
                // to be at the last sample
                startframenum = ((int32)iRPTable.size()) - 2;
            }
        }
    }

    return (startframenum*TIME_STAMP_PER_FRAME);
}

/***********************************************************************
 *      Function : CQCPFileParser::GetNextBundledAccessUnits()
 *      Purpose  : This function attempts to read in the number of QCP 
 *  		   frames specified by aNumSamples. It formats the read 
 *		   data to QCELP/EVRC bit order and stores it in 
 *  		   the GAU structure
 *
 *      Input    : aNumSamples = requested number of frames to be read from file
 *                 aGau        = frame information structure of type GAU
 *      Output   :
 *      Return   : 0 if success, -1 if failure
 **********************************************************************/
OSCL_EXPORT_REF int32 CQCPFileParser::GetNextBundledAccessUnits(uint32 *aNumSamples, GAU *aGau)
{
    // QCP format has already been identified in InitQCPFile function.
    // Check if QCP format is valid as the safeguard
    if (iQCPFormat == QCP_FORMAT_UNKNOWN)
    {
        PVMF_QCPPARSER_LOGERROR((0, "CQCPFileParser::GetNextBundledAccessUnits Failed - Unrecognized format"));
        return QcpBitstreamObject::MISC_ERROR;
    }

    // Check the requested number of frames is not greater than the max supported
    if (*aNumSamples > MAX_NUM_FRAMES_PER_BUFF)
    {
        PVMF_QCPPARSER_LOGERROR((0, "CQCPFileParser::GetNextBundledAccessUnits Failed - requested number of frames is greater than the max supported"));
        return QcpBitstreamObject::MISC_ERROR;
    }

    int32 returnValue = QcpBitstreamObject::EVERYTHING_OK;

    if (iEndOfFileReached)
    {
        *aNumSamples = 0;
        return QcpBitstreamObject::END_OF_FILE;
    }

    uint8* pTempGau = (uint8 *) aGau->buf.fragments[0].ptr;
    uint32 gauBufferSize = aGau->buf.fragments[0].len;
    uint32 i, bytesReadInGau = 0, numSamplesRead = 0;

    for (i = 0; i < *aNumSamples && !iEndOfFileReached; i++)
    {
        // get the next frame
        bool bHeaderIncluded = true;
        returnValue = ipBSO->getNextFrame(iQCPFrameBuffer, iQCPFrameHeaderBuffer[i], bHeaderIncluded);
        if (returnValue == QcpBitstreamObject::END_OF_FILE)
        {
            iEndOfFileReached = true;
            break;
        }
        else if (returnValue == QcpBitstreamObject::EVERYTHING_OK)

        {
        }
        else if (returnValue == QcpBitstreamObject::DATA_INSUFFICIENT)
        {
            *aNumSamples = 0;
            return returnValue;
        }
        else
        {   // error happens!!
            *aNumSamples = 0;
            return QcpBitstreamObject::READ_ERROR;
        }

	// Now a frame exists in iQCPFrameBuffer, move it to aGau
        int32 frame_size = 0;
	frame_size = iQCPFrameHeaderBuffer[i];

        // Check whether the gau buffer will be overflow
        if (bytesReadInGau + frame_size >= gauBufferSize)
        {
            // Undo the read
            ipBSO->undoGetNextFrame(frame_size);
            break;
        }

        if (frame_size > 0)
        {
            oscl_memcpy(pTempGau, iQCPFrameBuffer, frame_size);

            pTempGau += frame_size;
            bytesReadInGau += frame_size;
        }
        aGau->info[i].len = frame_size;
        aGau->info[i].ts  = (iTotalNumFramesRead + (numSamplesRead++)) * TIME_STAMP_PER_FRAME;

    } // end of: for(i = 0; i < *aNumSamples && !iEndOfFileReached; i++)

    aGau->info[0].ts = iTotalNumFramesRead * TIME_STAMP_PER_FRAME;

    *aNumSamples = numSamplesRead;
    iTotalNumFramesRead += numSamplesRead;

    //We may have reached EOF but also found some samples.
    //don't return EOF until there are no samples left.
    if (returnValue == QcpBitstreamObject::END_OF_FILE
            && numSamplesRead > 0)
        return QcpBitstreamObject::EVERYTHING_OK;

    return returnValue;
}

/***********************************************************************
 *      Function : CQCPFileParser::PeekNextTimestamp()
 *      Purpose  : This function attempts to peek next time stamp
 *
 *      Input    : 
 *      Output   :
 *      Return   : 
 **********************************************************************/
OSCL_EXPORT_REF int32 CQCPFileParser::PeekNextTimestamp(uint32 *aTimestamp)
{
    *aTimestamp = iTotalNumFramesRead * TIME_STAMP_PER_FRAME;

    return QcpBitstreamObject::EVERYTHING_OK;
}

/***********************************************************************
 *      Function : CQCPFileParser::getCodecSpecificInfo()
 *      Purpose  : This function attempts to get codec specific information
 *
 *      Input    : 
 *      Output   :
 *      Return   : 
 **********************************************************************/
OSCL_EXPORT_REF uint8* CQCPFileParser::getCodecSpecificInfo()
{

    bool bHeaderIncluded = true;
    ipBSO->getNextFrame(iQCPFrameBuffer, iQCPFrameHeaderBuffer[1], bHeaderIncluded);
    return 	iQCPFrameHeaderBuffer;
}


