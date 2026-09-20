//--------------------------------------------------------------------------------------
// GestureDetectionHeuristics.cpp
//
// Heuristics used for gesture detection
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "GestureDetectionHeuristics.h"
#include <AtgUtil.h>

const FLOAT LegsStraightPreviouslyBent::c_fBendThreshold = 100.0f;


//--------------------------------------------------------------------------------------
// Name: Heuristic::Heuristic()
// Desc: Constructor
//--------------------------------------------------------------------------------------

Heuristic::Heuristic( const EType eType )
{
    m_eType     = eType;
    m_fMin      = 0.0f;
    m_fMax      = 0.0f;

    Reset();
}


//--------------------------------------------------------------------------------------
// Name: Heuristic::Initialize()
// Desc: Initialize data
//--------------------------------------------------------------------------------------

HRESULT Heuristic::Initialize( const FLOAT fMin, const FLOAT fMax )
{
    RETURN_ON_FAIL( SetThresholds( fMin, fMax ) );
    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Heuristic::Reset()
// Desc: Reset data
//--------------------------------------------------------------------------------------

VOID Heuristic::Reset()
{
    ZeroMemory( m_fProbability, sizeof( m_fProbability ) );
    for ( UINT i = 0; i < NUI_SKELETON_COUNT; i++ )
    {
        m_bIsValid[ i ] = TRUE;
    }
}


//--------------------------------------------------------------------------------------
// Name: Heuristic::Reset()
// Desc: Reset data per player
//--------------------------------------------------------------------------------------

VOID Heuristic::Reset( const UINT uSkeletonIdx )
{
    m_fProbability[ uSkeletonIdx ] = 0.0f;
    m_bIsValid[ uSkeletonIdx ] = TRUE;
}


//--------------------------------------------------------------------------------------
// Name: Heuristic::SetThresholds()
// Desc: Set thresholds
//--------------------------------------------------------------------------------------

HRESULT Heuristic::SetThresholds( const FLOAT fMin, const FLOAT fMax )
{
    m_fMin  = fMin;
    m_fMax  = fMax;

    if ( ( fMax - fMin ) > 0.0f )
    {
        return E_FAIL;
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: HandsAboveHead::Update
// Desc: Update probability
//--------------------------------------------------------------------------------------

VOID HandsAboveHead::Update( NUI_SKELETON_FRAME* pSkeletonFrame )
{
    PIXBeginNamedEvent( 0, __FUNCTION__ );

    for ( INT i = 0; i < NUI_SKELETON_COUNT; i++ )
    {
        NUI_SKELETON_DATA* pSkeletonData = &pSkeletonFrame->SkeletonData[ i ];

        if ( pSkeletonData->eTrackingState == NUI_SKELETON_TRACKED &&
             pSkeletonData->eSkeletonPositionTrackingState[ NUI_SKELETON_POSITION_HEAD ] != NUI_SKELETON_POSITION_NOT_TRACKED )
        {
            XMVECTOR vHead      = pSkeletonData->SkeletonPositions[ NUI_SKELETON_POSITION_HEAD ];
            XMVECTOR vLeftHand  = pSkeletonData->SkeletonPositions[ NUI_SKELETON_POSITION_WRIST_LEFT ];
            XMVECTOR vRightHand = pSkeletonData->SkeletonPositions[ NUI_SKELETON_POSITION_WRIST_RIGHT ];

            FLOAT fLeftVerticalDistance  = 0.0f;
            FLOAT fRightVerticalDistance = 0.0f;

            // Fine with inferred data
            if ( pSkeletonData->eSkeletonPositionTrackingState[ NUI_SKELETON_POSITION_WRIST_LEFT ] != NUI_SKELETON_POSITION_NOT_TRACKED )
            {
                fLeftVerticalDistance = vLeftHand.y - vHead.y;
            }

            if ( pSkeletonData->eSkeletonPositionTrackingState[ NUI_SKELETON_POSITION_WRIST_RIGHT ] != NUI_SKELETON_POSITION_NOT_TRACKED )
            {
                fRightVerticalDistance = vRightHand.y - vHead.y;
            }
            
            if ( pSkeletonData->eSkeletonPositionTrackingState[ NUI_SKELETON_POSITION_WRIST_LEFT ] == NUI_SKELETON_POSITION_NOT_TRACKED &&
                 pSkeletonData->eSkeletonPositionTrackingState[ NUI_SKELETON_POSITION_WRIST_RIGHT ] == NUI_SKELETON_POSITION_NOT_TRACKED )
            {
                m_fProbability[ i ] = 0.0f;
                m_bIsValid[ i ] = FALSE;
                continue;
            }

            FLOAT fVerticalDistance = max( fLeftVerticalDistance, fRightVerticalDistance );

            // Calculate the probability from m_fMin, m_fMax
            m_fProbability[ i ] = CalcProbability( fVerticalDistance );
            m_bIsValid[ i ] = TRUE;
        }
        else
        {
            m_fProbability[ i ] = 0.0f;
            m_bIsValid[ i ] = FALSE;
        }
    }

    PIXEndNamedEvent();
}


//--------------------------------------------------------------------------------------
// Name: HeightAboveBaseLine::HeightAboveBaseLine()
// Desc: Constructor
//--------------------------------------------------------------------------------------

HeightAboveBaseLine::HeightAboveBaseLine( const NUI_SKELETON_POSITION_INDEX eJoint, const EType eType ) : Heuristic( eType )
{
    m_pHeightBaseLine   = NULL;
    m_eJoint            = eJoint;
}


//--------------------------------------------------------------------------------------
// Name: HeightAboveBaseLine::Update
// Desc: Update probability
//--------------------------------------------------------------------------------------

VOID HeightAboveBaseLine::Update( NUI_SKELETON_FRAME* pSkeletonFrame )
{
    PIXBeginNamedEvent( 0, __FUNCTION__ );

    for ( INT i = 0; i < NUI_SKELETON_COUNT; i++ )
    {
        NUI_SKELETON_DATA* pSkeletonData = &pSkeletonFrame->SkeletonData[ i ];

        // Skeleton has to be tracked, joint needs to have valid data and joint height base line needs to be valid
        if ( pSkeletonData->eTrackingState == NUI_SKELETON_TRACKED &&
             pSkeletonData->eSkeletonPositionTrackingState[ m_eJoint ] != NUI_SKELETON_POSITION_NOT_TRACKED &&
             m_pHeightBaseLine &&
             m_pHeightBaseLine[ i ] != 0.0f )
        {
            // Head is special, we allow it to be inferred when you jump outside the FOV
            if ( m_eJoint != NUI_SKELETON_POSITION_HEAD &&
                 pSkeletonData->eSkeletonPositionTrackingState[ m_eJoint ] != NUI_SKELETON_POSITION_TRACKED )
            {
                m_fProbability[ i ] = 0.0f;
                m_bIsValid[ i ] = FALSE;
                continue;
            }

            FLOAT fHeight               = pSkeletonData->SkeletonPositions[ m_eJoint ].y;
            FLOAT fHeightMinusBaseLine  = fHeight - m_pHeightBaseLine[ i ];

            // Calculate the probability from m_fMin, m_fMax
            m_fProbability[ i ] = CalcProbability( fHeightMinusBaseLine );
            m_bIsValid[ i ] = TRUE;
        }
        else
        {            
            m_fProbability[ i ] = 0.0f;
            m_bIsValid[ i ] = FALSE;
        }
    }

    PIXEndNamedEvent();
}


//--------------------------------------------------------------------------------------
// Name: HeightAboveBaseLineCumulativeSum::HeightAboveBaseLineCumulativeSum()
// Desc: Constructor
//--------------------------------------------------------------------------------------

HeightAboveBaseLineCumulativeSum::HeightAboveBaseLineCumulativeSum( const NUI_SKELETON_POSITION_INDEX eJoint, const EType eType)
                                                                                            : HeightAboveBaseLine( eJoint, eType )
{
    Reset();
}


//--------------------------------------------------------------------------------------
// Name: HeightAboveBaseLineCumulativeSum::Reset()
// Desc: Reset data
//--------------------------------------------------------------------------------------

VOID HeightAboveBaseLineCumulativeSum::Reset()
{
    Heuristic::Reset();

    for ( INT i = 0; i < NUI_SKELETON_COUNT; i++ )
    {
        // set the ringbuffer to it's size and clear with zeros
        m_HistoryFrames[ i ].clear();
        m_HistoryFrames[ i ].resize( c_uRingBufferSize, 0.0f );
    }
}


//--------------------------------------------------------------------------------------
// Name: HeightAboveBaseLineCumulativeSum::Reset()
// Desc: Reset data per player
//--------------------------------------------------------------------------------------

VOID HeightAboveBaseLineCumulativeSum::Reset( const UINT uSkeletonIdx )
{
    Heuristic::Reset( uSkeletonIdx );

    // set the ringbuffer to it's size and clear with zeros
    m_HistoryFrames[ uSkeletonIdx ].clear();
    m_HistoryFrames[ uSkeletonIdx ].resize( c_uRingBufferSize, 0.0f );
}


//--------------------------------------------------------------------------------------
// Name: HeightAboveBaseLineCumulativeSum::Update
// Desc: Update probability
//--------------------------------------------------------------------------------------

VOID HeightAboveBaseLineCumulativeSum::Update( NUI_SKELETON_FRAME* pSkeletonFrame )
{
    PIXBeginNamedEvent( 0, __FUNCTION__ );

    for ( INT i = 0; i < NUI_SKELETON_COUNT; i++ )
    {
        NUI_SKELETON_DATA* pSkeletonData = &pSkeletonFrame->SkeletonData[ i ];

        // Skeleton has to be tracked, joint needs to have valid data and joint height base line needs to be valid
        if ( pSkeletonData->eTrackingState == NUI_SKELETON_TRACKED &&
             pSkeletonData->eSkeletonPositionTrackingState[ m_eJoint ] != NUI_SKELETON_POSITION_NOT_TRACKED &&
             m_pHeightBaseLine &&
             m_pHeightBaseLine[ i ] != 0.0f )
        {
            // Head is special, we allow it to be inferred when you jump outside the FOV
            if ( m_eJoint != NUI_SKELETON_POSITION_HEAD &&
                 pSkeletonData->eSkeletonPositionTrackingState[ m_eJoint ] != NUI_SKELETON_POSITION_TRACKED )
            {
                m_fProbability[ i ] = 0.0f;
                m_bIsValid[ i ] = FALSE;
                continue;
            }

            FLOAT fHeight               = pSkeletonData->SkeletonPositions[ m_eJoint ].y;
            FLOAT fHeightMinusBaseLine  = fHeight - m_pHeightBaseLine[ i ];

            // For the CUSUM we only use positive values
            fHeightMinusBaseLine = max( 0.0f, fHeightMinusBaseLine );

            // pop the oldest value and push the current one
            m_HistoryFrames[ i ].pop_front();
            m_HistoryFrames[ i ].push_back( fHeightMinusBaseLine );

            // calc the CUSUM from data in the ring buffer
            FLOAT fCumulativeSum = 0.0f;
            const UINT uNumElements = m_HistoryFrames[ i ].size();
            for ( UINT j = 0; j < uNumElements; j++ )
            {
                fCumulativeSum += m_HistoryFrames[ i ][ j ];
            }

            // Calculate the probability from m_fMin, m_fMax
            m_fProbability[ i ] = CalcProbability( fCumulativeSum );
            m_bIsValid[ i ] = TRUE;
        }
        else
        {
            m_fProbability[ i ] = 0.0f;
            m_bIsValid[ i ] = FALSE;
        }
    }

    PIXEndNamedEvent();
}


//--------------------------------------------------------------------------------------
// Name: HeightBelowBaseLine::HeightBelowBaseLine()
// Desc: Constructor
//--------------------------------------------------------------------------------------

HeightBelowBaseLine::HeightBelowBaseLine( const NUI_SKELETON_POSITION_INDEX eJoint, const EType eType ) : Heuristic( eType )
{
    m_pHeightBaseLine   = NULL;
    m_eJoint            = eJoint;
}


//--------------------------------------------------------------------------------------
// Name: HeightBelowBaseLine::Update
// Desc: Update probability
//--------------------------------------------------------------------------------------

VOID HeightBelowBaseLine::Update( NUI_SKELETON_FRAME* pSkeletonFrame )
{
    PIXBeginNamedEvent( 0, __FUNCTION__ );

    for ( INT i = 0; i < NUI_SKELETON_COUNT; i++ )
    {
        NUI_SKELETON_DATA* pSkeletonData = &pSkeletonFrame->SkeletonData[ i ];

        // Skeleton has to be tracked, joint needs to have valid data and joint height base line needs to be valid
        if ( pSkeletonData->eTrackingState == NUI_SKELETON_TRACKED &&
             pSkeletonData->eSkeletonPositionTrackingState[ m_eJoint ] == NUI_SKELETON_POSITION_TRACKED &&
             m_pHeightBaseLine &&
             m_pHeightBaseLine[ i ] != 0.0f )
        {
            FLOAT fHeight               = pSkeletonData->SkeletonPositions[ m_eJoint ].y;
            FLOAT fBaseLineMinusHeight  = m_pHeightBaseLine[ i ] - fHeight;

            // Calculate the probability from m_fMin, m_fMax
            m_fProbability[ i ] = CalcProbability( fBaseLineMinusHeight );
            m_bIsValid[ i ] = TRUE;
        }
        else
        {
            m_fProbability[ i ] = 0.0f;
            m_bIsValid[ i ] = FALSE;
        }
    }

    PIXEndNamedEvent();
}


//--------------------------------------------------------------------------------------
// Name: HeightBelowBaseLineCumulativeSum::HeightBelowBaseLineCumulativeSum()
// Desc: Constructor
//--------------------------------------------------------------------------------------

HeightBelowBaseLineCumulativeSum::HeightBelowBaseLineCumulativeSum( const NUI_SKELETON_POSITION_INDEX eJoint, const EType eType)
                                                                                            : HeightBelowBaseLine( eJoint, eType )
{
    Reset();
}


//--------------------------------------------------------------------------------------
// Name: HeightBelowBaseLineCumulativeSum::Reset()
// Desc: Reset data
//--------------------------------------------------------------------------------------

VOID HeightBelowBaseLineCumulativeSum::Reset()
{
    Heuristic::Reset();

    for ( INT i = 0; i < NUI_SKELETON_COUNT; i++ )
    {
        // set the ringbuffer to it's size and clear with zeros
        m_HistoryFrames[ i ].clear();
        m_HistoryFrames[ i ].resize( c_uRingBufferSize, 0.0f );
    }
}


//--------------------------------------------------------------------------------------
// Name: HeightBelowBaseLineCumulativeSum::Reset()
// Desc: Reset data per player
//--------------------------------------------------------------------------------------

VOID HeightBelowBaseLineCumulativeSum::Reset( const UINT uSkeletonIdx )
{
    Heuristic::Reset( uSkeletonIdx );

    // set the ringbuffer to it's size and clear with zeros
    m_HistoryFrames[ uSkeletonIdx ].clear();
    m_HistoryFrames[ uSkeletonIdx ].resize( c_uRingBufferSize, 0.0f );
}


//--------------------------------------------------------------------------------------
// Name: HeightBelowBaseLineCumulativeSum::Update
// Desc: Update probability
//--------------------------------------------------------------------------------------

VOID HeightBelowBaseLineCumulativeSum::Update( NUI_SKELETON_FRAME* pSkeletonFrame )
{
    PIXBeginNamedEvent( 0, __FUNCTION__ );

    for ( INT i = 0; i < NUI_SKELETON_COUNT; i++ )
    {
        NUI_SKELETON_DATA* pSkeletonData = &pSkeletonFrame->SkeletonData[ i ];

        // Skeleton has to be tracked, joint needs to have valid data and joint height base line needs to be valid
        if ( pSkeletonData->eTrackingState == NUI_SKELETON_TRACKED &&
             pSkeletonData->eSkeletonPositionTrackingState[ m_eJoint ] == NUI_SKELETON_POSITION_TRACKED &&
             m_pHeightBaseLine &&
             m_pHeightBaseLine[ i ] != 0.0f )
        {
            FLOAT fHeight               = pSkeletonData->SkeletonPositions[ m_eJoint ].y;
            FLOAT fBaseLineMinusHeight  = m_pHeightBaseLine[ i ] - fHeight;

            // For the CUSUM we only use positive values
            fBaseLineMinusHeight = max( 0.0f, fBaseLineMinusHeight );

            // pop the oldest value and push the current one
            m_HistoryFrames[ i ].pop_front();
            m_HistoryFrames[ i ].push_back( fBaseLineMinusHeight );

            // calc the CUSUM from data in the ring buffer
            FLOAT fCumulativeSum = 0.0f;
            const UINT uNumElements = m_HistoryFrames[ i ].size();
            for ( UINT j = 0; j < uNumElements; j++ )
            {
                fCumulativeSum += m_HistoryFrames[ i ][ j ];
            }

            // Calculate the probability from m_fMin, m_fMax
            m_fProbability[ i ] = CalcProbability( fCumulativeSum );

            m_bIsValid[ i ] = TRUE;
        }
        else
        {
            m_fProbability[ i ] = 0.0f;
            m_bIsValid[ i ] = FALSE;
        }
    }

    PIXEndNamedEvent();
}


//--------------------------------------------------------------------------------------
// Name: MovingUpwards::MovingUpwards()
// Desc: Constructor
//--------------------------------------------------------------------------------------

MovingUpwards::MovingUpwards( const NUI_SKELETON_POSITION_INDEX eJoint, const EType eType ) : Heuristic( eType )
{
    m_eJoint = eJoint;
    Reset();
}


//--------------------------------------------------------------------------------------
// Name: MovingUpwards::Reset()
// Desc: Reset data
//--------------------------------------------------------------------------------------

VOID MovingUpwards::Reset()
{
    Heuristic::Reset();
    ZeroMemory( m_fPreviousHeight, sizeof( m_fPreviousHeight ) );

    for ( INT i = 0; i < NUI_SKELETON_COUNT; i++ )
    {
        m_PreviousProbability[ i ].clear();
        m_PreviousProbability[ i ].resize( m_uNumPreviousProbability, 0.0f );
    }
}


//--------------------------------------------------------------------------------------
// Name: MovingUpwards::Reset()
// Desc: Reset data per player
//--------------------------------------------------------------------------------------

VOID MovingUpwards::Reset( const UINT uSkeletonIdx )
{
    Heuristic::Reset( uSkeletonIdx );

    // set the ringbuffer to it's size and clear with zeros
    m_PreviousProbability[ uSkeletonIdx ].clear();
    m_PreviousProbability[ uSkeletonIdx ].resize( m_uNumPreviousProbability, 0.0f );
}


//--------------------------------------------------------------------------------------
// Name: MovingUpwards::Update
// Desc: Update probability
//--------------------------------------------------------------------------------------

VOID MovingUpwards::Update( NUI_SKELETON_FRAME* pSkeletonFrame )
{
    PIXBeginNamedEvent( 0, __FUNCTION__ );

    for ( INT i = 0; i < NUI_SKELETON_COUNT; i++ )
    {
        NUI_SKELETON_DATA* pSkeletonData = &pSkeletonFrame->SkeletonData[ i ];

        // Skeleton has to be tracked and joint needs to have valid data
        if ( pSkeletonData->eTrackingState == NUI_SKELETON_TRACKED &&
             pSkeletonData->eSkeletonPositionTrackingState[ m_eJoint ] == NUI_SKELETON_POSITION_TRACKED )
        {
            FLOAT fHeight = pSkeletonData->SkeletonPositions[ m_eJoint ].y;

            // if this is the very first time after a reset, just set it to the current value
            if ( m_fPreviousHeight[ i ] == 0.0f )
            {
                m_fPreviousHeight[ i ] = fHeight;
            }
            
            FLOAT fHeightMinusPrevious = fHeight - m_fPreviousHeight[ i ];

            // Calculate the probability from m_fMin, m_fMax
            FLOAT fProbability = CalcProbability( fHeightMinusPrevious );

            m_PreviousProbability[ i ].pop_front();
            m_PreviousProbability[ i ].push_back( fProbability );

            // Delay a few frames to make sure false positives don't cancel out
            FLOAT fMax = 0.0f;
            const UINT uNumPreviousProbability = m_PreviousProbability[ i ].size();
            for ( UINT j = 0; j < uNumPreviousProbability; j++ )
            {
                fMax = max( fMax, m_PreviousProbability[ i ][ j ] );
            }

            m_fProbability[ i ] = fMax;

            m_bIsValid[ i ] = TRUE;

            // Update for next frame
            m_fPreviousHeight[ i ] = fHeight;
        }
        else
        {            
            m_fProbability[ i ] = 0.0f;
            m_bIsValid[ i ] = FALSE;
        }
    }

    PIXEndNamedEvent();
}


//--------------------------------------------------------------------------------------
// Name: MovingDownwards::MovingDownwards()
// Desc: Constructor
//--------------------------------------------------------------------------------------

MovingDownwards::MovingDownwards( const NUI_SKELETON_POSITION_INDEX eJoint, const EType eType ) : Heuristic( eType )
{
    m_eJoint = eJoint;
    Reset();
}


//--------------------------------------------------------------------------------------
// Name: MovingDownwards::Reset()
// Desc: Reset data
//--------------------------------------------------------------------------------------

VOID MovingDownwards::Reset()
{
    Heuristic::Reset();
    ZeroMemory( m_fPreviousHeight, sizeof( m_fPreviousHeight ) );

    for ( INT i = 0; i < NUI_SKELETON_COUNT; i++ )
    {
        m_PreviousProbability[ i ].clear();
        m_PreviousProbability[ i ].resize( m_uNumPreviousProbability, 0.0f );
    }

}


//--------------------------------------------------------------------------------------
// Name: MovingDownwards::Reset()
// Desc: Reset data per player
//--------------------------------------------------------------------------------------

VOID MovingDownwards::Reset( const UINT uSkeletonIdx )
{
    Heuristic::Reset( uSkeletonIdx );

    // set the ringbuffer to it's size and clear with zeros
    m_PreviousProbability[ uSkeletonIdx ].clear();
    m_PreviousProbability[ uSkeletonIdx ].resize( m_uNumPreviousProbability, 0.0f );
}


//--------------------------------------------------------------------------------------
// Name: MovingDownwards::Update
// Desc: Update probability
//--------------------------------------------------------------------------------------

VOID MovingDownwards::Update( NUI_SKELETON_FRAME* pSkeletonFrame )
{
    PIXBeginNamedEvent( 0, __FUNCTION__ );

    for ( INT i = 0; i < NUI_SKELETON_COUNT; i++ )
    {
        NUI_SKELETON_DATA* pSkeletonData = &pSkeletonFrame->SkeletonData[ i ];

        // Skeleton has to be tracked and joint needs to have valid data
        if ( pSkeletonData->eTrackingState == NUI_SKELETON_TRACKED &&
             pSkeletonData->eSkeletonPositionTrackingState[ m_eJoint ] == NUI_SKELETON_POSITION_TRACKED )
        {
            FLOAT fHeight = pSkeletonData->SkeletonPositions[ m_eJoint ].y;

            // if this is the very first time after a reset, just set it to the current value
            if ( m_fPreviousHeight[ i ] == 0.0f )
            {
                m_fPreviousHeight[ i ] = fHeight;
            }
            
            FLOAT fHeightMinusPrevious = fHeight - m_fPreviousHeight[ i ];

            // Calculate the probability from m_fMin, m_fMax
            FLOAT fProbability = CalcProbability( -fHeightMinusPrevious );

            m_PreviousProbability[ i ].pop_front();
            m_PreviousProbability[ i ].push_back( fProbability );

            // Delay a few frames to make sure false positives don't cancel out
            FLOAT fMax = 0.0f;
            const UINT uNumPreviousProbability = m_PreviousProbability[ i ].size();
            for ( UINT j = 0; j < uNumPreviousProbability; j++ )
            {
                fMax = max( fMax, m_PreviousProbability[ i ][ j ] );
            }

            m_fProbability[ i ] = fMax;

            m_bIsValid[ i ] = TRUE;

            // Update for next frame
            m_fPreviousHeight[ i ] = fHeight;
        }
        else
        {            
            m_fProbability[ i ] = 0.0f;
            m_bIsValid[ i ] = FALSE;
        }
    }

    PIXEndNamedEvent();
}


//--------------------------------------------------------------------------------------
// Name: BodyFaceUpwards::Update
// Desc: Update probability
//--------------------------------------------------------------------------------------

VOID BodyFaceUpwards::Update( NUI_SKELETON_FRAME* pSkeletonFrame )
{
    PIXBeginNamedEvent( 0, __FUNCTION__ );

    for ( INT i = 0; i < NUI_SKELETON_COUNT; i++ )
    {
        NUI_SKELETON_DATA* pSkeletonData = &pSkeletonFrame->SkeletonData[ i ];

        if ( pSkeletonData->eTrackingState == NUI_SKELETON_TRACKED )
        {
            // Create 4 body direction vectors, two from shoulders and two from hips
            XMVECTOR vNeckPos           = pSkeletonData->SkeletonPositions[ NUI_SKELETON_POSITION_SHOULDER_CENTER ];
            XMVECTOR vLeftShoulderPos   = pSkeletonData->SkeletonPositions[ NUI_SKELETON_POSITION_SHOULDER_LEFT ];
            XMVECTOR vRightShoulderPos  = pSkeletonData->SkeletonPositions[ NUI_SKELETON_POSITION_SHOULDER_RIGHT ];
            XMVECTOR vHipCenter         = pSkeletonData->SkeletonPositions[ NUI_SKELETON_POSITION_HIP_CENTER ];
            XMVECTOR vLeftHipPos        = pSkeletonData->SkeletonPositions[ NUI_SKELETON_POSITION_HIP_LEFT ];
            XMVECTOR vRightHipPos       = pSkeletonData->SkeletonPositions[ NUI_SKELETON_POSITION_HIP_RIGHT ];

            // Calculate the 4 direction vectors
            XMVECTOR vLeftShoulderNormal    = XMVector3Normalize( XMVector3Cross( vLeftShoulderPos - vNeckPos, vHipCenter - vNeckPos ) );
            XMVECTOR vRightShoulderNormal   = XMVector3Normalize( XMVector3Cross( vHipCenter - vNeckPos, vRightShoulderPos - vNeckPos ) );
            XMVECTOR vLeftHipNormal         = XMVector3Normalize( XMVector3Cross( XMVector3Normalize( vNeckPos - vHipCenter ), XMVector3Normalize( vLeftHipPos - vHipCenter ) ) );
            XMVECTOR vRightHipNormal        = XMVector3Normalize( XMVector3Cross( XMVector3Normalize( vRightHipPos - vHipCenter ), XMVector3Normalize( vNeckPos - vHipCenter ) ) );

            // Get the tracking states for shoulders and hips
            NUI_SKELETON_POSITION_TRACKING_STATE leftShoulderState  = pSkeletonData->eSkeletonPositionTrackingState[ NUI_SKELETON_POSITION_SHOULDER_LEFT ];
            NUI_SKELETON_POSITION_TRACKING_STATE rightShoulderState = pSkeletonData->eSkeletonPositionTrackingState[ NUI_SKELETON_POSITION_SHOULDER_RIGHT ];
            NUI_SKELETON_POSITION_TRACKING_STATE leftHipState       = pSkeletonData->eSkeletonPositionTrackingState[ NUI_SKELETON_POSITION_HIP_LEFT ];
            NUI_SKELETON_POSITION_TRACKING_STATE rightHipState      = pSkeletonData->eSkeletonPositionTrackingState[ NUI_SKELETON_POSITION_HIP_RIGHT ];

            // Calculate weights from the tracking states, so that when we combine the left and right,
            // we weight angles from tracked positions higher than inferred positions
            FLOAT fWeights[ 4 ] = { ( leftShoulderState == NUI_SKELETON_POSITION_TRACKED ) ? 1.0f :
                                        ( leftShoulderState == NUI_SKELETON_POSITION_INFERRED ) ? 0.25f : 0.0f,
                                    ( rightShoulderState == NUI_SKELETON_POSITION_TRACKED ) ? 1.0f :
                                        ( rightShoulderState == NUI_SKELETON_POSITION_INFERRED ) ? 0.25f : 0.0f,
                                    ( leftHipState == NUI_SKELETON_POSITION_TRACKED ) ? 1.0f :
                                        ( leftHipState == NUI_SKELETON_POSITION_INFERRED ) ? 0.25f : 0.0f,
                                    ( rightHipState == NUI_SKELETON_POSITION_TRACKED ) ? 1.0f :
                                        ( rightHipState == NUI_SKELETON_POSITION_INFERRED ) ? 0.25f : 0.0f };

            // Get the totals of the weights so that we can normalize the averages
            FLOAT fTotalWeights = fWeights[ 0 ] + fWeights[ 1 ] + fWeights[ 2 ] + fWeights[ 3 ];

            if ( fTotalWeights == 0.0f )
            {
                m_fProbability[ i ] = 0.0f;
                m_bIsValid[ i ] = FALSE;
                continue;
            }

            // Calculate shoulder and hip rotations
            FLOAT fLeftShoulderAngle    = XMConvertToDegrees( atan2( vLeftShoulderNormal.y, vLeftShoulderNormal.z ) );
            FLOAT fRightShoulderAngle   = XMConvertToDegrees( atan2( vRightShoulderNormal.y, vRightShoulderNormal.z ) );
            FLOAT fLeftHipAngle         = XMConvertToDegrees( atan2( vLeftHipNormal.y, vLeftHipNormal.z ) );
            FLOAT fRightHipAngle        = XMConvertToDegrees( atan2( vRightHipNormal.y, vRightHipNormal.z ) );

            // Check for 180 degree flips
            fLeftShoulderAngle          = fLeftShoulderAngle > 90.0f ? ( fLeftShoulderAngle - 180.0f ) : fLeftShoulderAngle;
            fRightShoulderAngle         = fRightShoulderAngle > 90.0f ? ( fRightShoulderAngle - 180.0f ) : fRightShoulderAngle;
            fLeftHipAngle               = fLeftHipAngle > 90.0f ? ( fLeftHipAngle - 180.0f ) : fLeftHipAngle;
            fRightHipAngle              = fRightHipAngle > 90.0f ? ( fRightHipAngle - 180.0f ) : fRightHipAngle;

            FLOAT fAngle = ( ( fLeftShoulderAngle * fWeights[ 0 ] ) + ( fRightShoulderAngle * fWeights[ 1 ] ) ) +
                           ( ( fLeftHipAngle * fWeights[ 2 ] ) + ( fRightHipAngle * fWeights[ 3 ] ) );

            fAngle /= fTotalWeights;

            // Calculate the probability from m_fMin, m_fMax
            m_fProbability[ i ] = CalcProbability( -fAngle );
            m_bIsValid[ i ] = TRUE;
        }
        else
        {
            m_fProbability[ i ] = 0.0f;
            m_bIsValid[ i ] = FALSE;
        }
    }

    PIXEndNamedEvent();
}


//--------------------------------------------------------------------------------------
// Name: BodyFaceDownwards::Update
// Desc: Update probability
//--------------------------------------------------------------------------------------

VOID BodyFaceDownwards::Update( NUI_SKELETON_FRAME* pSkeletonFrame )
{
    PIXBeginNamedEvent( 0, __FUNCTION__ );

    for ( INT i = 0; i < NUI_SKELETON_COUNT; i++ )
    {
        NUI_SKELETON_DATA* pSkeletonData = &pSkeletonFrame->SkeletonData[ i ];

        if ( pSkeletonData->eTrackingState == NUI_SKELETON_TRACKED )
        {
            // Create 4 body direction vectors, two from shoulders and two from hips
            XMVECTOR vNeckPos           = pSkeletonData->SkeletonPositions[ NUI_SKELETON_POSITION_SHOULDER_CENTER ];
            XMVECTOR vLeftShoulderPos   = pSkeletonData->SkeletonPositions[ NUI_SKELETON_POSITION_SHOULDER_LEFT ];
            XMVECTOR vRightShoulderPos  = pSkeletonData->SkeletonPositions[ NUI_SKELETON_POSITION_SHOULDER_RIGHT ];
            XMVECTOR vHipCenter         = pSkeletonData->SkeletonPositions[ NUI_SKELETON_POSITION_HIP_CENTER ];
            XMVECTOR vLeftHipPos        = pSkeletonData->SkeletonPositions[ NUI_SKELETON_POSITION_HIP_LEFT ];
            XMVECTOR vRightHipPos       = pSkeletonData->SkeletonPositions[ NUI_SKELETON_POSITION_HIP_RIGHT ];

            // Calculate the 4 direction vectors
            XMVECTOR vLeftShoulderNormal    = XMVector3Normalize( XMVector3Cross( vLeftShoulderPos - vNeckPos, vHipCenter - vNeckPos ) );
            XMVECTOR vRightShoulderNormal   = XMVector3Normalize( XMVector3Cross( vHipCenter - vNeckPos, vRightShoulderPos - vNeckPos ) );
            XMVECTOR vLeftHipNormal         = XMVector3Normalize( XMVector3Cross( XMVector3Normalize( vNeckPos - vHipCenter ), XMVector3Normalize( vLeftHipPos - vHipCenter ) ) );
            XMVECTOR vRightHipNormal        = XMVector3Normalize( XMVector3Cross( XMVector3Normalize( vRightHipPos - vHipCenter ), XMVector3Normalize( vNeckPos - vHipCenter ) ) );

            // Get the tracking states for shoulders and hips
            NUI_SKELETON_POSITION_TRACKING_STATE leftShoulderState  = pSkeletonData->eSkeletonPositionTrackingState[ NUI_SKELETON_POSITION_SHOULDER_LEFT ];
            NUI_SKELETON_POSITION_TRACKING_STATE rightShoulderState = pSkeletonData->eSkeletonPositionTrackingState[ NUI_SKELETON_POSITION_SHOULDER_RIGHT ];
            NUI_SKELETON_POSITION_TRACKING_STATE leftHipState       = pSkeletonData->eSkeletonPositionTrackingState[ NUI_SKELETON_POSITION_HIP_LEFT ];
            NUI_SKELETON_POSITION_TRACKING_STATE rightHipState      = pSkeletonData->eSkeletonPositionTrackingState[ NUI_SKELETON_POSITION_HIP_RIGHT ];

            // Calculate weights from the tracking states, so that when we combine the left and right,
            // we weight angles from tracked positions higher than inferred positions
            FLOAT fWeights[ 4 ] = { ( leftShoulderState == NUI_SKELETON_POSITION_TRACKED ) ? 1.0f :
                                        ( leftShoulderState == NUI_SKELETON_POSITION_INFERRED ) ? 0.25f : 0.0f,
                                    ( rightShoulderState == NUI_SKELETON_POSITION_TRACKED ) ? 1.0f :
                                        ( rightShoulderState == NUI_SKELETON_POSITION_INFERRED ) ? 0.25f : 0.0f,
                                    ( leftHipState == NUI_SKELETON_POSITION_TRACKED ) ? 1.0f :
                                        ( leftHipState == NUI_SKELETON_POSITION_INFERRED ) ? 0.25f : 0.0f,
                                    ( rightHipState == NUI_SKELETON_POSITION_TRACKED ) ? 1.0f :
                                        ( rightHipState == NUI_SKELETON_POSITION_INFERRED ) ? 0.25f : 0.0f };

            // Get the totals of the weights so that we can normalize the averages
            FLOAT fTotalWeights = fWeights[ 0 ] + fWeights[ 1 ] + fWeights[ 2 ] + fWeights[ 3 ];

            if ( fTotalWeights == 0.0f )
            {
                m_fProbability[ i ] = 0.0f;
                m_bIsValid[ i ] = FALSE;
                continue;
            }

            // Calculate shoulder and hip rotations
            FLOAT fLeftShoulderAngle    = XMConvertToDegrees( atan2( vLeftShoulderNormal.y, vLeftShoulderNormal.z ) );
            FLOAT fRightShoulderAngle   = XMConvertToDegrees( atan2( vRightShoulderNormal.y, vRightShoulderNormal.z ) );
            FLOAT fLeftHipAngle         = XMConvertToDegrees( atan2( vLeftHipNormal.y, vLeftHipNormal.z ) );
            FLOAT fRightHipAngle        = XMConvertToDegrees( atan2( vRightHipNormal.y, vRightHipNormal.z ) );

            // Check for 180 degree flips
            fLeftShoulderAngle          = fLeftShoulderAngle > 90.0f ? ( fLeftShoulderAngle - 180.0f ) : fLeftShoulderAngle;
            fRightShoulderAngle         = fRightShoulderAngle > 90.0f ? ( fRightShoulderAngle - 180.0f ) : fRightShoulderAngle;
            fLeftHipAngle               = fLeftHipAngle > 90.0f ? ( fLeftHipAngle - 180.0f ) : fLeftHipAngle;
            fRightHipAngle              = fRightHipAngle > 90.0f ? ( fRightHipAngle - 180.0f ) : fRightHipAngle;

            FLOAT fAngle = ( ( fLeftShoulderAngle * fWeights[ 0 ] ) + ( fRightShoulderAngle * fWeights[ 1 ] ) ) +
                           ( ( fLeftHipAngle * fWeights[ 2 ] ) + ( fRightHipAngle * fWeights[ 3 ] ) );

            fAngle /= fTotalWeights;

            // Calculate the probability from m_fMin, m_fMax
            m_fProbability[ i ] = CalcProbability( fAngle );
            m_bIsValid[ i ] = TRUE;
        }
        else
        {
            m_fProbability[ i ] = 0.0f;
            m_bIsValid[ i ] = FALSE;
        }
    }

    PIXEndNamedEvent();
}


//--------------------------------------------------------------------------------------
// Name: UpperBodyAngleTowardsLowerBody::Update
// Desc: Update probability
//--------------------------------------------------------------------------------------

VOID UpperBodyAngleTowardsLowerBody::Update( NUI_SKELETON_FRAME* pSkeletonFrame )
{
    PIXBeginNamedEvent( 0, __FUNCTION__ );

    for ( INT i = 0; i < NUI_SKELETON_COUNT; i++ )
    {
        NUI_SKELETON_DATA* pSkeletonData = &pSkeletonFrame->SkeletonData[ i ];

        // Check that we have valid data
        if ( pSkeletonData->eTrackingState == NUI_SKELETON_TRACKED &&
             pSkeletonData->eSkeletonPositionTrackingState[ NUI_SKELETON_POSITION_SPINE ] == NUI_SKELETON_POSITION_TRACKED &&
             pSkeletonData->eSkeletonPositionTrackingState[ NUI_SKELETON_POSITION_SHOULDER_CENTER ] == NUI_SKELETON_POSITION_TRACKED )
        {
            XMVECTOR vSpine     = pSkeletonData->SkeletonPositions[ NUI_SKELETON_POSITION_SPINE ];
            XMVECTOR vNeck      = pSkeletonData->SkeletonPositions[ NUI_SKELETON_POSITION_SHOULDER_CENTER ];
            XMVECTOR vLeftKnee  = pSkeletonData->SkeletonPositions[ NUI_SKELETON_POSITION_KNEE_LEFT ];
            XMVECTOR vLeftHip   = pSkeletonData->SkeletonPositions[ NUI_SKELETON_POSITION_HIP_LEFT ];
            XMVECTOR vRightKnee = pSkeletonData->SkeletonPositions[ NUI_SKELETON_POSITION_KNEE_RIGHT ];
            XMVECTOR vRightHip  = pSkeletonData->SkeletonPositions[ NUI_SKELETON_POSITION_HIP_RIGHT ];

            // Calc normalized vector from spine to neck
            XMVECTOR vSpineToNeck = XMVector3Normalize( vNeck - vSpine );

            // Set initial values to something really big to indicate if we don't have valid data
            FLOAT fLeftAngle    = 360.0f;
            FLOAT fRightAngle   = 360.0f;

            if ( pSkeletonData->eSkeletonPositionTrackingState[ NUI_SKELETON_POSITION_HIP_LEFT ] == NUI_SKELETON_POSITION_TRACKED &&
                 pSkeletonData->eSkeletonPositionTrackingState[ NUI_SKELETON_POSITION_KNEE_LEFT ] == NUI_SKELETON_POSITION_TRACKED )
            {
                // Calc normalized vector from left knee to hip in spine space
                XMVECTOR vHipToKnee = XMVector3Normalize( vLeftKnee - vLeftHip );

                // Calculate angles between vectors
                XMVECTOR vAngle  = XMVector3AngleBetweenNormals( vSpineToNeck, vHipToKnee );

                // sanity check
                assert( !XMVector4IsNaN( vAngle ) );
                assert( !XMVector4IsInfinite( vAngle ) );

                fLeftAngle = 180.0f - XMConvertToDegrees( XMVectorGetX( vAngle ) );
            }


            if ( pSkeletonData->eSkeletonPositionTrackingState[ NUI_SKELETON_POSITION_HIP_RIGHT ] == NUI_SKELETON_POSITION_TRACKED &&
                 pSkeletonData->eSkeletonPositionTrackingState[ NUI_SKELETON_POSITION_KNEE_RIGHT ] == NUI_SKELETON_POSITION_TRACKED )
            {
                // Calc normalized vector from right knee to hip in spine space
                XMVECTOR vHipToKnee = XMVector3Normalize( vRightKnee - vRightHip );

                // Calculate angles between vectors
                XMVECTOR vAngle  = XMVector3AngleBetweenNormals( vSpineToNeck, vHipToKnee );

                // sanity check
                assert( !XMVector4IsNaN( vAngle ) );
                assert( !XMVector4IsInfinite( vAngle ) );

                fRightAngle = 180.0f - XMConvertToDegrees( XMVectorGetX( vAngle ) );
            }

            // Use mininimum of angles, otherwise you get false positives, e.g. walking
            FLOAT fAngle = min( fLeftAngle, fRightAngle );

            // Check for invalid angles if both legs failed
            if (fAngle > 180.0f)
            {
                m_fProbability[ i ] = 0.0f;
                m_bIsValid[ i ] = FALSE;
            }
            else
            {
                // Calculate the probability from m_fMin, m_fMax
                m_fProbability[ i ] = CalcProbability( fAngle );
                m_bIsValid[ i ] = TRUE;
            }
        }
        else
        {
            m_fProbability[ i ] = 0.0f;
            m_bIsValid[ i ] = FALSE;
        }
    }

    PIXEndNamedEvent();
}


//--------------------------------------------------------------------------------------
// Name: LegsStraightPreviouslyBent::LegsStraightPreviouslyBent()
// Desc: Constructor
//--------------------------------------------------------------------------------------

LegsStraightPreviouslyBent::LegsStraightPreviouslyBent( const EType eType) : Heuristic( eType )
{
    Reset();
}


//--------------------------------------------------------------------------------------
// Name: LegsStraightPreviouslyBent::Reset()
// Desc: Reset data
//--------------------------------------------------------------------------------------

VOID LegsStraightPreviouslyBent::Reset()
{
    Heuristic::Reset();

    for ( INT i = 0; i < NUI_SKELETON_COUNT; i++ )
    {
        // set the queue to it's size and clear with 0 degrees
        m_PreviousFrames[ i ].clear();
        m_PreviousFrames[ i ].resize( m_uNumPreviousFrames, 0.0f );

        m_PreviousProbability[ i ].clear();
    }
}


//--------------------------------------------------------------------------------------
// Name: LegsStraightPreviouslyBent::Reset()
// Desc: Reset data per player
//--------------------------------------------------------------------------------------

VOID LegsStraightPreviouslyBent::Reset( const UINT uSkeletonIdx )
{
    Heuristic::Reset( uSkeletonIdx );

    // set the queue to it's size and clear with 0 degrees
    m_PreviousFrames[ uSkeletonIdx ].clear();
    m_PreviousFrames[ uSkeletonIdx ].resize( m_uNumPreviousFrames, 0.0f );

    m_PreviousProbability[ uSkeletonIdx ].clear();
}


//--------------------------------------------------------------------------------------
// Name: LegsStraightPreviouslyBent::Update
// Desc: Update probability
//--------------------------------------------------------------------------------------

VOID LegsStraightPreviouslyBent::Update( NUI_SKELETON_FRAME* pSkeletonFrame )
{
    PIXBeginNamedEvent( 0, __FUNCTION__ );

    for ( INT i = 0; i < NUI_SKELETON_COUNT; i++ )
    {
        NUI_SKELETON_DATA* pSkeletonData = &pSkeletonFrame->SkeletonData[ i ];

        // Check that we have valid data
        if ( pSkeletonData->eTrackingState == NUI_SKELETON_TRACKED )
        {
            XMVECTOR vLeftHip   = pSkeletonData->SkeletonPositions[ NUI_SKELETON_POSITION_HIP_LEFT ];
            XMVECTOR vLeftKnee  = pSkeletonData->SkeletonPositions[ NUI_SKELETON_POSITION_KNEE_LEFT ];
            XMVECTOR vLeftAnkle = pSkeletonData->SkeletonPositions[ NUI_SKELETON_POSITION_ANKLE_LEFT ];
            
            XMVECTOR vRightHip  = pSkeletonData->SkeletonPositions[ NUI_SKELETON_POSITION_HIP_RIGHT ];
            XMVECTOR vRightKnee = pSkeletonData->SkeletonPositions[ NUI_SKELETON_POSITION_KNEE_RIGHT ];
            XMVECTOR vRightAnkle= pSkeletonData->SkeletonPositions[ NUI_SKELETON_POSITION_ANKLE_RIGHT ];
            
            FLOAT fLeftAngle    = 0.0f;
            FLOAT fRightAngle   = 0.0f;

            if ( pSkeletonData->eSkeletonPositionTrackingState[ NUI_SKELETON_POSITION_HIP_LEFT ] == NUI_SKELETON_POSITION_TRACKED &&
                 pSkeletonData->eSkeletonPositionTrackingState[ NUI_SKELETON_POSITION_KNEE_LEFT ] == NUI_SKELETON_POSITION_TRACKED &&
                 pSkeletonData->eSkeletonPositionTrackingState[ NUI_SKELETON_POSITION_ANKLE_LEFT ] == NUI_SKELETON_POSITION_TRACKED )
            {
                // Calc normalized vector from left knee to hip, and knee to ankle
                XMVECTOR vKneeToHip     = XMVector3Normalize( vLeftHip - vLeftKnee );
                XMVECTOR vKneeToAnkle   = XMVector3Normalize( vLeftAnkle - vLeftKnee );

                // Calculate angles between vectors
                XMVECTOR vAngle = XMVector3AngleBetweenNormals( vKneeToHip, vKneeToAnkle );

                // sanity check
                assert( !XMVector4IsNaN( vAngle ) );
                assert( !XMVector4IsInfinite( vAngle ) );

                fLeftAngle = XMConvertToDegrees( XMVectorGetX( vAngle ) );
            }


            if ( pSkeletonData->eSkeletonPositionTrackingState[ NUI_SKELETON_POSITION_HIP_RIGHT ] == NUI_SKELETON_POSITION_TRACKED &&
                 pSkeletonData->eSkeletonPositionTrackingState[ NUI_SKELETON_POSITION_KNEE_RIGHT ] == NUI_SKELETON_POSITION_TRACKED &&
                 pSkeletonData->eSkeletonPositionTrackingState[ NUI_SKELETON_POSITION_ANKLE_RIGHT ] == NUI_SKELETON_POSITION_TRACKED )
            {
                // Calc normalized vector from right knee to hip, and knee to ankle
                XMVECTOR vKneeToHip     = XMVector3Normalize( vRightHip - vRightKnee );
                XMVECTOR vKneeToAnkle   = XMVector3Normalize( vRightAnkle - vRightKnee );

                // Calculate angles between vectors
                XMVECTOR vAngle = XMVector3AngleBetweenNormals( vKneeToHip, vKneeToAnkle );

                // sanity check
                assert( !XMVector4IsNaN( vAngle ) );
                assert( !XMVector4IsInfinite( vAngle ) );

                fRightAngle = XMConvertToDegrees( XMVectorGetX( vAngle ) );
            }

            // Use maximum of angles, otherwise you get false positives, e.g. walking
            FLOAT fAngle = max( fLeftAngle, fRightAngle );

            // update queue with current leg angle
            m_PreviousFrames[ i ].pop_front();
            m_PreviousFrames[ i ].push_back( fAngle );

            // determine if angle is moving towards 180 degrees
            FLOAT fProbabilityStraightening = 0.0f;
            FLOAT fMin = m_PreviousFrames[ i ][ 0 ];
            FLOAT fMax = m_PreviousFrames[ i ][ 0 ];
            const UINT uNumFrames = m_PreviousFrames[ i ].size();
            const FLOAT fProbabilityAccumulation = 1.0f / ( uNumFrames - 1 );
            const FLOAT fFirstPrevious = m_PreviousFrames[ i ][ 0 ];

            for ( UINT j = 1; j < uNumFrames; j++ )
            {
                assert( ( j - 1 ) >= 0 );

                FLOAT fPrevious = m_PreviousFrames[ i ][ j ];

                if ( fPrevious >= fFirstPrevious &&
                     fPrevious >= m_PreviousFrames[ i ][ j - 1 ] )
                {
                    fProbabilityStraightening += fProbabilityAccumulation;
                }

                fMin = min( fMin, fPrevious );
                fMax = max( fMax, fPrevious );
            }

            FLOAT fDeviation = fMax - fMin;

            // Probability that leg is straight currently
            FLOAT fProbabilityStraight = min( 1.0f, max( 0.0f, fAngle - c_fBendThreshold ) / ( 180.0f - c_fBendThreshold ) );

            FLOAT fProbabilityEnoughDeviation = CalcProbability( fDeviation );

            // Calculate the probability from m_fMin, m_fMax and if all previous angles were straightening
            FLOAT fProbability = fProbabilityEnoughDeviation * fProbabilityStraight * fProbabilityStraightening;

            if ( m_PreviousProbability[ i ].size() >= m_uNumPreviousProbability )
            {
                m_PreviousProbability[ i ].pop_front();
            }
            else
            {
                fProbability = 0.0f;
            }
            m_PreviousProbability[ i ].push_back( fProbability );

            // Delay a few frames to make sure false positives don't cancel out
            fMax = 0;
            const UINT uNumPreviousProbability = m_PreviousProbability[ i ].size();
            for ( UINT j = 0; j < uNumPreviousProbability; j++ )
            {
                fMax = max( fMax, m_PreviousProbability[ i ][ j ] );
            }

            m_fProbability[ i ] = fMax;

            // fAngle will only be 0.0f if both legs failed
            if ( fAngle == 0.0f )
            {
                m_bIsValid[ i ] = FALSE;
            }
            else
            {
                m_bIsValid[ i ] = TRUE;
            }
        }
        else
        {
            m_fProbability[ i ] = 0.0f;
            m_bIsValid[ i ] = FALSE;
        }
    }

    PIXEndNamedEvent();
}
