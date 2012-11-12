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
//                 Q C P   F I L E   P A R S E R
//
// = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =


/**
 *  @file iqcpff.cpp
 *  @brief This file contains the implementation of the QCP File Format
 *  interface. It initializes and maintains the QCP File Format Library
 */

//----------------------------------------------------------------------
// Include Files
//----------------------------------------------------------------------

#include "iqcpff.h"
#include "qcputils.h"
#include "qcpparser.h"
#include "oscl_mem.h"
#include "oscl_utf8conv.h"
#include "pvmi_kvp_util.h"
#include "pvmi_kvp.h"
#include "pvmf_format_type.h"
#include "pv_mime_string_utils.h"

// Use default DLL entry point for Symbian
#include "oscl_dll.h"
OSCL_DLL_ENTRY_POINT_DEFAULT()

//----------------------------------------------------------------------
// Defines
//----------------------------------------------------------------------

//----------------------------------------------------------------------
// Type Declarations
//----------------------------------------------------------------------

//----------------------------------------------------------------------
// Global Constant Definitions
//----------------------------------------------------------------------

//----------------------------------------------------------------------
// Global Data Definitions
//----------------------------------------------------------------------

//----------------------------------------------------------------------
// Static Variable Definitions
//----------------------------------------------------------------------


OSCL_EXPORT_REF IQcpFile::IQcpFile(OSCL_wString& filename, QCPErrorType &bSuccess, Oscl_FileServer* fileServSession, PVMFCPMPluginAccessInterfaceFactory*aCPM, OsclFileHandle*aFileHandle, bool enableCRC) :
        pQCPParser(NULL)
{
    bSuccess = QCP_SUCCESS;

    // Initialize the metadata related variables
    iAvailableMetadataKeys.reserve(14);
    iAvailableMetadataKeys.clear();

    // Open the specified QCP file
    iQCPFile.SetCPM(aCPM);
    iQCPFile.SetFileHandle(aFileHandle);
    if (iQCPFile.Open(filename.get_cstr(), (Oscl_File::MODE_READ | Oscl_File::MODE_BINARY), *fileServSession) != 0)
    {
        bSuccess = QCP_FILE_OPEN_ERR;
        return;
    }

    if (!aCPM)
    {
        iScanFP.SetCPM(aCPM);
        iScanFP.SetFileHandle(aFileHandle);
        if (iScanFP.Open(filename.get_cstr(), (Oscl_File::MODE_READ | Oscl_File::MODE_BINARY), *fileServSession) != 0)
        {
            bSuccess = QCP_FILE_OPEN_ERR;
            return;
        }
    }

    int32 leavecode = OsclErrNone;
    OSCL_TRY(leavecode, pQCPParser = OSCL_NEW(QCPParser, (&iQCPFile)););
    if (pQCPParser && OsclErrNone == leavecode)
        bSuccess = QCP_SUCCESS;
    else
        bSuccess = QCP_ERROR_UNKNOWN;
}

OSCL_EXPORT_REF IQcpFile::IQcpFile(QCPErrorType &bSuccess): pQCPParser(NULL)
{
    int32 leavecode = OsclErrNone;
    OSCL_TRY(leavecode, pQCPParser = OSCL_NEW(QCPParser, ()););
    if (pQCPParser && OsclErrNone == leavecode)
        bSuccess = QCP_SUCCESS;
    else
        bSuccess = QCP_ERROR_UNKNOWN;
}

OSCL_EXPORT_REF IQcpFile::~IQcpFile()
{
    iAvailableMetadataKeys.clear();

    if (pQCPParser != NULL)
    {
        OSCL_DELETE(pQCPParser);
        pQCPParser = NULL;
    }

    if (iScanFP.IsOpen())
    {
        iScanFP.Close();
    }
    if (iQCPFile.IsOpen())
    {
        iQCPFile.Close();
    }
}

OSCL_EXPORT_REF QCPErrorType IQcpFile::ParseQcpFile()
{
    QCPErrorType errCode = QCP_SUCCESS;

    // Parse the qcp clip
    errCode = pQCPParser->ParseQCPFile(&iQCPFile);
    if (errCode == QCP_INSUFFICIENT_DATA)
    {
        // not enough data was available to parse the clip
        return errCode;
    }
    else if (errCode != QCP_SUCCESS)
    {
        // some other error occured while parsing the clip.
        // parsing can not proceed further
        OSCL_DELETE(pQCPParser);
        pQCPParser = NULL;
        iQCPFile.Close();
        return errCode;
    }

    return errCode;
}

OSCL_EXPORT_REF QCPErrorType IQcpFile::IsQcpFile(OSCL_wString& aFileName,
        PVMFCPMPluginAccessInterfaceFactory *aCPMAccessFactory)
{
    QCPErrorType errCode = QCP_ERROR_UNKNOWN_OBJECT;

    QCP_FF_FILE fileStruct;
    QCP_FF_FILE *fp = &fileStruct;

    fp->_pvfile.SetCPM(aCPMAccessFactory);

    // open the dummy file
    if (QCPUtils::OpenFile(aFileName,
                           Oscl_File::MODE_READ | Oscl_File::MODE_BINARY,
                           fp) != 0)
    {
        errCode = QCP_FILE_OPEN_FAILED;
        return errCode;
    }
    // create the file parser object to recognize the clip
    QCPParser* qcpParser = NULL;
    qcpParser = OSCL_NEW(QCPParser, ());

    errCode = (qcpParser) ? QCP_SUCCESS : QCP_ERROR_UNKNOWN;

    // File was opened successfully, clip recognition can proceed
    if (errCode == QCP_SUCCESS)
    {
        errCode = qcpParser->IsQcpFile(fp);
        //deallocate the QCPParser object created
        if (qcpParser)
        {
            delete qcpParser;
            qcpParser = NULL;
        }
    }
    // close the file
    QCPUtils::CloseFile(&(fp->_pvfile)); //Close the QCP File
    return errCode;
}

OSCL_EXPORT_REF uint32 IQcpFile::GetDuration() const
{
    if (pQCPParser != NULL)
    {
        return pQCPParser->GetDuration();
    }
    else
    {
        return 0;
    }
}
