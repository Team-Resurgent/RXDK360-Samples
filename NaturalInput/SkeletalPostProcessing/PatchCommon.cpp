//--------------------------------------------------------------------------------------
// PatchCommon.cpp
//
// Contains common code and infrastructure for skeleton postprocessing, including a
// skeleton filter stack, skeleton filter interface, depth image processing code,
// and several more classes & methods useful for implementing skeleton filters.
// 
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include <algorithm>
#include "PatchCommon.h"

static const XMVECTOR vHalf = XMVectorReplicate( 0.5f );

//--------------------------------------------------------------------------------------
// Name: SkeletonCommonProcessing constructor
//--------------------------------------------------------------------------------------
SkeletonCommonProcessing::SkeletonCommonProcessing()
{
    Reset();
}

//--------------------------------------------------------------------------------------
// Name: SkeletonCommonProcessing::Reset()
// Desc: Resets all member variables to their default values.
//--------------------------------------------------------------------------------------
VOID SkeletonCommonProcessing::Reset()
{
    SkeletonIndex = (DWORD)-1;
    m_dwLastFrameProcessed = 0;
    ZeroMemory( &Tracked, sizeof(Tracked) );
    ZeroMemory( &TrackedOrInferred, sizeof(TrackedOrInferred) );
    ZeroMemory( &Position, sizeof(Position) );
    ZeroMemory( &History, sizeof(History) );
    LeftArmArrangement = AA_Down;
    RightArmArrangement = AA_Down;
    BodyBasisValid = FALSE;
}

