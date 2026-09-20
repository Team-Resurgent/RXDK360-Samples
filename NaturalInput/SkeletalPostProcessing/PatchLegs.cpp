//--------------------------------------------------------------------------------------
// PatchLegs.cpp
//
// Contains a set of filters designed to improve joint position consistency in the legs
// and hips.
// 
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "PatchLegs.h"

//--------------------------------------------------------------------------------------

VOID FilterClippedLegs::Initialize()
{
    ResetFilterState();
}

BOOL FilterClippedLegs::FilterSkeleton( SkeletonFilterContext* pContext, const NUI_SKELETON_DATA* pInputSkeleton, NUI_SKELETON_DATA* pOutputSkeleton, const FLOAT fDeltaNuiTime )
{
    if ( pInputSkeleton->eTrackingState != NUI_SKELETON_TRACKED )
    {
        m_FilterDoubleExp.Reset();
    }

    m_FilterDoubleExp.Update( pInputSkeleton );
    const XMVECTOR* pFilteredPositions = m_FilterDoubleExp.GetFilteredJoints();

    // Update lerp state with the current delta NUI time.
    m_LerpLeftKnee.Tick( fDeltaNuiTime );
    m_LerpLeftAnkle.Tick( fDeltaNuiTime );
    m_LerpLeftFoot.Tick( fDeltaNuiTime );
    m_LerpRightKnee.Tick( fDeltaNuiTime );
    m_LerpRightAnkle.Tick( fDeltaNuiTime );
    m_LerpRightFoot.Tick( fDeltaNuiTime );

    // Exit early if we do not have a valid body basis - too much of the skeleton is invalid.
    const SkeletonCommonProcessing* pProcess = pContext->pProcess;
    if( !pProcess->BodyBasisValid )
    {
        return FALSE;
    }

    // Determine if the skeleton is clipped by the bottom of the FOV.
    const BOOL bClippedBottom = ( ( pInputSkeleton->dwQualityFlags & NUI_SKELETON_QUALITY_CLIPPED_BOTTOM ) == NUI_SKELETON_QUALITY_CLIPPED_BOTTOM );

#if FILTER_DEBUG
    m_FilterDebug[0] = pProcess->Tracked.KneeLeft ? 1.0f : 0.0f;
    m_FilterDebug[1] = pProcess->Tracked.AnkleLeft ? 1.0f : 0.0f;
    m_FilterDebug[2] = pProcess->Tracked.FootLeft ? 1.0f : 0.0f;

    m_FilterDebug[4] = pProcess->Tracked.KneeRight ? 1.0f : 0.0f;
    m_FilterDebug[5] = pProcess->Tracked.AnkleRight ? 1.0f : 0.0f;
    m_FilterDebug[6] = pProcess->Tracked.FootRight ? 1.0f : 0.0f;

    m_FilterDebug[15] = (FLOAT)bClippedBottom;
#endif

    // These masks define how much of the filtered joint positions should be blended
    // with the raw positions.  Based on the tracking state of the leg joints, we apply
    // more filtered data as more joints lose tracking.
    const FLOAT fLegMaskTable[4][3] =
    {
    //    knee  ankle foot      blend amount
        { 0.0f, 0.0f, 0.0f },   // all joints are tracked
        { 0.0f, 0.0f, 0.5f },   // foot is inferred
        { 0.5f, 1.0f, 1.0f },   // ankle is inferred
        { 1.0f, 1.0f, 1.0f }    // knee is inferred
    };

    // Select a mask for the left leg depending on which joints are not tracked.
    const FLOAT* pLeftLegMask = fLegMaskTable[0];
    if( !pProcess->Tracked.KneeLeft )
    {
        pLeftLegMask = fLegMaskTable[3];
    }
    else if( !pProcess->Tracked.AnkleLeft )
    {
        pLeftLegMask = fLegMaskTable[2];
    }
    else if( !pProcess->Tracked.FootLeft )
    {
        pLeftLegMask = fLegMaskTable[1];
    }

    // Select a mask for the right leg depending on which joints are not tracked.
    const FLOAT* pRightLegMask = fLegMaskTable[0];
    if( !pProcess->Tracked.KneeRight )
    {
        pRightLegMask = fLegMaskTable[3];
    }
    else if( !pProcess->Tracked.AnkleRight )
    {
        pRightLegMask = fLegMaskTable[2];
    }
    else if( !pProcess->Tracked.FootRight )
    {
        pRightLegMask = fLegMaskTable[1];
    }

    // If the skeleton is not clipped by the bottom of the FOV, cut the filtered data
    // blend in half.
    FLOAT fClipMask = bClippedBottom ? 1.0f : 0.5f;

    // Apply the mask values to the joints of each leg, by placing the mask values into the lerp targets.
    m_LerpLeftKnee.SetEnabled( pLeftLegMask[0] * fClipMask );
    m_LerpLeftAnkle.SetEnabled( pLeftLegMask[1] * fClipMask );
    m_LerpLeftFoot.SetEnabled( pLeftLegMask[2] * fClipMask );
    m_LerpRightKnee.SetEnabled( pRightLegMask[0] * fClipMask );
    m_LerpRightAnkle.SetEnabled( pRightLegMask[1] * fClipMask );
    m_LerpRightFoot.SetEnabled( pRightLegMask[2] * fClipMask );

    // The bSkeletonUpdated flag tracks whether we have modified the output skeleton or not.
    BOOL bSkeletonUpdated = FALSE;

    // Apply lerp to the left knee, which will blend the raw joint position with the filtered joint position based on the current lerp value.
    if( m_LerpLeftKnee.GetLerpEnabled() )
    {
        LerpVector( pOutputSkeleton->SkeletonPositions[NUI_SKELETON_POSITION_KNEE_LEFT], pFilteredPositions[NUI_SKELETON_POSITION_KNEE_LEFT], m_LerpLeftKnee.GetSmoothValue() );
        pOutputSkeleton->eSkeletonPositionTrackingState[NUI_SKELETON_POSITION_KNEE_LEFT] = NUI_SKELETON_POSITION_TRACKED;
        bSkeletonUpdated = TRUE;
    }

    // Apply lerp to the left ankle.
    if( m_LerpLeftAnkle.GetLerpEnabled() )
    {
        LerpVector( pOutputSkeleton->SkeletonPositions[NUI_SKELETON_POSITION_ANKLE_LEFT], pFilteredPositions[NUI_SKELETON_POSITION_ANKLE_LEFT], m_LerpLeftAnkle.GetSmoothValue() );
        pOutputSkeleton->eSkeletonPositionTrackingState[NUI_SKELETON_POSITION_ANKLE_LEFT] = NUI_SKELETON_POSITION_TRACKED;
        bSkeletonUpdated = TRUE;
    }

    // Apply lerp to the left foot.
    if( m_LerpLeftFoot.GetLerpEnabled() )
    {
        LerpVector( pOutputSkeleton->SkeletonPositions[NUI_SKELETON_POSITION_FOOT_LEFT], pFilteredPositions[NUI_SKELETON_POSITION_FOOT_LEFT], m_LerpLeftFoot.GetSmoothValue() );
        bSkeletonUpdated = TRUE;
    }

    // Apply lerp to the right knee.
    if( m_LerpRightKnee.GetLerpEnabled() )
    {
        LerpVector( pOutputSkeleton->SkeletonPositions[NUI_SKELETON_POSITION_KNEE_RIGHT], pFilteredPositions[NUI_SKELETON_POSITION_KNEE_RIGHT], m_LerpRightKnee.GetSmoothValue() );
        pOutputSkeleton->eSkeletonPositionTrackingState[NUI_SKELETON_POSITION_KNEE_RIGHT] = NUI_SKELETON_POSITION_TRACKED;
        bSkeletonUpdated = TRUE;
    }

    // Apply lerp to the right ankle.
    if( m_LerpRightAnkle.GetLerpEnabled() )
    {
        LerpVector( pOutputSkeleton->SkeletonPositions[NUI_SKELETON_POSITION_ANKLE_RIGHT], pFilteredPositions[NUI_SKELETON_POSITION_ANKLE_RIGHT], m_LerpRightAnkle.GetSmoothValue() );
        pOutputSkeleton->eSkeletonPositionTrackingState[NUI_SKELETON_POSITION_ANKLE_LEFT] = NUI_SKELETON_POSITION_TRACKED;
        bSkeletonUpdated = TRUE;
    }

    // Apply lerp to the right foot.
    if( m_LerpRightFoot.GetLerpEnabled() )
    {
        LerpVector( pOutputSkeleton->SkeletonPositions[NUI_SKELETON_POSITION_FOOT_RIGHT], pFilteredPositions[NUI_SKELETON_POSITION_FOOT_RIGHT], m_LerpRightFoot.GetSmoothValue() );
        bSkeletonUpdated = TRUE;
    }

    return bSkeletonUpdated;
}

