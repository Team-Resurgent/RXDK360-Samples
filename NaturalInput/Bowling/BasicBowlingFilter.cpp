//--------------------------------------------------------------------------------------
// BasicBowlingFilter.cpp
//
// Implements a filter that tries to infer left or right arm motion when the raw skeleton
// stops reporting accurate position. The filter assumes that the player is attempting 
// to throw a bowling ball.
// 
// Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include <xtl.h>
#include "BasicBowlingFilter.h"

#include <nuiapi.h>
#include <AtgFont.h>
#include <assert.h>


const FLOAT CONFIDENCE_THRESHOLD_HIGH = 0.60f;
const FLOAT CONFIDENCE_THRESHOLD_LOW  = 0.50f;
const FLOAT CONFIDENCE_THRESHOLD_NONE = 0.0f;


#define EPSILON 0.000001
#define FLOAT_EQ(x,v) (((v - EPSILON) < x) && (x <( v + EPSILON )))

#define MIN( v1, v2) ( v1 ) < ( v2 ) ? ( v1 ) : ( v2 )
#define MAX( v1, v2) ( v1 ) > ( v2 ) ? ( v1 ) : ( v2 )
#define CLAMP( value, min, max ) MIN( max, MAX( value, min ) )

inline BOOL XMVectorNearEqualBool( XMVECTOR vV1, XMVECTOR vV2, XMVECTOR vEpsilon )
{
    XMVECTOR vResult = XMVectorNearEqual( vV1, vV2, vEpsilon );

    if( XMVectorGetX( vResult ) == 0.0f || XMVectorGetY( vResult ) == 0.0f || XMVectorGetZ( vResult ) == 0.0f )
    {
        return FALSE;
    }

    return TRUE;
}


//-----------------------------------------------------------------------------
// Name: ComputeRotationToVectorMatrix
// Desc: Returns the rotation matrix needed to rotate a point on the line 
//       segment define by vVectorA to align it on the line segment defined by 
//       vVectorB.
//-----------------------------------------------------------------------------
XMMATRIX ComputeRotationToVectorMatrix( XMVECTOR vVectorA, XMVECTOR vVectorB )
{
    FLOAT fAngle = XMVectorGetX( XMVector3AngleBetweenVectors( vVectorA, vVectorB ) );

    XMVECTOR vAxis = XMVector3Normalize( XMVector3Cross( vVectorA, vVectorB ) );
    if( XMVectorNearEqualBool( vAxis, XMVectorZero(), XMVectorSplatEpsilon() ) )
    {
        return XMMatrixIdentity();
    }
    
    XMMATRIX mResult = XMMatrixRotationNormal( vAxis, fAngle );

    return mResult;
}