//--------------------------------------------------------------------------------------
// Name: SkeletonCommonProcessing::UpdateFromSkeleton()
// Desc: Extracts data from a NUI_SKELETON_DATA struct and computes many derivative values
// that are useful to skeleton filters.  This method will be executed once per active filter per
// frame.
//--------------------------------------------------------------------------------------
VOID SkeletonCommonProcessing::UpdateFromSkeleton( const NUI_SKELETON_DATA* pSkeleton )
{
    if( SkeletonIndex == (DWORD)-1 )
    {
        return;
    }

    BOOL bWholeSkeletonTracked = ( pSkeleton->eTrackingState == NUI_SKELETON_TRACKED );

    // copy skeleton data into easy-to-use structs
    for( DWORD i = 0; i < NUI_SKELETON_POSITION_COUNT; ++i )
    {
        ((BOOL*)&Tracked)[i] = IsTracked( pSkeleton, (NUI_SKELETON_POSITION_INDEX)i ) & bWholeSkeletonTracked;
        ((BOOL*)&TrackedOrInferred)[i] = IsTrackedOrInferred( pSkeleton, (NUI_SKELETON_POSITION_INDEX)i ) & bWholeSkeletonTracked;
        ((XMVECTOR*)&Position)[i] = pSkeleton->SkeletonPositions[i];
    }

    XMVECTOR vTorsoUpVector = XMVectorZero();
    if( Tracked.ShoulderCenter && Tracked.HipCenter )
    {
        vTorsoUpVector = XMVector3Normalize( Position.ShoulderCenter - Position.HipCenter );
    }

    // compute femurs
    LeftFemur = Position.HipCenter - Position.KneeLeft;
    LeftFemurLength = XMVector3Length( LeftFemur );
    RightFemur = Position.HipCenter - Position.KneeRight;
    RightFemurLength = XMVector3Length( RightFemur );

    // compute average femur length and direction - used for body basis
    if( Tracked.KneeLeft && Tracked.KneeRight )
    {
        AverageFemurLength = ( LeftFemurLength + RightFemurLength ) * vHalf;
        AverageFemurVector = XMVector3Normalize( LeftFemur + RightFemur );
    }
    else if( Tracked.KneeLeft )
    {
        AverageFemurLength = LeftFemurLength;
        AverageFemurVector = XMVector3Normalize( LeftFemur );
    }
    else if( Tracked.KneeRight )
    {
        AverageFemurLength = RightFemurLength;
        AverageFemurVector = XMVector3Normalize( RightFemur );
    }
    else
    {
        AverageFemurLength = XMVectorZero();
        AverageFemurVector = XMVectorZero();
    }

    // compute lower legs
    LeftLowerLeg = Position.HipCenter - Position.KneeLeft;
    LeftLowerLegLength = XMVector3Length( LeftLowerLeg );
    RightLowerLeg = Position.HipCenter - Position.KneeRight;
    RightLowerLegLength = XMVector3Length( RightLowerLeg );

    // compute clavicles
    LeftClavicle = Position.ShoulderLeft - Position.ShoulderCenter;
    LeftClavicleLength = XMVector3Length( LeftClavicle );
    RightClavicle = Position.ShoulderRight - Position.ShoulderCenter;
    RightClavicleLength = XMVector3Length( RightClavicle );

    // build a body orthonormal basis system
    BodyBasisValid = FALSE;
    BodyUp = AverageFemurVector + vTorsoUpVector;
    if( XMVectorGetX( XMVector3LengthSq( BodyUp ) ) > 0 )
    {
        BodyUp = XMVector3Normalize( BodyUp );
        BodyRight = XMVector3Normalize( Position.HipLeft - Position.HipCenter );
        if( XMVectorGetX( XMVector3LengthSq( BodyRight ) ) > 0 )
        {
            BodyForward = XMVector3Normalize( XMVector3Cross( BodyUp, BodyRight ) );
            BodyRight = XMVector3Cross( BodyUp, BodyForward );
            BodyBasisValid = TRUE;
        }
    }
    else
    {
        // not enough data to generate a valid basis
        BodyForward = XMVectorZero();
        BodyRight = XMVectorZero();
        BodyUp = XMVectorZero();
    }

    // compute dot products from body up and vectors between shoulder center and arm joints
    LeftArmDotProducts[ADP_Shoulder] = XMVectorGetX( XMVector3Dot( XMVector3Normalize( Position.ShoulderLeft - Position.ShoulderCenter ), BodyUp ) );
    LeftArmDotProducts[ADP_Elbow] = XMVectorGetX( XMVector3Dot( XMVector3Normalize( Position.ElbowLeft - Position.ShoulderCenter ), BodyUp ) );
    LeftArmDotProducts[ADP_Wrist] = XMVectorGetX( XMVector3Dot( XMVector3Normalize( Position.WristLeft - Position.ShoulderCenter ), BodyUp ) );

    RightArmDotProducts[ADP_Shoulder] = XMVectorGetX( XMVector3Dot( XMVector3Normalize( Position.ShoulderRight - Position.ShoulderCenter ), BodyUp ) );
    RightArmDotProducts[ADP_Elbow] = XMVectorGetX( XMVector3Dot( XMVector3Normalize( Position.ElbowRight - Position.ShoulderCenter ), BodyUp ) );
    RightArmDotProducts[ADP_Wrist] = XMVectorGetX( XMVector3Dot( XMVector3Normalize( Position.WristRight - Position.ShoulderCenter ), BodyUp ) );

    // determine left arm arrangement from dot products
    LeftArmArrangement = AA_Down;
    if( LeftArmDotProducts[ADP_Elbow] >= 0 )
    {
        if( LeftArmDotProducts[ADP_Wrist] >= LeftArmDotProducts[ADP_Elbow] )
        {
            LeftArmArrangement = AA_RaisedUp;
        }
        else
        {
            LeftArmArrangement = AA_ElbowHighWristLow;
        }
    }
    else if (LeftArmDotProducts[ADP_Elbow] < 0 )
    {
        if( LeftArmDotProducts[ADP_Wrist] <= LeftArmDotProducts[ADP_Elbow] )
        {
            LeftArmArrangement = AA_Down;
        }
        else
        {
            LeftArmArrangement = AA_ElbowLowWristHigh;
        }
    }

    // determine right arm arrangement from dot products
    RightArmArrangement = AA_Down;
    if( RightArmDotProducts[ADP_Elbow] >= 0 )
    {
        if( RightArmDotProducts[ADP_Wrist] >= RightArmDotProducts[ADP_Elbow] )
        {
            RightArmArrangement = AA_RaisedUp;
        }
        else
        {
            RightArmArrangement = AA_ElbowHighWristLow;
        }
    }
    else if (RightArmDotProducts[ADP_Elbow] < 0 )
    {
        if( RightArmDotProducts[ADP_Wrist] <= RightArmDotProducts[ADP_Elbow] )
        {
            RightArmArrangement = AA_Down;
        }
        else
        {
            RightArmArrangement = AA_ElbowLowWristHigh;
        }
    }
}

