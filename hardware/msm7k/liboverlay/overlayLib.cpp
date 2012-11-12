/*
 * Copyright (C) 2008 The Android Open Source Project
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

#include "overlayLib.h"
#include "gralloc_priv.h"

#define INTERLACE_MASK 0x80
/* Helper functions */

static inline int ALIGN(int value, int align) {
    //if align = 0, return the value. Else, do alignment.
    return align ? ((((value - 1) / align) + 1) * align) : value;
}
int overlay::get_mdp_format(int format) {
    switch (format) {
    case HAL_PIXEL_FORMAT_RGBA_8888 :
        return MDP_RGBA_8888;
    case HAL_PIXEL_FORMAT_BGRA_8888:
        return MDP_BGRA_8888;
    case HAL_PIXEL_FORMAT_RGB_565:
        return MDP_RGB_565;
    case HAL_PIXEL_FORMAT_RGBX_8888:
        return MDP_RGBX_8888;
    case HAL_PIXEL_FORMAT_YCbCr_422_SP:
        return MDP_Y_CBCR_H2V1;
    case HAL_PIXEL_FORMAT_YCbCr_420_SP:
        return MDP_Y_CBCR_H2V2;
    case HAL_PIXEL_FORMAT_YCrCb_420_SP:
        return MDP_Y_CRCB_H2V2;
    case HAL_PIXEL_FORMAT_YCbCr_420_SP_TILED:
        return MDP_Y_CRCB_H2V2_TILE;
    }
    return -1;
}

int overlay::get_size(int format, int w, int h) {
    int size, aligned_height, pitch;

    size = w * h;
    switch (format) {
    case MDP_RGBA_8888:
    case MDP_BGRA_8888:
    case MDP_RGBX_8888:
        size *= 4;
        break;
    case MDP_RGB_565:
    case MDP_Y_CBCR_H2V1:
        size *= 2;
        break;
    case MDP_Y_CBCR_H2V2:
    case MDP_Y_CRCB_H2V2:
        size = (size * 3) / 2;
        break;
    case MDP_Y_CRCB_H2V2_TILE:
        aligned_height = ALIGN(h , 32);
        pitch = ALIGN(w, 128);
        size = pitch * aligned_height;
        size = ALIGN(size, 8192);

        aligned_height = ALIGN(h >> 1, 32);
        size += pitch * aligned_height;
        size = ALIGN(size, 8192);
        break;
    default:
        return 0;
    }
    return size;
}

int overlay::get_mdp_orientation(int rotation, int flip) {
    switch(flip) {
    case HAL_TRANSFORM_FLIP_V:
        switch(rotation) {
        case 0: return MDP_FLIP_UD;
        case HAL_TRANSFORM_ROT_90:  return (MDP_ROT_90 | MDP_FLIP_LR);
        default: return -1;
        break;
        }
    break;
    case HAL_TRANSFORM_FLIP_H:
        switch(rotation) {
        case 0: return MDP_FLIP_LR;
        case HAL_TRANSFORM_ROT_90:  return (MDP_ROT_90 | MDP_FLIP_UD);
        default: return -1;
        break;
        }
    break;
    default:
        switch(rotation) {
        case 0: return MDP_ROT_NOP;
        case HAL_TRANSFORM_ROT_90:  return MDP_ROT_90;
        case HAL_TRANSFORM_ROT_180: return MDP_ROT_180;
        case HAL_TRANSFORM_ROT_270: return MDP_ROT_270;
        default: return -1;
        break;
        }
    break;
    }
    return -1;
}

#define LOG_TAG "OverlayLIB"
static void reportError(const char* message) {
    LOGE( "%s", message);
}

using namespace overlay;

bool overlay::isHDMIConnected () {
    char value[PROPERTY_VALUE_MAX];
    property_get("hw.hdmiON", value, "0");
    int isHDMI = atoi(value);
    return isHDMI ? true : false;
}

bool overlay::is3DTV() {
    char is3DTV = '0';
    FILE *fp = fopen(EDID_3D_INFO_FILE, "r");
    if (fp) {
        fread(&is3DTV, 1, 1, fp);
        fclose(fp);
    }
    LOGI("3DTV EDID flag: %d", is3DTV);
    return (is3DTV == '0') ? false : true;
}

bool overlay::isPanel3D() {
    int fd = open("/dev/graphics/fb0", O_RDWR, 0);
    if (fd < 0) {
        reportError("Can't open framebuffer 0");
        return false;
    }
    fb_fix_screeninfo finfo;
    if (ioctl(fd, FBIOGET_FSCREENINFO, &finfo) == -1) {
        reportError("FBIOGET_FSCREENINFO on fb0 failed");
        close(fd);
        fd = -1;
        return false;
    }
    return (FB_TYPE_3D_PANEL == finfo.type) ? true : false;
}

bool overlay::usePanel3D() {
    if(!isPanel3D())
        return false;
    char value[PROPERTY_VALUE_MAX];
    property_get("persist.user.panel3D", value, "0");
    int usePanel3D = atoi(value);
    return usePanel3D ? true : false;
}

bool overlay::send3DInfoPacket (unsigned int format3D) {
    FILE *fp = fopen(FORMAT_3D_FILE, "wb");
    if (fp) {
        fprintf(fp, "%d", format3D);
        fclose(fp);
        fp = NULL;
        return true;
    }
    LOGE("%s:no sysfs entry for setting 3d mode!", __func__);
    return false;
}

bool overlay::enableBarrier (unsigned int orientation) {
    FILE *fp = fopen(BARRIER_FILE, "wb");
    if (fp) {
        fprintf(fp, "%d", orientation);
        fclose(fp);
        fp = NULL;
        return true;
    }
    LOGE("%s:no sysfs entry for enabling barriers on 3D panel!", __func__);
    return false;
}

unsigned int overlay::getOverlayConfig (unsigned int format3D, bool poll,
                                                               bool isHDMI) {
    bool isTV3D = false;
    unsigned int curState = 0;
    if (poll)
        isHDMI = overlay::isHDMIConnected();
    if (isHDMI) {
        LOGD("%s: HDMI connected... checking the TV type", __func__);
        if (format3D) {
            isTV3D = is3DTV();
            if (isTV3D)
                curState = OV_3D_VIDEO_3D_TV;
            else
                curState = OV_3D_VIDEO_2D_TV;
        } else
            curState = OV_2D_VIDEO_ON_TV;
    } else {
        LOGD("%s: HDMI not connected...", __func__);
        bool use3DPanel = usePanel3D();
        if(format3D)
            if (use3DPanel)
                curState = OV_3D_VIDEO_3D_PANEL;
            else
                curState = OV_3D_VIDEO_2D_PANEL;
        else
            curState = OV_2D_VIDEO_ON_PANEL;
    }
    return curState;
}

