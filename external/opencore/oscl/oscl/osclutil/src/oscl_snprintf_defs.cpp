/*
Copyright (c) 2010, Code Aurora Forum. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above
      copyright notice, this list of conditions and the following
      disclaimer in the documentation and/or other materials provided
      with the distribution.
    * Neither the name of Code Aurora Forum, Inc. nor the names of its
      contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "oscl_snprintf.h"

#ifndef OSCLCONFIG_UTIL_H_INCLUDED
#include "osclconfig_util.h"
#endif

#ifndef OSCL_STRING_UTILS_H
#include "oscl_string_utils.h"
#endif

#ifndef OSCL_STDSTRING_H_INCLUDED
#include "oscl_stdstring.h"
#endif

#ifndef OSCL_STRING_UTILS_H_INCLUDED
#include "oscl_string_utils.h"
#endif

#ifndef OSCL_MEM_H_INCLUDED
#include "oscl_mem.h"
#endif

#include <stdio.h>
#include <wchar.h>

OSCL_EXPORT_REF int32 oscl_snprintf(char *str, uint32 count, const char *fmt, /*args*/ ...)
{
    int str_l;
    va_list args;
    va_start(args, fmt);
    str_l = vsnprintf(str, count, fmt, args);
    va_end(args);
    return str_l;
}

OSCL_EXPORT_REF int32 oscl_vsnprintf(char *str, uint32 count, const char *fmt, va_list args)
{
    return vsnprintf(str, count, fmt, args);
}


OSCL_EXPORT_REF int32 oscl_snprintf(oscl_wchar *str, uint32 count, const oscl_wchar *fmt, /*args*/ ...)
{
    int str_l;
    va_list args;
    va_start(args, fmt);
    str_l = vswprintf(str, count, fmt, args);
    va_end(args);
    return str_l;
}

OSCL_EXPORT_REF int32 oscl_vsnprintf(oscl_wchar *str, uint32 count, const oscl_wchar *fmt, va_list args)
{
   return vswprintf(str, count, fmt, args);
}

