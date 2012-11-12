/*
 * Copyright (C) 2010 The Android Open Source Project
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

#ifndef  _OMX_DRM_PLAY_RENDERER_H_
#define  _OMX_DRM_PLAY_RENDERER_H_

//////////////////////////////////////////////////////////////////////////////
//                             Include Files
//////////////////////////////////////////////////////////////////////////////
///
#include <media/stagefright/VideoRenderer.h>
#include <utils/RefBase.h>
#include <surfaceflinger/ISurface.h>
#include <fcntl.h>
#include <sys/mman.h>

using android::VideoRenderer;
using android::sp;
using android::ISurface;
class omx_drm_play_renderer;

extern omx_drm_play_renderer *gRenderer;

class omx_drm_play_renderer : public VideoRenderer {
public:
    sp<ISurface> mISurface;
    size_t mDisplayWidth;
    size_t mDisplayHeight;
    void *mLibHandle;
    VideoRenderer *mTarget;
public:
    omx_drm_play_renderer(
        const sp<ISurface> &surface,
        size_t displayWidth, size_t displayHeight){
       mISurface = surface;
       mDisplayHeight = displayHeight;
       mDisplayWidth = displayWidth;
    }

    ~omx_drm_play_renderer() {}

    static void CreateRenderer(const sp<ISurface> &surface,
        size_t decodedWidth, size_t decodedHeight);
    /*{
        gRenderer = new omx_drm_play_renderer(surface, decodedWidth, decodedHeight);
    }*/

    void render(
        const void *data, size_t size, void *platformPrivate) {}
};

class omx_drm_dummy_renderer : public VideoRenderer {
public:
    virtual void render(const void *data, size_t size, void *platformPrivate) {
        return;
    }
};

#endif //_OMX_DRM_PLAY_RENDERER_H_
