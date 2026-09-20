//----------------------------------------------------------------------------------------------------------------------//--------------------------------------------------------------------------------------
// PlayspaceBounds.cpp
//
// Implementation file for PlayspaceBounds.h methods. 
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//----------------------------------------------------------------------------------------------------------------------

#include <xtl.h>
#include <xavatar.h>
#include <xgraphics.h>
#include <xnamath.h>
#include <nuiapi.h>
#include <assert.h>
#include <AtgApp.h>
#include <AtgFont.h>
#include <AtgHelp.h>
#include <AtgInput.h>
#include <AtgSimpleShaders.h>
#include <AtgUtil.h>
#include <AtgNuiVisualization.h>
#include <AtgDebugDraw.h>

#include "SimpleAnim.h"
#include "PlayspaceBounds.h"
#include "Sample.h"

//----------------------------------------------------------------------------------------------------------------------
// Constants
//----------------------------------------------------------------------------------------------------------------------

// Wait time to avoid "popping" - about 1 second in most cases.
const DWORD HYSTERESIS_WAIT_TIME_FRAMES = 30;

// Player State human-readable strings
const WCHAR* g_pwstrPlayerState[] =
{
    L"None",                 /*PLAYERSTATE_NONE*/
    L"Lost Tracking",        /*PLAYERSTATE_LOST_TRACKING*/
    L"Outside Playspace",    /*PLAYERSTATE_OUTSIDE_PLAYSPACE*/
    L"In Playspace Border",  /*PLAYERSTATE_IN_PLAYSPACE_BORDER*/
    L"In Usable Playspace",  /*PLAYERSTATE_IN_USABLE_PLAYSPACE*/
    L"In Sweet Spot"         /*PLAYERSTATE_IN_SWEET_SPOT*/
};

//----------------------------------------------------------------------------------------------------------------------
// Name: CalculateHalfFrustumSpanAtDist
// Desc: Calculates the half-width of the Frustum in meters at the provided distance, with the FOV for the axis.
//----------------------------------------------------------------------------------------------------------------------
FORCEINLINE FLOAT CalculateHalfFrustumSpanAtDist( FLOAT fHalfFOVRadians, FLOAT fDistanceMeters )
{
    assert( fHalfFOVRadians > 0.0f && fHalfFOVRadians < XM_PIDIV2 );
    assert( fDistanceMeters > 0.0f );

    return tanf( fHalfFOVRadians ) * fDistanceMeters;   
}


//----------------------------------------------------------------------------------------------------------------------
// Name: PlayspaceFrustum::DistanceFromSweetSpotEdge
// Desc: Calculates the distance from the player's center to the edge of the sweet spot.
//----------------------------------------------------------------------------------------------------------------------
FLOAT PlayspaceFrustum::DistanceFromSweetSpotEdge( FXMVECTOR vPlayerCenter )
{
    const XMVECTOR vMinDistance = { SWEET_SPOT_RADIUS, SWEET_SPOT_RADIUS, SWEET_SPOT_RADIUS, SWEET_SPOT_RADIUS };

    // Ignore Y-axis (set it to 0), as we only care about radial distance from the sweet spot.
    XMVECTOR vCenter2D = XMVectorSelect( g_XMZero, vPlayerCenter, g_XMSelect1010 );
    XMVECTOR vDistanceFromCenter = XMVector3Length( vCenter2D - g_vSweetSpot );
    return XMVectorGetX( XMVectorMax( vDistanceFromCenter - vMinDistance, g_XMZero ) );
}


