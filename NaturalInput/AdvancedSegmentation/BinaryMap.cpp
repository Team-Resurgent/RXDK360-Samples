//--------------------------------------------------------------------------------------
// Binary.cpp
//
// This file contains straight C implementations. These are not generally optimized for 
// Xbox360 but intended to show the algorithms in a plain and understandable way.
//
// Advanced Technology Group
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include <xtl.h>

#include "AtgUtil.h"

#include "BinaryMap.h"
#include "DepthMap.h"
#include "RGBAValue.h"

//--------------------------------------------------------------------------------------
// Name: CopyToTexture()
// Copy map contents to a D3D texture by expanding bit values to greyscale.
// Be careful of write combine rules here.
//--------------------------------------------------------------------------------------
VOID BinaryMap::CopyToTexture( IDirect3DTexture9* pTexture ) const
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

    RGBAValue clearColor(0,0,0);         // Black
    RGBAValue setColor(128,128,128);     // Grey

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
                    if ( Value( x, y ) )
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
// Name: Union()
// Morphological union of map1 and map2.
// This map is set to the union of the input maps. This is the same as a logical or.
//--------------------------------------------------------------------------------------
VOID BinaryMap::Union( const BinaryMap& map1, const BinaryMap& map2 )
{
    for( UINT y = 0; y < BINARY_MAP_HEIGHT; y++ )
    {
        for( UINT x = 0; x < BINARY_MAP_WIDTH; x++ )
        {
            if( map1.Value( x, y ) || map2.Value( x, y ) )
                SetValue( x, y );
            else
                ClearValue( x, y );
        }
    }
}

//--------------------------------------------------------------------------------------
// Name: Intersection()
// Morphological intersection of map1 and map2.
// This map is set to the intersection of the input maps, ie logical and.
//--------------------------------------------------------------------------------------
VOID BinaryMap::Intersection( const BinaryMap& map1, const BinaryMap& map2 )
{
    for( UINT y = 0; y < BINARY_MAP_HEIGHT; y++ )
    {
        for( UINT x = 0; x < BINARY_MAP_WIDTH; x++ )
        {
            if( map1.Value( x, y ) && map2.Value( x, y ) )
                SetValue( x, y );
            else
                ClearValue( x, y );
        }
    }
}

//--------------------------------------------------------------------------------------
// Name: Xor()
// Exclusive or of the two input maps.
// This map is set where a pixel in exactly one input map is set and clear otherwise.
//--------------------------------------------------------------------------------------
VOID BinaryMap::Xor( const BinaryMap& map1, const BinaryMap& map2 )
{
    for( UINT y = 0; y < BINARY_MAP_HEIGHT; y++ )
    {
        for( UINT x = 0; x < BINARY_MAP_WIDTH; x++ )
        {
            if( map1.Value( x, y ) ^ map2.Value( x, y ) )
                SetValue( x, y );
            else
                ClearValue( x, y );
        }
    }
}

//--------------------------------------------------------------------------------------
// Name: Complement()
// Logical complement, aka 'not' of the input map.
// This map is set to the complement of itself.
// Could be implemented by calling Complement(*this) except that breaks const
// correctness for the function below.
//--------------------------------------------------------------------------------------
VOID BinaryMap::Complement()
{
    for( UINT y = 0; y < BINARY_MAP_HEIGHT; y++ )
    {
        for( UINT x = 0; x < BINARY_MAP_WIDTH_UNITS; x++ )
        {
            m_Bits[y][x] = ~m_Bits[y][x];
        }
    }
}

//--------------------------------------------------------------------------------------
// Name: Complement()
// Logical complement aka 'not' of the input map.
// This map is set to the complement of the input map.
//--------------------------------------------------------------------------------------
VOID BinaryMap::Complement( const BinaryMap& map )
{
    for( UINT y = 0; y < BINARY_MAP_HEIGHT; y++ )
    {
        for( UINT x = 0; x < BINARY_MAP_WIDTH_UNITS; x++ )
        {
            m_Bits[y][x] = ~map.m_Bits[y][x];
        }
    }
}

