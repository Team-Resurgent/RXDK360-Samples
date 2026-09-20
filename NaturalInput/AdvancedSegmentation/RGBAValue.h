//--------------------------------------------------------------------------------------
// RGBAValue.h
//
// Simple wrapper class for an 8888 (r,g,b,a) color. Ordering is as per A8R8G8B8 texture
// for easy copying / working with data from the NUI camera and D3D.
//
// Advanced Technology Group
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#ifndef RGBAVALUE_H
#define RGBAVALUE_H

//--------------------------------------------------------------------------------------
// RGBAValue class
//--------------------------------------------------------------------------------------
class RGBAValue
{
public:
    // Default constructor
    __forceinline RGBAValue()
    {
    }

    // Construction from components
    __forceinline RGBAValue( const UINT r, const UINT g, const UINT b )
    {
        m_Value = ( b << BLUE_SHIFT ) | ( g << GREEN_SHIFT ) | ( r << RED_SHIFT ) | FIXED_ALPHA;
    }

    // DWORD cast operator
    __forceinline operator DWORD() const
    {
        return m_Value;
    }

    // Copy constructor
    __forceinline RGBAValue( const RGBAValue& other ) : m_Value( other.m_Value )
    {
    }

    // Construction from color DWORD
    __forceinline RGBAValue( const DWORD word )
    {
        m_Value = word;
    }

    // Set individual channel values
    __forceinline VOID  SetRed( const UINT red )
    {
        m_Value &= RED_MASK;
        m_Value |= ( red << RED_SHIFT );
    }

    __forceinline VOID  SetGreen( const UINT green )
    {
        m_Value &= GREEN_MASK;
        m_Value |= ( green << GREEN_SHIFT );
    }

    __forceinline VOID  SetBlue( const UINT blue )
    {
        m_Value &= BLUE_MASK;
        m_Value |= ( blue << BLUE_SHIFT );
    }

    // Get individual channel values
    __forceinline UINT GetRed() const
    {
        return ( m_Value & ~RED_MASK ) >> RED_SHIFT;
    }

    __forceinline UINT GetGreen() const
    {
        return ( m_Value & ~GREEN_MASK ) >> GREEN_SHIFT;
    }

    __forceinline UINT GetBlue() const
    {
        return ( m_Value & ~BLUE_MASK ) >> BLUE_SHIFT;
    }

    // Get the intensity
    __forceinline UINT GetIntensity() const
    {
        // Cache channel values

        const UINT red = GetRed();
        const UINT green = GetGreen();
        const UINT blue = GetBlue();

        // Avoid integer mul as it is slow and unpipelined; do 8 bit fixed multiply.
        // Weights for rgb -> intensity transform are:
        // 76/255 for red, 149/255 for green, 30/255 for blue (blue channel rounding total to 255)

        // 76 = 64 + 8 + 4, so shift red 6, 3, 2 and add to mul by 76.
        UINT redMul = ( red << 6 ) + ( red << 3 ) + ( red << 2 );

        // 149 = 128 + 16 + 4 + 1, so shift green 7,4,2,1 to mul by 149
        UINT greenMul = ( green << 7 ) + ( green << 4 ) + ( green << 2 ) + green;

        // 29 = 32 - 4 + 1, so shift by 5, 2, 1 and add/subtract to mul by 29
        UINT blueMul = ( blue << 5 ) - ( blue << 2 );

        // Shift back down by 8 and return sum
        return ( redMul >> 8 ) + ( greenMul >> 8 ) + ( blueMul >> 8 );
    }

    __forceinline UINT SquareDistance(const RGBAValue color) const
    {
        const INT rdiff = GetRed() - color.GetRed();
        const INT gdiff = GetGreen() - color.GetGreen();
        const INT bdiff = GetBlue() - color.GetBlue();
        return ( rdiff * rdiff ) + ( gdiff * gdiff ) + ( bdiff * bdiff );
    }

private:
    enum RGBAValueConstants
    {
        ALPHA_MASK  = 0x00ffffff,
        ALPHA_SHIFT = 24,

        RED_MASK    = 0xff00ffff,
        RED_SHIFT   = 16,

        GREEN_MASK  = 0xffff00ff,
        GREEN_SHIFT = 8,

        BLUE_MASK   = 0xffffff00,
        BLUE_SHIFT  = 0,

        FIXED_ALPHA = 0xff000000
    };

    DWORD m_Value;
};

#endif
