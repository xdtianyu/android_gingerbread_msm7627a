/*
* Copyright (c) 2011, Code Aurora Forum. All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are
* met:
*     * Redistributions of source code must retain the above copyright
*       notice, this list of conditions and the following disclaimer.
*     * Redistributions in binary form must reproduce the above
*       copyright notice, this list of conditions and the following
*       disclaimer in the documentation and/or other materials provided
*       with the distribution.
*     * Neither the name of Code Aurora Forum, Inc. nor the names of its
*       contributors may be used to endorse or promote products derived
*       from this software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
* WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
* ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
* BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
* BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
* WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
* OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
* IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef WebTech_Renderer2D_h
#define WebTech_Renderer2D_h

#include "RefCount.h"

namespace WebTech {

class IRenderer2D : public virtual IRefCount {
public:
    virtual ~IRenderer2D() {};
    virtual void initBacked(int width, int height) = 0;
    virtual void init(int left, int top, int width, int height) = 0;
    virtual void lockBuffer() = 0;
    virtual void unlockBuffer() = 0;
    virtual void* getPixels() = 0;
    virtual int getPitch() = 0;
    virtual int width() = 0;
    virtual int height() = 0;
    virtual void setCacheSize(unsigned int size) = 0;

    //////////////////drawing/////////////////////
    virtual void clear(float r, float g, float b, float a) = 0;

    virtual void drawTexture(unsigned int texture, int width, int height,
        int inX, int inY, int inX2, int inY2,
        int outX, int outY, int outX2, int outY2, bool filter, bool blend) = 0;

    virtual void drawImage(void * ptr, int width, int height, int pitch, unsigned int key, bool reload,
        int inX, int inY, int inX2, int inY2,
        int outX, int outY, int outX2, int outY2) = 0;

};

// create an IBackingStore object
extern "C" IRenderer2D* createRenderer2D();
typedef IRenderer2D* (*CreateRenderer2D_t)();

const char* const CreateRenderer2DFuncName = "createRenderer2D";
}; // WebTech

#endif // WebTech_Renderer2D_h

