/**
 * @file
 * TestDaemonLib.cpp
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

#include "C:\aj\alljoyn_core\daemon\windows\DaemonLib.h"
#include <stdio.h>

const wchar_t* filename = L"TestLog.txt";

int _tmain(int argc, char* argv[])
{
    wchar_t buffer[500];
    printf("AllJoyn Daemon Windows Service\n");
    swprintf_s(buffer, 500, L"C:\\Alljoyn\\logging\\%s", filename);
    SetLogFile(buffer);
    swprintf_s(buffer, 500, L"%S %s", argv[0], L" --verbosity=15 --no-bt\n");
//	LoadDaemon(argc,argv);
    DaemonMain(buffer);
    getchar();
    return 0;
}


