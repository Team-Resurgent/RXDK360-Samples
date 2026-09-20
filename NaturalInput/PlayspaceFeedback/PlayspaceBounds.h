//----------------------------------------------------------------------------------------------------------------------
// PlaySpaceBounds.h
//
// Handles tracking the player relative to the usable playspace area.
//
// Advanced Technology Group (ATG)
// Copyright (c) Microsoft Corporation. All rights reserved.
//----------------------------------------------------------------------------------------------------------------------

#pragma once

#ifndef PLAYSPACEBOUNDS_H_GUARD
#define PLAYSPACEBOUNDS_H_GUARD

//----------------------------------------------------------------------------------------------------------------------
// Forward Declarations
//----------------------------------------------------------------------------------------------------------------------

class Sample;

//----------------------------------------------------------------------------------------------------------------------
// Constants
//----------------------------------------------------------------------------------------------------------------------

// Playspace constants - taken from the whitepaper "Natural Environs: Understanding  and Developing for the Natural
// User Input Play Space" by Scott Selfon.

const FLOAT SENSOR_H_FOV_HALF_RADIANS = XMConvertToRadians( NUI_CAMERA_DEPTH_NOMINAL_HORIZONTAL_FOV / 2.0f );
const FLOAT SENSOR_V_FOV_HALF_RADIANS = XMConvertToRadians( NUI_CAMERA_DEPTH_NOMINAL_VERTICAL_FOV / 2.0f );

const FLOAT PLAYSPACE_MIN_DISTANCE = 0.8f;    // meters
const FLOAT PLAYSPACE_MIN_USABLE_DIST = 1.2f; // meters
const FLOAT PLAYSPACE_MAX_USABLE_DIST = 3.5f; // meters
const FLOAT PLAYSPACE_MAX_DISTANCE = 4.0f;    // meters

const FLOAT PLAYER_RADIUS = 0.8f;            // Safe distance from edge of usable playspace for a given player in meters

const FLOAT SWEET_SPOT_RADIUS = 0.46f;       // Square of the radius of the "sweet spot" in meters
const FLOAT SWEET_SPOT_Z_DISTANCE = 2.26f;   // Distance of center of sweet spot from sensor array in meters
const FLOAT SWEET_SPOT_FADE_DISTANCE = 0.6f; // Distance from edge of sweetspot at which we hit minimum opacity
const FLOAT SWEET_SPOT_OPACITY_MAX = 0.75f;  // Max opacity when outside sweetspot.
const FLOAT SWEET_SPOT_OPACITY_MIN = 0.05f;  // Min opacity when outside sweetspot.

// The location of the sweet spot in space, on the floor, in the skeleton tracking coordinate system.
const XMVECTORF32 g_vSweetSpot = { 0.0f, 0.0f, SWEET_SPOT_Z_DISTANCE, 0.0f }; 

//----------------------------------------------------------------------------------------------------------------------
// Inline functions 
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
// Name: PointInFrontOfPlane
// Desc: Figures out if a point is in front, behind, or on the plane. All components of the returned vector will be
//       +ve if the point is in front of the plane (in the direction of the normal), -ve if the point is behind the
//       plane, and 0 if the point is on the plane.
//----------------------------------------------------------------------------------------------------------------------
inline XMVECTOR PointInFrontOfPlane( FXMVECTOR vPlane, FXMVECTOR vPoint )
{
    const FLOAT s_Neg1 = -1.0f;

    XMVECTOR vPointNegW = XMVectorSetWPtr( vPoint, &s_Neg1 );
    return XMVector4Dot( vPlane, vPointNegW );
}


//----------------------------------------------------------------------------------------------------------------------
// Name: TranslatePlaneAlongNormal
// Desc: Moves a normalized plane fDistance along its normal.
//----------------------------------------------------------------------------------------------------------------------
inline XMVECTOR TranslatePlaneAlongNormal( FXMVECTOR vNormalizedPlane, FLOAT fDistance )
{
    XMVECTOR vDisplacement = g_XMZero;
    vDisplacement = XMVectorSetWPtr( vDisplacement, &fDistance );
    return vNormalizedPlane + vDisplacement;
}


//----------------------------------------------------------------------------------------------------------------------
// Name: enum FRUSTUMPOSITION
// Desc: Describes the position of the player relative to the Frustum it is being tested against.
//----------------------------------------------------------------------------------------------------------------------
enum FRUSTUMPOSITION
{
    FRUSTUMPOSITION_INSIDE = 0,
    FRUSTUMPOSITION_OUT_FRONT = 1,
    FRUSTUMPOSITION_OUT_BACK = 2,
    FRUSTUMPOSITION_OUT_LEFT = 4,
    FRUSTUMPOSITION_OUT_RIGHT = 8
};


