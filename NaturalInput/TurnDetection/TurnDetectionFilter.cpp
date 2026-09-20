//--------------------------------------------------------------------------------------
// TurnDetectionFilter.cpp
//
// Filter for turn detection
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include <xtl.h>
#include <nuiapi.h>
#include <AtgUtil.h>

#include "TurnDetectionFilter.h"


//--------------------------------------------------------------------------------------
// Data structures
//--------------------------------------------------------------------------------------

// Structure for parameters for the smoothing filter. See the documentation for the
// NuiTransformSmooth() API for more information on these parameters
struct SmoothingFilterParams
{
    XMVECTOR m_vSmoothing;
    XMVECTOR m_vCorrection;
    XMVECTOR m_vPrediction;
    XMVECTOR m_vJitterRadius;
    XMVECTOR m_vMaxDeviationRadius;
};

// Structure for the smoothing filter state
struct SmoothingFilterState
{
    XMVECTOR m_vPrevRawData;
    XMVECTOR m_vPrevFilteredData;
    XMVECTOR m_vPrevTrend;
    XMVECTOR m_vFilteredData;   // .x for DiffMinMax, .y for returned avg body rotation angle
    DWORD    m_dwCount;
};

// Structure for parameters for turn detection filter
struct TurnDetectionParams
{
    FLOAT m_fThresholdStartStop;    // Threshold value to start/stop a search for a 180 flip
    FLOAT m_fThresholdFoundFlip;    // Threshold value for confirming when a 180 flip was found
    UINT  m_uThresholdReset;        // Number of frames before forcing a stop on the search for reset
};

// Define a filter state enum
enum EFilterState
{
    FILTER_STATE_IDLE,
    FILTER_STATE_SEARCH_FLIP,
    FILTER_STATE_FOUND_FLIP
};

// Structure for the turn detection filter state
struct TurnDetectionState
{
    XMVECTOR        m_vPreviousShoulderAngles;      // Shoulder angles of previous frame
    UINT            m_uPreviousSTFrameNumber;       // Frame number for ST of previous processed frame
    UINT            m_uNumFramesAfterFound;         // Num frames after a flip was found
    UINT            m_uTotalFlipsFound;             // Total number 180 flips found
    EFilterState    m_CurrentState;                 // Current state
    SmoothingFilterState m_SmoothingFilterState;    // State for smoothing data
};


//--------------------------------------------------------------------------------------
// Constants and global variables for turn detection and smoothing filters
//--------------------------------------------------------------------------------------

// Define the maximum number of frames the data is allowed to be out of sync with
// previous valid data
const DWORD g_dwMaxFramesOutOfSync = 2;

// Refer to the documentation of the sample for an explanation of of why these specific
// values were choosen for the filter parameters
const TurnDetectionParams g_TurnDetectionParams = { 40.0f,  // threshold to start/stop search for 180 flip
                                                    22.0f,  // threshold to confirm finding a 180 flip
                                                    60 };   // num frames before forcing a stop search after a flip was found

// Setup the smoothing filter parameters to smooth data. Refer to the documentation of
// NuiTransformSmooth API for a better understanding of these values
const SmoothingFilterParams g_SmoothingFilterParams   = { XMVectorSet( 0.5f, 0.20f, 0.0f, 0.00f ),
                                                          XMVectorSet( 0.5f, 0.8f, 0.0f, 0.0f ),
                                                          XMVectorSet( 0.5f, 0.0f, 0.0f, 0.0f ),
                                                          XMVectorSet( 25.0f, 10.0f, 0.0f, 0.0f ),
                                                          XMVectorSet( 5.0f, 10.0f, 0.0f, 0.0f ) };

// The global state of the turn detection filter
TurnDetectionState g_TurnDetectionState[ NUI_SKELETON_COUNT ];


//--------------------------------------------------------------------------------------
// Forward declarations
//--------------------------------------------------------------------------------------

VOID FilterData( XMVECTOR vRawData, const SmoothingFilterParams* __restrict pParams, SmoothingFilterState* __restrict pState );


//--------------------------------------------------------------------------------------
// Name: ResetTurnDetectionFilter()
// Desc: Reset the filter states
//--------------------------------------------------------------------------------------

