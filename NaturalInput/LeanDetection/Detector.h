//-----------------------------------------------------------------------------
// Detector.h
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------

#pragma once

#include <vector>
#include <assert.h>


namespace Detector
{
    //----------------------------------------------------------------------------------
    // Desc: COLOR and DEPTH NUI buffer sizes and processing depth
    //----------------------------------------------------------------------------------
    static const DWORD   NUI_COLOR_W = 640;
    static const DWORD   NUI_COLOR_H = 480;
    static const DWORD   NUI_DEPTH_W = 320;
    static const DWORD   NUI_DEPTH_H = 240;

    static const DWORD   DEPTH_DOWNSAMPLE = 4;

    static const DWORD   DEPTH_W = 80;
    static const DWORD   DEPTH_H = 60;

    static const DWORD   MAX_REAL_DEPTH = 4000;
    static const DWORD   MIN_REAL_DEPTH = 800;

    //----------------------------------------------------------------------------------
    // replace it with your own vector
    //----------------------------------------------------------------------------------
    template< typename type >
    struct Vector : public std::vector< type >
    {
    };


    typedef Vector< WORD >  MaskVector;
    typedef Vector< WORD >  DepthVector;
    typedef Vector< DWORD > ColorVector;


    //----------------------------------------------------------------------------------
    // Name: Box
    // Desc: A simple 2d bounding box
    //----------------------------------------------------------------------------------
    struct Box
    {
        WORD left;
        WORD right;
        WORD top;
        WORD bottom;

        void    Zero()
        {
            left = right = top = bottom = 0;
        }

        void    InvMax( DWORD w, DWORD h )
        {
            assert( w <= 0xffff );
            assert( h <= 0xffff );

            top = static_cast< WORD >( h );
            bottom = 0;
            left = static_cast< WORD >( w );
            right = 0;
        }
        
        void    Expand( DWORD i, DWORD j )
        {
            ExpandX( i );
            ExpandY( j );
        }

        void    ExpandY( DWORD i )
        {
            assert( i <= 0xffff );

            const WORD  ii = static_cast< WORD >( i );

            if( ii < top )
                top = ii;

            if( ii > bottom )
                bottom = ii;
        }

        void    ExpandX( DWORD j )
        {
            assert( j <= 0xffff );

            const WORD  jj = static_cast< WORD >( j );

            left = min( jj, left );
            right = max( jj, right );
        }

        void    ExpandX2( DWORD x0, DWORD x1 )
        {
            assert( x0 <= 0xffff );
            assert( x1 <= 0xffff );

            const WORD  xx0 = static_cast< WORD >( x0 );
            const WORD  xx1 = static_cast< WORD >( x1 );

            left = min( xx0, left );
            right = max( xx1, right );
        }
    };


    //----------------------------------------------------------------------------------
    // Name: ResultStatus
    //----------------------------------------------------------------------------------
    enum ResultStatus
    {
        RS_TRACKING,
        RS_INVALID
    };

    // internal
    namespace Detail
    {
        //----------------------------------------------------------------------------------
        // Name: HeadStatus
        // Desc: tell you what Detector thinks about a moving blob -- useful for debugging
        //----------------------------------------------------------------------------------
        enum HeadStatus
        {
            HS_DETECTED,
            HS_BOX_TOO_SMALL,
            HS_TOO_SMALL,
            HS_TOO_BIG,
            HS_LOW_SCORE,
            HS_WRONG_ASPECT
        };

        //----------------------------------------------------------------------------------
        // Name: DetectedHead
        // Desc: Stores every bit of information we collect during head signature matching
        //----------------------------------------------------------------------------------
        struct DetectedHead
        {
            XMVECTOR    m_vWorldPointMeters;        // high res coordinates
            FLOAT       m_centreLowRes[ 4 ];        // centre of mass coordinates X and Y + Znear + Zfar

            Box     m_headBox;                      // box for the head
            Box     m_bodyBox;                      // box for the island

            FLOAT   m_fDifferenceMean;              // mean of error
            FLOAT   m_fHeadSizeAtDistanceMin;       // minimum head size we're looking for
            FLOAT   m_fHeadSizeAtDistanceMax;       // maximum head size
            FLOAT   m_fHeadSizeAtDistance;          // found head size

            WORD    m_layerIslandId;                // this encodes layer and island index
            WORD    m_endShouldersScanMin;          // top scan line for the signature search
            WORD    m_endShouldersScanMax;          // bottom scan line for the signature search
            WORD    m_endShouldersScan;             // where we found the siganture match