//-----------------------------------------------------------------------------
// Name: ComputeArmExtractionMatrix
// Desc: Returns a transformation matrix that can be used to convert from the 
//       skeleton coord system to the coordinate system used by this filter.
//-----------------------------------------------------------------------------
XMMATRIX ComputeArmExtractionMatrix( const NUI_SKELETON_DATA* pSkeletonData, BOOL bIsLeftHanded )
{
    assert( pSkeletonData != NULL ); 

    XMMATRIX mTranslateHipToOrigin = XMMatrixTranslationFromVector( -1 * pSkeletonData->SkeletonPositions[ GetHipSkeletonPositionIndex( bIsLeftHanded ) ] );
    XMVECTOR vShoulder = XMVector3Transform( pSkeletonData->SkeletonPositions[ GetShoulderSkeletonPositionIndex( bIsLeftHanded ) ], 
                                             mTranslateHipToOrigin );
    XMVECTOR vOppShoulder = XMVector3Transform( pSkeletonData->SkeletonPositions[ GetShoulderSkeletonPositionIndex( !bIsLeftHanded ) ], 
                                                mTranslateHipToOrigin );

    // Align the hip to Shoulder vector to the Y axis
    XMMATRIX a = ComputeRotationToVectorMatrix( vShoulder, XMVectorSet( 0.0f, 1.0f, 0.0f, 0.0f ) );
    vShoulder    = XMVector3Transform( vShoulder, a );
    vOppShoulder = XMVector3Transform( vOppShoulder, a );

    // Align the hip to shoulder, hip to opposite shoulder plane to the  X - Y plane
    XMMATRIX mTranslateShoulderToOrigin = XMMatrixTranslationFromVector( XMVectorSet( -1 * XMVectorGetX( vShoulder ),
                                                                         -1 * XMVectorGetY( vOppShoulder ), 
                                                                         -1 * XMVectorGetZ( vShoulder ), 0.0f ) );
    vShoulder    = XMVector3Transform( vShoulder, mTranslateShoulderToOrigin );
    vOppShoulder = XMVector3Transform( vOppShoulder, mTranslateShoulderToOrigin );

    XMVECTOR vOppShoulderOnPlane = XMVectorSet( XMVectorGetX( vOppShoulder ), XMVectorGetY( vOppShoulder ), 0.0f, 0.0f );
    XMVECTOR vAngle = XMVector3AngleBetweenVectors( vOppShoulder, vOppShoulderOnPlane );
    XMMATRIX b;
    if( XMVectorGetX( XMVectorIsNaN( vAngle ) ) || XMVectorGetX( XMVectorIsInfinite( vAngle ) ) )
    {
        b = XMMatrixIdentity();
    }
    else
    {
        if( ( bIsLeftHanded && XMVectorGetZ( vOppShoulder ) > 0.0f ) || ( !bIsLeftHanded && XMVectorGetZ( vOppShoulder ) < 0.0f ) )
            b = XMMatrixRotationY( XMVectorGetX( vAngle ) );
        else
            b = XMMatrixRotationY( XMVectorGetX( vAngle * -1 ) );
    }

    XMVECTOR vDeterminant;
    return mTranslateHipToOrigin * a * mTranslateShoulderToOrigin * b * XMMatrixInverse( &vDeterminant, mTranslateShoulderToOrigin );
}


//-----------------------------------------------------------------------------
// Name: ExtractBowlingArmDataFromSkeleton
// Desc: Extracts any data needed for the bowling filter from a 
//       NUI_SKELETON_FRAME structure, converting the positions into vectors 
//       relative to each other with the HIP position at the origin.
//       The positions are rotated so that the hip to shoulder vector lies on 
//       the positive Y axis and the both shoulders are on the X - Y plane.
//-----------------------------------------------------------------------------
void ExtractBowlingArmDataFromSkeleton( BOWLING_HIP_RELATIVE_DATA* pBowlingArmData, 
                                        const NUI_SKELETON_DATA* pSkeletonData, BOOL bIsLeftHanded, FLOAT fDeltaTime )
{
    assert( pBowlingArmData != NULL ); 
    assert( pSkeletonData != NULL ); 

    XMMATRIX mExtractionMatrix = ComputeArmExtractionMatrix( pSkeletonData, bIsLeftHanded );

    pBowlingArmData->vShoulder    = XMVector3Transform( pSkeletonData->SkeletonPositions[ GetShoulderSkeletonPositionIndex( bIsLeftHanded ) ], mExtractionMatrix );
    pBowlingArmData->vElbow       = XMVector3Transform( pSkeletonData->SkeletonPositions[ GetElbowSkeletonPositionIndex( bIsLeftHanded ) ], mExtractionMatrix );
    pBowlingArmData->vWrist       = XMVector3Transform( pSkeletonData->SkeletonPositions[ GetWristSkeletonPositionIndex( bIsLeftHanded ) ], mExtractionMatrix );
    assert( XMVectorNearEqualBool( XMVectorIsNaN( pBowlingArmData->vWrist ), XMVectorSet( 0.0f, 0.0f, 0.0f, 0.0f ), XMVectorSplatEpsilon() ) );
    pBowlingArmData->vHand        = XMVector3Transform( pSkeletonData->SkeletonPositions[ GetHandSkeletonPositionIndex( bIsLeftHanded ) ], mExtractionMatrix );
    pBowlingArmData->vOppShoulder = XMVector3Transform( pSkeletonData->SkeletonPositions[ GetShoulderSkeletonPositionIndex( !bIsLeftHanded ) ], mExtractionMatrix );

    pBowlingArmData->bIsLeftArm = bIsLeftHanded;

    pBowlingArmData->fDeltaTime = fDeltaTime;
}


