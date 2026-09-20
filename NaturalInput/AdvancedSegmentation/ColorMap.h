//--------------------------------------------------------------------------------------
// ColorMap.h
//
// ColorMap is a class for handling an 8 bit r,g,b map. It has an associated class, 
// RGBValue. The map is fixed size 640x480 with the intention of handling color images
// from the NUI camera. The map has various classification and segmentation methods
// as part of its role in segmenting player foreground from background.
// The map is 640x480 x 4 bytes. It's strongly recommended any major processing is done
// with derivatives of this map - IntensityMap, BinaryMap - because they are so much 
// smaller in memory. RGBValue is actually a DWORD.
//
// Advanced Technology Group
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#ifndef COLOR_MAP_H
#define COLOR_MAP_H

#include "RGBAValue.h"                // Needed for RGBA class internals

//--------------------------------------------------------------------------------------
// Forward declarations to reduce translation unit size
//--------------------------------------------------------------------------------------

struct IDirect3DTexture9;
class DepthMap;
class BinaryMap;
class IntensityMap;
class CumulativeMovingAverageBuffer;

//--------------------------------------------------------------------------------------
// ColorMap class for handing color image operations
//--------------------------------------------------------------------------------------
class ColorMap
{
public:

    enum ColorMapDimensions
    {
        COLOR_MAP_WIDTH     = 640,
        COLOR_MAP_HEIGHT    = 480
    };

    // Default constructor
    ColorMap()
    {
    }

    // Copy constructor
    ColorMap( const ColorMap& other )
    {
        XMemCpy( m_Colors, other.m_Colors, COLOR_MAP_SIZE );
    }

    // Construct from array of DWORD containing colors
    ColorMap( DWORD* pWords, const UINT stride = COLOR_MAP_STRIDE )
    {
        Fill( pWords, stride );
    }

    // Clear the color map to zero (black)
    VOID Clear();
    // Fill with a color
    VOID Fill( const RGBAValue value );
    // Fill from an array of DWORD with an optional stride
    VOID Fill( const DWORD* pWords, const UINT iStride = COLOR_MAP_STRIDE );
    // Copy from another ColorMap
    VOID Copy( const ColorMap& map );

    // Set a color value in the map. RGBAValue is small enough to be passed by value
    __forceinline VOID SetValue( const UINT x, const UINT y, const RGBAValue value )
    {
        assert( x < COLOR_MAP_WIDTH );
        assert( y < COLOR_MAP_HEIGHT );
        m_Colors[y][x] = value;
    }

    // Get the color at an x,y position
    __forceinline RGBAValue Value( const UINT x, const UINT y ) const
    {
        assert( x < COLOR_MAP_WIDTH );
        assert( y < COLOR_MAP_HEIGHT );
        return m_Colors[y][x];
    }

    // Copy this color map out to the supplied D3D texture
    VOID CopyToTexture( IDirect3DTexture9* texture ) const;

    // Zero out pixels not set in the binary map
    VOID SelectPixels( const BinaryMap& selectMap );
    // Zero out pixels in the input map not set in the binary map
    VOID SelectPixels( const ColorMap& colorMap, const BinaryMap& selectMap );

    // Select the foreground pixels from the background using the input map
    // See function header for better explanation.
    VOID SelectForeground( const ColorMap& original, const IntensityMap& map,
                           const BinaryMap& fgPixels, const BinaryMap& edgePixels,
                           const CumulativeMovingAverageBuffer& cma,
                           BinaryMap& segmentationMap );

    VOID SelectForegroundClassify( const ColorMap& original, const IntensityMap& map,
									const BinaryMap& fgPixels, const BinaryMap& edgePixels,
									const CumulativeMovingAverageBuffer& cma,
									BinaryMap& segmentationMap );

    // Sobel edge detection in rgb space
    VOID SobelEdgeDetect( const ColorMap& inputMap );


private:
    enum ColorMapConstants
    {
        COLOR_MAP_STRIDE    = ( COLOR_MAP_WIDTH * sizeof( RGBAValue ) ),
        COLOR_MAP_SIZE      = ( COLOR_MAP_WIDTH * COLOR_MAP_HEIGHT * sizeof( RGBAValue ) ),
        FOREGROUND_DIFFERENCE_THRESHOLD_SQ = 600,
        BACKGROUND_DIFFERENCE_THRESHOLD_SQ = 100
    };

    RGBAValue m_Colors[COLOR_MAP_HEIGHT][COLOR_MAP_WIDTH];
};

#endif
