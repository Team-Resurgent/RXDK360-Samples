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

#ifndef FAST_BINARY_MAP_H
#define FAST_BINARY_MAP_H

#include <assert.h>

//--------------------------------------------------------------------------------------
// Forward declarations
//--------------------------------------------------------------------------------------

class FastDepthMap;
class DepthValue;
struct IDirect3DTexture9;


//--------------------------------------------------------------------------------------
// Bitmap class
//--------------------------------------------------------------------------------------
class FastBinaryMap
{
	public:
		enum BinaryMapDimensions
		{
			BINARY_MAP_WIDTH    = 320,
			BINARY_MAP_HEIGHT   = 240
		};

		// Default constructor
		FastBinaryMap()
		{
			XMemSet(this,0,sizeof(*this));
		}

		// Copy constructor
		FastBinaryMap( const FastBinaryMap& target )
		{
			XMemCpy(this,&target,sizeof(*this));
		}
	    
		// Clear to all zero
		VOID Clear()
		{
			XMemSet(this,0,sizeof(*this));
		}

		// Set all bits
		VOID Set()
		{
			XMemSet(this,0xff,sizeof(*this));
		}

		// Copy input map to this map
		VOID Copy( const FastBinaryMap& map )	
		{
			XMemCpy(this,&map,sizeof(*this));
		}


		// Get the set or clear value at x,y in the bitmap
		__forceinline UINT Value( const UINT inX, const UINT inY ) const
		{
			assert( inX < BINARY_MAP_WIDTH );
			assert( inY < BINARY_MAP_HEIGHT );

			const UINT vector = (inX & 0x180)>>7;
			const UINT word = (inX & 0x60)>>5;
			const UINT bit = 31-(inX & 0x1f);

			const UINT maskResult = m_Contents[inY][vector].u[word] & (1 << bit);
			return maskResult > 0;
		}


		// Set the bit at x,y in the bitmap
		__forceinline VOID SetValue( const UINT inX, const UINT inY )
		{
			assert( inX < BINARY_MAP_WIDTH );
			assert( inY < BINARY_MAP_HEIGHT );
			
			const UINT vector = (inX & 0x180)>>7;
			const UINT word = (inX & 0x60)>>5;
			const UINT bit = 31-(inX & 0x1f);

			m_Contents[inY][vector].u[word] |= (1 << bit);
		}

		// Clear the bit at x,y in the bitmap
		__forceinline VOID  ClearValue( const UINT inX, const UINT inY )
		{
			assert( inX < BINARY_MAP_WIDTH );
			assert( inY < BINARY_MAP_HEIGHT );

			const UINT vector = (inX & 0x180)>>7;
			const UINT word = (inX & 0x60)>>5;
			const UINT bit = 31-(inX & 0x1f);

			m_Contents[inY][vector].u[word] &= ~(1 << bit);
		}

		// Copy this bitmap to the D3D texture
		VOID CopyToTexture( IDirect3DTexture9* texture ) const;

		// Morphological operators - erode and dilate
		VOID Dilate3x3( const FastBinaryMap& inputMap );
		VOID Erode3x3( const FastBinaryMap& inputMap );

		// Logical/set operations on the binary map
		VOID Complement();
		VOID Complement( const FastBinaryMap& map );
		VOID Union( const FastBinaryMap& map1, const FastBinaryMap& map2 );
		VOID Intersection( const FastBinaryMap& map1, const FastBinaryMap& map2 );
		VOID Xor( const FastBinaryMap& map1, const FastBinaryMap& map2 );

		// Fill bitmap by selecting from the segmentation mask in the depth map
		BOOL SegmentationMaskSelect( UINT mask, const FastDepthMap& depthMap );

	private:
		enum
		{
			BINARY_MAP_WIDTH_UNITS = 3			// 384 bits worth, 64 bits per row wasted
		};

		__vector4 m_Contents[BINARY_MAP_HEIGHT][BINARY_MAP_WIDTH_UNITS];
};


#endif
