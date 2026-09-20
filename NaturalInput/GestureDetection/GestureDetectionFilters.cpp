//--------------------------------------------------------------------------------------
// GestureDetectionFilters.cpp
//
// Gesture detection filters driven by probabilities from detection heuristics
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "GestureDetectionFilters.h"
#include <AtgUtil.h>


//--------------------------------------------------------------------------------------
// Defines and constants
//--------------------------------------------------------------------------------------

const FLOAT JumpDetectionFilter::c_fMaxOffsetDueToArmsAboveHead = 0.1f;    // 10cm


//--------------------------------------------------------------------------------------
// Name: HeightBaseLine::Initialize()
// Desc: Initialize data
//--------------------------------------------------------------------------------------

HRESULT HeightBaseLine::Initialize( const NUI_SKELETON_POSITION_INDEX eJoint )
{
    m_eJoint = eJoint;
    Reset();
    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: HeightBaseLine::Reset()
// Desc: Reset data
//--------------------------------------------------------------------------------------

VOID HeightBaseLine::Reset()
{
    // reset the ringbuffer to all zeros
    m_RingBuffer.clear();
    m_RingBuffer.resize( HEIGHT_BASE_LINE_RING_BUFFER_SIZE, 0.0f );

    ZeroMemory( m_fFinalHeight, sizeof( m_fFinalHeight ) );
    ZeroMemory( m_fOriginalHeight, sizeof( m_fOriginalHeight ) );
}


//--------------------------------------------------------------------------------------
// Name: HeightBaseLine::OffsetHeight()
// Desc: Offset height data
//--------------------------------------------------------------------------------------

VOID HeightBaseLine::OffsetHeight( const UINT uSkeletonIdx, const FLOAT fOffset )
{
    // Only add offset if we have a valid value
    if ( m_fOriginalHeight[ uSkeletonIdx ] != 0.0f )
    {
        m_fFinalHeight[ uSkeletonIdx ] = m_fOriginalHeight[ uSkeletonIdx ] + fOffset;
    }
}


//--------------------------------------------------------------------------------------
// Name: HeightBaseLine::Update()
// Desc: Update data
//--------------------------------------------------------------------------------------

VOID HeightBaseLine::Update( NUI_SKELETON_FRAME* pSkeletonFrame, Heuristic* pHeuristic )
{
    PIXBeginNamedEvent( 0, __FUNCTION__ );

    for ( UINT i = 0; i < NUI_SKELETON_COUNT; i++ )
    {
        NUI_SKELETON_DATA* pSkeletonData = &pSkeletonFrame->SkeletonData[ i ];

        // If not tracked, then reset value
        if ( pSkeletonData->eTrackingState != NUI_SKELETON_TRACKED ||
             pSkeletonData->eSkeletonPositionTrackingState[ m_eJoint ] == NUI_SKELETON_POSITION_NOT_TRACKED )             
        {
            m_fOriginalHeight[ i ] = 0.0f;
            continue;
        }

        // only use actual tracked values for height update
        if ( pSkeletonData->eSkeletonPositionTrackingState[ m_eJoint ] == NUI_SKELETON_POSITION_TRACKED )
        {
            if ( pHeuristic &&
                 pHeuristic->IsValid( i ) &&
                 pHeuristic->GetProbability( i ) > 0.0f )
            {
                // early out, a heuristic fired, e.g. hands above head and we shouldn't update the baseline
                continue;
            }

            FLOAT fHeight = pSkeletonData->SkeletonPositions[ m_eJoint ].y;

            m_RingBuffer.pop_front();
            m_RingBuffer.push_back( fHeight );

            // find min and max values in ring buffer            
            FLOAT fMin =  FLT_MAX;
            FLOAT fMax = -FLT_MAX;
            const UINT uNumValues = m_RingBuffer.size();
            for ( UINT j = 0; j < uNumValues; j++ )
            {
                fHeight = m_RingBuffer[ j ];
                fMin = min( fMin, fHeight );
                fMax = max( fMax, fHeight );
            }

            // Find the deviation
            FLOAT fDeviation = fMax - fMin;

            // if within deviation threshold, update the head height
            if ( fDeviation < HEIGHT_BASE_LINE_DEVIATION_THRESHOLD )
            {
                UpdateFunction( i, fMax );
            }
        }   
    }

    // Make sure the output height is the same as the originally calculated one, we will
    // add a potential offset aftet his call
    for ( UINT i = 0; i < NUI_SKELETON_COUNT; i++ )
    {
        m_fFinalHeight[ i ] = m_fOriginalHeight[ i ];
    }

    PIXEndNamedEvent();
}


//--------------------------------------------------------------------------------------
// Name: HeightBaseLineUsingMinWithAngleRestriction::HeightBaseLineUsingMinWithAngleRestriction
// Desc: Constructor
//--------------------------------------------------------------------------------------

HeightBaseLineUsingMinWithAngleRestriction::HeightBaseLineUsingMinWithAngleRestriction() : HeightBaseLineUsingMin()
{
    m_pSkeletonFrame    = NULL;
    m_eBoneJoint1       = NUI_SKELETON_POSITION_COUNT;
    m_eBoneJointCenter  = NUI_SKELETON_POSITION_COUNT;
    m_eBoneJoint2       = NUI_SKELETON_POSITION_COUNT;

}


//--------------------------------------------------------------------------------------
// Name: HeightBaseLineUsingMinWithAngleRestriction::Initialize
// Desc: Initialize data
//--------------------------------------------------------------------------------------

HRESULT HeightBaseLineUsingMinWithAngleRestriction::Initialize( const NUI_SKELETON_POSITION_INDEX eJoint,
                                                                const NUI_SKELETON_FRAME* pSkeletonFrame,
                                                                const NUI_SKELETON_POSITION_INDEX eBoneJoint1,
                                                                const NUI_SKELETON_POSITION_INDEX eBoneJointCenter,
                                                                const NUI_SKELETON_POSITION_INDEX eBoneJoint2 )
{
    RETURN_ON_FAIL( HeightBaseLine::Initialize( eJoint ) );

    RETURN_ON_NULL( m_pSkeletonFrame = pSkeletonFrame );

    m_eBoneJoint1       = eBoneJoint1;
    m_eBoneJointCenter  = eBoneJointCenter;
    m_eBoneJoint2       = eBoneJoint2;

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: HeightBaseLineUsingMinWithAngleRestriction::Initialize
// Desc: Update data
//--------------------------------------------------------------------------------------

VOID HeightBaseLineUsingMinWithAngleRestriction::UpdateFunction( const UINT uSkeletonIdx, const FLOAT fValue )
{
    XMVECTOR vBoneJoint1        = m_pSkeletonFrame->SkeletonData[ uSkeletonIdx ].SkeletonPositions[ m_eBoneJoint1 ];
    XMVECTOR vBoneJointCenter   = m_pSkeletonFrame->SkeletonData[ uSkeletonIdx ].SkeletonPositions[ m_eBoneJointCenter ];
    XMVECTOR vBoneJoint2        = m_pSkeletonFrame->SkeletonData[ uSkeletonIdx ].SkeletonPositions[ m_eBoneJoint2 ];
      
    FLOAT fAngle = 0.0f;

    if ( m_pSkeletonFrame->SkeletonData[ uSkeletonIdx ].eSkeletonPositionTrackingState[ m_eBoneJoint1 ] != NUI_SKELETON_POSITION_NOT_TRACKED &&
         m_pSkeletonFrame->SkeletonData[ uSkeletonIdx ].eSkeletonPositionTrackingState[ m_eBoneJointCenter ] != NUI_SKELETON_POSITION_NOT_TRACKED &&
         m_pSkeletonFrame->SkeletonData[ uSkeletonIdx ].eSkeletonPositionTrackingState[ m_eBoneJoint2 ] != NUI_SKELETON_POSITION_NOT_TRACKED )
    {
        // Calc normalized vectors for two bones
        XMVECTOR vBone1 = XMVector3Normalize( vBoneJoint1 - vBoneJointCenter );
        XMVECTOR vBone2 = XMVector3Normalize( vBoneJoint2 - vBoneJointCenter );

        // Calculate angles between bones
        XMVECTOR vAngle = XMVector3AngleBetweenNormals( vBone1, vBone2 );

        // sanity check
        assert( !XMVector4IsNaN( vAngle ) );
        assert( !XMVector4IsInfinite( vAngle ) );

        fAngle = XMConvertToDegrees( XMVectorGetX( vAngle ) );
    }

    const FLOAT fEpsilon = 5.0f;    // Determines how straight the angle should be before updating
    if ( fAngle > ( 180.0f - fEpsilon  ) )
    {
        HeightBaseLineUsingMin::UpdateFunction( uSkeletonIdx, fValue );
    }
}


//--------------------------------------------------------------------------------------
// Name: HeightBaseLine2::HeightBaseLine2()
// Desc: Constructor
//--------------------------------------------------------------------------------------

HeightBaseLine2::HeightBaseLine2()
{
    m_pHeightBaseLineLeft   = NULL;
    m_pHeightBaseLineRight  = NULL;
    Reset();
}


//--------------------------------------------------------------------------------------
// Name: HeightBaseLine2::Initialize()
// Desc: Initialize data
//--------------------------------------------------------------------------------------

HRESULT HeightBaseLine2::Initialize( HeightBaseLine* pHeightBaseLineLeft, HeightBaseLine* pHeightBaseLineRight )
{
    RETURN_ON_NULL( m_pHeightBaseLineLeft = pHeightBaseLineLeft );
    RETURN_ON_NULL( m_pHeightBaseLineRight = pHeightBaseLineRight );
    Reset();

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: HeightBaseLine2::Reset()
// Desc: Reset data
//--------------------------------------------------------------------------------------

VOID HeightBaseLine2::Reset()
{
    if ( m_pHeightBaseLineLeft )
    {
        m_pHeightBaseLineLeft->Reset();
    }

    if ( m_pHeightBaseLineRight )
    {
        m_pHeightBaseLineRight->Reset();
    }

    ZeroMemory( m_fHeight, sizeof( m_fHeight ) );
}


//--------------------------------------------------------------------------------------
// Name: HeightBaseLine2::Update()
// Desc: Update data
//--------------------------------------------------------------------------------------

VOID HeightBaseLine2::Update( NUI_SKELETON_FRAME* pSkeletonFrame, Heuristic* pHeuristic )
{
    m_pHeightBaseLineLeft->Update( pSkeletonFrame, pHeuristic );
    m_pHeightBaseLineRight->Update( pSkeletonFrame, pHeuristic );

    for ( UINT i = 0; i < NUI_SKELETON_COUNT; i++ )
    {
        FLOAT fTotal = m_pHeightBaseLineLeft->GetHeight( i ) == 0.0f ? 0.0f : 1.0f;
        fTotal += m_pHeightBaseLineRight->GetHeight( i ) == 0.0f ? 0.0f : 1.0f;

        m_fHeight[ i ]  = m_pHeightBaseLineLeft->GetHeight( i ) + m_pHeightBaseLineRight->GetHeight( i );
        m_fHeight[ i ] /= max( 1.0f, fTotal );
    }
}


//--------------------------------------------------------------------------------------
// Name: GestureDetectionFilter::GestureDetectionFilter()
// Desc: Constructor
//--------------------------------------------------------------------------------------

GestureDetectionFilter::GestureDetectionFilter()
{
    m_HeuristicData.clear();
    Reset();
}


//--------------------------------------------------------------------------------------
// Name: GestureDetectionFilter::~GestureDetectionFilter()
// Desc: Destructor
//--------------------------------------------------------------------------------------

GestureDetectionFilter::~GestureDetectionFilter()
{
    Reset();

    // Delete all heuristic pointers added with AddHeuristic()
    const UINT uNumHeuristics = m_HeuristicData.size();
    for ( UINT i = 0; i < uNumHeuristics; i++ )
    {
        Heuristic* pHeuristic = m_HeuristicData[ i ].m_pHeuristic;
        SAFE_DELETE( pHeuristic );
    }

    m_HeuristicData.clear();
}


//--------------------------------------------------------------------------------------
// Name: GestureDetectionFilter::Reset()
// Desc: Reset data
//--------------------------------------------------------------------------------------

VOID GestureDetectionFilter::Reset()
{
    ZeroMemory( m_fProbability, sizeof( m_fProbability ) );

    // Reset all heuristics
    const UINT uNumHeuristics = m_HeuristicData.size();
    for ( UINT i = 0; i < uNumHeuristics; i++ )
    {
        Heuristic* pHeuristic = m_HeuristicData[ i ].m_pHeuristic;
        assert( pHeuristic );

        pHeuristic->Reset();
    }
}


//--------------------------------------------------------------------------------------
// Name: GestureDetectionFilter::Reset()
// Desc: Reset data per player
//--------------------------------------------------------------------------------------

VOID GestureDetectionFilter::Reset( const UINT uSkeletonIdx )
{
    m_fProbability[ uSkeletonIdx ] = 0.0f;

    // Reset all heuristics
    const UINT uNumHeuristics = m_HeuristicData.size();
    for ( UINT i = 0; i < uNumHeuristics; i++ )
    {
        Heuristic* pHeuristic = m_HeuristicData[ i ].m_pHeuristic;
        assert( pHeuristic );

        pHeuristic->Reset( uSkeletonIdx );
    }
}


//--------------------------------------------------------------------------------------
// Name: GestureDetectionFilter::Update()
// Desc: Update probabilities
//--------------------------------------------------------------------------------------

VOID GestureDetectionFilter::Update( NUI_SKELETON_FRAME* pSkeletonFrame )
{
    PIXBeginNamedEvent( 0, __FUNCTION__ );

    // Update all heuristics
    const UINT uNumHeuristics = m_HeuristicData.size();
    for ( UINT i = 0; i < uNumHeuristics; i++ )
    {
        Heuristic* pHeuristic = m_HeuristicData[ i ].m_pHeuristic;
        assert( pHeuristic );

        pHeuristic->Update( pSkeletonFrame );
    }

    // Calculate probabilities from heuristics
    for ( UINT i = 0; i < NUI_SKELETON_COUNT; i++ )
    {
        if ( pSkeletonFrame->SkeletonData[ i ].eTrackingState == NUI_SKELETON_TRACKED )
        {
            m_fProbability[ i ] = CalcProbability( i );
        }
        else
        {
            Reset( i ); // reset untracked skeletons
            m_fProbability[ i ] = 0.0f;
        }
    }

    PIXEndNamedEvent();
}


//--------------------------------------------------------------------------------------
// Name: GestureDetectionFilter::CalcProbability()
// Desc: Calculate weighted probability of detection from heuristics. Override this 
//       method if you want to calculate the probabilities differently, e.g. multiply
//       the probabilities of the converse, or let false positves sum up as negative, etc.
//--------------------------------------------------------------------------------------

FLOAT GestureDetectionFilter::CalcProbability( const UINT uSkeletonIdx ) const
{
    PIXBeginNamedEvent( 0, __FUNCTION__ );

    FLOAT fProbability  = 0.0f;
    FLOAT fTotalWeights = 0.0f;

    // Calculate the total weighted detection probability and return if a false positive was found
    const UINT uNumHeuristics = m_HeuristicData.size();
    for ( UINT i = 0; i < uNumHeuristics; i++ )
    {
        Heuristic* pHeuristic = m_HeuristicData[ i ].m_pHeuristic;
        FLOAT fWeight = m_HeuristicData[ i ].m_fWeight;

        assert( pHeuristic );

        switch( pHeuristic->GetType() )
        {
        case Heuristic::TYPE_DETECTION:            
            if ( pHeuristic->IsValid( uSkeletonIdx ) )
            {
                // Do a weighted sum of the probabilities of the detection heuristics
                fProbability += pHeuristic->GetProbability( uSkeletonIdx ) * fWeight;
                fTotalWeights += fWeight;
            }
            break;

        case Heuristic::TYPE_FALSE_DETECTION:
            if ( pHeuristic->IsValid( uSkeletonIdx ) &&
                 pHeuristic->GetProbability( uSkeletonIdx ) >= 1.0f )
            {
                // If we get a heuristic that has 100% probability that it's a false
                // positive, we early out with 0% probability
                fProbability = 0.0f;
                break;
            }
        }
    }

    // If for some reason a heuristic were invalid, we cannot just use the weighted sum
    // value, we need to rescale the results to the number of heuristics that were valid
    if ( fTotalWeights > 0.0f )
    {
        fProbability /= fTotalWeights;
    }

    PIXEndNamedEvent();

    return fProbability;
}

//--------------------------------------------------------------------------------------
// Name: DuckDetectionFilter::DuckDetectionFilter()
// Desc: Constructor
//--------------------------------------------------------------------------------------

DuckDetectionFilter::DuckDetectionFilter() : GestureDetectionFilter()
{
    m_pBodyFaceUpwards                  = NULL;
    m_pHeadHeightAboveBaseLine          = NULL;
    m_pHeadHeightBelowBaseLine          = NULL;
    m_pHeadHeightFarBelowBaseLine       = NULL;
    m_pBodyFaceDownwards                = NULL;
    m_pUpperBodyAngleTowardsLowerBody   = NULL;
    m_pHandsAboveHead                   = NULL;
}

//--------------------------------------------------------------------------------------
// Name: DuckDetectionFilter::Initialize()
// Desc: Initialize filter
//--------------------------------------------------------------------------------------

HRESULT DuckDetectionFilter::Initialize()
{
    Heuristic* pHeuristic;

    // Add heuristics for false positives first so that we can early out quickly
    RETURN_ON_NULL( pHeuristic = new BodyFaceUpwards( Heuristic::TYPE_FALSE_DETECTION ) );
    pHeuristic->Initialize( 0.0f, 5.0f );       // in degrees
    AddHeuristic( pHeuristic, 0 );
    m_pBodyFaceUpwards = (BodyFaceUpwards*)pHeuristic;

    RETURN_ON_NULL( pHeuristic = new HeightAboveBaseLine( NUI_SKELETON_POSITION_HEAD, Heuristic::TYPE_FALSE_DETECTION ) );
    pHeuristic->Initialize( -0.05f, -0.049f );  // false from about ~5cm under baseline and up
    pHeuristic->SetData( m_HeadHeightBaseLine.GetHeightValues() );
    AddHeuristic( pHeuristic, 0.0f );
    m_pHeadHeightAboveBaseLine = (HeightAboveBaseLine*)pHeuristic;

    // Add heurustics for soft detection
    RETURN_ON_NULL( pHeuristic = new BodyFaceDownwards );
    pHeuristic->Initialize( 0.0f, 30.0f );      // in degrees
    AddHeuristic( pHeuristic, 0.6f );
    m_pBodyFaceDownwards = (BodyFaceDownwards*)pHeuristic;

    RETURN_ON_NULL( pHeuristic = new HeightBelowBaseLine( NUI_SKELETON_POSITION_HEAD ) );
    pHeuristic->SetData( m_HeadHeightBaseLine.GetHeightValues() );
    pHeuristic->Initialize( 0.0f, 0.15f );      // ~15cm above baselin. Could also be defined as a % of shoulder width or player height
    AddHeuristic( pHeuristic, 0.2f );
    m_pHeadHeightBelowBaseLine = (HeightBelowBaseLine*)pHeuristic;

    RETURN_ON_NULL( pHeuristic = new UpperBodyAngleTowardsLowerBody );
    pHeuristic->Initialize( 0.0f, 20.0f );      // in degrees
    AddHeuristic( pHeuristic, 0.2f );
    m_pUpperBodyAngleTowardsLowerBody = (UpperBodyAngleTowardsLowerBody*)pHeuristic;

    // Add heurustics for hard detection
    RETURN_ON_NULL( pHeuristic = new HeightBelowBaseLine( NUI_SKELETON_POSITION_HEAD ) );
    pHeuristic->SetData( m_HeadHeightBaseLine.GetHeightValues() );
    pHeuristic->Initialize( 0.0f, 0.25f );      // ~25cm above baselin. Could also be defined as a % of shoulder width or player height
    AddHeuristic( pHeuristic, 0.0f );   // No weight, we'll won't use this as part of the soft probability
    m_pHeadHeightFarBelowBaseLine = (HeightBelowBaseLine*)pHeuristic;

    RETURN_ON_NULL( pHeuristic = new HandsAboveHead );
    pHeuristic->Initialize( 0.0f, 0.25f );      // ~25cm above baselin. Could also be defined as a % of shoulder width or player height
    AddHeuristic( pHeuristic, 0.0f );   // No weight, we'll won't use this as part of the soft probability
    m_pHandsAboveHead = (HandsAboveHead*)pHeuristic;

    RETURN_ON_FAIL( m_HeadHeightBaseLine.Initialize( NUI_SKELETON_POSITION_HEAD ) );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: DuckDetectionFilter::Update()
// Desc: Initialize filter
//--------------------------------------------------------------------------------------

VOID DuckDetectionFilter::Update( NUI_SKELETON_FRAME* pSkeletonFrame )
{
    m_HeadHeightBaseLine.Update( pSkeletonFrame, m_pHandsAboveHead );
    GestureDetectionFilter::Update( pSkeletonFrame );
}


//--------------------------------------------------------------------------------------
// Name: DuckDetectionFilter::CalcProbability()
// Desc: Calculate weighted probability of detection from heuristics
//--------------------------------------------------------------------------------------

FLOAT DuckDetectionFilter::CalcProbability( const UINT uSkeletonIdx ) const
{
    PIXBeginNamedEvent( 0, __FUNCTION__ );

    // Calculate soft weighted probability
    FLOAT fProbability = GestureDetectionFilter::CalcProbability( uSkeletonIdx );

    // Override if one of these are 100% true, but only if fProbability > 0.0f,
    // since we still want to keep the results of the false positives
    if ( fProbability > 0.0f )
    {
        if ( m_pHeadHeightFarBelowBaseLine->IsValid( uSkeletonIdx ) &&
             m_pHeadHeightFarBelowBaseLine->GetProbability( uSkeletonIdx ) == 1.0f )
        {
            fProbability = 1.0f;
        }
        else if ( m_pBodyFaceDownwards->IsValid( uSkeletonIdx ) &&
                  m_pBodyFaceDownwards->GetProbability( uSkeletonIdx ) == 1.0f )
        {
            fProbability = 1.0f;
        }
    }

    PIXEndNamedEvent();

    return fProbability;
}


//--------------------------------------------------------------------------------------
// Name: JumpDetectionFilter::JumpDetectionFilter()
// Desc: Constructor
//--------------------------------------------------------------------------------------

JumpDetectionFilter::JumpDetectionFilter() : GestureDetectionFilter()
{
    m_pHeadHeightBelowBaseLine                      = NULL;
    m_pLeftAnkleHeightBelowBaseLine                 = NULL;
    m_pRightAnkleHeightBelowBaseLine                = NULL;
    m_pLeftKneeHeightBelowBaseLine                  = NULL;
    m_pRightKneeHeightBelowBaseLine                 = NULL;
    m_pHeadHeightAboveBaseLineCumulativeSum         = NULL;
    m_pHeadHeightFarAboveBaseLineCumulativeSum      = NULL;
    m_pLeftKneeHeightAboveBaseLineCumulativeSum     = NULL;
    m_pRightKneeHeightAboveBaseLineCumulativeSum    = NULL;
    m_pLegsStraightPreviouslyBent                   = NULL;
    m_pHandsAboveHead                               = NULL;
    m_pBodyFaceUpwards                              = NULL;
}


//--------------------------------------------------------------------------------------
// Name: JumpDetectionFilter::Initialize()
// Desc: Initialize filter
//--------------------------------------------------------------------------------------

HRESULT JumpDetectionFilter::Initialize( NUI_SKELETON_FRAME* pSkeletonFrame )
{
    Heuristic* pHeuristic;

    // Add heuristics for false positives first so that we can early out quickly
    RETURN_ON_NULL( pHeuristic = new HeightBelowBaseLine( NUI_SKELETON_POSITION_HEAD, Heuristic::TYPE_FALSE_DETECTION ) );
    pHeuristic->SetData( m_HeadHeightBaseLine.GetHeightValues() );
    pHeuristic->Initialize( -0.03f, -0.029f );  // false from about ~3cm above baseline and down
    AddHeuristic( pHeuristic, 0.0f );
    m_pHeadHeightBelowBaseLine = (HeightBelowBaseLine*)pHeuristic;

    RETURN_ON_NULL( pHeuristic = new HeightBelowBaseLine( NUI_SKELETON_POSITION_ANKLE_LEFT, Heuristic::TYPE_FALSE_DETECTION ) );
    pHeuristic->SetData( m_AnkleHeightBaseLine.GetHeightValues() );
    pHeuristic->Initialize( -0.02f, -0.015f );  // false from about ~2cm above baseline and down
    AddHeuristic( pHeuristic, 0.0f );
    m_pLeftAnkleHeightBelowBaseLine = (HeightBelowBaseLine*)pHeuristic;

    RETURN_ON_NULL( pHeuristic = new HeightBelowBaseLine( NUI_SKELETON_POSITION_ANKLE_RIGHT, Heuristic::TYPE_FALSE_DETECTION ) );
    pHeuristic->SetData( m_AnkleHeightBaseLine.GetHeightValues() );
    pHeuristic->Initialize( -0.02f, -0.015f );  // false from about ~2cm above baseline and down
    AddHeuristic( pHeuristic, 0.0f );
    m_pRightAnkleHeightBelowBaseLine = (HeightBelowBaseLine*)pHeuristic;

    RETURN_ON_NULL( pHeuristic = new HeightBelowBaseLine( NUI_SKELETON_POSITION_KNEE_LEFT, Heuristic::TYPE_FALSE_DETECTION ) );
    pHeuristic->SetData( m_KneeHeightBaseLine.GetHeightValues() );
    pHeuristic->Initialize( -0.02f, -0.019f );  // false from about ~2cm above baseline and down
    AddHeuristic( pHeuristic, 0.0f );
    m_pLeftKneeHeightBelowBaseLine = (HeightBelowBaseLine*)pHeuristic;

    RETURN_ON_NULL( pHeuristic = new HeightBelowBaseLine( NUI_SKELETON_POSITION_KNEE_RIGHT, Heuristic::TYPE_FALSE_DETECTION ) );
    pHeuristic->SetData( m_KneeHeightBaseLine.GetHeightValues() );
    pHeuristic->Initialize( -0.02f, -0.019f );  // false from about ~2cm above baseline and down
    AddHeuristic( pHeuristic, 0.0f );
    m_pRightKneeHeightBelowBaseLine = (HeightBelowBaseLine*)pHeuristic;

    RETURN_ON_NULL( pHeuristic = new BodyFaceUpwards( Heuristic::TYPE_FALSE_DETECTION ) );
    pHeuristic->Initialize( 0.0f, 15.0f );      // in degrees
    AddHeuristic( pHeuristic, 0 );
    m_pBodyFaceUpwards = (BodyFaceUpwards*)pHeuristic;

    // Add heurustics for soft detection
    RETURN_ON_NULL( pHeuristic = new HeightAboveBaseLineCumulativeSum( NUI_SKELETON_POSITION_HEAD ) );
    pHeuristic->Initialize( 0.0f, 0.04f );      // ~4cm above baselin.
    pHeuristic->SetData( m_HeadHeightBaseLine.GetHeightValues() );
    AddHeuristic( pHeuristic, 0.3f );
    m_pHeadHeightAboveBaseLineCumulativeSum = (HeightAboveBaseLineCumulativeSum*)pHeuristic;

    RETURN_ON_NULL( pHeuristic = new HeightAboveBaseLineCumulativeSum( NUI_SKELETON_POSITION_KNEE_LEFT ) );
    pHeuristic->SetData( m_KneeHeightBaseLine.GetHeightValues() );
    pHeuristic->Initialize( 0.0f, 0.04f );      // ~4cm above baselin.
    AddHeuristic( pHeuristic, 0.1f );
    m_pLeftKneeHeightAboveBaseLineCumulativeSum = (HeightAboveBaseLineCumulativeSum*)pHeuristic;

    RETURN_ON_NULL( pHeuristic = new HeightAboveBaseLineCumulativeSum( NUI_SKELETON_POSITION_KNEE_RIGHT ) );
    pHeuristic->SetData( m_KneeHeightBaseLine.GetHeightValues() );
    pHeuristic->Initialize( 0.0f, 0.04f );      // ~4cm above baselin.
    AddHeuristic( pHeuristic, 0.1f );
    m_pRightKneeHeightAboveBaseLineCumulativeSum = (HeightAboveBaseLineCumulativeSum*)pHeuristic;

    RETURN_ON_NULL( pHeuristic = new LegsStraightPreviouslyBent );
    pHeuristic->Initialize( 0.0f, 15.0f );      // in degrees
    AddHeuristic( pHeuristic, 0.5f );
    m_pLegsStraightPreviouslyBent = (LegsStraightPreviouslyBent*)pHeuristic;

    // Add heurustics for hard detection
    RETURN_ON_NULL( pHeuristic = new HeightAboveBaseLine( NUI_SKELETON_POSITION_HEAD ) );
    pHeuristic->Initialize( 0.0f, 0.10f );      // ~10cm above baselin. Could also be defined as a % of shoulder width or player height
    pHeuristic->SetData( m_HeadHeightBaseLine.GetHeightValues() );
    AddHeuristic( pHeuristic, 0.0f );   // No weight, we'll won't use this as part of the soft probability
    m_pHeadHeightFarAboveBaseLineCumulativeSum = (HeightAboveBaseLineCumulativeSum*)pHeuristic;

    RETURN_ON_NULL( pHeuristic = new HandsAboveHead );
    pHeuristic->Initialize( 0.0f, 0.1f );   // ~10cm above baselin. Could also be defined as a % of shoulder width or player height
    AddHeuristic( pHeuristic, 0.0f );   // No weight, we'll won't use this as part of the soft probability
    m_pHandsAboveHead = (HandsAboveHead*)pHeuristic;

    RETURN_ON_FAIL( m_HeadHeightBaseLine.Initialize( NUI_SKELETON_POSITION_HEAD ) );

    RETURN_ON_FAIL( m_LeftKneeHeightBaseLine.Initialize( NUI_SKELETON_POSITION_KNEE_LEFT, pSkeletonFrame, NUI_SKELETON_POSITION_HIP_LEFT, NUI_SKELETON_POSITION_KNEE_LEFT, NUI_SKELETON_POSITION_ANKLE_LEFT ) );
    RETURN_ON_FAIL( m_RightKneeHeightBaseLine.Initialize( NUI_SKELETON_POSITION_KNEE_RIGHT, pSkeletonFrame, NUI_SKELETON_POSITION_HIP_RIGHT, NUI_SKELETON_POSITION_KNEE_RIGHT, NUI_SKELETON_POSITION_ANKLE_RIGHT ) );
    RETURN_ON_FAIL( m_KneeHeightBaseLine.Initialize( &m_LeftKneeHeightBaseLine, &m_RightKneeHeightBaseLine ) );

    RETURN_ON_FAIL( m_LeftAnkleHeightBaseLine.Initialize( NUI_SKELETON_POSITION_ANKLE_LEFT ) );
    RETURN_ON_FAIL( m_RightAnkleHeightBaseLine.Initialize( NUI_SKELETON_POSITION_ANKLE_RIGHT ) );
    RETURN_ON_FAIL( m_AnkleHeightBaseLine.Initialize( &m_LeftAnkleHeightBaseLine, &m_RightAnkleHeightBaseLine ) );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: JumpDetectionFilter::Update()
// Desc: Initialize filter
//--------------------------------------------------------------------------------------

VOID JumpDetectionFilter::Update( NUI_SKELETON_FRAME* pSkeletonFrame )
{
    // Update base line heights
    m_HeadHeightBaseLine.Update( pSkeletonFrame, m_pHandsAboveHead );
    m_KneeHeightBaseLine.Update( pSkeletonFrame, m_pHandsAboveHead );
    m_AnkleHeightBaseLine.Update( pSkeletonFrame, m_pHandsAboveHead );

    // Offset the base line height if arms are above head
    OffsetHeadBaseLine();

    // Update the filter
    GestureDetectionFilter::Update( pSkeletonFrame );
}


//--------------------------------------------------------------------------------------
// Name: JumpDetectionFilter::CalcProbability()
// Desc: Calculate weighted probability of detection from heuristics
//--------------------------------------------------------------------------------------

FLOAT JumpDetectionFilter::CalcProbability( const UINT uSkeletonIdx ) const
{
    PIXBeginNamedEvent( 0, __FUNCTION__ );

    // Calculate soft weighted probability
    FLOAT fProbability = GestureDetectionFilter::CalcProbability( uSkeletonIdx );

    // Override if one of these are 100% true, but only if fProbability > 0.0f,
    // since we still want to keep the results of the false positives
    if ( fProbability > 0.0f )
    {
        if ( m_pHeadHeightFarAboveBaseLineCumulativeSum->IsValid( uSkeletonIdx ) &&
             m_pHeadHeightFarAboveBaseLineCumulativeSum->GetProbability( uSkeletonIdx ) == 1.0f )
        {
            fProbability = 1.0f;
        }
    }

    PIXEndNamedEvent();

    return fProbability;
}


//--------------------------------------------------------------------------------------
// Name: JumpDetectionFilter::OffsetHeadBaseLine()
// Desc: Offset head base line height if hands are raised above head
//--------------------------------------------------------------------------------------

VOID JumpDetectionFilter::OffsetHeadBaseLine()
{
    // We get many false positives when hands are above the head, since it raises the
    // upper body joint positions. Therefore we dynamically change the the thresholds
    // if the hands are above the head
    for ( INT i = 0; i < NUI_SKELETON_COUNT; i++ )
    {
        if ( m_pHandsAboveHead->IsValid( i ) )
        {
            // Maximum of 10cm raise
            FLOAT fOffset = m_pHandsAboveHead->GetProbability( i ) * c_fMaxOffsetDueToArmsAboveHead;
            m_HeadHeightBaseLine.OffsetHeight( i, fOffset );
        }
    }
}