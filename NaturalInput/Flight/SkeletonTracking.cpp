//--------------------------------------------------------------------------------------
// File: SkeletonTracking.cpp
//
// A thin wrapper around the NUI API that converts skeleton data into flight control
// inputs.
// 
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "SkeletonTracking.h"

const FLOAT g_fFacingForwardConstant = 0.2f;

//--------------------------------------------------------------------------------------
// Constructor that sets up all of the calibration constants
//--------------------------------------------------------------------------------------
SkeletonTracking::SkeletonTracking()
{
    ZeroMemory( &m_FlightStatus, sizeof(FlightStatus) );

    // Establish dead zone for pitch, this is based on the lean forward/backward angle in radians
    m_FlightStatus.m_fYAxisDeadzone = 0.005f;
    m_FlightStatus.m_fMaxYAxis = 0.3f;
    
    // Establish dead zone for yaw, this is based on the lean left/right angle in radians
    m_FlightStatus.m_fXAxisDeadzone = 0.01f;
    m_FlightStatus.m_fMaxXAxis = 0.2f;

    // Establish dead zone for throttle, this is based on the amount that the right leg points forward
    m_FlightStatus.m_fThrottleDeadzone = .35f;
    m_FlightStatus.m_fMaxThrottle = 0.5f;

    // Establish a resting spine angle - the angle off vertical of the spine, in radians
    m_FlightStatus.m_fRestingSpineAngle = 0.0f;

}


