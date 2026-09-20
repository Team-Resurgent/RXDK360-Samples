//--------------------------------------------------------------------------------------
// Binary.cpp
//
// This file contains straight C implementations. 
// Intended to be an optimized path; currently only partially optimized.
//
// Advanced Technology Group
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include <xtl.h>

#include "AtgUtil.h"

#include "FastBinaryMap.h"
#include "FastDepthMap.h"
#include "RGBAValue.h"

//--------------------------------------------------------------------------------------
// Name: CopyToTexture()
// Copy map contents to a D3D texture by expanding bit values to greyscale.
// Be careful of write combine rules here.
//--------------------------------------------------------------------------------------
VOID FastBinaryMap::CopyToTexture( IDirect3DTexture9* pTexture ) const
{
    assert( pTexture != NULL );
    assert( pTexture->GetLevelCount() == 1 );		// Function does not fill mipmaps

    D3DSURFACE_DESC surfaceDesc;
    pTexture->GetLevelDesc( 0, &surfaceDesc );			// Check texture size matches
    assert( surfaceDesc.Width == BINARY_MAP_WIDTH );
    assert( surfaceDesc.Height == BINARY_MAP_HEIGHT );

    D3DLOCKED_RECT lockRect;
    pTexture->LockRect( 0, &lockRect, NULL, 0 );
    BYTE* pPosition = ( BYTE* )lockRect.pBits;

    RGBAValue clearColor( 0,0,0 );         // Black
    RGBAValue setColor( 128,128,128 );     // Grey

    switch( surfaceDesc.Format )
    {
        case D3DFMT_LIN_A8R8G8B8:
        case D3DFMT_LIN_X8R8G8B8:
            // Run through the binary map and expand each bit to a grey color

            for( UINT y = 0; y < BINARY_MAP_HEIGHT; y++ )
            {
                DWORD* pRow = ( DWORD* )pPosition;
                for( UINT x = 0; x < BINARY_MAP_WIDTH; x++ )
                {
                    if( Value( x, y ) )
                        pRow[x] = setColor;
                    else
                        pRow[x] = clearColor;
                }

                pPosition += lockRect.Pitch;            // To next row
            }
            break;

        default:
            assert( false );                            // Format not implemented
            break;
    }

    pTexture->UnlockRect( 0 );
}

//--------------------------------------------------------------------------------------
// Name: RotateLeft()
// Rotates two input vectors left, shifting bits in from the second vector to the first
// The rotate count is the difference between shiftLeft and shiftRight, in bits
// This achieves a bit shift not possible with vsldoi/vsl/etc.
//--------------------------------------------------------------------------------------
static __forceinline __vector4 RotateLeft( const __vector4 a, const __vector4 b, const __vector4 shiftLeft,
                                           const __vector4 shiftRight, const __vector4 zero )
{
    __vector4 bit;
    __vector4 rest;
    bit = __vsldoi( zero, b, 1 );
    bit = __vsr( bit, shiftRight );
    rest = __vsl( a, shiftLeft );
    return __vor( bit, rest );
}

//--------------------------------------------------------------------------------------
// Name: RotateRight()
// Rotates two input vectors right, shifting bits in from the second vector to the first
// The rotate count is the difference between shiftLeft and shiftRight, in bits
// This achieves a bit shift not possible with vsldoi/vsr/etc.
//--------------------------------------------------------------------------------------
static __forceinline __vector4 RotateRight( const __vector4 a, const __vector4 b, const __vector4 shiftLeft,
                                            const __vector4 shiftRight, const __vector4 zero )
{
    __vector4 bit;
    __vector4 rest;
    bit = __vsldoi( a, zero, 15 );
    bit = __vsl( bit, shiftLeft );
    rest = __vsr( b, shiftRight );
    return __vor( bit, rest );
}

