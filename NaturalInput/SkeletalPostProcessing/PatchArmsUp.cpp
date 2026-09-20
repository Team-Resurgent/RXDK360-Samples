//--------------------------------------------------------------------------------------
// PatchArmsUp.cpp
//
// Implements a filter that detects when the user has one or both of their arms above 
// their head.  When this condition is detected, the arm(s), shoulder(s), and head are
// procedurally rebuilt in a stable pose.
// 
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "PatchArmsUp.h"

//--------------------------------------------------------------------------------------
// Name: FilterArmsUp::Initialize()
// Desc: Resets filter state to defaults.
//--------------------------------------------------------------------------------------
VOID FilterArmsUp::Initialize()
{
    m_PatchingLeftArm.SetSpeed( 1.5f );
    m_PatchingRightArm.SetSpeed( 1.5f );
#if FILTER_DEBUG
    ZeroMemory( m_fFilterDebug, sizeof(m_fFilterDebug) );
#endif
    ResetFilterState();
}

//--------------------------------------------------------------------------------------
// Name: FilterArmsUp::FilterSkeleton
// Desc: Implements the per-frame filter logic for the arms up patch.
//--------------------------------------------------------------------------------------
BOOL FilterArmsUp::FilterSkeleton( SkeletonFilterContext* pContext, const NUI_SKELETON_DATA* pInputSkeleton, NUI_SKELETON_DATA* pOutputSkeleton, const FLOAT fDeltaNuiTime )
{
    const SkeletonCommonProcessing* pProcess = pContext->pProcess;
    static const XMVECTOR vHalf = XMVectorReplicate( 0.5f );

    // update our lerp modules
    m_PatchingLeftArm.Tick( fDeltaNuiTime );
    m_PatchingRightArm.Tick( fDeltaNuiTime );

    // exit early if we lose tracking on the entire skeleton
    if( pInputSkeleton->eTrackingState != NUI_SKELETON_TRACKED )
    {
        m_PatchingLeftArm.SetEnabled( FALSE );
        m_PatchingRightArm.SetEnabled( FALSE );
        return FALSE;
    }

    // exit early if we lose tracking on the hips
    if( !pProcess->Tracked.HipCenter ||
        !pProcess->Tracked.HipLeft ||
        !pProcess->Tracked.HipRight )
    {
        m_PatchingLeftArm.SetEnabled( FALSE );
        m_PatchingRightArm.SetEnabled( FALSE );
        return FALSE;
    }

    BOOL bModifiedSkeleton = FALSE;

    // if body basis is invalid, exit early
    if( !pProcess->BodyBasisValid )
    {
        return FALSE;
    }

    // compute basis vectors from the femurs
    XMVECTOR vLeftLegUp = pProcess->BodyUp;
    XMVECTOR vRightLegUp = pProcess->BodyUp;
    if( pProcess->Tracked.KneeLeft )
    {
        vLeftLegUp = XMVector3Normalize( pProcess->LeftFemur );
    }
    if( pProcess->Tracked.KneeRight )
    {
        vRightLegUp = XMVector3Normalize( pProcess->RightFemur );
    }

    // if we have somewhat valid data on the shoulder center, go into the fixup code
    if( pProcess->TrackedOrInferred.ShoulderCenter || m_PatchingLeftArm.GetLerpEnabled() || m_PatchingRightArm.GetLerpEnabled() )
    {
        // compute clavicle length ratio
        XMVECTOR vClavicleLengthRatio = pProcess->LeftClavicleLength / pProcess->RightClavicleLength;
#if FILTER_DEBUG
        m_fFilterDebug[12] = XMVectorGetX( vClavicleLengthRatio );
#endif

        // compute upper arms
        XMVECTOR vLeftUpperArm = pProcess->Position.ElbowLeft - pProcess->Position.ShoulderLeft;
        XMVECTOR vRightUpperArm = pProcess->Position.ElbowRight - pProcess->Position.ShoulderRight;

        // compute lower arms
        XMVECTOR vLeftLowerArm = pProcess->Position.WristLeft - pProcess->Position.ElbowLeft;
        XMVECTOR vRightLowerArm = pProcess->Position.WristRight - pProcess->Position.ElbowRight;

        // check for broken elbows (lower arm reverses upper arm)
        FLOAT fLeftElbowAngle = XMVectorGetX( XMVector3Dot( XMVector3Normalize( vLeftUpperArm ), XMVector3Normalize( vLeftLowerArm ) ) );
        FLOAT fRightElbowAngle = XMVectorGetX( XMVector3Dot( XMVector3Normalize( vRightUpperArm ), XMVector3Normalize( vRightLowerArm ) ) );
        BOOL bLeftElbowBroken = fLeftElbowAngle <= -0.5f;
        BOOL bRightElbowBroken = fRightElbowAngle <= -0.5f;

        // check for broken shoulders (upper arm reverses clavicle)
        FLOAT fLeftShoulderUp = XMVectorGetX( XMVector3Dot( XMVector3Normalize( vLeftUpperArm ), pProcess->BodyUp ) );
        FLOAT fRightShoulderUp = XMVectorGetX( XMVector3Dot( XMVector3Normalize( vRightUpperArm ), pProcess->BodyUp ) );
        FLOAT fLeftShoulderAngle = XMVectorGetX( XMVector3Dot( XMVector3Normalize( vLeftUpperArm ), XMVector3Normalize( pProcess->LeftClavicle ) ) );
        FLOAT fRightShoulderAngle = XMVectorGetX( XMVector3Dot( XMVector3Normalize( vRightUpperArm ), XMVector3Normalize( pProcess->RightClavicle ) ) );
        BOOL bLeftShoulderBroken = FALSE;
        if( fLeftShoulderUp > 0 )
        {
            bLeftShoulderBroken = fLeftShoulderAngle <= 0.0f;
        }
        else
        {
            bLeftShoulderBroken = fLeftShoulderAngle <= -0.5f;
        }
        BOOL bRightShoulderBroken = FALSE;
        if( fRightShoulderUp > 0 )
        {
            bRightShoulderBroken = fRightShoulderAngle <= 0.0f;
        }
        else
        {
            bRightShoulderBroken = fRightShoulderAngle <= -0.5f;
        }

        // check for flipped clavicles - clavicles going the wrong way on the body
        FLOAT fLeftClavicleDirection = XMVectorGetX( XMVector3Dot( -pProcess->BodyRight, XMVector3Normalize( pProcess->LeftClavicle ) ) );
        FLOAT fRightClavicleDirection = XMVectorGetX( XMVector3Dot( pProcess->BodyRight, XMVector3Normalize( pProcess->RightClavicle ) ) );
        BOOL bFlippedLeftClavicle = ( fLeftClavicleDirection < 0.3f );
        BOOL bFlippedRightClavicle = ( fRightClavicleDirection < 0.3f );

        // check how high the arms are up
        const FLOAT fHighArmMinThreshold = 0.30f;
        const FLOAT fHighArmMaxThreshold = 0.60f;
        FLOAT fLeftArmUpHigh = (FLOAT)( pProcess->LeftArmArrangement == SkeletonCommonProcessing::AA_RaisedUp ) * 
                              max( CompareAgainstThreshold( pProcess->LeftArmDotProducts[SkeletonCommonProcessing::ADP_Elbow], fHighArmMinThreshold, fHighArmMaxThreshold ),
                                   CompareAgainstThreshold( pProcess->LeftArmDotProducts[SkeletonCommonProcessing::ADP_Wrist], fHighArmMinThreshold, fHighArmMaxThreshold ) );
        FLOAT fRightArmUpHigh = (FLOAT)( pProcess->RightArmArrangement == SkeletonCommonProcessing::AA_RaisedUp ) *
                              max( CompareAgainstThreshold( pProcess->RightArmDotProducts[SkeletonCommonProcessing::ADP_Elbow], fHighArmMinThreshold, fHighArmMaxThreshold ),
                                   CompareAgainstThreshold( pProcess->RightArmDotProducts[SkeletonCommonProcessing::ADP_Wrist], fHighArmMinThreshold, fHighArmMaxThreshold ) );

        // check if arms are clipped (all joints are inferred or not tracked)
        BOOL bLeftArmClipped = !pProcess->Tracked.ShoulderLeft && !pProcess->Tracked.ElbowLeft && !pProcess->Tracked.WristLeft;
        BOOL bRightArmClipped = !pProcess->Tracked.ShoulderRight && !pProcess->Tracked.ElbowRight && !pProcess->Tracked.WristRight;

        // make the determination if we should apply the patch or not
        FLOAT fLeftArmUp = ( fLeftArmUpHigh ) +
                          (FLOAT)( pProcess->LeftArmArrangement == SkeletonCommonProcessing::AA_ElbowHighWristLow && ( bLeftShoulderBroken || bLeftElbowBroken ) ) +
                          (FLOAT)( pProcess->LeftArmArrangement != SkeletonCommonProcessing::AA_Down && bFlippedLeftClavicle ) +
                          (FLOAT)( pProcess->LeftArmArrangement != SkeletonCommonProcessing::AA_Down && bLeftArmClipped ) +
                          (FLOAT)( pProcess->LeftArmArrangement != SkeletonCommonProcessing::AA_Down && bLeftElbowBroken && !pProcess->Tracked.ElbowLeft );

        FLOAT fRightArmUp = ( fRightArmUpHigh ) +
                           (FLOAT)( pProcess->RightArmArrangement == SkeletonCommonProcessing::AA_ElbowHighWristLow && ( bRightShoulderBroken || bRightElbowBroken ) ) +
                           (FLOAT)( pProcess->RightArmArrangement != SkeletonCommonProcessing::AA_Down && bFlippedRightClavicle ) +
                           (FLOAT)( pProcess->RightArmArrangement != SkeletonCommonProcessing::AA_Down && bRightArmClipped ) +
                           (FLOAT)( pProcess->RightArmArrangement != SkeletonCommonProcessing::AA_Down && bRightElbowBroken && !pProcess->Tracked.ElbowRight );

#if FILTER_DEBUG
        m_fFilterDebug[14] = fLeftArmUp;
        m_fFilterDebug[15] = fRightArmUp;
#endif

        // enable patching on the left arm
        m_PatchingLeftArm.SetEnabled( fLeftArmUp );

        // enable patching on the right arm
        m_PatchingRightArm.SetEnabled( fRightArmUp );

        // apply patch if we have non zero patching amounts
        if( m_PatchingLeftArm.GetLerpEnabled() || m_PatchingRightArm.GetLerpEnabled() )
        {
            XMVECTOR vAverageUpperArmLength = ( pProcess->History.LeftUpperArmLength + pProcess->History.RightUpperArmLength ) * vHalf;
            XMVECTOR vAverageLowerArmLength = ( pProcess->History.LeftLowerArmLength + pProcess->History.RightLowerArmLength ) * vHalf;
            XMVECTOR vAverageWristLength = pProcess->History.WristLength;

            // compute average clavicle length
            XMVECTOR vAverageClavicleLength = ( pProcess->LeftClavicleLength + pProcess->RightClavicleLength ) * vHalf;

            XMVECTOR vShoulderCenter = pProcess->Position.ShoulderCenter;

            FLOAT fClavicleLengthRatio = XMVectorGetX( vClavicleLengthRatio );
            // If the clavicles are as much as 25% different from each other in length, then the "fix shoulder center" value starts incrementing,
            // up to 75% different or more from each other in length.
            FLOAT fFixShoulderCenter = ( 1.0f - CompareAgainstThreshold( fClavicleLengthRatio, 1.0f / 1.75f, 1.0f / 1.25f ) ) + CompareAgainstThreshold( fClavicleLengthRatio, 1.25f, 1.75f );
            XMVECTOR vTorsoUp = XMVector3Normalize( XMVectorLerp( pProcess->BodyUp, pProcess->AverageFemurVector, fFixShoulderCenter ) );

            // fix up the shoulder center if both the left and right arms are up in the air
            if( ( m_PatchingLeftArm.GetLerpEnabled() && m_PatchingRightArm.GetLerpEnabled() ) || fFixShoulderCenter )
            {
                vShoulderCenter = pProcess->Position.Spine + vTorsoUp * pProcess->History.TorsoLength;

                FLOAT fShoulderLerp = ( m_PatchingLeftArm.GetSmoothValue() + m_PatchingRightArm.GetSmoothValue() ) * 0.5f;
                fShoulderLerp = max( fShoulderLerp, fFixShoulderCenter );

                LerpVector( pOutputSkeleton->SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_CENTER], vShoulderCenter, fShoulderLerp );
                pOutputSkeleton->eSkeletonPositionTrackingState[NUI_SKELETON_POSITION_SHOULDER_CENTER] = NUI_SKELETON_POSITION_TRACKED;
            }

            XMVECTOR vRightArmDirection = XMVector3Normalize( vLeftLegUp + pProcess->BodyRight * vHalf );
            XMVECTOR vLeftArmDirection = XMVector3Normalize( vRightLegUp - pProcess->BodyRight * vHalf );
            XMVECTOR vForearmDirection = pProcess->BodyUp;

            // patch the left arm procedurally
            if( m_PatchingLeftArm.GetLerpEnabled() )
            {
                XMVECTOR vLeftShoulder = vShoulderCenter - pProcess->BodyRight * vAverageClavicleLength;

                // use the right leg's direction vector to place the left elbow and wrist in a natural pose
                XMVECTOR vLeftElbow = vLeftShoulder + vLeftArmDirection * vAverageUpperArmLength;
                XMVECTOR vLeftWrist = vLeftElbow + vForearmDirection * vAverageLowerArmLength;

                XMVECTOR vLeftHand = vLeftWrist + vForearmDirection * vAverageWristLength;

                FLOAT fLerpValue = m_PatchingLeftArm.GetSmoothValue();
                FLOAT fShoulderLerpValue = 1.0f;

                LerpVector( pOutputSkeleton->SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_LEFT], vLeftShoulder, fShoulderLerpValue );
                pOutputSkeleton->eSkeletonPositionTrackingState[NUI_SKELETON_POSITION_SHOULDER_LEFT] = NUI_SKELETON_POSITION_TRACKED;

                if( fLerpValue > 0 )
                {
                    LerpVector( pOutputSkeleton->SkeletonPositions[NUI_SKELETON_POSITION_ELBOW_LEFT], vLeftElbow, fLerpValue );
                    pOutputSkeleton->eSkeletonPositionTrackingState[NUI_SKELETON_POSITION_ELBOW_LEFT] = NUI_SKELETON_POSITION_TRACKED;
                    LerpVector( pOutputSkeleton->SkeletonPositions[NUI_SKELETON_POSITION_WRIST_LEFT], vLeftWrist, fLerpValue );
                    pOutputSkeleton->eSkeletonPositionTrackingState[NUI_SKELETON_POSITION_WRIST_LEFT] = NUI_SKELETON_POSITION_TRACKED;
                    LerpVector( pOutputSkeleton->SkeletonPositions[NUI_SKELETON_POSITION_HAND_LEFT], vLeftHand, fLerpValue );
                    pOutputSkeleton->eSkeletonPositionTrackingState[NUI_SKELETON_POSITION_HAND_LEFT] = NUI_SKELETON_POSITION_INFERRED;
                }
            }

            // patch the right arm procedurally
            if( m_PatchingRightArm.GetLerpEnabled() )
            {
                XMVECTOR vRightShoulder = vShoulderCenter + pProcess->BodyRight * vAverageClavicleLength;

                // use the left leg's direction vector to place the right elbow and wrist in a natural pose
                XMVECTOR vRightElbow = vRightShoulder + vRightArmDirection * vAverageUpperArmLength;
                XMVECTOR vRightWrist = vRightElbow + vForearmDirection * vAverageLowerArmLength;

                XMVECTOR vRightHand = vRightWrist + vForearmDirection * vAverageWristLength;

                FLOAT fLerpValue = m_PatchingRightArm.GetSmoothValue();
                FLOAT fShoulderLerpValue = 1.0f;

                LerpVector( pOutputSkeleton->SkeletonPositions[NUI_SKELETON_POSITION_SHOULDER_RIGHT], vRightShoulder, fShoulderLerpValue );
                pOutputSkeleton->eSkeletonPositionTrackingState[NUI_SKELETON_POSITION_SHOULDER_RIGHT] = NUI_SKELETON_POSITION_TRACKED;

                if( fLerpValue > 0 )
                {
                    LerpVector( pOutputSkeleton->SkeletonPositions[NUI_SKELETON_POSITION_ELBOW_RIGHT], vRightElbow, fLerpValue );
                    pOutputSkeleton->eSkeletonPositionTrackingState[NUI_SKELETON_POSITION_ELBOW_RIGHT] = NUI_SKELETON_POSITION_TRACKED;
                    LerpVector( pOutputSkeleton->SkeletonPositions[NUI_SKELETON_POSITION_WRIST_RIGHT], vRightWrist, fLerpValue );
                    pOutputSkeleton->eSkeletonPositionTrackingState[NUI_SKELETON_POSITION_WRIST_RIGHT] = NUI_SKELETON_POSITION_TRACKED;
                    LerpVector( pOutputSkeleton->SkeletonPositions[NUI_SKELETON_POSITION_HAND_RIGHT], vRightHand, fLerpValue );
                    pOutputSkeleton->eSkeletonPositionTrackingState[NUI_SKELETON_POSITION_HAND_RIGHT] = NUI_SKELETON_POSITION_INFERRED;
                }
            }

            // patch the head location
            FLOAT fHeadLerp = max( m_PatchingLeftArm.GetSmoothValue(), m_PatchingRightArm.GetSmoothValue() );
            fHeadLerp = max( fHeadLerp, fFixShoulderCenter );
            XMVECTOR vHeadDirection = vTorsoUp;

            LerpVector( pOutputSkeleton->SkeletonPositions[NUI_SKELETON_POSITION_HEAD], vShoulderCenter + pProcess->History.NeckLength * vHeadDirection, fHeadLerp );
            pOutputSkeleton->eSkeletonPositionTrackingState[NUI_SKELETON_POSITION_HEAD] = NUI_SKELETON_POSITION_TRACKED;

            bModifiedSkeleton = TRUE;
        }
    }

    return bModifiedSkeleton;
}