VOID FilterClippedLegs::ResetFilterState()
{
#if FILTER_DEBUG
    ZeroMemory( m_FilterDebug, sizeof(m_FilterDebug) );
#endif
    
    // set up a really floaty double exponential filter - we want maximum smoothness
    m_FilterDoubleExp.Init( 0.5f, 0.3f, 1.0f, 1.0f, 1.0f );

    m_LerpLeftKnee.Reset();
    m_LerpLeftAnkle.Reset();
    m_LerpLeftFoot.Reset();
    m_LerpRightKnee.Reset();
    m_LerpRightAnkle.Reset();
    m_LerpRightFoot.Reset();
}

VOID FilterClippedLegs::DebugRenderUI( IFilterDebugRenderer* pDebugRenderer )
{
#if FILTER_DEBUG
    DWORD DebugColor = 0xFF00FFFF;
    for( DWORD i = 0; i < ARRAYSIZE( m_FilterDebug ); i += 4 )
    {
        DWORD Count = min( 4, ARRAYSIZE(m_FilterDebug) - i );
        switch( Count )
        {
        case 4:
            pDebugRenderer->DrawTextLine( 1, 1, DebugColor, 0, TRUE, L"%0.2f %0.2f %0.2f %0.2f", m_FilterDebug[i], m_FilterDebug[i+1], m_FilterDebug[i+2], m_FilterDebug[i+3] );
            break;
        case 3:
            pDebugRenderer->DrawTextLine( 1, 1, DebugColor, 0, TRUE, L"%0.2f %0.2f %0.2f", m_FilterDebug[i], m_FilterDebug[i+1], m_FilterDebug[i+2] );
            break;
        case 2:
            pDebugRenderer->DrawTextLine( 1, 1, DebugColor, 0, TRUE, L"%0.2f %0.2f", m_FilterDebug[i], m_FilterDebug[i+1] );
            break;
        case 1:
            pDebugRenderer->DrawTextLine( 1, 1, DebugColor, 0, TRUE, L"%0.2f", m_FilterDebug[i] );
            break;
        }
    }
#endif
}