//-----------------------------------------------------------------------------
// Name: PatchFilteredArmIntoSkeleton
// Desc: Integrates the elbow, wrist and hand positions from a bowling arm 
//       structure back into a  NUI_SKELETON_FRAME structure.
//-----------------------------------------------------------------------------
void PatchFilteredArmIntoSkeleton( NUI_SKELETON_DATA* pSkeletonData, const BOWLING_HIP_RELATIVE_DATA* pBowlingArmData )
{
    assert( pSkeletonData != NULL ); 
    assert( pBowlingArmData != NULL ); 

    XMVECTOR vDeterminant;
    XMMATRIX mIntegrationMatrix = XMMatrixInverse( &vDeterminant, ComputeArmExtractionMatrix( pSkeletonData, pBowlingArmData->bIsLeftArm ) );

    pSkeletonData->SkeletonPositions[ GetElbowSkeletonPositionIndex( pBowlingArmData->bIsLeftArm ) ] = 
        XMVector3Transform( pBowlingArmData->vElbow, mIntegrationMatrix );
    pSkeletonData->SkeletonPositions[ GetWristSkeletonPositionIndex( pBowlingArmData->bIsLeftArm ) ] = 
        XMVector3Transform( pBowlingArmData->vWrist, mIntegrationMatrix );
    pSkeletonData->SkeletonPositions[ GetHandSkeletonPositionIndex( pBowlingArmData->bIsLeftArm ) ]  = 
        XMVector3Transform( pBowlingArmData->vHand, mIntegrationMatrix );
}


//-----------------------------------------------------------------------------
// Name: InferShoulder
// Desc: Predicts the new position of the arm based of the rotation on the 
//       shoulder during the previous frame.
//-----------------------------------------------------------------------------
VOID InferShoulder( BOWLING_HIP_RELATIVE_DATA* pBowlingArmData, 
                    const BOWLING_HIP_RELATIVE_DATA* pBowlingArmRef1, const BOWLING_HIP_RELATIVE_DATA* pBowlingArmRef2, 
                    FLOAT fMinAngle, FLOAT fMaxAngle )
{
    assert( fMinAngle < fMaxAngle );

    XMVECTOR vShoulderToElbow1 = pBowlingArmRef1->vElbow - pBowlingArmRef1->vShoulder;
    XMVECTOR vShoulderToElbow2 = pBowlingArmRef2->vElbow - pBowlingArmRef2->vShoulder;
    assert ( !FLOAT_EQ( pBowlingArmData->fDeltaTime, 0.0f ) );
    assert ( !FLOAT_EQ( pBowlingArmRef2->fDeltaTime, 0.0f ) );
    FLOAT fAngle = XMVectorGetX( XMVector3AngleBetweenVectors( vShoulderToElbow1, vShoulderToElbow2 ) ) * ( pBowlingArmData->fDeltaTime / pBowlingArmRef2->fDeltaTime );
    fAngle = fAngle < fMinAngle ? fMinAngle : fAngle;
    fAngle = fAngle > fMaxAngle ? fMaxAngle : fAngle;

    XMVECTOR vAxis = XMVector3Normalize( XMVector3Cross( vShoulderToElbow1, vShoulderToElbow2 ) );
    XMMATRIX mResult = XMMatrixRotationNormal( vAxis, fAngle );

    XMVECTOR vShoulderToElbow = pBowlingArmData->vElbow - pBowlingArmData->vShoulder;
    XMVECTOR vShoulderToWrist = pBowlingArmData->vWrist - pBowlingArmData->vShoulder;
    XMVECTOR vShoulderToHand  = pBowlingArmData->vHand - pBowlingArmData->vShoulder;

    vShoulderToElbow = XMVector3Transform( vShoulderToElbow, mResult );
    vShoulderToWrist = XMVector3Transform( vShoulderToWrist, mResult );
    vShoulderToHand  = XMVector3Transform( vShoulderToHand, mResult );

    pBowlingArmData->vElbow = vShoulderToElbow + pBowlingArmData->vShoulder;
    pBowlingArmData->vWrist = vShoulderToWrist + pBowlingArmData->vShoulder;
    assert( XMVectorNearEqualBool( XMVectorIsNaN( pBowlingArmData->vWrist ), XMVectorSet( 0.0f, 0.0f, 0.0f, 0.0f ), XMVectorSplatEpsilon() ) );
    pBowlingArmData->vHand  = vShoulderToHand + pBowlingArmData->vShoulder;
}


