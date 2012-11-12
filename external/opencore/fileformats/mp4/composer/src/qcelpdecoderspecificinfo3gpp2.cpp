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
    This PVA_FF_DecoderSpecificInfo Class that holds the Mpeg4 VOL header for the
	video stream
*/

#define __IMPLEMENT_QCELPDecoderSpecificInfo3GPP2__

#include "qcelpdecoderspecificinfo3gpp2.h"

#include "atom.h"
#include "atomutils.h"

#define QCELP_DECODER_SPECIFIC_SIZE 6

// Default constructor
PVA_FF_QCELPSpecificAtom::PVA_FF_QCELPSpecificAtom()
        : PVA_FF_Atom(FourCharConstToUint32('d', 'q', 'c', 'p'))
{
    _VendorCode = PACKETVIDEO_FOURCC;

    recomputeSize();
}

bool
PVA_FF_QCELPSpecificAtom::renderToFileStream(MP4_AUTHOR_FF_FILE_IO_WRAP *fp)
{
    int32 rendered = 0;

    renderAtomBaseMembers(fp);
    rendered += getDefaultSize();

    // Render decoder specific info payload
    if (!PVA_FF_AtomUtils::render32(fp, _VendorCode))
    {
        return false;
    }
    rendered += 4;

    if (!PVA_FF_AtomUtils::render8(fp, _encoder_version))
    {
        return false;
    }
    rendered += 1;

    if (!PVA_FF_AtomUtils::render8(fp, _frames_per_sample))
    {
        return false;
    }
    rendered += 1;

    return true;
}

void
PVA_FF_QCELPSpecificAtom::recomputeSize()
{
    uint32 size = getDefaultSize();

    size += QCELP_DECODER_SPECIFIC_SIZE; // FOR DECODER SPECIFIC STRUCT

    _size = size;

    if (_pparent != NULL)
    {
        _pparent->recomputeSize();
    }
}

