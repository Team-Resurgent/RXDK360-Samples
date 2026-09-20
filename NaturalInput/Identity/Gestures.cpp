//--------------------------------------------------------------------------------------
// Gestures.cpp
//
// XNA Developer Connection.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
 
#include "Gestures.h"
#include <assert.h>
 
 
//--------------------------------------------------------------------------------------
// Name: FlickLeftGesture::FlickLeftGesture
// Desc:
//--------------------------------------------------------------------------------------
FlickLeftGesture::FlickLeftGesture()
:m_eStatus( GESTURE_NOTSTARTED ),
m_fYRange( 0.1f ),
m_fXTarget( 0.25f )
{
}
 
 
//--------------------------------------------------------------------------------------
// Name: FlickLeftGesture::FlickLeftGesture
// Desc:
//--------------------------------------------------------------------------------------
FlickLeftGesture::FlickLeftGesture( FLOAT fYRange, FLOAT fXTarget )
:m_eStatus( GESTURE_NOTSTARTED ),
m_fYRange( fYRange ),
m_fXTarget( fXTarget )
{
}
 
 
//--------------------------------------------------------------------------------------
// Name: FlickLeftGesture::Update
// Desc:
//--------------------------------------------------------------------------------------
VOID FlickLeftGesture::Update( const NUI_SKELETON_DATA& skeletonData )
{
    if( skeletonData.eTrackingState != NUI_SKELETON_TRACKED )
    {
        Reset();
        return;
    }
 
    FLOAT fLastHandPosition = m_vCurrentPosition.x;
    m_vCurrentPosition = skeletonData.SkeletonPositions[ NUI_SKELETON_POSITION_HAND_RIGHT ];
 
    switch( m_eStatus )
    {
        case GESTURE_NOTSTARTED:
        {
            if( skeletonData.SkeletonPositions[ NUI_SKELETON_POSITION_HAND_RIGHT ].y <=
                    skeletonData.SkeletonPositions[ NUI_SKELETON_POSITION_ELBOW_RIGHT ].y + m_fYRange &&
                skeletonData.SkeletonPositions[ NUI_SKELETON_POSITION_HAND_RIGHT ].y >=
                    skeletonData.SkeletonPositions[ NUI_SKELETON_POSITION_ELBOW_RIGHT ].y - m_fYRange &&
                skeletonData.SkeletonPositions[ NUI_SKELETON_POSITION_HAND_RIGHT ].x >=
                    skeletonData.SkeletonPositions[ NUI_SKELETON_POSITION_ELBOW_RIGHT ].x                )
            {
                Reset();
                m_vStartPosition = m_vCurrentPosition;
                m_vTargetPosition.x = skeletonData.SkeletonPositions[ NUI_SKELETON_POSITION_ELBOW_RIGHT ].x - m_fXTarget;
                m_vTargetPosition.y = skeletonData.SkeletonPositions[ NUI_SKELETON_POSITION_ELBOW_RIGHT ].y;
                m_vTargetPosition.z = m_vCurrentPosition.z;
                m_eStatus = GESTURE_INPROGRESS;           
            }
           
            break;
        }
 
        case GESTURE_INPROGRESS:
        {
            if( skeletonData.SkeletonPositions[ NUI_SKELETON_POSITION_HAND_RIGHT ].y > m_vTargetPosition.y + m_fYRange ||
                skeletonData.SkeletonPositions[ NUI_SKELETON_POSITION_HAND_RIGHT ].y < m_vTargetPosition.y - m_fYRange    )
            {
                Reset();
            }
            else if( skeletonData.SkeletonPositions[ NUI_SKELETON_POSITION_HAND_RIGHT ].x > fLastHandPosition )
            {
                Reset();
            }
            else if( skeletonData.SkeletonPositions[ NUI_SKELETON_POSITION_HAND_RIGHT ].x <= m_vTargetPosition.x )
            {
                m_eStatus = GESTURE_COMPLETED;           
            }
            break;
        }
 
        case GESTURE_COMPLETED:
        {
            Reset();
            break;
        }
 
        default:
        {
            assert( false );
            break;
        }
    }
 
    return;
}
 
 
//--------------------------------------------------------------------------------------
// Name: FlickLeftGesture::GetProgress
// Desc:
//--------------------------------------------------------------------------------------
FLOAT FlickLeftGesture::GetProgress() const
{
    if( m_eStatus == GESTURE_COMPLETED )
    {
        return 1.0f;
    }
   
    else
    {
        return 0.0f;
    }
}
 
 
//--------------------------------------------------------------------------------------
// Name: FlickLeftGesture::GetBounds
// Desc:
//--------------------------------------------------------------------------------------
VOID FlickLeftGesture::GetBounds( XMVECTOR vBounds[ 2 ] ) const
{
    vBounds[ 0 ].x = min( m_vStartPosition.x, m_vTargetPosition.x );
    vBounds[ 0 ].y = m_vTargetPosition.y - m_fYRange;
    vBounds[ 0 ].z = m_vTargetPosition.z;
 
    vBounds[ 1 ].x = max( m_vStartPosition.x, m_vTargetPosition.x );
    vBounds[ 1 ].y = m_vTargetPosition.y + m_fYRange;
    vBounds[ 1 ].z = m_vTargetPosition.z;
}
 
 
//--------------------------------------------------------------------------------------
// Name: FlickRightGesture::FlickRightGesture
// Desc:
//--------------------------------------------------------------------------------------
FlickRightGesture::FlickRightGesture()
:m_eStatus( GESTURE_NOTSTARTED ),
m_fYRange( 0.1f ),
m_fXTarget( 0.25f )
{
}
 
 
//--------------------------------------------------------------------------------------
// Name: FlickRightGesture::FlickRightGesture
// Desc:
//--------------------------------------------------------------------------------------
FlickRightGesture::FlickRightGesture( FLOAT fYRange, FLOAT fXTarget )
:m_eStatus( GESTURE_NOTSTARTED ),
m_fYRange( fYRange ),
m_fXTarget( fXTarget )
{
}
 
 
//--------------------------------------------------------------------------------------
// Name: FlickRightGesture::Update
// Desc:
//--------------------------------------------------------------------------------------
VOID FlickRightGesture::Update( const NUI_SKELETON_DATA& skeletonData )
{
    if( skeletonData.eTrackingState != NUI_SKELETON_TRACKED )
    {
        Reset();
        return;
    }
 
    FLOAT fLastHandPosition = m_vCurrentPosition.x;
    m_vCurrentPosition = skeletonData.SkeletonPositions[ NUI_SKELETON_POSITION_HAND_RIGHT ];
 
    switch( m_eStatus )
    {
        case GESTURE_NOTSTARTED:
        {
            if( skeletonData.SkeletonPositions[ NUI_SKELETON_POSITION_HAND_RIGHT ].y <=
                    skeletonData.SkeletonPositions[ NUI_SKELETON_POSITION_ELBOW_RIGHT ].y + m_fYRange &&
                skeletonData.SkeletonPositions[ NUI_SKELETON_POSITION_HAND_RIGHT ].y >=
                    skeletonData.SkeletonPositions[ NUI_SKELETON_POSITION_ELBOW_RIGHT ].y - m_fYRange &&
                skeletonData.SkeletonPositions[ NUI_SKELETON_POSITION_HAND_RIGHT ].x <=
                    skeletonData.SkeletonPositions[ NUI_SKELETON_POSITION_ELBOW_RIGHT ].x                )
            {
                Reset();
                m_vStartPosition = m_vCurrentPosition;
                m_vTargetPosition.x = skeletonData.SkeletonPositions[ NUI_SKELETON_POSITION_ELBOW_RIGHT ].x + m_fXTarget;
                m_vTargetPosition.y = skeletonData.SkeletonPositions[ NUI_SKELETON_POSITION_ELBOW_RIGHT ].y;
                m_vTargetPosition.z = m_vCurrentPosition.z;
                m_eStatus = GESTURE_INPROGRESS;           
            }
           
            break;
        }
 
        case GESTURE_INPROGRESS:
        {
            if( skeletonData.SkeletonPositions[ NUI_SKELETON_POSITION_HAND_RIGHT ].y > m_vTargetPosition.y + m_fYRange ||
                skeletonData.SkeletonPositions[ NUI_SKELETON_POSITION_HAND_RIGHT ].y < m_vTargetPosition.y - m_fYRange    )
            {
                Reset();
            }
            else if( skeletonData.SkeletonPositions[ NUI_SKELETON_POSITION_HAND_RIGHT ].x < fLastHandPosition )
            {
                Reset();
            }
            else if( skeletonData.SkeletonPositions[ NUI_SKELETON_POSITION_HAND_RIGHT ].x >= m_vTargetPosition.x )
            {
                m_eStatus = GESTURE_COMPLETED;           
            }
           break;
        }
 
        case GESTURE_COMPLETED:
        {
            Reset();
            break;
        }
 
        default:
        {
            assert( false );
            break;
        }
    }
 
    return;
}
 
 
//--------------------------------------------------------------------------------------
// Name: FlickRightGesture::GetProgress
// Desc:
//--------------------------------------------------------------------------------------
FLOAT FlickRightGesture::GetProgress() const
{
    if( m_eStatus == GESTURE_COMPLETED )
    {
        return 1.0f;
    }
   
    else
    {
        return 0.0f;
    }
}
 
 
//--------------------------------------------------------------------------------------
// Name: FlickRightGesture::GetBounds
// Desc:
//--------------------------------------------------------------------------------------
VOID FlickRightGesture::GetBounds( XMVECTOR vBounds[ 2 ] ) const
{
    vBounds[ 0 ].x = min( m_vStartPosition.x, m_vTargetPosition.x );
    vBounds[ 0 ].y = m_vTargetPosition.y - m_fYRange;
    vBounds[ 0 ].z = m_vTargetPosition.z;
 
    vBounds[ 1 ].x = max( m_vStartPosition.x, m_vTargetPosition.x );
    vBounds[ 1 ].y = m_vTargetPosition.y + m_fYRange;
    vBounds[ 1 ].z = m_vTargetPosition.z;
}
 
 
//--------------------------------------------------------------------------------------
// Name: ListSelectionGesture::ListSelectionGesture
// Desc:
//--------------------------------------------------------------------------------------
ListSelectionGesture::ListSelectionGesture()
:m_eStatus( GESTURE_NOTSTARTED ),
m_dwNumItems( 1 ),
m_fRange( 0.12f ),
m_fHoldSeconds( 1.5f ),
m_fVelocityThreshold( 0.0005 )
{
}
 
 
//--------------------------------------------------------------------------------------
// Name: ListSelectionGesture::ListSelectionGesture
// Desc:
//--------------------------------------------------------------------------------------
ListSelectionGesture::ListSelectionGesture( DWORD dwNumItems )
:m_eStatus( GESTURE_NOTSTARTED ),
m_dwNumItems( dwNumItems ),
m_fRange( 0.12f ),
m_fHoldSeconds( 1.5f ),
m_fVelocityThreshold( 0.0005 )
{
}
 
 
//--------------------------------------------------------------------------------------
// Name: ListSelectionGesture::ListSelectionGesture
// Desc:
//--------------------------------------------------------------------------------------
ListSelectionGesture::ListSelectionGesture( DWORD dwNumItems, FLOAT fRange, FLOAT fHoldSeconds, FLOAT fVelocityThreshold )
:m_eStatus( GESTURE_NOTSTARTED ),
m_dwNumItems( dwNumItems ),
m_fRange( fRange ),
m_fHoldSeconds( fHoldSeconds ),
m_fVelocityThreshold( fVelocityThreshold )
{
}
 
 
//--------------------------------------------------------------------------------------
// Name: ListSelectionGesture::SetNumItems
// Desc:
//--------------------------------------------------------------------------------------
VOID ListSelectionGesture::SetNumItems(DWORD dwNumItems )
{
    m_dwNumItems = dwNumItems;
    Reset();
}
 
 
//--------------------------------------------------------------------------------------
// Name: ListSelectionGesture::Update
// Desc:
//--------------------------------------------------------------------------------------
VOID ListSelectionGesture::Update( const NUI_SKELETON_DATA& skeletonData )
{
    if( skeletonData.eTrackingState != NUI_SKELETON_TRACKED )
    {
        Reset();
        return;
    }
 
    DOUBLE fLastTime = m_fCurrentTime;
    m_fCurrentTime = m_Timer.GetAbsoluteTime();
 
    XMVECTOR vLastPosition = m_vCurrentPosition;
    m_vCurrentPosition = skeletonData.SkeletonPositions[ NUI_SKELETON_POSITION_HAND_RIGHT ];
 
    switch( m_eStatus )
    {
        case GESTURE_NOTSTARTED:
        {
            if( skeletonData.SkeletonPositions[ NUI_SKELETON_POSITION_HAND_RIGHT ].y >=
                    skeletonData.SkeletonPositions[ NUI_SKELETON_POSITION_ELBOW_RIGHT ].y - m_fRange    &&
                skeletonData.SkeletonPositions[ NUI_SKELETON_POSITION_HAND_RIGHT ].y <=
                    skeletonData.SkeletonPositions[ NUI_SKELETON_POSITION_SHOULDER_RIGHT ].y + m_fRange &&
                skeletonData.SkeletonPositions[ NUI_SKELETON_POSITION_HAND_RIGHT ].x >=
                    skeletonData.SkeletonPositions[ NUI_SKELETON_POSITION_ELBOW_RIGHT ].x - m_fRange    &&   
                skeletonData.SkeletonPositions[ NUI_SKELETON_POSITION_HAND_RIGHT ].x <=
                    skeletonData.SkeletonPositions[ NUI_SKELETON_POSITION_ELBOW_RIGHT ].x + m_fRange       )
            {
                Reset();
                m_vStartPosition  = m_vCurrentPosition;
                m_fCurrentVelocity = sqrt( pow( m_vCurrentPosition.x - vLastPosition.x, 2 ) * pow( m_vCurrentPosition.y - vLastPosition.y, 2 ) ) / ( FLOAT )( m_fCurrentTime - fLastTime );
   
                m_vBounds[ 0 ].x = skeletonData.SkeletonPositions[ NUI_SKELETON_POSITION_HAND_RIGHT ].x - m_fRange * 1.5f;
                m_vBounds[ 0 ].y = skeletonData.SkeletonPositions[ NUI_SKELETON_POSITION_SHOULDER_RIGHT ].y + m_fRange;
                m_vBounds[ 0 ].z = m_vStartPosition.z;
 
                m_vBounds[ 1 ].x = skeletonData.SkeletonPositions[ NUI_SKELETON_POSITION_HAND_RIGHT ].x + m_fRange * 1.5f;
                m_vBounds[ 1 ].y = skeletonData.SkeletonPositions[ NUI_SKELETON_POSITION_ELBOW_RIGHT ].y - m_fRange * 1.3f;
                m_vBounds[ 1 ].z = m_vStartPosition.z;
 
                m_fItemHeight = ( m_vBounds[ 0 ].y - m_vBounds[ 1 ].y ) / m_dwNumItems;
                m_dwSelectionIndex = ComputeSelectionIndex();
 
                m_Timer.Reset();
                m_eStatus = GESTURE_INPROGRESS;           
            }
           
            break;
        }
 
        case GESTURE_INPROGRESS:
        {
            m_fCurrentVelocity = sqrt( pow( m_vCurrentPosition.x - vLastPosition.x, 2 ) * pow( m_vCurrentPosition.y - vLastPosition.y, 2 ) ) / ( FLOAT )( m_fCurrentTime - fLastTime );
 
            if( skeletonData.SkeletonPositions[ NUI_SKELETON_POSITION_HAND_RIGHT ].y > m_vBounds[ 0 ].y ||
                skeletonData.SkeletonPositions[ NUI_SKELETON_POSITION_HAND_RIGHT ].y < m_vBounds[ 1 ].y ||
                skeletonData.SkeletonPositions[ NUI_SKELETON_POSITION_HAND_RIGHT ].x < m_vBounds[ 0 ].x ||
                skeletonData.SkeletonPositions[ NUI_SKELETON_POSITION_HAND_RIGHT ].x > m_vBounds[ 1 ].x    )
            {
                Reset();
            }
            else
            {
                DWORD dwNewSelectionIndex = ComputeSelectionIndex();
                if( m_dwSelectionIndex != dwNewSelectionIndex )
                {
                    m_dwSelectionIndex = dwNewSelectionIndex;
                    m_Timer.Reset();
                }
                else if( m_Timer.GetAppTime() >= m_fHoldSeconds )
                {
                    m_eStatus = GESTURE_COMPLETED;           
                }
            }
            break;
        }
 
        case GESTURE_COMPLETED:
        {
            Reset();
            break;
        }
 
        default:
        {
            assert( false );
            break;
        }
    }
 
    return;
}

 
//--------------------------------------------------------------------------------------
// Name: ListSelectionGesture::GetSelection
// Desc:
//--------------------------------------------------------------------------------------
DWORD ListSelectionGesture::GetSelection() const
{
    return m_dwSelectionIndex;
}
 