//--------------------------------------------------------------------------------------
// Name: Dilate3x3()
// Applies a 3x3 morphological dilate across the entire map, using the helpers above.
//--------------------------------------------------------------------------------------
VOID FastBinaryMap::Dilate3x3( const FastBinaryMap& inputMap )
{
	const __vector4* __restrict readPosition = ( const __vector4* __restrict )inputMap.m_Contents;
	__vector4* __restrict writePosition = ( __vector4* __restrict )m_Contents;

    const __vector4 zeroes = __vzero();
    const __vector4 shiftOne = __vspltisb( 1 );
    const __vector4 shiftSeven = __vspltisb( 7 );

    __vector4 row0_0, row0_1, row0_2;
    __vector4 row1_0, row1_1, row1_2;
    __vector4 row2_0, row2_1, row2_2;
    __vector4 out0, out1, out2;

    // First row - we only work with the first two rows
    row0_0 = zeroes;
    row0_1 = zeroes;
    row0_2 = zeroes;
    row1_0 = readPosition[0];
    row1_1 = readPosition[1];
    row1_2 = readPosition[2];
    readPosition += 3;

    // All the rest of the rows
    for( int y = 0; y < BINARY_MAP_HEIGHT-1; y++ )
    {
        row2_0 = readPosition[0];
        row2_1 = readPosition[1];
        row2_2 = readPosition[2];
        readPosition += 3;

        // Or with row above
        out0 = __vor( row1_0, row0_0 );
        out1 = __vor( row1_1, row0_1 );
        out2 = __vor( row1_2, row0_2 );
        // Or with row below
        out0 = __vor( out0, row2_0 );
        out1 = __vor( out1, row2_1 );
        out2 = __vor( out2, row2_2 );

        // Or with left neighbour - for the first two words, we need to shift in the neighbour's value, not zeros
        // so for example word 1, shift in bit 31 for word 0 of neighbouring vector4
        out0 = __vor( out0, RotateLeft( row1_0, row1_1, shiftOne, shiftSeven, zeroes ) );
        out1 = __vor( out1, RotateLeft( row1_1, row1_2, shiftOne, shiftSeven, zeroes ) );
        out2 = __vor( out2, __vsl( row1_2, shiftOne ) );

        // Or with right neighbour - for the last two words, we need to shift in neighbourd value, not zeros
        // We need to bring in bit 0 of left neighbour to bit31 of word 0
	
        writePosition[0] = __vor( out0, __vsr( row1_0, shiftOne ) );
        writePosition[1] = __vor( out1, RotateRight( row1_0, row1_1, shiftSeven, shiftOne, zeroes ) );
        writePosition[2] = __vor( out2, RotateRight( row1_1, row1_2, shiftSeven, shiftOne, zeroes ) );
        writePosition += 3;

        // shift rows up
        row0_0 = row1_0;
        row0_1 = row1_1;
        row0_2 = row1_2;
		
        row1_0 = row2_0;
        row1_1 = row2_1;
        row1_2 = row2_2;
    }

    // Last row, we don't need to do the bottom row since nominally it will be all zeros
    // Or with row above
    out0 = __vor( row1_0, row0_0 );
    out1 = __vor( row1_1, row0_1 );
    out2 = __vor( row1_2, row0_2 );

    // Or with left neighbour - for the first two words, we need to shift in the neighbour's value, not zeros
    // so for example word 1, shift in bit 31 for word 0 of neighbouring vector4
    out0 = __vor( out0, RotateLeft( row1_0, row1_1, shiftOne, shiftSeven, zeroes ) );
    out1 = __vor( out1, RotateLeft( row1_1, row1_2, shiftOne, shiftSeven, zeroes ) );
    out2 = __vor( out2, __vsl( row1_2, shiftOne ) );

    // Or with right neighbour - for the last two words, we need to shift in neighbourd value, not zeros
    // We need to bring in bit 0 of left neighbour to bit31 of word 0

    writePosition[0] = __vor( out0, __vsr( row1_0, shiftOne ) );
    writePosition[1] = __vor( out1, RotateRight( row1_0, row1_1, shiftSeven, shiftOne, zeroes ) );
    writePosition[2] = __vor( out2, RotateRight( row1_1, row1_2, shiftSeven, shiftOne, zeroes ) );
}

