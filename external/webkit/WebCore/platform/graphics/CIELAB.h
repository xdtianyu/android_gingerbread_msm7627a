/*
* Copyright (C) 2011, Code Aurora Forum. All rights reserved.
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

#ifndef CIELAB_h
#define CIELAB_h

namespace WebCore {

typedef unsigned RGBA32;
typedef unsigned short RGB565;

// Implementation of CIE XYZ Color space and CIE XYZ <-> RGB conversion.
class CIEXYZ {
public:

    CIEXYZ(float x, float y, float z)
        : m_X(x), m_Y(y), m_Z(z)
    {
    }

    CIEXYZ(int r, int g, int b);

    float X() const { return m_X; }
    float Y() const { return m_Y; }
    float Z() const { return m_Z; }

    RGBA32 rgb() const;

private:
    float m_X;
    float m_Y;
    float m_Z;
};

// Implementation of CIE L*a*b Color space and CIE L*a*b <-> CIE XYZ conversion.
class CIELAB {
public:
    CIELAB(RGBA32 color);
    CIELAB(int r, int g, int b);
    CIELAB(float r, float g, float b);

    void setL(float l) { m_L = l; }
    float L() const { return m_L; }
    float A() const { return m_A; }
    float B() const { return m_B; }

    RGBA32 rgb() const;

    static void initInversionTable();
    static RGB565 invert(RGB565 color);
    static RGBA32 invert(RGBA32 color);
    static RGBA32 invert(unsigned r, unsigned g, unsigned b);
    static RGBA32 invert(unsigned r, unsigned g, unsigned b, unsigned a);
    static RGBA32 invert(float r, float g, float b);

private:
    void init(const CIEXYZ& xyz);

    float m_L;
    float m_A;
    float m_B;
};

}

#endif //CIELAB_h