VOID ResetTurnDetectionFilter( const UINT uSkeletonIdx )
{
    assert( uSkeletonIdx < NUI_SKELETON_COUNT );
    ZeroMemory( &g_TurnDetectionState[ uSkeletonIdx ], sizeof( TurnDetectionState ) );
}


//--------------------------------------------------------------------------------------
// Name: ResetTurnDetectionFilter()
// Desc: Reset the filter states for all tracked skeletons
//--------------------------------------------------------------------------------------

VOID ResetTurnDetectionFilter()
{
    for ( UINT i = 0; i < NUI_SKELETON_COUNT; i++ )
    {
       ResetTurnDetectionFilter( i );
    }
}


//--------------------------------------------------------------------------------------
// Name: UpdateTurnDetectionFilter()
// Desc: Update the turn detection filter
//--------------------------------------------------------------------------------------

BOOL UpdateTurnDetectionFilter( NUI_SKELETON_DATA* __restrict pSkeletonData,
                                TurnDetectionState* __restrict pTurnDetectionState,
                                const DWORD dwSTFrameNumber )
{
    assert( pSkeletonData );
    assert( pTurnDetectionState );

    // Compare the ST time stamps or frame number with the previous frame processed, so that
    // if a frame was missed, we can scale accordingly.
    FLOAT fTimeScale = 1.0f;
    if ( pTurnDetectionState->m_uPreviousSTFrameNumber )
    {
        DWORD dwFrameDiff = dwSTFrameNumber - pTurnDetectionState->m_uPreviousSTFrameNumber;

        // If no difference, then don't process the filter again with the same data
        if ( dwFrameDiff == 0 )
        {
            return TRUE;
        }

        // If too many frames were missed, then we need to reset the filter state
        if ( dwFrameDiff > g_dwMaxFramesOutOfSync )
        {
            // returning false will reset the filter state
            printf( "\nNo valid data for %d frames, resetting filter state\n", dwFrameDiff );
            return FALSE;
        }

        fTimeScale = 1.0f / (FLOAT)dwFrameDiff;
    }

    XMVECTOR vTimeScale = XMVectorReplicate( fTimeScale );

    // Create 4 body direction vectors, two from shoulders and two from hips
    XMVECTOR vNeckPos           = pSkeletonData->SkeletonPositions[ NUI_SKELETON_POSITION_SHOULDER_CENTER ];
    XMVECTOR vLeftShoulderPos   = pSkeletonData->SkeletonPositions[ NUI_SKELETON_POSITION_SHOULDER_LEFT ];
    XMVECTOR vRightShoulderPos  = pSkeletonData->SkeletonPositions[ NUI_SKELETON_POSITION_SHOULDER_RIGHT ];
    XMVECTOR vHipCenter         = pSkeletonData->SkeletonPositions[ NUI_SKELETON_POSITION_HIP_CENTER ];
    XMVECTOR vLeftHipPos        = pSkeletonData->SkeletonPositions[ NUI_SKELETON_POSITION_HIP_LEFT ];
    XMVECTOR vRightHipPos       = pSkeletonData->SkeletonPositions[ NUI_SKELETON_POSITION_HIP_RIGHT ];

    // Calculate the 4 direction vectors
    XMVECTOR vLeftShoulderNormal    = XMVector3Normalize( XMVector3Cross( vLeftShoulderPos - vNeckPos, vHipCenter - vNeckPos ) );
    XMVECTOR vRightShoulderNormal   = XMVector3Normalize( XMVector3Cross( vHipCenter - vNeckPos, vRightShoulderPos - vNeckPos ) );
    XMVECTOR vLeftHipNormal         = XMVector3Normalize( XMVector3Cross( XMVector3Normalize( vNeckPos - vHipCenter ), XMVector3Normalize( vLeftHipPos - vHipCenter ) ) );
    XMVECTOR vRightHipNormal        = XMVector3Normalize( XMVector3Cross( XMVector3Normalize( vRightHipPos - vHipCenter ), XMVector3Normalize( vNeckPos - vHipCenter ) ) );

    // Get the tracking states for shoulders and hips
    NUI_SKELETON_POSITION_TRACKING_STATE leftShoulderState  = pSkeletonData->eSkeletonPositionTrackingState[ NUI_SKELETON_POSITION_SHOULDER_LEFT ];
    NUI_SKELETON_POSITION_TRACKING_STATE rightShoulderState = pSkeletonData->eSkeletonPositionTrackingState[ NUI_SKELETON_POSITION_SHOULDER_RIGHT ];
    NUI_SKELETON_POSITION_TRACKING_STATE leftHipState       = pSkeletonData->eSkeletonPositionTrackingState[ NUI_SKELETON_POSITION_HIP_LEFT ];
    NUI_SKELETON_POSITION_TRACKING_STATE rightHipState      = pSkeletonData->eSkeletonPositionTrackingState[ NUI_SKELETON_POSITION_HIP_RIGHT ];

    // Calculate weights from the tracking states, so that when we combine the left and right,
    // we weight angles from tracked positions higher than inferred positions
    FLOAT fWeights[ 4 ] = { ( leftShoulderState == NUI_SKELETON_POSITION_TRACKED ) ? 1.0f :
                                ( leftShoulderState == NUI_SKELETON_POSITION_INFERRED ) ? 0.25f : 0.0f,
                            ( rightShoulderState == NUI_SKELETON_POSITION_TRACKED ) ? 1.0f :
                                ( rightShoulderState == NUI_SKELETON_POSITION_INFERRED ) ? 0.25f : 0.0f,
                            ( leftHipState == NUI_SKELETON_POSITION_TRACKED ) ? 1.0f :
                                ( leftHipState == NUI_SKELETON_POSITION_INFERRED ) ? 0.25f : 0.0f,
                            ( rightHipState == NUI_SKELETON_POSITION_TRACKED ) ? 1.0f :
                                ( rightHipState == NUI_SKELETON_POSITION_INFERRED ) ? 0.25f : 0.0f };

    // Get the totals of the weights so that we can normalize the averages
    FLOAT fTotalShoulders   = fWeights[ 0 ] + fWeights[ 1 ];
    FLOAT fTotalHips        = fWeights[ 2 ] + fWeights[ 3 ];

    // Calculate shoulder and hip rotations
    FLOAT fLeftShoulderAngle    = XMConvertToDegrees( atan2( vLeftShoulderNormal.x, vLeftShoulderNormal.z ) );
    FLOAT fRightShoulderAngle   = XMConvertToDegrees( atan2( vRightShoulderNormal.x, vRightShoulderNormal.z ) );
    FLOAT fLeftHipAngle         = XMConvertToDegrees( atan2( vLeftHipNormal.x, vLeftHipNormal.z ) );
    FLOAT fRightHipAngle        = XMConvertToDegrees( atan2( vRightHipNormal.x, vRightHipNormal.z ) );

    // Calculate a weighted average for the shoulders and hips
    FLOAT fAvgShoulderAngle     = ( ( fLeftShoulderAngle * fWeights[ 0 ] ) + ( fRightShoulderAngle * fWeights[ 1 ] ) );
    FLOAT fAvgHipAngle          = ( ( fLeftHipAngle * fWeights[ 2 ] ) + ( fRightHipAngle * fWeights[ 3 ] ) );

    // If both shoulders or both hips have tracking state of _NONE, then we cannot use those and we need to reset the filter state
    if ( fTotalShoulders == 0.0f &&
         fTotalHips == 0.0f )
    {
        printf( "\nNo valid data, resetting filter state\n" );
        return FALSE;
    }
    else
    {
        // Normalize the weighted averages
        if ( fTotalShoulders != 0.0f )
        {
            fAvgShoulderAngle /= fTotalShoulders;
        }

        if ( fTotalHips != 0.0f )
        {
            fAvgHipAngle /= fTotalHips;
        }
    }

    // Put the data in vectors
    XMVECTOR vCurrentShoulderAngles = XMVectorSet( fLeftShoulderAngle, fRightShoulderAngle, fAvgShoulderAngle, 0.0f );
    XMVECTOR vCurrentHipAngles      = XMVectorSet( fLeftHipAngle, fRightHipAngle, fAvgHipAngle, 0.0f );
   
    // Calculate the difference between the previous frame's angles and the current frame's angles
    XMVECTOR vDiffShoulderAngle = XMVectorSubtract( vCurrentShoulderAngles, pTurnDetectionState->m_vPreviousShoulderAngles );

    // Apply the time scale to allow for frames that got dropped for some reason
    vDiffShoulderAngle = XMVectorMultiply( vDiffShoulderAngle, vTimeScale );

    // Calculate min and max shoulder angles
    XMVECTOR vSplatX        = XMVectorSplatX( vCurrentShoulderAngles );
    XMVECTOR vSplatY        = XMVectorSplatY( vCurrentShoulderAngles );
    XMVECTOR vSplatZ        = XMVectorSplatZ( vCurrentShoulderAngles );
    XMVECTOR vMinShoulder   = XMVectorMin( XMVectorMin( vSplatX, vSplatY ), vSplatZ );
    XMVECTOR vMaxShoulder   = XMVectorMax( XMVectorMax( vSplatX, vSplatY ), vSplatZ );

    // Calculate min and max hip angles
    vSplatX             = XMVectorSplatX( vCurrentHipAngles );
    vSplatY             = XMVectorSplatY( vCurrentHipAngles );
    vSplatZ             = XMVectorSplatZ( vCurrentHipAngles );
    XMVECTOR vMinHip    = XMVectorMin( XMVectorMin( vSplatX, vSplatY ), vSplatZ );
    XMVECTOR vMaxHip    = XMVectorMax( XMVectorMax( vSplatX, vSplatY ), vSplatZ );

    // Calculate the min and max of all current angles
    XMVECTOR vMinAngle  = XMVectorMin( vMinShoulder, vMinHip );
    XMVECTOR vMaxAngle  = XMVectorMax( vMaxShoulder, vMaxHip );

    // Calculate the difference between the current min and max angles
    XMVECTOR vDiffMinMax = XMVectorSubtract( vMaxAngle, vMinAngle );

    // Put the average shoulder rotation in .y so that it will get smoothed.
    vDiffMinMax = XMVectorSetYPtr( vDiffMinMax, &fAvgShoulderAngle );

    // Run a smoothing filter on the min/max difference, since we'll use this to determine when to start/stop looking
    // for a possible flip, and it's better to have smoother data
    FilterData( vDiffMinMax, &g_SmoothingFilterParams, &pTurnDetectionState->m_SmoothingFilterState );

    // Retrieve the filtered data for difference between min/max
    FLOAT fFilteredDiffMinMax = pTurnDetectionState->m_SmoothingFilterState.m_vFilteredData.x;

    switch ( pTurnDetectionState->m_CurrentState )
    {
        // Try to find a frame where we should start searching for a flip
        case FILTER_STATE_IDLE:
        {
            // If the min/max difference is larger than our threshold, then we start looking for a possible flip    
            if ( fFilteredDiffMinMax > g_TurnDetectionParams.m_fThresholdStartStop )
            {  
                // We found the start of a possible flip, so reset the appropriate states
                printf( "Start (%f)\n", pTurnDetectionState->m_SmoothingFilterState.m_vFilteredData.x );
                pTurnDetectionState->m_uNumFramesAfterFound = 0;
                pTurnDetectionState->m_CurrentState = FILTER_STATE_SEARCH_FLIP;
            }
        }
        break;

        // If we are in a search state, then try to find a flip
        case FILTER_STATE_SEARCH_FLIP:
        {
            // If the difference in average shoulder is large enough, we found a flip
            FLOAT fDiffAvgShoulder = vDiffShoulderAngle.z;
            if ( fabs( fDiffAvgShoulder ) > g_TurnDetectionParams.m_fThresholdFoundFlip )
            {
                // We found a flip in a start/end search, so update the state
                pTurnDetectionState->m_CurrentState = FILTER_STATE_FOUND_FLIP;
                pTurnDetectionState->m_uTotalFlipsFound++;
                BOOL bFacingBackward = pTurnDetectionState->m_uTotalFlipsFound % 2;
                printf( "    Found Flip (%f)\n", fDiffAvgShoulder );
                printf( "    Facing %s\n", bFacingBackward ? "Backward" : "Forward" );
            }

            // If the min/max difference is smaller than our threshold, then we stop looking for a possible flip
            if ( fFilteredDiffMinMax <= g_TurnDetectionParams.m_fThresholdStartStop )
            {  
                printf("Stop (%f)\n\n", fFilteredDiffMinMax );
                pTurnDetectionState->m_uNumFramesAfterFound = 0;
                pTurnDetectionState->m_CurrentState = FILTER_STATE_IDLE;
            }
        }
        break;

        case FILTER_STATE_FOUND_FLIP:
        {
            // We need to keep count of how many frames since we found a flip, so that we can end the
            // search after our threshold number of frames if we don't end early enough automatically
            pTurnDetectionState->m_uNumFramesAfterFound++;

            // If the min/max difference is smaller than our threshold, then we stop
            if ( fFilteredDiffMinMax <= g_TurnDetectionParams.m_fThresholdStartStop ||
                 pTurnDetectionState->m_uNumFramesAfterFound > g_TurnDetectionParams.m_uThresholdReset )
            {  
                printf("Stop (%f)\n\n", fFilteredDiffMinMax );
                pTurnDetectionState->m_uNumFramesAfterFound = 0;
                pTurnDetectionState->m_CurrentState = FILTER_STATE_IDLE;
            }
        }
        break;
    }

    // Save the current data
    pTurnDetectionState->m_vPreviousShoulderAngles = vCurrentShoulderAngles;
    pTurnDetectionState->m_uPreviousSTFrameNumber = dwSTFrameNumber;

    return TRUE;
}


