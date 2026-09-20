//--------------------------------------------------------------------------------------
// File: ThereminFilters.cpp
//
// Detects distance between each hand and default points in space for each hand, and 
// uses those distances to simulate a theremin. Left hand distance reflects volume,
// and right hand controls pitch.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include <xtl.h>
#include "ThereminFilters.h"
#include <xnamath.h>
#include <math.h>
#include <AtgFont.h>
#include <stdio.h>

// Meter to Inch conversion factor
static const FLOAT METERS_PER_INCH = 0.0254f;

// Frequency limits for audio output.
static const FLOAT MAX_FREQUENCY = 1046.50;   // in Hz; musical note C6
static const FLOAT MIN_FREQUENCY = 130.81;    // in Hz; musical note C3

// Log values used to linearize frequencies ranges.
static const FLOAT LOG2MIN_FREQ = log( MIN_FREQUENCY ) / log(2.0f);
static const FLOAT LOG2MAX_FREQ = log( MAX_FREQUENCY ) / log(2.0f);

// Amplitude range for audio output.
static const FLOAT MIN_VOL = 0.0;
static const FLOAT MAX_VOL = 1.0;

// Elbow-joint tracking constants:

// Angle range for right-arm (frequency)
static const FLOAT MIN_FREQ_ANGLE = XMConvertToRadians(90.0);
static const FLOAT MAX_FREQ_ANGLE = XMConvertToRadians(140.0);

// Angle range for left-arm (volume)
static const FLOAT MIN_VOL_ANGLE = XMConvertToRadians(90.0);
static const FLOAT MAX_VOL_ANGLE = XMConvertToRadians(140.0);

// Virtual Theremin tracking constants:
// 
// These are the defaults that are used for the virtual theremin positioning while it
// is waiting for arm lengths to settle. 
//
// Virtual antenna distance from player's waist; roughly 20"
static const FLOAT VIRTUAL_ANTENNA_DISTANCE = 15.0f * METERS_PER_INCH;

// Hand distances from the virtual antennae that correspond to the play-range.
static const FLOAT VIRTUAL_PITCH_MIN_HAND_DIST = 1.0f * METERS_PER_INCH;
static const FLOAT VIRTUAL_AMPLITUDE_MIN_HAND_DIST = 5.0f * METERS_PER_INCH;
static const FLOAT VIRTUAL_AMPLITUDE_MAX_HAND_DIST = 10.0f * METERS_PER_INCH;
static const FLOAT VIRTUAL_PITCH_MAX_HAND_DIST = 18.0f * METERS_PER_INCH;

// Waist-tracking parameters
static const FLOAT WAIST_HYSTERESIS_METERS = 0.1;
static const FLOAT WAIST_SETTLE_TIME_SECONDS = 1.0f;
static const FLOAT JOINT_AVERAGING_WINDOW_SIZE = 30.0f;

// Neutral stance parameters

// Angle from arms to gravity must be less than this to be "neutral".
static const FLOAT NEUTRAL_STANCE_COS_RANGE = XMScalarCos( XMConvertToRadians( 30.0f ) );

// Angle from forward to camera must be greater than this to be facing away
// (and not playing).
static const FLOAT NEUTRAL_FACING_CAMERA_RANGE = XMScalarCos( XMConvertToRadians( 70.0f ));

// Audio output interpolation settings

// Volume can lerp between 0.0 and 1.0; a rate of 10.0 units per second means that 
// it can go from 0.0 to 1.0 in 0.1s.
static const FLOAT MAX_VOLUME_CHANGE_PER_SECOND = 10.0f;

// Pitch change is rate in hz per second.
static const FLOAT MAX_PITCH_CHANGE_PER_SECOND = 10000.0f;

// Dummy value used to mark unused parameters.
static const FLOAT UNUSED_FLOAT_VALUE = 0.0f;


//--------------------------------------------------------------------------------------
// Name: Clamp
// Desc: Clamps the value to the min/max range.
//--------------------------------------------------------------------------------------
FORCEINLINE FLOAT Clamp( FLOAT value, FLOAT minRange, FLOAT maxRange )
{
    return XMMax( XMMin( value, maxRange ), minRange );
}

//--------------------------------------------------------------------------------------
// Name: Norm
// Desc: Normalizes the value between its min and max range, so that if the value is at
//       or below the minRange, it will be 0.0; and if it is at or above the maxRange, 
//       it will be 1.0. Values in between are mapped linearly.
//--------------------------------------------------------------------------------------
FORCEINLINE FLOAT Norm( FLOAT value, FLOAT minRange, FLOAT maxRange )
{
    return ( Clamp( value, minRange, maxRange ) - minRange ) / ( maxRange - minRange );
}


