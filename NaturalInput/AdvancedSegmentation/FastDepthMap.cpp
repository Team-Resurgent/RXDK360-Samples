//--------------------------------------------------------------------------------------
// FastDepthMap.cpp
//
// Implements a depth map intended to handle NUI depth maps including the 3 bit 
// segmentation mask, appropriate shifting, etc. Its size is fixed at 320x240.
// This file contains straight C implementations. These are not generally optimized for 
// Xbox360 but intended to show the algorithms in a plain and understandable way.
//
// Advanced Technology Group
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include <xtl.h>
#include <assert.h>

#include "RGBAValue.h"
#include "FastDepthMap.h"
#include "FastBinaryMap.h"

#include "CatmullRomColorSpline.h"

//--------------------------------------------------------------------------------------
// Name: CopyToTexture()
// Copies the depth map to the D3D texture supplied. Depths are just shifted
// down to make them 8 bit greyscale. This loses a lot of information.
//--------------------------------------------------------------------------------------
VOID FastDepthMap::CopyToTexture( IDirect3DTexture9* pTexture ) const
{
    assert( pTexture != NULL );

    // Only 1 level - we won't fill mips in
    assert( pTexture->GetLevelCount() == 1 );

    // Check width and height match.
    D3DSURFACE_DESC surfaceDesc;
    pTexture->GetLevelDesc( 0, &surfaceDesc );

    assert( surfaceDesc.Width == DEPTH_MAP_WIDTH );
    assert( surfaceDesc.Height == DEPTH_MAP_HEIGHT );

    // Lock the surface rect
    D3DLOCKED_RECT lockRect;
    pTexture->LockRect( 0, &lockRect, NULL, 0 );

    // Make start position
    BYTE* pPosition = ( BYTE* )lockRect.pBits;

    switch( surfaceDesc.Format )
    {
        case D3DFMT_LIN_A8R8G8B8:
        case D3DFMT_LIN_X8R8G8B8:
            for( UINT y = 0; y < DEPTH_MAP_HEIGHT; y++ )
            {
                // Make row pointer and fill
                DWORD* pRow = ( DWORD* )pPosition;
                for( UINT x = 0; x < DEPTH_MAP_WIDTH; x++ )
                {
                    const UINT intensity = Value( x, y ).ByteDepth();
                    pRow[x] = RGBAValue( intensity, intensity, intensity );
                }

                pPosition += lockRect.Pitch;                // To next row
            }
            break;

        default:
            assert( false );                                // Format not implemented
            break;
    }

    pTexture->UnlockRect( 0 );
}

//--------------------------------------------------------------------------------------
// Name: CopyToTexture()
// Copies the depth map to the D3D texture supplied. Depths are used to pull an 
// interpolated color from a color spline, which allows better illustration of full
// depth range.
//--------------------------------------------------------------------------------------
VOID FastDepthMap::CopyToTexture( IDirect3DTexture9* pTexture, const CatmullRomColorSpline& spline ) const
{
    assert( pTexture != NULL );
    assert( pTexture->GetLevelCount() == 1 );

    // Get surface desc
    D3DSURFACE_DESC surfaceDesc;
    pTexture->GetLevelDesc( 0, &surfaceDesc );

    assert( surfaceDesc.Width == DEPTH_MAP_WIDTH );
    assert( surfaceDesc.Height == DEPTH_MAP_HEIGHT );

    // Lock surface
    D3DLOCKED_RECT lockRect;
    pTexture->LockRect( 0, &lockRect, NULL, 0 );

    // Make start position
    BYTE* pPosition = ( BYTE* )lockRect.pBits;
    switch( surfaceDesc.Format )
    {
        case D3DFMT_LIN_A8R8G8B8:
        case D3DFMT_LIN_X8R8G8B8:
            for( UINT y = 0; y < DEPTH_MAP_HEIGHT; y++ )
            {
                DWORD* pRow = ( DWORD* )pPosition;
                for( UINT x = 0; x < DEPTH_MAP_WIDTH; x++ )
                {
                    // Interpolate color from spline with depth
                    pRow[x] = spline.Interpolate( Value( x, y ) );
                }

                pPosition += lockRect.Pitch;           // To next row
            }
        default:
            assert( false );
            break;
    }

    pTexture->UnlockRect( 0 );
}


//--------------------------------------------------------------------------------------
// Name: Fill()
// Fills the depth map from the provided array of unsigned short values, skipping 
// between rows using the stride provided.
//--------------------------------------------------------------------------------------
VOID FastDepthMap::Fill( const USHORT* pValues, const UINT stride )
{
    BYTE* pPosition = ( BYTE* )pValues;
    for( UINT y = 0; y < DEPTH_MAP_HEIGHT; y++ )
    {
        USHORT* pRow = ( USHORT* )pPosition;
        for( UINT x = 0; x < DEPTH_MAP_WIDTH; x++ )
        {
            if( pRow[x] == 0 )
            {
                SetValue( x, y, DepthValue( DepthValue::MAX_DEPTH, 0 ) );
            }
            else
            {
                SetValue( x, y, pRow[x] );
            }
        }

        pPosition += stride;                 // To next row
    }
}

//--------------------------------------------------------------------------------------
// Name: Fill()
// Fills the depth map with the provided DepthValue.
//--------------------------------------------------------------------------------------
VOID FastDepthMap::Fill( const DepthValue value )
{
    for( UINT y = 0; y < DEPTH_MAP_HEIGHT; y++ )
    {
        for( UINT x = 0; x < DEPTH_MAP_WIDTH; x++ )
        {
            SetValue( x, y, value );
        }
    }
}

//--------------------------------------------------------------------------------------
// Name: Sobel()
// Sets this map to the result of Sobel edge detection from the supplied map.
//--------------------------------------------------------------------------------------

VOID FastDepthMap::Sobel( const FastDepthMap& inputMap )
{
    // Convolution kernel applies SO do not work outputting to the same map as the source map
    assert( &inputMap != this );

    for( UINT y = 1; y < DEPTH_MAP_HEIGHT - 1; y++ )
    {
        for( UINT x = 1; x < DEPTH_MAP_WIDTH - 1; x++ )
        {
            // Horizontal edge detect
            INT hMagnitude = 0;

            hMagnitude += inputMap.Value( x - 1, y - 1 ).IntDepth() * -1;
            hMagnitude += inputMap.Value( x + 0, y - 1 ).IntDepth() * -2;
            hMagnitude += inputMap.Value( x + 1, y - 1 ).IntDepth() * -1;

            hMagnitude += inputMap.Value( x - 1, y + 1 ).IntDepth() * 1;
            hMagnitude += inputMap.Value( x + 0, y + 1 ).IntDepth() * 2;
            hMagnitude += inputMap.Value( x + 1, y + 1 ).IntDepth() * 1;

            // Vertical edge detect
            INT vMagnitude = 0;

            vMagnitude += inputMap.Value( x - 1, y - 1 ).IntDepth() * -1;
            vMagnitude += inputMap.Value( x - 1, y + 0 ).IntDepth() * -2;
            vMagnitude += inputMap.Value( x - 1, y + 1 ).IntDepth() * -1;

            vMagnitude += inputMap.Value( x + 1, y - 1 ).IntDepth() * 1;
            vMagnitude += inputMap.Value( x + 1, y + 0 ).IntDepth() * 2;
            vMagnitude += inputMap.Value( x + 1, y + 1 ).IntDepth() * 1;

            const UINT newDepth = min( abs( hMagnitude ) + abs( vMagnitude ), DepthValue::MAX_DEPTH );
            SetValue( x, y, DepthValue( newDepth, 0 ) );
        }
    }
}


