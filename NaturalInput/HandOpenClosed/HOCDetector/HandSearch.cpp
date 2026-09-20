//--------------------------------------------------------------------------------------
// HandSearch.cpp
//
// Given the position of the hand and the depth map it returns the hand's voxels
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "HandSearch.h"


// PC won't have XMemSet
#ifndef _XBOX
#define XMemSet memset
#endif

//--------------------------------------------------------------------------------------
// Name: Proposal
// Desc: Hand search proposed result
//--------------------------------------------------------------------------------------
struct Proposal
{
    XMVECTOR    vHand, vWrist, vElbow;
    INT         iStartX, iStartY, iStartZ;
    INT         iMinX, iMinY, iMinZ, iMaxX, iMaxY, iMaxZ;
    XMFLOAT4    vClipPlane;
    BOOL        bUseClipPlane;


    void    SetBoundingBox( FLOAT x, FLOAT y, FLOAT fHandSizeAtDistance )
    {
        iStartX = static_cast< INT >( x );
        iStartY = static_cast< INT >( y );

        // first approximation window
        iMinX = (INT)(x - fHandSizeAtDistance);
        iMaxX = (INT)(x + fHandSizeAtDistance);
        iMinY = (INT)(y - fHandSizeAtDistance);
        iMaxY = (INT)(y + fHandSizeAtDistance);

        iMinX = std::max( 0, std::min( INT( HOC_DEPTH_SIZE_X ), iMinX ) );
        iMaxX = std::max( 0, std::min( INT( HOC_DEPTH_SIZE_X ), iMaxX ) );
        iMinY = std::max( 0, std::min( INT( HOC_DEPTH_SIZE_Y ), iMinY ) );
        iMaxY = std::max( 0, std::min( INT( HOC_DEPTH_SIZE_Y ), iMaxY ) );
        iStartX = std::max( iMinX, std::min( iMaxX - 1, iStartX ) );
        iStartY = std::max( iMinY, std::min( iMaxY - 1, iStartY ) );
    }
};


//--------------------------------------------------------------------------------------
// Name: FloodFill
// Desc: Simple flood fill
//--------------------------------------------------------------------------------------
void    FloodFill( std::vector< Voxel >& voxels, const Proposal& p, const USHORT* pDepth, UINT depthStrideInShorts )
{
    // only one flood fill texture size to use
    assert( depthStrideInShorts == HOC_DEPTH_SIZE_X );

    const FLOAT   sx = HOC_DEPTH_SIZE_X;
    const FLOAT   sy = HOC_DEPTH_SIZE_Y;
    const FLOAT   fovH = FOVH_PER_PIXEL;

    static DWORD    fillStack[ 4096 ];
    static BYTE     visited[ HOC_DEPTH_SIZE_X * HOC_DEPTH_SIZE_Y ];

    INT x = p.iStartX;
    INT y = p.iStartY;

    // can the starting point be outside the proposal's boundary?
    if( x < p.iMinX || x >= p.iMaxX || y < p.iMinY || y >= p.iMaxY )
    {
        voxels.clear();
        return;
    }

    UINT    topStack = 1;
    fillStack[ 0 ] = x | (y << 16);

    XMemSet( visited, 0, sizeof( visited ) );

    // NOTE: some data coming from ST is wrongly pointing at the middle of the forearm
    // saying it's a hand. while it's still possible to find the hand, cases like that
    // are impossible to detect without extra work.
    //
    // one way to solve a bad hand position, as long as it's on the depth of the skeleton,
    // is to first flood fill in the direction of the forearm, then, having found the edge
    // (presumably the farthest edge of the hand), set clipping plane 20 cm from that point
    // (multiplied by the scale) back along the same direction, and perform the clipping
    // of voxels

    while( topStack )
    {
        if( topStack > _countof( fillStack ) - 1 )
        {
            voxels.clear();
            return;
        }

        --topStack;
        x = fillStack[ topStack ] & 0xffff;
        y = fillStack[ topStack ] >> 16;

        if( visited[ x + y * depthStrideInShorts ] )
            continue;

        visited[ x + y * depthStrideInShorts ] = 1;

        const WORD d = pDepth[ y * depthStrideInShorts + x ] >> 3;

        if( !d || d < p.iMinZ || d > p.iMaxZ )
            continue;

        if( p.bUseClipPlane )
        {
            const FLOAT fX = static_cast< FLOAT >( x );
            const FLOAT fY = static_cast< FLOAT >( y );
            const FLOAT zz = static_cast< FLOAT >( d ) / 1000.f;

            const FLOAT xx = (fX - sx * 0.5f) * zz * fovH;
            const FLOAT yy = (sy * 0.5f - fY) * zz * fovH;

            const FLOAT dd = p.vClipPlane.x * xx + p.vClipPlane.y * yy + p.vClipPlane.z * zz + p.vClipPlane.w;
            if( dd < 0 )
                continue;
        }

        Voxel v;
        v.x = static_cast< USHORT >( x );
        v.y = static_cast< USHORT >( y );
        v.z = static_cast< USHORT >( d );

        voxels.push_back( v );

        if( x + 1 < p.iMaxX )
        {
            fillStack[ topStack++ ] = (x + 1) | (y << 16);
        }

        if( x - 1 > p.iMinX )
        {
            fillStack[ topStack++ ] = (x - 1) | (y << 16);
        }

        if( y + 1 < p.iMaxY )
        {
            fillStack[ topStack++ ] = (x) | ((y + 1) << 16);
        }

        if( y - 1 > p.iMinY )
        {
            fillStack[ topStack++ ] = (x) | ((y - 1) << 16);
        }
    }
}


