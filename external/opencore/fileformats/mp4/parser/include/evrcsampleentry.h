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
/*
    This AudioSampleEntry Class is used for visual streams.
*/


#ifndef EVRCSAMPLEENTRY_H_INCLUDED
#define EVRCSAMPLEENTRY_H_INCLUDED

#ifndef ATOM_H_INCLUDED
#include "atom.h"
#endif
#ifndef ATOMUTILS_H_INCLUDED
#include "atomutils.h"
#endif
#ifndef OSCL_FILE_IO_H_INCLUDED
#include "oscl_file_io.h"
#endif
#ifndef GPP_EVRCDECODERSPECIFICINFO_H_INCLUDED
#include "3gpp2_evrcdecoderspecificinfo.h"
#endif
#include "oscl_vector.h"
class EVRCSampleEntry : public Atom
{

    public:
        EVRCSampleEntry(MP4_FF_FILE *fp, uint32 size, uint32 type); // Stream-in ctor
        virtual ~EVRCSampleEntry();

        uint16 getTimeScale()
        {
            return _timeScale;
        }

        virtual EVRCSpecificAtom *getDecoderSpecificInfo() const
        {
            return _pEVRCSpecificAtom;
        }

        DecoderSpecificInfo *getDecoderSpecificInfo(uint32 index);

    private:
        // Reserved constants
        uint8 _reserved[6];
        uint16 _dataReferenceIndex;
        uint32 _reserved1[2];
        uint16 _reserved2;
        uint16 _reserved3;
        uint32 _reserved4;
        uint16 _reserved5;
        uint16 _timeScale;

        EVRCSpecificAtom *_pEVRCSpecificAtom;
};


#endif // EVRCSAMPLEENTRY_H_INCLUDED
