/*
 * Copyright (C) 2009 The Android Open Source Project
 * Copyright (c) 2010-2011, Code Aurora Forum. All rights reserved.
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
#include <media/stagefright/Utils.h>

#include "QComHardwareOverlayRenderer.h"
#include "QComHardwareRenderer.h"
#undef LOG_TAG
#define LOG_TAG "StagefrightSurfaceOutput7630"
#include <utils/Log.h>
//#define NDEBUG 0
#include "omx_drmplay_renderer.h"

using namespace android;

VideoRenderer *createRenderer(
        const sp<ISurface> &surface,
        const char *componentName,
        OMX_COLOR_FORMATTYPE colorFormat,
        size_t displayWidth, size_t displayHeight,
        size_t decodedWidth, size_t decodedHeight,
        size_t rotation , size_t flags ) {
    using android::QComHardwareOverlayRenderer;
    using android::QComHardwareRenderer;

    if (!strncmp(componentName, "OMX.qcom.video.decoder.", 23))
    {
        bool useOverlay = true;
        int baseColorFormat, temp1, temp2;
        GET_BASE_COLOR_FORMAT(colorFormat, baseColorFormat, temp1, temp2);
        switch (baseColorFormat)
        {
            case OMX_COLOR_FormatYUV420SemiPlanar:
            case OMX_QCOM_COLOR_FormatYVU420SemiPlanar:
                useOverlay = true;
                break;
            case QOMX_COLOR_FormatYUV420PackedSemiPlanar64x32Tile2m8ka:
                int format3D, formatInterlace;
                GET_INTERLACE_FORMAT(colorFormat, formatInterlace);
                GET_3D_FORMAT(colorFormat, format3D);
                useOverlay = formatInterlace || format3D;
                break;
            default:
                LOGE("ERR: Unsupported color format");
                return NULL;
        }

        if (useOverlay)
            {
                LOGV("StagefrightSurfaceOutput7x30::createRenderer");
                QComHardwareOverlayRenderer *videoRenderer =  new QComHardwareOverlayRenderer(
                    surface, colorFormat,
                    displayWidth, displayHeight,
                    decodedWidth, decodedHeight, rotation);
                bool initSuccess = videoRenderer->InitOverlayRenderer();
                if (!initSuccess)
                {
                    LOGE("Create Overlay Renderer failed");
                    delete videoRenderer;
                return NULL;
                }
                LOGV("Create Overlay Renderer successful");
                return static_cast<VideoRenderer *>(videoRenderer);
            }
        else
            {
                LOGV("StagefrightSurfaceOutput7x30::createRenderer QComHardwareRenderer");
                return new QComHardwareRenderer(
                        surface, colorFormat,
                        displayWidth, displayHeight,
                        decodedWidth, decodedHeight, rotation);
        }

    }  else if(!strncmp(componentName, "drm.play", 8)) {
              LOGV("StagefrightSurfaceOutput7x30::createRenderer for drm.play display *= %d,%d  decode = %d,%d", displayWidth, displayHeight, decodedWidth, decodedHeight);
              omx_drm_play_renderer::CreateRenderer(surface, decodedWidth, decodedHeight);
              return new omx_drm_dummy_renderer();
    }


    LOGE("error: StagefrightSurfaceOutput7x30::createRenderer returning NULL!");
    return NULL;
}