//--------------------------------------------------------------------------------------
// Name: ListSelectionGesture::GetProgress
// Desc:
//--------------------------------------------------------------------------------------
FLOAT ListSelectionGesture::GetProgress() const
{
    switch( m_eStatus )
    {
        case GESTURE_NOTSTARTED:
            return 0.0f;
 
        case GESTURE_INPROGRESS:
            assert( m_fHoldSeconds > 0.0f );
            return (FLOAT)( const_cast<ListSelectionGesture *>( this )->m_Timer.GetAppTime() / m_fHoldSeconds );
 
        case GESTURE_COMPLETED:
            return 1.0f;
 
        default:
            assert( false );
            return 0.0f;
    }
}
 
 
//--------------------------------------------------------------------------------------
// Name: ListSelectionGesture::GetBounds
// Desc:
//--------------------------------------------------------------------------------------
VOID ListSelectionGesture::GetBounds( XMVECTOR vBounds[ 2 ] ) const
{
    vBounds[ 0 ].x = m_vBounds[ 0 ].x;
    vBounds[ 0 ].y = m_vBounds[ 0 ].y;
    vBounds[ 0 ].z = m_vBounds[ 0 ].z;
 
    vBounds[ 1 ].x = m_vBounds[ 1 ].x;
    vBounds[ 1 ].y = m_vBounds[ 1 ].y;
    vBounds[ 1 ].z = m_vBounds[ 1 ].z;
}
 
 
//--------------------------------------------------------------------------------------
// Name: ListSelectionGesture::GetBoundsForSelection
// Desc:
//--------------------------------------------------------------------------------------
VOID ListSelectionGesture::GetBoundsForSelection( XMVECTOR vBounds[ 2 ] ) const
{
    DWORD dwCurrentItem = ComputeSelectionIndex();
 
    if( m_fCurrentVelocity < m_fVelocityThreshold )
    {
        vBounds[ 0 ].x = m_vBounds[ 0 ].x - m_fItemHeight / 3;
        vBounds[ 0 ].y = m_vBounds[ 0 ].y - dwCurrentItem * m_fItemHeight + m_fItemHeight / 3;
        vBounds[ 0 ].z = m_vBounds[ 0 ].z;
 
        vBounds[ 1 ].x = m_vBounds[ 1 ].x + m_fItemHeight / 3;
        vBounds[ 1 ].y = vBounds[ 0 ].y - m_fItemHeight - 2 * m_fItemHeight / 3;
        vBounds[ 1 ].z = m_vBounds[ 1 ].z;
    }
    else
    {
        vBounds[ 0 ].x = m_vBounds[ 0 ].x;
        vBounds[ 0 ].y = m_vBounds[ 0 ].y - dwCurrentItem * m_fItemHeight;
        vBounds[ 0 ].z = m_vBounds[ 0 ].z;
 
        vBounds[ 1 ].x = m_vBounds[ 1 ].x;
        vBounds[ 1 ].y = vBounds[ 0 ].y - m_fItemHeight;
        vBounds[ 1 ].z = m_vBounds[ 1 ].z;
    }
}
 
 
//--------------------------------------------------------------------------------------
// Name: ListSelectionGesture::ComputeSelectionIndex
// Desc:
//--------------------------------------------------------------------------------------
DWORD ListSelectionGesture::ComputeSelectionIndex() const
{
    if( m_fCurrentVelocity < m_fVelocityThreshold )
    {
        if( m_vCurrentPosition.y - m_vBounds[ 0 ].y > m_dwSelectionIndex * m_fItemHeight + m_fItemHeight / 3 &&
            m_vCurrentPosition.y - m_vBounds[ 0 ].y < m_dwSelectionIndex * m_fItemHeight + m_fItemHeight / 3 - m_fItemHeight - 2 * m_fItemHeight / 3 )
        {
            return m_dwSelectionIndex;
        }
    }
 
    return (DWORD)( ( m_vBounds[ 0 ].y - m_vCurrentPosition.y ) / m_fItemHeight );
}
 
 
//--------------------------------------------------------------------------------------
// Name: StopGesture::StopGesture
// Desc:
//--------------------------------------------------------------------------------------
StopGesture::StopGesture()
:m_eStatus( GESTURE_NOTSTARTED ),
m_fRange( 0.1f ),
m_fHoldSeconds( 1.0f )
{
}
 
 
//--------------------------------------------------------------------------------------
// Name: StopGesture::StopGesture
// Desc:
//--------------------------------------------------------------------------------------
StopGesture::StopGesture( FLOAT fRange, FLOAT fHoldSeconds )
:m_eStatus( GESTURE_NOTSTARTED ),
m_fRange( fRange ),
m_fHoldSeconds( fHoldSeconds )
{
}
 
 
//--------------------------------------------------------------------------------------
// Name: StopGesture::Update
// Desc:
//--------------------------------------------------------------------------------------
VOID StopGesture::Update( const NUI_SKELETON_DATA& skeletonData )
{
    if( skeletonData.eTrackingState != NUI_SKELETON_TRACKED )
    {
        Reset();
        return;
    }
 
    m_vCurrentPositionLeftHand  = skeletonData.SkeletonPositions[ NUI_SKELETON_POSITION_HAND_LEFT ];
    m_vCurrentPositionRightHand = skeletonData.SkeletonPositions[ NUI_SKELETON_POSITION_HAND_RIGHT ];
 
    switch( m_eStatus )
    {
        case GESTURE_NOTSTARTED:
        {
            if( skeletonData.SkeletonPositions[ NUI_SKELETON_POSITION_HAND_LEFT ].y >
                skeletonData.SkeletonPositions[ NUI_SKELETON_POSITION_SHOULDER_LEFT ].y &&
                skeletonData.SkeletonPositions[ NUI_SKELETON_POSITION_HAND_LEFT ].x >=
                skeletonData.SkeletonPositions[ NUI_SKELETON_POSITION_ELBOW_LEFT ].x - m_fRange &&
                skeletonData.SkeletonPositions[ NUI_SKELETON_POSITION_HAND_LEFT ].x <=
                skeletonData.SkeletonPositions[ NUI_SKELETON_POSITION_ELBOW_LEFT ].x + m_fRange &&
                skeletonData.SkeletonPositions[ NUI_SKELETON_POSITION_HAND_RIGHT ].y >
                skeletonData.SkeletonPositions[ NUI_SKELETON_POSITION_SHOULDER_RIGHT ].y &&
                skeletonData.SkeletonPositions[ NUI_SKELETON_POSITION_HAND_RIGHT ].x >=
                skeletonData.SkeletonPositions[ NUI_SKELETON_POSITION_ELBOW_RIGHT ].x - m_fRange &&
                skeletonData.SkeletonPositions[ NUI_SKELETON_POSITION_HAND_RIGHT ].x <=
                skeletonData.SkeletonPositions[ NUI_SKELETON_POSITION_ELBOW_RIGHT ].x + m_fRange    )
            {
                Reset();
                m_vStartPositionLeftHand  = m_vCurrentPositionLeftHand;
                m_vStartPositionRightHand = m_vCurrentPositionRightHand;
                m_Timer.Reset();
                m_eStatus = GESTURE_INPROGRESS;           
            }
           
            break;
        }
 
        case GESTURE_INPROGRESS:
        {
            if( skeletonData.SkeletonPositions[ NUI_SKELETON_POSITION_HAND_LEFT ].y < m_vStartPositionLeftHand.y - m_fRange   ||
                skeletonData.SkeletonPositions[ NUI_SKELETON_POSITION_HAND_LEFT ].y > m_vStartPositionLeftHand.y + m_fRange   ||
                skeletonData.SkeletonPositions[ NUI_SKELETON_POSITION_HAND_LEFT ].x < m_vStartPositionLeftHand.x - m_fRange   ||
                skeletonData.SkeletonPositions[ NUI_SKELETON_POSITION_HAND_LEFT ].x > m_vStartPositionLeftHand.x + m_fRange   ||
                skeletonData.SkeletonPositions[ NUI_SKELETON_POSITION_HAND_RIGHT ].y < m_vStartPositionRightHand.y - m_fRange ||
                skeletonData.SkeletonPositions[ NUI_SKELETON_POSITION_HAND_RIGHT ].y > m_vStartPositionRightHand.y + m_fRange ||
                skeletonData.SkeletonPositions[ NUI_SKELETON_POSITION_HAND_RIGHT ].x < m_vStartPositionRightHand.x - m_fRange ||
                skeletonData.SkeletonPositions[ NUI_SKELETON_POSITION_HAND_RIGHT ].x > m_vStartPositionRightHand.x + m_fRange    )
            {
                Reset();
            }
            else if( m_Timer.GetAppTime() >= m_fHoldSeconds )
            {
                m_eStatus = GESTURE_COMPLETED;           
            }
            break;
        }
 
        case GESTURE_COMPLETED:
        {
            Reset();
            break;
        }
 
        default:
        {
            assert( false );
            break;
        }
    }
 
    return;
}
 
 
//--------------------------------------------------------------------------------------
// Name: StopGesture::GetProgress
// Desc:
//--------------------------------------------------------------------------------------
FLOAT StopGesture::GetProgress() const
{
    switch( m_eStatus )
    {
        case GESTURE_NOTSTARTED:
        {
            return 0.0f;
        }

        case GESTURE_INPROGRESS:
        {
            return (FLOAT)( const_cast<StopGesture *>( this )->m_Timer.GetAppTime() / m_fHoldSeconds );
        }

        case GESTURE_COMPLETED:
        {
            return 1.0f;
        }

        default:
        {
            assert( false );
            return 0.0f;
        }
    }
}
 
 
//--------------------------------------------------------------------------------------
// Name: StopGesture::GetBoundsLeftHand
// Desc:
//--------------------------------------------------------------------------------------
VOID StopGesture::GetBoundsLeftHand( XMVECTOR vBounds[ 2 ] ) const
{
    vBounds[ 0 ].x = m_vStartPositionLeftHand.x - m_fRange;
    vBounds[ 0 ].y = m_vStartPositionLeftHand.y - m_fRange;
    vBounds[ 0 ].z = m_vStartPositionLeftHand.z;
 
    vBounds[ 1 ].x = m_vStartPositionLeftHand.x + m_fRange;
    vBounds[ 1 ].y = m_vStartPositionLeftHand.y + m_fRange;
    vBounds[ 1 ].z = m_vStartPositionLeftHand.z;
}
 
 
//--------------------------------------------------------------------------------------
// Name: StopGesture::GetBoundsRightHand
// Desc:
//--------------------------------------------------------------------------------------
VOID StopGesture::GetBoundsRightHand( XMVECTOR vBounds[ 2 ] ) const
{
    vBounds[ 0 ].x = m_vStartPositionRightHand.x - m_fRange;
    vBounds[ 0 ].y = m_vStartPositionRightHand.y - m_fRange;
    vBounds[ 0 ].z = m_vStartPositionRightHand.z;
 
    vBounds[ 1 ].x = m_vStartPositionRightHand.x + m_fRange;
    vBounds[ 1 ].y = m_vStartPositionRightHand.y + m_fRange;
    vBounds[ 1 ].z = m_vStartPositionRightHand.z;
}


