//--------------------------------------------------------------------------------------
// FastIntensityMap.h
//
// A fixed size intensity map - in this case 640x480 to match the color resolution
// the sensor returns. Many operations will be much quicker using greyscale as the data
// size is 1/3rd to 1/4 a full color map. The individual elements are BYTEs.
//
// Advanced Technology Group
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma once

#ifndef FAST_INTENSITY_MAP_H
#define FAST_INTENSITY_MAP_H

//--------------------------------------------------------------------------------------
// Forward declarations
//--------------------------------------------------------------------------------------
struct IDirect3DTexture9;
class FastColorMap;
class FastBinaryMap;

//--------------------------------------------------------------------------------------
// Fast intensity map
//--------------------------------------------------------------------------------------

class FastIntensityMap
{
	friend FastColorMap;
	friend FastBinaryMap;

public:

    enum FastIntensityMapDimensions
    {
        INTENSITY_MAP_WIDTH     = 640,
        INTENSITY_MAP_HEIGHT    = 480
    };

    // Default constructor
    FastIntensityMap()
    {
    }

    // Copy constructor
    FastIntensityMap( const FastIntensityMap& map )
    {
        XMemCpy( m_Intensities, map.m_Intensities, INTENSITY_MAP_SIZE );
    }

    // Copy from another intensity map
    VOID Copy( const FastIntensityMap& map )
    {
        XMemCpy( m_Intensities, map.m_Intensities, INTENSITY_MAP_SIZE );
    }

    // Set a value at x,y in the map.
    __forceinline VOID SetValue( const UINT x, const UINT y, const UINT value )
    {
        assert( x < INTENSITY_MAP_WIDTH );
        assert( y < INTENSITY_MAP_HEIGHT );
		BYTE* pRow = (BYTE *)m_Intensities[y];
		pRow[x] = (BYTE)value;
    }

    // Get the value at x,y in the map
    __forceinline const UINT Value( const UINT x, const UINT y ) const
    {
        assert( x < INTENSITY_MAP_WIDTH );
        assert( y < INTENSITY_MAP_HEIGHT );

		BYTE* pRow = (BYTE *)m_Intensities[y];
		return pRow[x];
    }

    // Copy the intensities out to a texture
    VOID CopyToTexture( IDirect3DTexture9* texture ) const;

    // Fill from various sources
    VOID Fill( const BYTE* pValues, const UINT iStride = INTENSITY_MAP_STRIDE );
    VOID Fill( const FastColorMap& other );
    VOID Fill( const FastColorMap& other, const FastBinaryMap& selectMap );

    // Sobel edge detection operator
    VOID SobelEdgeDetect( const FastIntensityMap& inputMap );
    VOID SobelEdgeDetect( const FastIntensityMap& inputMap, const FastBinaryMap& selectMap );

    // Set this map to the abs difference between this and the input map
    VOID IntensityDifference( const FastIntensityMap& inputMap );
    VOID IntensityDifference( const FastIntensityMap& inputMap, const FastIntensityMap& map2 );

private:
    enum IntensityMapConstants
    {
        INTENSITY_MAP_STRIDE        = INTENSITY_MAP_WIDTH,
        INTENSITY_MAP_SIZE          = INTENSITY_MAP_WIDTH * INTENSITY_MAP_HEIGHT,
		INTENSITY_MAP_WIDTH_UNITS   = (INTENSITY_MAP_STRIDE / sizeof( __vector4 )),
        EDGE_SIGNIFICANT_THRESHOLD  = 60
    };

	__vector4 m_Intensities[INTENSITY_MAP_HEIGHT][INTENSITY_MAP_WIDTH_UNITS];		// 40 * 16 = 640
};



#endif
