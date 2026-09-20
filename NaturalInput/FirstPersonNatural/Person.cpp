//-------------------------------------------------------------------------------------
// Person.cpp
//  
// Represents the first person camera the user is controlling in this sample.
//  
// Microsoft XNA Developer Connection
// Copyright (c) Microsoft Corporation. All rights reserved.
//-------------------------------------------------------------------------------------

#include "Person.h"
#include <xnamath.h>

//--------------------------------------------------------------------------------------
// Constructor
//--------------------------------------------------------------------------------------
Person::Person() : 
    m_vPosition( 14.0f, 6.0f, 5.0f ), // Location of the user
    m_vFacing( 0.0f, 0.0f, 1.0f ), // Direction user is facing
    m_fDeadZoneForwardBackward( 0.2f ), // Dead zone for froward backward motion
    m_fDeadZoneTurn( 0.01f ), // Very small deadzone for turning
    m_fForwardSpeed( 0.0f ), // Initialize to stationary
    m_fStrafeSpeed( 0.0f), // Initialize to stationary
    m_fTurnAngleLeftRightDirection( 0 ), // Direction the user is twisting to turn
    m_fTurnAngleUpDown( 0 ), // Look up and down angle
    m_fTurnDampening( 0.1f ), // Turn sensitivity variable
    m_fSpeedMultiplier( 3.0f ), // Speed sensitivity variable
    m_fHeight ( 0.0f ), // Height of user calculated from skeleton
    m_fTurnAnglePlayerFacingDirection( 0.0f ), // Variable used by GUI visualization
    m_fStrafeSpeedDeadZone( 0.01f ), // Small dead zone to keep straffing from wandering all over
    m_fStrafeSpeedMultiplier( 10.0f ) // Strafe speed
{
    m_vSpineMin = XMVectorSet( -0.3f, -0.3f, 2.1, FLT_MAX ); // The forward backward is calculated by the bounds of the world
    m_vSpineMax = XMVectorSet( 0.3f, 0.3f, 2.4, -FLT_MAX );
    m_JointFilter.Init( 0.5f, 0.8f, 0.5f, 0.05f, 0.04f );
}

//--------------------------------------------------------------------------------------
// Destructor
//--------------------------------------------------------------------------------------
Person::~Person() 
{

}

// Helper variables to 0 out Y and W
XMVECTOR g_vPreserveXZ = XMVectorSet( 1.0f, 0.0f, 1.0f, 0.0f );
// play space in NUI camera coordinates
XMVECTOR g_vWorldBoundMin = XMVectorSet( -FLT_MAX, -FLT_MAX, 1.9, -FLT_MAX );
XMVECTOR g_vWorldBoundMax = XMVectorSet( FLT_MAX, FLT_MAX, 3.5, FLT_MAX );

