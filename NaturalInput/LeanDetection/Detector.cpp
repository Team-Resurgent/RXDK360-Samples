//-----------------------------------------------------------------------------
// Detector.cpp
//
// Signature detection using raw depth stream
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------

#include <xtl.h>
#include <NUIAPI.h>

#include <assert.h>

#include "Detector.h"
#include "DepthHistogram.h"


// This revision contains a lot of debugging helpers which may take a lot of memory
// and also cost some performance. However, it allows you to inspect every island.

// Enable this to make Detector output information about the process
#define  VERBOSE_DEBUG_OUTPUT   0


namespace Detector { namespace Detail {

    DWORD       g_printIndent = 0;

#if VERBOSE_DEBUG_OUTPUT
    void    Print( const char* fmt, ... )
    {
        char tmp[ 1024 ];

        for( DWORD i=0; i < g_printIndent; ++i )
        {
            tmp[ i ] = '\t';
        }

        va_list vl;
        va_start( vl, fmt );

        _vsnprintf_s( &tmp[ g_printIndent ], ARRAYSIZE( tmp ) - g_printIndent, _TRUNCATE, fmt, vl );

        va_end( vl );

        OutputDebugStringA( tmp );
    }
#else
    void Print( const char* fmt, ... )
    {
    }
#endif


    //----------------------------------------------------------------------------------
    // Name: g_matchSignature
    // Desc: This is the 1d signature we'll try to find in the image.
    //       We trace the contours of image slices found in the depth stream
    //       and try matching it to this contour. This is basically a human head and
    //       a bit of shoulders area.
    //       Copy paste the data into Excel and plot a graph to see the signature we're
    //       trying to find
    //----------------------------------------------------------------------------------
    static const FLOAT g_matchSignature[] =
    {
        // 0 is implicit here as we always start from the top edge of the box
        0.1,
        0.5,
        0.6,
        0.65,
        0.65,
        0.65,
        0.65,
        0.5,
        1,
    };

    // we only search for heads between 20 and 30 cm in size
    static const FLOAT    HEAD_SIZE_MIN_METERS = 0.20f;
    static const FLOAT    HEAD_SIZE_MAX_METERS = 0.3f;

    static const FLOAT    TAN_VERT_CAMERA_FOV        = tanf( XMConvertToRadians( 0.5f * NUI_CAMERA_DEPTH_NOMINAL_VERTICAL_FOV ) );
    static const FLOAT    TAN_HORZ_CAMERA_FOV        = tanf( XMConvertToRadians( 0.5f * NUI_CAMERA_DEPTH_NOMINAL_HORIZONTAL_FOV ) );
    
    static const FLOAT    FOVH_PER_PIXEL_80x60_INV   = 0.5f * 80.f / TAN_HORZ_CAMERA_FOV;
    static const FLOAT    FOVV_PER_PIXEL_80x60_INV   = 0.5f * 60.f / TAN_VERT_CAMERA_FOV;

    static const FLOAT    FOVH_PER_PIXEL_320x240_INV = 0.5f * 320.f / TAN_HORZ_CAMERA_FOV;
    static const FLOAT    FOVV_PER_PIXEL_320x240_INV = 0.5f * 240.f / TAN_VERT_CAMERA_FOV;

    static const FLOAT    FOVH_PER_PIXEL_1x1_INV     = 0.5f / TAN_HORZ_CAMERA_FOV;
    static const FLOAT    FOVV_PER_PIXEL_1x1_INV     = 0.5f / TAN_VERT_CAMERA_FOV;

    static const FLOAT    FOVH_PER_PIXEL_80x60       = TAN_HORZ_CAMERA_FOV / (0.5f * 80.f);
    static const FLOAT    FOVV_PER_PIXEL_80x60       = TAN_VERT_CAMERA_FOV / (0.5f * 60.f);

    static const FLOAT    FOVH_PER_PIXEL_320x240     = TAN_HORZ_CAMERA_FOV / (0.5f * 320.f);
    static const FLOAT    FOVV_PER_PIXEL_320x240     = TAN_VERT_CAMERA_FOV / (0.5f * 240.f);

    // 10% error -- we reject errors higher than this
    static const FLOAT    REJECT_THRESHOLD = 0.1f;