//--------------------------------------------------------------------------------------
// Name: Erode3x3()
// Applies a 3x3 morphological erode across the entire map, using the helpers above.
//--------------------------------------------------------------------------------------
VOID FastBinaryMap::Erode3x3( const FastBinaryMap& inputMap )
{
	const __vector4* __restrict readPosition = ( const __vector4* __restrict )inputMap.m_Contents;
	__vector4* __restrict writePosition = ( __vector4* __restrict )m_Contents;

    const __vector4 zeroes = __vzero();
    const __vector4 ones = __vnor( zeroes, zeroes );
    const __vector4 shiftOne = __vspltisb( 1 );
    const __vector4 shiftSeven = __vspltisb( 7 );

    __vector4 row0_0, row0_1, row0_2;
    __vector4 row1_0, row1_1, row1_2;
    __vector4 row2_0, row2_1, row2_2;
    __vector4 out0, out1, out2;

    // Set up first 2 rows
    row0_0 = zeroes;
    row0_1 = zeroes;
    row0_2 = zeroes;
    row1_0 = __vandc( ones, readPosition[0] );
    row1_1 = __vandc( ones, readPosition[1] );
    row1_2 = __vandc( ones, readPosition[2] );
    readPosition += 3;

    // All the rest of the rows
    for( int y = 0; y < BINARY_MAP_HEIGHT-1; y++ )
    {
        row2_0 = __vandc( ones, readPosition[0] );
        row2_1 = __vandc( ones, readPosition[1] );
        row2_2 = __vandc( ones, readPosition[2] );
        readPosition += 3;

        // Or with row above
        out0 = __vandc( __vandc( ones, row0_0 ), row1_0 );
        out1 = __vandc( __vandc( ones, row0_1 ), row1_1 );
        out2 = __vandc( __vandc( ones, row0_2 ), row1_2 );

        // Or with row below
        out0 = __vandc( out0, row2_0 );
        out1 = __vandc( out1, row2_1 );
        out2 = __vandc( out2, row2_2 );

        // Or with left neighbour - for the first two words, we need to shift in the neighbour's value, not zeros
        // so for example word 1, shift in bit 31 for word 0 of neighbouring vector4
        out0 = __vandc( out0, RotateLeft( row1_0, row1_1, shiftOne, shiftSeven, zeroes ) );
        out1 = __vandc( out1, RotateLeft( row1_1, row1_2, shiftOne, shiftSeven, zeroes ) );
        out2 = __vandc( out2, __vsl( row1_2, shiftOne ) );

        // Or with right neighbour - for the last two words, we need to shift in neighbourd value, not zeros
        // We need to bring in bit 0 of left neighbour to bit31 of word 0
        writePosition[0] = __vandc( out0, __vsr( row1_0, shiftOne ) );
        writePosition[1] = __vandc( out1, RotateRight( row1_0, row1_1, shiftSeven, shiftOne, zeroes ) );
        writePosition[2] = __vandc( out2, RotateRight( row1_1, row1_2, shiftSeven, shiftOne, zeroes ) );
        writePosition += 3;

        // shift rows up
        row0_0 = row1_0;
        row0_1 = row1_1;
        row0_2 = row1_2;
		
        row1_0 = row2_0;
        row1_1 = row2_1;
        row1_2 = row2_2;
    }

    // Or with row above
    out0 = __vandc( __vandc( ones, row0_0 ), row1_0 );
    out1 = __vandc( __vandc( ones, row0_1 ), row1_1 );
    out2 = __vandc( __vandc( ones, row0_2 ), row1_2 );

    // Or with left neighbour - for the first two words, we need to shift in the neighbour's value, not zeros
    // so for example word 1, shift in bit 31 for word 0 of neighbouring vector4
    out0 = __vandc( out0, RotateLeft( row1_0, row1_1, shiftOne, shiftSeven, zeroes ) );
    out1 = __vandc( out1, RotateLeft( row1_1, row1_2, shiftOne, shiftSeven, zeroes ) );
    out2 = __vandc( out2, __vsl( row1_2, shiftOne ) );

    // Or with right neighbour - for the last two words, we need to shift in neighbourd value, not zeros
    // We need to bring in bit 0 of left neighbour to bit31 of word 0
    writePosition[0] = __vandc( out0, __vsr( row1_0, shiftOne ) );
    writePosition[1] = __vandc( out1, RotateRight( row1_0, row1_1, shiftSeven, shiftOne, zeroes ) );
    writePosition[2] = __vandc( out2, RotateRight( row1_1, row1_2, shiftSeven, shiftOne, zeroes ) );
}