//--------------------------------------------------------------------------------------
// Name: SkeletonCommonProcessing::UpdateHistoricalValues
// Desc: Updates all time-dependent historical values.  This should only be executed once 
// per NUI frame, using the raw skeleton positions before filtering.
//--------------------------------------------------------------------------------------
VOID SkeletonCommonProcessing::UpdateHistoricalValues( const XMVECTOR vDeltaNuiTime )
{
    if( SkeletonIndex == (DWORD)-1 )
    {
        return;
    }

    if( Tracked.HipCenter && BodyBasisValid )
    {
        static const XMVECTOR NormalTimeWindow = XMVectorReplicate( 4.0f );

        if( Tracked.HipLeft && Tracked.KneeLeft )
        {
            AccumulateHistoricalSample( History.LeftFemurLength, LeftFemurLength, vDeltaNuiTime, NormalTimeWindow );
        }
        if( Tracked.HipRight && Tracked.KneeRight )
        {
            AccumulateHistoricalSample( History.RightFemurLength, RightFemurLength, vDeltaNuiTime, NormalTimeWindow );
        }
        if( Tracked.AnkleLeft && Tracked.KneeLeft )
        {
            AccumulateHistoricalSample( History.LeftLowerLegLength, LeftLowerLegLength, vDeltaNuiTime, NormalTimeWindow );
        }
        if( Tracked.AnkleRight && Tracked.KneeRight )
        {
            AccumulateHistoricalSample( History.RightLowerLegLength, RightLowerLegLength, vDeltaNuiTime, NormalTimeWindow );
        }
        if( Tracked.ShoulderCenter && Tracked.Spine )
        {
            AccumulateHistoricalSample( History.TorsoLength, XMVector3Length( Position.Spine - Position.ShoulderCenter ), vDeltaNuiTime, NormalTimeWindow );
        }
        if( Tracked.Spine )
        {
            AccumulateHistoricalSample( History.SpineLength, XMVector3Length( Position.Spine - Position.HipCenter ), vDeltaNuiTime, NormalTimeWindow );
        }
        if( Tracked.ShoulderLeft && Tracked.ElbowLeft )
        {
            AccumulateHistoricalSample( History.LeftUpperArmLength, XMVector3Length( Position.ElbowLeft - Position.ShoulderLeft ), vDeltaNuiTime, NormalTimeWindow );
        }
        if( Tracked.ShoulderRight && Tracked.ElbowRight )
        {
            AccumulateHistoricalSample( History.RightUpperArmLength, XMVector3Length( Position.ElbowRight - Position.ShoulderRight ), vDeltaNuiTime, NormalTimeWindow );
        }
        if( Tracked.ElbowLeft && Tracked.WristLeft )
        {
            AccumulateHistoricalSample( History.LeftLowerArmLength, XMVector3Length( Position.WristLeft - Position.ElbowLeft ), vDeltaNuiTime, NormalTimeWindow );
        }
        if( Tracked.ElbowRight && Tracked.WristRight )
        {
            AccumulateHistoricalSample( History.RightLowerArmLength, XMVector3Length( Position.WristRight - Position.ElbowRight ), vDeltaNuiTime, NormalTimeWindow );
        }
        if( ( Tracked.WristLeft && Tracked.HandLeft ) || ( Tracked.WristRight && Tracked.HandRight ) )
        {
            XMVECTOR AverageWristLength = XMVector3Length( Position.HandLeft - Position.WristLeft ) + XMVector3Length( Position.HandRight - Position.WristRight );
            AverageWristLength *= vHalf;
            AccumulateHistoricalSample( History.WristLength, AverageWristLength, vDeltaNuiTime, NormalTimeWindow );
        }
        if( Tracked.Head && Tracked.ShoulderCenter )
        {
            AccumulateHistoricalSample( History.NeckLength, XMVector3Length( Position.Head - Position.ShoulderCenter ), vDeltaNuiTime, NormalTimeWindow );
        }
    }
}