//--------------------------------------------------------------------------------------
// Name: UpdateTurnDetectionFilter()
// Desc: Update turn detection filter for all trackable skeletons
//--------------------------------------------------------------------------------------

VOID UpdateTurnDetectionFilter( NUI_SKELETON_FRAME* pSkeletonFrame )
{
    PIXBeginNamedEvent( 0, __FUNCTION__ );

    assert( pSkeletonFrame );

    for ( UINT i = 0; i < NUI_SKELETON_COUNT; i++ )
    {
        if ( pSkeletonFrame->SkeletonData[ i ].eTrackingState == NUI_SKELETON_TRACKED )
        {
            if ( !UpdateTurnDetectionFilter( &pSkeletonFrame->SkeletonData[ i ], &g_TurnDetectionState[ i ], pSkeletonFrame->dwFrameNumber ) )
            {
                // If for some reason the filter failed, we need to reset the state
                ResetTurnDetectionFilter( i );
            }
        }
        else
        {
            ResetTurnDetectionFilter( i );
        }
    }

    PIXEndNamedEvent();
}


//--------------------------------------------------------------------------------------
// Name: FilterData()
// Desc: This is an implementation of the double exponential smoothing filter which
//       filters single streams of data in parallel using XMVECTOR. This means we can
//       run 4 filters with different parameters at the same time.
//--------------------------------------------------------------------------------------