    // minimum island area we consider
    static const DWORD    MIN_ISLAND_AREA = (16 * 16) / (DEPTH_DOWNSAMPLE * DEPTH_DOWNSAMPLE);


    //----------------------------------------------------------------------------------
    DepthProcessor           g_depthProcessor;


    //----------------------------------------------------------------------------------
    inline
    FLOAT   ProjectWorldDistanceToScreen80x60( FLOAT fSize, FLOAT fDistMeters )
    {
        return fSize * FOVH_PER_PIXEL_80x60_INV / fDistMeters;
    }

    //----------------------------------------------------------------------------------
    inline
    FLOAT   ProjectWorldDistanceToScreen( FLOAT fSize, FLOAT fDistMeters )
    {
        return fSize * FOVH_PER_PIXEL_320x240_INV / fDistMeters;
    }

    //----------------------------------------------------------------------------------
    inline
    void    TransformScreen80x60ToWorld( XMVECTOR* pWorld, FLOAT fX, FLOAT fY, FLOAT fZ )
    {
        pWorld->x = (fX - DEPTH_W * 0.5f) * fZ * FOVH_PER_PIXEL_80x60;
        pWorld->y = (DEPTH_H * 0.5f - fY) * fZ * FOVV_PER_PIXEL_80x60;
        pWorld->z = fZ;
    }

    //----------------------------------------------------------------------------------
    inline
    void    ProjectWorldToScreen( XMVECTOR* pScreen, XMVECTOR vWorld )
    {
        pScreen->x = 0.5f + vWorld.x * FOVH_PER_PIXEL_1x1_INV / vWorld.z;
        pScreen->y = 0.5f - vWorld.y * FOVV_PER_PIXEL_1x1_INV / vWorld.z;
        pScreen->z = vWorld.z;
    }

    //----------------------------------------------------------------------------------
    inline
    void    TransformScreen320x240ToWorld( XMVECTOR* pWorld, FLOAT fX, FLOAT fY, FLOAT fZ )
    {
        pWorld->x = (fX - NUI_DEPTH_W * 0.5f) * fZ * FOVH_PER_PIXEL_320x240;
        pWorld->y = (NUI_DEPTH_H * 0.5f - fY) * fZ * FOVV_PER_PIXEL_320x240;
        pWorld->z = fZ;
    }