//--------------------------------------------------------------------------------------
// Executes joint tracking in synchronous mode, and runs the raw filter to generate control values
//--------------------------------------------------------------------------------------
HRESULT SkeletonTracking::Update( NUI_SKELETON_DATA* pSkeleton)
{
    RecognizeFlightGestures( pSkeleton );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Helper function that applies deadzone and bias to a value
//--------------------------------------------------------------------------------------
FLOAT FilterValue( FLOAT fRawValue, FLOAT fDeadZone, FLOAT fMaxValue )
{
    if( fabs( fRawValue ) <= fDeadZone )
    {
        fRawValue = 0.0f;
    }
    FLOAT fScaledValue = fRawValue / fMaxValue;
    return max( -1.0f, min( 1.0f, fScaledValue ) );
}


//--------------------------------------------------------------------------------------
// Converts skeleton state to flight controls
//--------------------------------------------------------------------------------------
VOID SkeletonTracking::RecognizeFlightGestures( NUI_SKELETON_DATA* pSkeleton )
{

    // Examine confidence for spine, exit if we don't have good values
    if( pSkeleton->eSkeletonPositionTrackingState[NUI_SKELETON_POSITION_SHOULDER_CENTER] == NUI_SKELETON_POSITION_NOT_TRACKED
        || pSkeleton->eSkeletonPositionTrackingState[NUI_SKELETON_POSITION_HIP_CENTER] == NUI_SKELETON_POSITION_NOT_TRACKED )
    {
        m_FlightStatus.m_eConfidence = NUI_SKELETON_POSITION_NOT_TRACKED;
        m_FlightStatus.m_fThrottle = 0.0f;
        m_FlightStatus.m_fYAxis = 0.0f;
        m_FlightStatus.m_fXAxis = 0.0f;
        return;
    }

    // Establish a coordinate system for the center of the body
    XMVECTOR vBodyRight = pSkeleton->SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_RIGHT]
        - pSkeleton->SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_LEFT];
    
    XMVECTOR vBodyUp = XMVector3Normalize( 
        pSkeleton->SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_CENTER]
    - pSkeleton->SkeletonPositions[NUI_SKELETON_POSITION_HIP_CENTER] );
    
    XMVECTOR vBodyForward = XMVector3Normalize( XMVector3Cross( vBodyUp, vBodyRight ) );

    static const XMVECTOR vRight = { 1, 0, 0, 0 };
    static const XMVECTOR vUp = { 0, 1, 0, 0 };
    static const XMVECTOR vForward = { 0, 0, 1, 0 };

    // Compute Y axis input from body up vector
    XMVECTOR vBodyUpOnYZPlane = XMVector3Normalize( vBodyUp - XMVector3Dot( vBodyUp, vRight ) * vRight );
    XMVECTOR vPitchUp = XMVector3Dot( vBodyUpOnYZPlane, vUp );
    XMVECTOR vPitchForward = XMVector3Dot( vBodyUpOnYZPlane, vForward );
    vPitchForward /= XMVectorAbs( vPitchForward );
    m_FlightStatus.m_fYAxis = acosf( XMVectorGetX( vPitchUp ) ) * XMVectorGetX( vPitchForward );
    m_FlightStatus.m_fYAxis -= m_FlightStatus.m_fRestingSpineAngle;
    
    m_FlightStatus.m_fYAxis = FilterValue( m_FlightStatus.m_fYAxis, m_FlightStatus.m_fYAxisDeadzone, m_FlightStatus.m_fMaxYAxis );


    // Compute X axis input from body right vector
    if( pSkeleton->eSkeletonPositionTrackingState[NUI_SKELETON_POSITION_SHOULDER_RIGHT] != NUI_SKELETON_POSITION_NOT_TRACKED
        && pSkeleton->eSkeletonPositionTrackingState[NUI_SKELETON_POSITION_SHOULDER_LEFT] != NUI_SKELETON_POSITION_NOT_TRACKED )
    {
        XMVECTOR vBodyRightOnXYPlane = XMVector3Normalize( vBodyRight - XMVector3Dot( vBodyRight, vForward ) * vForward );
        XMVECTOR vRollUp = XMVector3Dot( vBodyRightOnXYPlane, vUp );
        XMVECTOR vRollRight = XMVector3Dot( vBodyRightOnXYPlane, vRight );
        vRollUp /= XMVectorAbs( vRollUp );
        m_FlightStatus.m_fXAxis = acosf( XMVectorGetX( vRollRight ) ) * XMVectorGetX( vRollUp );
        m_FlightStatus.m_fXAxis = FilterValue( m_FlightStatus.m_fXAxis, m_FlightStatus.m_fXAxisDeadzone, m_FlightStatus.m_fMaxXAxis );
    }
    else
    {
        m_FlightStatus.m_fXAxis = 0.0f;
    }

    // Compute throttle from right leg
    FLOAT fCalculatedThrottle = 0.0f;
    XMVECTOR vFlatBodyForward = XMVector3Normalize( XMVectorMultiply( vBodyForward, XMVectorSet( 1, 1, 1, 0 ) ) );
    XMVECTOR vLeg;
    XMVECTOR vLegNormalize;

    // Calculate the throttle by measuring the vector between feet and dotting it against the forward vector.
    // There are several checks for things that can cause throttle to trigger erroneously.

    // check confidence of joints
    if( pSkeleton->eSkeletonPositionTrackingState[NUI_SKELETON_POSITION_KNEE_RIGHT] != NUI_SKELETON_POSITION_NOT_TRACKED 
        && pSkeleton->eSkeletonPositionTrackingState[NUI_SKELETON_POSITION_KNEE_LEFT] != NUI_SKELETON_POSITION_NOT_TRACKED
        )
    {
        if ( fabs ( m_FlightStatus.m_fXAxis ) < 0.95 )  // if body is facing forward we'll check for throttle
        {
            // When the user is turning hard sometimes the throttle will cut out due to the posture
            // Make sure the user isn't banking hard before we update the throttle
            if ( fabs( m_FlightStatus.m_fXAxis ) < 1.0f ) 
            {
                vLeg = pSkeleton->SkeletonPositions[NUI_SKELETON_POSITION_KNEE_RIGHT]
                       - pSkeleton->SkeletonPositions[NUI_SKELETON_POSITION_KNEE_LEFT];
                vLegNormalize = XMVector3Normalize( vLeg ); 
                fCalculatedThrottle = XMVectorGetX( XMVector3Dot( vLegNormalize, vFlatBodyForward ) );
            }
        }
    }

    FLOAT fRawThrottle = fCalculatedThrottle;
    m_FlightStatus.m_fThrottle = FilterValue( fRawThrottle, m_FlightStatus.m_fThrottleDeadzone, m_FlightStatus.m_fMaxThrottle );

    m_FlightStatus.m_eConfidence = NUI_SKELETON_POSITION_TRACKED;
}