Overlay::Overlay() : mChannelUP(false), mHDMIConnected(false), mS3DFormat(0),
                mState(-1), mSrcWidth(0), mSrcHeight(0) {
}

Overlay::~Overlay() {
    closeChannel();
}

int Overlay::getFBWidth(int channel) const {
    return objOvCtrlChannel[channel].getFBWidth();
}

int Overlay::getFBHeight(int channel) const {
    return objOvCtrlChannel[channel].getFBHeight();
}

bool Overlay::startChannel(int w, int h, int format, int fbnum,
                              bool norot, bool uichannel,
                              unsigned int format3D, int channel,
                              bool ignoreFB, int num_buffers) {
    int zorder = 0;
    mSrcWidth = w;
    mSrcHeight = h;
    if (format3D)
        zorder = channel;
    if (mState == -1)
        mState = OV_UI_MIRROR_TV;
    mChannelUP = objOvCtrlChannel[channel].startControlChannel(w, h, format,
                           fbnum, norot, uichannel, format3D, zorder, ignoreFB);
    if (!mChannelUP) {
        LOGE("startChannel for fb%d failed", fbnum);
        return mChannelUP;
    }
    return objOvDataChannel[channel].startDataChannel(objOvCtrlChannel[channel],
                                         fbnum, norot, uichannel, num_buffers);
}

bool Overlay::closeChannel() {
    if (!mChannelUP)
        return true;

    if(mS3DFormat) {
        if (mHDMIConnected)
            overlay::send3DInfoPacket(0);
        else if (mState == OV_3D_VIDEO_3D_PANEL)
            enableBarrier(0);
    }
    for (int i = 0; i < NUM_CHANNELS; i++) {
        objOvCtrlChannel[i].closeControlChannel();
        objOvDataChannel[i].closeDataChannel();
    }
    mChannelUP = false;
    mHDMIConnected = false;
    mS3DFormat = 0;
    mState = -1;
    return true;
}

bool Overlay::getPosition(int& x, int& y, uint32_t& w, uint32_t& h, int channel) {
    if (mS3DFormat)
        return false;
    return objOvCtrlChannel[channel].getPosition(x, y, w, h);
}

bool Overlay::getOrientation(int& orientation, int channel) const {
    return objOvCtrlChannel[channel].getOrientation(orientation);
}

bool Overlay::setPosition(int x, int y, uint32_t w, uint32_t h) {
    bool ret = false;
    overlay_rect rect;
    switch (mState) {
    case OV_UI_MIRROR_TV:
    case OV_2D_VIDEO_ON_PANEL:
    case OV_3D_VIDEO_2D_PANEL:
        return setChannelPosition(x, y, w, h, VG0_PIPE);
        break;
    case OV_2D_VIDEO_ON_TV:
        objOvCtrlChannel[VG1_PIPE].getAspectRatioPosition(mSrcWidth, mSrcHeight,
                                                            &rect);
        setChannelPosition(rect.x, rect.y, rect.w, rect.h, VG1_PIPE);
        return setChannelPosition(x, y, w, h, VG0_PIPE);
        break;
    case OV_3D_VIDEO_3D_PANEL:
        for (int i = 0; i < NUM_CHANNELS; i++) {
            if (!objOvCtrlChannel[i].useVirtualFB()) {
                LOGE("%s: failed virtual fb for channel %d", __func__, i);
                return false;
            }
            objOvCtrlChannel[i].getPositionS3D(i, 0x1, &rect);
            if(!setChannelPosition(rect.x, rect.y, rect.w, rect.h, i)) {
                LOGE("%s: failed for channel %d", __func__, i);
                return false;
            }
        }
        break;
    case OV_3D_VIDEO_2D_TV:
    case OV_3D_VIDEO_3D_TV:
        for (int i = 0; i < NUM_CHANNELS; i++) {
            ret = objOvCtrlChannel[i].getPositionS3D(i, mS3DFormat, &rect);
            if (!ret)
                ret = setChannelPosition(x, y, w, h, i);
            else
                ret = setChannelPosition(rect.x, rect.y, rect.w, rect.h, i);
            if (!ret) {
                LOGE("%s: failed for channel %d", __func__, i);
                return ret;
            }
        }
        break;
    default:
        break;
    }
    return true;
}

bool Overlay::setChannelPosition(int x, int y, uint32_t w, uint32_t h, int channel) {
    return objOvCtrlChannel[channel].setPosition(x, y, w, h);
}

bool Overlay::setSource(uint32_t w, uint32_t h, int format, int orientation,
                        bool hdmiConnected, bool ignoreFB, int num_buffers) {
    // Separate the color format from the 3D format.
    // If there is 3D content; the effective format passed by the client is:
    // effectiveFormat = 3D_IN | 3D_OUT | ColorFormat
    bool stateChange = false, ret = true;
    unsigned int format3D = FORMAT_3D(format);
    int colorFormat = COLOR_FORMAT(format);
    int fIn3D = FORMAT_3D_INPUT(format3D); // MSB 2 bytes are input format
    int fOut3D = FORMAT_3D_OUTPUT(format3D); // LSB 2 bytes are output format
    int newState = mState;
    format3D = fIn3D | fOut3D;
    // Use the same in/out format if not mentioned
    if (!fIn3D) {
        format3D |= fOut3D << SHIFT_3D; //Set the input format
    }
    if (!fOut3D) {
        format3D |= fIn3D >> SHIFT_3D; //Set the output format
    }

    if (mHDMIConnected != hdmiConnected || mState == -1) {
        newState = getOverlayConfig (format3D, false, hdmiConnected);
        stateChange = (mState == newState) ? false : true;
    }
    if (!mS3DFormat || (mS3DFormat & HAL_3D_OUT_MONOSCOPIC_MASK)) {
        if (mChannelUP)
            ret = objOvCtrlChannel[0].setSource(w, h, colorFormat, orientation, ignoreFB);
        else
            ret = false;
    }
    if (stateChange || !(ret)) {
        closeChannel();
        mHDMIConnected = hdmiConnected;
        mState = newState;
        mS3DFormat = format3D;
        if (mState == OV_3D_VIDEO_2D_PANEL || mState == OV_3D_VIDEO_2D_TV) {
            LOGI("3D content on 2D display: set the output format as monoscopic");
            mS3DFormat = FORMAT_3D_INPUT(format3D) | HAL_3D_OUT_MONOSCOPIC_MASK;
        }
        bool noRot = !orientation;
        switch(mState) {
        case OV_2D_VIDEO_ON_PANEL:
        case OV_3D_VIDEO_2D_PANEL:
            return startChannel(w, h, colorFormat, FRAMEBUFFER_0, noRot, false,
                               mS3DFormat, VG0_PIPE, ignoreFB, num_buffers);
            break;
        case OV_3D_VIDEO_3D_PANEL:
            for (int i=0; i<NUM_CHANNELS; i++) {
                if(!startChannel(w, h, colorFormat, FRAMEBUFFER_0, false, false,
                                 mS3DFormat, i, ignoreFB, num_buffers)) {
                    LOGE("%s:failed to open channel %d", __func__, i);
                    return false;
                }
            }
            break;
        case OV_2D_VIDEO_ON_TV:
        case OV_3D_VIDEO_2D_TV:
            for (int i=0; i<NUM_CHANNELS; i++) {
                if (FRAMEBUFFER_1 == i)
                    noRot = true;
                if(!startChannel(w, h, colorFormat, i, noRot, false, mS3DFormat,
                                 i, ignoreFB, num_buffers)) {
                    LOGE("%s:failed to open channel %d", __func__, i);
                    return false;
                }
            }
            overlay_rect rect;
            objOvCtrlChannel[VG1_PIPE].getAspectRatioPosition(w, h, &rect);
            return setChannelPosition(rect.x, rect.y, rect.w, rect.h, VG1_PIPE);
            break;
        case OV_3D_VIDEO_3D_TV:
            for (int i=0; i<NUM_CHANNELS; i++) {
                if(!startChannel(w, h, colorFormat, FRAMEBUFFER_1, true, false,
                                 mS3DFormat, i, ignoreFB, num_buffers)) {
                    LOGE("%s:failed to open channel %d", __func__, i);
                    return false;
                }
                send3DInfoPacket(mS3DFormat & OUTPUT_MASK_3D);
            }
            break;
        default:
            break;
        }
    }
    return true;
}

