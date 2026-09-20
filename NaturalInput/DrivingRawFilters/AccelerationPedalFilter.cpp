//-------------------------------------------------------------------------------------
// AccelerationPedalFilter.cpp
//  
// A natural input filter that interprets joint data to control an acceleration pedal.
//   
// Microsoft XNA Developer Connection
// Copyright (c) Microsoft Corporation. All rights reserved.
//-------------------------------------------------------------------------------------

#include "AccelerationPedalFilter.h"


const FLOAT PEDAL_ACCELERATION_SCALE = 150.0f;


//--------------------------------------------------------------------------------------
// Constructor
//--------------------------------------------------------------------------------------
AccelerationPedalFilter::AccelerationPedalFilter() :
    m_eConfidence( NUI_SKELETON_POSITION_NOT_TRACKED ),
    m_fAcceleration( 0.0f ),
    m_fLegDifference( 0.0f ),
    m_fHeightAboveFloor( 0.0f ),
    m_fLegDifferenceThreshold( 0.1f ),
    m_fMinimumHeightAboveFloor( 0.1f )
{
    
    m_LeftLegPosition = XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);
    m_RightLegPosition = XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);
}

//--------------------------------------------------------------------------------------
// Destructor
//--------------------------------------------------------------------------------------
AccelerationPedalFilter::~AccelerationPedalFilter() { }

NUI_SKELETON_POSITION_TRACKING_STATE ReturnLowestConfidence( NUI_SKELETON_POSITION_TRACKING_STATE conf1, NUI_SKELETON_POSITION_TRACKING_STATE conf2 )
{
    if ( conf1 == NUI_SKELETON_POSITION_NOT_TRACKED || conf2 == NUI_SKELETON_POSITION_NOT_TRACKED) 
        return NUI_SKELETON_POSITION_NOT_TRACKED;
    else if ( conf1 == NUI_SKELETON_POSITION_INFERRED || conf2 == NUI_SKELETON_POSITION_INFERRED) 
        return NUI_SKELETON_POSITION_INFERRED;
    else return NUI_SKELETON_POSITION_TRACKED;
}


//--------------------------------------------------------------------------------------
// Updates the filter based on the most recent joints.
//--------------------------------------------------------------------------------------
VOID AccelerationPedalFilter::Update( float fElapsedTime, const NUI_SKELETON_DATA* pSkeleton )
{
    // Retrieve the relevant joints

    // Determine if we're going to use the knees or the feet
    NUI_SKELETON_POSITION_TRACKING_STATE footConfidence = ReturnLowestConfidence( 
        pSkeleton->eSkeletonPositionTrackingState[NUI_SKELETON_POSITION_ANKLE_LEFT], 
        pSkeleton->eSkeletonPositionTrackingState[NUI_SKELETON_POSITION_ANKLE_RIGHT] ); 

    NUI_SKELETON_POSITION_TRACKING_STATE kneeConfidence = ReturnLowestConfidence( 
        pSkeleton->eSkeletonPositionTrackingState[NUI_SKELETON_POSITION_KNEE_LEFT], 
        pSkeleton->eSkeletonPositionTrackingState[NUI_SKELETON_POSITION_KNEE_RIGHT] ); 

    if ( footConfidence != NUI_SKELETON_POSITION_NOT_TRACKED )
    {
        m_LeftLegPosition = pSkeleton->SkeletonPositions[NUI_SKELETON_POSITION_ANKLE_LEFT];
        m_RightLegPosition = pSkeleton->SkeletonPositions[NUI_SKELETON_POSITION_ANKLE_RIGHT];
        m_eConfidence = footConfidence ;
    }
    else if ( kneeConfidence != NUI_SKELETON_POSITION_NOT_TRACKED)
    {
        m_LeftLegPosition = pSkeleton->SkeletonPositions[NUI_SKELETON_POSITION_KNEE_LEFT];
        m_RightLegPosition = pSkeleton->SkeletonPositions[NUI_SKELETON_POSITION_KNEE_RIGHT];
        m_eConfidence = kneeConfidence;
    }
    else
    {
        m_LeftLegPosition = m_RightLegPosition = XMVectorSet( 0.0f, 0.0f, 0.0f, 0.0f );
        m_eConfidence = NUI_SKELETON_POSITION_NOT_TRACKED;
    }

    // Calculate the distance between the feet/knees in the Z direction
    m_fLegDifference = m_LeftLegPosition.z - m_RightLegPosition.z;

    // Calculate the distance between the feet/knees in the Y direction
    m_fHeightAboveFloor = m_RightLegPosition.y - m_LeftLegPosition.y;

    // Determine the current acceleration
    if ( m_fHeightAboveFloor > m_fMinimumHeightAboveFloor )
    {
        // If the height above the floor is significant, then this value 
        // takes precendence over the leg-forward difference.
        m_fAcceleration = m_fHeightAboveFloor;
    }
    else
    {
        m_fAcceleration = m_fLegDifference; 
    }

    m_fAcceleration *= PEDAL_ACCELERATION_SCALE;
}