//--------------------------------------------------------------------------------------
// Name: Threshold()
// Build a binary map by thresholding a depth map at a particular depth.
// Returns the count of set pixels in the resultant binary map
//--------------------------------------------------------------------------------------
DWORD BinaryMap::Threshold( const UINT threshold, const DepthMap& inputMap )
{
    UINT setCount = 0;

    for( UINT y = 0; y < BINARY_MAP_HEIGHT; y++ )
    {
        for( UINT x = 0; x < BINARY_MAP_WIDTH; x++ )
        {
            const UINT depth = inputMap.Value( x, y ).IntDepth();
            if( depth > threshold )
            {
                ClearValue( x, y );
            }
            else
            {
                setCount++;
                SetValue( x, y );
            }
        }
    }

    return setCount;
}

//--------------------------------------------------------------------------------------
// Name: SegmentationMaskSelect()
// Builds a binary map by selecting parts of the segmentation mask in a depth map
// Returns the count of set pixels in the resultant binary map
//--------------------------------------------------------------------------------------
BOOL BinaryMap::SegmentationMaskSelect( UINT mask, const DepthMap& inputMap )
{
    UINT setCount = 0;
    for( UINT y = 0; y < BINARY_MAP_HEIGHT; y++ )
    {
        for( UINT x = 0; x < BINARY_MAP_WIDTH; x++ )
        {
            const UINT maskValue = inputMap.Value( x, y ).SegmentationValue();
            if( ( maskValue & mask ) != 0 )
            {
                setCount++;
                SetValue( x, y );
            }
            else
            {
                ClearValue( x, y );
            }
        }
    }
    return setCount > 0;
}

//--------------------------------------------------------------------------------------
// Name: InsideMap()
// Inline helper function for edge cases in morphological operators.
// Returns true if the x,y coordinate is inside the map area. This is used only when
// operating on edges where the kernel sampling may fall outside the map.
//--------------------------------------------------------------------------------------
static __forceinline INT InsideMap( const INT x, const INT y )
{
    return ( x >= 0 && x < BinaryMap::BINARY_MAP_WIDTH && y >= 0 && y < BinaryMap::BINARY_MAP_HEIGHT );
}

//--------------------------------------------------------------------------------------
// Name: Kernel3x3ErodeEvaluate()
// Evaluates a 3x3 erode kernel around the position x,y. It does not check for outside
// map conditions. It would be possible to early out on a set pixel but this would cause
// a very large amount of branching.
//--------------------------------------------------------------------------------------
static __forceinline UINT Kernel3x3ErodeEvaluate( const UINT x, const UINT y, const BinaryMap& inputMap )
{
    UINT setCount = 0;
    setCount += inputMap.Value( x + 0, y - 1 );
    setCount += inputMap.Value( x - 1, y + 0 );
    setCount += inputMap.Value( x + 0, y + 0 );
    setCount += inputMap.Value( x + 1, y + 0 );
    setCount += inputMap.Value( x + 0, y + 1 );
    return setCount == 5;
}

//--------------------------------------------------------------------------------------
// Name: Kernel5x5Erode()
// As per Kernel3x3Erode(), but with a 5x5 circular mask.
//--------------------------------------------------------------------------------------
static __forceinline UINT Kernel5x5ErodeEvaluate( const UINT x, const UINT y, const BinaryMap& inputMap )
{
    UINT setCount = 0;

    // Count all of input values
    setCount += inputMap.Value( x + 0, y - 2 );

    setCount += inputMap.Value( x - 1, y - 1 );
    setCount += inputMap.Value( x + 0, y - 1 );
    setCount += inputMap.Value( x + 1, y - 1 );

    setCount += inputMap.Value( x - 2, y + 0 );
    setCount += inputMap.Value( x - 1, y + 0 );
    setCount += inputMap.Value( x + 0, y + 0 );
    setCount += inputMap.Value( x + 1, y + 0 );
    setCount += inputMap.Value( x + 2, y + 0 );

    setCount += inputMap.Value( x - 1, y + 1 );
    setCount += inputMap.Value( x + 0, y + 1 );
    setCount += inputMap.Value( x + 1, y + 1 );

    setCount += inputMap.Value( x + 0, y + 2 );

    return setCount == 13;		// if all set we can erode this pixel
}