//-----------------------------------------------------------------------------
// Name: GetElbowAngle
// Desc: Returns the angle formed by the vectors going from the elbow to the 
//       wrist and from the elbow to the shoulder.
//-----------------------------------------------------------------------------
FLOAT GetElbowAngle( const BOWLING_HIP_RELATIVE_DATA* pBowlingArmData )
{
    XMVECTOR vElbowToWrist    = pBowlingArmData->vWrist - pBowlingArmData->vElbow;
    XMVECTOR vElbowToShoulder = pBowlingArmData->vShoulder - pBowlingArmData->vElbow;

    XMVECTOR vAngle = XMVector3AngleBetweenVectors( vElbowToShoulder, vElbowToWrist );

    return XMVectorGetX( vAngle );
}


//-----------------------------------------------------------------------------
// Name: InferElbow
// Desc: Predicts the new position of the arm from the elbow down based on the 
//       change in change in angle of the elbow in the previous frame.
//-----------------------------------------------------------------------------
VOID InferElbow( BOWLING_HIP_RELATIVE_DATA* pBowlingArmData, 
                 const BOWLING_HIP_RELATIVE_DATA* pBowlingArmRef1, const BOWLING_HIP_RELATIVE_DATA* pBowlingArmRef2 )
{
    FLOAT fArmDataAngle = GetElbowAngle( pBowlingArmData );
    if( FLOAT_EQ( fArmDataAngle, XM_PI ) )
        return;

    FLOAT fArmRef1Angle = GetElbowAngle( pBowlingArmRef1 );
    FLOAT fArmRef2Angle = GetElbowAngle( pBowlingArmRef2 );
    FLOAT fAngle = ( fArmRef2Angle - fArmRef1Angle ) * ( pBowlingArmData->fDeltaTime / pBowlingArmRef2->fDeltaTime );
    
    // Don't let the arm bend backward
    if( XMVectorGetZ( pBowlingArmRef1->vWrist ) > XMVectorGetZ( pBowlingArmRef1->vElbow ) || 
        XMVectorGetZ( pBowlingArmRef2->vWrist ) > XMVectorGetZ( pBowlingArmRef2->vElbow ) ||
        fArmDataAngle + fAngle >= XM_PI )
    {
        fAngle = XM_PI - fArmDataAngle;
    }

    XMVECTOR vElbowToWrist    = pBowlingArmData->vWrist - pBowlingArmData->vElbow;
    XMVECTOR vElbowToShoulder = pBowlingArmData->vShoulder - pBowlingArmData->vElbow;
    XMVECTOR vAxis = XMVector3Normalize( XMVector3Cross( vElbowToShoulder, vElbowToWrist ) );
    XMMATRIX mResult = XMMatrixRotationNormal( vAxis, fAngle );

    XMVECTOR vElbowToHand = pBowlingArmData->vHand - pBowlingArmData->vElbow;

    vElbowToWrist = XMVector3Transform( vElbowToWrist, mResult );
    vElbowToHand  = XMVector3Transform( vElbowToHand, mResult );

    pBowlingArmData->vWrist = vElbowToWrist + pBowlingArmData->vElbow;
    pBowlingArmData->vHand  = vElbowToHand + pBowlingArmData->vElbow;
}


//-----------------------------------------------------------------------------
// Name: BlendArms
// Desc: Determines the level of confidence to be attributed to the raw 
//       skeleton.
//-----------------------------------------------------------------------------
VOID BlendArms( BOWLING_HIP_RELATIVE_DATA* pPredictedArm, const BOWLING_HIP_RELATIVE_DATA* pTargetArm )
{
    pPredictedArm->vShoulder += ( pPredictedArm->vShoulder - pTargetArm->vShoulder ) * 0.5f;
    pPredictedArm->vElbow    += ( pTargetArm->vElbow - pPredictedArm->vElbow ) * 0.5f;
    pPredictedArm->vWrist    += ( pTargetArm->vWrist - pPredictedArm->vWrist ) * 0.5f;
    pPredictedArm->vHand     += ( pTargetArm->vHand - pPredictedArm->vHand ) * 0.5f;
    
    pPredictedArm->vOppShoulder += ( pTargetArm->vOppShoulder - pPredictedArm->vOppShoulder ) * 0.5f;
}