    //----------------------------------------------------------------------------------
    // Name: FindHeadInMaskSilhouette
    // Desc: Given a masked off area of the image try to find the head signature there
    //       using silhouette matching
    //       Note that this is not going to be robust on people wearing hats, having
    //       large hair or otherwise not matching the given profile. Waving hands may
    //       also break it.
    //       alternative approach would be:
    //       if depth resolution is 80x60, it's possible to scan every pixel on the image to try to
    //       find the head by sampling points around each sample. we just need to check that there
    //       is empty space above and around the head and that shoulders are present
    //----------------------------------------------------------------------------------
    HeadStatus FindHeadInMaskSilhouette(    DetectedHead& result,
                                            FLOAT fHeadScreenSizeMin,
                                            FLOAT fHeadScreenSizeMax,
                                            const MaskVector& mask,
                                            DWORD dwMaskId,
                                            const Box& islandBox )
    {
        ZeroMemory( &result, sizeof( result ) );

        result.m_bodyBox = islandBox;

        Print( "box: %d-%d %d-%d\n", result.m_bodyBox.left, result.m_bodyBox.right, result.m_bodyBox.top,
               result.m_bodyBox.bottom );

        // reject if box is too small
        const DWORD dwHeight = result.m_bodyBox.bottom - result.m_bodyBox.top;
        const DWORD dwArea = dwHeight * ( result.m_bodyBox.right - result.m_bodyBox.left );
        if( dwHeight < ARRAYSIZE( g_matchSignature ) ||
            dwArea < MIN_ISLAND_AREA )
        {
            Print( "reject point -- box is too small %d %d\n", dwHeight, dwArea );
            return HS_BOX_TOO_SMALL;
        }

        // find the left edge and the right edge of the silhouette, also find the slice thickness
        // so we follow left and right edges of the silhouette (mask) and collapse it against
        // the left edge to construct a 1d signature profile of it
        for( DWORD i = result.m_bodyBox.top; i < result.m_bodyBox.bottom; ++i )
        {
            WORD j, k;

            const DWORD dwBase = i * DEPTH_W;

            for( j = result.m_bodyBox.left; j < result.m_bodyBox.right; ++j )
            {
                if( dwMaskId == mask[ j + dwBase ] )
                    break;
            }

            for( k = result.m_bodyBox.right; k >= result.m_bodyBox.left; --k )
            {
                if( dwMaskId == mask[ k + dwBase ] )
                    break;
            }

            result.m_leftEdge[ i ] = j;
            result.m_rightEdge[ i ] = k;
            result.m_silhouetteThickness[ i ] = k - j;
        }

        // here we find what head sizes we should try to score
        const DWORD dwEndSearchMin = min( result.m_bodyBox.bottom, static_cast< WORD >( result.m_bodyBox.top + fHeadScreenSizeMin ) );
        const DWORD dwEndSearchMax = min( result.m_bodyBox.bottom, static_cast< WORD >( result.m_bodyBox.top + fHeadScreenSizeMax + 0.5f ) );
        const FLOAT fHeadRescaler = ( fHeadScreenSizeMax - fHeadScreenSizeMin ) / FLOAT( dwEndSearchMax - dwEndSearchMin );

        // the signature is normalised so should be the profile. to normalise the profile we need a maximum value so
        // instead of looking for a max on every pass we precalculate the max value here and cache it
        FLOAT maxSilhouetteThicknessValues[ DEPTH_H ];
        FLOAT slicesThicknessInFloat[ DEPTH_H ];
        {
            FLOAT fMaxBase = 0;
            for( DWORD i = result.m_bodyBox.top; i < dwEndSearchMin; ++i )
            {
                const FLOAT v = result.m_silhouetteThickness[ i ];
                fMaxBase = __fself( fMaxBase - v, fMaxBase, v );    // max
                slicesThicknessInFloat[ i ] = v;
            }

            for( DWORD i = dwEndSearchMin; i < dwEndSearchMax; ++i )
            {
                const FLOAT v = result.m_silhouetteThickness[ i ];
                fMaxBase = __fself( fMaxBase - v, fMaxBase, v );    // max
                maxSilhouetteThicknessValues[ i ] = fMaxBase;
                slicesThicknessInFloat[ i ] = v;
            }
        }

        // these aren't going to change so store them in the result now
        result.m_endShouldersScanMin = static_cast< WORD >( dwEndSearchMin );
        result.m_endShouldersScanMax = static_cast< WORD >( dwEndSearchMax );
        result.m_fHeadSizeAtDistanceMin = fHeadScreenSizeMin;
        result.m_fHeadSizeAtDistanceMax = fHeadScreenSizeMax;

        // this is to see if the loop failed because aspect ratio was wrong
        BOOL    bAspectFailed = FALSE;

        // here we actually test every possible size of head by testing the profile
        // against the given signature and score each attempt
        FLOAT fBestScore = FLT_MAX;
        for( DWORD dwEndSearch = dwEndSearchMin; dwEndSearch < dwEndSearchMax; ++dwEndSearch )
        {
            const FLOAT fHeadSize = fHeadScreenSizeMin + fHeadRescaler * FLOAT( dwEndSearch - dwEndSearchMin );

            // retrieve the cached max value here -- we cached it based on where we end our search
            const FLOAT fMaxValue = maxSilhouetteThicknessValues[ dwEndSearch ];

            // checking aspect ratio is important to reject objects that have the right outlines
            // but are too thick or thin. real humans' heads have aspect ratios close to 1
            // we just break out of the loop here because the silhouette's thickness can't become
            // smaller as we go down it
            const FLOAT fAspect = fMaxValue / fHeadSize;
            if( fAspect < 0.5f  ||
                fAspect > 2 )
            {
                bAspectFailed = TRUE;
                break;
            }

            FLOAT fScore = 0;

            const FLOAT fInvMaxValue = 1.f / fMaxValue;
            const FLOAT fInvSx = 1.f / ( dwEndSearch - result.m_headBox.top );      // 1 over the num steps
            const FLOAT fStepX = ( ARRAYSIZE( g_matchSignature ) - 1 ) * fInvSx;

            FLOAT fCurX = 0;

            // scan from the top of the box to the end of our search
            for( DWORD j = result.m_bodyBox.top; j < dwEndSearch; ++j )
            {
                // normalise the "slice" value to 1 -- this is "X" displacement
                const FLOAT fValue = slicesThicknessInFloat[ j ] * fInvMaxValue;

                // find the value in the profile
                const DWORD dwIdx0 = static_cast< DWORD >( fCurX );
                const DWORD dwIdx1 = ( dwIdx0 + 1 ) >= ARRAYSIZE( g_matchSignature ) ? ( ARRAYSIZE( g_matchSignature ) - 1 ) : ( dwIdx0 + 1 );
                const FLOAT fFrac = fCurX - floorf( fCurX );
                const FLOAT fProfileAtX = Lerp( g_matchSignature[ dwIdx0 ], g_matchSignature[ dwIdx1 ], fFrac );

                // get the normalised difference
                const FLOAT fDiff = fValue - fProfileAtX;
                const FLOAT fDiffAbs = fabsf( fDiff );

                // accumulate score
                fScore += fDiffAbs;
                fCurX += fStepX;
            }

            fScore *= fInvSx;

            // store the best result
            if( fScore < fBestScore )
            {
                fBestScore = fScore;
                result.m_fDifferenceMean = fScore;
                result.m_endShouldersScan = static_cast< WORD >( dwEndSearch );
                result.m_fHeadSizeAtDistance = fHeadSize;
                result.m_fInvMaxValue = fInvMaxValue;
            }
        }

        // we reject heads that too small, too large or the scores that are too big
        const   FLOAT fFoundHeadSize = static_cast< FLOAT >( result.m_endShouldersScan - result.m_bodyBox.top );
        if( fFoundHeadSize > result.m_fHeadSizeAtDistanceMax ||
            fFoundHeadSize < result.m_fHeadSizeAtDistanceMin ||
            result.m_fDifferenceMean > REJECT_THRESHOLD )
        {
            Print( "reject point head = %f (check = %f/%f, search %d), mean = %f\n",
                   fFoundHeadSize,
                   result.m_fHeadSizeAtDistanceMin,
                   result.m_fHeadSizeAtDistanceMax,
                   result.m_endShouldersScan - result.m_bodyBox.top,
                   result.m_fDifferenceMean );

            // wrong aspect above will cause the search loop to stop and so it may end up here
            if( bAspectFailed )
                return HS_WRONG_ASPECT;

            if( fFoundHeadSize < result.m_fHeadSizeAtDistanceMin )
                return HS_TOO_SMALL;

            if( fFoundHeadSize > result.m_fHeadSizeAtDistanceMax )
                return HS_TOO_BIG;

            return HS_LOW_SCORE;
        }

        // bound head by the body box from the top, neck line from the bottom and the profile's extents on the left and on the right
        result.m_headBox = result.m_bodyBox;
        result.m_headBox.left = DEPTH_W;
        result.m_headBox.right = 0;
        result.m_headBox.bottom = result.m_endShouldersScan;
        for( DWORD i = result.m_bodyBox.top; i < result.m_endShouldersScan; ++i )
        {
            result.m_headBox.left  = min( result.m_headBox.left, result.m_leftEdge[ i ] );
            result.m_headBox.right = max( result.m_headBox.right, result.m_rightEdge[ i ] );
        }

        return HS_DETECTED;
    }

