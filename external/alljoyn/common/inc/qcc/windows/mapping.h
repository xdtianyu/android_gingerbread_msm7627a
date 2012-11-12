/**
 * @file
 *
 * Map API names for Win32
 */

/******************************************************************************
 * Copyright 2010-2011, Qualcomm Innovation Center, Inc.
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 ******************************************************************************/
#ifndef _TOOLCHAIN_QCC_MAPPING_H
#define _TOOLCHAIN_QCC_MAPPING_H

#include <windows.h>
/// @cond ALLJOYN_DEV
/**
 * Map snprintf to _snprintf
 *
 * snprintf does not properly map in windows this is needed to insure calls to
 * snprintf(char *str, size_t size, const char *format, ...) will compile in
 * Windows.
 */
#define snprintf _snprintf
/// @endcond
#endif