//--------------------------------------------------------------------------------------
// Name: Map
// Desc: Maps a value between 0.0 and 1.0 to an output range between outRangeMin and
//       outRangeMax. Values outside the range 0.0 and 1.0 are not clamped, but are
//       mapped linearly with the range.
//--------------------------------------------------------------------------------------
FORCEINLINE FLOAT Map( FLOAT value, FLOAT outRangeMin, FLOAT outRangeMax )
{
    return ( value * ( outRangeMax - outRangeMin ) ) + outRangeMin;
}


//--------------------------------------------------------------------------------------
// Name: Remap
// Desc: Linearly maps a value from one range (inRangeMin<->inRangeMax) to another range
//       (ourRangeMin<->outRangeMax). Values outside the range are clamped to the
//       limits of the range.
//--------------------------------------------------------------------------------------
FORCEINLINE FLOAT Remap( FLOAT value, FLOAT inRangeMin, FLOAT inRangeMax,
                        FLOAT outRangeMin, FLOAT outRangeMax )
{
    return Map( Norm( value, inRangeMin, inRangeMax ), outRangeMin, outRangeMax);
}

//--------------------------------------------------------------------------------------
// Name: ExponentialAverage
// Desc: Performs a windowed-average against a value. Each frame of the window is fixed
//       width in time.
//--------------------------------------------------------------------------------------
template<typename T> T ExponentialAverage( T oldValue, T newValue )
{
    return ( oldValue * ( JOINT_AVERAGING_WINDOW_SIZE - 1.0f ) + newValue )
            / JOINT_AVERAGING_WINDOW_SIZE;
}


//--------------------------------------------------------------------------------------
// Constructor
//--------------------------------------------------------------------------------------
CControls::CControls()
:   m_dwLastTrackedPlayerSkeleton( NO_TRACKED_SKELETON ),
    m_fLeftElbowAngle( 0.0f ),
    m_fRightElbowAngle( 0.0f ),
    m_fVolume( MIN_VOL ),
    m_fTargetVolume( MIN_VOL ),
    m_fPitch( MIN_FREQUENCY ),
    m_fTargetPitch( MIN_FREQUENCY ),
    m_fWaistSettleTime( 0.0f ),
    m_eCurrentControlType( CONTROLSCHEME_VIRTUAL ),
    m_bOctaveMode( FALSE ),
    m_bPlayerNeutral( FALSE ),
    m_bPlayerConfidenceGood( FALSE )
{
    Reset();
}


//--------------------------------------------------------------------------------------
// Name: Reset
// Desc: Resets any control-specific parameters. This function is called when you
//       change control schemes.
//--------------------------------------------------------------------------------------
void CControls::Reset()
{
    // Stop interpolating pitch
    m_fTargetPitch = MIN_FREQUENCY;
    m_fPitch =       MIN_FREQUENCY;

    // Fade to 0 volume (just in case).
    m_fTargetVolume = MIN_VOL;
    m_fVolume =       MIN_VOL;

    // Reset waist-height tracking for the "virtual theremin" control scheme.
    XMVECTOR vZero = XMVectorZero();
    m_vWaistPosAvg = vZero;
    m_vWaistPosSet = vZero;
    m_fWaistSettleTime = WAIST_SETTLE_TIME_SECONDS;

    // Reset bone-length values for the virtual control scheme.
    m_fLUpperArmLength = -1.0f;
    m_fRUpperArmLength = -1.0f;
    m_fLLowerArmLength = -1.0f;
    m_fRLowerArmLength = -1.0f;

    // Initial antenna distances...
    m_fVolumeDistance = VIRTUAL_ANTENNA_DISTANCE;
    m_fPitchDistance = VIRTUAL_ANTENNA_DISTANCE;

    m_fVolumeDistanceRange = VIRTUAL_AMPLITUDE_MAX_HAND_DIST;
    m_fPitchDistanceRange = VIRTUAL_PITCH_MAX_HAND_DIST;

}


//--------------------------------------------------------------------------------------
// Name: UpdateTrackedSkeleton
// Desc: Keeps track of which skeleton is being tracked for use by the player (we only
//       allow one person to play the theremin at a time in this sample).
//--------------------------------------------------------------------------------------
void CControls::UpdateTrackedSkeleton( const NUI_SKELETON_FRAME* pSkeletonFrame )
{
    if ( m_dwLastTrackedPlayerSkeleton != NO_TRACKED_SKELETON )
    {
        // Is confidence good enough on the last skeleton we were tracking? If not,
        // we let the other player (if present) steal the skeleton.

        if ( CheckPlayerConfidence( &pSkeletonFrame->SkeletonData[m_dwLastTrackedPlayerSkeleton] ) )
            return;
    }

    // We could skip the skeleton pointed to by m_dwLastTrackedPlayerSkeleton if there
    // is one, but for simplicity, we just loop over them:

    for ( DWORD dwSkeleton = 0; dwSkeleton < NUI_SKELETON_COUNT; ++dwSkeleton )
    {
        if ( CheckPlayerConfidence( &pSkeletonFrame->SkeletonData[dwSkeleton] ) )
        {
            m_dwLastTrackedPlayerSkeleton = dwSkeleton;
            return;
        }
    }

    m_dwLastTrackedPlayerSkeleton = NO_TRACKED_SKELETON;
}


