/* ------------------------------------------------------------------
 * Copyright (C) 2008 PacketVideo
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
/*********************************************************************************/
/*     -------------------------------------------------------------------       */
/*                        MPEG-4 AudioSampleEntry Class                          */
/*     -------------------------------------------------------------------       */
/*********************************************************************************/
/*
    This AudioSampleEntry Class is used for visual streams.
*/

#define IMPLEMENT_QCELPSampleEntry

#include "qcelpsampleentry.h"
#include "atomutils.h"
#include "atomdefs.h"

typedef Oscl_Vector<DecoderSpecificInfo*, OsclMemAllocator> decoderSpecificInfoVecType;
// Stream-in ctor
QCELPSampleEntry::QCELPSampleEntry(MP4_FF_FILE *fp, uint32 size, uint32 type)
        : Atom(fp, size, type)
{
    _pQCELPSpecificAtom = NULL;

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

        if (_success)
        {
            uint32 atom_type = UNKNOWN_ATOM;
            uint32 atom_size = 0;

            AtomUtils::getNextAtomType(fp, atom_size, atom_type);

            if (atom_type != QCELP_SPECIFIC_ATOM)
            {
                _success = false;
                _mp4ErrorCode = READ_QCELP_SAMPLE_ENTRY_FAILED;
            }
            else
            {
                PV_MP4_FF_NEW(fp->auditCB, QCELPSpecificAtom, (fp, atom_size, atom_type), _pQCELPSpecificAtom);

                if (!_pQCELPSpecificAtom->MP4Success())
                {
                    _success = false;
                    _mp4ErrorCode = READ_QCELP_SAMPLE_ENTRY_FAILED;
                }
            }
        }
        else
        {
            _mp4ErrorCode = READ_QCELP_SAMPLE_ENTRY_FAILED;
        }
    }
    else
    {
        _mp4ErrorCode = READ_QCELP_SAMPLE_ENTRY_FAILED;
    }

}

// Destructor
QCELPSampleEntry::~QCELPSampleEntry()
{
    if (_pQCELPSpecificAtom != NULL)
    {
        // Cleanup ESDAtom
        PV_MP4_FF_DELETE(NULL, QCELPSpecificAtom, _pQCELPSpecificAtom);
    }
}