//----------------------------------------------------------------------------------------------------------------------
// Name: PlayspaceFrustum::Initialize
// Desc: Creates the planes which define the playable area.
//----------------------------------------------------------------------------------------------------------------------
void PlayspaceFrustum::Init( Sample* pSample, DebugPlayspaceVerts* pDebug /*= NULL*/ )
{
    m_pSample = pSample;

    // Calculate Frustum half extents.

    FLOAT fHalfWidthMinDist = CalculateHalfFrustumSpanAtDist( SENSOR_H_FOV_HALF_RADIANS, PLAYSPACE_MIN_DISTANCE );
    
    FLOAT fHalfHeightMinDist = CalculateHalfFrustumSpanAtDist( SENSOR_V_FOV_HALF_RADIANS, PLAYSPACE_MIN_DISTANCE );
    
    FLOAT fHalfWidthMaxDist = CalculateHalfFrustumSpanAtDist( SENSOR_H_FOV_HALF_RADIANS, PLAYSPACE_MAX_DISTANCE );
    
    FLOAT fHalfHeightMaxDist = CalculateHalfFrustumSpanAtDist( SENSOR_V_FOV_HALF_RADIANS, PLAYSPACE_MAX_DISTANCE );

    // Create the planes from the extents.

    const XMVECTOR vLeftPlaneNearTop = { -fHalfWidthMinDist, fHalfHeightMinDist, PLAYSPACE_MIN_DISTANCE, 0.0f };
    const XMVECTOR vLeftPlaneNearBottom = { -fHalfWidthMinDist, -fHalfHeightMinDist, PLAYSPACE_MIN_DISTANCE, 0.0f };
    const XMVECTOR vLeftPlaneFarTop = { -fHalfWidthMaxDist, fHalfHeightMaxDist, PLAYSPACE_MAX_DISTANCE, 0.0f };
    const XMVECTOR vRightPlaneFarTop = { fHalfWidthMaxDist, fHalfHeightMaxDist, PLAYSPACE_MAX_DISTANCE, 0.0f };
    const XMVECTOR vRightPlaneNearTop = { fHalfWidthMinDist, fHalfHeightMinDist, PLAYSPACE_MIN_DISTANCE, 0.0f };
    const XMVECTOR vRightPlaneNearBottom = { fHalfWidthMinDist, -fHalfHeightMinDist, PLAYSPACE_MIN_DISTANCE, 0.0f };

    const XMVECTORF32 vBackPlaneUsable = { 0.0f, 0.0f, -1.0f, -PLAYSPACE_MAX_USABLE_DIST };
    const XMVECTORF32 vBackPlaneMax = { 0.0f, 0.0f, -1.0f, -PLAYSPACE_MAX_DISTANCE };

    const XMVECTORF32 vFrontPlaneUsable = { 0.0f, 0.0f, 1.0f, PLAYSPACE_MIN_USABLE_DIST };
    const XMVECTORF32 vFrontPlaneMax = { 0.0f, 0.0f, 1.0f, PLAYSPACE_MIN_DISTANCE };


    m_vLeftPlane = XMPlaneFromPoints( vLeftPlaneNearBottom,  vLeftPlaneNearTop, vLeftPlaneFarTop );
    m_vRightPlane = XMPlaneFromPoints( vRightPlaneNearBottom, vRightPlaneFarTop, vRightPlaneNearTop );
    m_vBackPlane = vBackPlaneMax;
    m_vBackPlaneUsable = vBackPlaneUsable;
    m_vFrontPlane = vFrontPlaneMax;
    m_vFrontPlaneUsable = vFrontPlaneUsable;

    // Bring in the left and right planes by player radius to create "usable area" warning zones.
    m_vRightPlaneEncroach = TranslatePlaneAlongNormal( m_vRightPlane, PLAYER_RADIUS );
    m_vLeftPlaneEncroach = TranslatePlaneAlongNormal( m_vLeftPlane, PLAYER_RADIUS );

    m_playerStatus.SetUntracked();

    if ( pDebug )
    {
        // Copy the calculated verts to the debug rendering array if we want them.

        const XMVECTOR vLeftPlaneFarBottom = { -fHalfWidthMaxDist, -fHalfHeightMaxDist, PLAYSPACE_MAX_DISTANCE, 0.0f };
        const XMVECTOR vRightPlaneFarBottom = { fHalfWidthMaxDist, -fHalfHeightMaxDist, PLAYSPACE_MAX_DISTANCE, 0.0f };

        XMStoreFloat3( &pDebug->avBoundingVertices[ DebugPlayspaceVerts::VERTINDEX_FRONT_TOP_LEFT ], vLeftPlaneNearTop );
        XMStoreFloat3( &pDebug->avBoundingVertices[ DebugPlayspaceVerts::VERTINDEX_FRONT_TOP_RIGHT ], vRightPlaneNearTop );
        XMStoreFloat3( &pDebug->avBoundingVertices[ DebugPlayspaceVerts::VERTINDEX_FRONT_BOTTOM_LEFT ], vLeftPlaneNearBottom );
        XMStoreFloat3( &pDebug->avBoundingVertices[ DebugPlayspaceVerts::VERTINDEX_FRONT_BOTTOM_RIGHT ], vRightPlaneNearBottom );

        XMStoreFloat3( &pDebug->avBoundingVertices[ DebugPlayspaceVerts::VERTINDEX_BACK_TOP_LEFT ], vLeftPlaneFarTop );
        XMStoreFloat3( &pDebug->avBoundingVertices[ DebugPlayspaceVerts::VERTINDEX_BACK_TOP_RIGHT ], vRightPlaneFarTop );
        XMStoreFloat3( &pDebug->avBoundingVertices[ DebugPlayspaceVerts::VERTINDEX_BACK_BOTTOM_LEFT ], vLeftPlaneFarBottom );
        XMStoreFloat3( &pDebug->avBoundingVertices[ DebugPlayspaceVerts::VERTINDEX_BACK_BOTTOM_RIGHT ], vRightPlaneFarBottom );

        pDebug->aiFaceOrdering[ DebugPlayspaceVerts::DBGPSFACE_FRONT ][ 0 ] = DebugPlayspaceVerts::VERTINDEX_FRONT_TOP_LEFT;
        pDebug->aiFaceOrdering[ DebugPlayspaceVerts::DBGPSFACE_FRONT ][ 1 ] = DebugPlayspaceVerts::VERTINDEX_FRONT_TOP_RIGHT;
        pDebug->aiFaceOrdering[ DebugPlayspaceVerts::DBGPSFACE_FRONT ][ 2 ] = DebugPlayspaceVerts::VERTINDEX_FRONT_BOTTOM_RIGHT;
        pDebug->aiFaceOrdering[ DebugPlayspaceVerts::DBGPSFACE_FRONT ][ 3 ] = DebugPlayspaceVerts::VERTINDEX_FRONT_BOTTOM_LEFT;

        pDebug->aiFaceOrdering[ DebugPlayspaceVerts::DBGPSFACE_LEFT ][ 0 ] = DebugPlayspaceVerts::VERTINDEX_BACK_TOP_LEFT;
        pDebug->aiFaceOrdering[ DebugPlayspaceVerts::DBGPSFACE_LEFT ][ 1 ] = DebugPlayspaceVerts::VERTINDEX_FRONT_TOP_LEFT;
        pDebug->aiFaceOrdering[ DebugPlayspaceVerts::DBGPSFACE_LEFT ][ 2 ] = DebugPlayspaceVerts::VERTINDEX_FRONT_BOTTOM_LEFT;
        pDebug->aiFaceOrdering[ DebugPlayspaceVerts::DBGPSFACE_LEFT ][ 3 ] = DebugPlayspaceVerts::VERTINDEX_BACK_BOTTOM_LEFT;

        pDebug->aiFaceOrdering[ DebugPlayspaceVerts::DBGPSFACE_RIGHT ][ 0 ] = DebugPlayspaceVerts::VERTINDEX_FRONT_TOP_RIGHT;
        pDebug->aiFaceOrdering[ DebugPlayspaceVerts::DBGPSFACE_RIGHT ][ 1 ] = DebugPlayspaceVerts::VERTINDEX_BACK_TOP_RIGHT;
        pDebug->aiFaceOrdering[ DebugPlayspaceVerts::DBGPSFACE_RIGHT ][ 2 ] = DebugPlayspaceVerts::VERTINDEX_BACK_BOTTOM_RIGHT;
        pDebug->aiFaceOrdering[ DebugPlayspaceVerts::DBGPSFACE_RIGHT ][ 3 ] = DebugPlayspaceVerts::VERTINDEX_FRONT_BOTTOM_RIGHT;

        pDebug->aiFaceOrdering[ DebugPlayspaceVerts::DBGPSFACE_BACK ][ 0 ] = DebugPlayspaceVerts::VERTINDEX_BACK_TOP_LEFT;
        pDebug->aiFaceOrdering[ DebugPlayspaceVerts::DBGPSFACE_BACK ][ 1 ] = DebugPlayspaceVerts::VERTINDEX_BACK_TOP_RIGHT;
        pDebug->aiFaceOrdering[ DebugPlayspaceVerts::DBGPSFACE_BACK ][ 2 ] = DebugPlayspaceVerts::VERTINDEX_BACK_BOTTOM_RIGHT;
        pDebug->aiFaceOrdering[ DebugPlayspaceVerts::DBGPSFACE_BACK ][ 3 ] = DebugPlayspaceVerts::VERTINDEX_BACK_BOTTOM_LEFT;
    }

}