//--------------------------------------------------------------------------------------
// Name: SkeletonCommonProcessing::DebugRenderUI
// Desc: Draws some of the common data to the screen.
//--------------------------------------------------------------------------------------
VOID SkeletonCommonProcessing::DebugRenderUI( IFilterDebugRenderer* pRenderer )
{
    if( SkeletonIndex == (DWORD)-1 )
    {
        return;
    }

    const FLOAT fFontWidth = 0.6f;
    const FLOAT fFontHeight = 0.8f;

    pRenderer->DrawTextLine( fFontWidth, fFontHeight, 0xFFFFFFFF, 0, TRUE, L"Skeleton %d:", SkeletonIndex );
    pRenderer->DrawTextLine( fFontWidth, fFontHeight, 0xFFFFFFFF, 0, TRUE, L"Basis %d Arms L %d R %d", BodyBasisValid, LeftArmArrangement, RightArmArrangement );
    pRenderer->DrawTextLine( fFontWidth, fFontHeight, 0xFFFFFFFF, 0, TRUE, L"Torso %0.2f Spine %0.2f", 
        XMVectorGetX( History.TorsoLength ), 
        XMVectorGetX( History.SpineLength ) );
    pRenderer->DrawTextLine( fFontWidth, fFontHeight, 0xFFFFFFFF, 0, TRUE, L"Neck %0.2f Wrist %0.2f", 
        XMVectorGetX( History.NeckLength ), 
        XMVectorGetX( History.WristLength ) );
    pRenderer->DrawTextLine( fFontWidth, fFontHeight, 0xFFFFFFFF, 0, TRUE, L"Femur L %0.2f R %0.2f", 
        XMVectorGetX( History.LeftFemurLength ), 
        XMVectorGetX( History.RightFemurLength ) );
    pRenderer->DrawTextLine( fFontWidth, fFontHeight, 0xFFFFFFFF, 0, TRUE, L"LLegs L %0.2f R %0.2f", 
        XMVectorGetX( History.LeftLowerLegLength ), 
        XMVectorGetX( History.RightLowerLegLength ) );
    pRenderer->DrawTextLine( fFontWidth, fFontHeight, 0xFFFFFFFF, 0, TRUE, L"UArms L %0.2f R %0.2f", 
        XMVectorGetX( History.LeftUpperArmLength ), 
        XMVectorGetX( History.RightUpperArmLength ) );
    pRenderer->DrawTextLine( fFontWidth, fFontHeight, 0xFFFFFFFF, 0, TRUE, L"LArms L %0.2f R %0.2f", 
        XMVectorGetX( History.LeftLowerArmLength ), 
        XMVectorGetX( History.RightLowerArmLength ) );
}