    //----------------------------------------------------------------------------------
    // Name: FindCentreOfMasses
    // Desc: Scan our mask and depth to find average x, y, z min and z max values
    //----------------------------------------------------------------------------------
    void    FindCentreOfMasses( DetectedHead& result, const MaskVector& mask, const DepthVector& depth )
    {
        const DWORD dwMaskId = result.m_layerIslandId;

        // if we accept a full 320x240 image the maximum value here is
        // 12249600 so it fits into a dword
        DWORD   dwX = 0;
        DWORD   dwY = 0;
        DWORD   dwMinZ = ~0ul;
        DWORD   dwMaxZ = 0;
        DWORD   dwWeight = 0;

        for( DWORD i=result.m_headBox.top; i < result.m_headBox.bottom; ++i )
        {
            const DWORD dwBase = i * DEPTH_W;

            for( DWORD j=result.m_headBox.left; j < result.m_headBox.right; ++j )
            {
                if( dwMaskId == mask[ dwBase + j ] )
                {
                    dwX += j;
                    dwY += i;

                    ++dwWeight;

                    const WORD    d = depth[ dwBase + j ];
                    if( d < dwMinZ )
                        dwMinZ = d;
                    if( d > dwMaxZ )
                        dwMaxZ = d;
                }
            }
        }

        if( dwWeight )
        {
            const FLOAT fWeightInv = 1.f / static_cast< FLOAT >( dwWeight );
            result.m_centreLowRes[ 0 ] = static_cast< FLOAT >( dwX ) * fWeightInv;
            result.m_centreLowRes[ 1 ] = static_cast< FLOAT >( dwY ) * fWeightInv;
            result.m_centreLowRes[ 2 ] = static_cast< FLOAT >( dwMinZ );     // mm
            result.m_centreLowRes[ 3 ] = static_cast< FLOAT >( dwMaxZ );
        }
    }