//--------------------------------------------------------------------------------------

VOID FilterHipHeight::Initialize()
{
    ResetFilterState();
}

BOOL FilterHipHeight::FilterSkeleton( SkeletonFilterContext* pContext, const NUI_SKELETON_DATA* pInputSkeleton, NUI_SKELETON_DATA* pOutputSkeleton, const FLOAT fDeltaNuiTime )
{
    static const XMVECTOR vHalf = XMVectorReplicate( 0.5f );
    static const XMVECTOR vOne = XMVectorReplicate( 1.0f );

    if( pInputSkeleton->eSkeletonPositionTrackingState[ NUI_SKELETON_POSITION_SHOULDER_CENTER ] == NUI_SKELETON_POSITION_NOT_TRACKED ||
        pInputSkeleton->eSkeletonPositionTrackingState[ NUI_SKELETON_POSITION_SPINE ]           == NUI_SKELETON_POSITION_NOT_TRACKED ||
        pInputSkeleton->eSkeletonPositionTrackingState[ NUI_SKELETON_POSITION_HIP_RIGHT ]       == NUI_SKELETON_POSITION_NOT_TRACKED ||
        pInputSkeleton->eSkeletonPositionTrackingState[ NUI_SKELETON_POSITION_KNEE_RIGHT ]      == NUI_SKELETON_POSITION_NOT_TRACKED ||
        pInputSkeleton->eSkeletonPositionTrackingState[ NUI_SKELETON_POSITION_HIP_LEFT ]        == NUI_SKELETON_POSITION_NOT_TRACKED ||
        pInputSkeleton->eSkeletonPositionTrackingState[ NUI_SKELETON_POSITION_KNEE_LEFT ]       == NUI_SKELETON_POSITION_NOT_TRACKED ||
        pInputSkeleton->eSkeletonPositionTrackingState[ NUI_SKELETON_POSITION_ANKLE_LEFT ]      == NUI_SKELETON_POSITION_NOT_TRACKED    )
    {
        return FALSE;
    }

    const SkeletonCommonProcessing* pProcess = pContext->pProcess;
    XMVECTOR vTorsoLength = XMVector3Length( pProcess->Position.ShoulderCenter - pProcess->Position.Spine );
    m_vTorsoLength = vTorsoLength;
    m_vFemurLength = pProcess->AverageFemurLength;
    m_vHTorsoLength = pProcess->History.TorsoLength;
    m_vHFemurLength = ( pProcess->History.LeftFemurLength + pProcess->History.RightFemurLength ) * vHalf;

    // compute ratio between torso length and femur length
    XMVECTOR vFemurToTorsoRatio = pProcess->AverageFemurLength / vTorsoLength;
    m_vCurrentFemurTorso = vFemurToTorsoRatio;
    
    XMVECTOR vLeftLegLength = XMVector3Length( pProcess->Position.AnkleLeft - pProcess->Position.HipCenter );
    XMVECTOR vRightLegLength = XMVector3Length( pProcess->Position.AnkleRight - pProcess->Position.HipCenter );
    XMVECTOR vLegLength = ( vLeftLegLength + vRightLegLength ) * vHalf;
    m_vLegLength = vLegLength;
    XMVECTOR vLegToTorsoRatio = vLeftLegLength / vTorsoLength;
    m_vCurrentLegTorso = vLegToTorsoRatio;

    m_vHipHeight = XMVector3Dot( pProcess->Position.HipCenter - pProcess->Position.HipLeft, pProcess->BodyUp );

    // if the arms are at a rest pose at the sides, accumulate current ratio into the running historical average
    if( pProcess->LeftArmArrangement == SkeletonCommonProcessing::AA_Down &&
        pProcess->RightArmArrangement == SkeletonCommonProcessing::AA_Down )
    {
        XMVECTOR vDeltaTime = XMVectorReplicate( fDeltaNuiTime );
        const XMVECTOR vTimeWindow = XMVectorReplicate( 8.0f );
        SkeletonCommonProcessing::AccumulateHistoricalSample( m_vFemurToTorsoRatio, vFemurToTorsoRatio, vDeltaTime, vTimeWindow );
        SkeletonCommonProcessing::AccumulateHistoricalSample( m_vLegToTorsoRatio, vLegToTorsoRatio, vDeltaTime, vTimeWindow );
    }

    // see how far off from normal the ratio is
    XMVECTOR vAdjustAmount = vFemurToTorsoRatio / m_vFemurToTorsoRatio;
    XMVECTOR vTorsoLengthIncrease = ( pProcess->AverageFemurLength - m_vFemurToTorsoRatio * vTorsoLength ) / ( m_vFemurToTorsoRatio + vOne );
    if( pProcess->Tracked.AnkleLeft && pProcess->Tracked.AnkleRight )
    {
        vAdjustAmount = vLegToTorsoRatio / m_vLegToTorsoRatio;
        vTorsoLengthIncrease = ( vLegLength - m_vLegToTorsoRatio * vTorsoLength ) / ( m_vLegToTorsoRatio + vOne );
    }
    m_fAdjustPercentage = XMVectorGetX( vAdjustAmount );

    // Compute the amount of length to add to the torso and subtract from the legs
    m_fAdjustLength = XMVectorGetX( vTorsoLengthIncrease );

    // apply the entire torso length adjustment to the hips and spine, which will shorten the femurs
    pOutputSkeleton->SkeletonPositions[NUI_SKELETON_POSITION_HIP_CENTER] -= vTorsoLengthIncrease * pProcess->BodyUp;
    pOutputSkeleton->SkeletonPositions[NUI_SKELETON_POSITION_HIP_LEFT] -= vTorsoLengthIncrease * pProcess->BodyUp;
    pOutputSkeleton->SkeletonPositions[NUI_SKELETON_POSITION_HIP_RIGHT] -= vTorsoLengthIncrease * pProcess->BodyUp;
    pOutputSkeleton->SkeletonPositions[NUI_SKELETON_POSITION_SPINE] -= vTorsoLengthIncrease * pProcess->BodyUp;

    // apply half of the torso length adjustment to the lower legs; they grow with the femurs at the same rate
    XMVECTOR vKneeShiftAmount = vTorsoLengthIncrease * XMVectorReplicate( 0.5f );
    pOutputSkeleton->SkeletonPositions[NUI_SKELETON_POSITION_KNEE_LEFT] -= vKneeShiftAmount * pProcess->BodyUp;
    pOutputSkeleton->SkeletonPositions[NUI_SKELETON_POSITION_KNEE_RIGHT] -= vKneeShiftAmount * pProcess->BodyUp;

    return TRUE;
}