//--------------------------------------------------------------------------------------
// Name: CheckPlayerConfidence
// Desc: Checks the player confidence before we proceed. We assume that if there is no
//       confidence in the data, we cannot continue tracking the player in this sample.
//--------------------------------------------------------------------------------------
BOOL CControls::CheckPlayerConfidence( const NUI_SKELETON_DATA* pSkeleton )
{
    m_bPlayerConfidenceGood = pSkeleton->eTrackingState == NUI_SKELETON_TRACKED &&
        pSkeleton->eSkeletonPositionTrackingState[NUI_SKELETON_POSITION_HAND_LEFT] != NUI_SKELETON_POSITION_NOT_TRACKED &&
        pSkeleton->eSkeletonPositionTrackingState[NUI_SKELETON_POSITION_HAND_RIGHT] != NUI_SKELETON_POSITION_NOT_TRACKED &&
        pSkeleton->eSkeletonPositionTrackingState[NUI_SKELETON_POSITION_ELBOW_LEFT] != NUI_SKELETON_POSITION_NOT_TRACKED &&
        pSkeleton->eSkeletonPositionTrackingState[NUI_SKELETON_POSITION_ELBOW_RIGHT] != NUI_SKELETON_POSITION_NOT_TRACKED &&
        pSkeleton->eSkeletonPositionTrackingState[NUI_SKELETON_POSITION_SHOULDER_RIGHT] != NUI_SKELETON_POSITION_NOT_TRACKED &&
        pSkeleton->eSkeletonPositionTrackingState[NUI_SKELETON_POSITION_HIP_CENTER] != NUI_SKELETON_POSITION_NOT_TRACKED;
    return m_bPlayerConfidenceGood;
}


//--------------------------------------------------------------------------------------
// Name: CheckPlayerNeutral
// Desc: Checks to see if the player is standing in a neutral position, and as such
//       should be treated as if he's "not playing" any more. This hopefully will
//       allow us to prevent squeal.
// 
//       The player is neutral if her hands are by her sides, or the player is facing
//       away from the camera.
//--------------------------------------------------------------------------------------
BOOL CControls::CheckPlayerNeutral( const NUI_SKELETON_DATA* pSkeleton )
{
    XMVECTOR vDown = XMVectorSet( 0.0f, -1.0f, 0.0f, 1.0f );
    XMVECTOR vRHandPos = pSkeleton->SkeletonPositions[NUI_SKELETON_POSITION_HAND_RIGHT];
    XMVECTOR vLHandPos = pSkeleton->SkeletonPositions[NUI_SKELETON_POSITION_HAND_LEFT];
    XMVECTOR vRShoulderPos = pSkeleton->SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_RIGHT];
    XMVECTOR vLShoulderPos = pSkeleton->SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_LEFT];
    XMVECTOR vRElbowPos = pSkeleton->SkeletonPositions[NUI_SKELETON_POSITION_ELBOW_RIGHT];
    XMVECTOR vLElbowPos = pSkeleton->SkeletonPositions[NUI_SKELETON_POSITION_ELBOW_LEFT];

    // Are we facing away from the camera? 

    XMVECTOR vUp = XMVectorSet( 0.0f, 1.0f, 0.0f, 1.0f );
    XMVECTOR vRight = vRShoulderPos - vLShoulderPos;
    XMVECTOR vForward = XMVector3Normalize( XMVector3Cross( vUp, vRight ) );
    XMVECTOR vCamAxis = XMVectorSet( 0.0f, 0.0f, -1.0f, 1.0f );
    FLOAT fFdotCam = XMVectorGetX( XMVector3Dot( vCamAxis, vForward ) );


    // Find shoulder/hand angles; if we're not facing away from the camera, this is our "neutral" pose.

    XMVECTOR vLShoulderToHand = XMVector3NormalizeEst( vLHandPos - vLShoulderPos );
    XMVECTOR vRShoulderToHand = XMVector3NormalizeEst( vRHandPos - vRShoulderPos );
    XMVECTOR vLShoulderToElbow = XMVector3NormalizeEst( vLElbowPos - vLShoulderPos );
    XMVECTOR vRShoulderToElbow = XMVector3NormalizeEst( vRElbowPos - vRShoulderPos );

    FLOAT fLStH_dot_Down = XMVectorGetX( XMVector3Dot( vDown, vLShoulderToHand ) );
    FLOAT fRStH_dot_Down = XMVectorGetX( XMVector3Dot( vDown, vRShoulderToHand ) );
    FLOAT fLStE_dot_Down = XMVectorGetX( XMVector3Dot( vDown, vLShoulderToElbow ) );
    FLOAT fRStE_dot_Down = XMVectorGetX( XMVector3Dot( vDown, vRShoulderToElbow ) );

    m_bPlayerNeutral = ( fFdotCam < NEUTRAL_FACING_CAMERA_RANGE ) ||
                       ( fLStH_dot_Down > NEUTRAL_STANCE_COS_RANGE &&
                         fRStH_dot_Down > NEUTRAL_STANCE_COS_RANGE &&
                         fLStE_dot_Down > NEUTRAL_STANCE_COS_RANGE &&
                         fRStE_dot_Down > NEUTRAL_STANCE_COS_RANGE );
    return m_bPlayerNeutral;
}