//--------------------------------------------------------------------------------------
// Name: Kernel3x3ErodeEvaluateConditional()
// Evaluates a 3x3 erode kernel (circular) around the position x,y but checks the 
// coordinates are inside the map as it goes. This is a specialised version for use along 
// edges of maps and in corners. In the main part of the map where we are sure the kernel 
// can't go outside the map we use a non-conditional version.
// It is possible to write the exact kernel for each edge and corner, but the code
// becomes incredibly unwieldy.
//--------------------------------------------------------------------------------------
static __forceinline UINT Kernel3x3ErodeEvaluateConditional( const UINT x, const UINT y, const BinaryMap& inputMap )
{
    if( InsideMap( x + 0, y - 1 ) && !inputMap.Value( x + 0, y - 1 ) )
        return 0;
    if( InsideMap( x - 1, y + 0 ) && !inputMap.Value( x - 1, y + 0 ) )
        return 0;
    if( InsideMap( x + 0, y + 0 ) && !inputMap.Value( x + 0, y + 0 ) )
        return 0;
    if( InsideMap( x + 1, y + 0 ) && !inputMap.Value( x + 1, y + 0 ) )
        return 0;
    if( InsideMap( x + 0, y + 1 ) && !inputMap.Value( x + 0, y + 1 ) )
        return 0;
    return 1;
}

//--------------------------------------------------------------------------------------
// Name: Kernel5x5ErodeEvaluateConditional()
// As per Kernel3x3ErodeEvaluateConditional(), but with a 5x5 circular mask.
//--------------------------------------------------------------------------------------
static __forceinline UINT Kernel5x5ErodeEvaluateConditional( const UINT x, const UINT y, const BinaryMap& inputMap )
{
    // All of the pixels in the mask must be set to erode the pixel at x,y
    // If any pixel inside the map and in the kernel are not set, we can return 0
    
    if( InsideMap( x + 0, y - 2 ) && !inputMap.Value( x + 0, y - 2 ) )
        return 0;
    if( InsideMap( x - 1, y - 1 ) && !inputMap.Value( x - 1, y - 1 ) )
        return 0;
    if( InsideMap( x + 0, y - 1 ) && !inputMap.Value( x + 0, y - 1 ) )
        return 0;
    if( InsideMap( x + 1, y - 1 ) && !inputMap.Value( x + 1, y - 1 ) )
        return 0;
    if( InsideMap( x - 2, y + 0 ) && !inputMap.Value( x - 2, y + 0 ) )
        return 0;
    if( InsideMap( x - 1, y + 0 ) && !inputMap.Value( x - 1, y + 0 ) )
        return 0;
    if( InsideMap( x + 0, y + 0 ) && !inputMap.Value( x + 0, y + 0 ) )
        return 0;
    if( InsideMap( x + 1, y + 0 ) && !inputMap.Value( x + 1, y + 0 ) )
        return 0;
    if( InsideMap( x + 2, y + 0 ) && !inputMap.Value( x + 2, y + 0 ) )
        return 0;
    if( InsideMap( x - 1, y + 1 ) && !inputMap.Value( x - 1, y + 1 ) )
        return 0;
    if( InsideMap( x + 0, y + 1 ) && !inputMap.Value( x + 0, y + 1 ) )
        return 0;
    if( InsideMap( x + 1, y + 1 ) && !inputMap.Value( x + 1, y + 1 ) )
        return 0;
    if( InsideMap( x + 0, y + 2 ) && !inputMap.Value( x + 0, y + 2 ) )
        return 0;

    return 1;		// All pixels are set, we can erode this pixel
}

//--------------------------------------------------------------------------------------
// Name: Erode3x3()
// Applies a 3x3 morphological erode across the entire map, using the helpers above.
//--------------------------------------------------------------------------------------
VOID BinaryMap::Erode3x3( const BinaryMap& inputMap )
{
    assert( &inputMap != this );	// Dilate does not work with source and dest the same

    // general case - 1 pixel in from the sides, we know the kernel cannot go outside
    // the map.
    for( UINT y = 1; y < BINARY_MAP_HEIGHT - 1; y++ )
    {
        for( UINT x = 1; x < BINARY_MAP_WIDTH - 1; x++ )
        {
            if( Kernel3x3ErodeEvaluate( x, y, inputMap ) )
                SetValue( x, y );
            else
                ClearValue( x, y );
        }
    }

    // Run along the top and bottom edge, using the conditional kernel applies.
    // This includes the corners.
    for( UINT x = 0; x < BINARY_MAP_WIDTH; x++ )
    {
        if( Kernel3x3ErodeEvaluateConditional( x, 0, inputMap ) )
            SetValue( x, 0 );
        else
            ClearValue( x, 0 );

        if( Kernel3x3ErodeEvaluateConditional( x, BINARY_MAP_HEIGHT - 1, inputMap ) )
            SetValue( x, BINARY_MAP_HEIGHT - 1 );
        else
            ClearValue( x, BINARY_MAP_HEIGHT - 1 );
    }

    // Run along the left and right edge, using the conditional kernel applies.
    // Don't include the corners, we did them above.
    for( UINT y = 1; y < BINARY_MAP_HEIGHT - 1; y++ )
    {
        if( Kernel3x3ErodeEvaluateConditional( 0, y, inputMap ) )
            SetValue( 0, y );
        else
            ClearValue( 0, y );

        if( Kernel3x3ErodeEvaluateConditional( BINARY_MAP_WIDTH - 1, y, inputMap ) )
            SetValue( BINARY_MAP_WIDTH - 1, y );
        else
            ClearValue( BINARY_MAP_WIDTH - 1, y );
    }
}