bool Overlay::setCrop(uint32_t x, uint32_t y, uint32_t w, uint32_t h) {
    if (!mChannelUP)
        return false;
    overlay_rect rect, inRect;
    inRect.x = x; inRect.y = y; inRect.w = w; inRect.h = h;
    mSrcWidth = w;
    mSrcHeight = h;
    switch (mState) {
    case OV_UI_MIRROR_TV:
    case OV_2D_VIDEO_ON_PANEL:
        return setChannelCrop(x, y, w, h, VG0_PIPE);
        break;
    case OV_3D_VIDEO_2D_PANEL:
        objOvDataChannel[VG0_PIPE].getCropS3D(&inRect, VG0_PIPE, mS3DFormat, &rect);
        return setChannelCrop(rect.x, rect.y, rect.w, rect.h, VG0_PIPE);
        break;
    case OV_2D_VIDEO_ON_TV:
        for (int i=0; i<NUM_CHANNELS; i++) {
            if(!setChannelCrop(x, y, w, h, i)) {
                LOGE("%s: failed for pipe %d", __func__, i);
                return false;
            }
        }
        break;
    case OV_3D_VIDEO_3D_PANEL:
    case OV_3D_VIDEO_2D_TV:
    case OV_3D_VIDEO_3D_TV:
        for (int i=0; i<NUM_CHANNELS; i++) {
            objOvDataChannel[i].getCropS3D(&inRect, i, mS3DFormat, &rect);
            if(!setChannelCrop(rect.x, rect.y, rect.w, rect.h, i)) {
                LOGE("%s: failed for pipe %d", __func__, i);
                return false;
            }
        }
        break;
    default:
        break;
    }
    return true;
}

bool Overlay::setChannelCrop(uint32_t x, uint32_t y, uint32_t w, uint32_t h, int channel) {
    return objOvDataChannel[channel].setCrop(x, y, w, h);
}

bool Overlay::setParameter(int param, int value) {
    switch (mState) {
    case OV_UI_MIRROR_TV:
    case OV_2D_VIDEO_ON_PANEL:
    case OV_3D_VIDEO_2D_PANEL:
        return objOvCtrlChannel[VG0_PIPE].setParameter(param, value);
        break;
    case OV_2D_VIDEO_ON_TV:
    case OV_3D_VIDEO_2D_TV:
    case OV_3D_VIDEO_3D_TV:
        for (int i=0; i<NUM_CHANNELS; i++) {
            if(!objOvCtrlChannel[i].setParameter(param, value)) {
                LOGE("%s:failed for channel %d", __func__, i);
                return false;
            }
        }
        break;
    case OV_3D_VIDEO_3D_PANEL:
        if (param == OVERLAY_TRANSFORM) {
            int barrier = 0;
            switch (value) {
                case HAL_TRANSFORM_ROT_90:
                case HAL_TRANSFORM_ROT_270:
                    barrier = BARRIER_LANDSCAPE;
                    break;
                default:
                    barrier = BARRIER_PORTRAIT;
                    break;
            }
            if(!enableBarrier(barrier))
                LOGE("%s:failed to enable barriers for 3D video");
        }
        for (int i=0; i<NUM_CHANNELS; i++) {
            if(!objOvCtrlChannel[i].setParameter(param, value)) {
                LOGE("%s:failed for channel %d", __func__, i);
                return false;
            }
        }
        break;
    default:
        break;
    }
    return true;
}

bool Overlay::setOrientation(int value, int channel) {
    return objOvCtrlChannel[channel].setParameter(OVERLAY_TRANSFORM, value);
}

bool Overlay::setFd(int fd, int channel) {
    return objOvDataChannel[channel].setFd(fd);
}

bool Overlay::queueBuffer(uint32_t offset, int channel) {
    return objOvDataChannel[channel].queueBuffer(offset);
}

bool Overlay::queueBuffer(buffer_handle_t buffer) {
    private_handle_t const* hnd = reinterpret_cast
                                   <private_handle_t const*>(buffer);
    const size_t offset = hnd->offset;
    const int fd = hnd->fd;
    switch (mState) {
    case OV_UI_MIRROR_TV:
    case OV_2D_VIDEO_ON_PANEL:
    case OV_3D_VIDEO_2D_PANEL:
        if(!queueBuffer(fd, offset, VG0_PIPE)) {
            LOGE("%s:failed for channel 0", __func__);
            return false;
        }
        break;
    case OV_2D_VIDEO_ON_TV:
    case OV_3D_VIDEO_3D_PANEL:
    case OV_3D_VIDEO_2D_TV:
    case OV_3D_VIDEO_3D_TV:
        for (int i=0; i<NUM_CHANNELS; i++) {
            if(!queueBuffer(fd, offset, i)) {
                LOGE("%s:failed for channel %d", __func__, i);
                return false;
            }
        }
        break;
    default:
        break;
    }
    return true;
}