//-----------------------------------------------------------------------------
// Name: DetermineSkeletonConfidence
// Desc: Determines the level of confidence to be attributed to the raw 
//       skeleton.
//-----------------------------------------------------------------------------
FLOAT DetermineSkeletonConfidence( const NUI_SKELETON_DATA* pSkeletonData, const BOWLING_HIP_RELATIVE_DATA* pBowlingData )
{
    assert( pSkeletonData->eTrackingState == NUI_SKELETON_TRACKED );

    if( pSkeletonData->eSkeletonPositionTrackingState[ GetWristSkeletonPositionIndex( pBowlingData->bIsLeftArm ) ]    == NUI_SKELETON_POSITION_TRACKED &&
        pSkeletonData->eSkeletonPositionTrackingState[ GetElbowSkeletonPositionIndex( pBowlingData->bIsLeftArm ) ]    == NUI_SKELETON_POSITION_TRACKED &&
        pSkeletonData->eSkeletonPositionTrackingState[ GetShoulderSkeletonPositionIndex( pBowlingData->bIsLeftArm ) ] == NUI_SKELETON_POSITION_TRACKED && 
        !( fabs( XMVectorGetZ( pBowlingData->vWrist ) ) < 0.05f || ( XMVectorGetZ( pBowlingData->vElbow ) > 0.0f && XMVectorGetZ( pBowlingData->vWrist ) < 0.0f ) ) &&
        !(XMVectorGetZ( pBowlingData->vWrist ) >  XMVectorGetZ( pBowlingData->vElbow ) ) )
    {
        return 1.0f;
    }

    float fConfidence = 1.0f;

    if( fabs( XMVectorGetZ( pBowlingData->vWrist ) ) < 0.05f || ( XMVectorGetZ( pBowlingData->vElbow ) > 0.0f && XMVectorGetZ( pBowlingData->vWrist ) < 0.0f ) )
        fConfidence -= 0.33;

    if( XMVectorGetZ( pBowlingData->vWrist ) >  XMVectorGetZ( pBowlingData->vElbow ) ) // Likely bent backward
        fConfidence -= 0.33f;

    if( XMVectorGetZ( pBowlingData->vWrist ) > 0.0f )
        fConfidence -= 0.33f;

    if( pSkeletonData->eSkeletonPositionTrackingState[ GetWristSkeletonPositionIndex( pBowlingData->bIsLeftArm ) ] != NUI_SKELETON_POSITION_TRACKED )
        fConfidence -= 0.33f;

    if( pSkeletonData->eSkeletonPositionTrackingState[ GetElbowSkeletonPositionIndex( pBowlingData->bIsLeftArm ) ] != NUI_SKELETON_POSITION_TRACKED )
        fConfidence -= 0.33f;

    if( pSkeletonData->eSkeletonPositionTrackingState[ GetShoulderSkeletonPositionIndex( pBowlingData->bIsLeftArm ) ] != NUI_SKELETON_POSITION_TRACKED )
        fConfidence -= 0.33f;

    return fConfidence > 0 ? fConfidence : 0.0f;
}


//-----------------------------------------------------------------------------
// Name: DeterminePredictionAccuracy
// Desc: Determines how accurate was the prediction of the filter by comparing 
//       the position of the inferred wrist to the position of the raw wrist.
//-----------------------------------------------------------------------------
FLOAT DeterminePredictionAccuracy( const BOWLING_HIP_RELATIVE_DATA* pActual, const BOWLING_HIP_RELATIVE_DATA* pPredicted )
{
    return XMVectorGetX( XMVector3Length( pPredicted->vWrist - pActual->vWrist ) );
}


//-----------------------------------------------------------------------------
// Name: BasicBowlingFilter::ResetFilterState
// Desc: Resets the filter, including emptying all historic data.
//-----------------------------------------------------------------------------
VOID BasicBowlingFilter::ResetFilterState()
{
    m_dwBowlingArmHistoryCount = 0;

    m_fAccuracy   = 0.0f;
    m_fConfidence = 0.0f;

    Reset();
}


//-----------------------------------------------------------------------------
// Name: BasicBowlingFilter::Reset
// Desc: Resets the filter data, but not the historic data
//-----------------------------------------------------------------------------
VOID BasicBowlingFilter::Reset()
{
    m_dwOverride = 0 ;

    m_bIsBlending  = FALSE;
    m_bIsReturning = FALSE;

    m_dwSteps    = 0;
    m_dwMaxSteps = 30;
}


