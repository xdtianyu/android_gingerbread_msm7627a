/*
 * Copyright (c) 2011, Code Aurora Forum. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <media/stagefright/HardwareAPI.h>

#include "QComHardwareRenderer.h"
#include "omx_drmplay_renderer.h"

using android::sp;
using android::ISurface;
using android::VideoRenderer;

VideoRenderer *createRenderer(
        const sp<ISurface> &surface,
        const char *componentName,
        OMX_COLOR_FORMATTYPE colorFormat,
        size_t displayWidth, size_t displayHeight,
        size_t decodedWidth, size_t decodedHeight,
        size_t rotation, size_t flags) { //rotation unused for now
    using android::QComHardwareRenderer;

    static const int OMX_QCOM_COLOR_FormatYVU420SemiPlanar = 0x7FA30C00;
    static const int QOMX_INTERLACE_FLAG = 0x49283654;

    if ((colorFormat == OMX_QCOM_COLOR_FormatYVU420SemiPlanar ||
         colorFormat == (OMX_QCOM_COLOR_FormatYVU420SemiPlanar ^ QOMX_INTERLACE_FLAG))
        && !strncmp(componentName, "OMX.qcom.video.decoder.", 23)) {
        return new QComHardwareRenderer(
                surface, colorFormat,
                displayWidth, displayHeight,
                decodedWidth, decodedHeight, rotation);
    } else if(!strncmp(componentName, "drm.play", 8)) {
              LOGV("StagefrightSurfaceOutput7x30::createRenderer for drm.play display *= %d,%d  decode = %d,%d", displayWidth, displayHeight, decodedWidth, decodedHeight);
              omx_drm_play_renderer::CreateRenderer(surface, decodedWidth, decodedHeight);
              return new omx_drm_dummy_renderer();
    }
    return NULL;
}