//----------------------------------------------------------------------------------------------------------------------
// Name: enum PLAYERSTATE
// Desc: The current gross state of the player
//----------------------------------------------------------------------------------------------------------------------
enum PLAYERSTATE
{
    PLAYERSTATE_NONE,
    PLAYERSTATE_LOST_TRACKING,
    PLAYERSTATE_OUTSIDE_PLAYSPACE,
    PLAYERSTATE_IN_PLAYSPACE_BORDER,
    PLAYERSTATE_IN_USABLE_PLAYSPACE,
    PLAYERSTATE_IN_SWEET_SPOT
};

// Player State human-readable strings
extern const WCHAR* g_pwstrPlayerState[];

//----------------------------------------------------------------------------------------------------------------------
// Name: enum PLAYSPACEHINTEFFECT
// Desc: The type of feedback to request for the HUD hints.
//----------------------------------------------------------------------------------------------------------------------
enum PLAYSPACEHINTEFFECT
{
    PSH_NONE,
    PSH_SOLID,
    PSH_PULSE
};

//----------------------------------------------------------------------------------------------------------------------
// Name: struct PlayerStatus
// Desc: The player's current state with respect to the playspace.
//----------------------------------------------------------------------------------------------------------------------
struct PlayerStatus
{
    PLAYERSTATE m_state;
    FLOAT m_fDistanceFromSweetSpot;
    DWORD m_dwUsablePlayspaceFlags;
    DWORD m_dwNormalPlayspaceFlags;
    XMVECTOR m_vLocation;

    inline void SetUntracked();
};

//----------------------------------------------------------------------------------------------------------------------
// Name: struct DebugPlayspaceVerts
// Desc: Vertices used to render the playspace while debugging
//----------------------------------------------------------------------------------------------------------------------
struct DebugPlayspaceVerts
{
    // The index of faces stored in aiFaceOrdering
    enum DBGPSFACE
    {
        DBGPSFACE_FRONT,
        DBGPSFACE_LEFT,
        DBGPSFACE_RIGHT,
        DBGPSFACE_BACK,

        DBGPSFACE_COUNT    // This value will be a count of the previously listed entries.
    };

    // The index of verts stored in arrvBoundingVertices
    enum VERTINDEX
    {
        VERTINDEX_FRONT_TOP_LEFT,
        VERTINDEX_FRONT_TOP_RIGHT,
        VERTINDEX_FRONT_BOTTOM_LEFT,
        VERTINDEX_FRONT_BOTTOM_RIGHT,
        VERTINDEX_BACK_TOP_LEFT,
        VERTINDEX_BACK_TOP_RIGHT,
        VERTINDEX_BACK_BOTTOM_LEFT,
        VERTINDEX_BACK_BOTTOM_RIGHT,

        VERTINDEX_COUNT  // This value will be a count of the previously listed entries.
    };

    XMFLOAT3 avBoundingVertices[ VERTINDEX_COUNT ];

    static const INT VERTS_PER_FACE = 4;

    INT aiFaceOrdering[ DBGPSFACE_COUNT ][ VERTS_PER_FACE ];
};


//----------------------------------------------------------------------------------------------------------------------
// Name: class PlayspaceFrustum
// Desc: Manages the Playspace Frustum, and performs collisions and tests against it for the user.
//----------------------------------------------------------------------------------------------------------------------
class PlayspaceFrustum 
{
public:
    void Init( Sample* pSample, DebugPlayspaceVerts* pDebug = NULL );
    void Update( UINT iTrackedIndex, const NUI_SKELETON_FRAME& frame );

    const PlayerStatus& GetPlayerStatus() const { return m_playerStatus; }

private:
    inline FLOAT DistanceFromSweetSpotEdge( FXMVECTOR vPlayerCenter );
    inline DWORD TestAgainstPlayspaceFrustum( FXMVECTOR vPlayerCenter );
    inline DWORD TestAgainstUsablePlayspaceFrustum( FXMVECTOR vPlayerCenter );

    void UpdateStateMachine();
    inline void EnterState_InSweetSpot( PLAYERSTATE oldState );
    inline void ExitState_LostTracking( PLAYERSTATE newState );
    inline void ExitState_InSweetSpot( PLAYERSTATE newState );
    inline void UpdateState_LostTracking();
    inline void UpdateState_OutsideUsablePlayspace();
    inline void UpdateState_InUsablePlayspace();
    inline void UpdateState_InSweetSpot();

    inline FLOAT CalculateAvatarOpacity() const;

    XMVECTOR m_vBackPlane;
    XMVECTOR m_vBackPlaneUsable;
    XMVECTOR m_vFrontPlane;
    XMVECTOR m_vFrontPlaneUsable;
    XMVECTOR m_vLeftPlane;
    XMVECTOR m_vLeftPlaneEncroach;
    XMVECTOR m_vRightPlane;
    XMVECTOR m_vRightPlaneEncroach;

    PlayerStatus m_playerStatus;
    PLAYERSTATE m_previousState;
    PLAYERSTATE m_currentState;
    DWORD m_dwFramesInState;

    Sample* m_pSample;
    
};

#include "PlayspaceBounds.inl"

#endif //PLAYSPACEBOUNDS_H_GUARD