VOID FilterData( XMVECTOR vRawData, const SmoothingFilterParams* __restrict pParams, SmoothingFilterState* __restrict pState )
{
    PIXBeginNamedEvent( 0, __FUNCTION__ );

    CXMVECTOR vEpsilon = XMVectorSplatEpsilon();
    CXMVECTOR vOne = XMVectorSplatOne();
    XMVECTOR vFilteredData;
    XMVECTOR vPredictedData;
    XMVECTOR vDiff;
    XMVECTOR vTrend;

    // Check for divide by zero.
    XMVECTOR vJitterRadius = XMVectorMax( pParams->m_vJitterRadius, vEpsilon );

    // Initial start values
    if ( pState->m_dwCount == 0 )
    {
        vFilteredData = vRawData;
        vTrend = XMVectorZero();
        pState->m_dwCount++;
    }
    else if (pState->m_dwCount == 1)
    {
        vFilteredData = XMVectorScale( XMVectorAdd( vRawData, pState->m_vPrevRawData ), 0.5f );
        vDiff = XMVectorSubtract( vFilteredData, pState->m_vPrevFilteredData );
        vTrend = XMVectorLerpV( pState->m_vPrevTrend, vDiff, pParams->m_vCorrection );
        pState->m_dwCount++;
    }
    else
    {              
        // First apply jitter filter
        vDiff = XMVectorAbs( XMVectorSubtract( vRawData, pState->m_vPrevFilteredData ) );
        vFilteredData = XMVectorLerpV( pState->m_vPrevFilteredData, vRawData, vDiff / vJitterRadius );

        XMVECTOR vCompare = XMVectorLessOrEqual( vDiff, vJitterRadius );
        XMVECTOR vLerp = XMVectorAndInt( vCompare, vOne );
        vFilteredData = XMVectorLerpV( vRawData, vFilteredData, vLerp );

        // Now the double exponential smoothing filter
        vFilteredData = XMVectorLerpV( vFilteredData, XMVectorAdd( pState->m_vPrevFilteredData, pState->m_vPrevTrend ), pParams->m_vSmoothing );
        vDiff = XMVectorSubtract( vFilteredData, pState->m_vPrevFilteredData );
        vTrend = XMVectorLerpV( pState->m_vPrevTrend, vDiff, pParams->m_vCorrection );
    }      

    // Predict into the future to reduce latency
    vPredictedData = XMVectorAdd( vFilteredData, XMVectorMultiply( vTrend, pParams->m_vPrediction ) );

    // Check that we are not too far away from raw data
    vDiff = XMVectorMax( XMVectorAbs( XMVectorSubtract( vPredictedData, vRawData ) ), vEpsilon );

    XMVECTOR vCompare = XMVectorGreater( vDiff, pParams->m_vMaxDeviationRadius );
    XMVECTOR vLerp = XMVectorAndInt( vCompare, vOne );
    vPredictedData = XMVectorLerpV( vPredictedData, XMVectorLerpV( vRawData, vPredictedData, pParams->m_vMaxDeviationRadius / vDiff ), vLerp );

    // Save the data from this frame
    pState->m_vPrevRawData = vRawData;
    pState->m_vPrevFilteredData = vFilteredData;
    pState->m_vPrevTrend = vTrend;
  
    // Output the data
    pState->m_vFilteredData = vPredictedData;

    PIXEndNamedEvent();
}

