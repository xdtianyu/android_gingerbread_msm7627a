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
/*********************************************************************************/
/*     -------------------------------------------------------------------       */
/*                          MPEG-4 Util: AudioHintSample                         */
/*     -------------------------------------------------------------------       */
/*********************************************************************************/
/*
    This DecoderSpecificInfo Class that holds the Mpeg4 VOL header for the
	video stream
*/

#define __IMPLEMENT_EVRCDecoderSpecificInfo3GPP__

#include "3gpp2_evrcdecoderspecificinfo.h"

#include "atom.h"
#include "atomutils.h"

// Default constructor
EVRCSpecificAtom::EVRCSpecificAtom(MP4_FF_FILE *fp, uint32 size, uint32 type)
        : Atom(fp, size, type)
{
    if (_success)
    {
        AtomUtils::read32(fp, _VendorCode);

        AtomUtils::read8(fp, _decoder_version);

        AtomUtils::read8(fp, _frames_per_sample);
    }
}