//-----------------------------------------------------------------------------
// Name: BasicBowlingFilter::FilterSkeleton
// Desc:
//-----------------------------------------------------------------------------
BOOL BasicBowlingFilter::FilterSkeleton( const NUI_SKELETON_DATA* pInputSkeleton, NUI_SKELETON_DATA* pOutputSkeleton, FLOAT fDeltaTime )
{
    // Make the output skeleton the same as the input. We'll patch the infered arm later if needed
    XMemCpy( pOutputSkeleton, pInputSkeleton, sizeof( NUI_SKELETON_DATA ) );

    // Cannot filter a skeleton that is not tracked
    if( pInputSkeleton->eTrackingState != NUI_SKELETON_TRACKED )
    {
        ResetFilterState();
        return FALSE;
    }

    // Ensure DeltaTime is withing acceptable limits
    fDeltaTime = CLAMP( fDeltaTime, 0.015f, 0.099f );

    // Extract arm data from the input skeleton 
    BOWLING_HIP_RELATIVE_DATA InputBowlingArm;
    ExtractBowlingArmDataFromSkeleton( &InputBowlingArm, pInputSkeleton, m_bIsLeftHanded, fDeltaTime );

    // If not enough data to infer
    if( m_dwBowlingArmHistoryCount < 2 || !m_bIsFilterActive )
    {
        UpdateHistoryData( &InputBowlingArm );
        Reset();
        return FALSE;
    }

    BOWLING_HIP_RELATIVE_DATA InferredBowlingArm;
    if( m_bIsBlending )
    {
        XMemCpy( &InferredBowlingArm, &m_BowlingArmHistory[ 1 ], sizeof( BOWLING_HIP_RELATIVE_DATA ) );
        BlendArms( &InferredBowlingArm, &InputBowlingArm );

        UpdateHistoryData( &InferredBowlingArm );
        PatchFilteredArmIntoSkeleton( pOutputSkeleton, &InferredBowlingArm );

        m_dwSteps /= 2;
        if( XMVectorGetX( XMVector3Length( InferredBowlingArm.vWrist - InputBowlingArm.vWrist ) ) < 0.1f || m_dwSteps < 2 )
        {
            ResetFilterState();
        }

        return TRUE;
    }

    // Infer the position of the arm based on the last two positions
    XMemCpy( &InferredBowlingArm, &m_BowlingArmHistory[ 1 ], sizeof( BOWLING_HIP_RELATIVE_DATA ) );
    FLOAT fMaxAngle = 90 / 15 * XM_PI / 180;
    if( m_bIsReturning )
        fMaxAngle *= ( fabs( XMVectorGetZ( XMVector3Normalize( m_BowlingArmHistory[ 0 ].vWrist ) ) ) + 1 ) / 2.0f;
    else
        fMaxAngle *= 2;
    float fMinAngle = m_bIsReturning ? 1.0f * XM_PI / 180 : 90 / 15 * XM_PI / 180;

    fMaxAngle *= fDeltaTime / ( 33.0f / 1000.0f );
    fMinAngle *= fDeltaTime / ( 33.0f / 1000.0f );

    InferShoulder( &InferredBowlingArm, &m_BowlingArmHistory[ 0 ], &m_BowlingArmHistory[ 1 ], fMinAngle, fMaxAngle );
    InferElbow( &InferredBowlingArm, &m_BowlingArmHistory[ 0 ], &m_BowlingArmHistory[ 1 ] );

    UpdateQualityData( pInputSkeleton, &InputBowlingArm, &InferredBowlingArm );

    BOWLING_ACTION eNextAction = GetNextAction( &InputBowlingArm, &InferredBowlingArm );
    switch( eNextAction )
    {
        case BOWLING_ACTION_BLEND:
            BlendArms( &InferredBowlingArm, &InputBowlingArm );
            m_bIsBlending = TRUE;
            break;

        case BOWLING_ACTION_RESET:
            Reset();
            break;

        case BOWLING_ACTION_RESET_ALL:
            ResetFilterState();
            break;

        case BOWLING_ACTION_RETURN_SWING:
            m_dwMaxSteps = m_dwSteps + 15;

            BOWLING_HIP_RELATIVE_DATA Inferred2BowlingArm;
            XMemCpy( &Inferred2BowlingArm, &InferredBowlingArm, sizeof( BOWLING_HIP_RELATIVE_DATA ) );
            InferShoulder( &Inferred2BowlingArm, &m_BowlingArmHistory[ 1 ], &InferredBowlingArm, 90 / 15 * XM_PI / 180, 90 / 15 * XM_PI / 180 * 2 );
            InferElbow( &Inferred2BowlingArm, &m_BowlingArmHistory[ 1 ], &InferredBowlingArm );

            UpdateHistoryData( &Inferred2BowlingArm );

            m_bIsReturning = TRUE;
            break;

        case BOWLING_ACTION_USE_INPUT:
            break;

        case BOWLING_ACTION_USE_PREDICTED:
            ++ m_dwSteps;
            break;

        default:
            assert( false );
            break;
    }

    switch( eNextAction )
    {
        case BOWLING_ACTION_BLEND:
        case BOWLING_ACTION_RETURN_SWING:
        case BOWLING_ACTION_USE_PREDICTED:
            if( m_dwOverride > 0 )
                -- m_dwOverride;
            UpdateHistoryData( &InferredBowlingArm );
            PatchFilteredArmIntoSkeleton( pOutputSkeleton, &InferredBowlingArm );
            return TRUE;

        case BOWLING_ACTION_RESET:
        case BOWLING_ACTION_RESET_ALL:
        case BOWLING_ACTION_USE_INPUT:
            UpdateHistoryData( &InputBowlingArm );
            return FALSE;

        default:
            assert( false );
            UpdateHistoryData( &InputBowlingArm );
            return FALSE;
    }
}