    //----------------------------------------------------------------------------------
    // Name: FindHeads
    // Desc: Given the depth we generate the depth slices, flood fill them to find islands,
    //       then check each island against the 1d signature and return all possible heads found
    //----------------------------------------------------------------------------------
    void FindHeads( Vector< DetectedHead >& res,
                    Vector< DepthSlice >& layers,
                    MaskVector& markedMask,
                    const DepthVector& depth,
                    BOOL bDebugMode )
    {
        // builds islands. the quality of islands directly affects robustness of detection
        if( !g_depthProcessor.BuildLayers( depth, layers, markedMask, bDebugMode ) )
            return;

        g_printIndent = 1;

        PIXBeginNamedEvent( 0, "Searching for signature" );
        const DWORD dwNumLayers = layers.size();
        for( DWORD i = 0; i < dwNumLayers; ++i )
        {
            PIXBeginNamedEvent( 0, "Detect signature %d", i );

            // get the max head size at this distance
            layers[ i ].m_headSizesAtDistances[ 0 ] = ProjectWorldDistanceToScreen80x60( HEAD_SIZE_MIN_METERS, static_cast< FLOAT >( layers[ i ].m_farDist ) / 1000.f );
            layers[ i ].m_headSizesAtDistances[ 1 ] = ProjectWorldDistanceToScreen80x60( HEAD_SIZE_MAX_METERS, static_cast< FLOAT >( layers[ i ].m_nearDist ) / 1000.f );

            const DWORD dwNumIslands = layers[ i ].m_islands.size();
            for( DWORD j = 0; j < dwNumIslands; ++j )
            {
                // layer + island paint value
                const WORD id = PackLayerAndIslandIntoWord( i, j );

                --g_printIndent;
                Print( "layer: %d, island: %d\n", i, j );
                ++g_printIndent;

                // note that we store all heads for debugging purposes so they match the islands,
                // we really shouldn't be storing blobs that aren't heads because it wastes a lot of memory
                // but we do it in the sample for demonstration purposes
                layers[ i ].m_islands[ j ].m_dwHeadIndex = res.size();
                res.push_back( DetectedHead() );

                res.back().m_status = FindHeadInMaskSilhouette( res.back(),
                                                                layers[ i ].m_headSizesAtDistances[ 0 ],
                                                                layers[ i ].m_headSizesAtDistances[ 1 ],
                                                                markedMask,
                                                                id,
                                                                layers[ i ].m_islands[ j ].m_Box );
                res.back().m_layerIslandId = id;

                if( HS_DETECTED != res.back().m_status )
                {
                    Print( "rejected\n" );
                }
                else
                {
                    Print( "accepted mean %f search height %d\n",
                           res.back().m_fDifferenceMean,
                           res.back().m_endShouldersScan - res.back().m_bodyBox.top );
                }
            }
            PIXEndNamedEvent();
        }
        PIXEndNamedEvent();
    }