//--------------------------------------------------------------------------------------
// UpdateNUI,  when the NUI apis return a new skeleton the real work of itnerpreting the user control is done here
//--------------------------------------------------------------------------------------
VOID Person::UpdateNUI ( const NUI_SKELETON_DATA* pSkeletonData )
{
    if( pSkeletonData->eTrackingState == NUI_SKELETON_TRACKED )
    {
        // If the joints are all high confidence proceed
        if( pSkeletonData->eSkeletonPositionTrackingState[ NUI_SKELETON_POSITION_SPINE ] != 
                NUI_SKELETON_POSITION_NOT_TRACKED &&
            pSkeletonData->eSkeletonPositionTrackingState[ NUI_SKELETON_POSITION_SHOULDER_LEFT ] != 
                NUI_SKELETON_POSITION_NOT_TRACKED &&
            pSkeletonData->eSkeletonPositionTrackingState[ NUI_SKELETON_POSITION_SHOULDER_RIGHT ] != 
                NUI_SKELETON_POSITION_NOT_TRACKED &&
            pSkeletonData->eSkeletonPositionTrackingState[ NUI_SKELETON_POSITION_HAND_RIGHT ] != 
                NUI_SKELETON_POSITION_NOT_TRACKED                                                ) 
        {
            
            m_JointFilter.Update( pSkeletonData );
            XMVECTOR* pFilteredJoints = m_JointFilter.GetFilteredJoints();

            // Clip the spine to the play area
            XMVECTOR vBoundModifiedSpine = pFilteredJoints[ NUI_SKELETON_POSITION_SPINE ];
            XMVECTOR vLeftShoulder = pFilteredJoints[ NUI_SKELETON_POSITION_SHOULDER_LEFT ];
            XMVECTOR vRightShoulder = pFilteredJoints[ NUI_SKELETON_POSITION_SHOULDER_RIGHT ];
            XMVECTOR vCenterShoulder = pFilteredJoints[ NUI_SKELETON_POSITION_SHOULDER_CENTER ];            
            XMVECTOR vSpine = pFilteredJoints[NUI_SKELETON_POSITION_SPINE];
            XMVECTOR vCenterHip = pFilteredJoints[ NUI_SKELETON_POSITION_HIP_CENTER ];
            
            m_vSpineMin = XMVectorMin( m_vSpineMin, XMVectorMax( g_vWorldBoundMin, vBoundModifiedSpine ) );
            m_vSpineMax = XMVectorMax( m_vSpineMax, XMVectorMin( g_vWorldBoundMax, vBoundModifiedSpine ) );
            vBoundModifiedSpine = XMVectorMin( XMVectorMax( vBoundModifiedSpine, g_vWorldBoundMin ), g_vWorldBoundMax ); 
            
            // Calculate the forward backward and stafe dead zones
            XMVECTOR vDiff = m_vSpineMax - m_vSpineMin;
            XMVECTOR vDeadZoneValues = XMVectorSet( 1.0f, 0, 1.0f - m_fDeadZoneForwardBackward, 0 );
            XMVECTOR vDeadZone = vDiff * ( vDeadZoneValues );
            vDeadZone *= 0.5f;
            XMVECTOR vDeadZoneMin = m_vSpineMin + vDeadZone;
            XMVECTOR vDeadZoneMax = m_vSpineMax - vDeadZone;
            FLOAT fDeadZoneMinZ = XMVectorGetZ( vDeadZoneMin );
            FLOAT fDeadZoneMaxZ = XMVectorGetZ( vDeadZoneMax );
            FLOAT fBoundedSpineZ = XMVectorGetZ( vBoundModifiedSpine );

            // Calculate speed based on location within the play area taking into account dead zone
            if ( fBoundedSpineZ > fDeadZoneMaxZ )
            {
                FLOAT fSpineMaxZ = XMVectorGetZ( m_vSpineMax );
                m_fForwardSpeed = ( fBoundedSpineZ - fDeadZoneMaxZ ) / ( fSpineMaxZ - fDeadZoneMaxZ );
                m_fForwardSpeed *= -1;
            }
            else if ( fBoundedSpineZ < fDeadZoneMinZ )
            {
                FLOAT fSpineMinZ = XMVectorGetZ( m_vSpineMin );
                m_fForwardSpeed = 1.0f - ( ( fSpineMinZ - fBoundedSpineZ ) / ( fSpineMinZ - fDeadZoneMinZ ) );
            }
            m_fForwardSpeed *= 0.75f;

            FLOAT fXCenterOfMass = 0;
            fXCenterOfMass += XMVectorGetX( vLeftShoulder );
            fXCenterOfMass += XMVectorGetX( vRightShoulder );
            fXCenterOfMass += XMVectorGetX( vCenterShoulder );
            fXCenterOfMass *= 0.33333333334;
            FLOAT fBoundedSpineX = XMVectorGetX( vBoundModifiedSpine );
            m_fStrafeSpeed = fXCenterOfMass - fBoundedSpineX;
            // Clamp strafe speed, subtract out deadzone to make it smooth
            if ( fabs( m_fStrafeSpeed ) < m_fStrafeSpeedDeadZone ) {
                m_fStrafeSpeed = 0.0f;
            }else {
               if ( m_fStrafeSpeed > m_fStrafeSpeedDeadZone ) m_fStrafeSpeed -= m_fStrafeSpeedDeadZone;
               if ( m_fStrafeSpeed < -m_fStrafeSpeedDeadZone ) m_fStrafeSpeed += m_fStrafeSpeedDeadZone;
            }
            m_fStrafeSpeed *= m_fStrafeSpeedMultiplier;
            
            // Calculate look up/down angle
            FLOAT fZCenterOfMass = 0;
            fZCenterOfMass += XMVectorGetZ( vLeftShoulder );
            fZCenterOfMass += XMVectorGetZ( vRightShoulder );
            fZCenterOfMass += XMVectorGetZ( vCenterShoulder );
            fZCenterOfMass *= 0.33333333334;
            FLOAT fNewAngle = 10.0f * ( XMVectorGetZ(vSpine) - fZCenterOfMass );

            // Average the angle a lot.  latency isn't important.
            m_fTurnAngleUpDown = fNewAngle * 0.2f + m_fTurnAngleUpDown * 0.8f;
            
            // Calculate the left right turning based on shoulders
            XMVECTOR vShoulderDiff = vLeftShoulder - vRightShoulder;
            float fTurnAmount =  
                ( XM_PIDIV2 + atan2f( XMVectorGetX( vShoulderDiff ), XMVectorGetZ( vShoulderDiff ) ) ); 
            m_fTurnAnglePlayerFacingDirection = fTurnAmount;
            fTurnAmount *= m_fTurnDampening;
                
            // Clamp to the dead zone
            if ( fabsf( fTurnAmount) >= m_fDeadZoneTurn )
            {
                if ( fTurnAmount > m_fDeadZoneTurn )
                {
                    fTurnAmount -= m_fDeadZoneTurn;
                }else 
                {
                    fTurnAmount += m_fDeadZoneTurn;
                }
                m_fTurnAngleLeftRightDirection -= fTurnAmount;
            }
            // Calculate height for a little bit more realistic feel.  This allows for jumping etc.
            FLOAT fNewHeight = XMVectorGetY( vSpine );
            fNewHeight += XMVectorGetY ( vCenterShoulder );
            fNewHeight += XMVectorGetY ( vCenterHip );
            fNewHeight *= .33334;
            fNewHeight *= 4.0f;

            if ( m_fHeight == 0.0f )
            {
                m_fHeight = fNewHeight;    
            }
            else {
                m_fHeight = m_fHeight * 0.5f +  fNewHeight * 0.5f;
            }
            
        }
    }
    else
    {
        m_JointFilter.Reset();
    }
}

