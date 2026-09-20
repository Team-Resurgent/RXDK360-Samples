//--------------------------------------------------------------------------------------
// File: RunningSpeedDetection.cpp
//
// Detects how fast you are running.
// The returned speed indicates how many steps the user is taking in one second
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include <xtl.h>
#include "RunningSpeedDetection.h"
#include <xnamath.h>

//--------------------------------------------------------------------------------------
// Name: Constructor
// Desc: Initializes
//--------------------------------------------------------------------------------------
CRunningSpeedDetection::CRunningSpeedDetection()
:   m_uStepsSoFar( 0 ),
    m_fBigRunTolerence( 0.05f ),
    m_eKneesStatus( BOTH_KNEES_NEUTRAL )
{

}

//--------------------------------------------------------------------------------------
// Name: TakeStep
// Desc: Once step is determined, this function is called to update the steps taken and update the tracker
//--------------------------------------------------------------------------------------
VOID CRunningSpeedDetection::TakeStep ()
{
    m_FootfallTimes.push_back( GetTickCount() );
    if ( m_FootfallTimes.size() >= MAX_FOOTFALL_HISTORY )
        m_FootfallTimes.pop_front();
    ++m_uStepsSoFar;

    m_LRKneeDiffHistory.Step( m_eKneesStatus );
}

