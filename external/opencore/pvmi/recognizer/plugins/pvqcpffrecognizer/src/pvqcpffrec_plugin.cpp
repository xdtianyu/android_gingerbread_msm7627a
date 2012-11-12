/* ------------------------------------------------------------------
 * Copyright (C) 1998-2009 PacketVideo
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
#include "pvqcpffrec_plugin.h"
#include "pvfile.h"
#include "oscl_file_io.h"
#include "iqcpff.h"

PVMFStatus PVQCPFFRecognizerPlugin::SupportedFormats(PVMFRecognizerMIMEStringList& aSupportedFormatsList)
{
    // Return QCP as supported type
    OSCL_HeapString<OsclMemAllocator> supportedformat = PVMF_MIME_QCPFF;
    aSupportedFormatsList.push_back(supportedformat);
    return PVMFSuccess;
}


PVMFStatus PVQCPFFRecognizerPlugin::Recognize(PVMFDataStreamFactory& aSourceDataStreamFactory, PVMFRecognizerMIMEStringList* aFormatHint,
        Oscl_Vector<PVMFRecognizerResult, OsclMemAllocator>& aRecognizerResult)
{
    OSCL_UNUSED_ARG(aFormatHint);
    // Instantiate the IQcpFile object, which is the class representing the qcp ff parser library.
    OSCL_wStackString<1> tmpfilename;
    QCPErrorType eSuccess = QCP_SUCCESS;
    IQcpFile* qcpFile = OSCL_NEW(IQcpFile, (eSuccess));
    if (!qcpFile || (eSuccess != QCP_SUCCESS))
    {
	// unable to construct parser object
	return PVMFSuccess;
    }

    eSuccess = qcpFile->IsQcpFile(tmpfilename, &aSourceDataStreamFactory);

    PVMFRecognizerResult result;
    if (eSuccess == QCP_SUCCESS)
    {
        // It is an QCP file so add positive result
        result.iRecognizedFormat = PVMF_MIME_QCPFF;
        result.iRecognitionConfidence = PVMFRecognizerConfidenceCertain;
        aRecognizerResult.push_back(result);
    }
    else if (eSuccess == QCP_INSUFFICIENT_DATA)
    {
        // It could be an QCP file, but not sure
        result.iRecognizedFormat = PVMF_MIME_QCPFF;
        result.iRecognitionConfidence = PVMFRecognizerConfidencePossible;
        aRecognizerResult.push_back(result);
    }
    if (qcpFile)
    {
        OSCL_DELETE(qcpFile);
        qcpFile = NULL;
    }
    return PVMFSuccess;
}


PVMFStatus PVQCPFFRecognizerPlugin::GetRequiredMinBytesForRecognition(uint32& aBytes)
{
    aBytes = QCPFF_MIN_DATA_SIZE_FOR_RECOGNITION;
    return PVMFSuccess;
}