//--------------------------------------------------------------------------------------
// Updates the Person for the elapsed frame.
//--------------------------------------------------------------------------------------
VOID Person::Update( FLOAT fElapsedTime )
{

    // Calculate the current direction the person is facing
    XMMATRIX matFacing = XMMatrixRotationRollPitchYaw( m_fTurnAngleUpDown, m_fTurnAngleLeftRightDirection + -XM_PIDIV2, 0 );
    XMVECTOR vUnit = XMVectorSet( 0.0f, 0.0f, 1.0f, 0.0f );
    XMVECTOR xmFacing = XMVector3Transform( vUnit, matFacing );
    XMStoreFloat3( &m_vFacing, xmFacing );
    XMVECTOR vFacing = XMLoadFloat3( &m_vFacing );
    vFacing *= g_vPreserveXZ;
    // Calculate the new position

    XMVECTOR vPosition = XMLoadFloat3( &m_vPosition );
    XMVECTOR vUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f );
    XMVECTOR vRight = XMVector3Cross( vFacing, vUp );
    vRight = XMVectorSetY( vRight, 0 );
    vRight = -XMVector3Normalize( vRight );
    
    if ( fElapsedTime > 0.0f )
    {
        vPosition += vFacing * m_fForwardSpeed * m_fSpeedMultiplier * fElapsedTime;
        vPosition += vRight * m_fStrafeSpeed * m_fSpeedMultiplier * fElapsedTime;
    }
    // Fix up height
    vPosition.y = m_fHeight + 5.5f;
    XMStoreFloat3( &m_vPosition, vPosition );

    // Perform hard coded collision detection
    if( m_vPosition.x > 16 )
        m_vPosition.x = 16;
    if( m_vPosition.x < -16 )
        m_vPosition.x = -16;
    if( m_vPosition.y > 8.5 )
        m_vPosition.y = 8.5;
    if( m_vPosition.y < 4.9 )
        m_vPosition.y = 4.9;
    if( m_vPosition.z > 7 )
        m_vPosition.z = 7;
    if( m_vPosition.z < -7 )
        m_vPosition.z = -7;
    if( ( m_vPosition.x < 11.80f ) && ( m_vPosition.x > -11.80f ) &&
        ( m_vPosition.z < 3.4f ) && ( m_vPosition.z > -3.4f ) )
    {
        float xsm1 = fabs ( 11.80f  - m_vPosition.x);
        float xsm2 = fabs ( -11.80f - m_vPosition.x);
        
        float ysm1 = fabs ( 3.4f  - m_vPosition.z);
        float ysm2 = fabs ( -3.4f - m_vPosition.z);

        if ( xsm1  < xsm2 && xsm1 < ysm1 && xsm1 < ysm2 )
        {
            m_vPosition.x = 11.80f;
        }
        else if (xsm2 < ysm1 && xsm2 < ysm2 )
        {
            m_vPosition.x = -11.80f;
        }
        else if ( ysm1 < ysm2 )
        {
            m_vPosition.z = 3.4f;
        }
        else 
        {
            m_vPosition.z = -3.4f;
        }
    }

}