//----------------------------------------------------------------------------------------------------------------------
// Name: PlayspaceFrustum::UpdateState_LostTracking
// Desc: Called every frame while we're in the Lost Tracking state.
//----------------------------------------------------------------------------------------------------------------------
inline void PlayspaceFrustum::UpdateState_LostTracking()
{
    // Show the "Lost tracking" glyph animation if we're lost for 1 second or more.

    if ( m_dwFramesInState == HYSTERESIS_WAIT_TIME_FRAMES )
    {
        m_pSample->ShowHUDPlayerLost( GAME_PLAYER_ONE );

        // Clear the playspace hints; we just show the glyph because we've lost them entirely.
        m_pSample->ShowHUDPlayspaceHint( GAME_PLAYER_ONE, PSH_NONE, FRUSTUMPOSITION_INSIDE );
    }
}


//----------------------------------------------------------------------------------------------------------------------
// Name: PlayspaceFrustum::ExitState_LostTracking
// Desc: Called when we leave the Lost Tracking state
//----------------------------------------------------------------------------------------------------------------------
inline void PlayspaceFrustum::ExitState_LostTracking( PLAYERSTATE oldState )
{
    m_pSample->ShowHUDPlayerFound( GAME_PLAYER_ONE );
}



//----------------------------------------------------------------------------------------------------------------------
// Name: PlayspaceFrustum::EnterState_InSweetSpot
// Desc: Triggered when you enter the sweet spot; causes the avatar to glow.
//----------------------------------------------------------------------------------------------------------------------
inline void PlayspaceFrustum::EnterState_InSweetSpot( PLAYERSTATE oldState )
{
    m_pSample->StartSweetSpotGlow();
    m_pSample->ShowHUDPlayspaceHint( GAME_PLAYER_ONE, PSH_NONE, FRUSTUMPOSITION_INSIDE );
}


