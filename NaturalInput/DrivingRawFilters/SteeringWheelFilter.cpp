//-------------------------------------------------------------------------------------
// SteeringWheelFilter.cpp
//  
// A natural input that interprets joint data in the form of a steering wheel.
//   
// Microsoft XNA Developer Connection
// Copyright (c) Microsoft Corporation. All rights reserved.
//-------------------------------------------------------------------------------------

#include "SteeringWheelFilter.h"
#include <AtgUtil.h>
#include <XBdm.h>

FLOAT g_fDampenSteering = 0.2f;

const static XMVECTOR vXMZero = XMVectorSet( 0.0f, 0.0f, 0.0f, 0.0f );


XMVECTOR g_xmOnlyXY = XMVectorSet(1.0f, 1.0f, 0.0f, 0.0f);

//--------------------------------------------------------------------------------------
// Constructor
//--------------------------------------------------------------------------------------
SteeringWheelFilter::SteeringWheelFilter() :
    m_eConfidence( NUI_SKELETON_POSITION_INFERRED ),
    m_fWheelRotation( 0.0f ),
    m_fMinimumRotationAngleDegrees( -110.0f ),
    m_fMaximumRotationAngleDegrees( 110.0f ),
    m_fZeroAccelerationDistance( 0.2f ),
    m_fMinimumHandBackDistance( 0.10f ),
    m_eArmsState( ARMS_NEUTRAL ),
    m_fDeadZone( 0.001f)
{
    
}


//--------------------------------------------------------------------------------------
// Denstructor
//--------------------------------------------------------------------------------------
SteeringWheelFilter::~SteeringWheelFilter() { }