            WORD    m_silhouetteThickness[ DEPTH_H ];// 1d profile
            WORD    m_leftEdge[ DEPTH_H ];          // left edge of the profile
            WORD    m_rightEdge[ DEPTH_H ];         // right edge of the profile
            
            FLOAT   m_fInvMaxValue;                 // normalization coefficient for the profile

            // debugging information
            HeadStatus  m_status;                   // we store heads information for debugging
        };

        //----------------------------------------------------------------------------------
        // Name: Island
        // Desc: Stores the detected island.
        //----------------------------------------------------------------------------------
        struct Island
        {
            Box             m_Box;              // encompassing box
            DWORD           m_dwNumPixels;      // actual number of pixels in the island

            // debugging information that links island to head
            DWORD           m_dwHeadIndex;
        };

        //----------------------------------------------------------------------------------
        // Name: DepthSlice
        // Desc: This contains a single depth slice
        //----------------------------------------------------------------------------------
        struct DepthSlice
        {
            Vector< Island >    m_islands;
            FLOAT               m_headSizesAtDistances[ 2 ];

            // debugging information
            SHORT               m_nearDist;
            SHORT               m_farDist;
        };

        //----------------------------------------------------------------------------------
        // Name: IndexIntoPaintValue
        // Desc: We don't write a 0 for layer 0, we write 1 to distinguish it from empty space
        //----------------------------------------------------------------------------------
        inline
        WORD    IndexIntoPaintValue( DWORD dwIndex )
        {
            return static_cast< WORD >( dwIndex + 1 );
        }


        //----------------------------------------------------------------------------------
        // Name: PackLayerAndIslandIntoWord
        // Desc: We store two eight bit fields into one 16 bit variable and use it later as
        //       a flood fill value
        //----------------------------------------------------------------------------------
        inline
        WORD    PackLayerAndIslandIntoWord( DWORD dwLayer, DWORD dwIsland )
        {
            return static_cast< WORD >( IndexIntoPaintValue( dwLayer ) | ( IndexIntoPaintValue( dwIsland  ) << 8 ) );
        }
    }


    //----------------------------------------------------------------------------------
    // Name: LeanResult
    // Desc: High level result our functions return
    //----------------------------------------------------------------------------------
    struct LeanResult
    {
        LeanResult() :  m_dwLastFrameSeen( 0 ),
                        m_fLastScore( 0 ),
                        m_dwMatchedHead( ~0ul ),
                        m_vWorldPointMeters( XMVectorZero() ),
                        m_vScreenPoint( XMVectorZero() ),
                        m_status( RS_INVALID )
        {
            m_headBox.Zero();
            m_bodyBox.Zero();
        }

        XMVECTOR        m_vWorldPointMeters;       // centre of mass coordinates
        XMVECTOR        m_vScreenPoint;            // centre of mass coordinates
        DWORD           m_dwLastFrameSeen;         // last tick we saw this result valid
        Box             m_headBox;                 // box encompassing head
        Box             m_bodyBox;                 // box encompassing island
        FLOAT           m_fLastScore;              // score for this result
        ResultStatus    m_status;

        // this is used for debugging -- you don't need this in the structure at all
        // as it only slows it down. it's great for debugging though
        DWORD   m_dwMatchedHead;                    // ~0ul means no match
        Vector< Detail::DetectedHead >  m_heads;    // all potential heads detected
        Vector< Detail::DepthSlice >    m_layers;   // all slices/islands in the scene
    };



    //----------------------------------------------------------------------------------
    // Name: Lerp
    //----------------------------------------------------------------------------------
    template< typename type0, typename type1 >
    type0 Lerp( type0 a, type0 b, type1 t )
    {
        return a + ( b - a ) * t;
    }

    //----------------------------------------------------------------------------------
    // Name: Initialize
    //----------------------------------------------------------------------------------
    void Initialize();

    //----------------------------------------------------------------------------------
    // Name: DetectLean
    // Desc: Finds heads, then uses history to find a matching one in the found list,
    //       then detects the lean by scannign delta depth pixels around the head
    //----------------------------------------------------------------------------------
    void DetectLean(    LeanResult& result,
                        MaskVector& mask,
                        const DepthVector& depthFull,
                        const DepthVector& depth80x60,
                        BOOL bDebugMode );
}

