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
/*
    This QCPUtils Class contains sime useful methods for operating FILE
*/
/*********************************************************************************/

#include "qcputils.h"
#include "oscl_base.h"

#ifndef OSCL_SNPRINTF_H_INCLUDED
#include "oscl_snprintf.h"
#endif

int32 QCPUtils::OpenFile(OSCL_wHeapString<OsclMemAllocator> filename,
                         uint32 mode,
                         QCP_FF_FILE *fp)
{
    OSCL_UNUSED_ARG(mode);
    if (fp != NULL)
    {
        return (fp->_pvfile.Open(filename.get_cstr(),
                                 Oscl_File::MODE_READ | Oscl_File::MODE_BINARY,
                                 (fp->_fileServSession)));
    }
    return -1;
}

int32 QCPUtils::CloseFile(PVFile *fp)
{
    if (fp != NULL)
    {
        return (fp->Close());
    }
    return -1;
}