VOID FilterHipHeight::ResetFilterState()
{
    m_vFemurToTorsoRatio = XMVectorZero();
    m_vLegToTorsoRatio = XMVectorZero();
    m_fAdjustLength = 0;
    m_fAdjustPercentage = 0;
    m_vCurrentFemurTorso = XMVectorZero();
    m_vCurrentLegTorso = XMVectorZero();
    m_vHFemurLength = XMVectorZero();
    m_vHTorsoLength = XMVectorZero();
    m_vHipHeight = XMVectorZero();
    m_vTorsoLength = XMVectorZero();
    m_vFemurLength = XMVectorZero();
    m_vLegLength = XMVectorZero();
}

VOID FilterHipHeight::DebugRenderUI( IFilterDebugRenderer* pDebugRenderer )
{
    pDebugRenderer->DrawTextLine( 1.0f, 1.0f, 0xFF00FF80, 0, TRUE, L"Leg: %0.3f LegTorso: %0.3f %0.3f", XMVectorGetX( m_vLegLength ), XMVectorGetX( m_vLegToTorsoRatio ), XMVectorGetX( m_vCurrentLegTorso ) );
    pDebugRenderer->DrawTextLine( 1.0f, 1.0f, 0xFF00FF80, 0, TRUE, L"FemurTorso: %0.3f %0.3f HH: %0.3f", XMVectorGetX( m_vFemurToTorsoRatio ), XMVectorGetX( m_vCurrentFemurTorso ), XMVectorGetX( m_vHipHeight ) );
    pDebugRenderer->DrawTextLine( 1.0f, 1.0f, 0xFFFF0000, 0, TRUE, L"Leg C/H: %0.3f Torso C/H: %0.3f", XMVectorGetX( m_vCurrentLegTorso / m_vLegToTorsoRatio ), XMVectorGetX( m_vCurrentFemurTorso / m_vFemurToTorsoRatio ) );
    pDebugRenderer->DrawTextLine( 1.0f, 1.0f, 0xFF00FF80, 0, TRUE, L"Femur: %0.3f Torso: %0.3f %0.3f", XMVectorGetX( m_vFemurLength ), XMVectorGetX( m_vTorsoLength ) );
    pDebugRenderer->DrawTextLine( 1.0f, 1.0f, 0xFF00FF80, 0, TRUE, L"HFemur: %0.3f HTorso: %0.3f", XMVectorGetX( m_vHFemurLength ), XMVectorGetX( m_vHTorsoLength ) );
    pDebugRenderer->DrawTextLine( 1.0f, 1.0f, 0xFF00FF80, 0, TRUE, L"Adjust %% %0.3f Distance %0.3f", m_fAdjustPercentage, m_fAdjustLength );
}
