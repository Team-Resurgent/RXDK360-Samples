//--------------------------------------------------------------------------------------
// CHighLatencySample.cpp
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "CHighLatencySample.h"


//--------------------------------------------------------------------------------------
// Name: Render
// Desc: Rendering for high latency.
//--------------------------------------------------------------------------------------

VOID CHighLatencySample::Render( const UINT uFrameBufferIdx )
{
    CFrameBufferData* pFrameBufferData = &m_FrameBufferData[ uFrameBufferIdx ];

    // All data is double buffered, so just render all shadow casters
    PIXBeginNamedEvent( 0, "Shadow Pass" );
    BeginRenderPass( RENDER_PASS_SHADOW, uFrameBufferIdx );
    m_Renderer.Render( RENDER_FISH, pFrameBufferData );
    m_Renderer.Render( RENDER_AVATAR, pFrameBufferData );
    EndRenderPass( RENDER_PASS_SHADOW );
    PIXEndNamedEvent();

    // Render the main pass with predicated tiling
    PIXBeginNamedEvent( 0, "Main Pass" );
    BeginRenderPass( RENDER_PASS_OPAQUE_AND_Z, uFrameBufferIdx );
    m_Renderer.Render( RENDER_SCENE, pFrameBufferData );
    m_Renderer.Render( RENDER_SCENE_FLOOR, pFrameBufferData );
    m_Renderer.Render( RENDER_FISH, pFrameBufferData );
    m_Renderer.Render( RENDER_AVATAR, pFrameBufferData );
    EndRenderPass( RENDER_PASS_OPAQUE_AND_Z );

    BeginRenderPass( RENDER_PASS_TRANSPARENT, uFrameBufferIdx );
    m_Renderer.Render( RENDER_SCENE, pFrameBufferData );
    EndRenderPass( RENDER_PASS_TRANSPARENT );
    PIXEndNamedEvent();

    PIXBeginNamedEvent( 0, "RenderPostEffects" );
    m_Renderer.RenderPostEffects( RENDER_POST_WATER_DISTORTION, pFrameBufferData );
    PIXEndNamedEvent();

    RenderUI();

    m_Renderer.Present();
}


//--------------------------------------------------------------------------------------
// Name: Initialize
// Desc: Initialize the sample
//--------------------------------------------------------------------------------------