//----------------------------------------------------------------------------------------------------------------------
// Name: PlayspaceFrustum::ExitState_InSweetSpot
// Desc: Triggered when you leave the sweet spot; causes the avatar to stop glowing.
//----------------------------------------------------------------------------------------------------------------------
inline void PlayspaceFrustum::ExitState_InSweetSpot( PLAYERSTATE newState )
{
    // If the Avatar is doing the "sweet spot glow", stop it.
    m_pSample->StopSweetSpotGlow();
}


//----------------------------------------------------------------------------------------------------------------------
// Name: PlayspaceFrustum::Tick_OutsidePlayspace
// Desc: Called when we update and we're in the playspace boundary or (unlikely) outside the playspace entirely.
//----------------------------------------------------------------------------------------------------------------------
inline void PlayspaceFrustum::UpdateState_OutsideUsablePlayspace()
{
    // Have we been in this state long enough?
    if ( m_dwFramesInState >= HYSTERESIS_WAIT_TIME_FRAMES )
    {
        // Did we just move from a different (potentially fine-grain) state to this one?
        if ( m_playerStatus.m_state != m_previousState )
        {
            if ( m_playerStatus.m_state == PLAYERSTATE_IN_PLAYSPACE_BORDER )
            {
                // Show the hint which indicates that we're outside of what's considered the usable playspace.
                m_pSample->ShowHUDPlayspaceHint( GAME_PLAYER_ONE, PSH_PULSE,
                    m_playerStatus.m_dwNormalPlayspaceFlags | m_playerStatus.m_dwUsablePlayspaceFlags );    
            }
            else if ( m_playerStatus.m_state  == PLAYERSTATE_OUTSIDE_PLAYSPACE )
            {
                // It's unlikely, but we could be outside the playspace entirely (our center of mass may be outside on
                // the left/right, but more likely we're too close or too far from the sensor).
                m_pSample->ShowHUDPlayspaceHint( GAME_PLAYER_ONE, PSH_SOLID,
                    m_playerStatus.m_dwNormalPlayspaceFlags | m_playerStatus.m_dwUsablePlayspaceFlags );    
            }
        }
    }

    FLOAT fOpacity = CalculateAvatarOpacity();
    m_pSample->SetAvatarOpacity( fOpacity );
}


