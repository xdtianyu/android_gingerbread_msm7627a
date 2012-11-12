/*
** Copyright 2006, The Android Open Source Project
** Copyright (c) 2010, Code Aurora Forum. All rights reserved.
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/

#define LOG_TAG "CND"

#include <stdio.h>
#include <assert.h>
#include <sys/types.h>
#include <fcntl.h>
#include <cnd.h>
#include "cutils/properties.h"

int main (int argc, char **argv)
{
  char prop_value[PROPERTY_VALUE_MAX] = {'\0'};
  int len = property_get("persist.cne.UseCne", prop_value, "none");
  prop_value[len] = '\0';
  if((strcasecmp(prop_value, "vendor") == 0) ||
     (strcasecmp(prop_value, "reference") == 0))
  {
    cnd_init();
    cnd_startEventLoop();
    while(1)
    {
      // sleep(UINT32_MAX) seems to return immediately on bionic
      sleep(0x00ffffff);
    }
  }
  return 0;
}


