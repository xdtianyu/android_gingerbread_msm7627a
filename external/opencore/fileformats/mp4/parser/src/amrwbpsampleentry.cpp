/* ------------------------------------------------------------------
 * Copyright (C) 1998-2009 PacketVideo
 * Copyright (c) 2009, Code Aurora Forum. All rights reserved
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
/*********************************************************************************/
/*     -------------------------------------------------------------------       */
/*                        MPEG-4 AudioSampleEntry Class                          */
/*     -------------------------------------------------------------------       */
/*********************************************************************************/
/*
    This AudioSampleEntry Class is used for visual streams.
*/

#define IMPLEMENT_AMRWBPSampleEntry

#include "amrwbpsampleentry.h"
#include "atomutils.h"
#include "atomdefs.h"

// Stream-in ctor
AMRWBPSampleEntry::AMRWBPSampleEntry(MP4_FF_FILE *fp, uint32 size, uint32 type)
        : Atom(fp, size, type)
{
    _pAMRWBPSpecificAtom = NULL;

	 iLogger = PVLogger::GetLoggerObject("mp4ffparser");

    PVMF_MP4FFPARSER_LOGERROR((0, "ERROR =>AMRWBPSampleEntryAtom - in"));
    if (_success)
    {
        // Read reserved values
        if (!AtomUtils::read8read8(fp, _reserved[0], _reserved[1]))
            _success = false;
        if (!AtomUtils::read8read8(fp, _reserved[2], _reserved[3]))
            _success = false;
        if (!AtomUtils::read8read8(fp, _reserved[4], _reserved[5]))
            _success = false;

        if (!AtomUtils::read16(fp, _dataReferenceIndex))
            _success = false;

        if (!AtomUtils::read32read32(fp, _reserved1[0], _reserved1[1]))
            _success = false;
        if (!AtomUtils::read16read16(fp, _reserved2, _reserved3))
            _success = false;
        if (!AtomUtils::read32(fp, _reserved4))
            _success = false;

        if (!AtomUtils::read16read16(fp, _timeScale, _reserved5))
            _success = false;

		  PVMF_MP4FFPARSER_LOGERROR((0, "ERROR =>AMRWBPSampleEntryAtom - in()"));
        if (_success)
        {
            uint32 atom_type = UNKNOWN_ATOM;
            uint32 atom_size = 0;

            AtomUtils::getNextAtomType(fp, atom_size, atom_type);

            if (atom_type != AMRWBP_SPECIFIC_ATOM)
            {
					  PVMF_MP4FFPARSER_LOGERROR((0, "ERROR =>AMRWBPSampleEntryAtom - could not find AMRWBP_SPECIFIC_ATOM"));
                _success = false;
                _mp4ErrorCode = READ_AMR_SAMPLE_ENTRY_FAILED;
            }
            else
            {
					 PVMF_MP4FFPARSER_LOGERROR((0, "ERROR =>AMRWPSampleEntryAtom - AMRWBP_SPECIFIC_ATOM found successfully"));
                PV_MP4_FF_NEW(fp->auditCB, AMRWBPSpecificAtom, (fp, atom_size, atom_type), _pAMRWBPSpecificAtom);

                if (!_pAMRWBPSpecificAtom->MP4Success())
                {
                    _success = false;
                    _mp4ErrorCode = READ_AMR_SAMPLE_ENTRY_FAILED;
                }
            }
        }
        else
        {
            _mp4ErrorCode = READ_AMR_SAMPLE_ENTRY_FAILED;
        }
    }
    else
    {
        _mp4ErrorCode = READ_AMR_SAMPLE_ENTRY_FAILED;
    }

}

// Destructor
AMRWBPSampleEntry::~AMRWBPSampleEntry()
{
    if (_pAMRWBPSpecificAtom != NULL)
    {
        // Cleanup ESDAtom
        PV_MP4_FF_DELETE(NULL, AMRWBPSpecificAtom, _pAMRWBPSpecificAtom);
    }
}

