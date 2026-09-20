//--------------------------------------------------------------------------------------
// HOCDetector.cpp
//
// Hand open/closed detector. Shared between the trainer and runtime
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------



#include "HOCDetectorInternal.h"
#include "HandSearch.h"


// PC won't have XMemSet
#ifndef _XBOX
#define XMemSet memset
#endif


// debug draw
IHOCDetectorDebugDraw*   HOCDetector::ms_debugDraw;


//--------------------------------------------------------------------------------------
// Name: Load
// Desc: Loads detector data
//--------------------------------------------------------------------------------------
HRESULT    HOCDetector::Load( FILE* pFile )
{
    UINT    uFileVersion;
    if( FAILED( ReadUInt( &uFileVersion, pFile ) ) )
        return E_FAIL;

    if( uFileVersion == HOC_FILE_VERSION )
    {
        // Read classifiers
        if( FAILED( m_rootClassifier.Read( pFile ) ) )
            return E_FAIL;

        // read ensemble
        if( FAILED( m_ensemble.Read( pFile ) ) )
            return E_FAIL;
    } else if( uFileVersion == HOC_FILE_PREV_VERSION )
    {
        // Read classifiers
        if( FAILED( m_rootClassifier.ReadPrev( pFile ) ) )
            return E_FAIL;

        // read ensemble
        if( FAILED( m_ensemble.Read( pFile ) ) )
            return E_FAIL;
    } else
    {
        printf( "failed to load classifier -- wrong HOC_FILE_VERSION\n" );
        return E_FAIL;
    }
    return S_OK;
}

//--------------------------------------------------------------------------------------
// Name: Load
// Desc: Loads detector data
//--------------------------------------------------------------------------------------
HRESULT    HOCDetector::Load( const CHAR* szFileName )
{
    FILE* pFile = NULL;
    fopen_s( &pFile, szFileName, "rb" );
    if ( !pFile )
        return E_FAIL;

    HRESULT hr = Load( pFile );

    fclose( pFile );

    return hr;
}