//--------------------------------------------------------------------------------------
// Name: Complement()
// Inverts the bits in this BinaryMap
//--------------------------------------------------------------------------------------
VOID FastBinaryMap::Complement()
{
    __vector4* position = ( __vector4* )m_Contents;
    const UINT vectorCount = BINARY_MAP_HEIGHT * BINARY_MAP_WIDTH_UNITS;
    const __vector4 ones = __vnor( __vzero(), __vzero() );

    for( int vector = 0; vector < vectorCount; vector += 8 )
    {
        position[vector + 0] = __vandc( ones, position[vector + 0] );
        position[vector + 1] = __vandc( ones, position[vector + 1] );
        position[vector + 2] = __vandc( ones, position[vector + 2] );
        position[vector + 3] = __vandc( ones, position[vector + 3] );
        position[vector + 4] = __vandc( ones, position[vector + 4] );		
        position[vector + 5] = __vandc( ones, position[vector + 5] );
        position[vector + 6] = __vandc( ones, position[vector + 6] );
        position[vector + 7] = __vandc( ones, position[vector + 7] );
    }
}

//--------------------------------------------------------------------------------------
// Name: Complement()
// Inverts the bits in the input map into this map
//--------------------------------------------------------------------------------------
VOID FastBinaryMap::Complement( const FastBinaryMap& inputMap )
{
	__vector4* __restrict writePosition = ( __vector4* __restrict )m_Contents;
	const __vector4* __restrict readPosition = ( const __vector4* __restrict )inputMap.m_Contents;
    const UINT vectorCount = BINARY_MAP_HEIGHT * BINARY_MAP_WIDTH_UNITS;
    const __vector4 ones = __vnor( __vzero(), __vzero() );

    for( int vector = 0; vector < vectorCount; vector += 8 )
    {
        writePosition[vector + 0] = __vandc( ones, readPosition[vector + 0] );
        writePosition[vector + 1] = __vandc( ones, readPosition[vector + 1] );
        writePosition[vector + 2] = __vandc( ones, readPosition[vector + 2] );
        writePosition[vector + 3] = __vandc( ones, readPosition[vector + 3] );
        writePosition[vector + 4] = __vandc( ones, readPosition[vector + 4] );		
        writePosition[vector + 5] = __vandc( ones, readPosition[vector + 5] );
        writePosition[vector + 6] = __vandc( ones, readPosition[vector + 6] );
        writePosition[vector + 7] = __vandc( ones, readPosition[vector + 7] );
    }
}

//--------------------------------------------------------------------------------------
// Name: Union()
// Sets this map to the union (or) of the two input maps
//--------------------------------------------------------------------------------------
VOID FastBinaryMap::Union( const FastBinaryMap& inputMap0,  const FastBinaryMap& inputMap1 )
{
	__vector4* __restrict writePosition = ( __vector4* __restrict )m_Contents;
	const __vector4* __restrict readPosition0 = ( const __vector4* __restrict )inputMap0.m_Contents;
	const __vector4* __restrict readPosition1 = ( const __vector4* __restrict )inputMap1.m_Contents;
    const UINT vectorCount = BINARY_MAP_HEIGHT * BINARY_MAP_WIDTH_UNITS;

    for( int vector = 0; vector < vectorCount; vector += 8 )
    {
        writePosition[vector + 0] = __vor( readPosition0[vector + 0], readPosition1[vector + 0] );
        writePosition[vector + 1] = __vor( readPosition0[vector + 1], readPosition1[vector + 1] );
        writePosition[vector + 2] = __vor( readPosition0[vector + 2], readPosition1[vector + 2] );
        writePosition[vector + 3] = __vor( readPosition0[vector + 3], readPosition1[vector + 3] );
        writePosition[vector + 4] = __vor( readPosition0[vector + 4], readPosition1[vector + 4] );		
        writePosition[vector + 5] = __vor( readPosition0[vector + 5], readPosition1[vector + 5] );
        writePosition[vector + 6] = __vor( readPosition0[vector + 6], readPosition1[vector + 6] );
        writePosition[vector + 7] = __vor( readPosition0[vector + 7], readPosition1[vector + 7] );
    }
}