//--------------------------------------------------------------------------------------
// Name: SkeletonCommonProcessing::AccumulateHistoricalSample
// Desc: Implements a time-based lerp of the incoming value in Sample with the existing
// value in Destination.  TimeWindow controls the speed of the lerp.
//--------------------------------------------------------------------------------------
VOID SkeletonCommonProcessing::AccumulateHistoricalSample( XMVECTOR& vDestination, const XMVECTOR vSample, const XMVECTOR vDeltaTime, const XMVECTOR vTimeWindow )
{
    if( XMVectorGetX( vDestination ) <= 0.01f )
    {
        vDestination = vSample;
        return;
    }

    XMVECTOR vLerpValue = vDeltaTime / vTimeWindow;

    vDestination = XMVectorLerpV( vSample, vDestination, vLerpValue );
}

//--------------------------------------------------------------------------------------
// Name: FrameFilterStack constructor
//--------------------------------------------------------------------------------------
FrameFilterStack::FrameFilterStack()
{
    Initialize();
}

//--------------------------------------------------------------------------------------
// Name: FrameFilterStack::Initialize()
// Desc: Initializes all member data in the filter stack.
//--------------------------------------------------------------------------------------
VOID FrameFilterStack::Initialize()
{
    ZeroMemory( m_SkeletonFrame, sizeof(m_SkeletonFrame) );
    m_pOutputSkeletonFrame = &m_SkeletonFrame[0];
    m_Filters.clear();
    for( DWORD i = 0; i < ARRAYSIZE(m_CommonProcessing); ++i )
    {
        m_CommonProcessing[i].Reset();
    }
    m_bEnabled = TRUE;
    ZeroMemory( &m_FilterContext, sizeof(m_FilterContext) );
    m_FilterContext.pDepthProcess = &m_DepthProcessing;
}

//--------------------------------------------------------------------------------------
// Name: FrameFilterStack::AddFilter()
// Desc: Initializes the filter and adds it to the stack.
//--------------------------------------------------------------------------------------
VOID FrameFilterStack::AddFilter( IFrameFilter* pFilter, DWORD Mask )
{ 
    pFilter->Initialize();
    FilterEntry Entry = { Mask, pFilter }; 
    m_Filters.push_back( Entry ); 
}

//--------------------------------------------------------------------------------------
// Name: FrameFilterStack::FilterDepthTexture
// Desc: Passes the depth texture to the depth processing module.
//--------------------------------------------------------------------------------------
VOID FrameFilterStack::FilterDepthTexture( IDirect3DTexture9* pDepthTexture, IDirect3DTexture9* pSmallDepthTexture )
{
    m_DepthProcessing.CopyDepthTexture( pDepthTexture, pSmallDepthTexture );
}

