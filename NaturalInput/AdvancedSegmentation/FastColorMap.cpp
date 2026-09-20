//--------------------------------------------------------------------------------------
// FastColorMap.cpp
//
// Implements a color map, intended for use with NUI or general image processing.
// Intended to be an optimized path; currently only partially optimized.
//
// Advanced Technology Group
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include <xtl.h>
#include <assert.h>

#include "RGBAValue.h"
#include "FastColorMap.h"
#include "FastDepthMap.h"
#include "FastBinaryMap.h"
#include "FastIntensityMap.h"
#include "FastCumulativeMovingAverageBuffer.h"

//--------------------------------------------------------------------------------------
// Name: CopyToTexture()
// Copies color data to D3D texture.
//--------------------------------------------------------------------------------------

VOID FastColorMap::CopyToTexture( IDirect3DTexture9* pTexture ) const
{
    assert( pTexture != NULL );
    assert( pTexture->GetLevelCount() == 1 );

    D3DSURFACE_DESC surfaceDesc;
    pTexture->GetLevelDesc( 0, &surfaceDesc );

    assert( surfaceDesc.Width == COLOR_MAP_WIDTH );
    assert( surfaceDesc.Height == COLOR_MAP_HEIGHT );

    D3DLOCKED_RECT lockRect;
    pTexture->LockRect( 0, &lockRect, NULL, 0 );


    BYTE* pPosition = ( BYTE* )lockRect.pBits;
    switch( surfaceDesc.Format )
    {
        case D3DFMT_LIN_X8R8G8B8:
        case D3DFMT_LIN_A8R8G8B8:
            // Same pitch means we can memcpy
            if( lockRect.Pitch == COLOR_MAP_STRIDE )
            {
                memcpy( lockRect.pBits, m_Colors, COLOR_MAP_SIZE );
            }
            else
            {
                // Copy row by row; at end of each row, step with stride
                // in target image.
                for( UINT y = 0; y < COLOR_MAP_HEIGHT; y++ )
                {
                    DWORD* pRow = ( DWORD* )pPosition;
                    for( UINT x = 0; x < COLOR_MAP_WIDTH; x++ )
                    {
                        pRow[x] = Value( x, y );
                    }
                    pPosition += lockRect.Pitch;
                }
            }
            break;
        default:
            assert( false );      // No implementation for this texture type
            break;
    }

    pTexture->UnlockRect( 0 );
}

//--------------------------------------------------------------------------------------
// Name: Copy()
// Fills this color map from an array of DWORDs containing colors with a possible stride
//--------------------------------------------------------------------------------------
VOID FastColorMap::Fill( const DWORD* pWords, const UINT stride )
{
    if( stride == COLOR_MAP_STRIDE )
    {
        // Memcpy in one go.
        XMemCpy( m_Colors, pWords, ( COLOR_MAP_HEIGHT * COLOR_MAP_WIDTH * sizeof( RGBAValue ) ) );
    }
    else
    {
        // Memcpy row by row as we have a stride bigger than our width, ie junk on the end of rows
        const BYTE* pPosition = ( const BYTE* )pWords;
        for( UINT y = 0; y < COLOR_MAP_HEIGHT; y++ )
        {
            XMemCpy( m_Colors[y], pPosition, COLOR_MAP_WIDTH * sizeof( RGBAValue ) );
            pPosition += stride;
        }
    }
}


//--------------------------------------------------------------------------------------
// Name: Fill()
// Fills the map with the selected RGB value
//--------------------------------------------------------------------------------------
VOID FastColorMap::Fill( const RGBAValue color )
{
    for( UINT y = 0; y < COLOR_MAP_HEIGHT; y++ )
    {
        for( UINT x = 0; x < COLOR_MAP_WIDTH; x++ )
        {
            SetValue( x, y, color );
        }
    }
}

