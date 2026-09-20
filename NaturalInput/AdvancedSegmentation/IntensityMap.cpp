//--------------------------------------------------------------------------------------
// IntensityMap.cpp
//
// Implementation of intensity fixed size map for use with NUI color images. The methods
// here are written for clarity, not optimal performance.
//
// Advanced Technology Group
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include <xtl.h>
#include <assert.h>

#include "RGBAValue.h"
#include "IntensityMap.h"
#include "ColorMap.h"
#include "BinaryMap.h"

//--------------------------------------------------------------------------------------
// Name: CopyToTexture()
// Copies grey scale data out to D3D texture
//-------------------------------------------------------------------------------------
VOID IntensityMap::CopyToTexture( IDirect3DTexture9* pTexture ) const
{
    assert( pTexture != NULL );
    assert( pTexture->GetLevelCount() == 1 );

    D3DSURFACE_DESC surfaceDesc;
    pTexture->GetLevelDesc( 0, &surfaceDesc );

    assert( surfaceDesc.Width == INTENSITY_MAP_WIDTH );
    assert( surfaceDesc.Height == INTENSITY_MAP_HEIGHT );

    D3DLOCKED_RECT lockRect;
    pTexture->LockRect( 0, &lockRect, NULL, 0 );

    BYTE* pPosition = ( BYTE* )lockRect.pBits;
    switch( surfaceDesc.Format )
    {
        case D3DFMT_LIN_X8R8G8B8:
        case D3DFMT_LIN_A8R8G8B8:

            for( UINT y = 0; y < INTENSITY_MAP_HEIGHT; y++ )
            {
                DWORD* pRow = ( DWORD* )pPosition;
                for( UINT x = 0; x < INTENSITY_MAP_WIDTH; x++ )
                {
                    const UINT intensity = Value( x, y );
                    pRow[x] = RGBAValue( intensity, intensity, intensity );
                }

                pPosition += lockRect.Pitch;
            }
            break;
        default:
            assert( false );     // No implementation for this texture type
            break;
    }

    pTexture->UnlockRect( 0 );
}


//--------------------------------------------------------------------------------------
// Name: Fill()
// Fills map from a ColorMap; conversion to intensity handled by RGBAValue
//-------------------------------------------------------------------------------------
VOID IntensityMap::Fill( const ColorMap& other )
{
    for( UINT y = 0; y < INTENSITY_MAP_HEIGHT; y++ )
    {
        for( UINT x = 0; x < INTENSITY_MAP_WIDTH; x++ )
        {
            const RGBAValue color = other.Value( x, y );
            SetValue( x, y, ( BYTE )color.GetIntensity() );
        }
    }
}

//--------------------------------------------------------------------------------------
// Name: Fill()
// Fills map from an array of BYTE values with an optional stride
//-------------------------------------------------------------------------------------
VOID IntensityMap::Fill( const BYTE* pValues, const UINT stride )
{
    if( stride == INTENSITY_MAP_STRIDE )
    {
        // Memcpy in one go.
        XMemCpy( m_Intensities, pValues, ( INTENSITY_MAP_HEIGHT * INTENSITY_MAP_WIDTH ) );
    }
    else
    {
        // Memcpy row by row as we have a stride bigger than our width, ie junk on the end of rows
        const BYTE* pPosition = pValues;
        for( UINT y = 0; y < INTENSITY_MAP_HEIGHT; y++ )
        {
            XMemCpy( m_Intensities[y], pPosition, INTENSITY_MAP_WIDTH );
            pPosition += stride;
        }
    }
}

//--------------------------------------------------------------------------------------
// Name: Sobel()
// Carries out Sobel operator on input map and sets this map to results
//-------------------------------------------------------------------------------------
VOID IntensityMap::SobelEdgeDetect( const IntensityMap& inputMap )
{
    // Start 1 pixel in, because we know the kernel will fit inside the map
    // and thus dump a load of error checking.
    for( UINT y = 1; y < INTENSITY_MAP_HEIGHT - 1; y++ )
    {
        for( UINT x = 1; x < INTENSITY_MAP_WIDTH - 1; x++ )
        {
            // Cache required intensities.
            const UINT xmym = inputMap.Value( x - 1, y - 1 );
            const UINT x0ym = inputMap.Value( x + 0, y - 1 );
            const UINT xpym = inputMap.Value( x + 1, y - 1 );

            const UINT xmy0 = inputMap.Value( x - 1, y + 0 );
            const UINT xpy0 = inputMap.Value( x + 1, y + 0 );

            const UINT xmyp = inputMap.Value( x - 1, y + 1 );
            const UINT x0yp = inputMap.Value( x + 0, y + 1 );
            const UINT xpyp = inputMap.Value( x + 1, y + 1 );

            // Make horizontal and vertical detection sums
            INT hsum = 0;
            INT vsum = 0;

            hsum += xmym * -1;
            hsum += x0ym * -2;
            hsum += xpym * -1;
            hsum += xmyp * 1;
            hsum += x0yp * 2;
            hsum += xpyp * 1;

            vsum += xmym * -1;
            vsum += xmy0 * -2;
            vsum += xmyp * -1;
            vsum += xpym * 1;
            vsum += xpy0 * 2;
            vsum += xpyp * 1;

            // Sum abs values and clamp
            UINT intensity = min( abs( hsum ) + abs( vsum ), UCHAR_MAX );
            SetValue( x, y, intensity );
        }
    }
}