//--------------------------------------------------------------------------------------
// Name: Erode5x5()
// Applies a 5x5 morphological erode across the entire map, using the helpers above.
//--------------------------------------------------------------------------------------
VOID BinaryMap::Erode5x5( const BinaryMap& inputMap )
{
    assert( &inputMap != this );	// Dilate does not work with source and dest the same

    // General case - we know that 2 pixels in from the edges, out 5x5 kernel won't go
    // outside the map so we don't need a conditional check on the x,y positions.
    for( UINT y = 2; y < BINARY_MAP_HEIGHT - 2; y++ )
    {
        for( UINT x = 2; x < BINARY_MAP_WIDTH - 2; x++ )
        {
            if( Kernel5x5ErodeEvaluate( x, y, inputMap ) )
                SetValue( x, y );
            else
                ClearValue( x, y );
        }
    }

    // Run along top two rows of pixels and apply conditional kernel, including corners.
    for( UINT y = 0; y < 2; y++ )
    {
        for( UINT x = 0; x < BINARY_MAP_WIDTH; x++ )
        {
            if( Kernel5x5ErodeEvaluateConditional( x, y, inputMap ) )
                SetValue( x, y );
            else
                ClearValue( x, y );
        }
    }

    // Run along bottom two rows of pixels and apply conditional kernel, including corners.
    for( UINT y = BINARY_MAP_HEIGHT - 2; y < BINARY_MAP_HEIGHT; y++ )
    {
        for( UINT x = 0; x < BINARY_MAP_WIDTH; x++ )
        {
            if( Kernel5x5ErodeEvaluateConditional( x, y, inputMap ) )
                SetValue( x, y );
            else
                ClearValue( x, y );
        }
    }

    // Run down left 2 columns of pixels (don't include corners)
    for( UINT y = 2; y < BINARY_MAP_HEIGHT - 2; y++ )
    {
        for( UINT x = 0; x < 2; x++ )
        {
            if( Kernel5x5ErodeEvaluateConditional( x, y, inputMap ) )
                SetValue( x, y );
            else
                ClearValue( x, y );
        }
    }

    // Run down right 2 columns of pixels (don't include corners)
    for( UINT y = 2; y < BINARY_MAP_HEIGHT - 2; y++ )
    {
        for( UINT x = BINARY_MAP_WIDTH - 2; x < BINARY_MAP_WIDTH; x++ )
        {
            if( Kernel5x5ErodeEvaluateConditional( x, y, inputMap ) )
                SetValue( x, y );
            else
                ClearValue( x, y );
        }
    }
}


//--------------------------------------------------------------------------------------
// Name: Kernel3x3DilateEvaluate()
// Evaluates a 3x3 dilate kernel around the position x,y. It does not check for outside
// map conditions. It would be possible to early out on a set pixel but this would cause
// a very large amount of branching.
//--------------------------------------------------------------------------------------
static __forceinline UINT Kernel3x3DilateEvaluate( const UINT x, const UINT y, const BinaryMap& inputMap )
{
    UINT setCount = 0;
    setCount += inputMap.Value( x + 0, y - 1 );
    setCount += inputMap.Value( x - 1, y + 0 );
    setCount += inputMap.Value( x + 0, y + 0 );
    setCount += inputMap.Value( x + 1, y + 0 );
    setCount += inputMap.Value( x + 0, y + 1 );
    return setCount > 0;
}

//--------------------------------------------------------------------------------------
// Name: Kernel3x3DilateEvaluateConditional()
// Evaluates a 3x3 dilate kernel (circular) around the position x,y but checks the 
// coordinates are inside the map as it goes. This is a specialised version for use along 
// edges of maps and in corners. In the main part of the map where we are sure the kernel 
// can't go outside the map we use a non-conditional version.
// It is possible to write the exact kernel for each edge and corner, but the code
// becomes incredibly unwieldy.
//--------------------------------------------------------------------------------------