//--------------------------------------------------------------------------------------
// Name: FilterArmsUp::ResetFilterState()
// Desc: Resets the lerp values to zero.
//--------------------------------------------------------------------------------------
VOID FilterArmsUp::ResetFilterState()
{
    m_PatchingLeftArm.Reset();
    m_PatchingRightArm.Reset();
}

//--------------------------------------------------------------------------------------
// Name: FilterArmsUp::DebugRenderUI
// Desc: Displays a series of debug values from the arms up filter logic.
//--------------------------------------------------------------------------------------
VOID FilterArmsUp::DebugRenderUI( IFilterDebugRenderer* pDebugRenderer )
{
    pDebugRenderer->DrawTextLine( 1, 1, 0xFF00FFFF, 0, TRUE, L"Patch left %0.2f right %0.2f\n", m_PatchingLeftArm.GetSmoothValue(), m_PatchingRightArm.GetSmoothValue() );

#if FILTER_DEBUG
    DWORD DebugColor = 0xFF00FFFF;
    for( DWORD i = 0; i < ARRAYSIZE( m_fFilterDebug ); i += 4 )
    {
        DWORD Count = min( 4, ARRAYSIZE(m_fFilterDebug) - i );
        switch( Count )
        {
        case 4:
            pDebugRenderer->DrawTextLine( 1, 1, DebugColor, 0, TRUE, L"%0.2f %0.2f %0.2f %0.2f", m_fFilterDebug[i], m_fFilterDebug[i+1], m_fFilterDebug[i+2], m_fFilterDebug[i+3] );
            break;
        case 3:
            pDebugRenderer->DrawTextLine( 1, 1, DebugColor, 0, TRUE, L"%0.2f %0.2f %0.2f", m_fFilterDebug[i], m_fFilterDebug[i+1], m_fFilterDebug[i+2] );
            break;
        case 2:
            pDebugRenderer->DrawTextLine( 1, 1, DebugColor, 0, TRUE, L"%0.2f %0.2f", m_fFilterDebug[i], m_fFilterDebug[i+1] );
            break;
        case 1:
            pDebugRenderer->DrawTextLine( 1, 1, DebugColor, 0, TRUE, L"%0.2f", m_fFilterDebug[i] );
            break;
        }
    }
#endif
}
