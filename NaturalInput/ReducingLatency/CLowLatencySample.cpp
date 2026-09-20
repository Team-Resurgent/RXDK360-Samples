//--------------------------------------------------------------------------------------
// CLowLatencySample.cpp
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "CLowLatencySample.h"


//--------------------------------------------------------------------------------------
// Name: Render
// Desc: Rendering for low latency.
//--------------------------------------------------------------------------------------

VOID CLowLatencySample::Render( const UINT uFrameBufferIdx )
{
    // We double buffer to utilze the hardware best, but single buffer the
    // avatar that is dependent on skeleton tracking for lower latency
    CFrameBufferData* pSceneFrameBufferData = &m_FrameBufferData[ uFrameBufferIdx ];
    CFrameBufferData* pAvatarFrameBufferData = &m_FrameBufferData[ 0 ];

    // Render shadows for objects independent of skeleton tracking
    PIXBeginNamedEvent( 0, "Shadow Pass Independent on ST" );
    BeginRenderPass( RENDER_PASS_SHADOW, uFrameBufferIdx );
    m_Renderer.Render( RENDER_FISH, pSceneFrameBufferData );
    EndRenderPass( RENDER_PASS_SHADOW );
    PIXEndNamedEvent();

    // Render a custom z pre-pass for the scene and fish. We don't want to use BeginZPass()/EndZPass(), since
    // this will write render commands into a command buffer and only submit the render calls once you
    // get to EndZPass()
    PIXBeginNamedEvent( 0, "Pre-Z Pass" );
    BeginRenderPass( RENDER_PASS_Z, uFrameBufferIdx );
    m_Renderer.Render( RENDER_SCENE, pSceneFrameBufferData );
    m_Renderer.Render( RENDER_SCENE_FLOOR, pSceneFrameBufferData );
    m_Renderer.Render( RENDER_FISH, pSceneFrameBufferData );
    EndRenderPass( RENDER_PASS_Z );
    PIXEndNamedEvent();

    // Render all opaque objects independent of skeleton tracking input
    PIXBeginNamedEvent( 0, "Opaque Pass Independent on ST" );
    BeginRenderPass( RENDER_PASS_OPAQUE, uFrameBufferIdx );
    m_Renderer.Render( RENDER_SCENE, pSceneFrameBufferData );
    m_Renderer.Render( RENDER_FISH, pSceneFrameBufferData );
    EndRenderPass( RENDER_PASS_OPAQUE );
    PIXEndNamedEvent();

    // Save the current color and depth buffers
    PIXBeginNamedEvent( 0, "Save Color and Depth Buffers" );
    m_pd3dDevice->Resolve( D3DRESOLVE_RENDERTARGET0, NULL, m_Renderer.m_pBackBufferTexture, NULL, 0, 0, NULL, 1.0f, 0L, NULL );
    m_pd3dDevice->Resolve( D3DRESOLVE_DEPTHSTENCIL, NULL, m_Renderer.m_pDepthBufferTexture, NULL, 0, 0, NULL, 1.0f, 0L, NULL );
    PIXEndNamedEvent();

    // After the previous frame's SynchronizeToPresentationInterval(), Direct3D allows the thread to continue running until enough
    // drawing commands are submitted to fill a segment of the command buffer—typically, 32KB in size. This means that the
    // render thread can potentially run ahead of the skeleton update thread by enough time to still wait on the previous frame's
    // m_hAvatarUpdatedEvent if a frame were dropped for some reason. This block ensures that the render thread never runs
    // ahead of the skeleton update thread for the current frame.
    PIXBeginNamedEvent( 0, "Sync RenderThread to SkeletonUpdateThread" );
    DWORD dwFence = m_pd3dDevice->InsertFence();
    m_pd3dDevice->BlockOnFence( dwFence );
    PIXEndNamedEvent();

    // Wait for avatar to be updated with latest skeleton data.
    PIXBeginNamedEvent( 0, "Wait For Avatar Update" );
    WaitForSingleObject( m_hAvatarUpdatedEvent, 16 );
    ResetEvent( m_hAvatarUpdatedEvent );
    PIXEndNamedEvent();

    // Render shadows for objects dependent of skeleton tracking into hybrid shadowmap
    PIXBeginNamedEvent( 0, "Shadow Pass Dependent on ST" );
    BeginRenderPass( RENDER_PASS_HYBRID_SHADOW, uFrameBufferIdx );
    m_Renderer.Render( RENDER_AVATAR, pAvatarFrameBufferData );
    EndRenderPass( RENDER_PASS_HYBRID_SHADOW );
    PIXEndNamedEvent();

    // Restore the color and depth buffers
    PIXBeginNamedEvent( 0, "Restore Color and Depth Buffers" );
    m_Renderer.RestoreBuffers();
    PIXEndNamedEvent();

    // Render all opaque objects dependent of skeleton tracking input
    PIXBeginNamedEvent( 0, "Opaque Pass Dependent on ST" );
    BeginRenderPass( RENDER_PASS_OPAQUE, uFrameBufferIdx );
    m_Renderer.Render( RENDER_SCENE_FLOOR, pSceneFrameBufferData );
    m_Renderer.Render( RENDER_AVATAR, pAvatarFrameBufferData );
    EndRenderPass( RENDER_PASS_OPAQUE );
    PIXEndNamedEvent();

    // Since the avatar vertex buffer is updated on another thread and we single buffer this vertex buffer
    // for low latency, we need to make sure to unset the avatar resources from the device and add a fence so
    // that D3D can block on the skeleton update thread if needed in AvatarRenderer::RebuildJoints()
    dwFence = m_pd3dDevice->InsertFence();
    m_Renderer.SetAvatarRenderedFence( dwFence );

    // Render transparent objects
    PIXBeginNamedEvent( 0, "Transparent Pass" );
    BeginRenderPass( RENDER_PASS_TRANSPARENT, uFrameBufferIdx );
    m_Renderer.Render( RENDER_SCENE, pSceneFrameBufferData );
    EndRenderPass( RENDER_PASS_TRANSPARENT );
    PIXEndNamedEvent();

    PIXBeginNamedEvent( 0, "RenderPostEffects" );
    m_Renderer.RenderPostEffects( RENDER_POST_SCREEN_SPACE_AA | RENDER_POST_WATER_DISTORTION, pSceneFrameBufferData );
    PIXEndNamedEvent();

    RenderUI();

    m_Renderer.Present();

}