bool Overlay::queueBuffer(int fd, uint32_t offset, int channel) {
    bool ret = false;
    ret = setFd(fd, channel);
    if(!ret) {
        LOGE("Overlay::queueBuffer channel %d setFd failed", channel);
        return false;
    }
    ret = queueBuffer(offset, channel);
    if(!ret) {
        LOGE("Overlay::queueBuffer channel %d queueBuffer failed", channel);
        return false;
    }
    return ret;
}

OverlayControlChannel::OverlayControlChannel() : mNoRot(false), mFD(-1), mRotFD(-1), mFormat3D(0) {
    memset(&mOVInfo, 0, sizeof(mOVInfo));
    memset(&m3DOVInfo, 0, sizeof(m3DOVInfo));
    memset(&mRotInfo, 0, sizeof(mRotInfo));
}


OverlayControlChannel::~OverlayControlChannel() {
    closeControlChannel();
}

bool OverlayControlChannel::getAspectRatioPosition(int w, int h, overlay_rect *rect)
{
    int width = w, height = h, x, y;
    int fbWidth  = getFBWidth();
    int fbHeight = getFBHeight();
    // width and height for YUV TILE format
    int tempWidth = w, tempHeight = h;
    /* Calculate the width and height if it is YUV TILE format*/
    if(getFormat() == HAL_PIXEL_FORMAT_YCbCr_420_SP_TILED) {
        tempWidth = w - (ALIGN(w, 64) - w);
        tempHeight = h - (ALIGN(h, 32) - h);
    }
    if (width * fbHeight > fbWidth * height) {
        height = fbWidth * height / width;
        EVEN_OUT(height);
        width = fbWidth;
    } else if (width * fbHeight < fbWidth * height) {
        width = fbHeight * width / height;
        EVEN_OUT(width);
        height = fbHeight;
    } else {
        width = fbWidth;
        height = fbHeight;
    }
    /* Scaling of upto a max of 8 times supported */
    if(width >(tempWidth * HW_OVERLAY_MAGNIFICATION_LIMIT)){
        width = HW_OVERLAY_MAGNIFICATION_LIMIT * tempWidth;
    }
    if(height >(tempHeight*HW_OVERLAY_MAGNIFICATION_LIMIT)) {
        height = HW_OVERLAY_MAGNIFICATION_LIMIT * tempHeight;
    }
    if (width > fbWidth) width = fbWidth;
    if (height > fbHeight) height = fbHeight;

    width = width * (1.0f - ActionSafe::getWidthRatio() / 100.0f);
    height = height * (1.0f - ActionSafe::getHeightRatio() / 100.0f);

    x = (fbWidth - width) / 2;
    y = (fbHeight - height) / 2;

    rect->x = x;
    rect->y = y;
    rect->w = width;
    rect->h = height;
    return true;
}

bool OverlayControlChannel::getPositionS3D(int channel, int format, overlay_rect *rect) {
    int wDisp = getFBWidth();
    int hDisp = getFBHeight();
    switch (format & OUTPUT_MASK_3D) {
    case HAL_3D_OUT_SIDE_BY_SIDE_MASK:
        if (channel == VG0_PIPE) {
            rect->x = 0;
            rect->y = 0;
            rect->w = wDisp/2;
            rect->h = hDisp;
        } else {
            rect->x = wDisp/2;
            rect->y = 0;
            rect->w = wDisp/2;
            rect->h = hDisp;
        }
        break;
    case HAL_3D_OUT_TOP_BOTTOM_MASK:
        if (channel == VG0_PIPE) {
            rect->x = 0;
            rect->y = 0;
            rect->w = wDisp;
            rect->h = hDisp/2;
        } else {
            rect->x = 0;
            rect->y = hDisp/2;
            rect->w = wDisp;
            rect->h = hDisp/2;
        }
        break;
    case HAL_3D_OUT_MONOSCOPIC_MASK:
        if (channel == VG1_PIPE) {
            rect->x = 0;
            rect->y = 0;
            rect->w = wDisp;
            rect->h = hDisp;
        }
        else
            return false;
        break;
    case HAL_3D_OUT_INTERLEAVE_MASK:
        break;
    default:
        reportError("Unsupported 3D output format");
        break;
    }
    return true;
}

bool OverlayControlChannel::openDevices(int fbnum) {
    if (fbnum < 0)
        return false;

    char const * const device_template =
                       "/dev/graphics/fb%u";
    char dev_name[64];
    snprintf(dev_name, 64, device_template, fbnum);

    mFD = open(dev_name, O_RDWR, 0);
    if (mFD < 0) {
        reportError("Cant open framebuffer ");
        return false;
    }

    fb_fix_screeninfo finfo;
    if (ioctl(mFD, FBIOGET_FSCREENINFO, &finfo) == -1) {
        reportError("FBIOGET_FSCREENINFO on fb1 failed");
        close(mFD);
        mFD = -1;
        return false;
    }

    fb_var_screeninfo vinfo;
    if (ioctl(mFD, FBIOGET_VSCREENINFO, &vinfo) == -1) {
        reportError("FBIOGET_VSCREENINFO on fb1 failed");
        close(mFD);
        mFD = -1;
        return false;
    }
    mFBWidth = vinfo.xres;
    mFBHeight = vinfo.yres;
    mFBbpp = vinfo.bits_per_pixel;
    mFBystride = finfo.line_length;

    if (!mNoRot) {
        mRotFD = open("/dev/msm_rotator", O_RDWR, 0);
        if (mRotFD < 0) {
            reportError("Cant open rotator device");
            close(mFD);
            mFD = -1;
            return false;
        }
    }

    return true;
}

bool OverlayControlChannel::setOverlayInformation(int w, int h,
                                  int format, int flags, int zorder,
                                  bool ignoreFB) {
    int origW, origH, xoff, yoff;

    mOVInfo.id = MSMFB_NEW_REQUEST;
    mOVInfo.src.width  = w;
    mOVInfo.src.height = h;
    mOVInfo.src_rect.x = 0;
    mOVInfo.src_rect.y = 0;
    mOVInfo.dst_rect.x = 0;
    mOVInfo.dst_rect.y = 0;
    mOVInfo.dst_rect.w = mFBWidth;
    mOVInfo.dst_rect.h = mFBHeight;
    if(format == MDP_Y_CRCB_H2V2_TILE) {
        if (mNoRot) {
           mOVInfo.src_rect.w = w - (ALIGN(w, 64) - w);
           mOVInfo.src_rect.h = h - (ALIGN(h, 32) - h);
        } else {
           mOVInfo.src_rect.w = w;
           mOVInfo.src_rect.h = h;
           mOVInfo.src.width  = ALIGN(w, 64);
           mOVInfo.src.height = ALIGN(h, 32);
           mOVInfo.src_rect.x = mOVInfo.src.width - w;
           mOVInfo.src_rect.y = mOVInfo.src.height - h;
        }
    } else {
        mOVInfo.src_rect.w = w;
        mOVInfo.src_rect.h = h;
    }

    mOVInfo.src.format = format;
    if (w > mFBWidth)
        mOVInfo.dst_rect.w = mFBWidth;
    if (h > mFBHeight)
        mOVInfo.dst_rect.h = mFBHeight;
    mOVInfo.z_order = zorder;
    mOVInfo.alpha = 0xff;
    mOVInfo.transp_mask = 0xffffffff;
    mOVInfo.flags = flags;
    if (!ignoreFB)
        mOVInfo.flags |= MDP_OV_PLAY_NOWAIT;
    mSize = get_size(format, w, h);
    return true;
}