//--------------------------------------------------------------------------------------
// Name: Intersection()
// Sets this map to the intersection (and) of the two input maps
//--------------------------------------------------------------------------------------
VOID FastBinaryMap::Intersection( const FastBinaryMap& inputMap0,  const FastBinaryMap& inputMap1 )
{
	__vector4* __restrict writePosition = ( __vector4* __restrict )m_Contents;
	const __vector4* __restrict readPosition0 = ( const __vector4* __restrict )inputMap0.m_Contents;
	const __vector4* __restrict readPosition1 = ( const __vector4* __restrict )inputMap1.m_Contents;
    const UINT vectorCount = BINARY_MAP_HEIGHT * BINARY_MAP_WIDTH_UNITS;

    for( int vector = 0; vector < vectorCount; vector += 8 )
    {
        writePosition[vector + 0] = __vand( readPosition0[vector + 0], readPosition1[vector + 0] );
        writePosition[vector + 1] = __vand( readPosition0[vector + 1], readPosition1[vector + 1] );
        writePosition[vector + 2] = __vand( readPosition0[vector + 2], readPosition1[vector + 2] );
        writePosition[vector + 3] = __vand( readPosition0[vector + 3], readPosition1[vector + 3] );
        writePosition[vector + 4] = __vand( readPosition0[vector + 4], readPosition1[vector + 4] );		
        writePosition[vector + 5] = __vand( readPosition0[vector + 5], readPosition1[vector + 5] );
        writePosition[vector + 6] = __vand( readPosition0[vector + 6], readPosition1[vector + 6] );
        writePosition[vector + 7] = __vand( readPosition0[vector + 7], readPosition1[vector + 7] );
    }
}


//--------------------------------------------------------------------------------------
// Name: Xor()
// Sets this map to the exclusive or of the two input maps
//--------------------------------------------------------------------------------------
VOID FastBinaryMap::Xor( const FastBinaryMap& inputMap0, const FastBinaryMap& inputMap1 )
{
	__vector4* __restrict writePosition = ( __vector4* __restrict )m_Contents;
	const __vector4* __restrict readPosition0 = ( const __vector4* __restrict )inputMap0.m_Contents;
	const __vector4* __restrict readPosition1 = ( const __vector4* __restrict )inputMap1.m_Contents;
    const UINT vectorCount = BINARY_MAP_HEIGHT * BINARY_MAP_WIDTH_UNITS;

    for( int vector = 0; vector < vectorCount; vector += 8 )
    {
        writePosition[vector + 0] = __vxor( readPosition0[vector + 0], readPosition1[vector + 0] );
        writePosition[vector + 1] = __vxor( readPosition0[vector + 1], readPosition1[vector + 1] );
        writePosition[vector + 2] = __vxor( readPosition0[vector + 2], readPosition1[vector + 2] );
        writePosition[vector + 3] = __vxor( readPosition0[vector + 3], readPosition1[vector + 3] );
        writePosition[vector + 4] = __vxor( readPosition0[vector + 4], readPosition1[vector + 4] );		
        writePosition[vector + 5] = __vxor( readPosition0[vector + 5], readPosition1[vector + 5] );
        writePosition[vector + 6] = __vxor( readPosition0[vector + 6], readPosition1[vector + 6] );
        writePosition[vector + 7] = __vxor( readPosition0[vector + 7], readPosition1[vector + 7] );
    }
}

//--------------------------------------------------------------------------------------
// Name: CompressBits()
// Takes an input vector containing 16 bytes with 00 or 01 in them, and crushes this down
// to 16 single bits in halfword 0.
// The long parameter list of pre-set up shift masks should be entirely eliminated 
// by inlining.
//--------------------------------------------------------------------------------------
__forceinline __vector4 CompressBits( const __vector4 bits, const __vector4 zeroes, const __vector4 shiftFour,  const __vector4 shiftSeven, 
									  const __vector4 shiftFourteen, const __vector4 shiftTwentyOne, const __vector4 byteMask)
{
    __vector4 temp;
    __vector4 shift;
    shift = __vsraw( bits, shiftSeven );	// rotate in each word by seven right, ie compress first two bits onto each other
    temp = __vor( bits, shift );

    shift = __vsraw( bits, shiftFourteen );
    temp = __vor( temp, shift );

    shift = __vsraw( bits, shiftTwentyOne );
    temp = __vor( temp, shift );
    temp = __vand( temp, byteMask );

    // Now each word contains 4 bits set at the bottom

    __vector4 quad;
    __vector4 result = __vsl( temp, shiftFour );
    quad = __vsldoi( temp, zeroes, 4 );				// shift by 4 bytes left
    result = __vor( result, quad );					// or first word in
    result = __vsl( result, shiftFour );
    quad = __vsldoi( temp, zeroes, 8 );				// shift by 4 bytes left
    result = __vor( result, quad );					// or first word in
    result = __vsl( result, shiftFour );
    quad = __vsldoi( temp, zeroes, 12 );			// shift by 4 bytes left
    result = __vor( result, quad );					// or first word in

    return result;
}

