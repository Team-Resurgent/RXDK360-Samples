//--------------------------------------------------------------------------------------
// FastDepthMap.h
//
// Class representing the depth map returned by NUI - ie 320x240 16 bit with 3 bit
// segmentation mask in the bottom 3 places.
// The class is an array of DepthValue, which deals with common operations on
// individual depth values. It is fixed in size at 320x240.
//
// Advanced Technology Group
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#ifndef FAST_DEPTH_MAP_H
#define FAST_DEPTH_MAP_H


#include "DepthValue.h"

//--------------------------------------------------------------------------------------
// Forward declarations
//--------------------------------------------------------------------------------------

struct IDirect3DTexture9;

class CatmullRomColorSpline;
class FastBinaryMap;

//--------------------------------------------------------------------------------------
// Depth map class
//--------------------------------------------------------------------------------------

class FastDepthMap
{
	friend class FastBinaryMap;
public:

    enum DepthMapDimensions
    {
        DEPTH_MAP_WIDTH     = 320,
        DEPTH_MAP_HEIGHT    = 240

    };

    // Default constructor
    FastDepthMap()
    {
    }

    // Copy constructor
    FastDepthMap( const FastDepthMap& other )
    {
        XMemCpy( m_Values, other.m_Values, DEPTH_MAP_SIZE );
    }

    // Construct from array of short values with an optional stride
    FastDepthMap( const USHORT* pValues, const UINT stride = DEPTH_MAP_STRIDE )
    {
        Fill( pValues, stride );
    }

    // Set depth value at x,y in the map
    __forceinline VOID SetValue( const UINT x, const UINT y, const DepthValue& value )
    {
        assert( x < DEPTH_MAP_WIDTH );
        assert( y < DEPTH_MAP_HEIGHT );
		DepthValue* pRow = (DepthValue*)m_Values[y];
		pRow[x] = value;
    }

    // Get depth value at x,y in the map
    __forceinline const DepthValue Value( const UINT x, const UINT y ) const
    {
        assert( x < DEPTH_MAP_WIDTH );
        assert( y < DEPTH_MAP_HEIGHT );
		DepthValue* pRow = (DepthValue*)m_Values[y];
		return pRow[x];
    }

    // Fill methods
    VOID Fill( const USHORT* pValues, const UINT iStride = DEPTH_MAP_STRIDE );
    VOID Fill( const DepthValue value );

    // Copy depth map to texture, either as greyscale 8 bit, or using color spline
    VOID CopyToTexture( IDirect3DTexture9* texture ) const;
    VOID CopyToTexture( IDirect3DTexture9* texture, const CatmullRomColorSpline& spline ) const;

    // Set this map to the Sobel edge detection result from input map
    VOID Sobel( const FastDepthMap& inputMap );

private:
    enum DepthMapConstants
    {
        DEPTH_MAP_STRIDE		= ( DEPTH_MAP_WIDTH * sizeof( DepthValue ) ),
        DEPTH_MAP_SIZE			= ( DEPTH_MAP_WIDTH * DEPTH_MAP_HEIGHT * sizeof( DepthValue ) ),
		DEPTH_MAP_WIDTH_UNITS	= ( DEPTH_MAP_STRIDE / sizeof(__vector4))
	};

	__vector4 m_Values[DEPTH_MAP_HEIGHT][DEPTH_MAP_WIDTH_UNITS];
};


#endif