bool OverlayControlChannel::startOVRotatorSessions(int w, int h,
                                          int format) {
    bool ret = true;
    if (!mNoRot) {
        mRotInfo.src.format = format;
        mRotInfo.src.width = w;
        mRotInfo.src.height = h;
        mRotInfo.src_rect.w = w;
        mRotInfo.src_rect.h = h;
        mRotInfo.dst.width = w;
        mRotInfo.dst.height = h;
        if(format == MDP_Y_CRCB_H2V2_TILE) {
           mRotInfo.src.width =  ALIGN(w, 64);
           mRotInfo.src.height = ALIGN(h, 32);
           mRotInfo.src_rect.w = ALIGN(w, 64);
           mRotInfo.src_rect.h = ALIGN(h, 32);
           mRotInfo.dst.width = ALIGN(w, 64);
           mRotInfo.dst.height = ALIGN(h, 32);
           mRotInfo.dst.format = MDP_Y_CRCB_H2V2;
        } else {
           mRotInfo.dst.format = format;
        }
        mRotInfo.dst_x = 0;
        mRotInfo.dst_y = 0;
        mRotInfo.src_rect.x = 0;
        mRotInfo.src_rect.y = 0;
        mRotInfo.rotations = 0;
        mRotInfo.enable = 0;
        if(mUIChannel)
            mRotInfo.enable = 1;
        mRotInfo.session_id = 0;
        int result = ioctl(mRotFD, MSM_ROTATOR_IOCTL_START, &mRotInfo);
        if (result) {
            reportError("Rotator session failed");
            ret = false;
        }
    }

    if (ret && ioctl(mFD, MSMFB_OVERLAY_SET, &mOVInfo)) {
        reportError("startOVRotatorSessions, Overlay set failed");
        ret = false;
    }

    if (!ret)
        closeControlChannel();

    return ret;
}

bool OverlayControlChannel::startControlChannel(int w, int h,
                                           int format, int fbnum, bool norot,
                                           bool uichannel,
                                           unsigned int format3D, int zorder,
                                           bool ignoreFB) {
    mNoRot = norot;
    mFormat = format;
    mUIChannel = uichannel;
    fb_fix_screeninfo finfo;
    fb_var_screeninfo vinfo;
    int hw_format;
    int flags = 0;
    int colorFormat = format;
    if (format & INTERLACE_MASK) {
        flags |= MDP_DEINTERLACE;

        // Get the actual format
        colorFormat = format ^ HAL_PIXEL_FORMAT_INTERLACE;
    }
    hw_format = get_mdp_format(colorFormat);
    if (hw_format < 0) {
        reportError("Unsupported format");
        return false;
    }

    mFormat3D = format3D;
    if ( !mFormat3D || (mFormat3D && HAL_3D_OUT_MONOSCOPIC_MASK) ) {
        // Set the share bit for sharing the VG pipe
        flags |= MDP_OV_PIPE_SHARE;
    }
    if (!openDevices(fbnum))
        return false;

    if (!setOverlayInformation(w, h, hw_format, flags, zorder, ignoreFB))
        return false;

    return startOVRotatorSessions(w, h, hw_format);
}

bool OverlayControlChannel::closeControlChannel() {
    if (!isChannelUP())
        return true;

    if (!mNoRot && mRotFD > 0) {
        ioctl(mRotFD, MSM_ROTATOR_IOCTL_FINISH, &(mRotInfo.session_id));
        close(mRotFD);
        mRotFD = -1;
    }

    int ovid = mOVInfo.id;
    ioctl(mFD, MSMFB_OVERLAY_UNSET, &ovid);
    if (m3DOVInfo.is_3d) {
        m3DOVInfo.is_3d = 0;
        ioctl(mFD, MSMFB_OVERLAY_3D, &m3DOVInfo);
    }

    close(mFD);
    memset(&mOVInfo, 0, sizeof(mOVInfo));
    memset(&mRotInfo, 0, sizeof(mRotInfo));
    memset(&m3DOVInfo, 0, sizeof(m3DOVInfo));
    mFD = -1;

    return true;
}

bool OverlayControlChannel::setSource(uint32_t w, uint32_t h,
                        int cFormat, int orientation, bool ignoreFB) {
    int format = cFormat & INTERLACE_MASK ?
                (cFormat ^ HAL_PIXEL_FORMAT_INTERLACE) : cFormat;
    format = get_mdp_format(format);
    if (orientation == mOrientation && orientation != 0){
        //set format to non-tiled and align w, h to 64-bit and 32-bit respectively.
        if (format == MDP_Y_CRCB_H2V2_TILE) {
            format = MDP_Y_CRCB_H2V2;
             w = ALIGN(w, 64);
             h = ALIGN(h, 32);
        }
        switch(orientation){
            case (HAL_TRANSFORM_ROT_90 | HAL_TRANSFORM_FLIP_H):
            case (HAL_TRANSFORM_ROT_90 | HAL_TRANSFORM_FLIP_V):
            case HAL_TRANSFORM_ROT_90:
            case HAL_TRANSFORM_ROT_270:
                {
                    int tmp = w;
                    w = h;
                    h = tmp;
                }
            default:
                break;
        }
    }

    if (w == mOVInfo.src.width && h == mOVInfo.src.height
            && format == mOVInfo.src.format && orientation == mOrientation) {
        mdp_overlay ov;
        ov.id = mOVInfo.id;
        if (ioctl(mFD, MSMFB_OVERLAY_GET, &ov))
            return false;
        mOVInfo = ov;
        int flags = mOVInfo.flags;

        if (!ignoreFB)
            mOVInfo.flags |= MDP_OV_PLAY_NOWAIT;
        else
            mOVInfo.flags &= ~MDP_OV_PLAY_NOWAIT;

        if (flags != mOVInfo.flags) {
            if (ioctl(mFD, MSMFB_OVERLAY_SET, &mOVInfo))
                return false;
        }

        return true;
    }
    mOrientation = orientation;
    return false;
}