//----------------------------------------------------------------------------------------------------------------------
// Name: PlayspaceFrustum::UpdateState_InUsablePlayspace
// Desc: Renders avatar feedback based on how far the player is from the sweet spot.
//----------------------------------------------------------------------------------------------------------------------
inline void PlayspaceFrustum::UpdateState_InUsablePlayspace()
{
    m_pSample->ShowHUDPlayspaceHint( GAME_PLAYER_ONE, PSH_NONE, FRUSTUMPOSITION_INSIDE );

    FLOAT fOpacity = CalculateAvatarOpacity();
    m_pSample->SetAvatarOpacity( fOpacity );
}


//----------------------------------------------------------------------------------------------------------------------
// Name: PlayspaceFrustum::Update
// Desc: Updates the Frustum status based on the tracked index.
//----------------------------------------------------------------------------------------------------------------------
void PlayspaceFrustum::Update( UINT iTrackedIndex, const NUI_SKELETON_FRAME& frame )
{
    
    if ( frame.SkeletonData[ iTrackedIndex ].eTrackingState == NUI_SKELETON_NOT_TRACKED )
    {
        m_playerStatus.SetUntracked();
    }
    else
    {
        XMVECTOR vPlayerCenter = frame.SkeletonData[iTrackedIndex].Position;

        // Correct for tilt...
        XMMATRIX matLevel = NuiTransformMatrixLevel( frame.vNormalToGravity );
        XMVECTOR vPlayerCenterTC = XMVector3Transform( vPlayerCenter, matLevel );
     
        m_playerStatus.m_vLocation = XMVectorSelect( g_XMZero, vPlayerCenterTC, g_XMSelect1010 );

        // Do the checks in camera-relative space for camera Frustum tests, and in
        // camera-at-origin world-space for the 2D sweet spot test.

        m_playerStatus.m_fDistanceFromSweetSpot = DistanceFromSweetSpotEdge( vPlayerCenterTC );
        m_playerStatus.m_dwNormalPlayspaceFlags = TestAgainstPlayspaceFrustum( vPlayerCenter );
        m_playerStatus.m_dwUsablePlayspaceFlags = TestAgainstUsablePlayspaceFrustum( vPlayerCenter );

        if ( m_playerStatus.m_fDistanceFromSweetSpot == 0.0f )
        {
            m_playerStatus.m_state = PLAYERSTATE_IN_SWEET_SPOT;
        }
        else if ( m_playerStatus.m_dwUsablePlayspaceFlags == FRUSTUMPOSITION_INSIDE )
        {
            m_playerStatus.m_state = PLAYERSTATE_IN_USABLE_PLAYSPACE;
        }
        else if ( m_playerStatus.m_dwNormalPlayspaceFlags == FRUSTUMPOSITION_INSIDE )
        {
            m_playerStatus.m_state = PLAYERSTATE_IN_PLAYSPACE_BORDER;
        }
        else
        {
            m_playerStatus.m_state = PLAYERSTATE_OUTSIDE_PLAYSPACE;
        }
    }

    UpdateStateMachine();
}


