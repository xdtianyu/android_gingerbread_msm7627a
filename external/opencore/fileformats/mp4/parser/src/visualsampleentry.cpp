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
/*********************************************************************************/
/*     -------------------------------------------------------------------       */
/*                        MPEG-4 VisualSampleEntry Class                         */
/*     -------------------------------------------------------------------       */
/*********************************************************************************/
/*
    This VisualSampleEntry Class is used for visual streams.
*/


#define IMPLEMENT_VisualSampleEntry

#include "visualsampleentry.h"
#include "atomutils.h"
#include "atomdefs.h"

#include "h263decoderspecificinfo.h"


VisualSampleEntry::VisualSampleEntry(MP4_FF_FILE *fp, uint32 size, uint32 type)
        : SampleEntry(fp, size, type)
{
    uint32 count = _size - DEFAULT_ATOM_SIZE - getSampleEntrySize();

    _pes = NULL;

    if (_success)
    {
        _pparent = NULL;
        _success = false;
        _mp4ErrorCode = READ_VISUAL_SAMPLE_ENTRY_FAILED;

        // Read in all reserved members
        for (int32 i = 0; i < 4; i++)
        {
            if (!AtomUtils::read32(fp, _reserved1[i]))
            {
                return;
            }
            count -= 4;
        }

        if (!AtomUtils::read32read32(fp, _reserved2, _reserved3))
        {
            return;
        }
        count -= 8;

        if (!AtomUtils::read32read32(fp, _reserved4, _reserved5))
        {
            return;
        }
        count -= 8;

        if (!AtomUtils::read16(fp, _reserved6))
        {
            return;
        }
        count -= 2;

        for (int32 i = 0; i < 32; i++)
        {
            if (!AtomUtils::read8(fp, _reserved7[i]))
            {
                return;
            }
            count -= 1;
        }

        if (!AtomUtils::read16read16(fp, _reserved8, _reserved9))
        {
            return;
        }
        count -= 4;

        while (count >= DEFAULT_ATOM_SIZE)
        {
            uint32 atomType = UNKNOWN_ATOM;
            uint32 atomSize = 0;

            AtomUtils::getNextAtomType(fp, atomSize, atomType);

            if (atomType == ESD_ATOM)
            {

                PV_MP4_FF_NEW(fp->auditCB, ESDAtom, (fp, atomSize, atomType), _pes);
                if (!_pes->MP4Success())
                {
                    _mp4ErrorCode = _pes->GetMP4Error();
                    return;
                }
                else
                {
                    if (_pes->getObjectTypeIndication() == H263_VIDEO)
                    {
                        const ESDescriptor *_pdescriptor = _pes->getESDescriptorPtr();
                        if (NULL != _pdescriptor)
                        {
                            H263DecoderSpecificInfo *_pH263decSpecificInfo =
                                (H263DecoderSpecificInfo *)(_pdescriptor->getDecoderSpecificInfo());
                            if (NULL != _pH263decSpecificInfo)
                            {
                                if (_pH263decSpecificInfo->_max_height <= 0)
                                    _pH263decSpecificInfo->_max_height = (uint16)((_reserved2 << 16) >> 16);
                                if (_pH263decSpecificInfo->_max_width <= 0)
                                    _pH263decSpecificInfo->_max_width = (uint16)(_reserved2 >> 16);
                            }
                        }
                    }
                    _pes->setParent(this);
                }
                count -= atomSize;
            }
            else
            {
                if (atomSize < DEFAULT_ATOM_SIZE)
                {
                    _mp4ErrorCode = ZERO_OR_NEGATIVE_ATOM_SIZE;
                    return;
                }

                if (count < (uint32)atomSize)
                {
                    _mp4ErrorCode = READ_FAILED;
                    break;
                }

                /* skip any unrecognized atoms here */
                count -= atomSize;
                atomSize -= DEFAULT_ATOM_SIZE;
                AtomUtils::seekFromCurrPos(fp, atomSize);
            }
        }

        if (count > 0)
        {
            //skip over any left over bytes
            AtomUtils::seekFromCurrPos(fp, count);
            count = 0;
        }

        /*
         * ESD atom is mandatory. In case any of it is absent
         * return ERROR!!!
         */
        if (_pes != NULL)
        {
            _success = true;
            _mp4ErrorCode = EVERYTHING_FINE;
        }
    }
}

// Destructor
VisualSampleEntry::~VisualSampleEntry()
{
    if (_pes != NULL)
    {
        // Cleanup ESDAtom
        PV_MP4_FF_DELETE(NULL, ESDAtom, _pes);
    }
}

uint32 VisualSampleEntry::getESID() const
{
    if (_pes != NULL)
    {
        if (_pes->getESDescriptorPtr() != NULL)
        {
            return _pes->getESDescriptorPtr()->getESID();
        }
        else
        {
            return 0;
        }
    }
    else
    {
        return 0;
    }
}

uint16 VisualSampleEntry::getWidth() const
{
    uint32 width = _reserved2 & 0xFFFF0000;
    uint32 ret = width >> 16;
    return (uint16)ret;

}