bool OverlayControlChannel::setPosition(int x, int y, uint32_t w, uint32_t h) {

    int width = w, height = h;
    if (!isChannelUP() ||
           (x < 0) || (y < 0) || ((x + w) > mFBWidth) ||
           ((y + h) > mFBHeight)) {
        reportError("setPosition failed");
        return false;
    }

    mdp_overlay ov;
    ov.id = mOVInfo.id;
    if (ioctl(mFD, MSMFB_OVERLAY_GET, &ov)) {
        reportError("setPosition, overlay GET failed");
        return false;
    }

    /* Scaling of upto a max of 8 times supported */
    if(w >(ov.src_rect.w * HW_OVERLAY_MAGNIFICATION_LIMIT)){
        w = HW_OVERLAY_MAGNIFICATION_LIMIT * ov.src_rect.w;
        x = (mFBWidth - w) / 2;
    }
    if(h >(ov.src_rect.h * HW_OVERLAY_MAGNIFICATION_LIMIT)) {
        h = HW_OVERLAY_MAGNIFICATION_LIMIT * ov.src_rect.h;
        y = (mFBHeight - h) / 2;
   }

    //Use even values for destination co-ordinates, because
    //in case of some videos on rotation, the hardware
    //picks up a line from the padding in source, if odd
    //value are used.
    EVEN_OUT(x);
    EVEN_OUT(y);
    EVEN_OUT(w);
    EVEN_OUT(h);

    ov.dst_rect.x = x;
    ov.dst_rect.y = y;
    ov.dst_rect.w = w;
    ov.dst_rect.h = h;

    if (ioctl(mFD, MSMFB_OVERLAY_SET, &ov)) {
        reportError("setPosition, Overlay SET failed");
        return false;
    }
    mOVInfo = ov;

    return true;
}

void OverlayControlChannel::swapOVRotWidthHeight() {
    int tmp = mOVInfo.src.width;
    mOVInfo.src.width = mOVInfo.src.height;
    mOVInfo.src.height = tmp;

    tmp = mOVInfo.src_rect.h;
    mOVInfo.src_rect.h = mOVInfo.src_rect.w;
    mOVInfo.src_rect.w = tmp;

    tmp = mRotInfo.dst.width;
    mRotInfo.dst.width = mRotInfo.dst.height;
    mRotInfo.dst.height = tmp;
}

bool OverlayControlChannel::useVirtualFB() {
    if(!m3DOVInfo.is_3d) {
        m3DOVInfo.is_3d = 1;
        mFBWidth *= 2;
        mFBHeight /= 2;
        m3DOVInfo.width = mFBWidth;
        m3DOVInfo.height = mFBHeight;
        return ioctl(mFD, MSMFB_OVERLAY_3D, &m3DOVInfo) ? false : true;
    }
    return true;
}

bool OverlayControlChannel::setParameter(int param, int value, bool fetch) {
    if (!isChannelUP())
        return false;

    if(param == OVERLAY_TRANSFORM && (value == mOVInfo.user_data[0]))
        return true;

    mdp_overlay ov = mOVInfo;
    if (fetch && ioctl(mFD, MSMFB_OVERLAY_GET, &ov)) {
        reportError("setParameter, overlay GET failed");
        return false;
    }
    mOVInfo = ov;

    switch (param) {
    case OVERLAY_DITHER:
        break;
    case OVERLAY_TRANSFORM:
    {
        int val = mOVInfo.user_data[0];
        if (mNoRot)
            return true;

        int rot = value;
        int flip = 0;

        switch(rot) {
        case 0:
        case HAL_TRANSFORM_FLIP_H:
        case HAL_TRANSFORM_FLIP_V:
        {
            if (val == MDP_ROT_90) {
                    int tmp = mOVInfo.src_rect.y;
                    mOVInfo.src_rect.y = mOVInfo.src.width -
                            (mOVInfo.src_rect.x + mOVInfo.src_rect.w);
                    mOVInfo.src_rect.x = tmp;
                    swapOVRotWidthHeight();
            }
            else if (val == MDP_ROT_270) {
                    int tmp = mOVInfo.src_rect.x;
                    mOVInfo.src_rect.x = mOVInfo.src.height - (
                            mOVInfo.src_rect.y + mOVInfo.src_rect.h);
                    mOVInfo.src_rect.y = tmp;
                    swapOVRotWidthHeight();
            }
            rot = 0;
            flip = value & (HAL_TRANSFORM_FLIP_H|HAL_TRANSFORM_FLIP_V);
            break;
        }
        case HAL_TRANSFORM_ROT_90:
        case (HAL_TRANSFORM_ROT_90|HAL_TRANSFORM_FLIP_H):
        case (HAL_TRANSFORM_ROT_90|HAL_TRANSFORM_FLIP_V):
        {
            if (val == MDP_ROT_270) {
                    mOVInfo.src_rect.x = mOVInfo.src.width - (
                            mOVInfo.src_rect.x + mOVInfo.src_rect.w);
                    mOVInfo.src_rect.y = mOVInfo.src.height - (
                    mOVInfo.src_rect.y + mOVInfo.src_rect.h);
            }
            else if (val == MDP_ROT_NOP || val == MDP_ROT_180) {
                    int tmp = mOVInfo.src_rect.x;
                    mOVInfo.src_rect.x = mOVInfo.src.height -
                               (mOVInfo.src_rect.y + mOVInfo.src_rect.h);
                    mOVInfo.src_rect.y = tmp;
                    swapOVRotWidthHeight();
            }
            rot = HAL_TRANSFORM_ROT_90;
            flip = value & (HAL_TRANSFORM_FLIP_H|HAL_TRANSFORM_FLIP_V);
            break;
        }
        case HAL_TRANSFORM_ROT_180:
        {
            if (val == MDP_ROT_270) {
                    int tmp = mOVInfo.src_rect.y;
                    mOVInfo.src_rect.y = mOVInfo.src.width -
                               (mOVInfo.src_rect.x + mOVInfo.src_rect.w);
                    mOVInfo.src_rect.x = tmp;
                    swapOVRotWidthHeight();
            }
            else if (val == MDP_ROT_90) {
                    int tmp = mOVInfo.src_rect.x;
                    mOVInfo.src_rect.x = mOVInfo.src.height - (
                             mOVInfo.src_rect.y + mOVInfo.src_rect.h);
                    mOVInfo.src_rect.y = tmp;
                    swapOVRotWidthHeight();
            }
            break;
        }
        case HAL_TRANSFORM_ROT_270:
        {
            if (val == MDP_ROT_90) {
                    mOVInfo.src_rect.y = mOVInfo.src.height -
                               (mOVInfo.src_rect.y + mOVInfo.src_rect.h);
                    mOVInfo.src_rect.x = mOVInfo.src.width -
                               (mOVInfo.src_rect.x + mOVInfo.src_rect.w);
            }
            else if (val == MDP_ROT_NOP || val == MDP_ROT_180) {
                    int tmp = mOVInfo.src_rect.y;
                    mOVInfo.src_rect.y = mOVInfo.src.width - (
                        mOVInfo.src_rect.x + mOVInfo.src_rect.w);
                    mOVInfo.src_rect.x = tmp;
                    swapOVRotWidthHeight();
            }
            break;
        }
        default: return false;
    }
    int mdp_rotation = get_mdp_orientation(rot, flip);
    if (mdp_rotation == -1)
        return false;

    mOVInfo.user_data[0] = mdp_rotation;
    mRotInfo.rotations = mOVInfo.user_data[0];

    /* Rotator always outputs non-tiled formats.
    If rotator is used, set Overlay input to non-tiled
    Else, overlay input remains tiled */

    if (mOVInfo.user_data[0]) {
        if (mRotInfo.src.format == MDP_Y_CRCB_H2V2_TILE)
            mOVInfo.src.format = MDP_Y_CRCB_H2V2;
        mRotInfo.enable = 1;
    }
    else {
        if(mRotInfo.src.format == MDP_Y_CRCB_H2V2_TILE)
            mOVInfo.src.format = MDP_Y_CRCB_H2V2_TILE;
        mRotInfo.enable = 0;
        if(mUIChannel)
            mRotInfo.enable = 1;
    }
    if (ioctl(mRotFD, MSM_ROTATOR_IOCTL_START, &mRotInfo)) {
        reportError("setParameter, rotator start failed");
        return false;
    }

    if ((mOVInfo.user_data[0] == MDP_ROT_90) ||
        (mOVInfo.user_data[0] == MDP_ROT_270))
        mOVInfo.flags |= MDP_SOURCE_ROTATED_90;
    else
        mOVInfo.flags &= ~MDP_SOURCE_ROTATED_90;

    if (ioctl(mFD, MSMFB_OVERLAY_SET, &mOVInfo)) {
        reportError("setParameter, overlay set failed");
        return false;
    }
        break;
    }
    default:
        reportError("Unsupproted param");
    return false;
    }

    return true;
}