FILE* fl = NULL;
//--------------------------------------------------------------------------------------
// Updates the filter based on the most recent joints.
//--------------------------------------------------------------------------------------
VOID SteeringWheelFilter::Update( float fElapsedTime, const NUI_SKELETON_DATA* pSkeleton )
{
    // Retreive the relevant joint positions
    const NUI_SKELETON_POSITION_TRACKING_STATE eNeckConfidence = pSkeleton->eSkeletonPositionTrackingState[NUI_SKELETON_POSITION_SHOULDER_CENTER];
    const XMVECTOR vNeck = pSkeleton->SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_CENTER];

    const NUI_SKELETON_POSITION_TRACKING_STATE eSpineConfidence = pSkeleton->eSkeletonPositionTrackingState[NUI_SKELETON_POSITION_SPINE];
    const XMVECTOR vSpine = pSkeleton->SkeletonPositions[NUI_SKELETON_POSITION_SPINE];

    const NUI_SKELETON_POSITION_TRACKING_STATE eRShoulderConfidence = pSkeleton->eSkeletonPositionTrackingState[NUI_SKELETON_POSITION_SHOULDER_RIGHT];
    const XMVECTOR vRShoulder = pSkeleton->SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_RIGHT];

    const NUI_SKELETON_POSITION_TRACKING_STATE eLShoulderConfidence = pSkeleton->eSkeletonPositionTrackingState[NUI_SKELETON_POSITION_SHOULDER_LEFT];
    const XMVECTOR vLShoulder = pSkeleton->SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_LEFT];

    const NUI_SKELETON_POSITION_TRACKING_STATE eLHandConfidence = pSkeleton->eSkeletonPositionTrackingState[NUI_SKELETON_POSITION_HAND_LEFT];
    XMVECTOR vLHand = pSkeleton->SkeletonPositions[NUI_SKELETON_POSITION_HAND_LEFT];

    const NUI_SKELETON_POSITION_TRACKING_STATE eRHandConfidence = pSkeleton->eSkeletonPositionTrackingState[NUI_SKELETON_POSITION_HAND_RIGHT];
    XMVECTOR vRHand = pSkeleton->SkeletonPositions[NUI_SKELETON_POSITION_HAND_RIGHT];

    if ( ( eNeckConfidence == NUI_SKELETON_POSITION_NOT_TRACKED) ||
         ( eSpineConfidence == NUI_SKELETON_POSITION_NOT_TRACKED ) ||
         ( eRShoulderConfidence == NUI_SKELETON_POSITION_NOT_TRACKED ) ||
         ( eLShoulderConfidence == NUI_SKELETON_POSITION_NOT_TRACKED ) )
    {
        m_fWheelRotation = 0.0f;
        m_eConfidence = NUI_SKELETON_POSITION_NOT_TRACKED;
        return;
    }

    // Calculate the up vector for the skeleton based on the joints in the back
    XMVECTOR vUp = XMVector3Normalize( XMVectorSubtract( vNeck, vSpine ) );

    // Calculate the right vector for the skeleton based on the shoulder joints
    XMVECTOR vRight = XMVectorSubtract( vRShoulder, vLShoulder );

    // Calculate a rough forward vector using the up and right vectors
    XMVECTOR vForward = XMVector3Normalize( XMVector3Cross( vUp, vRight  ) );

    // Determine if the hands are in valid driving positions
    const BOOL bIsLeftHandValid = ( eLHandConfidence != NUI_SKELETON_POSITION_NOT_TRACKED ) && 
        IsHandValid( vLHand, vSpine, vForward );
    const BOOL bIsRightHandValid = ( eRHandConfidence != NUI_SKELETON_POSITION_NOT_TRACKED ) && 
        IsHandValid( vRHand, vSpine, vForward );

    // if either hand is not valid.  There is no sense in continuing.
    if ( !bIsLeftHandValid  || !bIsRightHandValid ) {
        m_eConfidence = NUI_SKELETON_POSITION_NOT_TRACKED;        
        return;
    }

    vRHand = vRHand * g_xmOnlyXY;
    vLHand = vLHand * g_xmOnlyXY;

    float fVRHandX = XMVectorGetX( vRHand );
    float fVLHandX = XMVectorGetX( vLHand );
    float fVRHandY = XMVectorGetY( vRHand );
    float fVLHandY = XMVectorGetY( vLHand );

    const XMVECTOR vHands = XMVectorSubtract( vRHand, vLHand );
    m_fWheelRotation = atan2f( XMVectorGetY( vHands ), XMVectorGetX( vHands ));

    // Keep track when right arm goes over left or left over right.  
    // This will help us detect when the angle accidently flips from PI to -PI etc.
    if ( m_eArmsState == ARMS_NEUTRAL )
    {
        if ( fVLHandX > fVRHandX )
        {
            if ( fVLHandY > fVRHandY )
            {
                m_eArmsState = ARMS_LEFT_OVER_RIGHT;
            } else
            {
                m_eArmsState = ARMS_RIGHT_OVER_LEFT;
            }
        }
    } 
    else 
    {

        if ( fVLHandX < fVRHandX )
        {
            m_eArmsState = ARMS_NEUTRAL;
        }
        if ( m_eArmsState == ARMS_LEFT_OVER_RIGHT && m_fWheelRotation > 0 )
        {
            // flip rotation back to -PI and clamp it there
            m_fWheelRotation = -XM_PI;
        }
        else if (m_eArmsState == ARMS_RIGHT_OVER_LEFT && m_fWheelRotation < 0 )
        {
            // flip rotation back to PI and clamp it there
            m_fWheelRotation = XM_PI;
        }

    }
    // fit from -PI:PI to -1:1
    m_fWheelRotation *= -XM_1DIVPI;
    m_fWheelRotation *= g_fDampenSteering;

    m_eConfidence = NUI_SKELETON_POSITION_TRACKED;
}


//--------------------------------------------------------------------------------------
// Update helper function that determines if the given hand position is valid for steering.
//--------------------------------------------------------------------------------------
BOOL SteeringWheelFilter::IsHandValid( FXMVECTOR vHand, FXMVECTOR vLowerBack, FXMVECTOR vForward )
{
    // Calculate how far the hand is from the vLower back, typically representing holding the hands up.
    const XMVECTOR vHandBack = XMVectorSubtract( vHand, vLowerBack );
    const FLOAT fHandDistance = XMVectorGetX( XMVector3Dot( vForward, vHandBack ) );

    bool valid = ( fHandDistance >= m_fMinimumHandBackDistance );

    return valid;
}