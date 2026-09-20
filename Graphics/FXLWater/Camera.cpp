//--------------------------------------------------------------------------------------
// Camera.cpp
//
// Simple First Person Camera Implementation
//
// Microsoft Game Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "camera.h"
#include <AtgInput.h>


//--------------------------------------------------------------------------------------
// Name: FirstPersonCamera
// Desc: 
//--------------------------------------------------------------------------------------
class FirstPersonCamera : public Camera
{
public:
                    FirstPersonCamera( const XMVECTOR& fPosition,
                                       FLOAT fYaw,
                                       FLOAT fPitch,
                                       FLOAT fSpeed );
    virtual VOID    Update( FLOAT fElapsedTime );
    virtual UINT    GetPosition( XMVECTOR& vPosition );
    virtual UINT    GetOrientation( XMVECTOR& vQuaternion );

protected:
    XMVECTOR m_vEye;
    bool m_bInvertPitch;
    FLOAT m_fCameraPitchAngle;
    FLOAT m_fCameraYawAngle;
    FLOAT m_bEnableYAxisMovement;
    FLOAT m_fSpeed;
    VOID            UpdateMatrices( FLOAT x1, FLOAT y1, FLOAT x2, FLOAT y2, FLOAT fElapsedTime );
};


//--------------------------------------------------------------------------------------
// Name: FirstPersonCamera()
// Desc: Camera constructor
//--------------------------------------------------------------------------------------
FirstPersonCamera::FirstPersonCamera( const XMVECTOR& fPosition,
                                      FLOAT fYaw,
                                      FLOAT fPitch,
                                      FLOAT fSpeed ) : Camera(),
                                                       m_vEye( fPosition ),
                                                       m_fCameraYawAngle( fYaw ),
                                                       m_fCameraPitchAngle( fPitch ),
                                                       m_fSpeed( fSpeed )
{
    m_bInvertPitch = true;
    m_bEnableYAxisMovement = false;

    UpdateMatrices( 0.f, 0.f, 0.f, 0.f, 0.f );
}


//--------------------------------------------------------------------------------------
// Name: UpdateMatrices()
// Desc: 
//--------------------------------------------------------------------------------------
VOID FirstPersonCamera::UpdateMatrices( FLOAT x1, FLOAT y1,
                                        FLOAT x2, FLOAT y2,
                                        FLOAT fElapsedTime )
{
    // Simple euler method to calculate position delta
    XMVECTOR vPosDelta = XMVectorSet( x1, 0.f, y1, 0.f );
    vPosDelta *= fElapsedTime * m_fSpeed;

    // Update the pitch & yaw angle based on mouse movement
    FLOAT fYawDelta = x2 * 2.f;
    FLOAT fPitchDelta = y2 * 2.f;

    // Invert pitch if requested
    if( m_bInvertPitch )
    {
        fPitchDelta = -fPitchDelta;
    }

    m_fCameraPitchAngle += fPitchDelta * fElapsedTime;
    m_fCameraYawAngle += fYawDelta * fElapsedTime;

    // Limit pitch to straight up or straight down
    m_fCameraPitchAngle = __max( -D3DX_PI / 2.0f, m_fCameraPitchAngle );
    m_fCameraPitchAngle = __min( +D3DX_PI / 2.1f, m_fCameraPitchAngle );

    // Make a rotation matrix based on the camera's yaw & pitch
    XMMATRIX mCameraRot = XMMatrixRotationRollPitchYaw( m_fCameraPitchAngle,
                                                        m_fCameraYawAngle,
                                                        0.f );

    // Transform vectors based on camera's rotation matrix
    XMVECTOR vWorldUp, vWorldAhead;
    XMVECTOR vLocalUp = XMVectorSet( 0.f, 1.f, 0.f, 0.f );
    XMVECTOR vLocalAhead = XMVectorSet( 0.f, 0.f, 1.f, 0.f );
    vWorldUp = XMVector3TransformCoord( vLocalUp, mCameraRot );
    vWorldAhead = XMVector3TransformCoord( vLocalAhead, mCameraRot );

    // Transform the position delta by the camera's rotation 
    XMVECTOR vPosDeltaWorld;
    if( !m_bEnableYAxisMovement )
    {
        // If restricting Y movement, do not include pitch
        // when transforming position delta vector.
        mCameraRot = XMMatrixRotationRollPitchYaw( 0.0f, m_fCameraYawAngle, 0.0f );
    }
    vPosDeltaWorld = XMVector3TransformCoord( vPosDelta, mCameraRot );
    vPosDeltaWorld.w = 0.f;

    // Move the eye position 
    m_vEye += vPosDeltaWorld;

    // Update the lookAt position based on the eye position 
    XMVECTOR vLookAt = m_vEye + vWorldAhead;
    vLookAt.w = 1.f;

    // Update the view matrix
    m_matView = XMMatrixLookAtLH( m_vEye, vLookAt, vWorldUp );
}


//--------------------------------------------------------------------------------------
// Name: Update()
// Desc: 
//--------------------------------------------------------------------------------------
VOID FirstPersonCamera::Update( FLOAT fElapsedTime )
{
    // Get the current gamepad status
    ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput();

    // If no input, early out -
    // most importantly, do not update the view timestamp (or matrix, of course)
    if( __max( fabs( pGamepad->fX1 ), fabs( pGamepad->fY1 ) ) < 0.05f
        && __max( fabs( pGamepad->fX2 ), fabs( pGamepad->fY2 ) ) < 0.05f )
    {
        return;
    }

    UpdateMatrices( pGamepad->fX1, pGamepad->fY1,
                    pGamepad->fX2, pGamepad->fY2,
                    fElapsedTime );

    // Allow objects using the camera view matrix to detect that it has been updated
    // by updating the associated timestamp
    m_nViewTimestamp++;
}


//--------------------------------------------------------------------------------------
// Name: GetOrientation()
// Desc: Get the current orientation quaternion, return the associated timestamp
//--------------------------------------------------------------------------------------
UINT FirstPersonCamera::GetOrientation( XMVECTOR& vQuaternion )
{
    static UINT timestamp = 0;
    return timestamp;
}


//--------------------------------------------------------------------------------------
// Name: GetPosition()
// Desc: Get the current position vector, return the associated timestamp
//--------------------------------------------------------------------------------------
UINT FirstPersonCamera::GetPosition( XMVECTOR& vPosition )
{
    static UINT timestamp = 0;
    vPosition = m_vEye;
    return timestamp;
}


//--------------------------------------------------------------------------------------
// Name: CreateFirstPerson()
// Desc: Creates and returns a first person camera object
//--------------------------------------------------------------------------------------
Camera* Camera::CreateFirstPerson( const XMVECTOR& fPosition,
                                   FLOAT fYaw,
                                   FLOAT fPitch,
                                   FLOAT fSpeed )
{
    return new FirstPersonCamera( fPosition, fYaw, fPitch, fSpeed );
}