//--------------------------------------------------------------------------------------
// Name: Select()
// Zeroes pixels where the input select map is not set, and leaves other pixels alone.
// The input select map is assumed to be half the size of the color map (as per NUI).
//--------------------------------------------------------------------------------------
VOID FastColorMap::SelectPixels( const FastBinaryMap& selectMap )
{
    const RGBAValue backgroundColor(0);
    for( UINT y = 0; y < COLOR_MAP_HEIGHT; y++ )
    {
        for( UINT x = 0; x < COLOR_MAP_WIDTH; x++ )
        {
            if( selectMap.Value( x >> 1, y >> 1 ) == 0 )
                SetValue( x, y, backgroundColor );
        }
    }
}

//--------------------------------------------------------------------------------------
// Name: Select()
// Copies pixels from the input color map where indicated by the select map.
// The input select map is assumed to be half the size of the color map (as per NUI).
//--------------------------------------------------------------------------------------
VOID FastColorMap::SelectPixels( const FastColorMap& map, const FastBinaryMap& selectMap )
{
    const RGBAValue backgroundColor( 0 );
    for( UINT y = 0; y < COLOR_MAP_HEIGHT; y++ )
    {
        for( UINT x = 0; x < COLOR_MAP_WIDTH; x++ )
        {
            if( selectMap.Value( x >> 1, y >> 1 ) == 0 )
                SetValue( x, y, backgroundColor );
            else
                SetValue( x, y, map.Value( x, y ) );
        }
    }
}

//--------------------------------------------------------------------------------------
// Name: Sobel()
// Runs the Sobel edge detector on the input map and puts the result in this map.
// The response is clamped at 255 (since this is an 8 bit map), and returned in all 3
// channels. This is typically not that useful; try converting to an intensity map
// and doing Sobel from there; 1/3rd the size and more meaningful/applicable results.
// Currently, this version does not do edge detection on the 1 pixel boundary of the map.
//--------------------------------------------------------------------------------------
VOID FastColorMap::SobelEdgeDetect( const FastColorMap& inputMap )
{
    assert( &inputMap != this );		// cannot do Sobel with source and dest the same

    for( UINT y = 1; y < COLOR_MAP_HEIGHT - 1; y++ )
    {
        for( UINT x = 1; x < COLOR_MAP_WIDTH - 1; x++ )
        {
            const RGBAValue xmym = inputMap.Value( x - 1, y - 1 );
            const RGBAValue x0ym = inputMap.Value( x + 0, y - 1 );
            const RGBAValue xpym = inputMap.Value( x + 1, y - 1 );

            const RGBAValue xmy0 = inputMap.Value( x - 1, y + 0 );
            const RGBAValue xpy0 = inputMap.Value( x + 1, y + 0 );

            const RGBAValue xmyp = inputMap.Value( x - 1, y + 1 );
            const RGBAValue x0yp = inputMap.Value( x + 0, y + 1 );
            const RGBAValue xpyp = inputMap.Value( x + 1, y + 1 );

            INT iHRsum = 0;
            INT iHGsum = 0;
            INT iHBsum = 0;

            iHRsum += xmym.GetRed() * -1;
            iHRsum += x0ym.GetRed() * -2;
            iHRsum += xpym.GetRed() * -1;
            iHRsum += xmyp.GetRed() * 1;
            iHRsum += x0yp.GetRed() * 2;
            iHRsum += xpyp.GetRed() * 1;

            iHGsum += xmym.GetGreen() * -1;
            iHGsum += x0ym.GetGreen() * -2;
            iHGsum += xpym.GetGreen() * -1;
            iHGsum += xmyp.GetGreen() * 1;
            iHGsum += x0yp.GetGreen() * 2;
            iHGsum += xpyp.GetGreen() * 1;

            iHBsum += xmym.GetBlue() * -1;
            iHBsum += x0ym.GetBlue() * -2;
            iHBsum += xpym.GetBlue() * -1;
            iHBsum += xmyp.GetBlue() * 1;
            iHBsum += x0yp.GetBlue() * 2;
            iHBsum += xpyp.GetBlue() * 1;


            INT iVRsum = 0;
            INT iVGsum = 0;
            INT iVBsum = 0;

            iVRsum += xmym.GetRed() * -1;
            iVRsum += xmy0.GetRed() * -2;
            iVRsum += xmyp.GetRed() * -1;
            iVRsum += xpym.GetRed() * 1;
            iVRsum += xpy0.GetRed() * 2;
            iVRsum += xpyp.GetRed() * 1;

            iVGsum += xmym.GetGreen() * -1;
            iVGsum += xmy0.GetGreen() * -2;
            iVGsum += xmyp.GetGreen() * -1;
            iVGsum += xpym.GetGreen() * 1;
            iVGsum += xpy0.GetGreen() * 2;
            iVGsum += xpyp.GetGreen() * 1;

            iVBsum += xmym.GetBlue() * -1;
            iVBsum += xmy0.GetBlue() * -2;
            iVBsum += xmyp.GetBlue() * -1;
            iVBsum += xpym.GetBlue() * 1;
            iVBsum += xpy0.GetBlue() * 2;
            iVBsum += xpyp.GetBlue() * 1;

            UINT newr = min( abs( iHRsum ) + abs( iVRsum ), 255 );
            UINT newg = min( abs( iHGsum ) + abs( iVGsum ), 255 );
            UINT newb = min( abs( iHBsum ) + abs( iVBsum ), 255 );

            SetValue( x, y, RGBAValue( newr, newg, newb ) );
        }
    }
}


