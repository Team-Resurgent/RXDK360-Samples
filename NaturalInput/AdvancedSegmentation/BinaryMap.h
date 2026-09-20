//--------------------------------------------------------------------------------------
// BinaryMap.h
//
// A 2D bitmap with morphological operators and various construction mechanisms from
// other kinds of bitmaps - depth map, color map, etc. The map is fixed size for best
// performance, and fixed at 320x240 to match NUI depth map size.
// Morphological operators are useful for classifying pixels, and at 9k, working with
// with this map is much cheaper than any other map type.
//
// Advanced Technology Group
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#ifndef BINARY_MAP_H
#define BINARY_MAP_H

#include <assert.h>

//--------------------------------------------------------------------------------------
// Forward declarations
//--------------------------------------------------------------------------------------

class DepthMap;
class DepthValue;
struct IDirect3DTexture9;

class BinaryMapIterator;

//--------------------------------------------------------------------------------------
// Bitmap class
//--------------------------------------------------------------------------------------
class BinaryMap
{
public:
	friend class BinaryMapIterator;
    enum BinaryMapDimensions
    {
        BINARY_MAP_WIDTH    = 320,
        BINARY_MAP_HEIGHT   = 240
    };

    // Default constructor
    BinaryMap()
    {
    }

    // Copy constructor
    BinaryMap( const BinaryMap& target )
    {
        XMemCpy( m_Bits, target.m_Bits, BINARY_MAP_SIZE );
    }
    
    // Clear to all zero
    VOID Clear()
    {
        XMemSet( m_Bits, 0x00, BINARY_MAP_SIZE );
    }

    // Set all bits
    VOID Set()
    {
        XMemSet( m_Bits, 0xff, BINARY_MAP_SIZE );
    }

    // Copy input map to this map
    VOID Copy( const BinaryMap& map )	
    {
        XMemCpy( m_Bits, map.m_Bits, BINARY_MAP_SIZE );
    }


    // Get the set or clear value at x,y in the bitmap
    __forceinline UINT Value( const UINT x, const UINT y ) const
    {
        assert( x < BINARY_MAP_WIDTH );
        assert( y < BINARY_MAP_HEIGHT );
            
        // Find the unit containing the bit, and the bit index
        const UINT unitIndex = BinaryMapGetWordIndex( x );
        const UINT unit = m_Bits[y][unitIndex];
            
        // Mask the bit value out
        const UINT bitMask = 1 << BinaryMapGetBitIndex( x );
        const INT maskResult = unit & bitMask;

        // Avoid a branch - if uint & bitMask > 0, return 1, else 0
        return ( ( -maskResult ) >> 31 ) & 0x1;
    }


    // Set the bit at x,y in the bitmap
    __forceinline VOID  SetValue( const UINT x, const UINT y )
    {
        assert( x < BINARY_MAP_WIDTH );
        assert( y < BINARY_MAP_HEIGHT );

        // Find the unit containing the bit, and the bit index
        const unsigned int unitIndex = BinaryMapGetWordIndex( x );
        const UINT bitMask = 1 << BinaryMapGetBitIndex( x );
            
        // Set the bit
        m_Bits[y][unitIndex] |= bitMask;
    }

    // Clear the bit at x,y in the bitmap
    __forceinline VOID  ClearValue( const UINT x, const UINT y )
    {
        assert( x < BINARY_MAP_WIDTH );
        assert( y < BINARY_MAP_HEIGHT );

        // Find the unit containing the bit, and the bit index
        const unsigned int unitIndex = BinaryMapGetWordIndex( x );
        const UINT bitMask = 1 << BinaryMapGetBitIndex( x );

        // Clear the bit
        m_Bits[y][unitIndex] &= ~bitMask;
    }

    // Copy this bitmap to the D3D texture
    VOID CopyToTexture( IDirect3DTexture9* texture ) const;

    // Morphological operators - erode and dilate
    VOID Dilate3x3( const BinaryMap& inputMap );
    VOID Dilate5x5( const BinaryMap& inputMap );
    VOID Erode3x3( const BinaryMap& inputMap );
    VOID Erode5x5( const BinaryMap& inputMap );

    // Logical/set operations on the binary map
    VOID Complement();
    VOID Union( const BinaryMap& map1, const BinaryMap& map2 );
    VOID Intersection( const BinaryMap& map1, const BinaryMap& map2 );
    VOID Xor( const BinaryMap& map1, const BinaryMap& map2 );
    VOID Complement( const BinaryMap& map );

    // Fill bitmap by thresholding the input depth map at the given depth
    DWORD Threshold( const UINT depth, const DepthMap& inputMap );

    // Fill bitmap by selecting from the segmentation mask in the depth map
    BOOL SegmentationMaskSelect( UINT mask, const DepthMap& depthMap );

private:

    // Helper for selecting which word the bit for an x coordinate is in
    __forceinline UINT  BinaryMapGetWordIndex( const UINT x ) const
    {
        return ( x >> BINARY_MAP_UNIT_INDEX_SHIFT );
    }

    // Helper for selecting which bit in a word is for an x coordinate
    __forceinline UINT  BinaryMapGetBitIndex( const UINT x ) const
    {
        return ( x & BINARY_MAP_UNIT_BIT_MASK );
    }

    enum BinaryMapConstants
    {
        BINARY_MAP_UNIT_BITS        = 32,
        BINARY_MAP_UNIT_INDEX_SHIFT = 5,
        BINARY_MAP_UNIT_BIT_MASK    = 0x1f,
        BINARY_MAP_WIDTH_UNITS      = BINARY_MAP_WIDTH/BINARY_MAP_UNIT_BITS,
        BINARY_MAP_SIZE             = ( BINARY_MAP_HEIGHT * ( BINARY_MAP_WIDTH / 8 ) )
    };

    // Bit array is 'height' rows of 'width units'
    UINT m_Bits[BINARY_MAP_HEIGHT][BINARY_MAP_WIDTH_UNITS];
};


#endif