//--------------------------------------------------------------------------------------
// Name: Initialize
// Desc: Initialize the sample
//--------------------------------------------------------------------------------------

HRESULT CLowLatencySample::Initialize()
{
    RETURN_ON_FAIL( CLatencySample::Initialize() );

    // For low latency don't use GPU double buffering
    m_pd3dDevice->SetRenderState( D3DRS_BUFFER2FRAMES, FALSE );
    m_pCmdBufferDevice->SetRenderState( D3DRS_BUFFER2FRAMES, FALSE );

    // If we do happen to miss completing the frame, we still want to
    // swap the results within a threshold
    m_pd3dDevice->SetRenderState( D3DRS_PRESENTIMMEDIATETHRESHOLD, 20 );  
    m_pCmdBufferDevice->SetRenderState( D3DRS_PRESENTIMMEDIATETHRESHOLD, 20 );

    // Don't use predicated tiling
    m_Renderer.m_bUsePredicatedTiling = FALSE;

    // Using hybrid shadow maps so that we can render objects not dependent
    // on skeleton tracking in one channel and objects dependent on
    // skeleton tracking in another channel of the same shadowmap
    m_Renderer.m_bUseHybridShadowMap = TRUE;

    // Set the avatar renderer to use single buffering since we
    // our game engine architecture will garentee that we won't
    // have resources still being used by the GPU
    m_Renderer.m_pAvatarRenderer->SetSingleBuffering();

    // Initialize smoothing filter
    m_FilterTaylorSeries.Init( 0.7f );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: BeginRenderPass
// Desc: Begins a render pass
//--------------------------------------------------------------------------------------

VOID CLowLatencySample::BeginRenderPass( const ERenderPass RenderPass, const UINT uFrameBufferIdx )
{
    CFrameBufferData* pFrameBufferData = &m_FrameBufferData[ uFrameBufferIdx ];

    m_Renderer.m_RenderPass = RenderPass;

    switch ( RenderPass )
    {
    case RENDER_PASS_SHADOW:
    case RENDER_PASS_HYBRID_SHADOW:
        m_Renderer.BeginShadowPass( pFrameBufferData, RenderPass );
        break;

    case RENDER_PASS_Z:
        m_Renderer.BeginZPass( pFrameBufferData );
        m_pd3dDevice->SetRenderTarget( 0, m_Renderer.m_pBackBufferSurface );
        m_pd3dDevice->SetDepthStencilSurface( m_Renderer.m_pDepthStencilSurface );
        m_pd3dDevice->Clear( 0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, m_Renderer.m_dwFogColor, 1.0f, 0 );
        m_pd3dDevice->SetRenderTarget( 0, NULL );
        break;

    case RENDER_PASS_OPAQUE:
        m_pd3dDevice->SetShaderGPRAllocation( 0, 60, GPU_GPRS - 60 );
        m_Renderer.SetRenderStatesAndConstants( pFrameBufferData );
        m_pd3dDevice->SetRenderTarget( 0, m_Renderer.m_pBackBufferSurface );
        m_pd3dDevice->SetDepthStencilSurface( m_Renderer.m_pDepthStencilSurface );
        break;

    case RENDER_PASS_TRANSPARENT:
        m_Renderer.SetRenderStatesAndConstants( pFrameBufferData );
        break;
    }
}


//--------------------------------------------------------------------------------------
// Name: EndRenderPass
// Desc: Ends a render pass
//--------------------------------------------------------------------------------------

VOID CLowLatencySample::EndRenderPass( const ERenderPass RenderPass )
{
    switch ( RenderPass )
    {
    case RENDER_PASS_SHADOW:
    case RENDER_PASS_HYBRID_SHADOW:
        m_Renderer.EndShadowPass( RenderPass );
        break;

    case RENDER_PASS_Z:
        m_Renderer.EndZPass();
        break;
    }

    m_Renderer.m_RenderPass = RENDER_PASS_NONE;
}


//--------------------------------------------------------------------------------------
// Name: FilterJointPositions
// Desc: Filter joint positions. Note that the avateering library is already filtering
//       the joint positions, so we don't need to filter the joint positions ourselves.
//       This is purely for to show how filtering can effect latency
//--------------------------------------------------------------------------------------

VOID CLowLatencySample::FilterJointPositions( const UINT uFrameBufferIdx )
{
    CFrameBufferData* pFrameBufferData = &m_FrameBufferData[ uFrameBufferIdx ];
    INT iSkeletonIdx = pFrameBufferData->m_iSkeletonIdx;

    // if filtering is switched on, we do Taylor series filtering
    if ( m_bEnableJointFiltering &&
         iSkeletonIdx >= 0 )
    {
        NUI_SKELETON_DATA* pSkeletonData = &pFrameBufferData->m_SkeletonFrame.SkeletonData[ iSkeletonIdx ];

        // if the player isn't tracked reset the filter state
        if ( pSkeletonData->eTrackingState != NUI_SKELETON_TRACKED )
        {
            m_FilterTaylorSeries.Reset();
        }

        // Filter the joints
        m_FilterTaylorSeries.Update( pSkeletonData->SkeletonPositions );
 
        // Copy the filtered joints over the original data
        XMemCpy( pSkeletonData->SkeletonPositions, m_FilterTaylorSeries.GetFilteredJoints(), sizeof( pSkeletonData->SkeletonPositions ) );
    }
}