//--------------------------------------------------------------------------------------
// Name: UpdateWaistTracking
// Desc: Tracks the position of the player's waist to allow for body-centric
//       gesture tracking, and returns the Y position of the waist in world space.
//--------------------------------------------------------------------------------------
FLOAT CControls::UpdateWaistTracking( const FLOAT fDeltaTime,
                                              FXMVECTOR vHipCenterJoint )
{
    // Tracks the waist of the player in space.

    // The waist of the player is averaged using a decaying 30 measurement-wide window.

    // If the player has moved more than the dead sphere distance from the last
    // set-point, the average waist position is used after the settling time has
    // expired.

    if (fDeltaTime != 0.0f)
    {
        m_vWaistPosAvg = ExponentialAverage( m_vWaistPosAvg, vHipCenterJoint );

        // If the distance from the last set waist position is > dead sphere radius, we
        // kick the settle time and the average. If the settle time has expired, we use
        // that as the new position.

        if ( XMVectorGetX( XMVector2LengthSq( vHipCenterJoint - m_vWaistPosSet ) )
            >= WAIST_HYSTERESIS_METERS * WAIST_HYSTERESIS_METERS )
        {
            // If we're outside the set-point bubble, we keep averaging for a certain
            // amount of time, then snap the waist position to the new location.

            m_fWaistSettleTime = WAIST_SETTLE_TIME_SECONDS;
            m_vWaistPosSet = m_vWaistPosAvg;
        }
        else if ( IsWaistPosSettling() )
        {
            m_vWaistPosSet = m_vWaistPosAvg;
            m_fWaistSettleTime -= fDeltaTime;
        }
        
        
    }

    return XMVectorGetY( m_vWaistPosSet );
}

