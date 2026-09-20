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
    m_fTurnAngleLeftRightRealTime( 0.0f ), // The user orientation angle when turning is engaged
    m_fTurnAngleLeftRightResidual( -XM_PIDIV2 ), // Stores the angle the users last rotated too
    m_fTurnAngleUpDown( 0.0f ), // Look up and down angle
    m_fTurnDampening( 0.1f ), // Turn multiplier
    m_fSpeedMultiplier( 3.0f ), // Speed sensitivity variable
    m_fHandHeightToEngageLeftRightTurning( 0.0f ), // When the Right hand goes above this point turning is engaged.
    m_dwWheelColor( 0xFFFF0000 ), // Draw the wheel in green to show that it's engaged.  fade from Red to orange to show how close before wheel is engaged
    m_fRightHandMin( FLT_MAX ), // Smallest right hand value
    m_bHandOnWheel( FALSE ), // Used by UI to show when hand is engaged with wheel
    m_fStrafeSpeedDeadZone( 0.01f ), // Small dead zone to keep straffing from wandering all over
    m_fSpineOffsetX( 0.0f ) // Store the Spine Offset from the time wheel engaged to give an offset to make thigns reletive
{
    m_vSpineMin = XMVectorSet( -0.3f, -0.3f, 2.1, FLT_MAX ); // the forward backward is calculated by the bounds of the world
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
            // Calcualte the hand y value where turning is engaged.
            XMVECTOR* pFilteredJoints = m_JointFilter.GetFilteredJoints();

            XMVECTOR vElbowRight = pFilteredJoints[ NUI_SKELETON_POSITION_ELBOW_RIGHT ];
            XMVECTOR vHandRight = pFilteredJoints[ NUI_SKELETON_POSITION_HAND_RIGHT ];
            XMVECTOR vSpine = pFilteredJoints[ NUI_SKELETON_POSITION_SPINE ];
            XMVECTOR vShoulderRight = pFilteredJoints[ NUI_SKELETON_POSITION_SHOULDER_RIGHT ];
            XMVECTOR vShoulderLeft = pFilteredJoints[ NUI_SKELETON_POSITION_SHOULDER_LEFT ];
            XMVECTOR vShoulderCenter = pFilteredJoints[ NUI_SKELETON_POSITION_SHOULDER_CENTER ];

            XMVECTOR vRShoulderRElbow = XMVector3Normalize( vElbowRight - vShoulderRight );
            XMVECTOR vRElbowRHand = XMVector3Normalize( vHandRight - vElbowRight  );
            
            XMVECTOR vDown = XMVectorSet(0.0f, -1.0f, 0.0f, 0.0f );
            XMVECTOR vShoulder = XMVector3Dot( vRShoulderRElbow, vDown ); 
            XMVECTOR vElbow = XMVector3Dot( vRElbowRHand, vDown ); 
            // If the Shoulder and arm are pointed sufficiently down then we can calculate the "m_fHandHeightToEngageLeftRightTurning"
            if ( XMVectorGetX( vShoulder ) > 0.8f && XMVectorGetX( vElbow ) > 0.5f )
            {
                FLOAT newHeight = ( 0.5f * XMVectorGetY( vHandRight ) + 0.5f * XMVectorGetY( vElbowRight ) );
                m_fHandHeightToEngageLeftRightTurning = newHeight; 
            }
            
            // Clip the spine to the play area
            XMVECTOR vBoundModifiedSpine = vSpine;
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
            FLOAT fBoundModifiedSpineZ = XMVectorGetZ( vBoundModifiedSpine );
            
            // Calculate speed based on location within the play area taking into account dead zone
            if ( fBoundModifiedSpineZ > fDeadZoneMaxZ )
            {
                FLOAT fSpineMaxZ = XMVectorGetZ( m_vSpineMax );
                m_fForwardSpeed = ( fBoundModifiedSpineZ - fDeadZoneMaxZ ) / ( fSpineMaxZ - fDeadZoneMaxZ );
                m_fForwardSpeed *= -1;
            }
            else if ( fBoundModifiedSpineZ < fDeadZoneMinZ )
            {
                FLOAT fSpineMinZ = XMVectorGetZ( m_vSpineMin );
                m_fForwardSpeed = 1.0f - ( ( fSpineMinZ - fBoundModifiedSpineZ ) / ( fSpineMinZ - fDeadZoneMinZ ) );
            }

            // Calculate strafe based on how far user is leaning left or right
            // Average 3 shoulder joints
            FLOAT fXCenterOfMass = 0;
            fXCenterOfMass += XMVectorGetX( vShoulderLeft );
            fXCenterOfMass += XMVectorGetX( vShoulderRight );
            fXCenterOfMass += XMVectorGetX( vShoulderCenter );
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
            m_fStrafeSpeed *= 18.0f;
            
            // Calculate look up/down angle
            FLOAT fZCenterOfMass = 0;
            fZCenterOfMass += XMVectorGetZ( vShoulderLeft );
            fZCenterOfMass += XMVectorGetZ( vShoulderRight );
            fZCenterOfMass += XMVectorGetZ( vShoulderCenter );
            fZCenterOfMass *= 0.33333333334;
            FLOAT fNewAngle = 10.0f * ( XMVectorGetZ(vSpine) - fZCenterOfMass );

            // Average the angle a lot.  latency isn't important.
            m_fTurnAngleUpDown = fNewAngle * 0.2f + m_fTurnAngleUpDown * 0.8f;

            FLOAT fRightHandY = XMVectorGetY( vHandRight );

            // detect when users is using arms to scroll 
            m_fRightHandMin = min( m_fRightHandMin, fRightHandY );
            
            // When Right hand raised above this point, we use the X location to turn left/right
            if ( fRightHandY > m_fHandHeightToEngageLeftRightTurning )
            {
                // Wheel engaged so draw it green
                m_dwWheelColor = 0xFF00FF00;
                if ( !m_bHandOnWheel )
                {
                    // If just engaged, calculate offsets to keep the rotation reletive to the spine.
                    m_fSpineOffsetX = XMVectorGetX( vHandRight ) - XMVectorGetX( vSpine );
                }
                
                // calculate turn amount
                XMVECTOR vPivotMeasure = XMVectorSet( m_fSpineOffsetX, 0, 0, 0 );
                vPivotMeasure += vSpine;
                XMVECTOR vSpineDiff = vPivotMeasure - vHandRight;
                float fTurnAmount = 4.0f *  XMVectorGetX( vSpineDiff ); 
                m_fTurnAngleLeftRightRealTime = - fTurnAmount;
                m_bHandOnWheel = TRUE;
            } 
            else 
            {
                // We're not turning. Add in last turn angle and zero it
                m_fTurnAngleLeftRightResidual += m_fTurnAngleLeftRightRealTime;
                m_fTurnAngleLeftRightRealTime = 0.0f;

                //GUI needs turn ratios and the wheel color for visualization
                FLOAT fPercent = ( fRightHandY - m_fRightHandMin ) / ( m_fHandHeightToEngageLeftRightTurning - m_fRightHandMin );
                DWORD dwPercent = min( 255,  (INT) ( fPercent * 255.0f ) );
                m_dwWheelColor = 0xFFFF0000 + ( dwPercent << 8 );
                m_bHandOnWheel = FALSE;
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
    XMMATRIX matFacing = XMMatrixRotationRollPitchYaw( m_fTurnAngleUpDown, m_fTurnAngleLeftRightResidual + m_fTurnAngleLeftRightRealTime, 0 );
    XMVECTOR vUnit = XMVectorSet( 0.0f, 0.0f, 1.0f, 0.0f );
    XMVECTOR xmFacing = XMVector3Transform( vUnit, matFacing );
    XMStoreFloat3( &m_vFacing, xmFacing );
    XMVECTOR vFacing = XMLoadFloat3( &m_vFacing );
    vFacing *= g_vPreserveXZ;
    // Calculate the new position
    XMVECTOR vPosition = XMLoadFloat3( &m_vPosition );
    XMVECTOR vUp = XMVectorSet( 0.0f, 1.0f, 0.0f, 0.0f );
    XMVECTOR vRight = XMVector3Cross( vFacing, vUp );
    vRight = XMVectorSetY( vRight, 0 );
    vRight = -XMVector3Normalize( vRight );
    if ( fElapsedTime > 0.0f )
    {
        vPosition += vFacing * m_fForwardSpeed * m_fSpeedMultiplier * fElapsedTime;
        vPosition += vRight * m_fStrafeSpeed * m_fSpeedMultiplier * fElapsedTime;
    }
    
    XMStoreFloat3( &m_vPosition, vPosition );

    // Perform collision detection 
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

