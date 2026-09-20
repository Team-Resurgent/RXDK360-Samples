//--------------------------------------------------------------------------------------
// AnimationPlayback.cpp
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include <assert.h>
#include "AnimationPlayback.h"

VOID SimpleAnimationPlayer::BindAnimationToFrames( ATG::Animation* pAnimation, ATG::Scene* pScene,
                                                   ATG::Frame* pRootFrame )
{
    m_pAnimation = pAnimation;
    DWORD dwTrackCount = m_pAnimation->GetAnimationTrackCount();
    for( DWORD i = 0; i < dwTrackCount; i++ )
    {
        ATG::AnimationTransformTrack* pTrack = m_pAnimation->GetAnimationTrack( i );
        ATG::Frame* pFrame = ( ATG::Frame* )pScene->FindObject( pTrack->GetName() );
        if( pFrame == NULL )
            continue;
        if( !pFrame->IsAncestor( pRootFrame ) )
            continue;
        AnimationFrameBinding Binding;
        ZeroMemory( &Binding, sizeof( AnimationFrameBinding ) );
        Binding.pFrame = pFrame;
        Binding.pTrack = pTrack;
        m_BindingList.push_back( Binding );
    }
}

VOID SimpleAnimationPlayer::Update( FLOAT fDeltaTime )
{
    if( m_pAnimation == NULL )
        return;
    m_fCurrentTime += m_fPlaybackSpeed * fDeltaTime;
    FLOAT fDuration = m_pAnimation->GetDuration();
    if( m_bLooping )
    {
        while( m_fCurrentTime < 0 )
            m_fCurrentTime += fDuration;
        while( m_fCurrentTime > fDuration )
            m_fCurrentTime -= fDuration;
    }
    else
    {
        if( m_fCurrentTime < 0 )
            m_fCurrentTime = 0;
        else if( m_fCurrentTime > fDuration )
            m_fCurrentTime = fDuration;
    }
    DWORD dwBindingCount = ( DWORD )m_BindingList.size();
    for( DWORD i = 0; i < dwBindingCount; i++ )
    {
        AnimationFrameBinding& Binding = m_BindingList[i];
        XMVECTOR vPos = Binding.pTrack->SamplePosition( m_fCurrentTime, &Binding.dwPositionIndex,
                                                        m_fPlaybackSpeed > 0 );
        XMVECTOR vOrientation = Binding.pTrack->SampleOrientation( m_fCurrentTime, &Binding.dwOrientationIndex,
                                                                   m_fPlaybackSpeed > 0 );
        XMVECTOR vScale = Binding.pTrack->SampleScale( m_fCurrentTime, &Binding.dwScaleIndex, m_fPlaybackSpeed > 0 );

        XMMATRIX matScale = XMMatrixScalingFromVector( vScale );
        XMMATRIX matRotation = XMMatrixRotationQuaternion( XMQuaternionNormalize( vOrientation ) );
        XMMATRIX matTransform = matScale * matRotation;
        matTransform.r[3] = XMVectorSelect( matTransform.r[3], vPos, XMVectorSelectControl( 1, 1, 1, 0 ) );

        Binding.pFrame->SetLocalTransform( matTransform );
    }
}

VOID SimpleAnimationPlayer::Clear()
{
    m_pAnimation = NULL;
    m_BindingList.clear();
    m_fCurrentTime = 0;
}