//--------------------------------------------------------------------------------------
// Name: UpdateArmBoneLengths
// Desc: Updates the lengths of each of the arm bones over time.
//--------------------------------------------------------------------------------------
VOID CControls::UpdateArmBoneLengths( const NUI_SKELETON_DATA* pSkeleton )
{
    XMVECTOR vRHandPos = pSkeleton->SkeletonPositions[NUI_SKELETON_POSITION_HAND_RIGHT];
    XMVECTOR vLHandPos = pSkeleton->SkeletonPositions[NUI_SKELETON_POSITION_HAND_LEFT];
    XMVECTOR vRElbowPos = pSkeleton->SkeletonPositions[NUI_SKELETON_POSITION_ELBOW_RIGHT];
    XMVECTOR vLElbowPos = pSkeleton->SkeletonPositions[NUI_SKELETON_POSITION_ELBOW_LEFT];
    XMVECTOR vRShoulderPos = pSkeleton->SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_RIGHT];
    XMVECTOR vLShoulderPos = pSkeleton->SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_LEFT];

    // We calculate the distances regardless of accuracy (avoids the branch penalty).
    FLOAT fRUpperArmLength =
                          XMVectorGetX( XMVector3Length( vRShoulderPos - vRElbowPos ) );

    FLOAT fRLowerArmLength =
                          XMVectorGetX( XMVector3Length( vRElbowPos - vRHandPos ) );

    FLOAT fLUpperArmLength = XMVectorGetX(
                          XMVector3Length( vLShoulderPos - vLElbowPos ) );

    FLOAT fLLowerArmLength = XMVectorGetX(
                          XMVector3Length( vLElbowPos - vLHandPos ) );

    // Exponentially average the values.

    FLOAT fAvgRUpperArmLength = ExponentialAverage( m_fRUpperArmLength, fRUpperArmLength );
    FLOAT fAvgLUpperArmLength = ExponentialAverage( m_fLUpperArmLength, fLUpperArmLength );
    FLOAT fAvgRLowerArmLength = ExponentialAverage( m_fRLowerArmLength, fRLowerArmLength );
    FLOAT fAvgLLowerArmLength = ExponentialAverage( m_fLLowerArmLength, fLLowerArmLength );

    // Now the trick; when we reset, the arm length is negative. Use __fself to reset
    // the averager in this case to be the current measured length - but we don't store
    // it back in yet! That needs the confidence values.
    fRUpperArmLength = __fself( m_fRUpperArmLength, fAvgRUpperArmLength, fRUpperArmLength );
    fLUpperArmLength = __fself( m_fLUpperArmLength, fAvgLUpperArmLength, fLUpperArmLength );
    fRLowerArmLength = __fself( m_fRLowerArmLength, fAvgRLowerArmLength, fRLowerArmLength );
    fLLowerArmLength = __fself( m_fLLowerArmLength, fAvgLLowerArmLength, fLLowerArmLength );

    // Pull the confidence values, and if the joints are all high confidence, we use them
    // to update the bone lengths.

    NUI_SKELETON_POSITION_TRACKING_STATE eRHandConf = pSkeleton->eSkeletonPositionTrackingState[NUI_SKELETON_POSITION_HAND_RIGHT];
    NUI_SKELETON_POSITION_TRACKING_STATE eLHandConf = pSkeleton->eSkeletonPositionTrackingState[NUI_SKELETON_POSITION_HAND_LEFT];
    NUI_SKELETON_POSITION_TRACKING_STATE eRElbowConf = pSkeleton->eSkeletonPositionTrackingState[NUI_SKELETON_POSITION_ELBOW_RIGHT];
    NUI_SKELETON_POSITION_TRACKING_STATE eLElbowConf = pSkeleton->eSkeletonPositionTrackingState[NUI_SKELETON_POSITION_ELBOW_LEFT];
    NUI_SKELETON_POSITION_TRACKING_STATE eRShoulderConf = pSkeleton->eSkeletonPositionTrackingState[NUI_SKELETON_POSITION_SHOULDER_RIGHT];
    NUI_SKELETON_POSITION_TRACKING_STATE eLShoulderConf = pSkeleton->eSkeletonPositionTrackingState[NUI_SKELETON_POSITION_SHOULDER_LEFT];

    if ( eRShoulderConf == NUI_SKELETON_POSITION_TRACKED &&
         eRElbowConf == NUI_SKELETON_POSITION_TRACKED )
    {
        m_fRUpperArmLength = fRUpperArmLength;
    }

    if ( eLShoulderConf == NUI_SKELETON_POSITION_TRACKED &&
         eLElbowConf == NUI_SKELETON_POSITION_TRACKED )
    {
        m_fLUpperArmLength = fLUpperArmLength;
    }

    if ( eRHandConf == NUI_SKELETON_POSITION_TRACKED &&
         eRElbowConf == NUI_SKELETON_POSITION_TRACKED )
    {
        m_fRLowerArmLength = fRLowerArmLength;
    }

    if ( eLHandConf == NUI_SKELETON_POSITION_TRACKED &&
         eLElbowConf == NUI_SKELETON_POSITION_TRACKED )
    {
        m_fLLowerArmLength = fLLowerArmLength;
    }

    // Update the virtual theremin positions now.

    // These are the distances in front of the player of the pieces.
    m_fVolumeDistance = m_fLLowerArmLength;
    m_fPitchDistance = m_fRLowerArmLength;

    // This is the range of motion for the controller
    m_fVolumeDistanceRange = m_fLLowerArmLength + m_fLUpperArmLength;

    m_fPitchDistanceRange = m_fRLowerArmLength + m_fRUpperArmLength;
}


