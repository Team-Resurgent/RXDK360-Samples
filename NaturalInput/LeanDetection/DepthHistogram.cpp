//-----------------------------------------------------------------------------
// DepthProcessor.cpp
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------

#include <xtl.h>

#include "DepthHistogram.h"



//----------------------------------------------------------------------------------
// Flood fill stack. STL isn't used here only because the floodfill is not 100% optimal
// and it is slow in debug if we call .empty() and .push_back() etc all the time
// of course, there is an overflow check in the code
//----------------------------------------------------------------------------------
static DWORD    g_dwStackTop;
static DWORD    g_floodFillStack[ Detector::DEPTH_W * Detector::DEPTH_H + 4 ];


//----------------------------------------------------------------------------------
// Name: DepthProcessor::Reset
// Desc: Call to initialise the depth processor
//----------------------------------------------------------------------------------
void  Detector::DepthProcessor::Reset()
{
    m_average.resize( DEPTH_W * DEPTH_H );

    for( DWORD i=0; i < DEPTH_W * DEPTH_H; ++i )
    {
        m_average[ i ] = MAX_REAL_DEPTH;
    }
}


//----------------------------------------------------------------------------------
// Name: IslandFromFloodFill3d
// Desc: This is a very very basic 3d flood fill. we use both MarkedMask and Depth
//       to step the flood filling algorithm
//----------------------------------------------------------------------------------
void    Detector::DepthProcessor::IslandFromFloodFill3d( Vector< Detail::DepthSlice >& layers,
                                                         MaskVector& markedMask,
                                                         DWORD dwSeedX,
                                                         DWORD dwSeedY,
                                                         const DepthVector& depth )
{
    WORD* __restrict pMarkedMask = &markedMask[ 0 ];
    const WORD* __restrict pDepth = &depth[ 0 ];

    // already filled or invalid depth?
    if( pMarkedMask[ dwSeedX + dwSeedY * DEPTH_W ]   ||
        pDepth[ dwSeedX + dwSeedY * DEPTH_W ] >= MAX_REAL_DEPTH )
    {
        return;
    }

    PIXBeginNamedEvent( 0, "Marking layer %d", layers.size() );

    // generate a layer + an island. historically a layer could have
    // multiple islands, but it's just one now
    const WORD dwPaintId = Detail::PackLayerAndIslandIntoWord( layers.size(), 0 );
    layers.push_back( Detail::DepthSlice() );
    layers.back().m_islands.resize( 1 );

    Detail::DepthSlice& layer = layers.back();
    Detail::Island& isle = layer.m_islands[ 0 ];

    layer.m_nearDist = MAX_REAL_DEPTH;
    layer.m_farDist = MIN_REAL_DEPTH;
    layer.m_islands[ 0 ].m_dwNumPixels = 0;
    layer.m_islands[ 0 ].m_Box.InvMax( DEPTH_W, DEPTH_H );

    g_floodFillStack[ 0 ] = ( (dwSeedX << 0) | (dwSeedY << 16) );
    g_dwStackTop = 1;

    // sometimes when you are far away (2+m) and really slouching, this will not
    // detect head->neck->chest transition and will stop at the chin
    // usually though, 5 cm is good enough to make a reliable floodfill
    static const INT FLOODFILL_DEPTH_THRESHOLD = 50;

    while( g_dwStackTop )
    {
        const DWORD   xy = g_floodFillStack[ --g_dwStackTop ];
        const DWORD   x = xy & 0xffff;
        const DWORD   y = xy >> 16;
        const DWORD   dwIdx = x + y * DEPTH_W;

        // visited already?
        if( pMarkedMask[ dwIdx ] )
            continue;

        // invalid depth?
        const SHORT   d = pDepth[ dwIdx ];
        if( d >= MAX_REAL_DEPTH )
            continue;

        // paint, expand bounding box and z range
        pMarkedMask[ dwIdx ] = dwPaintId;

        ++isle.m_dwNumPixels;
        isle.m_Box.Expand( x, y );
        layer.m_nearDist = min( d, layer.m_nearDist );
        layer.m_farDist = max( d, layer.m_farDist );

        assert( 4 + g_dwStackTop < _countof( g_floodFillStack ) );

        // "recurse"

        if( x > 1 )
        {
            const DWORD dwNewIdx = x + y * DEPTH_W - 1;
            const INT d1 = pDepth[ dwNewIdx ];
            const INT delta = abs( d1 - d );

            if( delta < FLOODFILL_DEPTH_THRESHOLD   &&
                !pMarkedMask[ dwNewIdx ] )
            {
                g_floodFillStack[ g_dwStackTop++ ] = ( (x - 1) | (y << 16) );
            }
        }

        if( x < DEPTH_W - 2 )
        {
            const DWORD dwNewIdx = x + y * DEPTH_W + 1;
            const INT d1 = pDepth[ dwNewIdx ];
            const INT delta = abs( d1 - d );

            if( delta < FLOODFILL_DEPTH_THRESHOLD   &&
                !pMarkedMask[ dwNewIdx ] )
            {
                g_floodFillStack[ g_dwStackTop++ ] = ( (x + 1) | (y << 16) );
            }
        }

        if( y > 1 )
        {
            const DWORD dwNewIdx = x + (y - 1) * DEPTH_W;
            const INT d1 = pDepth[ dwNewIdx ];
            const INT delta = abs( d1 - d );

            if( delta < FLOODFILL_DEPTH_THRESHOLD   &&
                !pMarkedMask[ dwNewIdx ] )
            {
                g_floodFillStack[ g_dwStackTop++ ] = ( (x) | ((y - 1) << 16) );
            }
        }

        if( y < DEPTH_H - 2 )
        {
            const DWORD dwNewIdx = x + (y + 1) * DEPTH_W;
            const INT d1 = pDepth[ dwNewIdx ];
            const INT delta = abs( d1 - d );

            if( delta < FLOODFILL_DEPTH_THRESHOLD   &&
                !pMarkedMask[ dwNewIdx ] )
            {
                g_floodFillStack[ g_dwStackTop++ ] = ( (x) | ((y + 1) << 16) );
            }
        }
    }


    PIXEndNamedEvent();
}