//--------------------------------------------------------------------------------------
// Name: Save
// Desc: Saves trainer data
//--------------------------------------------------------------------------------------
HRESULT HOCDetector::Save( FILE *pFile ) const
{
    if( FAILED( WriteUInt( &HOC_FILE_VERSION, pFile ) ) )
        return E_FAIL;

    // write strong classifier
    if( FAILED( m_rootClassifier.Write( pFile ) ) )
        return E_FAIL;

    // write ensemble
    if( FAILED( m_ensemble.Write( pFile ) ) )
        return E_FAIL;

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Name: Save
// Desc: Saves trainer data
//--------------------------------------------------------------------------------------
HRESULT HOCDetector::Save( const CHAR* szFileName ) const
{
    FILE* pFile = NULL;
    fopen_s( &pFile, szFileName, "wb" );
    if( !pFile )
        return E_FAIL;

    HRESULT hr = Save( pFile );

    fclose( pFile );

    return hr;
}



//--------------------------------------------------------------------------------------
// Name: GetPlayerSize
// Desc: Given the skeleton, update current player's size. Size is always updated,
// confidence is returned
//--------------------------------------------------------------------------------------
BOOL HOCDetector::GetPlayerSize( FLOAT& fPlayerSize, const NUI_SKELETON_DATA* pSkeleton )
{
    BOOL bRet = FALSE;

    if( pSkeleton->eSkeletonPositionTrackingState[ NUI_SKELETON_POSITION_SHOULDER_RIGHT ] == NUI_SKELETON_POSITION_TRACKED   &&
        pSkeleton->eSkeletonPositionTrackingState[ NUI_SKELETON_POSITION_SHOULDER_LEFT ] == NUI_SKELETON_POSITION_TRACKED    &&
        pSkeleton->eSkeletonPositionTrackingState[ NUI_SKELETON_POSITION_SHOULDER_CENTER ] == NUI_SKELETON_POSITION_TRACKED )
    {
        bRet = TRUE;
    }

    CONST XMVECTOR* pPositions = pSkeleton->SkeletonPositions;

    const XMVECTOR  vShoulderToNeckR = XMVectorSubtract( pPositions[ NUI_SKELETON_POSITION_SHOULDER_RIGHT ], pPositions[ NUI_SKELETON_POSITION_SHOULDER_CENTER ] );
    const XMVECTOR  vShoulderToNeckL = XMVectorSubtract( pPositions[ NUI_SKELETON_POSITION_SHOULDER_LEFT ], pPositions[ NUI_SKELETON_POSITION_SHOULDER_CENTER ] );
    const FLOAT     fSize = XMVectorGetX( XMVector3Length( vShoulderToNeckL ) + XMVector3Length( vShoulderToNeckR ) );

    fPlayerSize = fSize;

    return bRet;
}

//--------------------------------------------------------------------------------------
// Name: GetFrameData
// Desc: Given NUI data fill out the source and computed data structures
//--------------------------------------------------------------------------------------
BOOL    HOCDetector::GetFrameData(  HOCDataViews& dataViews,
                                    BOOL bTrackedElbow,
                                    BOOL bTrackedWrist,
                                    XMVECTOR vElbow,
                                    XMVECTOR vWrist,
                                    XMVECTOR vHand,
                                    const USHORT* pDepth,
                                    FLOAT fPlayerSize )
{
#ifdef _XBOX
    PIXBeginNamedEvent( 0, "HOC Get Frame Data" );
#endif

    XMStoreFloat3( &dataViews.m_sourceData.m_vHand, vHand );
    XMStoreFloat3( &dataViews.m_sourceData.m_vElbow, vElbow );
    XMStoreFloat3( &dataViews.m_sourceData.m_vWrist, vWrist );

    dataViews.m_sourceData.m_fPlayerSize = fPlayerSize;

    // 3d flood fill, store points
    std::vector< Voxel >    voxels;
    FindHand(   voxels,
                dataViews.m_sourceData.m_fHandSizeAtDistance,
                fPlayerSize,
                bTrackedElbow,
                bTrackedWrist,
                pDepth,
                vHand,
                vWrist,
                vElbow );

    if( voxels.size() >= HOCSourceData::MAX_POINTS )
        return FALSE;

    if( voxels.size() < MINIMUM_ACCEPTABLE_NUM_VOXELS )
        return FALSE;

    // copy to structure we save/load
    dataViews.m_sourceData.m_uNumVoxels = static_cast< USHORT >( voxels.size() );
    dataViews.m_sourceData.m_pVoxels = new Voxel[ dataViews.m_sourceData.m_uNumVoxels ];
    memcpy( &dataViews.m_sourceData.m_pVoxels [ 0 ], &voxels[ 0 ], sizeof( voxels[ 0 ] ) * voxels.size() );

    // refine and build and normalize histogram
    dataViews.m_sourceData.ComputeData( dataViews.m_computedData, &dataViews.m_transientData );

    // store this info with the data so the trainer knows how it's derived
    dataViews.m_sourceData.m_bTrackedElbowWrist = 0;
    
    if( bTrackedElbow )
        dataViews.m_sourceData.m_bTrackedElbowWrist |= HOCSourceData::ELBOW_TRACKED;

    if( bTrackedWrist )
        dataViews.m_sourceData.m_bTrackedElbowWrist |= HOCSourceData::WRIST_TRACKED;

#ifdef _XBOX
    PIXEndNamedEvent();
#endif

    return TRUE;
}

//--------------------------------------------------------------------------------------
// Name: Classify
// Desc: Given the source and computed data returns whether it's open or closed hand.
// The sign of the result value is the label (-1 or 1) and the magnitude is confidence
//--------------------------------------------------------------------------------------
FLOAT   HOCStrongClassifier::Classify( const HOCDataViews& data, const HOCExtraClassifierData& e ) const
{
    if( !m_bUseRealAdaboost )
    {
        const UINT uNumRules = static_cast< UINT >( m_listOfRules.size() );
        for( UINT i=0; i < uNumRules; ++i )
        {
            if( m_listOfRules[ i ].SimpleOutcome( data, e ) )
                return m_listOfRules[ i ].m_ruleOutcome;
        }
    }

    FLOAT fSum = 0.0f;

    if( m_bUseRealAdaboost )
    {
        const UINT nNumWeakClassifiers = static_cast< UINT >( m_weakClassifiersR.size() );
        if( !nNumWeakClassifiers )
            return 0;

        const HOCWeakClassifierR* pClassifier = &m_weakClassifiersR[ 0 ];

        for ( UINT i = 0; i < nNumWeakClassifiers; i++ )
        {
#ifdef _XBOX
            __dcbt( 0, &pClassifier[ 10 ] );
            __dcbt( 128, &pClassifier[ 10 ] );
#endif

            const FLOAT fValue = pClassifier->GetDataValue( data, e );
            const FLOAT fAlpha = pClassifier->GetAlpha();

            fSum += pClassifier->Classify( fValue, fAlpha );

            ++pClassifier;
        }

    } else
    {
        const UINT nNumWeakClassifiers = static_cast< UINT >( m_weakClassifiersD.size() );
        if( !nNumWeakClassifiers )
            return 0;

        const HOCWeakClassifierD* pClassifier = &m_weakClassifiersD[ 0 ];

        for ( UINT i = 0; i < nNumWeakClassifiers; i++ )
        {
            const FLOAT fValue = pClassifier->GetDataValue( data, e );
            const FLOAT fAlpha = pClassifier->GetAlpha();

            fSum += pClassifier->Classify( fValue, fAlpha );

            ++pClassifier;
        }
    }

    return fSum * m_fInvTotalAlpha;
}



//--------------------------------------------------------------------------------------
// Name: Write
// Desc: Write classifier data
//--------------------------------------------------------------------------------------
HRESULT HOCStrongClassifier::Write( FILE* pFile ) const
{
    if( !pFile )
        return E_FAIL;

    // write rules if any
    if( FAILED( HOCStrongClassifier::WriteArrayOfClassifiers( pFile, m_listOfRules ) ) )
        return E_FAIL;

    // write weak classifier
    if( FAILED( WriteUInt( &m_bUseRealAdaboost, pFile ) )   ||
        FAILED( WriteFloat( &m_fTotalAlpha, pFile ) )       ||
        FAILED( WriteUInt( m_uNumVoxels, pFile, 2 ) )       ||
        FAILED( WriteFloat( m_fDistance, pFile, 2 ) )       ||
        FAILED( WriteUInt( &m_uNumFilterFrames, pFile ) )   ||
        FAILED( WriteFloat( &m_fFilterThreshold, pFile ) ) )
    {
        return E_FAIL;
    }

    if( FAILED( WriteArrayOfClassifiers( pFile, m_weakClassifiersD ) )   ||
        FAILED( WriteArrayOfClassifiers( pFile, m_weakClassifiersR ) ) )
    {
        return E_FAIL;
    }

    return S_OK;
}



//--------------------------------------------------------------------------------------
// Name: WriteArrayOfClassifiers
// Desc: Write classifier data
//--------------------------------------------------------------------------------------
HRESULT HOCStrongClassifier::WriteArrayOfClassifiers( FILE* pFile, const HOCWeakClassifiersVectorD& arr )
{
    const UINT uNumWeaks = static_cast< UINT >( arr.size() );

    if( FAILED( WriteUInt( &uNumWeaks, pFile ) ) )
        return E_FAIL;

    for( UINT i=0; i < uNumWeaks; ++i )
    {
        if( FAILED( WriteFloat( &arr[ i ].m_fThreshold, pFile ) )   ||
            FAILED( WriteFloat( &arr[ i ].m_fAlpha, pFile ) )       ||
            FAILED( WriteUByte( arr[ i ].data, pFile, 4 ) ) )
        {
            return E_FAIL;
        }
    }

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Name: WriteArrayOfClassifiers
// Desc: Write classifier data
//--------------------------------------------------------------------------------------
HRESULT HOCStrongClassifier::WriteArrayOfClassifiers( FILE* pFile, const HOCWeakClassifiersVectorR& arr )
{
    const UINT uNumWeaks = static_cast< UINT >( arr.size() );

    if( FAILED( WriteUInt( &uNumWeaks, pFile ) ) )
        return E_FAIL;

    for( UINT i=0; i < uNumWeaks; ++i )
    {
        if( FAILED( WriteFloat( &arr[ i ].m_fMinValue, pFile ) )   ||
            FAILED( WriteFloat( &arr[ i ].m_fMaxValue, pFile ) )   ||
            FAILED( WriteFloat( arr[ i ].m_fResponse, pFile, _countof( arr[ i ].m_fResponse ) ) )   ||
            FAILED( WriteFloat( &arr[ i ].m_fAlpha, pFile ) )       ||
            FAILED( WriteUByte( arr[ i ].data, pFile, 4 ) ) )
        {
            return E_FAIL;
        }
    }

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Name: Read
// Desc: Read classifier data
//--------------------------------------------------------------------------------------
HRESULT HOCStrongClassifier::Read( FILE* pFile )
{
    if( !pFile )
        return E_FAIL;

    // read decision tree
    if( FAILED( HOCStrongClassifier::ReadArrayOfClassifiers( pFile, m_listOfRules ) ) )
        return E_FAIL;
    
    if( FAILED( ReadUInt( &m_bUseRealAdaboost, pFile ) )    ||
        FAILED( ReadFloat( &m_fTotalAlpha, pFile ) )        ||
        FAILED( ReadUInt( m_uNumVoxels, pFile, 2 ) )        ||
        FAILED( ReadFloat( m_fDistance, pFile, 2 ) )        ||
        FAILED( ReadUInt( &m_uNumFilterFrames, pFile ) )    ||
        FAILED( ReadFloat( &m_fFilterThreshold, pFile ) ) )
    {
        return E_FAIL;
    }

    m_fInvTotalAlpha = m_fTotalAlpha ? (1.f / m_fTotalAlpha) : 0;

    if( FAILED( ReadArrayOfClassifiers( pFile, m_weakClassifiersD ) )   ||
        FAILED( ReadArrayOfClassifiers( pFile, m_weakClassifiersR ) ) )
    {
        return E_FAIL;
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Read
// Desc: Read classifier data
//--------------------------------------------------------------------------------------
HRESULT HOCStrongClassifier::ReadPrev( FILE* pFile )
{
    if( !pFile )
        return E_FAIL;

    m_bUseRealAdaboost = FALSE;

    // read decision tree
    if( FAILED( HOCStrongClassifier::ReadArrayOfClassifiers( pFile, m_listOfRules ) ) )
        return E_FAIL;
    
    if( FAILED( ReadFloat( &m_fTotalAlpha, pFile ) )        ||
        FAILED( ReadUInt( m_uNumVoxels, pFile, 2 ) )        ||
        FAILED( ReadFloat( m_fDistance, pFile, 2 ) )        ||
        FAILED( ReadUInt( &m_uNumFilterFrames, pFile ) )    ||
        FAILED( ReadFloat( &m_fFilterThreshold, pFile ) ) )
    {
        return E_FAIL;
    }

    m_fInvTotalAlpha = m_fTotalAlpha ? (1.f / m_fTotalAlpha) : 0;

    if( FAILED( ReadArrayOfClassifiers( pFile, m_weakClassifiersD ) ) )
    {
        return E_FAIL;
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: ReadArrayOfClassifiers
// Desc: Read classifier data
//--------------------------------------------------------------------------------------
HRESULT HOCStrongClassifier::ReadArrayOfClassifiers( FILE* pFile, HOCWeakClassifiersVectorD& arr )
{
    UINT uNumWeaks;
    if( FAILED( ReadUInt( &uNumWeaks, pFile ) ) )
        return E_FAIL;

    arr.resize( uNumWeaks );

    for( UINT i=0; i < uNumWeaks; ++i )
    {
        if( FAILED( ReadFloat( &arr[ i ].m_fThreshold, pFile ) )   ||
            FAILED( ReadFloat( &arr[ i ].m_fAlpha, pFile ) )       ||
            FAILED( ReadUByte( arr[ i ].data, pFile, 4 ) ) )
        {
            return E_FAIL;
        }
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: ReadArrayOfClassifiers
// Desc: Read classifier data
//--------------------------------------------------------------------------------------
HRESULT HOCStrongClassifier::ReadArrayOfClassifiers( FILE* pFile, HOCWeakClassifiersVectorR& arr )
{
    UINT uNumWeaks;
    if( FAILED( ReadUInt( &uNumWeaks, pFile ) ) )
        return E_FAIL;

    arr.resize( uNumWeaks );

    for( UINT i=0; i < uNumWeaks; ++i )
    {
        if( FAILED( ReadFloat( &arr[ i ].m_fMinValue, pFile ) )   ||
            FAILED( ReadFloat( &arr[ i ].m_fMaxValue, pFile ) )   ||
            FAILED( ReadFloat( arr[ i ].m_fResponse, pFile, _countof( arr[ i ].m_fResponse ) ) )   ||
            FAILED( ReadFloat( &arr[ i ].m_fAlpha, pFile ) )       ||
            FAILED( ReadUByte( arr[ i ].data, pFile, 4 ) ) )
        {
            return E_FAIL;
        }
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: TraceLineToCentre
// Desc: In the depth image, trace a line from x, y to the centre of sx,sy. returns
//       the normalized length it travelled until it hit a non empty pixel
//--------------------------------------------------------------------------------------
static inline
FLOAT   TraceLineToCentre( INT x0, INT y0, INT x1, INT y1, INT sx, INT sy, const std::vector< USHORT >& depth )
{
    const BOOL bSteep = abs( y1 - y0 ) > abs( x1 - x0 );

    if( bSteep )
    {
        HOCSwap( x0, y0 );
        HOCSwap( x1, y1 );
    }

    const INT dx = abs( x1 - x0 );
    const INT dy = abs( y1 - y0 );

    const INT inc = ( x0 < x1 ) ? 1 : -1;
    const INT ystep = (y0 < y1) ? 1 : -1;

    INT y = y0;
    INT error = dx / 2;
    for( INT x = x0; x != x1; x += inc )
    {
        USHORT v = 0;

        if( bSteep )
        {
            if( y >= 0 && y < sx &&
                x >= 0 && x < sy )
            {
                v = depth[ y + x * sx ];
            }
        } else
        {
            if( x >= 0 && x < sx &&
                y >= 0 && y < sy )
            {
                v = depth[ x + y * sx ];
            }
        }

        if( v )
        {
            const FLOAT fToPoint = sqrtf( (FLOAT)((x1 - x) * (x1 - x) + (y1 - y) * (y1 - y)) ); // lines are short so integer overflow isn't likely
            return fToPoint;
        }

        error = error - dy;
        if( error < 0 )
        {
            y += ystep;
            error += dx;
        }
    }

    return 0;
}

//--------------------------------------------------------------------------------------
// Name: ComputeData
// Desc: Computes classifiers data from the source data each frame
//--------------------------------------------------------------------------------------
void    HOCSourceData::ComputeData( HOCComputedData& data, _In_opt_ HOCComputedTransientData* pTransientData ) const
{
    if( !m_uNumVoxels )
        return;

#ifdef _XBOX
    PIXBeginNamedEvent( 0, "HOC Compute Data" );
#endif

    // keep this data locally if needed
    HOCComputedTransientData    temp;
    if( !pTransientData )
        pTransientData = &temp;

    SHORT   minX = 32767;
    SHORT   minY = 32767;
    SHORT   maxX = 0;
    SHORT   maxY = 0;
    SHORT   sizeX, sizeY;

    // convert voxels to 2d points, get the centroid, also write a small 2D array of depths
    {
        pTransientData->m_points.resize( m_uNumVoxels );

        FLOAT   cx = 0;
        FLOAT   cy = 0;
        FLOAT   cz = 0;

        for( UINT i=0; i < m_uNumVoxels; ++i )
        {
            minX = std::min( minX, (SHORT)m_pVoxels[ i ].x );
            minY = std::min( minY, (SHORT)m_pVoxels[ i ].y );
            maxX = std::max( maxX, (SHORT)m_pVoxels[ i ].x );
            maxY = std::max( maxY, (SHORT)m_pVoxels[ i ].y );

            FLOAT   fD = m_pVoxels[ i ].z;
            FLOAT   fX = m_pVoxels[ i ].x;
            FLOAT   fY = m_pVoxels[ i ].y;

            const FLOAT zz = fD / 1000.f;           // millimetres to metres
            const FLOAT xx = (fX - FLOAT( HOC_DEPTH_SIZE_X ) * 0.5f) * zz * FOVH_PER_PIXEL;
            const FLOAT yy = (FLOAT( HOC_DEPTH_SIZE_Y ) * 0.5f - fY) * zz * FOVV_PER_PIXEL;

            XMFLOAT3& worldPos = pTransientData->m_points[ i ];
            worldPos.x = xx;
            worldPos.y = yy;
            worldPos.z = zz;

            cx += xx;
            cy += yy;
            cz += zz;
        }

        const FLOAT fNormalize = 1.f / static_cast< FLOAT >( m_uNumVoxels );

        pTransientData->m_vCentroid.x = cx * fNormalize;
        pTransientData->m_vCentroid.y = cy * fNormalize;
        pTransientData->m_vCentroid.z = cz * fNormalize;

        sizeX = maxX - minX + 1;
        sizeY = maxY - minY + 1;

        pTransientData->m_depthImage.resize( sizeX * sizeY );
        XMemSet( &pTransientData->m_depthImage[ 0 ], 0, sizeX * sizeY * sizeof( pTransientData->m_depthImage[ 0 ] ) );

        // write the depth image because some classifier are easier with an image
        for( UINT i=0; i < m_uNumVoxels; ++i )
        {
            const SHORT   xx = m_pVoxels[ i ].x - minX;
            const SHORT   yy = m_pVoxels[ i ].y - minY;
            
            pTransientData->m_depthImage[ sizeX * yy + xx ] = m_pVoxels[ i ].z;
        }

        // store the bounds in screen space
        pTransientData->m_depthImageBoundingBox[ 0 ] = minX;
        pTransientData->m_depthImageBoundingBox[ 1 ] = minY;
        pTransientData->m_depthImageBoundingBox[ 2 ] = sizeX;
        pTransientData->m_depthImageBoundingBox[ 3 ] = sizeY;
    }

    // draw bounding area of the voxels
    ADD_DEBUG_QUAD( minX, minY, sizeX, sizeY, 0xffff8080 );

    // calcualte palm centre
    // this didn't use to work well before August, now it works pretty well
    pTransientData->m_vPalmCentre = m_vWrist;

    // build the histogram of distances
    BuildHistogramRelatedData( 0, data, *pTransientData, XMLoadFloat3( &pTransientData->m_vPalmCentre ) );  // from the palm origin
    BuildHistogramRelatedData( 1, data, *pTransientData, XMLoadFloat3( &pTransientData->m_vCentroid ) );    // from the centroid
    BuildContourRelatedData( data, *pTransientData, sizeX, sizeY );

    // compute bones lengths ratios
    {
        // tracking state checked outside
        const XMVECTOR    d0 = XMVectorSubtract( XMLoadFloat3( &m_vHand ), XMLoadFloat3( &m_vWrist ) );
        const XMVECTOR    d1 = XMVectorSubtract( XMLoadFloat3( &m_vElbow ), XMLoadFloat3( &m_vWrist ) );

        // how many hands fit into a forearm
        XMStoreFloat( &data.m_fBoneLengths, XMVectorMultiply( XMVector3Length( d1 ), XMVector3ReciprocalLength( d0 ) ) );
    }

#ifdef _XBOX
    PIXEndNamedEvent();
#endif
}



//--------------------------------------------------------------------------------------
// Name: BuildContourRelatedData
// Desc: Builds a contour to figure out if it's not round
//--------------------------------------------------------------------------------------
VOID HOCSourceData::BuildContourRelatedData( HOCComputedData& data, HOCComputedTransientData& transientData, INT sizeX, INT sizeY ) const
{
    if( !m_uNumVoxels )
        return;

    FLOAT fAngleStep = 2 * XM_PI / (FLOAT)HOCComputedData::NUM_CONTOUR_POINTS;

    const FLOAT fRadius = sqrtf( (FLOAT)(sizeX * sizeX / 4) + (FLOAT)(sizeY * sizeY / 4) );

    // starting angle
    // tracking state is checked outside this function
    const XMVECTOR  d  =  XMVector3Normalize( XMVectorSubtract( XMLoadFloat3( &m_vWrist ), XMLoadFloat3( &m_vElbow ) ) );
    const FLOAT     dX =  XMVectorGetX( d );
    const FLOAT     dY = -XMVectorGetY( d );    // world space is Y up, screen space is Y down

    const FLOAT fCentreX = sizeX / 2.f;
    const FLOAT fCentreY = sizeY / 2.f;

    const UINT  x1 = (UINT)fCentreX;
    const UINT  y1 = (UINT)fCentreY;

    // start with the angle at which the forearm "enters" the bounding box for the hand
    // this gives a stable contour regardless of the hand orientation
    // for example, the "thumb peak" is always in the last 80% of the contour
    FLOAT   fAngle = atan2f( dY, dX );

    // do the tracing now
    for( UINT i=0; i < HOCComputedData::NUM_CONTOUR_POINTS; ++i )
    {
        const FLOAT fX = cosf( fAngle );
        const FLOAT fY = sinf( fAngle );

        const UINT  x0 = (UINT)(fCentreX + fRadius * fX);
        const UINT  y0 = (UINT)(fCentreY + fRadius * fY);

        const FLOAT fDist = TraceLineToCentre( x0, y0, x1, y1, sizeX, sizeY, transientData.m_depthImage );

        transientData.m_contour[ i ] = fDist;

        fAngle += fAngleStep;
    }

    // got the profile but it's noisy. run a low pass filter on it
    {
        static const INT   SUPPORT_SIZE = 2;
        static const FLOAT sigma = 2.f;

        FLOAT   fWeights[ 2 * SUPPORT_SIZE + 1 ];
        for( INT j=-SUPPORT_SIZE; j <= SUPPORT_SIZE; ++j )
        {
            fWeights[ SUPPORT_SIZE + j ] = expf( -(j * j) / (2 * sigma*sigma ) ) / sqrtf( 2 * XM_PI * sigma*sigma );
        }

        for( UINT i=0; i < HOCComputedData::NUM_CONTOUR_POINTS; ++i )
        {
            FLOAT   blurred = 0;
            for( INT j=-SUPPORT_SIZE; j <= SUPPORT_SIZE; ++j )
            {
                const FLOAT weight = fWeights[ SUPPORT_SIZE + j ];
                const FLOAT v = transientData.m_contour[ (i + j + HOCComputedData::NUM_CONTOUR_POINTS) % HOCComputedData::NUM_CONTOUR_POINTS ];

                blurred += v * weight;
            }

            transientData.m_contourSmooth[ i ] = blurred / fRadius;
        }
    }

    // we aligned the beginning of the contour with the angle of the wrist so in theory
    // we can even figure out which fingers stick out
    // as a general metric, though, we calculate how asymmetric the contour is

    // mean of the graph
    FLOAT   fMax = -FLT_MAX;
    FLOAT   fMin = FLT_MAX;
    FLOAT   fMean = 0;
    for( UINT i=0; i < HOCComputedData::NUM_CONTOUR_POINTS; ++i )
    {
        const FLOAT fValue = transientData.m_contourSmooth[ i ];
        fMax = std::max( fMax, fValue );
        fMin = std::min( fMin, fValue );
        fMean += fValue;
    }
    fMean /= (FLOAT)HOCComputedData::NUM_CONTOUR_POINTS;

    // find out how unbalanced it is arond the mean. a perfectly symmetric contour will give fMoment = 0
    FLOAT   fSpread = 0;
    for( UINT i=0; i < HOCComputedData::NUM_CONTOUR_POINTS; ++i )
    {
        const FLOAT fDelta = transientData.m_contourSmooth[ i ] - fMean;
        fSpread += fabsf( fDelta );
    }

    const FLOAT fMiddle = (fMax + fMin) * 0.5f;

    data.m_fContourMean = fMean;
    data.m_fContourSpread = fSpread;
    data.m_fContourSymmetry = fabsf( fMean - fMiddle );

    // contour experiments
    FLOAT   fExp1 = 0;
    FLOAT   fExp2 = 0;
    for( UINT i=0; i < HOCComputedData::NUM_CONTOUR_POINTS; ++i )
    {
        const FLOAT fDelta = transientData.m_contourSmooth[ i ] - fMiddle;
        fExp1 += fabsf( fDelta );
        fExp2 += fDelta;
    }
    data.m_fContourExp1 = fExp1;
    data.m_fContourExp2 = fExp2;

    // experiments
    data.m_fExperiment1 = m_fPlayerSize * m_uNumVoxels / transientData.m_vCentroid.z;
    data.m_fExperiment2 = m_uNumVoxels / (m_fPlayerSize * 100.f * transientData.m_vCentroid.z);
}

//--------------------------------------------------------------------------------------
// Name: BuildHistogramRelatedData
// Desc: Builds a distance histogram and the relevant moments
//--------------------------------------------------------------------------------------
VOID HOCSourceData::BuildHistogramRelatedData( UINT uIndex, HOCComputedData& data, HOCComputedTransientData& transientData, const XMVECTOR vCentroid ) const
{
    assert( uIndex < _countof( transientData.m_uIndexToFurthestVoxel ) );

    if( !m_uNumVoxels )
        return;

    const UINT uNumPoints = static_cast< UINT >( transientData.m_points.size() );

    std::vector< FLOAT >  toVoxelsAndDists( 5 * uNumPoints );                     // transient data, optimise allocation
    XMFLOAT3* __restrict  toVoxels = (XMFLOAT3*)&toVoxelsAndDists[ 0 ];           // N float3s
    FLOAT*    __restrict  dists = &toVoxelsAndDists[ 3 * uNumPoints ];            // N floats
    UINT*     __restrict  buckets = (UINT*)&toVoxelsAndDists[ 4 * uNumPoints ];   // N UINTs

    FLOAT     fScale = 1.f;
    {
        XMVECTOR vMaxDist = XMVectorZero();

        // compute distances
        for( UINT i=0; i < uNumPoints; ++i )
        {
            const XMVECTOR  vPoint = XMLoadFloat3( &transientData.m_points[ i ] );
            const XMVECTOR  vDist = XMVectorSubtract( vPoint, vCentroid );
            const XMVECTOR  vLength = XMVector3Length( vDist );

            vMaxDist = XMVectorMax( vMaxDist, vLength );

            XMStoreFloat3( &toVoxels[ i ], vDist );
            XMStoreFloat( &dists[ i ], vLength );
        }

        // distance normalising coefficient
        vMaxDist = XMVectorReciprocal( vMaxDist );
        XMStoreFloat( &fScale, vMaxDist );

        // normalize distances, generate histogram
        for( UINT i=0; i < uNumPoints; ++i )
        {
            const XMVECTOR  vLength = XMVectorMultiply( XMLoadFloat( &dists[ i ] ), vMaxDist );
            const XMVECTOR  vBucket = XMVectorFloor( XMVectorMultiply( vLength, TO_BIN_SCALER ) );
            const XMVECTOR  vBucketInt = XMConvertVectorFloatToInt( vBucket, 0 );

            // this is to avoid LHS on each access, store first, then read out in a separate loop
            XMStoreInt( &buckets[ i ], vBucketInt );
            XMStoreFloat( &dists[ i ], vLength );
        }

        // compute unnormalised histogram
        UINT    uNumPointsMadeIntoHistogram = 0;
        XMemSet( data.m_unnormalizedHistogram[ uIndex ], 0, sizeof( data.m_unnormalizedHistogram[ uIndex ] ) );
        for( UINT i=0; i < uNumPoints; ++i )
        {
            const UINT bin = buckets[ i ];

            if( bin < HOCComputedData::NUM_BUCKETS )
            {
                data.m_unnormalizedHistogram[ uIndex ][ bin ]++;
                ++uNumPointsMadeIntoHistogram;
            }
        }

        // normalize the histogram
        const FLOAT fHistoScale = 1.f / static_cast< FLOAT >( uNumPointsMadeIntoHistogram );
        for( UINT i=0; i < _countof( data.m_unnormalizedHistogram[ uIndex ] ); ++i )
        {
            data.m_normalizedHistogram[ uIndex ][ i ] = static_cast< FLOAT >( data.m_unnormalizedHistogram[ uIndex ][ i ] ) * fHistoScale;
        }

        // generate the integral histogram
        data.m_integralHistogram[ uIndex ][ 0 ] = data.m_normalizedHistogram[ uIndex ][ 0 ];
        for( UINT i=1; i < HOCComputedData::NUM_BUCKETS; ++i )
        {
            data.m_integralHistogram[ uIndex ][ i ] = data.m_integralHistogram[ uIndex ][ i - 1 ] + data.m_normalizedHistogram[ uIndex ][ i ];
        }
    }

    // mean and variance of the histogram of distances
    {
        // normalization factor
        const FLOAT is = 1.f / static_cast< FLOAT >( uNumPoints );

        FLOAT   fMean = 0;
        FLOAT   fVariance = 0;
        for( UINT i=0; i < uNumPoints; ++i )
        {
            const FLOAT   d = dists[ i ];
            fMean += d;
        }
        data.m_fMean[ uIndex ] = fMean * is;

        for( UINT i=0; i < uNumPoints; ++i )
        {
            const FLOAT  d = dists[ i ];
            const FLOAT dd = data.m_fMean[ uIndex ] - d;

            fVariance += dd * dd;
        }
        data.m_fVariance[ uIndex ] = sqrtf( fVariance * is ); // std dev
    }

    // roundness -- simply the ratio between the depth thickness and the size in XY
    {
        FLOAT   maxXY = 0, maxZ = 0;
        for( UINT i=0; i < uNumPoints; ++i )
        {
            maxXY = std::max( maxXY, sqrtf( toVoxels[ i ].x * toVoxels[ i ].x + toVoxels[ i ].y * toVoxels[ i ].y ) );
            maxZ = std::max( maxZ, fabsf( toVoxels[ i ].z ) );
        }

        data.m_fZroundness[ uIndex ] = maxZ / maxXY;
    }

    // find furthest point, 
    {
        // fcmps penalties when using floats, using the fact that positive floats are ordered
        const UINT* pDistances = reinterpret_cast< UINT* >( dists );
        UINT    uMaxDistance = 0;
        UINT    uFurthestVoxel = 0;
        for( UINT i=0; i < uNumPoints; ++i )
        {
            if( pDistances[ i ] > uMaxDistance )
            {
                uMaxDistance = pDistances[ i ];
                uFurthestVoxel = i;
            }
        }

        transientData.m_uIndexToFurthestVoxel[ uIndex ] = static_cast< USHORT >( uFurthestVoxel );
    }

    const XMVECTOR vInvDistanceFromPalmToCentroid = XMVectorReciprocal( XMVector3Length( XMVectorSubtract( XMLoadFloat3( &transientData.m_vCentroid ), XMLoadFloat3( &transientData.m_vPalmCentre ) ) ) );

    XMStoreFloat( &data.m_fExperiment3[ uIndex ], XMVectorMultiply(
                                            XMVector3Length( XMVectorSubtract( XMLoadFloat3( &transientData.m_points[ transientData.m_uIndexToFurthestVoxel[ uIndex ] ] ), 
                                                              XMLoadFloat3( &transientData.m_vCentroid ) ) ),
                                             vInvDistanceFromPalmToCentroid )
                                      );
}


//--------------------------------------------------------------------------------------
// Name: Load
// Desc: Loads source data
//--------------------------------------------------------------------------------------
HRESULT    HOCSourceData::Load( FILE* fp )
{
    if( !fp )
        return E_FAIL;

    delete[] m_pVoxels;
    m_pVoxels = NULL;

    if( FAILED( ReadFloat( &m_vHand.x, fp, 3 ) )     ||
        FAILED( ReadFloat( &m_vWrist.x, fp, 3 ) )    ||
        FAILED( ReadFloat( &m_vElbow.x, fp, 3 ) )    ||
        FAILED( ReadFloat( &m_fPlayerSize, fp ) )           ||
        FAILED( ReadUShort( &m_uNumVoxels, fp ) )           ||
        FAILED( ReadUShort( &m_bTrackedElbowWrist, fp ) ) )
    {
        return E_FAIL;
    }

    m_pVoxels = new Voxel[ m_uNumVoxels ];

    for( UINT i=0; i < m_uNumVoxels; ++i )
    {
        USHORT  v[ 3 ];

        if( FAILED( ReadUShort( v, fp, 3 ) ) )
            return E_FAIL;

        // this is the change to also support RGB voxels
        // if the top bit is set, this is "intensity" voxel
        const BOOL bIntensityVoxel = ( v[ 0 ] & (1 << 15) );

        m_pVoxels[ i ].x = v[ 0 ] & ~(1 << 15);
        m_pVoxels[ i ].y = v[ 1 ];
        m_pVoxels[ i ].z = v[ 2 ];

        if( bIntensityVoxel )
        {
            if( FAILED( ReadUShort( v, fp, 1 ) ) )
                return E_FAIL;

#ifdef PC_DEPTH_RESOLUTION
            m_pVoxels[ i ].extra = v[ 0 ];
#endif
        }
    }

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Name: Save
// Desc: Saves source data
//--------------------------------------------------------------------------------------
HRESULT HOCSourceData::Save( FILE* fp ) const
{
    if( !fp )
        return E_FAIL;

    if( FAILED( WriteFloat( &m_vHand.x, fp, 3 ) )     ||
        FAILED( WriteFloat( &m_vWrist.x, fp, 3 ) )    ||
        FAILED( WriteFloat( &m_vElbow.x, fp, 3 ) )    ||
        FAILED( WriteFloat( &m_fPlayerSize, fp ) )     ||
        FAILED( WriteUShort( &m_uNumVoxels, fp ) )     ||
        FAILED( WriteUShort( &m_bTrackedElbowWrist, fp ) ) )
    {
        return E_FAIL;
    }

    for( UINT i=0; i < m_uNumVoxels; ++i )
    {
        USHORT  v[ 3 ];

        v[ 0 ] = m_pVoxels[ i ].x;
        v[ 1 ] = m_pVoxels[ i ].y;
        v[ 2 ] = m_pVoxels[ i ].z;
        
        if( FAILED( WriteUShort( v, fp, 3 ) ) )
            return E_FAIL;
    }

    return S_OK;
}



//--------------------------------------------------------------------------------------
// Name: SaveDebugThumbnail
// Desc: Saves a tga thumbnail for debugging
//--------------------------------------------------------------------------------------
void    HOCSourceData::SaveDebugThumbnail( FILE* fp ) const
{
    INT minX = INT_MAX, minY = INT_MAX, maxX = -INT_MAX, maxY = -INT_MAX, minZ = INT_MAX, maxZ = -INT_MAX;
    for( INT i=0; i < m_uNumVoxels; ++i )
    {
        if( m_pVoxels[ i ].x < minX )
            minX = m_pVoxels[ i ].x;
        if( m_pVoxels[ i ].x > maxX )
            maxX = m_pVoxels[ i ].x;

        if( m_pVoxels[ i ].y < minY )
            minY = m_pVoxels[ i ].y;
        if( m_pVoxels[ i ].y > maxY )
            maxY = m_pVoxels[ i ].y;

        if( m_pVoxels[ i ].z < minZ )
            minZ = m_pVoxels[ i ].z;
        if( m_pVoxels[ i ].y > maxZ )
            maxZ = m_pVoxels[ i ].z;
    }

    // 
    const INT sx = maxX - minX + 1;
    const INT sy = maxY - minY + 1;
    std::vector< BYTE >    canvas( sx * sy, 0 );

    for( INT i=0; i < m_uNumVoxels; ++i )
    {
        INT x = m_pVoxels[ i ].x - minX;
        INT y = m_pVoxels[ i ].y - minY;
        INT z = m_pVoxels[ i ].z - minZ;

        canvas[ x + (sy - y - 1) * sx ] = static_cast< BYTE >( z & 255 );
    }

#pragma pack( push, 1 ) // this header has to be byte packed
    struct Header
    {
       CHAR  idlength;
       CHAR  colourmaptype;
       CHAR  datatypecode;
       SHORT colourmaporigin;
       SHORT colourmaplength;
       CHAR  colourmapdepth;
       SHORT x_origin;
       SHORT y_origin;
       SHORT width;
       SHORT height;
       CHAR  bitsperpixel;
       CHAR  imagedescriptor;
    };
#pragma pack( pop )

    const INT zoom = 4;

    Header  h = { 0 };
    h.datatypecode = 2;
    h.width = static_cast< SHORT >( sx ) * zoom;
    h.height = static_cast< SHORT >( sy ) * zoom;
    h.bitsperpixel = 24;

    fwrite( &h, sizeof( h ), 1, fp );

    for( INT i=0; i < sy; ++i )
    {
        for( INT z=0; z < zoom; ++z )
        {
            UINT k = i * sx;
            for( INT j=0; j < sx; ++j )
            {
                for( UINT zz = 0; zz < zoom; ++zz )
                {
                    fwrite( &canvas[ k ], 1, 1, fp );
                    fwrite( &canvas[ k ], 1, 1, fp );
                    fwrite( &canvas[ k ], 1, 1, fp );
                }

                ++k;
            }
        }
    }
}



//--------------------------------------------------------------------------------------
// Name: HOCPrintClassifier
// Desc: Debug printout of the classifier
//--------------------------------------------------------------------------------------
void HOCPrintClassifier( const HOCWeakClassifierR& c, UINT i, BOOL bPrintEol )
{
    if( c.m_type >= HOCBaseWeakClassifier::TYPE_REQUIRES_TWO_BUCKETS    &&
        c.m_type < HOCBaseWeakClassifier::TYPE_REQUIRES_ONE_BUCKET )
    {
        printf( "[%d] %s %d-%d [%f,%f], alpha %f",
                i,
                HOCGetClassiferName( c.m_type ),
                c.m_bucketIndices[ 0 ], c.m_bucketIndices[ 1 ],
                c.m_fMinValue, c.m_fMaxValue,
                c.m_fAlpha );

    } else if(  c.m_type >= HOCBaseWeakClassifier::TYPE_REQUIRES_ONE_BUCKET    &&
                c.m_type < HOCBaseWeakClassifier::TYPE_REQUIRES_NO_BUCKETS )
    {
        printf( "[%d] %s %d [%f,%f], alpha %f",
                i,
                HOCGetClassiferName( c.m_type ),
                c.m_bucketIndices[ 0 ],
                c.m_fMinValue, c.m_fMaxValue,
                c.m_fAlpha );
    } else
    {
        printf( "[%d] %s [%f,%f], alpha %f",
            i,
            HOCGetClassiferName( c.m_type ),
            c.m_fMinValue, c.m_fMaxValue,
            c.m_fAlpha );
    }


    if( bPrintEol )
        printf( "\n" );
}


//--------------------------------------------------------------------------------------
// Name: HOCPrintClassifier
// Desc: Debug printout of the classifier
//--------------------------------------------------------------------------------------
void HOCPrintClassifier( const HOCWeakClassifierD& c, UINT i, BOOL bPrintEol )
{
    if( c.m_type >= HOCBaseWeakClassifier::TYPE_REQUIRES_TWO_BUCKETS    &&
        c.m_type < HOCBaseWeakClassifier::TYPE_REQUIRES_ONE_BUCKET )
    {
        printf( "[%d] %s %d-%d > %f ? +1 : -1, alpha %f",
                i,
                HOCGetClassiferName( c.m_type ),
                c.m_bucketIndices[ 0 ], c.m_bucketIndices[ 1 ],
                c.m_fThreshold,
                c.m_fAlpha );

    } else if(  c.m_type >= HOCBaseWeakClassifier::TYPE_REQUIRES_ONE_BUCKET    &&
                c.m_type < HOCBaseWeakClassifier::TYPE_REQUIRES_NO_BUCKETS )
    {
        printf( "[%d] %s %d %s %f ? +1 : -1, alpha %f",
                i,
                HOCGetClassiferName( c.m_type ),
                c.m_bucketIndices[ 0 ],
                c.m_bReverse ? ">" : "<",
                c.m_fThreshold,
                c.m_fAlpha );
    } else
    {
        printf( "[%d] %s %s %f ? +1 : -1, alpha %f",
            i,
            HOCGetClassiferName( c.m_type ),
            c.m_bReverse ? ">" : "<",
            c.m_fThreshold,
            c.m_fAlpha );
    }

    if( bPrintEol )
        printf( "\n" );
}

//--------------------------------------------------------------------------------------
// Name: HOCGetClassiferName
// Desc: Returns a printable name for a classifier
//--------------------------------------------------------------------------------------
const CHAR* HOCGetClassiferName( UINT t )
{
    static const CHAR* names[ HOCBaseWeakClassifier::TYPE_NUM_CLASSIFIERS ] =
    {
        "bucketDiffW",
        "bucketDiffC",
        "bucketAbsW",
        "bucketAbsC",
        "ibucketAbsW",
        "ibucketAbsC",
        "meanW",
        "meanC",
        "varianceW",
        "varianceC",
        "zroundnessW",
        "zroundnessC",
        "cmean",
        "cspread",
        "csymm",
        "cexp1",
        "cexp2",
        "bonelengths",
        "experiment1",
        "experiment2",
        "experiment3",
        "experiment4",
    };

    assert( t < _countof( names ) );

    return names[ t ];
}


//--------------------------------------------------------------------------------------
// Name: Load
// Desc: Loads source data, recomputed compute data
//--------------------------------------------------------------------------------------
HRESULT HOCDataViews::Load( _In_ FILE* fp )
{
    if( FAILED( m_sourceData.Load( fp ) ) )
        return E_FAIL;

    m_sourceData.ComputeData( m_computedData );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Save
// Desc: Saves source data
//--------------------------------------------------------------------------------------
HRESULT HOCDataViews::Save( _In_ FILE* fp ) const
{
    return m_sourceData.Save( fp );
}



//--------------------------------------------------------------------------------------
// Name: Level1Detect
// Desc: Level1 doesn't use ada boost
//--------------------------------------------------------------------------------------
FLOAT   HOCDetector::Level1Detect( const HOCDataViews& data, const HOCExtraClassifierData& e ) const
{
    FLOAT   w = 0;

    // check gate redirects
    const UINT uNumGates = (UINT)e.m_level1Gates.size();
    for( UINT i=0; i < uNumGates; ++i )
    {
        const HOCLevel1Gate& gate = e.m_level1Gates[ i ];

        if( data.m_sourceData.m_uNumVoxels < gate.m_uMinVoxels ||
            data.m_sourceData.m_uNumVoxels > gate.m_uMaxVoxels )
        {
            continue;
        }
                
        const HOCDetector& d = e.m_level0Detectors[ gate.m_uDestinationIndex ];

        w += d.Detect( data );
    }

    return w;
}

//--------------------------------------------------------------------------------------
// Name: Write
// Desc: Writes to a file
//--------------------------------------------------------------------------------------
HRESULT HOCExtraClassifierData::Write( FILE* pFile ) const
{
    const UINT uNumStrongClassifiers = static_cast< UINT >( m_level0Detectors.size() );
    const UINT uNumGates = static_cast< UINT >( m_level1Gates.size() );

    if( FAILED( WriteUShort( &uNumStrongClassifiers, pFile ) )   ||
        FAILED( WriteUShort( &uNumGates, pFile ) ) )
    {
        return E_FAIL;
    }

    for( UINT i=0; i < uNumStrongClassifiers; ++i )
    {
        if( FAILED( m_level0Detectors[ i ].Save( pFile ) ) )
            return E_FAIL;
    }

    for( UINT i=0; i < uNumGates; ++i )
    {
        if( FAILED( WriteUShort( &m_level1Gates[ i ].m_uMinVoxels, pFile ) )  ||
            FAILED( WriteUShort( &m_level1Gates[ i ].m_uMaxVoxels, pFile ) )  ||
            FAILED( WriteUByte( &m_level1Gates[ i ].m_uDestinationIndex, pFile ) ) )
        {
            return E_FAIL;
        }
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Read
// Desc: Reads from a file
//--------------------------------------------------------------------------------------
HRESULT HOCExtraClassifierData::Read( FILE* pFile )
{
    UINT uNumStrongClassifiers;
    UINT uNumGates;

    if( FAILED( ReadUShort( &uNumStrongClassifiers, pFile ) ) ||
        FAILED( ReadUShort( &uNumGates, pFile ) ) )
    {
        return E_FAIL;
    }

    m_level0Detectors.resize( uNumStrongClassifiers );
    m_level1Gates.resize( uNumGates );

    for( UINT i=0; i < uNumStrongClassifiers; ++i )
    {
        if( FAILED( m_level0Detectors[ i ].Load( pFile ) ) )
            return E_FAIL;
    }

    for( UINT i=0; i < uNumGates; ++i )
    {
        if( FAILED( ReadUShort( &m_level1Gates[ i ].m_uMinVoxels, pFile ) )  ||
            FAILED( ReadUShort( &m_level1Gates[ i ].m_uMaxVoxels, pFile ) )  ||
            FAILED( ReadUByte( &m_level1Gates[ i ].m_uDestinationIndex, pFile ) ) )
        {
            return E_FAIL;
        }
    }

    return S_OK;
}