//--------------------------------------------------------------------------------------
// Name: Update
// Desc: This is called each time new skeleton data is retrieved to allow the current
//       control scheme to process the data.
//--------------------------------------------------------------------------------------
HRESULT CControls::Update( FLOAT fDeltaTime,
                                   const NUI_SKELETON_FRAME* pSkeletonFrame )
{
    UpdateTrackedSkeleton( pSkeletonFrame );

    // Don't emit any sound if we're not tracking you, or we can't get a good read on
    // your limbs.
    m_fTargetVolume = 0.0f;

    if ( IsTracking() )
    {
        const NUI_SKELETON_DATA* pSkeleton = &pSkeletonFrame->SkeletonData[ m_dwLastTrackedPlayerSkeleton ];

        // If we're in a neutral stance, don't control the thermin; just nuke the volume.
        if ( !CheckPlayerNeutral( pSkeleton ) )
        {
            // m_fTargetVolume will be appropriately set in here based on our tracking
            // before we interpolate towards it.

            switch (m_eCurrentControlType)
            {
                case CONTROLSCHEME_ELBOWS:
                    ControlViaElbowAngles( fDeltaTime, pSkeleton );
                break;
                case CONTROLSCHEME_VIRTUAL:
                default:
                    ControlViaVirtualPosition( fDeltaTime, pSkeleton );
                break;
            }
        }

    }

    InterpolatePitchAndVolume( fDeltaTime );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: ControlViaElbowAngles
// Desc: A simple control scheme which uses the angles made by the arms at the elbows to
//       control pitch and volume.
//--------------------------------------------------------------------------------------
void CControls::ControlViaElbowAngles(const FLOAT fDeltaTime,
                                              const NUI_SKELETON_DATA* pSkeleton )
{
    // The elbow is a hinge joint, which goes from fully open (~165-180 degrees) to
    // roughly 45 degrees when closed.
    // 
    // We simply take this angle for each arm, and map the left elbow's angle to
    // volume, and the right elbow's angle to frequency.
    // 
    // Using joint angles (provided that you give enough dead-zone to handle different
    // player abilities) is probably the simplest body-relative control scheme
    // possible.

    XMVECTOR vRHandPos = pSkeleton->SkeletonPositions[NUI_SKELETON_POSITION_HAND_RIGHT];
    XMVECTOR vLHandPos = pSkeleton->SkeletonPositions[NUI_SKELETON_POSITION_HAND_LEFT];
    XMVECTOR vRElbowPos = pSkeleton->SkeletonPositions[NUI_SKELETON_POSITION_ELBOW_RIGHT];
    XMVECTOR vLElbowPos = pSkeleton->SkeletonPositions[NUI_SKELETON_POSITION_ELBOW_LEFT];
    XMVECTOR vRShoulderPos = pSkeleton->SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_RIGHT];
    XMVECTOR vLShoulderPos = pSkeleton->SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_LEFT];

    // Calculate the amplitude based on the left elbow angle:

    XMVECTOR vLHandNorm = XMVector3Normalize(vLHandPos - vLElbowPos);
    XMVECTOR vLShoulderNorm = XMVector3Normalize(vLShoulderPos - vLElbowPos);

    FLOAT fLUpperDotLowerArm = XMVectorGetX( XMVector3Dot( vLHandNorm,
                                                           vLShoulderNorm ) );
    
    FLOAT fLeftElbowAngle = XMScalarACos( fLUpperDotLowerArm );
    m_fLeftElbowAngle = fLeftElbowAngle;
    
    FLOAT fAmplitude = Remap( fLeftElbowAngle, MIN_VOL_ANGLE, MAX_VOL_ANGLE,
                                               MIN_VOL, MAX_VOL );

    // Calculate the pitch based on the right elbow angle:
    XMVECTOR vRHandNorm = XMVector3Normalize( vRHandPos - vRElbowPos );
    XMVECTOR vRShoulderNorm = XMVector3Normalize( vRShoulderPos - vRElbowPos );

    FLOAT fRUpperDotLowerArm = XMVectorGetX( XMVector3Dot( vRHandNorm,
                                                           vRShoulderNorm ) );

    FLOAT fRightElbowAngle = XMScalarACos( fRUpperDotLowerArm );
    m_fRightElbowAngle = fRightElbowAngle;
    
    FLOAT fPitch = 1.0f - Norm( fRightElbowAngle, MIN_FREQ_ANGLE, MAX_FREQ_ANGLE );
    
    m_fTargetPitch = RangeAndLinearizeFreq( fPitch );
    m_fTargetVolume = fAmplitude;
}


