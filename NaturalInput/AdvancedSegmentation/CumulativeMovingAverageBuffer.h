//--------------------------------------------------------------------------------------
// CumulativeMovingAverageBuffer.h
//
// Implements a pseudo cumulative moving average buffer, with a mask for maintaining 
// valid and invalid pixels, to facilitate selective updating of the moving average.
// The color map is 640x480, but the select mask is 320x240. This is because using a 
// small select mask helps performance, but also because the select mask is typically
// derived from some convolution of a NUI depth map, which is 320x240.
// The class keeps a colormap with the average color in it, and a BinaryMap for the mask.
// It is a pseudo-moving average because keeping a real moving average requires dividing
// differences by count of updates (sample set size) which will quickly become enormous/
// lose accuracy even with float. Instead the update is a fraction of color difference
// per frame. This is not frame rate independent, or correct, but it is fast.
// ('correct' means it does not implement a true cumulative moving average, which would
// need a large history buffer, and also need to divide by ever increasing values which
// has a definite acuracy issue).
// The class is intended for use in classifying pixels as background or foreground,
// depending on their difference from the averaged color buffer. Because the buffer
// will change quickly if the input image intensity changes quickly, it can adapt
// to fast intensity changes in the scene which is important as the NUI camera can
// give quite quick intensity change for a mostly fixed scene.
//
// Advanced Technology Group
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#ifndef CUMULATIVE_MOVING_AVERAGE_BUFFER_H
#define CUMULATIVE_MOVING_AVERAGE_BUFFER_H

#include "ColorMap.h"
#include "BinaryMap.h"

//--------------------------------------------------------------------------------------
// Cumulative moving average buffer class.
//--------------------------------------------------------------------------------------
class CumulativeMovingAverageBuffer
{
public:

    enum CumulativeAveragingBufferConstants
    {
        UPDATE_FRACTION = 4
    };

    // Default constructor clears valid pixel map to zero.
    CumulativeMovingAverageBuffer()
    {
        m_CumulativeAverageImage.Clear();
        m_ValidPixels.Clear();
    }

    // Get validity for pixel at x,y (x,y in color map space)
    __forceinline UINT ValidPixel( const UINT x, const UINT y )
    {
        assert( x < ColorMap::COLOR_MAP_WIDTH );
        assert( y < ColorMap::COLOR_MAP_HEIGHT );
        return m_ValidPixels.Value( x >> 1, y >> 1 );
    }

    // Get color value from the averaged color buffer
    // Note that this may be an invalid color, it's client responsibility
    // to check appropriately depending on their update methodology
    __forceinline RGBAValue Value( const UINT x, const UINT y ) const
    {
        assert( x < ColorMap::COLOR_MAP_WIDTH );
        assert( y < ColorMap::COLOR_MAP_HEIGHT );
        return m_CumulativeAverageImage.Value( x, y );
    }

    // Return the internal color map
    __forceinline const ColorMap& GetColorMap() const
    {
        return m_CumulativeAverageImage;
    }
    // Return the internal validity map
    __forceinline const BinaryMap& GetValidMap() const
    {
        return m_ValidPixels;
    }

    // Give back a probability that an input color at a position is a background pixel
    // This ignores the validity map so it is client responsibility to use the right
    // matching function
    __forceinline UINT ForegroundProbability( const UINT x, const UINT y, const RGBAValue color ) const
    {
        const RGBAValue cmaColor = m_CumulativeAverageImage.Value( x, y );
        return cmaColor.SquareDistance(color);
    }


    // Give back foreground / background probability, taking into account pixel validity
    __forceinline UINT ForegroundProbabilitySelective( const UINT x, const UINT y, const RGBAValue color ) const
    {
        if( !m_ValidPixels.Value( x >> 1, y >> 1 ) )
        {
            // We don't have valid data at this pixel.
            // This means we have never seen the background to build an average color.
            // The best we can do is return 0, to say 'this is foreground'
            return 0;
        }
        else
        {
            return ForegroundProbability( x, y, color );
        }
    }

    // Update the entire color buffer with the color map provided
    VOID Update( const ColorMap& newColorImage );

    // Update entire average buffer, but only where the Binary Map says so
    VOID UpdateSelected( const ColorMap& newColorImage, const BinaryMap& selectMap );

private:

    BinaryMap m_ValidPixels;
    ColorMap m_CumulativeAverageImage;
};

#endif