//----------------------------------------------------------------------------------------------------------------------
// Name: PlayspaceFrustum::UpdateStateMachine
// Desc: Updates the state machine which controls responses to transitions between playspace zones.
//----------------------------------------------------------------------------------------------------------------------
void PlayspaceFrustum::UpdateStateMachine()
{
    // Note: InPlayspaceBorder and OutsidePlayspace are both treated as being one single "OutsideUsablePlayspace" state
    //       - we handle these as a unit, and treat transitions between them as "fine-grain" transitions.

    PLAYERSTATE oldState = m_currentState;
    PLAYERSTATE newState = m_playerStatus.m_state;
    m_currentState = newState;

    if ( oldState == newState || 
        ( newState == PLAYERSTATE_OUTSIDE_PLAYSPACE && oldState == PLAYERSTATE_IN_PLAYSPACE_BORDER) &&
        ( newState == PLAYERSTATE_IN_PLAYSPACE_BORDER && oldState == PLAYERSTATE_OUTSIDE_PLAYSPACE ) )
    {
        // Either we didn't change state, or we're in a really fine-grain state transition (between two highly-correlated
        // states), so we just tick the states and return.

        ++m_dwFramesInState;

        switch ( m_currentState )
        {
            case PLAYERSTATE_LOST_TRACKING:
            {
                UpdateState_LostTracking();
                break;
            }
            case PLAYERSTATE_OUTSIDE_PLAYSPACE:
            case PLAYERSTATE_IN_PLAYSPACE_BORDER:
            {
                UpdateState_OutsideUsablePlayspace();
                break;
            }
            case PLAYERSTATE_IN_USABLE_PLAYSPACE:
            {
                UpdateState_InUsablePlayspace();
                break;
            }
            case PLAYERSTATE_IN_SWEET_SPOT:
            case PLAYERSTATE_NONE:
            default:
            {

            }
        }

        return;
    }

    // We're moving between states, so we perform an Exit, then an Enter transition.

    m_dwFramesInState = 0;

    switch ( oldState )
    {
    case PLAYERSTATE_IN_SWEET_SPOT:
        {
            ExitState_InSweetSpot( newState );
            break;
        }
    case PLAYERSTATE_LOST_TRACKING:
        {
            ExitState_LostTracking( newState );
            break;
        }
    case PLAYERSTATE_IN_USABLE_PLAYSPACE:
    case PLAYERSTATE_IN_PLAYSPACE_BORDER:
    case PLAYERSTATE_OUTSIDE_PLAYSPACE:
    case PLAYERSTATE_NONE:
    default:
        {
            break;
        }
    }

    switch ( newState )
    {
    case PLAYERSTATE_IN_SWEET_SPOT:
        {
            EnterState_InSweetSpot( oldState );
            break;
        }
    case PLAYERSTATE_LOST_TRACKING:
    case PLAYERSTATE_IN_USABLE_PLAYSPACE:
    case PLAYERSTATE_OUTSIDE_PLAYSPACE:
    case PLAYERSTATE_NONE:
    default:
        {
            break;
        }
    }
}