//--------------------------------------------------------------------------------------
// Name: SelectForeground()
// This is the final stage of segmenting a full image at 640x480 resolution.
// The method segments the color map provided as originalImage - typically this is the
// camera color frame. The image is segmented via the following rule:
// - All pixels outside the set (fgPixels U edgePixels) are zeroed.
// - Every pixel indicated as foreground in the BinaryMap fgPixels is set
// - Every pixel set in the map 'edgeDifferences' which is typically the grey scale xor
//   of two 640x480 Sobel edge detection results, full image and background image
//   This requires brief explanation:
//      Given an edge detection mask for foreground image (i.e. containing player or object
//      to be segmented) and background (ie, not containing player), any pixel
//      with a high edge detection value in one map but not the other is definitely 
//      foreground. Imagine the two cases of high and low values:
//      bg high, fg low: There is an edge in the background we can't see. That means
//      something in the foreground is occluding the edge at this pixel.
//      bg low, fg high: An edge is present in the foreground not part of the background
//      which by definition must be part of the foreground.
//      This classification seems strange, but all pixels set are foreground.
// - every pixel the CumulativeMovingAverageBuffer classifies as foreground
// - remaining unclassified pixels are judged with the depth map
// Performance is maintained by only reading the 640x480 color map as a last resort -
// classification is done via BinaryMap where possible.
//--------------------------------------------------------------------------------------

bool debugcols = false;

VOID FastColorMap::SelectForeground( const FastColorMap& originalImage, const FastIntensityMap& edgeDifferences,
                                 const FastBinaryMap& fgPixels, const FastBinaryMap& edgePixels,
                                 const FastCumulativeMovingAverageBuffer& cma, const FastBinaryMap& segmentationMap)

{
	Clear();		// Quicker than filling in the background color ourselves

    const RGBAValue backgroundColor( 0 );

    for( UINT y = 0; y < COLOR_MAP_HEIGHT; y++ )
    {
        for( UINT x = 0; x < COLOR_MAP_WIDTH; x++ )
        {
            if( fgPixels.Value( x >> 1, y >> 1 ) )
            {
                // Pixel is definitely part of the foreground
                const RGBAValue originalColor = originalImage.Value( x, y );
                SetValue( x, y, originalColor );
            }
            else
            {
                if( edgePixels.Value( x >> 1, y >> 1 ) )
                {
                    const RGBAValue originalColor = originalImage.Value( x, y );
                    // Pixel is in the silhoutte area
                    if( edgeDifferences.Value( x, y ) > 0 )
                    {
                        // Pixel is definitely foreground, from edge difference map
                        SetValue( x, y, originalColor );
                    }
                    else
                    {
                        // Use the cumulative moving average buffer to decide
                        // distance from the color in the background
                        UINT distance = cma.ForegroundProbability( x, y, originalColor );
                        if( distance > FOREGROUND_DIFFERENCE_THRESHOLD_SQ)
                        {
                            SetValue( x, y, originalColor );
                        }
   
                    }
                }
            }
        }
 	}
}