//--------------------------------------------------------------------------------------
// Name: CompressShorts128()
// Reads 16 __vector4 worth of USHORT/DepthValue, compares then with the requested
// segmentation mask, and compresses the values down to a single __vector4 with bits
// indicating the presence or absence of the mask bit being sent.
//--------------------------------------------------------------------------------------
__forceinline __vector4 CompressShorts128( const __vector4* readPosition, const __vector4 unitMask,
                                           const __vector4 onesMask, const __vector4 sixteenBitMask,
                                           const __vector4 zeroes )
{
    const __vector4 shiftFour = __vspltisb( 4 );
    const __vector4 shiftSeven = __vspltisw(7);
    const __vector4 shiftFourteen = __vslw(shiftSeven,__vspltisw(1));
    const __vector4 shiftTwentyOne = __vadduws(shiftSeven,shiftFourteen);
    const __vector4 byteMask = __vspltisw(15);

    // Read 16 __vector4, with 8 shorts per vector which is 128 bit results
    __vector4 value0 = __vand( readPosition[0], unitMask );
    __vector4 value1 = __vand( readPosition[1], unitMask );
    __vector4 value2 = __vand( readPosition[2], unitMask );
    __vector4 value3 = __vand( readPosition[3], unitMask );
    __vector4 value4 = __vand( readPosition[4], unitMask );
    __vector4 value5 = __vand( readPosition[5], unitMask );
    __vector4 value6 = __vand( readPosition[6], unitMask );
    __vector4 value7 = __vand( readPosition[7], unitMask );
    __vector4 value8 = __vand( readPosition[8], unitMask );
    __vector4 value9 = __vand( readPosition[9], unitMask );
    __vector4 value10 = __vand( readPosition[10], unitMask );
    __vector4 value11 = __vand( readPosition[11], unitMask );
    __vector4 value12 = __vand( readPosition[12], unitMask );
    __vector4 value13 = __vand( readPosition[13], unitMask );
    __vector4 value14 = __vand( readPosition[14], unitMask );
    __vector4 value15 = __vand( readPosition[15], unitMask );

    value0 = __vand( onesMask, __vcmpgtuh(  value0, zeroes ) );
    value1 = __vand( onesMask, __vcmpgtuh(  value1, zeroes ) );
    value2 = __vand( onesMask, __vcmpgtuh(  value2, zeroes ) );
    value3 = __vand( onesMask, __vcmpgtuh(  value3, zeroes ) );
    value4 = __vand( onesMask, __vcmpgtuh(  value4, zeroes ) );
    value5 = __vand( onesMask, __vcmpgtuh(  value5, zeroes ) );
    value6 = __vand( onesMask, __vcmpgtuh(  value6, zeroes ) );
    value7 = __vand( onesMask, __vcmpgtuh(  value7, zeroes ) );
    value8 = __vand( onesMask, __vcmpgtuh(  value8, zeroes ) );
    value9 = __vand( onesMask, __vcmpgtuh(  value9, zeroes ) );
    value10 = __vand( onesMask, __vcmpgtuh( value10, zeroes ) );
    value11 = __vand( onesMask, __vcmpgtuh( value11, zeroes ) );
    value12 = __vand( onesMask, __vcmpgtuh( value12, zeroes ) );
    value13 = __vand( onesMask, __vcmpgtuh( value13, zeroes ) );
    value14 = __vand( onesMask, __vcmpgtuh( value14, zeroes ) );
    value15 = __vand( onesMask, __vcmpgtuh( value15, zeroes ) );


    value0 = CompressBits( __vpkuhus( value0, value1 ), zeroes, shiftFour, shiftSeven, shiftFourteen, shiftTwentyOne, byteMask );
    value1 = CompressBits( __vpkuhus( value2, value3 ), zeroes, shiftFour, shiftSeven, shiftFourteen, shiftTwentyOne, byteMask );
    value2 = CompressBits( __vpkuhus( value4, value5 ), zeroes, shiftFour, shiftSeven, shiftFourteen, shiftTwentyOne, byteMask );
    value3 = CompressBits( __vpkuhus( value6, value7 ), zeroes, shiftFour, shiftSeven, shiftFourteen, shiftTwentyOne, byteMask );
    value4 = CompressBits( __vpkuhus( value8, value9 ), zeroes, shiftFour, shiftSeven, shiftFourteen, shiftTwentyOne, byteMask );
    value5 = CompressBits( __vpkuhus( value10, value11 ), zeroes, shiftFour, shiftSeven, shiftFourteen, shiftTwentyOne, byteMask );
    value6 = CompressBits( __vpkuhus( value12, value13 ), zeroes, shiftFour, shiftSeven, shiftFourteen, shiftTwentyOne, byteMask );
    value7 = CompressBits( __vpkuhus( value14, value15 ), zeroes, shiftFour, shiftSeven, shiftFourteen, shiftTwentyOne, byteMask );

    // Each value holds 16 bits worth of data in the bottom 16 bits of word 0
    // Munge together

    value0 = __vand( value0, sixteenBitMask );
    value4 = __vpermwi( __vand( value4, sixteenBitMask ), VPERMWI_CONST( 3, 0, 3, 3 ) );
    value0 = __vor( value0, value4 );

    value1 = __vand( value1, sixteenBitMask );
    value5 = __vpermwi( __vand( value5, sixteenBitMask ), VPERMWI_CONST( 3, 0, 3, 3 ) );
    value1 = __vor( value1, value5 );

    value0 = __vmrghh( value0, value1 );
    value0 = __vsldoi( value0, zeroes, 4 );

    value2 = __vand( value2, sixteenBitMask );
    value6 = __vpermwi( __vand( value6, sixteenBitMask ), VPERMWI_CONST( 3, 0, 3, 3 ) );
    value2 = __vor( value2, value6 );

    value3 = __vand( value3, sixteenBitMask );
    value7 = __vpermwi( __vand( value7, sixteenBitMask ), VPERMWI_CONST( 3, 0, 3, 3 ) );
    value3 = __vor( value3, value7 );

    value2 = __vmrghh( value2, value3 );
    return __vor( value0, value2 );
}