HRESULT CHighLatencySample::Initialize()
{
    RETURN_ON_FAIL( CLatencySample::Initialize() );

    // Use GPU double buffering
    m_pd3dDevice->SetRenderState( D3DRS_BUFFER2FRAMES, TRUE );
    m_pCmdBufferDevice->SetRenderState( D3DRS_BUFFER2FRAMES, TRUE );

    // Don't allow any tearing
    m_pd3dDevice->SetRenderState( D3DRS_PRESENTIMMEDIATETHRESHOLD, 0 );  
    m_pCmdBufferDevice->SetRenderState( D3DRS_PRESENTIMMEDIATETHRESHOLD, 0 );

    // Use predicated tiling
    m_Renderer.m_bUsePredicatedTiling = TRUE;

    // Use a standard shadowmap
    m_Renderer.m_bUseHybridShadowMap = FALSE;

    // Double buffer the avatar resources
    m_Renderer.m_pAvatarRenderer->SetDoubleBuffering();

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: BeginRenderPass
// Desc: Begin a render pass
//--------------------------------------------------------------------------------------

VOID CHighLatencySample::BeginRenderPass( const ERenderPass RenderPass, const UINT uFrameBufferIdx )
{
    CFrameBufferData* pFrameBufferData = &m_FrameBufferData[ uFrameBufferIdx ];

    m_Renderer.m_RenderPass = RenderPass;
    
    switch ( RenderPass )
    {
    case RENDER_PASS_SHADOW:
        m_Renderer.BeginShadowPass( pFrameBufferData, RenderPass );
        break;

    case RENDER_PASS_OPAQUE_AND_Z:
        m_pd3dDevice->SetShaderGPRAllocation( 0, 60, GPU_GPRS - 60 );
        m_Renderer.SetRenderStatesAndConstants( pFrameBufferData );
        m_pd3dDevice->SetRenderTarget( 0, m_Renderer.m_pTiledBackBufferSurface );
        m_pd3dDevice->SetDepthStencilSurface( m_Renderer.m_pTiledDepthStencilSurface );
        m_pd3dDevice->BeginTiling( 0, 2, m_Renderer.m_pTilingRects, &m_Renderer.m_vFogColor, 1.0f, 0L );
        m_pd3dDevice->BeginZPass( 0 );
        break;

    case RENDER_PASS_OPAQUE:
        m_pd3dDevice->SetShaderGPRAllocation( 0, 60, GPU_GPRS - 60 );
        m_Renderer.SetRenderStatesAndConstants( pFrameBufferData );
        m_pd3dDevice->SetRenderTarget( 0,m_Renderer. m_pTiledBackBufferSurface );
        m_pd3dDevice->SetDepthStencilSurface( m_Renderer.m_pTiledDepthStencilSurface );
        m_pd3dDevice->BeginTiling( 0, 2, m_Renderer.m_pTilingRects, &m_Renderer.m_vFogColor, 1.0f, 0L );
        break;

    case RENDER_PASS_TRANSPARENT:
        m_Renderer.SetRenderStatesAndConstants( pFrameBufferData );
        break;
    }
}


//--------------------------------------------------------------------------------------
// Name: EndRenderPass
// Desc: End the render pass
//--------------------------------------------------------------------------------------

VOID CHighLatencySample::EndRenderPass( const ERenderPass RenderPass )
{
    switch ( RenderPass )
    {
    case RENDER_PASS_SHADOW:
        m_Renderer.EndShadowPass( RenderPass );
        break;

    case RENDER_PASS_OPAQUE_AND_Z:
        m_pd3dDevice->EndZPass();
        break;

    case RENDER_PASS_TRANSPARENT:
        m_pd3dDevice->EndTiling( D3DRESOLVE_RENDERTARGET0 | D3DRESOLVE_CLEARRENDERTARGET | D3DRESOLVE_CLEARDEPTHSTENCIL,
                                NULL, m_Renderer.m_pBackBufferTexture, &m_Renderer.m_vFogColor, 1.0f, 0L, NULL );
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

VOID CHighLatencySample::FilterJointPositions( const UINT uFrameBufferIdx )
{
    CFrameBufferData* pFrameBufferData = &m_FrameBufferData[ uFrameBufferIdx ];
    INT iSkeletonIdx = pFrameBufferData->m_iSkeletonIdx;

    // if filtering is switched on, we do some naive average smoothing
    if ( m_bEnableJointFiltering &&
         iSkeletonIdx >= 0 )
    {
        static XMVECTOR vPreviousRawPositions[ NUI_SKELETON_POSITION_COUNT ];
 
        // if the player isn't tracked reset the filter state
        if ( pFrameBufferData->m_SkeletonFrame.SkeletonData[ iSkeletonIdx ].eTrackingState != NUI_SKELETON_TRACKED )
        {
            XMemSet( vPreviousRawPositions, 0, sizeof( vPreviousRawPositions ) );
        }

        // Do naive average filtering
        XMVECTOR vCurrentRawPositions[ NUI_SKELETON_POSITION_COUNT ];

        // Make a copy of the original positions
        XMemCpy( vCurrentRawPositions, pFrameBufferData->m_SkeletonFrame.SkeletonData[ iSkeletonIdx ].SkeletonPositions, sizeof( vCurrentRawPositions ) );

        for ( UINT i = 0; i < NUI_SKELETON_POSITION_COUNT; i++ )
        {
            XMVECTOR vAverage = ( vPreviousRawPositions[ i ] + vCurrentRawPositions[ i ] ) * 0.5f;
            pFrameBufferData->m_SkeletonFrame.SkeletonData[ iSkeletonIdx ].SkeletonPositions[ i ] = vAverage;
        }

        // Make a copy of the original positions into the previous array
        XMemCpy( vPreviousRawPositions, pFrameBufferData->m_SkeletonFrame.SkeletonData[ iSkeletonIdx ].SkeletonPositions, sizeof( vCurrentRawPositions ) );
    }
}