//--------------------------------------------------------------------------------------
// Name: ControlViaVirtualPosition
// Desc: 
//--------------------------------------------------------------------------------------
void CControls::ControlViaVirtualPosition( const FLOAT fDeltaTime,
                                                   const NUI_SKELETON_DATA* pSkeleton )
{
    // This method simulates a virtual theremin. The theremin is positioned at roughly
    // waist height, with the assumption that it's placed in front of you, square with
    // your shoulders.
    // 
    // We position the antennae about where you can touch if you hold your arm
    // out fully extended, with your hand at waist height.
    //
    // There is a slack of a few inches around this position which acts as a dead zone.
    // The limit of the range is the length of your arm from the antenna.

    XMVECTOR vRHandPos = pSkeleton->SkeletonPositions[NUI_SKELETON_POSITION_HAND_RIGHT];
    XMVECTOR vLHandPos = pSkeleton->SkeletonPositions[NUI_SKELETON_POSITION_HAND_LEFT];
    XMVECTOR vRShoulderPos = pSkeleton->SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_RIGHT];
    XMVECTOR vLShoulderPos = pSkeleton->SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_LEFT];
    XMVECTOR vHipCenterPos = pSkeleton->SkeletonPositions[NUI_SKELETON_POSITION_HIP_CENTER];

    FLOAT fWaistHeight = UpdateWaistTracking( fDeltaTime, vHipCenterPos );

    // We can't get these from the API, so we need to calculate them and (for now)
    // average them:
    UpdateArmBoneLengths( pSkeleton );

    // Figure out our Z Axis from the up vector, and the vector from right shoulder to
    // left shoulder.

    XMVECTOR vUp = XMVectorSet( 0.0f, 1.0f, 0.0f, 1.0f );
    XMVECTOR vRight = XMVectorSubtract( vRShoulderPos, vLShoulderPos );
    XMVECTOR vForward = XMVector3Normalize( XMVector3Cross( vUp, vRight ) );  

    // Figure out the position of the pitch and amplitude antennae.

    // We directly set the Y component of the antenna positions to the Y value for the
    // waist height. We can do this because our skeletons have been tilt-corrected, so
    // up is in the direction (0, 1, 0).

    XMVECTOR vAmplitudeAntennaPos;
    XMVECTOR vFreqAntennaPos;

    // Calculate the antenna positions along the vector forward out from the shoulders.
    XMVECTOR vPitchDistance = vForward * m_fPitchDistance;
    XMVECTOR vAmplitudeDistance = vForward * m_fVolumeDistance;

    vAmplitudeAntennaPos = vLShoulderPos + vAmplitudeDistance;
    vAmplitudeAntennaPos = XMVectorSetY( vAmplitudeAntennaPos, fWaistHeight );

    vFreqAntennaPos = vRShoulderPos + vPitchDistance;
    vFreqAntennaPos = XMVectorSetY( vFreqAntennaPos, fWaistHeight );

    // Now we take the distance from the left hand to the amplitude antenna, and the
    // right hand to the freq. antenna, and map these to volume and frequency ranges
    // between 0.0f and 1.0f.
    
    // If the hand is further away than the expected virtual antenna distance,
    // we clamp it to the max distance.

    // Amplitude and frequency go up as you get closer to the virtual antenna points.

    FLOAT fDistanceLeftAntenna = XMVectorGetX(
                                   XMVector3Length( vLHandPos - vAmplitudeAntennaPos )
                                 );
    FLOAT fDistanceRightAntenna = XMVectorGetX(
                                   XMVector3Length( vRHandPos - vFreqAntennaPos )
                                  );

    FLOAT fAmplitude = Remap( fDistanceLeftAntenna,
                        VIRTUAL_AMPLITUDE_MIN_HAND_DIST, m_fVolumeDistanceRange,
                        MIN_VOL, MAX_VOL );
    FLOAT fPitch = 1.0f - Norm( fDistanceRightAntenna,
                        VIRTUAL_PITCH_MIN_HAND_DIST, m_fPitchDistanceRange);

    // Linearize the frequencies if desired (makes octaves linear in space, like a
    // piano).

    fPitch = RangeAndLinearizeFreq( fPitch );

    m_fTargetVolume = fAmplitude;
    m_fTargetPitch = fPitch;

    // Store them out so we can show them in debug.
    m_vAmplitudeAntennaPos = vAmplitudeAntennaPos;
    m_vPitchAntennaPos = vFreqAntennaPos;
    m_vLeftHandPos = vLHandPos;
    m_vRightHandPos = vRHandPos;

}


//--------------------------------------------------------------------------------------
// Name: RangeAndLinearizeFreq
// Desc: Takes an input frequency in the range 0.0 to 1.0, and maps it to the output
//       frequency range. If the appropriate flag is set, (see SetOctaveMode), then
//       the pitch value will be mapped so that it is perceptually linear
//       ("octave mode") as opposed to physically linear ("linear mode").
//--------------------------------------------------------------------------------------
FLOAT CControls::RangeAndLinearizeFreq( const FLOAT fLinearFreq )
{
    if ( m_bOctaveMode)
    {
        // The math here is:
        // Input value is between 0.0 -> 1.0
        // Output value is 2 ^ ( v * (log2(maxFreq) - log2(minFreq)) + log2(minFreq) )
        // i.e. we map the input to a range in log space, then antilog it to get the
        //      value in the frequency domain.
        return (FLOAT)pow( 2.0f, Map( fLinearFreq, LOG2MIN_FREQ, LOG2MAX_FREQ ) );
    }
    else
    {
        // Linear mode? Simply map it directly to the physical frequency range.
        return Map( fLinearFreq, MIN_FREQUENCY, MAX_FREQUENCY );
    }
}


//--------------------------------------------------------------------------------------
// Name: InterpolatePitchAndVolume
// Desc: Interpolates the pitch and volume values from their current values to a target
//       value specified by the control scheme. The rate of interpolation is limited to
//       a maximum slew rate specified in delta per seconds by two constants.
//--------------------------------------------------------------------------------------
void CControls::InterpolatePitchAndVolume(FLOAT rDeltaTime)
{
    static const XMVECTOR vMaxDeltas = {
        MAX_VOLUME_CHANGE_PER_SECOND,    // Volume in X component
        MAX_PITCH_CHANGE_PER_SECOND,     // Pitch in Y component
        0.0f, 0.0f // Unused Z, W
    };

    // Scale to get max changes this frame.
    XMVECTOR vScaledMaxDeltas = vMaxDeltas * rDeltaTime; 

    XMVECTOR vCurrentValues = XMVectorSet( m_fVolume, m_fPitch, 0.0f, 0.0f );
    XMVECTOR vTargetDeltas = XMVectorSet( m_fTargetVolume - m_fVolume,
                                         m_fTargetPitch - m_fPitch,
                                         0.0f, 0.0f );

    XMVECTOR vMaxDeltaThisFrame = XMVectorMax( XMVectorMin( vTargetDeltas,
                                  vScaledMaxDeltas ), XMVectorNegate( vScaledMaxDeltas ));

    XMVECTOR vResult = vCurrentValues + vMaxDeltaThisFrame;

    m_fVolume = XMVectorGetX(vResult);
    m_fPitch = XMVectorGetY(vResult);
}