static __forceinline UINT Kernel3x3DilateEvaluateConditional( const UINT x, const UINT y, const BinaryMap& inputMap )
{
    if( InsideMap( x + 0, y - 1 ) && inputMap.Value( x + 0, y - 1 ) )
        return 1;
    if( InsideMap( x - 1, y + 0 ) && inputMap.Value( x - 1, y + 0 ) )
        return 1;
    if( InsideMap( x + 0, y + 0 ) && inputMap.Value( x + 0, y + 0 ) )
        return 1;
    if( InsideMap( x + 1, y + 0 ) && inputMap.Value( x + 1, y + 0 ) )
        return 1;
    if( InsideMap( x + 0, y + 1 ) && inputMap.Value( x + 0, y + 1 ) )
        return 1;
    return 0;
}

//--------------------------------------------------------------------------------------
// Name: Kernel5x5DilateEvaluateConditional()
// As per Kernel3x3DilateEvaluateConditional(), but with a 5x5 circular mask.
//--------------------------------------------------------------------------------------

static __forceinline UINT Kernel5x5DilateEvaluateConditional( const UINT x, const UINT y, const BinaryMap& inputMap )
{
    if( InsideMap( x + 0, y - 2 ) && inputMap.Value( x + 0, y - 2 ) )
        return 1;
    if( InsideMap( x - 1, y - 1 ) && inputMap.Value( x - 1, y - 1 ) )
        return 1;
    if( InsideMap( x + 0, y - 1 ) && inputMap.Value( x + 0, y - 1 ) )
        return 1;
    if( InsideMap( x + 1, y - 1 ) && inputMap.Value( x + 1, y - 1 ) )
        return 1;
    if( InsideMap( x - 2, y + 0 ) && inputMap.Value( x - 2, y + 0 ) )
        return 1;
    if( InsideMap( x - 1, y + 0 ) && inputMap.Value( x - 1, y + 0 ) )
        return 1;
    if( InsideMap( x + 0, y + 0 ) && inputMap.Value( x + 0, y + 0 ) )
        return 1;
    if( InsideMap( x + 1, y + 0 ) && inputMap.Value( x + 1, y + 0 ) )
        return 1;
    if( InsideMap( x + 2, y + 0 ) && inputMap.Value( x + 2, y + 0 ) )
        return 1;
    if( InsideMap( x - 1, y + 1 ) && inputMap.Value( x - 1, y + 1 ) )
        return 1;
    if( InsideMap( x + 0, y + 1 ) && inputMap.Value( x + 0, y + 1 ) )
        return 1;
    if( InsideMap( x + 1, y + 1 ) && inputMap.Value( x + 1, y + 1 ) )
        return 1;
    if( InsideMap( x + 0, y + 2 ) && inputMap.Value( x + 0, y + 2 ) )
        return 1;

    return 0;
}

//--------------------------------------------------------------------------------------
// Name: Kernel5x5DilateEvaluate()
// Evaluates a 3x3 erode kernel around the position x,y. It does not check for outside
// map conditions. It would be possible to early out on a set pixel but this would cause
// a very large amount of branching.
//--------------------------------------------------------------------------------------
static __forceinline UINT Kernel5x5DilateEvaluate( const UINT x, const UINT y, const BinaryMap& inputMap )
{
    UINT setCount = 0;

    setCount += inputMap.Value( x + 0, y - 2 );

    setCount += inputMap.Value( x - 1, y - 1 );
    setCount += inputMap.Value( x + 0, y - 1 );
    setCount += inputMap.Value( x + 1, y - 1 );

    setCount += inputMap.Value( x - 2, y + 0 );
    setCount += inputMap.Value( x - 1, y + 0 );
    setCount += inputMap.Value( x + 0, y + 0 );
    setCount += inputMap.Value( x + 1, y + 0 );
    setCount += inputMap.Value( x + 2, y + 0 );

    setCount += inputMap.Value( x - 1, y + 1 );
    setCount += inputMap.Value( x + 0, y + 1 );
    setCount += inputMap.Value( x + 1, y + 1 );

    setCount += inputMap.Value( x + 0, y + 2 );

    return setCount > 0;
}

