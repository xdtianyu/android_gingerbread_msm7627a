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

#include "config.h"
#include "CIELAB.h"

namespace WebCore {

inline RGB565 makeRGB565(unsigned r, unsigned g, unsigned b)
{
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
}

inline RGB565 makeRGB565(float fr, float fg, float fb)
{
    unsigned r = static_cast<unsigned>(fr * 0x1f) & 0x1f;
    unsigned g = static_cast<unsigned>(fg * 0x3f) & 0x3f;
    unsigned b = static_cast<unsigned>(fb * 0x1f) & 0x1f;
    return (r << 11) | (g << 5) | b ;
}

inline RGB565 makeRGB565(RGBA32 color)
{
    unsigned r = (color >> 19) & 0x1F;
    unsigned g = (color >> 10) & 0x3F;
    unsigned b = (color >> 3 ) & 0x1F;
    return (r << 11) | (g << 5) | b;
}

inline RGB565 makeRGB565(unsigned char r, unsigned char g, unsigned char b)
{
    return ((static_cast<unsigned>(r) >> 3) << 11) | ((static_cast<unsigned>(g) >> 2) << 5) | (static_cast<unsigned>(b) >> 3);
}

// sRGB <=> CIEXYZ conversion algorithm
// Reference: http://www.w3.org/Graphics/Color/sRGB Section 1.1
CIEXYZ::CIEXYZ(int ir, int ig, int ib)
{
    float r = ir / 255.0f;
    float g = ig / 255.0f;
    float b = ib / 255.0f;

    if (r > 0.03928f)
        r = pow((r + 0.055f) / 1.055f, 2.4f);
    else
        r = r / 12.92f;

    if (g > 0.03928f)
        g = pow((g + 0.055f) / 1.055f, 2.4f);
    else
        g = g / 12.92f;

    if (b > 0.03928f)
        b = pow((b + 0.055f) / 1.055f, 2.4f);
    else
        b = b / 12.92f;

    m_X = (r * 0.4124f) + (g * 0.3576f) + (b * 0.1805f);
    m_Y = (r * 0.2126f) + (g * 0.7152f) + (b * 0.0722f);
    m_Z = (r * 0.0193f) + (g * 0.1192f) + (b * 0.9505f);
}

RGBA32 CIEXYZ::rgb() const
{
    float r = m_X *  3.2406f + m_Y * -1.5372f + m_Z * -0.4986f;
    float g = m_X * -0.9692f + m_Y *  1.8760f + m_Z *  0.0416f;
    float b = m_X *  0.0556f + m_Y * -0.2040f + m_Z *  1.0570f;

    float t = 1.0f / 2.4f;

    if (r > 0.00304f)
        r = 1.055f * pow(r, t) - 0.055f;
    else
        r = 12.92f * r;

    if (g > 0.00304f)
        g = 1.055f * pow(g, t) - 0.055f;
    else
        g = 12.92f * g;

    if (b > 0.00304f)
        b = 1.055f * pow(b, t) - 0.055f;
    else
        b = 12.92f * b;

    r = r * 255.0f;
    if (r < 0.0f)
        r = 0.0f ;
    else if (r > 255.0f)
        r = 255.0f ;

    g = g * 255.0f;
    if (g < 0.0f)
        g = 0.0f ;
    else if (g > 255.0f)
        g = 255.0f ;

    b = b * 255.0f;
    if (b < 0.0f)
        b = 0.0f ;
    else if (b > 255.0f)
        b = 255.0f ;

    return (0xFF000000 | (static_cast<unsigned>(r) << 16 | static_cast<unsigned>(g) << 8 | static_cast<unsigned>(b)));
}

static unsigned int s_inversionTable[0xFFFF];
static bool s_init = false;

void CIELAB::initInversionTable()
{
    if (s_init)
        return;

    float r, g, b;
    s_init = true;

    for (unsigned int i = 0; i < 0xFFFF; i++)
    {
        r = static_cast<float>(((i) >> 11) & 0x1f) / 0x1f;
        g = static_cast<float>(((i) >>  5) & 0x3f) / 0x3f;
        b = static_cast<float>( (i)        & 0x1f) / 0x1f;
        CIELAB lab(r, g, b);
        lab.m_L = 100.0f - lab.m_L;
        s_inversionTable[i] = (lab.rgb() & 0x00FFFFFF);
    }
}

RGB565 CIELAB::invert(RGB565 color)
{
    unsigned char *c = reinterpret_cast<unsigned char*>(&s_inversionTable[color]);
    return makeRGB565(c[1], c[2], c[3]);
}

RGBA32 CIELAB::invert(RGBA32 color)
{
    return s_inversionTable[makeRGB565(color)];
}

RGBA32 CIELAB::invert(float r, float g, float b)
{
    return s_inversionTable[makeRGB565(r, g, b)];
}

RGBA32 CIELAB::invert(unsigned r, unsigned g, unsigned b)
{
    return s_inversionTable[makeRGB565(r, g, b)];
}

RGBA32 CIELAB::invert(unsigned r, unsigned g, unsigned b, unsigned a)
{
    return (a << 24 & 0xFF000000) | s_inversionTable[makeRGB565(r, g, b)];
}

// CIEXYZ <=> CIEL*a*b conversion algorithm
// Reference: CIE Publication 15.2 (1986) Section 4.2 (http://www.cie.co.at/)
CIELAB::CIELAB(RGBA32 color)
{
    int r = (color >> 16) & 0xFF;
    int g = (color >> 8) & 0xFF;
    int b = color & 0xFF;

    init(CIEXYZ(r, g, b));
}

CIELAB::CIELAB(float r, float g, float b)
{
    init(CIEXYZ(static_cast<int>(r * 255.0f), static_cast<int>(g * 255.0f), static_cast<int>(b * 255.0f)));
}

CIELAB::CIELAB(int r, int g, int b)
{
    init(CIEXYZ(r, g, b));
}

void CIELAB::init(const CIEXYZ& xyz)
{
    float x = xyz.X() / 0.95047f;
    float y = xyz.Y();
    float z = xyz.Z() / 1.08883f;

    float t = 16.0f / 116.0f;
    float cr = 1.0f / 3.0f;

    if (x > 0.008856f)
        x = pow(x, cr);
    else
        x = (7.787f * x) + t;

    if (y > 0.008856f)
        y = pow(y, cr);
    else
        y = (7.787f * y) + t;

    if (z > 0.008856f)
        z = pow(z, cr);
    else
        z = (7.787f * z) + t;

    m_L = (116.0f * y) - 16.0f;
    m_A = 500.0f * (x - y);
    m_B = 200.0f * (y - z);
}

RGBA32 CIELAB::rgb() const
{
    float y = (m_L + 16.0f) / 116.0f;
    float x = (m_A / 500.0f) + y;
    float z = y - (m_B / 200.0f);

    float xcube = x * x * x;
    float ycube = y * y * y;
    float zcube = z * z * z;

    float t = 16.0f/116.0f;

    if (xcube > 0.008856f)
        x = xcube;
    else
        x = (x - t) / 7.787f;

    if (ycube > 0.008856f)
        y = ycube;
    else
        y = (y - t) / 7.787f;

    if (zcube > 0.008856f)
        z = zcube;
    else
        z = (z - t) / 7.787f;

    x = x * 0.95047f;
    z = z * 1.08883f;

    CIEXYZ xyz(x, y, z);

    return xyz.rgb();
}

}
