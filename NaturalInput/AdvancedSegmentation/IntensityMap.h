//--------------------------------------------------------------------------------------
// IntensityMap.h
//
// A fixed size intensity map - in this case 640x480 to match the color resolution
// the sensor returns. Many operations will be much quicker using greyscale as the data
// size is 1/3rd to 1/4 a full color map. The individual elements are BYTEs.
//
// Advanced Technology Group
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma once

#ifndef INTENSITY_MAP_H
#define INTENSITY_MAP_H

//--------------------------------------------------------------------------------------
// Forward declarations
//--------------------------------------------------------------------------------------
struct IDirect3DTexture9;
class ColorMap;
class BinaryMap;

//--------------------------------------------------------------------------------------
// Intensity map
//--------------------------------------------------------------------------------------
class IntensityMap
{
public:

    enum IntensityMapDimensions
    {
        INTENSITY_MAP_WIDTH     = 640,
        INTENSITY_MAP_HEIGHT    = 480
    };

    // Default constructor
    IntensityMap()
    {
    }

    // Copy constructor
    IntensityMap( const IntensityMap& map )
    {
        XMemCpy( m_Intensities, map.m_Intensities, INTENSITY_MAP_SIZE );
    }

    // Copy from another intensity map
    VOID Copy( const IntensityMap& map )
    {
        XMemCpy( m_Intensities, map.m_Intensities, INTENSITY_MAP_SIZE );
    }

    // Set a value at x,y in the map.
    __forceinline VOID SetValue( const UINT x, const UINT y, const UINT value )
    {
        assert( x < INTENSITY_MAP_WIDTH );
        assert( y < INTENSITY_MAP_HEIGHT );

        m_Intensities[y][x] = ( BYTE )value;
    }

    // Get the value at x,y in the map
    __forceinline const UINT Value( const UINT x, const UINT y ) const
    {
        assert( x < INTENSITY_MAP_WIDTH );
        assert( y < INTENSITY_MAP_HEIGHT );

        return m_Intensities[y][x];
    }

    // Copy the intensities out to a texture
    VOID CopyToTexture( IDirect3DTexture9* texture ) const;

    // Fill from various sources
    VOID Fill( const BYTE* pValues, const UINT iStride = INTENSITY_MAP_STRIDE );
    VOID Fill( const ColorMap& other );

    // Sobel edge detection operator
    VOID SobelEdgeDetect( const IntensityMap& inputMap );
    VOID SobelEdgeDetect( const IntensityMap& inputMap, const BinaryMap& selectMap );

    // Set this map to the abs difference between this and the input map
    VOID IntensityDifference( const IntensityMap& inputMap );
    VOID IntensityDifference( const IntensityMap& inputMap0, const IntensityMap& inputMap1 );

private:
    enum IntensityMapConstants
    {
        INTENSITY_MAP_STRIDE        =  INTENSITY_MAP_WIDTH,
        INTENSITY_MAP_SIZE          = INTENSITY_MAP_WIDTH * INTENSITY_MAP_HEIGHT,
        EDGE_SIGNIFICANT_THRESHOLD  = 60
    };

    BYTE m_Intensities[INTENSITY_MAP_HEIGHT][INTENSITY_MAP_WIDTH];
};

#endif