//--------------------------------------------------------------------------------------
// Name: FrameFilterStack::FilterSkeleton
// Desc: Performs post processing on a single skeleton in a NUI_SKELETON_FRAME struct.
// Active filters (selected using the Mask parameter) are executed in sequence to
// produce a fully processed skeleton, which is returned from this function.
//--------------------------------------------------------------------------------------
const NUI_SKELETON_FRAME* FrameFilterStack::FilterFrame( const NUI_SKELETON_FRAME* pInputSkeletonFrame, const FLOAT fDeltaNuiTime, const DWORD Mask )
{
    const DWORD dwFilterCount = m_Filters.size();
    if( dwFilterCount == 0 || !m_bEnabled )
    {
        m_pOutputSkeletonFrame = NULL;
        return pInputSkeletonFrame;
    }

    DWORD CurrentCommonIndex = 0;
    for( DWORD i = 0; i < ARRAYSIZE(pInputSkeletonFrame->SkeletonData); ++i )
    {
        BOOL SkeletonTracked = ( pInputSkeletonFrame->SkeletonData[i].eTrackingState == NUI_SKELETON_TRACKED );
        if( SkeletonTracked )
        {
            assert( CurrentCommonIndex < ARRAYSIZE(m_CommonProcessing) );
            SkeletonCommonProcessing* pCurrentProcessingModule = &m_CommonProcessing[CurrentCommonIndex++];
            if( m_FilterContext.pProcess[i] != pCurrentProcessingModule )
            {
                pCurrentProcessingModule->Reset();
            }
            m_FilterContext.pProcess[i] = pCurrentProcessingModule;
            pCurrentProcessingModule->SkeletonIndex = i;
            pCurrentProcessingModule->UpdateFromSkeleton( &pInputSkeletonFrame->SkeletonData[i] );
            pCurrentProcessingModule->UpdateHistoricalValues( XMVectorReplicate( fDeltaNuiTime ) );
        }
        else
        {
            m_FilterContext.pProcess[i] = NULL;
        }
    }

    DWORD dwSourceIndex = 0;
    NUI_SKELETON_FRAME* pSourceFrame = &m_SkeletonFrame[dwSourceIndex];
    NUI_SKELETON_FRAME* pDestFrame = &m_SkeletonFrame[(dwSourceIndex + 1) % ARRAYSIZE(m_SkeletonFrame)];

    m_FilterContext.fDeltaTime = fDeltaNuiTime;

    // copy input to src
    XMemCpy( pSourceFrame, pInputSkeletonFrame, sizeof(NUI_SKELETON_FRAME) );
    // copy src to dest
    XMemCpy( pDestFrame, pSourceFrame, sizeof(NUI_SKELETON_FRAME) );

    for( DWORD i = 0; i < dwFilterCount; ++i )
    {
        if( ( m_Filters[i].Mask & Mask ) == 0 )
        {
            continue;
        }
        IFrameFilter* pFilter = m_Filters[i].pFilter;
        
        BOOL bDestinationChanged = pFilter->FilterFrame( &m_FilterContext, pSourceFrame, pDestFrame );

        if( bDestinationChanged )
        {
            // swap source and dest pointers
            dwSourceIndex = ( dwSourceIndex + 1 ) % ARRAYSIZE( m_SkeletonFrame );
            pSourceFrame = &m_SkeletonFrame[dwSourceIndex];
            pDestFrame = &m_SkeletonFrame[(dwSourceIndex + 1) % ARRAYSIZE(m_SkeletonFrame)];

            // copy src to dest
            XMemCpy( pDestFrame, pSourceFrame, sizeof(NUI_SKELETON_FRAME) );

            // update common processing
            for( DWORD j = 0; j < ARRAYSIZE(m_FilterContext.pProcess); ++j )
            {
                if( m_FilterContext.pProcess[j] != NULL )
                {
                    m_FilterContext.pProcess[j]->UpdateFromSkeleton( &pSourceFrame->SkeletonData[j] );
                }
            }
        }
    }

    m_pOutputSkeletonFrame = pSourceFrame;
    return m_pOutputSkeletonFrame;
}

//--------------------------------------------------------------------------------------
// Name: FrameFilterStack::ResetFilterState()
// Desc: Instructs each filter to reset its internal state.  This is used when the title
// knows that the skeleton has changed significantly, such as switching to a different
// player, or a different filter set is being used.
//--------------------------------------------------------------------------------------
VOID FrameFilterStack::ResetFilterState()
{
    for( DWORD i = 0; i < ARRAYSIZE(m_CommonProcessing); ++i )
    {
        m_CommonProcessing[i].Reset();
    }
    const DWORD dwFilterCount = m_Filters.size();
    for( DWORD i = 0; i < dwFilterCount; ++i )
    {
        m_Filters[i].pFilter->ResetFilterState();
    }
}