VOID BasicBowlingFilter::UpdateQualityData( const NUI_SKELETON_DATA* pInputSkeleton, 
                                            const BOWLING_HIP_RELATIVE_DATA* pInputArm, 
                                            const BOWLING_HIP_RELATIVE_DATA* pPredictedArm )
{
    // Determine skeleton arm quality
    m_fConfidence = DetermineSkeletonConfidence( pInputSkeleton, pInputArm );

    if( m_fConfidence >= CONFIDENCE_THRESHOLD_HIGH )
    {
        // Compute Accuracy
        m_fAccuracy = DeterminePredictionAccuracy( pInputArm, pPredictedArm );
        if( m_fAccuracy < 0.05f )
        {
            if( m_dwOverride < 5 )
                ++ m_dwOverride;
        }
        else 
        {
            if( m_dwOverride > 0 )
                -- m_dwOverride;
        }
    }
}


VOID BasicBowlingFilter::UpdateHistoryData( const BOWLING_HIP_RELATIVE_DATA* pBowlingArmData )
{
    XMemCpy( &m_BowlingArmHistory[ 0 ], &m_BowlingArmHistory[ 1 ], sizeof( BOWLING_HIP_RELATIVE_DATA ) );
    XMemCpy( &m_BowlingArmHistory[ 1 ], pBowlingArmData, sizeof( BOWLING_HIP_RELATIVE_DATA ) );
    if( m_dwBowlingArmHistoryCount < 2 )
    {   
        ++ m_dwBowlingArmHistoryCount;
    }
}


BOWLING_ACTION BasicBowlingFilter::GetNextAction( const BOWLING_HIP_RELATIVE_DATA* pInputArm, 
                                                  const BOWLING_HIP_RELATIVE_DATA* pInferredArm ) const
{
    assert( !m_bIsBlending );

    // If we are more confident in the result of this filter than in the skeleton
    if( m_fConfidence <= CONFIDENCE_THRESHOLD_LOW || 
        ( m_fAccuracy > 0.05f && m_dwOverride > 2 && XMVectorGetZ(  pInferredArm->vWrist ) > -0.05f ) )
    {
            // if back limit reached
            FLOAT fAngle = XMVectorGetX( XMVector3AngleBetweenVectors( pInferredArm->vElbow -  pInferredArm->vShoulder, 
                                                                       XMVectorZero() -  pInferredArm->vShoulder ) );
            if( !m_bIsReturning && 
                ( m_dwSteps == 15 || ( m_dwSteps > 1 && XMVectorGetZ(  pInferredArm->vElbow ) > 0.0f && fAngle > 90 * XM_PI / 180 ) ) )
            {
                return BOWLING_ACTION_RETURN_SWING;
            }
            else
            {
                return BOWLING_ACTION_USE_PREDICTED;
            }
    }

    // If the skeleton is good and our prediction is bad, reset the filter, the player is probably not bowling...
    if( m_fConfidence >= CONFIDENCE_THRESHOLD_HIGH && m_fAccuracy > 0.05f && XMVectorGetZ(  pInferredArm->vWrist ) > -0.05f )
    {
        if( m_dwSteps > 2 )
        {
            if( XMVectorGetX( XMVector3Length(  pInferredArm->vWrist - pInputArm->vWrist ) ) > 0.1f )
            {
                return BOWLING_ACTION_BLEND;
            }
            else
            {
                return BOWLING_ACTION_RESET;
            }
        }
        else
        {
            return BOWLING_ACTION_RESET;
        }
    }

    return BOWLING_ACTION_USE_INPUT;
}