//--------------------------------------------------------------------------------------
// Name: GetControlSchemeName
//--------------------------------------------------------------------------------------
const wchar_t* CControls::GetControlSchemeName()
{
    static const wchar_t* schemeNames[] = {
        L"Elbow Angle Controls",
        L"Virtual Theremin",
        L"Old Control Scheme"
    };

    return schemeNames[ m_eCurrentControlType ];
}


//--------------------------------------------------------------------------------------
// Name: PrevControlScheme
//--------------------------------------------------------------------------------------
void CControls::PrevControlScheme()
{
    int prevCtl = m_eCurrentControlType - 1;
    if ( prevCtl == -1 )
    {
        prevCtl = CONTROLSCHEME_COUNT - 1;
    }

    m_eCurrentControlType = (CONTROLSCHEME) prevCtl;
    Reset();
}


//--------------------------------------------------------------------------------------
// Name: NextControlScheme
//--------------------------------------------------------------------------------------
void CControls::NextControlScheme()
{
    int nextCtl = m_eCurrentControlType + 1;
    if ( nextCtl == CONTROLSCHEME_COUNT )
    {
        nextCtl = 0;
    }

    m_eCurrentControlType = (CONTROLSCHEME) nextCtl;
    Reset();
}


//--------------------------------------------------------------------------------------
// Name: RenderDebug
// Desc: Renders debug information to the display.
//--------------------------------------------------------------------------------------
void CControls::RenderDebug( ATG::Font& font ) const
{
    WCHAR buf[256];
    swprintf_s( buf, 256, L"Target Pitch %.0f Hz", m_fTargetPitch );
    font.DrawText( 0, 60, 0xffffffff, buf );
    
    swprintf_s( buf, 256, L"Target Volume %.0f", m_fTargetVolume * 100.0f );
    font.DrawText( 500, 60, 0xffffffff, buf );

    if ( IsPlayerInNeutral() )
    {
        font.DrawText( 0, 150, 0xFF00FF00, L"Player is in Neutral Stance" );
    }
    
    if ( !IsPlayerConfidenceGood() )
    {
        font.DrawText( 0, 210, 0xFFFF0000, L"Player joint confidence not high enough" );
    }

    if ( !IsPlayerInNeutral() && IsPlayerConfidenceGood() )
    {
        switch (m_eCurrentControlType)
        {
            case CONTROLSCHEME_ELBOWS:
            {
                swprintf_s( buf, 256, L"Left Elbow Angle: %.1f",
                            XMConvertToDegrees(m_fLeftElbowAngle) );
                font.DrawText( 0, 120, 0xffffffff, buf );
                swprintf_s( buf, 256, L"Right Elbow Angle: %.1f",
                            XMConvertToDegrees(m_fRightElbowAngle) );
                font.DrawText( 0, 150, 0xffffffff, buf );
            }
            break;

            case CONTROLSCHEME_VIRTUAL:
            default:
            {
                if (IsWaistPosSettling())
                {
                    swprintf_s( buf, 256, L"Waist Settle Time: %.1f",
                                m_fWaistSettleTime );
                    font.DrawText( 0, 120, 0xffffffff, buf );
                }
                else
                {
                    font.DrawText( 0, 120, 0xffffffff, L"Waist has settled" );
                }
                
                swprintf_s( buf, 256, L"Waist Pos: (%.3f, %.3f, %.3f)",
                            XMVectorGetX(m_vWaistPosSet), XMVectorGetY(m_vWaistPosSet),
                            XMVectorGetZ(m_vWaistPosSet) );
                font.DrawText( 0, 150, 0xffffffff, buf );

                FLOAT fDistAmplitude = XMVectorGetX( 
                                         XMVector3Length( m_vLeftHandPos - 
                                                          m_vAmplitudeAntennaPos
                                                         ) );
                FLOAT fDistPitch = XMVectorGetX(
                                    XMVector3Length( m_vRightHandPos - 
                                                     m_vPitchAntennaPos ) );

                swprintf_s( buf, 256, L"Amplitude Antenna Distance: %.2fm",
                            fDistAmplitude );
                font.DrawText( 0, 180, 0xffffffff, buf );

                swprintf_s( buf, 256, L"Pitch Antenna Distance: %.2fm",
                            fDistPitch );
                font.DrawText( 500, 180, 0xffffffff, buf );
            }
            break;
        }   
    }
}