//--------------------------------------------------------------------------------------
// Name: CompressShorts64()
// Reads 8 __vector4 worth of USHORT/DepthValue, compares then with the requested
// segmentation mask, and compresses the values down to a single __vector4 with bits
// indicating the presence or absence of the mask bit being sent.
// A subset of the above designed for a 384 bit wide binary mask.
//--------------------------------------------------------------------------------------
__forceinline __vector4 CompressShorts64( const __vector4* readPosition, const __vector4 unitMask,
                                          const __vector4 onesMask, const __vector4 sixteenBitMask,
                                          const __vector4 zeroes )
{
    const __vector4 shiftFour = __vspltisb( 4 );
    const __vector4 shiftSeven = __vspltisw( 7 );
    const __vector4 shiftFourteen = __vslw(shiftSeven,__vspltisw( 1 ));
    const __vector4 shiftTwentyOne = __vadduws(shiftSeven,shiftFourteen);
    const __vector4 byteMask = __vspltisw( 15 );

    // Read 16 __vector4, with 8 shorts per vector which is 128 bit results
    __vector4 value0 = __vand( readPosition[0], unitMask );
    __vector4 value1 = __vand( readPosition[1], unitMask );
    __vector4 value2 = __vand( readPosition[2], unitMask );
    __vector4 value3 = __vand( readPosition[3], unitMask );
    __vector4 value4 = __vand( readPosition[4], unitMask );
    __vector4 value5 = __vand( readPosition[5], unitMask );
    __vector4 value6 = __vand( readPosition[6], unitMask );
    __vector4 value7 = __vand( readPosition[7], unitMask );

    value0 = __vand( onesMask, __vcmpgtuh( value0, zeroes ) );
    value1 = __vand( onesMask, __vcmpgtuh( value1, zeroes ) );
    value2 = __vand( onesMask, __vcmpgtuh( value2, zeroes ) );
    value3 = __vand( onesMask, __vcmpgtuh( value3, zeroes ) );
    value4 = __vand( onesMask, __vcmpgtuh( value4, zeroes ) );
    value5 = __vand( onesMask, __vcmpgtuh( value5, zeroes ) );
    value6 = __vand( onesMask, __vcmpgtuh( value6, zeroes ) );
    value7 = __vand( onesMask, __vcmpgtuh( value7, zeroes ) );

    value0 = CompressBits( __vpkuhus( value0, value1 ), zeroes, shiftFour, shiftSeven, shiftFourteen, shiftTwentyOne, byteMask );
    value1 = CompressBits( __vpkuhus( value2, value3 ), zeroes, shiftFour, shiftSeven, shiftFourteen, shiftTwentyOne, byteMask );
    value2 = CompressBits( __vpkuhus( value4, value5 ), zeroes, shiftFour, shiftSeven, shiftFourteen, shiftTwentyOne, byteMask );
    value3 = CompressBits( __vpkuhus( value6, value7 ), zeroes, shiftFour, shiftSeven, shiftFourteen, shiftTwentyOne, byteMask );

    // Each value holds 16 bits worth of data in the bottom 16 bits of word 0
    // Munge together

    value0 = __vand( value0, sixteenBitMask );
    value1 = __vand( value1, sixteenBitMask );
    value0 = __vmrghh( value0, value1 );
    value0 = __vsldoi( value0, zeroes, 4 );

    value2 = __vand( value2, sixteenBitMask );
    value3 = __vand( value3, sixteenBitMask );
    value2 = __vmrghh( value2, value3 );

    return __vor( value0, value2 );
}