//--------------------------------------------------------------------------------------
// Name: WaveGesture::WaveGesture
// Desc:
//--------------------------------------------------------------------------------------
WaveGesture::WaveGesture()
:m_eStatus( GESTURE_NOTSTARTED ),
 m_fRange( 0.1f ),
 m_fHoldSeconds( 1.0f )
{
}
 
 
//--------------------------------------------------------------------------------------
// Name: WaveGesture::WaveGesture
// Desc:
//--------------------------------------------------------------------------------------
WaveGesture::WaveGesture( FLOAT fRange, FLOAT fHoldSeconds )
:m_eStatus( GESTURE_NOTSTARTED ),
 m_fRange( fRange ),
 m_fHoldSeconds( fHoldSeconds )
{
}
 
 
//--------------------------------------------------------------------------------------
// Name: WaveGesture::Update
// Desc:
//--------------------------------------------------------------------------------------
VOID WaveGesture::Update( const NUI_SKELETON_DATA& skeletonData )
{
    if( skeletonData.eTrackingState != NUI_SKELETON_TRACKED )
    {
        Reset();
        return;
    }
 
    m_vCurrentPosition = skeletonData.SkeletonPositions[ NUI_SKELETON_POSITION_HAND_RIGHT ];
 
    switch( m_eStatus )
    {
        case GESTURE_NOTSTARTED:
        {
            if( skeletonData.SkeletonPositions[ NUI_SKELETON_POSITION_HAND_RIGHT ].y >
                skeletonData.SkeletonPositions[ NUI_SKELETON_POSITION_SHOULDER_RIGHT ].y         &&
                skeletonData.SkeletonPositions[ NUI_SKELETON_POSITION_HAND_RIGHT ].x >=
                skeletonData.SkeletonPositions[ NUI_SKELETON_POSITION_ELBOW_RIGHT ].x - m_fRange &&
                skeletonData.SkeletonPositions[ NUI_SKELETON_POSITION_HAND_RIGHT ].x <=
                skeletonData.SkeletonPositions[ NUI_SKELETON_POSITION_ELBOW_RIGHT ].x + m_fRange &&
                skeletonData.SkeletonPositions[ NUI_SKELETON_POSITION_HAND_LEFT ].y <
                skeletonData.SkeletonPositions[ NUI_SKELETON_POSITION_SPINE ].y                  &&
                skeletonData.SkeletonPositions[ NUI_SKELETON_POSITION_HAND_LEFT ].x <
                skeletonData.SkeletonPositions[ NUI_SKELETON_POSITION_SPINE ].x                     )

            {
                Reset();
                m_vStartPosition = m_vCurrentPosition;
                m_Timer.Reset();
                m_eStatus = GESTURE_INPROGRESS;           
            }
           
            break;
        }
 
        case GESTURE_INPROGRESS:
        {
            if( skeletonData.SkeletonPositions[ NUI_SKELETON_POSITION_HAND_RIGHT ].y < 
                    m_vStartPosition.y - m_fRange                                       ||
                skeletonData.SkeletonPositions[ NUI_SKELETON_POSITION_HAND_RIGHT ].y >
                    m_vStartPosition.y + m_fRange                                       ||
                skeletonData.SkeletonPositions[ NUI_SKELETON_POSITION_HAND_RIGHT ].x < 
                    m_vStartPosition.x - m_fRange                                       ||
                skeletonData.SkeletonPositions[ NUI_SKELETON_POSITION_HAND_RIGHT ].x > 
                    m_vStartPosition.x + m_fRange                                       ||
                skeletonData.SkeletonPositions[ NUI_SKELETON_POSITION_HAND_LEFT ].y >
                skeletonData.SkeletonPositions[ NUI_SKELETON_POSITION_SPINE ].y         ||
                skeletonData.SkeletonPositions[ NUI_SKELETON_POSITION_HAND_LEFT ].x >
                skeletonData.SkeletonPositions[ NUI_SKELETON_POSITION_SPINE ].x              )
            {
                Reset();
            }
            else if( m_Timer.GetAppTime() >= m_fHoldSeconds )
            {
                m_eStatus = GESTURE_COMPLETED;           
            }
            break;
        }
 
        case GESTURE_COMPLETED:
        {
            Reset();
            break;
        }
 
        default:
        {
            assert( false );
            break;
        }
    }
 
    return;
}
 
 
//--------------------------------------------------------------------------------------
// Name: WaveGesture::GetProgress
// Desc:
//--------------------------------------------------------------------------------------
FLOAT WaveGesture::GetProgress() const
{
    switch( m_eStatus )
    {
        case GESTURE_NOTSTARTED:
        {
            return 0.0f;
        }

        case GESTURE_INPROGRESS:
        {
            return (FLOAT)( const_cast<WaveGesture *>( this )->m_Timer.GetAppTime() / m_fHoldSeconds );
        }

        case GESTURE_COMPLETED:
        {
            return 1.0f;
        }

        default:
        {
            assert( false );
            return 0.0f;
        }
    }
}
 
 
//--------------------------------------------------------------------------------------
// Name: WaveGesture::GetBounds
// Desc:
//--------------------------------------------------------------------------------------
VOID WaveGesture::GetBounds( XMVECTOR vBounds[ 2 ] ) const
{
    vBounds[ 0 ].x = m_vStartPosition.x - m_fRange;
    vBounds[ 0 ].y = m_vStartPosition.y - m_fRange;
    vBounds[ 0 ].z = m_vStartPosition.z;
 
    vBounds[ 1 ].x = m_vStartPosition.x + m_fRange;
    vBounds[ 1 ].y = m_vStartPosition.y + m_fRange;
    vBounds[ 1 ].z = m_vStartPosition.z;
}
 

