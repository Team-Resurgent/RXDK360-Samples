//----------------------------------------------------------------------------------------------------------------------
// PlayspaceBounds.inl
// 
// Inline function implementation for PlayspaceBounds.h
//
// Advanced Technology Group (ATG)
// Copyright (c) Microsoft Corporation. All rights reserved.
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
// Name: PlayspaceFrustum::TestAgainstPlayspaceFrustum
// Desc: Tests against the actual outer edges of the playspace.
//----------------------------------------------------------------------------------------------------------------------
DWORD PlayspaceFrustum::TestAgainstPlayspaceFrustum( FXMVECTOR vPlayerCenter )
{
    DWORD pos = FRUSTUMPOSITION_INSIDE;

    if ( XMVectorGetX( PointInFrontOfPlane( m_vLeftPlane, vPlayerCenter ) ) < 0.0f )
    {
        pos |= FRUSTUMPOSITION_OUT_LEFT;
    }

    if ( XMVectorGetX( PointInFrontOfPlane( m_vRightPlane, vPlayerCenter ) ) < 0.0f )
    {
        pos |= FRUSTUMPOSITION_OUT_RIGHT;
    }

    if ( XMVectorGetX( PointInFrontOfPlane( m_vFrontPlane, vPlayerCenter ) ) < 0.0f )
    {
        pos |= FRUSTUMPOSITION_OUT_FRONT;
    }

    if ( XMVectorGetX( PointInFrontOfPlane( m_vBackPlane, vPlayerCenter ) ) < 0.0f )
    {
        pos |= FRUSTUMPOSITION_OUT_BACK;
    }

    return pos;
}


//----------------------------------------------------------------------------------------------------------------------
// Name: PlayspaceFrustum::TestAgainstUsablePlayspaceFrustum
// Desc: Tests against the outer edges of the /usable/ playspace.
//----------------------------------------------------------------------------------------------------------------------
DWORD PlayspaceFrustum::TestAgainstUsablePlayspaceFrustum( FXMVECTOR vPlayerCenter )
{
    DWORD pos = FRUSTUMPOSITION_INSIDE;

    if ( XMVectorGetX( PointInFrontOfPlane( m_vLeftPlaneEncroach, vPlayerCenter ) ) < 0.0f )
    {
        pos |= FRUSTUMPOSITION_OUT_LEFT;
    }

    if ( XMVectorGetX( PointInFrontOfPlane( m_vRightPlaneEncroach, vPlayerCenter ) ) < 0.0f )
    {
        pos |= FRUSTUMPOSITION_OUT_RIGHT;
    }

    if ( pos == (FRUSTUMPOSITION_OUT_LEFT | FRUSTUMPOSITION_OUT_RIGHT) )
    {
        // This is only possible if we're past the front of the Frustum, so no need for further tests against that
        // plane. (We also reset the Left/Right Frustum test bits, as this will only confuse code which interprets
        // these values).

        pos = FRUSTUMPOSITION_OUT_FRONT;
    }
    else if ( XMVectorGetX( PointInFrontOfPlane( m_vFrontPlaneUsable, vPlayerCenter ) ) < 0.0f )
    {
        pos |= FRUSTUMPOSITION_OUT_FRONT;
    }

    if ( XMVectorGetX( PointInFrontOfPlane( m_vBackPlaneUsable, vPlayerCenter ) ) < 0.0f )
    {
        pos |= FRUSTUMPOSITION_OUT_BACK;
    }

    return pos;
}


//----------------------------------------------------------------------------------------------------------------------
// Name: PlayerStatus::SetUntracked
// Desc: Marks the player status as untracked.
//----------------------------------------------------------------------------------------------------------------------
inline void PlayerStatus::SetUntracked()
{
    m_state = PLAYERSTATE_LOST_TRACKING;
    m_fDistanceFromSweetSpot = -1.0f;
    m_dwNormalPlayspaceFlags = FRUSTUMPOSITION_OUT_BACK | FRUSTUMPOSITION_OUT_FRONT | FRUSTUMPOSITION_OUT_LEFT | FRUSTUMPOSITION_OUT_RIGHT;
    m_dwUsablePlayspaceFlags = FRUSTUMPOSITION_OUT_BACK | FRUSTUMPOSITION_OUT_FRONT | FRUSTUMPOSITION_OUT_LEFT | FRUSTUMPOSITION_OUT_RIGHT;

    m_vLocation = g_XMZero;
}


//----------------------------------------------------------------------------------------------------------------------
// Name: PlayspaceFrustum::CalculateAvatarOpacity
// Desc: Calculates the opacity for the avatar based on its distance from the sweet spot.
//----------------------------------------------------------------------------------------------------------------------
inline FLOAT PlayspaceFrustum::CalculateAvatarOpacity() const
{
    const FLOAT fOpacityRange = SWEET_SPOT_OPACITY_MAX - SWEET_SPOT_OPACITY_MIN;

    FLOAT fOpacityRaw = 1.0f -  ( m_playerStatus.m_fDistanceFromSweetSpot / SWEET_SPOT_FADE_DISTANCE );
    FLOAT fOpacityClamped = (FLOAT)fpmax( fOpacityRaw, 0.0f );
    return ( fOpacityClamped * fOpacityRange ) + SWEET_SPOT_OPACITY_MIN;
}
