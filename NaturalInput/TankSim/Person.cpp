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
#include "Environment.h"
#include <xbox.h>

XMVECTOR g_vUnit = XMVectorSet( 0.0f, 0.0f, 1.0f, 0.0f );
//--------------------------------------------------------------------------------------
// Sets a pointer to the environment.
// Tank needs the environment to do collision detection with terrain
//--------------------------------------------------------------------------------------
VOID Tank::SetEnvironmentPointer( Environment* pEnvironment )
{
    m_pEnvironment = pEnvironment;
}

//--------------------------------------------------------------------------------------
// Update tank physics based on tread.
//--------------------------------------------------------------------------------------
VOID Tank::Update( FLOAT fLeftTread, FLOAT fRightTread )
{
    // scale treads from 0 to 1 to -0.5 to 0.5
    fLeftTread -= 0.5f;
    fRightTread -= 0.5f;
    // If accelerating in the oposite direction then brake
    if ( fLeftTread * m_fVelocity < 0.0f )
    {
        m_fVelocity *= 0.95f;
    }
    if ( fRightTread * m_fVelocity < 0.0f )
    {
        m_fVelocity *= 0.95f;
    }
    // add in velocity based on treads
    m_fVelocity += ( fLeftTread * 0.001f + fRightTread * 0.001f );
    m_fAngularVelocity = ( fLeftTread - fRightTread );
    if ( m_fVelocity > 0 )
        m_fVelocity = max( 0, m_fVelocity - m_fFriction );
    else 
        m_fVelocity = min( 0, m_fVelocity + m_fFriction );

    m_fFacingAngle += m_fAngularVelocity * 0.03f;

    XMVECTOR vAxis = XMVectorSet( 0, 1, 0, 0 );
    XMMATRIX mRotation = XMMatrixRotationAxis( vAxis, m_fFacingAngle );
    m_vFacingDirection = XMVector3Transform( g_vUnit, mRotation );
    m_vPosition+= m_fVelocity * m_vFacingDirection;
    
    // Cheesy ground collision
    XMFLOAT4 fPosition;
    XMStoreFloat4( &fPosition, m_vPosition );
    FLOAT fHeight = 1.0f + (FLOAT)m_pEnvironment->HeightAt( fPosition.x, fPosition.z );
    fPosition.y = fPosition.y * 0.8f + fHeight * 0.2f;
    m_vPosition =  XMLoadFloat4( &fPosition );
    
};

//--------------------------------------------------------------------------------------
// Update the throttle based on the hand position
//--------------------------------------------------------------------------------------
VOID ThrottleControl::Update( XMFLOAT4 fHand )
{
    if ( m_bHandOn )
    {
        XNuiDelayUI( 2000 );
        // The max Y value in the bounding box determines when the throttle disengages
        if ( fHand.y > m_vMax.y )
        {
            // Disengage hand from throttle.
            m_bHandOn = FALSE;
            if ( m_fThrottleValueWhenHandsStartedDetaching != -1.0f )
            {
                m_fThrottleValue = m_fThrottleValueWhenHandsStartedDetaching;             
            }
        }
        else 
        {
            // Move the throttle based on Z
            m_ScalerTrackerHandY.Update( fHand.y );
            m_ScalerTrackerHandZ.Update( fHand.z );
            FLOAT fYsum =  m_ScalerTrackerHandY.m_fLastTwoVelocities[0] + m_ScalerTrackerHandY.m_fLastTwoVelocities[1];
            FLOAT fZsum =  m_ScalerTrackerHandZ.m_fLastTwoVelocities[0] + m_ScalerTrackerHandZ.m_fLastTwoVelocities[1];

            if ( fYsum > 0 && fYsum > fabs( fZsum) )
            {
                // When player begins taking hands of throttle we need to track the Z value
                // so we can snap the throttle back to where it was
                if ( m_fThrottleValueWhenHandsStartedDetaching == -1.0f )
                {
                    if ( m_fThrottleHistory[1] != -1.0f )
                    {
                        m_fThrottleValueWhenHandsStartedDetaching = m_fThrottleHistory[1];                       
                    }
                    else if ( m_fThrottleHistory[0] != -1.0f )
                    {
                        m_fThrottleValueWhenHandsStartedDetaching = m_fThrottleHistory[0];                       
                    }
                    else 
                    {
                        m_fThrottleValueWhenHandsStartedDetaching = m_fThrottleValue;                       
                    }
                }
            }
            else 
            {
                // hand was actually moving forwards so we're not detaching.
                m_fThrottleValueWhenHandsStartedDetaching = -1.0f;                   
            }
            m_fThrottleHistory[1] = m_fThrottleHistory[0];
            m_fThrottleHistory[0] = m_fThrottleValue;
            UpdateThrottleValue( fHand.z );
        }
    }
    else
    {
        // Hand comes on throttle when it's within X range, below Y range, and above Z range.
        if ( fHand.x < m_vMax.x && 
             fHand.x > m_vMin.x &&
             fHand.y <  m_vMax.y &&
             fHand.z > m_vMin.z ) 
        {
            m_ScalerTrackerHandY.Clear();
            m_ScalerTrackerHandY.Update( fHand.y );
            m_ScalerTrackerHandZ.Clear();
            m_ScalerTrackerHandZ.Update( fHand.z );
            m_bHandOn =  TRUE;
            m_fThrottleHistory[0] = -1.0f;
            m_fThrottleHistory[1] = -1.0f;
        }            
    }
};
//--------------------------------------------------------------------------------------
// Set range values.
//--------------------------------------------------------------------------------------
VOID ThrottleControl::Init( XMFLOAT3 vMin, XMFLOAT3 vMax ){
    m_vMin = vMin;
    m_vMax = vMax;
    m_fRangeZ = ( vMax.z - vMin.z );

};