//--------------------------------------------------------------------------------------
// Name: SegmentationMaskSelect()
// Builds a binary mask from the segmentation mask bits of a DepthMap.
//--------------------------------------------------------------------------------------

BOOL FastBinaryMap::SegmentationMaskSelect( UINT mask, const FastDepthMap& depthMap )
{
	const __vector4* __restrict readPosition = ( const __vector4* __restrict )depthMap.m_Values;
	__vector4* __restrict writePosition = ( __vector4* __restrict )m_Contents;
	
    __vector4 shortMask;
    shortMask.u[0] = shortMask.u[1] = shortMask.u[2] = shortMask.u[3] = mask | ( mask << 16 );
    const __vector4 zeroes = __vzero();

    __vector4 onesMask = __vspltish(1);
    onesMask.u[0] = onesMask.u[1] = onesMask.u[2] = onesMask.u[3] = 0x00010001;

    __vector4 sixteenBitMask;
    sixteenBitMask.u[0] = 0x0000ffff;
    sixteenBitMask.u[1] = 0x00000000;
    sixteenBitMask.u[2] = 0x00000000;
    sixteenBitMask.u[3] = 0x00000000;

    __vector4 concat = zeroes;

    for( int y = 0; y < BINARY_MAP_HEIGHT; y++ )
    {
        const __vector4 part0 = CompressShorts128( readPosition, shortMask, onesMask, sixteenBitMask, zeroes );
        readPosition += 16;
        const __vector4 part1 = CompressShorts128( readPosition, shortMask, onesMask, sixteenBitMask, zeroes );
        readPosition += 16;
        const __vector4 part2 = CompressShorts64( readPosition, shortMask, onesMask, sixteenBitMask, zeroes );
        readPosition += 8;

        concat = __vor( concat, __vor( __vor( part0, part1 ), part2 ) );
        writePosition[0] = part0;
        writePosition[1] = part1;
        writePosition[2] = part2;
        writePosition += 3;
    }
    return ( concat.u[0] != 0 || concat.u[1] != 0 || concat.u[2] != 0 || concat.u[3] != 0 );
}