bool OverlayControlChannel::getPosition(int& x, int& y,
                                  uint32_t& w, uint32_t& h) {
    if (!isChannelUP())
        return false;
    //mOVInfo has the current Overlay Position
    x = mOVInfo.dst_rect.x;
    y = mOVInfo.dst_rect.y;
    w = mOVInfo.dst_rect.w;
    h = mOVInfo.dst_rect.h;

    return true;
}

bool OverlayControlChannel::getOrientation(int& orientation) const {
    if (!isChannelUP())
        return false;
    // mOVInfo has the current orientation
    orientation = mOVInfo.user_data[0];
    return true;
}
bool OverlayControlChannel::getOvSessionID(int& sessionID) const {
    if (!isChannelUP())
        return false;
    sessionID = mOVInfo.id;
    return true;
}

bool OverlayControlChannel::getRotSessionID(int& sessionID) const {
    if (!isChannelUP())
        return false;
    sessionID = mRotInfo.session_id;
    return true;
}

bool OverlayControlChannel::getSize(int& size) const {
    if (!isChannelUP())
        return false;
    size = mSize;
    return true;
}

OverlayDataChannel::OverlayDataChannel() : mNoRot(false), mFD(-1), mRotFD(-1),
                                  mPmemFD(-1), mPmemAddr(0) {
}

OverlayDataChannel::~OverlayDataChannel() {
    closeDataChannel();
}

bool OverlayDataChannel::startDataChannel(
               const OverlayControlChannel& objOvCtrlChannel,
               int fbnum, bool norot, bool uichannel, int num_buffers) {
    int ovid, rotid, size;
    mNoRot = norot;
    memset(&mOvData, 0, sizeof(mOvData));
    memset(&mOvDataRot, 0, sizeof(mOvDataRot));
    memset(&mRotData, 0, sizeof(mRotData));
    if (objOvCtrlChannel.getOvSessionID(ovid) &&
            objOvCtrlChannel.getRotSessionID(rotid) &&
            objOvCtrlChannel.getSize(size)) {
        return startDataChannel(ovid, rotid, size, fbnum,
                      norot, uichannel, num_buffers);
    }
    else
        return false;
}

bool OverlayDataChannel::openDevices(int fbnum, bool uichannel, int num_buffers) {
    if (fbnum < 0)
        return false;
    char const * const device_template =
                      "/dev/graphics/fb%u";
    char dev_name[64];
    snprintf(dev_name, 64, device_template, fbnum);

    mFD = open(dev_name, O_RDWR, 0);
    if (mFD < 0) {
        reportError("Cant open framebuffer ");
        return false;
    }
    if (!mNoRot) {
        mRotFD = open("/dev/msm_rotator", O_RDWR, 0);
        if (mRotFD < 0) {
            reportError("Cant open rotator device");
            close(mFD);
            mFD = -1;
            return false;
        }

        mPmemAddr = MAP_FAILED;

        if(!uichannel) {
            mPmemFD = open("/dev/pmem_smipool", O_RDWR | O_SYNC);
            if(mPmemFD >= 0)
                mPmemAddr = (void *) mmap(NULL, mPmemOffset * num_buffers, PROT_READ | PROT_WRITE,
                         MAP_SHARED, mPmemFD, 0);
        }

        if (mPmemAddr == MAP_FAILED) {
            mPmemFD = open("/dev/pmem_adsp", O_RDWR | O_SYNC);
            if (mPmemFD < 0) {
                reportError("Cant open pmem_adsp ");
                close(mFD);
                mFD = -1;
                close(mRotFD);
                mRotFD = -1;
                return false;
           } else {
                mPmemAddr = (void *) mmap(NULL, mPmemOffset * num_buffers, PROT_READ | PROT_WRITE,
                    MAP_SHARED, mPmemFD, 0);
                if (mPmemAddr == MAP_FAILED) {
                    reportError("Cant map pmem_adsp ");
                    close(mFD);
                    mFD = -1;
                    close(mPmemFD);
                    mPmemFD = -1;
                    close(mRotFD);
                    mRotFD = -1;
                    return false;
                }
            }
        }

        mOvDataRot.data.memory_id = mPmemFD;
        mRotData.dst.memory_id = mPmemFD;
        mRotData.dst.offset = 0;
        mNumBuffers = num_buffers;
        mCurrentItem = 0;
        for (int i = 0; i < num_buffers; i++)
            mRotOffset[i] = i * mPmemOffset;
    }

    return true;
}