FLOAT BasicBowlingFilter::DebugOutput( ATG::Font* pFont, FLOAT fX, FLOAT fY, DWORD dwColor, DWORD dwFlags ) const
{
    WCHAR wszMessage[ 256 ];

    if( m_bIsFilterActive )
    {
        pFont->DrawText( fX, fY, dwColor, L"Filter is active", dwFlags );
    }
    else
    {
        pFont->DrawText( fX, fY, dwColor, L"Filter is NOT active", dwFlags );
    }

    wsprintfW( wszMessage, L"Prediction accuracy: %f", m_fAccuracy );
    pFont->DrawText( fX, fY + 20, dwColor, wszMessage, dwFlags );
    wsprintfW( wszMessage, L"Confidence level: %f", m_fConfidence );
    pFont->DrawText( fX, fY + 40, dwColor, wszMessage, dwFlags );
    wsprintfW( wszMessage, L"Override counter: %lu", m_dwOverride );
    pFont->DrawText( fX, fY + 60, dwColor, wszMessage, dwFlags );
    wsprintfW( wszMessage, L"Step %lu of %lu", m_dwSteps, m_dwMaxSteps );
    pFont->DrawText( fX, fY + 80, dwColor, wszMessage, dwFlags );

    if( m_bIsReturning )
        pFont->DrawText( fX, fY + 100, dwColor, L"Returning", dwFlags );

    if( m_bIsBlending )
        pFont->DrawText( fX, fY + 120, dwColor, L"Blending", dwFlags );

    return fY + 140;
}


FLOAT ComputeWristPosition( const NUI_SKELETON_DATA* pSkeletonData, BOOL bIsLeftHanded )
{
    assert( pSkeletonData->eTrackingState == NUI_SKELETON_TRACKED );

    BOWLING_HIP_RELATIVE_DATA HipRelativeArmArmData;
    ExtractBowlingArmDataFromSkeleton( &HipRelativeArmArmData, pSkeletonData, bIsLeftHanded, 0.0f );

    XMVECTOR vShoulderToWrist    = HipRelativeArmArmData.vWrist - HipRelativeArmArmData.vShoulder;
    vShoulderToWrist = XMVectorSet( 0.0f, XMVectorGetY( vShoulderToWrist ), XMVectorGetZ( vShoulderToWrist ), 0.0f );
    vShoulderToWrist = XMVector3Normalize( vShoulderToWrist );
    XMVECTOR vShoulderToWristOnZ = XMVectorSet( 0.0f, -1.0f, 0.0f, 0.0f );

    FLOAT fAngle = XMVectorGetX( XMVector3AngleBetweenNormals( vShoulderToWrist, vShoulderToWristOnZ ) );
    if( XMVectorGetZ( vShoulderToWrist ) < 0 )
    {
        if( fAngle >= 0 && fAngle <= XM_PI / 2)
        {
            fAngle = ( fAngle - XM_PI / 2 ) * -1;
        }
        else
        {
            fAngle -= XM_PI / 2;
            fAngle = ( fAngle - XM_PI / 2 ) * -1;
            fAngle += 2 * XM_PI /3;
        }
    }
    else
    {
        if( fAngle >= 0 && fAngle <= XM_PI / 2)
        {
            fAngle += XM_PI / 2;
        }
        else
        {
            fAngle += XM_PI / 2;
        }
    }

    return fAngle;
}
