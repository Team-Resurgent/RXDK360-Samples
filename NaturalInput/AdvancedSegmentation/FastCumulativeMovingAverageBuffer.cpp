//--------------------------------------------------------------------------------------
// FastCumulativeMovingAverageBuffer.cpp
//
// Advanced Technology Group
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------


#include <xtl.h>
#include <assert.h>

#include "FastCumulativeMovingAverageBuffer.h"

//--------------------------------------------------------------------------------------
// UpdateColor()
// Helper function to 'moving average' colors. 
//--------------------------------------------------------------------------------------
static __forceinline RGBAValue UpdateColor( const RGBAValue currentColor, const RGBAValue newColor )
{
    UINT red = currentColor.GetRed() + ( newColor.GetRed() - currentColor.GetRed() ) /
        FastCumulativeMovingAverageBuffer::UPDATE_FRACTION;
    UINT green = currentColor.GetGreen() + ( newColor.GetGreen() - currentColor.GetGreen() ) /
        FastCumulativeMovingAverageBuffer::UPDATE_FRACTION;
    UINT blue = currentColor.GetBlue() + ( newColor.GetBlue() - currentColor.GetBlue() ) /
        FastCumulativeMovingAverageBuffer::UPDATE_FRACTION;

    return RGBAValue( red, green, blue );

}
//--------------------------------------------------------------------------------------
// Update2x2ColorPixels()
// Helper function for update methods.
// Our validity map is 320x240 for reasons listed above. When we find a valid pixel,
// we need to update a 2x2 pixel area in the color map. To avoid code replication
// extract into inlined helper.
//--------------------------------------------------------------------------------------

static __forceinline VOID Update2x2ColorPixels( const UINT x, const UINT y, FastColorMap& currentColorMap,
                                                const FastColorMap& newColorMap, FastBinaryMap& validMap )
{
    if( validMap.Value( x, y ) )
    {
        RGBAValue currentColor;
        RGBAValue newColor;
        UINT i, j;

        i = ( x << 1 ) + 0;
        j = ( y << 1 ) + 0;
        currentColor = currentColorMap.Value( i, j );
        newColor = newColorMap.Value( i, j );
        currentColorMap.SetValue( i, j, UpdateColor( currentColor, newColor ) );

        i = ( x << 1 ) + 1;
        j = ( y << 1 ) + 0;
        currentColor = currentColorMap.Value( i, j );
        newColor = newColorMap.Value( i, j );
        currentColorMap.SetValue( i, j, UpdateColor( currentColor, newColor ) );

        i = ( x << 1 ) + 0;
        j = ( y << 1 ) + 1;
        currentColor = currentColorMap.Value( i, j );
        newColor = newColorMap.Value( i, j );
        currentColorMap.SetValue( i, j, UpdateColor( currentColor, newColor ) );

        i = ( x << 1 ) + 1;
        j = ( y << 1 ) + 1;
        currentColor = currentColorMap.Value( i, j );
        newColor = newColorMap.Value( i, j );
        currentColorMap.SetValue( i, j, UpdateColor( currentColor, newColor ) );
    }
    else
    {
        // Invalid value for 4 pixels in the CMA (remember binary map is half size)
        // Just set the value for the 4 pixels this time around.

        const INT i = ( x << 1 );
        const INT j = ( y << 1 );
        currentColorMap.SetValue( i + 0, j + 0, newColorMap.Value( i + 0, j + 0 ) );
        currentColorMap.SetValue( i + 0, j + 1, newColorMap.Value( i + 0, j + 1 ) );
        currentColorMap.SetValue( i + 1, j + 0, newColorMap.Value( i + 1, j + 0 ) );
        currentColorMap.SetValue( i + 1, j + 1, newColorMap.Value( i + 1, j + 1 ) );

        // Mark this portion of the CMA buffer active
        validMap.SetValue( x, y );
    }
}

//--------------------------------------------------------------------------------------
// Update()
// Updates entire color map from supplied image.
//--------------------------------------------------------------------------------------
VOID FastCumulativeMovingAverageBuffer::Update( const FastColorMap& newColorImage )
{
    // Run through pixels in map. 
    // We run through at half resolution (since the CMA valid map is 320x240 resolution).
    // Where we find invalid pixels in the validity map, just set the four color map
    // pixels to the current color. Where valid, update pixels with a fraction of the
    // difference between input and current colors.

    for( UINT y = 0; y < FastBinaryMap::BINARY_MAP_HEIGHT; y++ )
    {
        for( UINT x = 0; x < FastBinaryMap::BINARY_MAP_WIDTH; x++ )
        {
            Update2x2ColorPixels( x, y, m_CumulativeAverageImage, newColorImage, m_ValidPixels );
        }
    }
}

//--------------------------------------------------------------------------------------
// UpdateSelected()
// Updates color map in areas specified by the supplied BinaryMap.
// Typically, this is used to build a picture of the background in a NUI app. By
// filtering the foreground out (from the depth map or segmentation map input), we can
// build up a cumulative average color model of the background only, and this is useful
// for classifying pixels as foreground or background.
// For performance reasons, minimising the set area of the selectMap is a good idea.
//--------------------------------------------------------------------------------------
VOID FastCumulativeMovingAverageBuffer::UpdateSelected( const FastColorMap& newColorImage, const FastBinaryMap& selectMap )
{
    // Run through pixels in map. 
    // Run through at half resolution (since the valid map is 320x240 resolution).
    // Where we find invalid pixels in the validity map, just set the four color map
    // pixels to the current color. Where valid, update pixels with a fraction of the
    // difference between input and current colors.
    // However, only do any of the above where the select map says so.

    for( UINT y = 0; y < FastBinaryMap::BINARY_MAP_HEIGHT; y++ )
    {
        for( UINT x = 0; x < FastBinaryMap::BINARY_MAP_WIDTH; x++ )
        {
            if( selectMap.Value( x, y ) )
                Update2x2ColorPixels( x, y, m_CumulativeAverageImage, newColorImage, m_ValidPixels );
        }
    }
}