const WCHAR* pDebugText = NULL;

//----------------------------------------------------------------------------------
// Name: FindHand
// Desc: Clean input data is very important, we just do a simple floodfill here but
//       other things can be done
//----------------------------------------------------------------------------------
void    FindHand( std::vector< Voxel >& voxels,
                  FLOAT& fHandSizeAtDistance,
                  FLOAT fPlayerSize,
                  BOOL bElbowTracked, BOOL bWristTracked,
                  const USHORT* pDepthMap,
                  XMVECTOR vHand, XMVECTOR vWrist, XMVECTOR vElbow )
{
#ifdef _XBOX
    PIXBeginNamedEvent( 0, "HOC Find Hand" );
#endif

    // average size is about 200-300 voxels
    voxels.reserve( 256 );

    pDebugText = NULL;

    // meters, half size because its meaning is radius, rescale to skeleton size
    const FLOAT fHalfSize = (HAND_SIZE_FOR_NORMALIZED_SKELETON / 2.f) * (fPlayerSize / NORMALIZED_SKELETON_SIZE);
    const FLOAT fZRadius = fHalfSize * DEPTH_RANGE_MULTIPLIER * 1000;

    Proposal    initialProposal = { 0 };
    {
        // clip plane

        // create the cutoff plane which is a plane at wrist looking down the direction of the elbow
        // we need to aim to cut off as much of the wrist and forehand as possible
        const XMVECTOR vPalm = XMVectorAdd( vWrist, XMVectorSubtract( vHand, vWrist ) * WRIST_CUTOFF_RATIO );
        const XMVECTOR vPlane = XMPlaneFromPointNormal( vPalm, XMVector3Normalize( XMVectorSubtract( vWrist, vElbow ) ) );

        XMStoreFloat4( &initialProposal.vClipPlane, vPlane );

        initialProposal.bUseClipPlane = (bElbowTracked & bWristTracked);

        // z range
        // bone z is usually submerged in the depth
        XMFLOAT3 vScreen;
        ProjectWorldToScreen( &vScreen, XMVectorGetX( vHand ), XMVectorGetY( vHand ), XMVectorGetZ( vHand ) );

        initialProposal.iStartZ = (INT)vScreen.z;
        initialProposal.iMinZ = (INT)((vScreen.z - fZRadius));
        initialProposal.iMaxZ = (INT)((vScreen.z + fZRadius));

        // xy range
        fHandSizeAtDistance = ProjectWorldDistanceToScreen( fHalfSize, vScreen.z );

        // starting point
        initialProposal.SetBoundingBox( vScreen.x * FLOAT( HOC_DEPTH_SIZE_X ),
                                        vScreen.y * FLOAT( HOC_DEPTH_SIZE_Y ),
                                        fHandSizeAtDistance );
    }

    Proposal    refinedProposal = initialProposal;

    // get the point on depth instead of relying on the ST and refresh z boundaries
    refinedProposal.iStartZ = pDepthMap[ initialProposal.iStartY * HOC_DEPTH_SIZE_X + initialProposal.iStartX ] >> 3;
    refinedProposal.iMinZ = static_cast< INT >( refinedProposal.iStartZ - fZRadius );
    refinedProposal.iMaxZ = static_cast< INT >( refinedProposal.iStartZ + fZRadius );

    ADD_DEBUG_QUAD_MIN_MAX( initialProposal.iMinX, initialProposal.iMinY, initialProposal.iMaxX, initialProposal.iMaxY, 0xff80ff80 );

    FloodFill( voxels, refinedProposal, pDepthMap, HOC_DEPTH_SIZE_X );

#ifdef _XBOX
    PIXEndNamedEvent();
#endif
}