    //----------------------------------------------------------------------------------
    // Name: RefineAverage
    // Desc: project world to depth and find average values of x, y and z in the given
    //       box
    //----------------------------------------------------------------------------------
    void    RefineAverage( XMVECTOR* pDest, XMVECTOR vOrg, const DepthVector& depthFull, FLOAT fBoxSizeX, FLOAT fBoxSizeY, FLOAT fBoxSizeZ, INT minNumSamples )
    {
        const FLOAT fScreenSizeX = ProjectWorldDistanceToScreen( fBoxSizeX, vOrg.z );
        const FLOAT fScreenSizeY = ProjectWorldDistanceToScreen( fBoxSizeY, vOrg.z );

        XMVECTOR    vScreen;
        ProjectWorldToScreen( &vScreen, vOrg );

        INT minX = (INT)(vScreen.x * (FLOAT)(NUI_DEPTH_W) - fScreenSizeX);
        INT maxX = (INT)(vScreen.x * (FLOAT)(NUI_DEPTH_W) + fScreenSizeX);
        INT minY = (INT)(vScreen.y * (FLOAT)(NUI_DEPTH_H) - fScreenSizeY);
        INT maxY = (INT)(vScreen.y * (FLOAT)(NUI_DEPTH_H) + fScreenSizeY);
        INT minZ = (INT)((vOrg.z - fBoxSizeZ) * 1000.f);
        INT maxZ = (INT)((vOrg.z + fBoxSizeZ) * 1000.f);

        minX = max( 0, min( NUI_DEPTH_W, minX ) );
        maxX = max( 0, min( NUI_DEPTH_W, maxX ) );
        minY = max( 0, min( NUI_DEPTH_H, minY ) );
        maxY = max( 0, min( NUI_DEPTH_H, maxY ) );

        INT avgX = 0;
        INT avgY = 0;
        INT avgZ = 0;
        INT count = 0;

        for( INT i=minY; i < maxY; ++i )
        {
            const DWORD dwBase = i * NUI_DEPTH_W;

            for( INT j=minX; j < maxX; ++j )
            {
                const WORD d = depthFull[ dwBase + j ];

                if( d > minZ && d < maxZ )
                {
                    avgX += j;
                    avgY += i;
                    avgZ += d;

                    ++count;
                }
            }
        }

        if( count < minNumSamples )
        {
            *pDest = vOrg;
            return;
        }

        const FLOAT fInvCount = 1.f / (FLOAT)count;
        const FLOAT fAvgX = (FLOAT)avgX * fInvCount;
        const FLOAT fAvgY = (FLOAT)avgY * fInvCount;
        const FLOAT fAvgZ = (FLOAT)avgZ * (fInvCount / 1000.f);

        TransformScreen320x240ToWorld( pDest, fAvgX, fAvgY, fAvgZ );
    }

    
    //----------------------------------------------------------------------------------
    // Name: MatchBlobsToHeads
    // Desc: Tries to find a blob near where the previously detected head was
    //----------------------------------------------------------------------------------
    BOOL MatchBlobsToHeads( LeanResult& result )
    {
        FLOAT fNearestDistance = HEAD_SIZE_MAX_METERS;
        for( DWORD i = 0; i < result.m_heads.size(); ++i )
        {
            const DetectedHead& head = result.m_heads[ i ];

            if( HS_DETECTED != head.m_status )
                continue;

            const FLOAT fDist = XMVector3Length( result.m_vWorldPointMeters - head.m_vWorldPointMeters ).x;
            if( fDist < fNearestDistance )
            {
                result.m_dwMatchedHead = i;
                fNearestDistance = fDist;
            }
        }

        Print( "nearest = %f\n", fNearestDistance );

        return result.m_dwMatchedHead != ~0ul;
    }

    //----------------------------------------------------------------------------------
    // Name: ReacquireHead
    // Desc: Just picks the winner blob
    //----------------------------------------------------------------------------------
    BOOL ReacquireHead( LeanResult& result )
    {
        Print( "reacquiring\n" );

        FLOAT fBestScore = FLT_MAX;
        for( DWORD i = 0; i < result.m_heads.size(); ++i )
        {
            if( HS_DETECTED != result.m_heads[ i ].m_status )
                continue;

            if( result.m_heads[ i ].m_fDifferenceMean < fBestScore )
            {
                result.m_dwMatchedHead = i;
                fBestScore = result.m_heads[ i ].m_fDifferenceMean;
            }
        }

        return result.m_dwMatchedHead != ~0ul;
    }
} }     // Detector/Detail