//--------------------------------------------------------------------------------------
// Name: GetTurnAngleInDegrees()
// Desc: Returns the current turn angle in degrees
//--------------------------------------------------------------------------------------

FLOAT GetTurnAngleInDegrees( const UINT uSkeletonIdx )
{
    assert( uSkeletonIdx < NUI_SKELETON_COUNT );

    // If we're facing backwards, then add 180 degrees
    BOOL bFacingBackward = g_TurnDetectionState[ uSkeletonIdx ].m_uTotalFlipsFound % 2;
    FLOAT fFlip = bFacingBackward ? 180.0f : 0.0f;

    // Using .x for DiffMinMax and .y for Avg body rotation angle
    return g_TurnDetectionState[ uSkeletonIdx ].m_SmoothingFilterState.m_vFilteredData.y + fFlip;
}


//--------------------------------------------------------------------------------------
// Name: GetDebugInfo()
// Desc: Returns some debug info from the filter
//--------------------------------------------------------------------------------------

VOID GetDebugInfo( const UINT uSkeletonIdx, BOOL& bSearchForFlip, BOOL& bFoundFlip )
{
    assert( uSkeletonIdx < NUI_SKELETON_COUNT );

    bSearchForFlip = g_TurnDetectionState[ uSkeletonIdx ].m_CurrentState == FILTER_STATE_SEARCH_FLIP ||
                     g_TurnDetectionState[ uSkeletonIdx ].m_CurrentState == FILTER_STATE_FOUND_FLIP;
    bFoundFlip = g_TurnDetectionState[ uSkeletonIdx ].m_CurrentState == FILTER_STATE_FOUND_FLIP;
}


//--------------------------------------------------------------------------------------
// Name: GetNumFlips()
// Desc: Returns the number of flips
//--------------------------------------------------------------------------------------

UINT GetNumFlips( const UINT uSkeletonIdx )
{
    assert( uSkeletonIdx < NUI_SKELETON_COUNT );
    return g_TurnDetectionState[ uSkeletonIdx ].m_uTotalFlipsFound;
}
