//-------------------------------------------------------------------------------------
// TurboFilter.cpp
//  
// A XGesture filter that interprets joint data to control a turbo boost and e-brake.
//   
// Microsoft XNA Developer Connection
// Copyright (c) Microsoft Corporation. All rights reserved.
//-------------------------------------------------------------------------------------

#include "TurboFilter.h"

const WCHAR* g_TurboFilterStateNames[] =
{
    L"Waiting",
    L"Boosting In",
    L"Boosting",
    L"Boosting Out",
    L"TOGGLED"
};


//--------------------------------------------------------------------------------------
// Constructor
//--------------------------------------------------------------------------------------
TurboFilter::TurboFilter() :
    m_fMinimumBoostingInVelocity( 1.85f ),
    m_fMinimumBoostingInDuration( 0.05f ),
    m_fMinimumBoostingOutVelocity( -1.25f ),
    m_fMinimumBoostingOutDuration( 0.03f ),
    m_fMaximumGestureDuration( 5.0f )
{
    Reset();
}


//--------------------------------------------------------------------------------------
// Destructor
//--------------------------------------------------------------------------------------
TurboFilter::~TurboFilter() { }


//--------------------------------------------------------------------------------------
// Resets all state data (not configuration).
//--------------------------------------------------------------------------------------
VOID TurboFilter::Reset()
{
    m_eConfidence = NUI_SKELETON_POSITION_CONFIDENCE_NONE;
    m_eState = TURBO_WAITING;
    m_bIsToggled = FALSE;
    m_fLastTrackedVelocity = 0.0f;
    m_fElapsedGestureDuration = 0;
    m_fElapsedStateDuration = 0;
    m_bTrackingRightHand = FALSE;
}


//--------------------------------------------------------------------------------------
// Updates the filter based on the most recent frames to have come into the XGesture system.
//--------------------------------------------------------------------------------------
VOID TurboFilter::Update( float fElapsedTime, const NUI_SKELETON_DATA *pSkeleton, const XMFLOAT4 velocities[] )
{
    
    //todo:datuft is htis right?
    
    FLOAT fLeftHandVelocity = velocities[NUI_SKELETON_POSITION_HAND_LEFT].z;
    FLOAT fRightHandVelocity = velocities[NUI_SKELETON_POSITION_HAND_RIGHT].z;
    
    // If the state is toggled, only reset the state
    if ( m_eState == TURBO_TOGGLED )
    {
        Reset();
    }
    // If the filter is waiting, then see if it has started yet
    else if ( m_eState == TURBO_WAITING )
    {
        // determine if the hand has started moving away from the player - Boosting In is +z
        BOOL bGestureStarted = FALSE;
        if ( fLeftHandVelocity >= m_fMinimumBoostingInVelocity )
        {
            m_bTrackingRightHand = FALSE;
            m_fLastTrackedVelocity = fLeftHandVelocity;
            bGestureStarted = TRUE;
        }
        else if ( fRightHandVelocity >= m_fMinimumBoostingInVelocity )
        {
            m_bTrackingRightHand = TRUE;
            m_fLastTrackedVelocity = fRightHandVelocity;
            bGestureStarted = TRUE;
        }
        else
        {
            m_fLastTrackedVelocity = ( fRightHandVelocity > fLeftHandVelocity ) ? fRightHandVelocity : fLeftHandVelocity;
        }
        if ( bGestureStarted )
        {
            m_eState = TURBO_BOOSTING_IN;
            m_eConfidence = NUI_SKELETON_POSITION_CONFIDENCE_LOW;
            m_fElapsedGestureDuration = m_fElapsedStateDuration = 0.0f;
        }
    }
    // The gesture is in progress
    else
    {
        m_fElapsedGestureDuration += fElapsedTime;
        m_fElapsedStateDuration += fElapsedTime;
        m_fLastTrackedVelocity = m_bTrackingRightHand ? fRightHandVelocity : fLeftHandVelocity;

        // If the gesture is taking too long, then reset
        if ( m_fElapsedGestureDuration >= m_fMaximumGestureDuration )
        {
            Reset();
        }
        else
        {
            switch ( m_eState )
            {
            case TURBO_BOOSTING_IN:
                // If the hand has moved on +z fast enough long enough, then we're really boosting
                if ( ( m_fLastTrackedVelocity >= m_fMinimumBoostingInVelocity ) &&
                     ( m_fElapsedStateDuration >= m_fMinimumBoostingInDuration ) )
                {
                    m_eState = TURBO_BOOSTING;
                    m_eConfidence = NUI_SKELETON_POSITION_CONFIDENCE_LOW;
                    m_fElapsedStateDuration = 0.0f;
                }
                // If the hand has stopped moving on +z, the player has given up; reset
                else if ( m_fLastTrackedVelocity < m_fMinimumBoostingInVelocity )
                {
                    Reset();
                }
                break;

            case TURBO_BOOSTING:
                // determine if the hand has started on -z
                if ( m_fLastTrackedVelocity <= m_fMinimumBoostingOutVelocity )
                {
                    m_eState = TURBO_BOOSTING_OUT;
                    m_eConfidence = NUI_SKELETON_POSITION_CONFIDENCE_LOW;
                    m_fElapsedStateDuration = 0.0f;
                }
                break;

            case TURBO_BOOSTING_OUT:
                // If the hand has moved on -z fast enough long enough, then the gesture is toggled
                if ( ( m_fLastTrackedVelocity <= m_fMinimumBoostingOutVelocity ) &&
                     ( m_fElapsedStateDuration <= m_fMinimumBoostingOutDuration ) )
                {
                    m_eState = TURBO_TOGGLED;
                    m_eConfidence = NUI_SKELETON_POSITION_CONFIDENCE_LOW;
                }
                // If the hand has stopped moving on -z, the player has given up; reset
                else if ( m_fLastTrackedVelocity > m_fMinimumBoostingOutVelocity )
                {
                    Reset();
                }
            }
        }
    }
}


//--------------------------------------------------------------------------------------
// Return the name of the current turbo filter state, for debug text rendering.
//--------------------------------------------------------------------------------------
const WCHAR* TurboFilter::GetState() const
{
    return g_TurboFilterStateNames[ m_eState ];
}