//----------------------------------------------------------------------------------
// Name: Initialize
//----------------------------------------------------------------------------------
void Detector::Initialize()
{
    Detail::g_depthProcessor.Reset();
}


//----------------------------------------------------------------------------------
// Name: DetectLean
// Desc: Finds heads, then uses history to find a matching one in the found list,
//       then detects the lean by scanning delta depth pixels around the head
//----------------------------------------------------------------------------------
void Detector::DetectLean(  LeanResult& result,
                            MaskVector& markedMask,
                            const DepthVector& depthFull,
                            const DepthVector& depth80x60,
                            BOOL bDebugMode )
{
    using namespace Detail;

    // find all moving blobs that look like heads
    result.m_heads.clear();
    result.m_layers.clear();
    FindHeads( result.m_heads, result.m_layers, markedMask, depth80x60, bDebugMode );

    // debug output
    Print( "numResults = %d\n", result.m_heads.size() );

    // find COMs, refine blobs
    for( DWORD i=0; i < result.m_heads.size(); ++i )
    {
        DetectedHead& head = result.m_heads[ i ];

        if( HS_DETECTED != head.m_status )
            continue;

        // find the centre of masses in the low res buffer based on our mask
        FindCentreOfMasses( head, markedMask, depth80x60 );

        // 80x60 to world
        TransformScreen80x60ToWorld( &head.m_vWorldPointMeters, head.m_centreLowRes[ 0 ], head.m_centreLowRes[ 1 ], head.m_centreLowRes[ 2 ] / 1000.f );

        // refine the results using high res depth buffer
        // place an imaginary sphere at the detected world position and get its centre of masses
        RefineAverage( &head.m_vWorldPointMeters, head.m_vWorldPointMeters, depthFull, HEAD_SIZE_MIN_METERS / 2, HEAD_SIZE_MIN_METERS / 2, HEAD_SIZE_MIN_METERS / 2, 5 );
    }

    // we will prefer the previous winner if it hasn't been seen for 2 seconds
    // after that we'll abandon it, and will move on to the next winner
    // this is useful for eliminating noise from false positives

    const DWORD dwBecomeStale = 2000;
    const DWORD dwCurTick = GetTickCount();

    BOOL    bFoundBlob = FALSE;
    result.m_dwMatchedHead = ~0ul;

    if( result.m_dwLastFrameSeen > dwCurTick - dwBecomeStale )
    {
        if( MatchBlobsToHeads( result ) )
        {
            result.m_dwLastFrameSeen = dwCurTick;
            bFoundBlob = TRUE;
        }
    } else
    {
        if( ReacquireHead( result ) )
        {
            result.m_dwLastFrameSeen = dwCurTick;
        }
    }

    // nothing found, nothing to acquire, total fail, use dead reckoning outside
    if( ~0ul == result.m_dwMatchedHead )
    {
        result.m_status = RS_INVALID;
        return;
    }

    // update the results structure
    const DetectedHead& head = result.m_heads[ result.m_dwMatchedHead ];

    if( bFoundBlob )
    {
        // noise is inevitable even with our high res averaging, so use a spring
        const XMVECTOR  vDelta = head.m_vWorldPointMeters - result.m_vWorldPointMeters;
        const FLOAT     fSpringForce = min( 1, XMVector3Length( vDelta ).x / HEAD_SIZE_MAX_METERS );
        result.m_vWorldPointMeters = result.m_vWorldPointMeters + vDelta * fSpringForce;
    } else
    {
        // a totally new, previously unseen, blob is found
        result.m_vWorldPointMeters = head.m_vWorldPointMeters;
    }

    result.m_fLastScore = head.m_fDifferenceMean;
    result.m_bodyBox = head.m_bodyBox;
    result.m_headBox = head.m_headBox;
    ProjectWorldToScreen( &result.m_vScreenPoint, result.m_vWorldPointMeters );
    result.m_status = RS_TRACKING;
}


