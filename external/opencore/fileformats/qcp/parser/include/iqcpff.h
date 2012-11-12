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

//                 Q C P   F I L E   P A R S E R

// = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =


/**
 *  @file iqcpff.h
 *  @brief This file defines the QCP File Format Interface Definition.
 *  It initializes and maintains the QCP File Format Library
 */

#ifndef IQCPFF_H_INCLUDED
#define IQCPFF_H_INCLUDED


//----------------------------------------------------------------------
// Include Files
//----------------------------------------------------------------------

#ifndef OSCL_BASE_H_INCLUDED
#include "oscl_base.h"
#endif
#ifndef OSCL_VECTOR_H_INCLUDED
#include "oscl_vector.h"
#endif
#ifndef OSCL_String_H_INCLUDED
#include "oscl_string.h"

#endif
#ifndef OSCL_STRING_CONTAINERS_H_INCLUDED
#include "oscl_string_containers.h"
#endif
#ifndef OSCL_FILE_IO_H_INCLUDED
#include "oscl_file_io.h"
#endif
#ifndef OSCL_MEM_H_INCLUDED
#include "oscl_mem.h"
#endif
#ifndef PVFILE_H_INCLUDED
#include "pvfile.h"
#endif

// Header files reqd for multiple sample retrieval api
#ifndef OSCL_MEDIA_DATA_H
#include "oscl_media_data.h"
#endif
#ifndef PV_GAU_H_
#include "pv_gau.h"
#endif

// Header files for pvmf metadata handling
#ifndef PVMF_RETURN_CODES_H_INCLUDED
#include "pvmf_return_codes.h"
#endif
#ifndef PVMF_META_DATA_H_INCLUDED
#include "pvmf_meta_data_types.h"
#endif
#ifndef PVMI_KVP_INCLUDED
#include "pvmi_kvp.h"
#endif



//----------------------------------------------------------------------
// Global Type Declarations
//----------------------------------------------------------------------

/* QCP format types */
typedef enum {
    QCP_FORMAT_EVRC,   // EVRC
    QCP_FORMAT_QCELP,  // QCELP
    QCP_FORMAT_UNKNOWN // Unknown
}QCPFormatType;

typedef enum
{
    QCP_ERROR_UNKNOWN = 0,
    QCP_SUCCESS,
    QCP_END_OF_FILE,
    QCP_CRC_ERR,
    QCP_FILE_READ_ERR,
    QCP_FILE_HDR_READ_ERR,
    QCP_FILE_HDR_DECODE_ERR,
    QCP_FILE_XING_HDR_ERR,
    QCP_FILE_VBRI_HDR_ERR,
    QCP_NO_SYNC_FOUND,
    QCP_FILE_OPEN_ERR,
    QCP_ERROR_UNKNOWN_FORMAT,
    /* PD related Error values*/
    QCP_ERROR_UNKNOWN_OBJECT,
    QCP_FILE_OPEN_FAILED,
    QCP_INSUFFICIENT_DATA,
    QCP_METADATA_NOTPARSED,
    /* Duration related Info value*/
    QCP_DURATION_PRESENT
}QCPErrorType;

//----------------------------------------------------------------------
// Forward Class Declarations
//----------------------------------------------------------------------
class QCPParser;
class PVMFCPMPluginAccessInterfaceFactory;

/**
 *  @brief The IQcpFile Class is the class that will construct and maintain all the
 *  necessary data structures to be able to render a valid QCP file to disk.
 *
 */

class IQcpFile
{
    public:
        /**
        * @brief Constructor
        *
        * @param filename QCP filename
        * @param bSuccess Result of operation: true=successful, false=failed
        * @param fileServSession Pointer to opened file server session. Used when opening
        * and reading the file on certain operating systems.
        * @returns None
        */
        OSCL_IMPORT_REF  IQcpFile(OSCL_wString& filename, QCPErrorType &bSuccess, Oscl_FileServer* fileServSession = NULL, PVMFCPMPluginAccessInterfaceFactory*aCPM = NULL, OsclFileHandle*aHandle = NULL, bool enableCRC = true);

        /**
        * @brief Constructor
        *
        * @param bSuccess Result of operation: true=successful, false=failed
        * @returns None
        */
        OSCL_IMPORT_REF IQcpFile(QCPErrorType &bSuccess);

        /**
        * @brief Destructor
        *
        * @param None
        * @returns None
        */
        OSCL_IMPORT_REF ~IQcpFile();

        /**
        * @brief Returns the parse status of the file
        *
        * @param None
        * @returns Result of operation
        */
        OSCL_IMPORT_REF QCPErrorType ParseQcpFile();

        /**
        * @brief Returns the duration of the clip.
        *
        * @param None
        * @returns Duration
        */
        OSCL_IMPORT_REF uint32 GetDuration() const;

        /**
        * @brief Verifies if the supplied file is valid qcp file
        *
        * @param aFileName
        * @param aCPMAccessFactory
        */
        OSCL_IMPORT_REF QCPErrorType IsQcpFile(OSCL_wString& aFileName,
                                               PVMFCPMPluginAccessInterfaceFactory *aCPMAccessFactory);

    private:
        QCPParser* pQCPParser;
        PVFile iQCPFile;
        PVFile iScanFP;
        Oscl_Vector<OSCL_HeapString<OsclMemAllocator>, OsclMemAllocator> iAvailableMetadataKeys;
};

#endif // #ifndef IQCPFF_H_INCLUDED