//--------------------------------------------------------------------------------------
// Name: Dilate3x3()
// Applies a 3x3 morphological erode across the entire map, using the helpers above.
//--------------------------------------------------------------------------------------
VOID BinaryMap::Dilate3x3( const BinaryMap& inputMap )
{
    assert( &inputMap != this );   // Dilate does not work with source and dest the same

    // general case - 1 pixel in from the sides, we know the kernel cannot go outside
    // the map.
    for( UINT y = 1; y < BINARY_MAP_HEIGHT - 1; y++ )
    {
        for( UINT x = 1; x < BINARY_MAP_WIDTH - 1; x++ )
        {
            if( Kernel3x3DilateEvaluate( x, y, inputMap ) )
                SetValue( x, y );
            else
                ClearValue( x, y );
        }
    }

    // Run along the top and bottom edge, using the conditional kernel applies.
    // This includes the corners.
    for( UINT x = 0; x < BINARY_MAP_WIDTH; x++ )
    {
        if( Kernel3x3DilateEvaluateConditional( x, 0, inputMap ) )
            SetValue( x, 0 );
        else
            ClearValue( x, 0 );

        if( Kernel3x3DilateEvaluateConditional( x, BINARY_MAP_HEIGHT - 1, inputMap ) )
            SetValue( x, BINARY_MAP_HEIGHT - 1 );
        else
            ClearValue( x, BINARY_MAP_HEIGHT - 1 );
    }

    // Run along the left and right edge, using the conditional kernel applies.
    // Don't include the corners, we did them above.
    for( UINT y = 1; y < BINARY_MAP_HEIGHT - 1; y++ )
    {
        if( Kernel3x3DilateEvaluateConditional( 0, y, inputMap ) )
            SetValue( 0, y );
        else
            ClearValue( 0, y );

        if( Kernel3x3DilateEvaluateConditional( BINARY_MAP_WIDTH - 1, y, inputMap ) )
            SetValue( BINARY_MAP_WIDTH - 1, y );
        else
            ClearValue( BINARY_MAP_WIDTH - 1, y );
    }
}

//--------------------------------------------------------------------------------------
// Name: Dilate5x5()
// Applies a 5x5 morphological erode across the entire map, using the helpers above.
//--------------------------------------------------------------------------------------
VOID BinaryMap::Dilate5x5( const BinaryMap& inputMap )
{
    assert( &inputMap != this );	// Dilate does not work with source and dest the same

    // General case - we know that 2 pixels in from the edges, out 5x5 kernel won't go
    // outside the map so we don't need a conditional check on the x,y positions.
    for( UINT y = 2; y < BINARY_MAP_HEIGHT - 2; y++ )
    {
        for( UINT x = 2; x < BINARY_MAP_WIDTH - 2; x++ )
        {
            if( Kernel5x5DilateEvaluate( x, y, inputMap ) )
                SetValue( x, y );
            else
                ClearValue( x, y );
        }
    }

    // Run along top two rows of pixels and apply conditional kernel, including corners.
    for( UINT y = 0; y < 2; y++ )
    {
        for( UINT x = 0; x < BINARY_MAP_WIDTH; x++ )
        {
            if( Kernel5x5DilateEvaluateConditional( x, y, inputMap ) )
                SetValue( x, y );
            else
                ClearValue( x, y );
        }
    }

    // Run along bottom two rows of pixels and apply conditional kernel, including corners.
    for( UINT y = BINARY_MAP_HEIGHT - 2; y < BINARY_MAP_HEIGHT; y++ )
    {
        for( UINT x = 0; x < BINARY_MAP_WIDTH; x++ )
        {
            if( Kernel5x5DilateEvaluateConditional( x, y, inputMap ) )
                SetValue( x, y );
            else
                ClearValue( x, y );
        }
    }

    // Run down left 2 columns of pixels (don't include corners)
    for( UINT y = 2; y < BINARY_MAP_HEIGHT - 2; y++ )
    {
        for( UINT x = 0; x < 2; x++ )
        {
            if( Kernel5x5DilateEvaluateConditional( x, y, inputMap ) )
                SetValue( x, y );
            else
                ClearValue( x, y );
        }
    }

    // Run down right 2 columns of pixels (don't include corners)
    for( UINT y = 2; y < BINARY_MAP_HEIGHT - 2; y++ )
    {
        for( UINT x = BINARY_MAP_WIDTH - 2; x < BINARY_MAP_WIDTH; x++ )
        {
            if( Kernel5x5DilateEvaluateConditional( x, y, inputMap ) )
                SetValue( x, y );
            else
                ClearValue( x, y );
        }
    }
}