//--------------------------------------------------------------------------------------
// Gameplay gestures
//--------------------------------------------------------------------------------------
 
FLOAT ComputeAngle( FLOAT fX, FLOAT fY );
ANGLE_DIRECTION ComputeDirection( FLOAT fAngle1, FLOAT fAngle2 );
FLOAT ComputeProgress( FLOAT fAngle1, FLOAT fAngle2, ANGLE_DIRECTION eAngleDirection );


//--------------------------------------------------------------------------------------
// Name: AirCirclesGesture::AirCirclesGesture
// Desc:
//--------------------------------------------------------------------------------------
AirCirclesGesture::AirCirclesGesture()
:m_eStatus( GESTURE_NOTSTARTED ),
 m_eDirection( ANGLE_UNKNOWNDIRECTION ),
 m_fInnerRadius( 0.0001f ),
 m_fOuterRadius( 0.05f ),
 m_dwMaxFreePass( 3 )
{
    assert( XMScalarNearEqual( 0.0f, ComputeAngle( 0, 0 ), 0.0000001f ) );
    assert( XMScalarNearEqual( D3DX_PI / 2.0f, ComputeAngle( 0, 5 ), 0.0000001f ) );
    assert( XMScalarNearEqual( D3DX_PI, ComputeAngle( -8, 0 ), 0.0000001f ) );
    assert( XMScalarNearEqual( D3DX_PI * 3.0f / 2.0f, ComputeAngle( 0, -3 ), 0.0000001f ) );
 
    assert( XMScalarNearEqual( D3DX_PI / 4.0f, ComputeAngle( 3, 3 ), 0.0000001f ) );
    assert( XMScalarNearEqual( D3DX_PI * 3.0f / 4.0f, ComputeAngle( -5, 5 ), 0.0000001f ) );
    assert( XMScalarNearEqual( D3DX_PI * 5.0f / 4.0f, ComputeAngle( -8, -8 ), 0.0000001f ) );
    assert( XMScalarNearEqual( D3DX_PI * 7.0f / 4.0f, ComputeAngle( 3, -3 ), 0.0000001f ) );
 
    // Using mirrored values... i.e. inverted from the player
    assert( ANGLE_COUNTERCLOCKWISE == ComputeDirection( D3DX_PI / 4.0f, D3DX_PI / 2.0f ) );
    assert( ANGLE_CLOCKWISE == ComputeDirection( D3DX_PI * 3.0f / 2.0f, D3DX_PI * 5.0f / 4.0f ) );
 
    assert( ANGLE_CLOCKWISE == ComputeDirection( 0.1, 0.0611436926f ) );
    assert( ANGLE_CLOCKWISE == ComputeDirection( 0.0611436926f, 5.84151316f ) );
 
    assert( XMScalarNearEqual( 0.25f,ComputeProgress( D3DX_PI / 4.0f, D3DX_PI * 3.0f / 4.0f, ANGLE_COUNTERCLOCKWISE ), 0.0000001f ) );
    assert( XMScalarNearEqual( 0.50f,ComputeProgress( D3DX_PI / 4.0f, D3DX_PI * 5.0f / 4.0f, ANGLE_COUNTERCLOCKWISE ), 0.0000001f ) );
    assert( XMScalarNearEqual( 0.75f,ComputeProgress( D3DX_PI / 4.0f, D3DX_PI * 7.0f / 4.0f, ANGLE_COUNTERCLOCKWISE ), 0.0000001f ) );
 
    assert( XMScalarNearEqual( 0.75f,ComputeProgress( D3DX_PI / 4.0f, D3DX_PI * 3.0f / 4.0f, ANGLE_CLOCKWISE ), 0.0000001f ) );
    assert( XMScalarNearEqual( 0.50f,ComputeProgress( D3DX_PI / 4.0f, D3DX_PI * 5.0f / 4.0f, ANGLE_CLOCKWISE ), 0.0000001f ) );
    assert( XMScalarNearEqual( 0.25f,ComputeProgress( D3DX_PI / 4.0f, D3DX_PI * 7.0f / 4.0f, ANGLE_CLOCKWISE ), 0.0000001f ) );
}
 
 
//--------------------------------------------------------------------------------------
// Name: AirCirclesGesture::AirCirclesGesture
// Desc:
//--------------------------------------------------------------------------------------
AirCirclesGesture::AirCirclesGesture( FLOAT fInnerRange, FLOAT fOuterRange, DWORD dwMaxFreePass )
:m_eStatus( GESTURE_NOTSTARTED ),
 m_eDirection( ANGLE_UNKNOWNDIRECTION ),
 m_fInnerRadius( fInnerRange ),
 m_fOuterRadius( fOuterRange ),
 m_dwMaxFreePass( dwMaxFreePass )
{
}
 
 
//--------------------------------------------------------------------------------------
// Name: AirCirclesGesture::Update
// Desc:
//--------------------------------------------------------------------------------------
VOID AirCirclesGesture::Update( const NUI_SKELETON_DATA& skeletonData )
{
    if( skeletonData.eTrackingState != NUI_SKELETON_TRACKED )
    {
        Reset();
        return;
    }
 
 
    XMVECTOR vLastPosition = m_vCurrentPosition;
    m_vCurrentPosition = skeletonData.SkeletonPositions[ NUI_SKELETON_POSITION_HAND_RIGHT ];
 
    m_vCenter = skeletonData.SkeletonPositions[ NUI_SKELETON_POSITION_ELBOW_RIGHT ];
    FLOAT fCurrentRadius =  sqrt( pow( m_vCurrentPosition.x - m_vCenter.x, 2 ) * pow( m_vCurrentPosition.y - m_vCenter.y, 2 ) );
 
    switch( m_eStatus )
    {
        case GESTURE_NOTSTARTED:
        {
            if( m_bStarting )
            {
                if( fCurrentRadius >= m_fInnerRadius && fCurrentRadius <= m_fOuterRadius )
                {
                    FLOAT a2 = ComputeAngle( vLastPosition.x - m_vCenter.x, vLastPosition.y - m_vCenter.y );
                    FLOAT a3 = ComputeAngle( m_vCurrentPosition.x - m_vCenter.x, m_vCurrentPosition.y - m_vCenter.y );
 
                    if( ComputeDirection( a2, a3 ) == m_eDirection )
                    {
                        FLOAT a1 = ComputeAngle( m_vStartPosition.x - m_vCenter.x, m_vStartPosition.y - m_vCenter.y );
                        m_fProgress = ComputeProgress( a1, a3, m_eDirection );
                        if( m_fProgress > 0.6f )
                        {
                            m_eStatus = GESTURE_INPROGRESS;
                        }
                    }
                    else
                    {
                        Reset();
                    }
                }
            }
            else
            {
                if( fCurrentRadius >= m_fInnerRadius && fCurrentRadius <= m_fOuterRadius )
                {
                    FLOAT a2 = ComputeAngle( vLastPosition.x - m_vCenter.x, vLastPosition.y - m_vCenter.y );
                    FLOAT a3 = ComputeAngle( m_vCurrentPosition.x - m_vCenter.x, m_vCurrentPosition.y - m_vCenter.y );
                    m_eDirection = ComputeDirection( a2, a3 );
                    if( m_eDirection != ANGLE_UNKNOWNDIRECTION )
                    {
                        Reset();
                        m_bStarting = TRUE;
 
                        m_vStartPosition  = m_vCurrentPosition;
                        m_fProgress = 0.0f;
                        m_dwFreePass = 0;
                    }
                }
            }

            break;
        }
 
        case GESTURE_COMPLETED:
        {
            m_eStatus = GESTURE_INPROGRESS;
            m_dwFreePass = 0;
 
            // No "break;", Let it flow into GESTURE_INPROGRESS
        }
 
        case GESTURE_INPROGRESS:
            {
            if( fCurrentRadius < m_fInnerRadius || fCurrentRadius > m_fOuterRadius )
            {
                if( m_dwFreePass < m_dwMaxFreePass)
                {
                    ++ m_dwFreePass;
                }
                else
                {
                    Reset();
                }
            }
            else
            {
                FLOAT a2 = ComputeAngle( vLastPosition.x - m_vCenter.x, vLastPosition.y - m_vCenter.y );
                FLOAT a3 = ComputeAngle( m_vCurrentPosition.x - m_vCenter.x, m_vCurrentPosition.y - m_vCenter.y );
 
                if( ComputeDirection( a2, a3 ) != m_eDirection )
                {
                    if( m_dwFreePass < m_dwMaxFreePass)
                    {
                        ++ m_dwFreePass;
                    }
                    else
                    {
                        Reset();
                    }
                }
                else
                {
                    FLOAT a1 = ComputeAngle( m_vStartPosition.x - m_vCenter.x, m_vStartPosition.y - m_vCenter.y );
                    FLOAT fLastProgress = m_fProgress;
                    m_fProgress = ComputeProgress( a1, a3, m_eDirection );
                    if( m_fProgress < fLastProgress )
                    {
                        m_eStatus = GESTURE_COMPLETED;
                    }
                }
            }
            break;
        }
 
        default:
        {
            assert( false );
            break;
        }
    }
 
}
 
 
//--------------------------------------------------------------------------------------
// Name: AirCirclesGesture::GetProgress
// Desc:
//--------------------------------------------------------------------------------------
FLOAT AirCirclesGesture::GetProgress() const
{
    switch( m_eStatus )
    {
        case GESTURE_NOTSTARTED:
            return 0.0f;
 
        case GESTURE_INPROGRESS:
            return m_fProgress;
 
        case GESTURE_COMPLETED:
            return 1.0f;
 
        default:
            assert( false );
            return 0.0f;
    }
}
 
 
//--------------------------------------------------------------------------------------
// Name: AirCirclesGesture::GetBounds
// Desc:
//--------------------------------------------------------------------------------------
VOID AirCirclesGesture::GetBounds( XMVECTOR* pvCenter, FLOAT* pfInnerRadius, FLOAT* pfOuterRadius ) const
{
    *pvCenter = m_vCenter;
    *pfInnerRadius = m_fInnerRadius;
    *pfOuterRadius = m_fOuterRadius;
}
 
 
//--------------------------------------------------------------------------------------
// Name: ComputeAngle
// Desc:
//--------------------------------------------------------------------------------------
FLOAT ComputeAngle( FLOAT fX, FLOAT fY )
{
    FLOAT fRadius = sqrt(  fX * fX + fY * fY );
   
    if( fRadius == 0 )
          return 0;
    else
    {
        assert( fY / fRadius >= D3DX_PI / -2.0f );
        assert( fY / fRadius <= D3DX_PI / 2.0f );
        FLOAT fAngle = asin( fY / fRadius );
 
        if( fX < 0.0f )
        {
                fAngle = fAngle * -1 + D3DX_PI;
        }
 
        if( fAngle < 0.0f )
        {
            fAngle += 2 * D3DX_PI;
        }
 
        return fAngle;
    }
}
 
 
//--------------------------------------------------------------------------------------
// Name: ComputeDirection
// Desc:
//--------------------------------------------------------------------------------------
ANGLE_DIRECTION ComputeDirection( FLOAT fAngle1, FLOAT fAngle2 )
{
    if( XMScalarNearEqual( fAngle1, fAngle2, 0.0000001f ) || XMScalarNearEqual ( abs( fAngle1 - fAngle2 ), D3DX_PI, 0.0000001f ) )
    {
        return ANGLE_UNKNOWNDIRECTION;
    }

    ANGLE_DIRECTION eAngleDirection = ANGLE_UNKNOWNDIRECTION;
    if( fAngle1 < fAngle2 )
    {
        if( abs( fAngle1 - fAngle2 ) > D3DX_PI )
        {
            eAngleDirection = ANGLE_CLOCKWISE;
        }
        else
        {
            eAngleDirection = ANGLE_COUNTERCLOCKWISE;
        }
    }
    else
    {
        if( abs( fAngle1 - fAngle2 ) > D3DX_PI )
        {
            eAngleDirection = ANGLE_COUNTERCLOCKWISE;
        }
        else
        {
            eAngleDirection = ANGLE_CLOCKWISE;
        }
    }
 
    return eAngleDirection;
}
 
 
//--------------------------------------------------------------------------------------
// Name: ComputeProgress
// Desc:
//--------------------------------------------------------------------------------------
FLOAT ComputeProgress( FLOAT fAngle1, FLOAT fAngle2, ANGLE_DIRECTION eAngleDirection )
{
    assert( eAngleDirection != ANGLE_UNKNOWNDIRECTION );
 
    FLOAT fProgress = fAngle2 - fAngle1;
    if( fProgress < 0 )
    {
        fProgress += 2 * D3DX_PI;
    }
    else if( fProgress > 2 * D3DX_PI )
    {
        fProgress -= 2 * D3DX_PI;
    }
 
 
    if( eAngleDirection == ANGLE_CLOCKWISE )
        fProgress = 2 * D3DX_PI - fProgress;
 
    return fProgress / ( 2 * D3DX_PI );
}