//----------------------------------------------------------------------------------
// Name: DepthProcessor::BuildLayers
// Desc: We use an average value of the depth to decide if a pixel moved. If it did,
//       we flood fill in 3d from there and generate an island
//----------------------------------------------------------------------------------
BOOL Detector::DepthProcessor::BuildLayers( const DepthVector& depth,
                                            Vector< Detail::DepthSlice >& layers,
                                            MaskVector& markedMask,
                                            BOOL bFreezeFrame )
{
    PIXBeginNamedEvent( 0, "Build layers" );

    layers.clear();

    memset( &markedMask[ 0 ], 0, markedMask.size() * sizeof( markedMask[ 0 ] ) );

    static const INT    AVERAGE_OVER_N_FRAMES = 30;
    static const DWORD  NOISE_THRESHOLD = 6;

    for( DWORD i=0; i < DEPTH_W * DEPTH_H; ++i )
    {
        const DWORD dwDepth = depth[ i ];

        // allows us to debug frames
        if( !bFreezeFrame )
        {
            m_average[ i ] = (WORD)((AVERAGE_OVER_N_FRAMES * (INT)m_average[ i ] + (INT)dwDepth) / (AVERAGE_OVER_N_FRAMES + 1));
        }

        const DWORD   dwNoise = m_average[ i ] >> NOISE_THRESHOLD;
        const DWORD   dwNoiseMin = m_average[ i ] - dwNoise;

        // when we use 3d flood fill it's really enough to check for the depth change
        // in one direction as there is always noise around the edges of objects anyway
        if( dwDepth < dwNoiseMin )
        {
            IslandFromFloodFill3d( layers, markedMask, i % DEPTH_W, i / DEPTH_W, depth );
        }
    }

    PIXEndNamedEvent();

    return !layers.empty();
}