//--------------------------------------------------------------------------------------
// Name: FrameFilterStack::DebugRenderUI
// Desc: Instructs each active filter to render its debug UI.
//--------------------------------------------------------------------------------------
VOID FrameFilterStack::DebugRenderUI( IFilterDebugRenderer* pDebugRenderer, const DWORD Mask )
{
    if( m_Filters.size() == 0 || !m_bEnabled )
    {
        return;
    }

    pDebugRenderer->StartRendering();

    for( DWORD i = 0; i < ARRAYSIZE(m_CommonProcessing); ++i )
    {
        m_CommonProcessing[i].DebugRenderUI( pDebugRenderer );
    }

    const DWORD dwFilterCount = m_Filters.size();
    for( DWORD i = 0; i < dwFilterCount; ++i )
    {
        if( ( m_Filters[i].Mask & Mask ) == 0 )
            continue;

        m_Filters[i].pFilter->DebugRenderUI( pDebugRenderer );
    }

    pDebugRenderer->EndRendering();
}

//--------------------------------------------------------------------------------------
// Name: DepthImageProcessing::CopyDepthTexture()
// Desc: Locks and copies texture contents from a depth texture to cached memory.
//--------------------------------------------------------------------------------------
VOID DepthImageProcessing::CopyDepthTexture( IDirect3DTexture9* pDepthTexture, IDirect3DTexture9* pSmallDepthTexture )
{
    D3DLOCKED_RECT LockRect = { NULL, 0 };
    pDepthTexture->LockRect( 0, &LockRect, NULL, D3DLOCK_READONLY );

    if( LockRect.pBits != NULL )
    {
        assert( (DWORD)LockRect.Pitch >= ( m_dwImageWidth * sizeof(USHORT) ) );
        const BYTE* pSrc = (const BYTE*)LockRect.pBits;
        BYTE* pDest = (BYTE*)m_pDepthImageCopy;
        for( DWORD i = 0; i < m_dwImageHeight; ++i )
        {
            XMemCpy( pDest, pSrc, m_dwImageWidth * sizeof(USHORT) );
            pSrc += LockRect.Pitch;
            pDest += m_dwImageWidth * sizeof(USHORT);
        }
    }

    pDepthTexture->UnlockRect( 0 );

    pSmallDepthTexture->LockRect( 0, &LockRect, NULL, D3DLOCK_READONLY );
    
    if( LockRect.pBits != NULL )
    {
        const BYTE* pSrc = (const BYTE*)LockRect.pBits;
        BYTE* pDest = (BYTE*)m_pDepthImageCopy;
        for( DWORD i = 0; i < 60; ++i )
        {
            memcpy( pDest, pSrc, 80 * sizeof(USHORT) );
            pSrc += LockRect.Pitch;
            pDest += 80 * sizeof(USHORT);
        }
    }

    pSmallDepthTexture->UnlockRect( 0 );
}

//--------------------------------------------------------------------------------------
// Name: ConvertDepth()
// Desc: Converts a NUI depth image sample to a linear depth value, by shifting off the
// lower 3 bits.
//--------------------------------------------------------------------------------------
inline FLOAT ConvertDepth( USHORT DepthImageSample )
{
    return (FLOAT)( DepthImageSample >> 3 );
}