bool OverlayDataChannel::startDataChannel(int ovid, int rotid, int size,
                                   int fbnum, bool norot,
                                   bool uichannel, int num_buffers) {
    memset(&mOvData, 0, sizeof(mOvData));
    memset(&mOvDataRot, 0, sizeof(mOvDataRot));
    memset(&mRotData, 0, sizeof(mRotData));
    mNoRot = norot;
    mOvData.data.memory_id = -1;
    mOvData.id = ovid;
    mOvDataRot = mOvData;
    mPmemOffset = size;
    mRotData.session_id = rotid;
    mNumBuffers = 0;
    mCurrentItem = 0;

    return openDevices(fbnum, uichannel, num_buffers);
}

bool OverlayDataChannel::closeDataChannel() {
    if (!isChannelUP())
        return true;

    if (!mNoRot && mRotFD > 0) {
        munmap(mPmemAddr, mPmemOffset * mNumBuffers);
        close(mPmemFD);
        mPmemFD = -1;
        close(mRotFD);
        mRotFD = -1;
    }
    close(mFD);
    mFD = -1;
    memset(&mOvData, 0, sizeof(mOvData));
    memset(&mOvDataRot, 0, sizeof(mOvDataRot));
    memset(&mRotData, 0, sizeof(mRotData));

    mNumBuffers = 0;
    mCurrentItem = 0;

    return true;
}

bool OverlayDataChannel::setFd(int fd) {
    mOvData.data.memory_id = fd;
    return true;
}

bool OverlayDataChannel::queueBuffer(uint32_t offset) {
    if ((!isChannelUP()) || mOvData.data.memory_id < 0) {
        reportError("QueueBuffer failed, either channel is not set or no file descriptor to read from");
        return false;
    }

    msmfb_overlay_data *odPtr;
    mOvData.data.offset = offset;
    odPtr = &mOvData;

    if (!mNoRot) {
        mRotData.src.memory_id = mOvData.data.memory_id;
        mRotData.src.offset = offset;
        mRotData.dst.offset = (mRotData.dst.offset) ? 0 : mPmemOffset;
        mRotData.dst.offset = mRotOffset[mCurrentItem];
        mCurrentItem = (mCurrentItem + 1) % mNumBuffers;

        int result = ioctl(mRotFD,
                               MSM_ROTATOR_IOCTL_ROTATE, &mRotData);

        if (!result) {
            mOvDataRot.data.offset = (uint32_t) mRotData.dst.offset;
            odPtr = &mOvDataRot;
        }
    }

    if (ioctl(mFD, MSMFB_OVERLAY_PLAY, odPtr)) {
        reportError("overlay play failed.");
        return false;
    }

    return true;
}

bool OverlayDataChannel::getCropS3D(overlay_rect *inRect, int channel, int format,
                                    overlay_rect *rect) {
    // for the 3D usecase extract channels from a frame
    switch (format & INPUT_MASK_3D) {
    case HAL_3D_IN_SIDE_BY_SIDE_L_R:
        if(channel == 0) {
            rect->x = 0;
            rect->y = 0;
            rect->w = inRect->w/2;
            rect->h = inRect->h;
        } else {
            rect->x = inRect->w/2;
            rect->y = 0;
            rect->w = inRect->w/2;
            rect->h = inRect->h;
        }
        break;
    case HAL_3D_IN_SIDE_BY_SIDE_R_L:
         if(channel == 1) {
            rect->x = 0;
            rect->y = 0;
            rect->w = inRect->w/2;
            rect->h = inRect->h;
        } else {
            rect->x = inRect->w/2;
            rect->y = 0;
            rect->w = inRect->w/2;
            rect->h = inRect->h;
        }
         break;
    case HAL_3D_IN_TOP_BOTTOM:
        if(channel == 0) {
            rect->x = 0;
            rect->y = 0;
            rect->w = inRect->w;
            rect->h = inRect->h/2;
        } else {
            rect->x = 0;
            rect->y = inRect->h/2;
            rect->w = inRect->w;
            rect->h = inRect->h/2;
        }
        break;
    case HAL_3D_IN_INTERLEAVE:
      break;
    default:
        reportError("Unsupported 3D format...");
        break;
   }
   return true;
}

bool OverlayDataChannel::setCrop(uint32_t x, uint32_t y, uint32_t w, uint32_t h) {
    if (!isChannelUP()) {
        reportError("Channel not set");
        return false;
    }

    mdp_overlay ov;
    ov.id = mOvData.id;
    if (ioctl(mFD, MSMFB_OVERLAY_GET, &ov)) {
        reportError("setCrop, overlay GET failed");
        return false;
    }

    if ((ov.user_data[0] == MDP_ROT_90) ||
        (ov.user_data[0] == (MDP_ROT_90 | MDP_FLIP_UD)) ||
        (ov.user_data[0] == (MDP_ROT_90 | MDP_FLIP_LR))){
        if (ov.src.width < (y + h))
            return false;

        uint32_t tmp = x;
        x = ov.src.width - (y + h);
        y = tmp;

        tmp = w;
        w = h;
        h = tmp;
    }
    else if (ov.user_data[0] == MDP_ROT_270) {
        if (ov.src.height < (x + w))
            return false;

        uint32_t tmp = y;
        y = ov.src.height - (x + w);
        x = tmp;

        tmp = w;
        w = h;
        h = tmp;
    }
    else if(ov.user_data[0] == MDP_ROT_180) {
        if ((ov.src.height < (y + h)) || (ov.src.width < ( x + w)))
           return false;

        x = ov.src.width - (x + w);
        y = ov.src.height - (y + h);
    }


    if ((ov.src_rect.x == x) &&
           (ov.src_rect.y == y) &&
           (ov.src_rect.w == w) &&
           (ov.src_rect.h == h))
        return true;

    ov.src_rect.x = x;
    ov.src_rect.y = y;
    ov.src_rect.w = w;
    ov.src_rect.h = h;

    /* Scaling of upto a max of 8 times supported */
    if(ov.dst_rect.w >(ov.src_rect.w * HW_OVERLAY_MAGNIFICATION_LIMIT)){
        ov.dst_rect.w = HW_OVERLAY_MAGNIFICATION_LIMIT * ov.src_rect.w;
    }
    if(ov.dst_rect.h >(ov.src_rect.h * HW_OVERLAY_MAGNIFICATION_LIMIT)) {
        ov.dst_rect.h = HW_OVERLAY_MAGNIFICATION_LIMIT * ov.src_rect.h;
    }
    if (ioctl(mFD, MSMFB_OVERLAY_SET, &ov)) {
        reportError("setCrop, overlay set error");
        return false;
    }

    return true;
}

float ActionSafe::widthRatio = 0.0f;
float ActionSafe::heightRatio = 0.0f;