//--------------------------------------------------------------------------------------
// Name: Update
// Desc: This function takes the skeleton data and updates the knee state based on foot step detection code
//This should be called each time skeleton data is received
//--------------------------------------------------------------------------------------
HRESULT CRunningSpeedDetection::Update( const NUI_SKELETON_DATA* pSkeleton)
{
    static const FLOAT fLiftThreshold = 9.0f;
    static const XMVECTOR BaseAngle = XMVectorReplicate( 90.0f );    
    
    // We must be reasonably confident in various joints to continue.  We check confidence for joints that are not used 
    // as an indicator of the overall quality of the Skeleton.
    if (
        pSkeleton->eSkeletonPositionTrackingState[ NUI_SKELETON_POSITION_HIP_CENTER] != NUI_SKELETON_POSITION_NOT_TRACKED && 
        pSkeleton->eSkeletonPositionTrackingState[ NUI_SKELETON_POSITION_SHOULDER_CENTER] != NUI_SKELETON_POSITION_NOT_TRACKED && 
        pSkeleton->eSkeletonPositionTrackingState[ NUI_SKELETON_POSITION_ANKLE_RIGHT] != NUI_SKELETON_POSITION_NOT_TRACKED && 
        pSkeleton->eSkeletonPositionTrackingState[ NUI_SKELETON_POSITION_ANKLE_LEFT] != NUI_SKELETON_POSITION_NOT_TRACKED && 
        pSkeleton->eSkeletonPositionTrackingState[ NUI_SKELETON_POSITION_KNEE_LEFT] != NUI_SKELETON_POSITION_NOT_TRACKED && 
        pSkeleton->eSkeletonPositionTrackingState[ NUI_SKELETON_POSITION_KNEE_RIGHT] != NUI_SKELETON_POSITION_NOT_TRACKED
        ) 
    {

        // The only data we use for detecting running is the Knees. 
        // They move up and down as much as ankles or feet.
        // They show up better in close to camera situations
        XMVECTOR LeftKneePos    = pSkeleton->SkeletonPositions[NUI_SKELETON_POSITION_KNEE_LEFT];
        XMVECTOR RightKneePos   = pSkeleton->SkeletonPositions[NUI_SKELETON_POSITION_KNEE_RIGHT];

        float fLKneeY = XMVectorGetY( LeftKneePos );
        float fRKneeY = XMVectorGetY( RightKneePos );
        
        m_LRKneeDiffHistory.Update( fLKneeY - fRKneeY );
 
        // Don't change foot direction if the noise threshold hasn't been passed in the last 3 deltas.
        if ( m_LRKneeDiffHistory.NoiseThresholdPassedInLastThreeDeltas() )
        {
            if ( m_eKneesStatus == BOTH_KNEES_NEUTRAL )
            {
                // Check to see if a foot is lifting.
                if ( m_LRKneeDiffHistory.SlopeChangedNegative() )
                {
                    m_eKneesStatus = LEFT_GOING_UP;
                    m_LRKneeDiffHistory.SetSlopeAndPivotNegative();

                }
                else if ( m_LRKneeDiffHistory.SlopeChangedPositive() )
                {
                    m_eKneesStatus = RIGHT_GOING_UP;
                    m_LRKneeDiffHistory.SetSlopeAndPivotPositive();
                }
            }
            else if ( m_eKneesStatus == RIGHT_GOING_UP )
            {
                if ( m_LRKneeDiffHistory.SlopeChangedNegative() )
                {
                    if ( m_LRKneeDiffHistory.ThreePositionsInPastPostive() )
                    {
                        m_eKneesStatus = RIGHT_GOING_DOWN;
                        m_LRKneeDiffHistory.SetSlopeAndPivotNegative();
                    } 
                    else {
                        // We were falsely trying to look for a right foot we need to switch to the left foot now.
                        m_eKneesStatus = LEFT_GOING_UP;
                        m_LRKneeDiffHistory.SetSlopeAndPivotNegative();
                    }
                }                
            }
            else if (  m_eKneesStatus == LEFT_GOING_UP )
            {
                if ( m_LRKneeDiffHistory.SlopeChangedPositive() )
                {
                    if ( m_LRKneeDiffHistory.ThreePositionsInPastNegative()  )
                    {
                        m_eKneesStatus = LEFT_GOING_DOWN;
                        m_LRKneeDiffHistory.SetSlopeAndPivotPositive();
                    } 
                    else 
                    {
                        // we were falsely trying to look for a left foot we need to switch to the right foot now.
                        m_eKneesStatus = RIGHT_GOING_UP;
                        m_LRKneeDiffHistory.SetSlopeAndPivotPositive();
                    }
                }
            } 
        }
        if ( m_eKneesStatus == LEFT_GOING_DOWN )
        {
            if ( m_LRKneeDiffHistory.CurrentAndPreviousPositionPositive() && !m_LRKneeDiffHistory.AfterShock() )
            {
                // foot cycle complete take a step and switch to right going up.
                TakeStep();
                m_eKneesStatus = RIGHT_GOING_UP;
            }
        }
        else if ( m_eKneesStatus == RIGHT_GOING_DOWN )
        {
            if ( m_LRKneeDiffHistory.CurrentAndPreviousPositionNegative() && !m_LRKneeDiffHistory.AfterShock() )
            {
                // Foot cycle complete. Take a step and switch state to left going up
                TakeStep();
                m_eKneesStatus = LEFT_GOING_UP;
            }
        }
        // Switch to neutral state when knees are together and flat for X deltas.
        if ( m_eKneesStatus != BOTH_KNEES_NEUTRAL && m_LRKneeDiffHistory.FlatteningOut() )
        {
            m_eKneesStatus = BOTH_KNEES_NEUTRAL;
            m_LRKneeDiffHistory.SetSlopeAndPivotPositive();
        }

        // Calculate Running speed every 2 seconds.
        if ( ( !m_FootfallTimes.empty() && ( GetTickCount() - m_FootfallTimes.back()) < 2000 ) )
        {
            DWORD steps = 0;
            DWORD duration = 0;
            for ( std::list<DWORD>::iterator it = m_FootfallTimes.begin(); it != m_FootfallTimes.end(); ++it  )
            {
                std::list<DWORD>::iterator next = it;
                ++next;
                if ( next == m_FootfallTimes.end() )
                    break;                

                ++steps;
                duration += *next - *it;
            }

            if ( duration > 0 )
            {
                m_fRunningSpeed = (FLOAT)steps / duration * 1000.0f;
                return S_OK;
            }            
        }
    }
    m_fRunningSpeed = 0.0f;
    return S_OK;
}