//--------------------------------------------------------------------------------------
// Constructor
//--------------------------------------------------------------------------------------
Person::Person() : 
    m_fLeftHandReletive( FLT_MAX, FLT_MAX ),    
    m_fRightHandReletive( FLT_MAX, FLT_MAX )
{
    m_ThrottleLeft.Init( XMFLOAT3( -0.7f, -10.0f, 0.25f ), XMFLOAT3( 0.7f, 0.1f, .45f ) );
    m_ThrottleRight.Init( XMFLOAT3( -0.7f, -10.0f, 0.25f ), XMFLOAT3( 0.7f, 0.1f, .45f ) );
    m_FilterDouble.Init( 0.5f, 0.8f, 0.5f, 0.05f, 0.04f );
}

//--------------------------------------------------------------------------------------
// Destructor
//--------------------------------------------------------------------------------------
Person::~Person() 
{

}

// global helper variables 
static const XMVECTOR g_vPreserveXZ = XMVectorSet( 1.0f, 0.0f, 1.0f, 0.0f );
static const XMVECTOR g_vWorldBoundMin = XMVectorSet( -FLT_MAX, -FLT_MAX, 1.9, -FLT_MAX );
static const XMVECTOR g_vWorldBoundMax = XMVectorSet( FLT_MAX, FLT_MAX, 3.5, FLT_MAX );

//--------------------------------------------------------------------------------------
// Update joints and pass them into the helper functions
//--------------------------------------------------------------------------------------

VOID Person::UpdateNUI ( const NUI_SKELETON_DATA* pSkeletonData )
{
    const NUI_SKELETON_POSITION_TRACKING_STATE* pTracking = pSkeletonData->eSkeletonPositionTrackingState;

    if (pSkeletonData->eTrackingState == NUI_SKELETON_TRACKED)
    {
        if ( 
            pTracking[ NUI_SKELETON_POSITION_SPINE ] != NUI_SKELETON_POSITION_NOT_TRACKED &&
            pTracking[ NUI_SKELETON_POSITION_SHOULDER_LEFT ] != NUI_SKELETON_POSITION_NOT_TRACKED &&
            pTracking[ NUI_SKELETON_POSITION_SHOULDER_RIGHT ] != NUI_SKELETON_POSITION_NOT_TRACKED &&
            pTracking[ NUI_SKELETON_POSITION_WRIST_RIGHT ] != NUI_SKELETON_POSITION_NOT_TRACKED 
           ) 
        {
            // Add new frame of data to filter
            m_FilterDouble.Update( pSkeletonData ) ;
            // returns filtered joints
            XMVECTOR* pFilteredJoints = m_FilterDouble.GetFilteredJoints();
            // Add filtered joits to body reletive coordinate system
            m_BodyCoord.Update( pFilteredJoints );

            // Get back hands
            XMVECTOR vRightHandReletive = m_BodyCoord.GetRightHandReletive();
            XMFLOAT4 fRightHandReletive;
            XMVECTOR vLeftHandReletive = m_BodyCoord.GetLeftHandReletive();
            XMFLOAT4 fLeftHandReletive;
            XMStoreFloat4( &fRightHandReletive, vRightHandReletive );
            XMStoreFloat4( &fLeftHandReletive, vLeftHandReletive );

            // Pass hands into throttle
            m_ThrottleLeft.Update( fLeftHandReletive ); 
            m_ThrottleRight.Update( fRightHandReletive ); 
            
            // Calculate hands in scren spacee
            m_fvRightHandScreenSpace.x = fRightHandReletive.x;
            m_fvRightHandScreenSpace.y = fRightHandReletive.y;
            m_fvLeftHandScreenSpace.x = fLeftHandReletive.x;
            m_fvLeftHandScreenSpace.y = fLeftHandReletive.y;
            
            // Update tank physics
            m_Tank.Update( m_ThrottleLeft.m_fThrottleValue, m_ThrottleRight.m_fThrottleValue );
        }
    }
    else
    {
        m_FilterDouble.Reset();
    }
}