//--------------------------------------------------------------------------------------
// Name: Sobel()
// Carries out Sobel operator on input map and sets this map to results
// Only returns edge detector results in pixels indicated by the BinaryMap selectMap
//-------------------------------------------------------------------------------------
VOID IntensityMap::SobelEdgeDetect( const IntensityMap& inputMap, const BinaryMap& selectMap )
{
    for( UINT y = 1; y < INTENSITY_MAP_HEIGHT - 1; y++ )
    {
        for( UINT x = 1; x < INTENSITY_MAP_WIDTH - 1; x++ )
        {
            // Apply Sobel operator on pixels selected only
            if( selectMap.Value( x >> 1, y >> 1 ) )
            {
                const INT xmym = inputMap.Value( x - 1, y - 1 );
                const INT x0ym = inputMap.Value( x + 0, y - 1 );
                const INT xpym = inputMap.Value( x + 1, y - 1 );

                const INT xmy0 = inputMap.Value( x - 1, y + 0 );
                const INT xpy0 = inputMap.Value( x + 1, y + 0 );

                const INT xmyp = inputMap.Value( x - 1, y + 1 );
                const INT x0yp = inputMap.Value( x + 0, y + 1 );
                const INT xpyp = inputMap.Value( x + 1, y + 1 );

                INT hsum = 0;
                INT vsum = 0;

                hsum += xmym * -1;
                hsum += x0ym * -2;
                hsum += xpym * -1;
                hsum += xmyp * 1;
                hsum += x0yp * 2;
                hsum += xpyp * 1;

                vsum += xmym * -1;
                vsum += xmy0 * -2;
                vsum += xmyp * -1;
                vsum += xpym * 1;
                vsum += xpy0 * 2;
                vsum += xpyp * 1;

                UINT intensity = abs( hsum ) + abs( vsum );
                intensity = min( intensity, 255 );

                SetValue( x, y, intensity );
            }
        }
    }
}

//--------------------------------------------------------------------------------------
// Name: Difference()
// Sets this map to the abs difference between this and the input map.
// Values are thresholded. (This method should perhaps belong to BinaryMap)
//-------------------------------------------------------------------------------------

VOID IntensityMap::IntensityDifference( const IntensityMap& inputMap )
{
    const UINT thresholdIntensity = 127;
    const UINT backgroundIntensity = 0;

    for( UINT y = 0; y < INTENSITY_MAP_HEIGHT; y++ )
    {
        for( UINT x = 0; x < INTENSITY_MAP_WIDTH; x++ )
        {
            const INT iDifference = Value( x, y ) - inputMap.Value( x, y );
            const UINT intensity = abs( iDifference );

            if( intensity > EDGE_SIGNIFICANT_THRESHOLD )
                SetValue( x, y, thresholdIntensity );
            else
                SetValue( x, y, backgroundIntensity );
        }
    }
}

//--------------------------------------------------------------------------------------
// Name: Difference()
// Sets this map to the abs difference between this and the input map.
// Values are thresholded. (This method should perhaps belong to BinaryMap)
//-------------------------------------------------------------------------------------

VOID IntensityMap::IntensityDifference( const IntensityMap& inputMap0, const IntensityMap& inputMap1 )
{
    const UINT thresholdIntensity = 127;
    const UINT backgroundIntensity = 0;

    for( UINT y = 0; y < INTENSITY_MAP_HEIGHT; y++ )
    {
        for( UINT x = 0; x < INTENSITY_MAP_WIDTH; x++ )
        {
            const INT iDifference = inputMap1.Value( x, y ) - inputMap0.Value( x, y );
            const UINT intensity = abs( iDifference );

            if( intensity > EDGE_SIGNIFICANT_THRESHOLD )
                SetValue( x, y, thresholdIntensity );
            else
                SetValue( x, y, backgroundIntensity );
        }
    }
}