//--------------------------------------------------------------------------------------
// Name: DepthImageProcessing::EvaluateLine
// Desc: Walks a 3D line in the depth image from BeginPos to EndPos, capturing data
// along the way.  Analysis is done on the data and returned through the LineEvaluation
// struct.
//--------------------------------------------------------------------------------------
BOOL DepthImageProcessing::EvaluateLine( const XMVECTOR vBeginPos, const XMVECTOR vEndPos, LineEvaluation* pEvaluation, DWORD dwSampleCount )
{
    if( m_pDepthImageCopy == NULL || pEvaluation == NULL )
    {
        return FALSE;
    }

    ZeroMemory( pEvaluation, sizeof(LineEvaluation) );

    if( dwSampleCount == 0 )
    {
        // TODO: compute ideal sample count based on BeginPos/EndPos
        dwSampleCount = 8;
    }

    static USHORT g_pScratchPad[16];

    USHORT* pSamples = NULL;
    if( dwSampleCount <= ARRAYSIZE( g_pScratchPad ) )
    {
        pSamples = g_pScratchPad;
    }
    else
    {
        pSamples = new USHORT[dwSampleCount];
    }

    CaptureLine( vBeginPos, vEndPos, pSamples, dwSampleCount );

    // Look for consistent player ID
    pEvaluation->m_bConsistentPlayerID = TRUE;
    USHORT ReferencePlayerIndex = pSamples[0] & 0x7;
    for( DWORD i = 0; i < dwSampleCount; ++i )
    {
        USHORT PlayerIndex = pSamples[i] & 0x7;
        if( PlayerIndex != ReferencePlayerIndex )
        {
            pEvaluation->m_bConsistentPlayerID = FALSE;
        }
    }

    // Look for linear depth progression
    FLOAT fBeginDepth = ConvertDepth( pSamples[0] );
    FLOAT fEndDepth = ConvertDepth( pSamples[dwSampleCount - 1] );

    pEvaluation->m_fBeginDepth = fBeginDepth;
    pEvaluation->m_fEndDepth = fEndDepth;
    pEvaluation->m_fMaxDepth = max( fBeginDepth, fEndDepth );
    pEvaluation->m_fMinDepth = min( fBeginDepth, fEndDepth );

    if( fBeginDepth > 0 && fEndDepth > 0 )
    {
        pEvaluation->m_bConsistentLinearDepth = TRUE;
        FLOAT IdealDepthDeltaPerSample = (fEndDepth - fBeginDepth) / (FLOAT)dwSampleCount;
        FLOAT CurrentIdealDepth = fBeginDepth;

        // depth tolerance from linear, in millimeters
        const FLOAT LinearDepthTolerance = 50.0f;

        for( DWORD i = 0; i < dwSampleCount; ++i )
        {
            FLOAT CurrentDepth = ConvertDepth( pSamples[i] );
            pEvaluation->m_fMaxDepth = max( pEvaluation->m_fMaxDepth, CurrentDepth );
            pEvaluation->m_fMinDepth = min( pEvaluation->m_fMinDepth, CurrentDepth );

            // If the depth goes above the ideal linear value by a certain amount, then we do not have consistent linear depth
            if( ( CurrentDepth - CurrentIdealDepth ) > LinearDepthTolerance )
            {
                pEvaluation->m_bConsistentLinearDepth = FALSE;
            }

            CurrentIdealDepth += IdealDepthDeltaPerSample;
        }
    }

    if( pSamples != g_pScratchPad )
    {
        delete[] pSamples;
    }
    return TRUE;
}

//--------------------------------------------------------------------------------------
// Name: DepthImageProcessing::CaptureLine
// Desc: Walks a 3D line from BeginPos to EndPos and returns the specified number of
// depth samples.
//--------------------------------------------------------------------------------------
VOID DepthImageProcessing::CaptureLine( const XMVECTOR vBeginPos, const XMVECTOR vEndPos, USHORT* pPixels, DWORD dwSampleCount )
{
    XMVECTOR vPositionDelta = ( vEndPos - vBeginPos ) * XMVectorReciprocal( XMVectorReplicate( (FLOAT)dwSampleCount ) );

    XMVECTOR vCurrentPos = vBeginPos;
    for( DWORD i = 0; i < dwSampleCount; ++i )
    {
        LONG DepthX, DepthY;
        USHORT DepthValue;
        NuiTransformSkeletonToDepthImage( vCurrentPos, &DepthX, &DepthY, &DepthValue );

        if( DepthX >= 0 && DepthX < (LONG)m_dwImageWidth && DepthY >= 0 && DepthY < (LONG)m_dwImageHeight )
        {
            const USHORT* pDepthImagePixel = m_pDepthImageCopy + m_dwImageWidth * DepthY + DepthX;
            pPixels[i] = *pDepthImagePixel;
        }
        else
        {
            pPixels[i] = 0;
        }

        vCurrentPos += vPositionDelta;
    }
}
