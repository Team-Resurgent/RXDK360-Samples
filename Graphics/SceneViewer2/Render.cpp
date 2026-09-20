//--------------------------------------------------------------------------------------
// Render.cpp
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "SceneViewer2.h"

// Disable warning C6211: Leaking memory <pointer> due to an exception.
// We "know" that we won't run out of memory in this sample
#pragma warning ( disable : 6211 )

#if 1
#include <tracerecording.h>
#pragma comment( lib, "tracerecording.lib" )
#endif


VOID dprintmatrix( const XMMATRIX matMatrix )
{
    ATG::DebugSpew( "matrix %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f\n", matMatrix._11, matMatrix._12,
                    matMatrix._13, matMatrix._14, matMatrix._21, matMatrix._22, matMatrix._23, matMatrix._24,
                    matMatrix._31, matMatrix._32, matMatrix._33, matMatrix._34, matMatrix._41, matMatrix._42,
                    matMatrix._43, matMatrix._44 );
}


//--------------------------------------------------------------------------------------
// Name: Render()
// Desc: Sets up render states, clears the viewport, and renders the scene.
//--------------------------------------------------------------------------------------
HRESULT SceneViewer::Render()
{
    EnterCriticalSection( &m_RenderSettingsCriticalSection );

    UpdateRenderingSettings();

    m_TaskUpdate.WaitUntilComplete();

    // Copy all render-related scene objects to a shadow copy
    CreateSceneShadowCopy();

    m_TaskUpdate.BeginTask();

    // Update camera matrix.
    XMMATRIX matProjOriginal = m_ShadowSceneState.Camera.GetProjection().GetMatrix();
    XMMATRIX matProj = matProjOriginal;
    if( m_bRenderUpsideDown )
    {
        static const XMVECTOR vNegativeOne = XMVectorReplicate( -1.0f );
        matProj.r[0] *= vNegativeOne;
        matProj.r[1] *= vNegativeOne;
    }
    XMMATRIX matView = m_ShadowSceneState.Camera.GetWorldView();
    m_ShadowSceneState.matVP = matView * matProj;

    // Set up debug draw with current camera matrix.
    ATG::DebugDraw::SetViewProjection( m_ShadowSceneState.matVP );

    PIXBeginNamedEvent( 0xFFFFFFFF, "Visibility" );
    if( m_pScene != NULL )
    {
        // Build visible list and sort by distance.
        //if( m_bLockVisibleSet == FALSE )
        BuildVisibilityList( &m_ShadowSceneState.Camera );

        m_dwModelsVisible = m_ShadowSceneState.VisibleModels.size();
        m_dwTotalModels = m_ShadowSceneState.AllModels.size();
    }
    PIXEndNamedEvent();

    AcquireD3D();

    m_pd3dDevice->BeginScene();

    // Reset rendering statistics.
    m_dwPrimitivesRendered = 0;
    m_dwSubsetsRendered = 0;
    m_dwModelsRendered = 0;
    m_dwActiveLights = 0;
    m_dwLightInfluences = 0;
    m_dwShadowMapsRendered = 0;
    m_dwModelsRenderedToShadowMaps = 0;

    // Query perf counters for the start of the frame.
    UpdatePerfCounterUsage();
    RecordPerfEvent( SVPE_BEGINSCENE );

    if( m_pScene == NULL )
    {
        ClearAndBeginTiling();
    }
    else
    {
        // Render shadow maps for each spotlight and directional light.
        if( m_bEnableShadowUpdates )
        {
            m_pd3dDevice->SetShaderGPRAllocation( 0, 112, 16 );
            ResetShadowMapAllocator();
            RenderAllDirLightShadowMaps();
            RenderAllSpotlightShadowMaps();
            // Query perf counters for the end of shadow map rendering.
            RecordPerfEvent( SVPE_END_SHADOWS );
            m_pd3dDevice->SetShaderGPRAllocation( 0, 64, 64 );
        }

        // Begin tiling, or just clear the back buffer if we're not tiling.
        ClearAndBeginTiling();

        BOOL bPerformZPass = ( m_RenderMode != SVRM_PASSPERLIGHT && m_RenderMode != SVRM_DEFERRED );

        // Tell D3D to start recording a Z pass.
        if( m_ZPassMode == SVZP_D3D && bPerformZPass )
        {
            m_pd3dDevice->BeginZPass( 0 );
        }

        // Acquire a lock on the scene.
        AcquireScene();

        // Render a manual Z pass if that is enabled.
        if( m_ZPassMode == SVZP_MANUAL && bPerformZPass )
        {
            RenderManualZPass();
            if( m_TilingMode == SVTM_NONE && m_RenderMode != SVRM_DEFERRED )
            {
                RecordPerfEvent( SVPE_END_ZPASS );
            }
        }

        // Render the scene using the selected renderer.
        switch( m_RenderMode )
        {
            case SVRM_DEFERRED:
                RenderSceneDeferred();
                break;
            case SVRM_PASSPERLIGHT:
                RenderScenePassPerLight();
                break;
            case SVRM_NORMAL:
            case SVRM_SHADERLIB:
            default:
                RenderSceneNormal();
                break;
        }

        // Render all debug objects.
        RenderDebugObjects();

        // End the recording of the Z pass.
        if( m_ZPassMode == SVZP_D3D && bPerformZPass )
        {
            m_pd3dDevice->EndZPass();
        }

        // TODO: Render transparent objects here.

        // Release the scene lock.
        ReleaseScene();
    }

    // End tiling, or just resolve if we're not tiling.
    EndTiling();

    // Setup rendertargets for post-effect rendering.
    SetupPostEffectTargets();

    if( m_RenderMode == SVRM_DEFERRED )
    {
        // Query perf counters for the end of scene rendering.
        RecordPerfEvent( SVPE_END_DEFERRED_CONSTRUCTION );

        // Perform the deferred lighting pass.
        if( m_pScene == NULL )
        {
            // Render staging screen (blue gradient background) to the lighting buffer.
            RenderStagingScreen();
        }
        else
        {
            // Render the lighting passes to the lighting buffer.
            RenderLightsDeferred();
        }

        // Resolve lit scene to scene resolve buffer.
        m_pd3dDevice->Resolve( D3DRESOLVE_RENDERTARGET0, NULL, m_pSceneResolveBuffer,
                               NULL, 0, 0, NULL, 1.0f, 0, NULL );
    }

    // Query perf counters for the end of scene rendering.
    RecordPerfEvent( SVPE_END_SCENERENDER );

    // Perform post-effects.
    RenderPostEffects();
    // Query perf counters for the end of post-effect rendering.
    RecordPerfEvent( SVPE_END_POSTEFFECTS );

    // Set up debug draw with current camera matrix (without optional inversion).
    ATG::DebugDraw::SetViewProjection( matView * matProjOriginal );

    // Render UI.
    RenderUI();
    // Query perf counters for the end of UI rendering.
    RecordPerfEvent( SVPE_END_UIRENDER );

    // Synchronize to presentation interval before the resolve.
    m_pd3dDevice->SynchronizeToPresentationInterval();

    // Resolve post-effect buffer to front buffer.
    ResolveFullScreenToFrontBuffer();
    // Query perf counters for the end of the resolve.
    RecordPerfEvent( SVPE_END_RESOLVE );

    // Swap the current front buffer pointer.
    m_pd3dDevice->Swap( m_pFrontBuffer, NULL );

    // Release the lock on the Direct3D device.
    ReleaseD3D();

    LeaveCriticalSection( &m_RenderSettingsCriticalSection );

    return S_OK;
}


VOID SceneViewer::CreateSceneShadowCopy()
{
    if( m_pScene == NULL )
    {
        return;
    }

    // clear shadow copy lists
    m_ShadowSceneState.Clear();
    m_ShadowSceneState.EntireSceneBounds.Clear();

    ATG::NameIndexedCollection::iterator i;
    for( i = m_pScene->GetInstanceList()->begin(); i != m_pScene->GetInstanceList()->end(); i++ )
    {
        // Look for models.
        if( ( *i )->IsDerivedFrom( ATG::Model::TypeID ) )
        {
            ATG::Model* pModel = ( ATG::Model* )( *i );
            m_ShadowSceneState.AllModelWorldTransforms.push_back( pModel->GetWorldTransform() );
            ATG::Bound WorldBound = pModel->GetWorldBound();
            m_ShadowSceneState.EntireSceneBounds.Merge( WorldBound );
            m_ShadowSceneState.AllModelWorldBounds.push_back( WorldBound );
            m_ShadowSceneState.AllModels.push_back( pModel );
        }
        else if( ( *i )->IsDerivedFrom( ATG::PointLight::TypeID ) )
        {
            ATG::PointLight* pLight = ( ATG::PointLight* )( *i );
            m_ShadowSceneState.PointLights.PushBack( *pLight );
            m_ShadowSceneState.PointLights.Back().DisconnectFromParent();
        }
        else if( ( *i )->IsDerivedFrom( ATG::SpotLight::TypeID ) )
        {
            ATG::SpotLight* pLight = ( ATG::SpotLight* )( *i );
            m_ShadowSceneState.SpotLights.PushBack( *pLight );
            m_ShadowSceneState.SpotLights.Back().DisconnectFromParent();
        }
        else if( ( *i )->IsDerivedFrom( ATG::DirectionalLight::TypeID ) )
        {
            ATG::DirectionalLight* pLight = ( ATG::DirectionalLight* )( *i );
            m_ShadowSceneState.DirLights.PushBack( *pLight );
            m_ShadowSceneState.DirLights.Back().DisconnectFromParent();
        }
    }

    if( m_pCurrentSceneCamera != NULL )
    {
        m_ShadowSceneState.Camera = *m_pCurrentSceneCamera;
        m_ShadowSceneState.Camera.DisconnectFromParent();
    }
}


VOID SceneViewer::UpdateRenderingSettings()
{
    ++m_dwRenderThreadFrameCount;

    // Update rendering mode and tiling type.
    {
        static SceneViewerRenderMode s_CurrentRenderMode = m_RenderMode;
        static SceneViewerTilingMode s_CurrentTilingMode = m_TilingMode;
        if( s_CurrentRenderMode != m_RenderMode ||
            s_CurrentTilingMode != m_TilingMode )
        {
            BOOL bRebindShaders = FALSE;
            // If the render mode has changed, we need to re-bind shaders.
            if( s_CurrentRenderMode != m_RenderMode )
            {
                bRebindShaders = TRUE;
            }
            s_CurrentRenderMode = m_RenderMode;
            s_CurrentTilingMode = m_TilingMode;

            // Re-configure the rendering buffers.
            AcquireD3D();
            SetupBuffers();
            ReleaseD3D();
            // Rebind shaders if necessary.
            if( bRebindShaders && m_pScene != NULL )
            {
                AcquireScene();
                RebindMaterialShaders( m_pScene );
                ReleaseScene();
            }
        }
    }

    // Update presentation interval.
    {
        static DWORD s_CurrentPresentationInterval = m_d3dpp.PresentationInterval;
        if( m_d3dpp.PresentationInterval != s_CurrentPresentationInterval )
        {
            s_CurrentPresentationInterval = m_d3dpp.PresentationInterval;
            AcquireD3D();
            m_pd3dDevice->SetRenderState( D3DRS_PRESENTINTERVAL, m_d3dpp.PresentationInterval );
            //m_pd3dDevice->Reset( &m_d3dpp );
            ReleaseD3D();
        }
    }

    {
        static DWORD s_dwSecondaryRBSize = m_dwSecondaryRingBufferSize;
        if( m_dwSecondaryRingBufferSize != s_dwSecondaryRBSize )
        {
            s_dwSecondaryRBSize = m_dwSecondaryRingBufferSize;
            D3DRING_BUFFER_PARAMETERS RBParams;
            RBParams.Flags = 0;
            RBParams.pPrimary = NULL;
            RBParams.PrimarySize = 32 * 1024;
            RBParams.pSecondary = NULL;
            RBParams.SecondarySize = m_dwSecondaryRingBufferSize * 1024;
            RBParams.SegmentCount = m_dwSecondaryRingBufferSize / 64;

            AcquireD3D();
            m_pd3dDevice->SetRingBufferParameters( &RBParams );
            ReleaseD3D();
        }
    }

    {
        static DWORD s_dwShadowMapSize = m_dwShadowMapSize;
        static BOOL s_bMipShadowMaps = m_bMipShadowMaps;
        if( m_dwShadowMapSize != s_dwShadowMapSize || m_bMipShadowMaps != s_bMipShadowMaps )
        {
            s_dwShadowMapSize = m_dwShadowMapSize;
            s_bMipShadowMaps = m_bMipShadowMaps;
            AcquireD3D();
            // Re-configure the rendering buffers.
            SetupBuffers();
            BuildShadowMapBank();
            ReleaseD3D();
        }
    }
}


VOID SceneViewer::SetupPostEffectTargets()
{
    m_pd3dDevice->SetRenderTarget( 0, m_pFullScreenTarget );
    m_pd3dDevice->SetRenderTarget( 1, NULL );
    m_pd3dDevice->SetRenderTarget( 2, NULL );
    m_pd3dDevice->SetRenderTarget( 3, NULL );
    m_pd3dDevice->SetDepthStencilSurface( NULL );
}


VOID SceneViewer::RenderPostEffects()
{
    PIXBeginNamedEvent( 0xFF00FFFF, "Post Effects" );
    static const FLOAT fPostEffectRect[15] =
    {
        -1,  1, 0, 0,  0,
        1,  1, 0, 1,  0,
        -1, -1, 0, 0,  1
    };
    static const FLOAT fPostEffectRectUpsideDown[15] =
    {
        -1,  1, 0, 1,  1,
        1,  1, 0, 0,  1,
        -1, -1, 0, 1,  0
    };

    m_PostEffectParameterPool.SetBuffers( m_pSceneResolveBuffer, m_pDepthTexture );

    switch( m_dwPostEffectTechniqueIndex )
    {
        case 1:
            m_PostEffectParameterPool.SetFocalSettings( m_fFocalDepth, m_fFocalAperture, m_fFocalSlope,
                                                        m_fMaxCircleOfConfusion );
            break;
    }

    ATG::FXLiteMaterialImplementation* pFXLiteMaterialImpl = ( ATG::FXLiteMaterialImplementation* )
        m_pPostEffects->GetMaterialImplementation();
    FXLEffect* pEffect = pFXLiteMaterialImpl->m_pEffect;
    assert( pEffect != NULL );
    pEffect->BeginTechniqueFromIndex( m_dwPostEffectTechniqueIndex, 0 );
    pEffect->BeginPassFromIndex( 0 );
    pEffect->Commit();

    ATG::SimpleShaders::SetDeclPosTex();
    if( m_bRenderUpsideDown )
        m_pd3dDevice->DrawPrimitiveUP( D3DPT_RECTLIST, 1, fPostEffectRectUpsideDown, 5 * sizeof( FLOAT ) );
    else
        m_pd3dDevice->DrawPrimitiveUP( D3DPT_RECTLIST, 1, fPostEffectRect, 5 * sizeof( FLOAT ) );

    pEffect->EndPass();
    pEffect->EndTechnique();
    PIXEndNamedEvent();
}


VOID SceneViewer::SetupSceneRenderingTargets()
{
    m_pd3dDevice->SetRenderTarget( 0, m_pColorRenderTargets[0] );
    m_pd3dDevice->SetRenderTarget( 1, m_pColorRenderTargets[1] );
    m_pd3dDevice->SetRenderTarget( 2, m_pColorRenderTargets[2] );
    m_pd3dDevice->SetRenderTarget( 3, m_pColorRenderTargets[3] );
    m_pd3dDevice->SetDepthStencilSurface( m_pDepthBuffer );
}


VOID SceneViewer::ClearAndBeginTiling()
{
    SetupSceneRenderingTargets();

    if( m_dwTilingRectCount > 0 )
    {
        D3DVECTOR4 ClearColor = { 0, 0, 0, 0 };
        switch( m_RenderMode )
        {
            case SVRM_NORMAL:
            case SVRM_SHADERLIB:
            case SVRM_PASSPERLIGHT:
                m_pd3dDevice->BeginTiling( 0, m_dwTilingRectCount, m_TilingRects,
                                           &ClearColor, 1.0f, 0 );
                break;
            case SVRM_DEFERRED:
                m_pd3dDevice->BeginTiling( D3DTILING_SKIP_FIRST_TILE_CLEAR,
                                           m_dwTilingRectCount,
                                           m_TilingRects,
                                           &ClearColor, 1.0f, 0 );
                m_pd3dDevice->SetPredication( D3DPRED_TILE( 0 ) );
                m_pd3dDevice->Clear( 0, NULL,
                                     D3DCLEAR_TARGET0 |
                                     D3DCLEAR_TARGET1 |
                                     D3DCLEAR_ZBUFFER |
                                     D3DCLEAR_STENCIL, 0, 1.0f, 0 );
                m_pd3dDevice->SetPredication( 0 );
                break;
        }
    }
    else
    {
        switch( m_RenderMode )
        {
            case SVRM_NORMAL:
            case SVRM_SHADERLIB:
            case SVRM_PASSPERLIGHT:
                m_pd3dDevice->Clear( 0, NULL, D3DCLEAR_TARGET0 | D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL, 0, 1.0f, 0 );
                break;
            case SVRM_DEFERRED:
                m_pd3dDevice->Clear( 0, NULL, D3DCLEAR_TARGET0 | D3DCLEAR_TARGET1 | D3DCLEAR_ZBUFFER |
                                     D3DCLEAR_STENCIL, 0, 1.0f, 0 );
                break;
        }
    }
}

VOID SceneViewer::ResolveFullScreenToFrontBuffer()
{
    m_pd3dDevice->Resolve( D3DRESOLVE_RENDERTARGET0,
                           NULL,
                           m_pFrontBuffer,
                           NULL,
                           0, 0, NULL, 1.0f, 0, NULL );
}

VOID SceneViewer::EndTiling()
{
    // Determine if we need to resolve out the depth buffer.
    BOOL bResolveDepthBuffer = FALSE;
    switch( m_dwPostEffectTechniqueIndex )
    {
        case 1:
        case 2:
        case 3:
        case 5:
        case 6:
            bResolveDepthBuffer = TRUE;
            break;
    }

    D3DVECTOR4 ClearColor = { 0, 0, 0, 0 };
    if( m_dwTilingRectCount > 0 )
    {
        // Tiling rects are present; call EndTiling as necessary.
        switch( m_RenderMode )
        {
            case SVRM_NORMAL:
            case SVRM_SHADERLIB:
            case SVRM_PASSPERLIGHT:
                {
                    // Optionally resolve out depth buffer.
                    if( bResolveDepthBuffer )
                    {
                        for( DWORD i = 0; i < m_dwTilingRectCount; i++ )
                        {
                            D3DPOINT* pDestPoint = ( D3DPOINT* )&m_TilingRects[i];
                            m_pd3dDevice->SetPredication( D3DPRED_TILE( i ) );

                            m_pd3dDevice->Resolve( D3DRESOLVE_DEPTHSTENCIL |
                                                   D3DRESOLVE_FRAGMENT0,
                                                   &m_TilingRects[i],
                                                   m_pDepthTexture,
                                                   pDestPoint,
                                                   0, 0, &ClearColor, 1.0f, 0, NULL );
                        }
                        m_pd3dDevice->SetPredication( 0 );
                    }

                    // End tiling.  This will resolve out the color buffer.
                    m_EndTilingResult = m_pd3dDevice->EndTiling( D3DRESOLVE_RENDERTARGET0 |
                                                                 D3DRESOLVE_ALLFRAGMENTS |
                                                                 D3DRESOLVE_CLEARDEPTHSTENCIL |
                                                                 D3DRESOLVE_CLEARRENDERTARGET, NULL,
                                                                 m_pSceneResolveBuffer,
                                                                 &ClearColor, 1.0f, 0, NULL );
                }
                break;
            case SVRM_DEFERRED:
            {
                for( DWORD i = 0; i < m_dwTilingRectCount; i++ )
                {
                    D3DPOINT* pDestPoint = ( D3DPOINT* )&m_TilingRects[i];
                    m_pd3dDevice->SetPredication( D3DPRED_TILE( i ) );

                    // Resolve out the depth buffer.
                    m_pd3dDevice->Resolve( D3DRESOLVE_DEPTHSTENCIL |
                                           D3DRESOLVE_FRAGMENT0,
                                           &m_TilingRects[i],
                                           m_pDepthTexture,
                                           pDestPoint,
                                           0, 0, &ClearColor, 1.0f, 0, NULL );

                    // Resolve out the color buffer.
                    // Also clear the depth/stencil here too, since that can't happen
                    // alongside the depth resolve.
                    m_pd3dDevice->Resolve( D3DRESOLVE_RENDERTARGET0 |
                                           D3DRESOLVE_CLEARRENDERTARGET |
                                           D3DRESOLVE_CLEARDEPTHSTENCIL,
                                           &m_TilingRects[i],
                                           m_pDeferredColorBuffer,
                                           pDestPoint,
                                           0, 0, &ClearColor, 1.0f, 0, NULL );

                    // Resolve out the normal buffer.
                    m_pd3dDevice->Resolve( D3DRESOLVE_RENDERTARGET1 |
                                           D3DRESOLVE_CLEARRENDERTARGET,
                                           &m_TilingRects[i],
                                           m_pDeferredNormalBuffer,
                                           pDestPoint,
                                           0, 0, &ClearColor, 1.0f, 0, NULL );

                }
                m_pd3dDevice->SetPredication( 0 );
                // End tiling.  No resolves are done automatically.
                m_EndTilingResult = m_pd3dDevice->EndTiling( 0, NULL, NULL, &ClearColor, 1.0f, 0, NULL );
                break;
            }
        }
    }
    else
    {
        // No tiling rects - just call Resolve as necessary.
        switch( m_RenderMode )
        {
            case SVRM_NORMAL:
            case SVRM_SHADERLIB:
            case SVRM_PASSPERLIGHT:
                {
                    // Optionally resolve depth buffer.
                    if( bResolveDepthBuffer )
                    {
                        m_pd3dDevice->Resolve( D3DRESOLVE_DEPTHSTENCIL |
                                               D3DRESOLVE_FRAGMENT0,
                                               NULL,
                                               m_pDepthTexture,
                                               NULL,
                                               0, 0, &ClearColor, 1.0f, 0, NULL );
                    }
                    // Resolve color rendertarget to scene resolve buffer.
                    m_pd3dDevice->Resolve( D3DRESOLVE_RENDERTARGET0 |
                                           D3DRESOLVE_CLEARRENDERTARGET |
                                           D3DRESOLVE_CLEARDEPTHSTENCIL,
                                           NULL,
                                           m_pSceneResolveBuffer,
                                           NULL,
                                           0, 0, &ClearColor, 1.0f, 0, NULL );
                    break;
                }
            case SVRM_DEFERRED:
            {
                // Resolve deferred color buffer.
                m_pd3dDevice->Resolve( D3DRESOLVE_RENDERTARGET0 |
                                       D3DRESOLVE_CLEARRENDERTARGET,
                                       NULL,
                                       m_pDeferredColorBuffer,
                                       NULL,
                                       0, 0, &ClearColor, 1.0f, 0, NULL );

                // Resolve deferred normal buffer.
                m_pd3dDevice->Resolve( D3DRESOLVE_RENDERTARGET1 |
                                       D3DRESOLVE_CLEARRENDERTARGET,
                                       NULL,
                                       m_pDeferredNormalBuffer,
                                       NULL,
                                       0, 0, &ClearColor, 1.0f, 0, NULL );

                // Resolve deferred depth buffer.
                m_pd3dDevice->Resolve( D3DRESOLVE_DEPTHSTENCIL |
                                       D3DRESOLVE_FRAGMENT0 |
                                       D3DRESOLVE_CLEARDEPTHSTENCIL,
                                       NULL,
                                       m_pDepthTexture,
                                       NULL,
                                       0, 0, &ClearColor, 1.0f, 0, NULL );
                break;
            }
        }
    }
}


VOID SceneViewer::BuildShadowMapBank()
{
    const DWORD dwSimultaneousShadowMapCount = 8;
    m_ShadowMapBank.reserve( dwSimultaneousShadowMapCount );

    DWORD dwCount = ( DWORD )m_ShadowMapBank.size();
    for( DWORD i = 0; i < dwCount; ++i )
    {
        if( m_ShadowMapBank[i] != NULL )
        {
            m_ShadowMapBank[i]->Release();
        }
    }
    m_ShadowMapBank.clear();

    for( DWORD i = 0; i < dwSimultaneousShadowMapCount; ++i )
    {
        m_ShadowMapBank.push_back( CreateShadowMapTexture() );
    }

    ResetShadowMapAllocator();
}


VOID SceneViewer::ResetShadowMapAllocator()
{
    m_dwShadowMapBankUsage = 0;
}


D3DTexture* SceneViewer::AllocateShadowMap( DWORD dwFlags )
{
    if( m_dwShadowMapBankUsage >= ( DWORD )m_ShadowMapBank.size() )
        return NULL;
    D3DTexture* pTex = m_ShadowMapBank[ m_dwShadowMapBankUsage ];
    ++m_dwShadowMapBankUsage;
    return pTex;
}


//--------------------------------------------------------------------------------------
// Name: CreateShadowMapTexture()
// Desc: Creates a square shadow map texture.
//--------------------------------------------------------------------------------------
D3DTexture* SceneViewer::CreateShadowMapTexture()
{
    const D3DFORMAT DepthTextureFormat = D3DFMT_D24S8;
    D3DTexture* pTexture = NULL;
    DWORD dwLevelCount = 1;
    if( m_bMipShadowMaps )
    {
        DWORD dwSize = m_dwShadowMapSize;
        const DWORD dwSmallestMipSize = 32;
        while( dwSize > dwSmallestMipSize )
        {
            ++dwLevelCount;
            dwSize /= 2;
        }
    }
    m_pd3dDevice->CreateTexture( m_dwShadowMapSize, m_dwShadowMapSize, dwLevelCount, 0,
                                 DepthTextureFormat, NULL, &pTexture, NULL );

#ifdef PROFILE
    // Report texture to texture tracker
    PIXReportNewTexture( pTexture );
    PIXSetTextureName( pTexture, "SceneViewer::CreateShadowMapTexture" );
#endif
    return pTexture;
}


XMVECTOR g_vObjectPosWorld;
BOOL PointLightCloser( ATG::PointLight* pLightA, ATG::PointLight* pLightB )
{
    XMVECTOR vToA = pLightA->GetWorldPosition() - g_vObjectPosWorld;
    XMVECTOR vToB = pLightB->GetWorldPosition() - g_vObjectPosWorld;
    vToA = XMVector3Dot( vToA, vToA );
    vToB = XMVector3Dot( vToB, vToB );

    return vToA.x < vToB.x;
}


DWORD SceneViewer::SetupLighting( ATG::Model* pModel, const XMMATRIX matModelWorld, const XMMATRIX matModelInvWorld,
                                  const ATG::Bound& ModelBound )
{
    assert( pModel != NULL );
    PointLightPtrList PointLights;
    DWORD dwScenePointLightCount = m_ShadowSceneState.PointLights.Size();
    PointLights.reserve( dwScenePointLightCount );
    for( DWORD i = 0; i < dwScenePointLightCount; ++i )
    {
        if( m_ShadowSceneState.PointLights[i].TestFlag( ATG::Light::IsDisabled ) )
            continue;
        if( ModelBound.Collide( m_ShadowSceneState.PointLights[i].GetWorldBound() ) )
        {
            PointLights.push_back( &m_ShadowSceneState.PointLights[i] );
        }
    }
    if( m_bSortLightsByDistance )
    {
        // sort
        g_vObjectPosWorld = matModelWorld.r[3];
        std::sort( PointLights.begin(), PointLights.end(), PointLightCloser );
    }
    DWORD dwPointLightCount = PointLights.size();
    for( DWORD i = 0; i < g_dwMaxPointLightCount; ++i )
    {
        if( i >= dwPointLightCount )
        {
            /*
              m_SampleParameterPool.SetPointLightColor( i, XMVectorZero() );
              m_SampleParameterPool.SetPointLightPosition( i, XMVectorZero() );
             */
            continue;
        }
        ATG::PointLight* pLight = PointLights[i];
        XMVECTOR vColor = pLight->GetColor();
        vColor = XMVectorScale( vColor, m_fLightIntensityScale );
        m_SampleParameterPool.SetPointLightColor( i, vColor );
        XMVECTOR vObjLightPos;
        vObjLightPos = XMVector3TransformCoord( pLight->GetWorldPosition(), matModelInvWorld );
        vObjLightPos.w = 1.0f / ( pLight->GetWorldRange() * m_fLightRangeScale );
        m_SampleParameterPool.SetPointLightPosition( i, vObjLightPos );
    }

    SpotLightPtrList SpotLights;
    TexturePtrList SpotLightShadowMaps;
    DWORD dwSceneSpotLightCount = m_ShadowSceneState.SpotLights.Size();
    SpotLights.reserve( dwSceneSpotLightCount );
    SpotLightShadowMaps.reserve( dwSceneSpotLightCount );
    for( DWORD i = 0; i < dwSceneSpotLightCount; ++i )
    {
        ATG::SpotLight* pSpotLight = &m_ShadowSceneState.SpotLights[i];
        if( pSpotLight->TestFlag( ATG::Light::IsDisabled ) )
            continue;
        if( ModelBound.Collide( pSpotLight->GetWorldBound() ) )
        {
            SpotLights.push_back( pSpotLight );
            SpotLightShadowMaps.push_back( m_ShadowSceneState.SpotShadowMaps[ i ] );
        }
    }
    DWORD dwSpotLightCount = ( DWORD )SpotLights.size();
    for( DWORD i = 0; i < g_dwMaxSpotLightCount; ++i )
    {
        if( i >= dwSpotLightCount )
        {
            /*
              m_SampleParameterPool.SetSpotLightColor( i, XMVectorZero() );
              m_SampleParameterPool.SetSpotLightObjectDir( i, XMVectorZero() );
              m_SampleParameterPool.SetSpotLightPosition( i, XMQuaternionIdentity() );
              m_SampleParameterPool.SetSpotLightAngles( i, 0, 0 );
             */
            continue;
        }
        ATG::SpotLight* pSpotLight = SpotLights[i];
        XMVECTOR vColor = pSpotLight->GetColor();
        vColor = XMVectorScale( vColor, m_fLightIntensityScale );
        m_SampleParameterPool.SetSpotLightColor( i, vColor );
        XMVECTOR vObjLightPos;
        vObjLightPos = XMVector3TransformCoord( pSpotLight->GetWorldPosition(), matModelInvWorld );
        vObjLightPos.w = 1.0f / ( pSpotLight->GetWorldRange() * m_fLightRangeScale );
        m_SampleParameterPool.SetSpotLightPosition( i, vObjLightPos );
        XMVECTOR vObjLightDir;
        vObjLightDir = XMVector3TransformNormal( pSpotLight->GetWorldDirection(), matModelInvWorld );
        m_SampleParameterPool.SetSpotLightObjectDir( i, vObjLightDir );
        m_SampleParameterPool.SetSpotLightAngles( i, pSpotLight->GetOuterAngle(), pSpotLight->GetInnerAngle() );
        XMMATRIX matLightWVP = matModelWorld * pSpotLight->GetLightViewProjection();
        m_SampleParameterPool.SetSpotLightProjMatrix( i, matLightWVP * m_matShadowTextureOffset );
        m_SampleParameterPool.SetSpotLightShadowDepthTexture( i, SpotLightShadowMaps[i] );
    }

    DWORD dwDirLightCount = m_ShadowSceneState.DirLights.Size();
    DWORD dwShaderSlot = 0;
    DWORD dwDirLightIndex = 0;
    while( dwShaderSlot < g_dwMaxDirLightCount && dwDirLightIndex < dwDirLightCount )
    {
        ATG::DirectionalLight* pDirLight = &m_ShadowSceneState.DirLights[dwDirLightIndex];
        if( pDirLight->TestFlag( ATG::Light::IsDisabled ) )
        {
            ++dwDirLightIndex;
            continue;
        }
        XMVECTOR vColor = pDirLight->GetColor();
        vColor = XMVectorScale( vColor, m_fLightIntensityScale );
        m_SampleParameterPool.SetDirLightColor( dwShaderSlot, vColor );
        XMVECTOR vObjLightDir;
        vObjLightDir = XMVector3TransformNormal( pDirLight->GetWorldDirection(), matModelInvWorld );
        m_SampleParameterPool.SetDirLightObjectDir( dwShaderSlot, vObjLightDir );

        XMMATRIX matLightVPTight = pDirLight->GetLightViewProjection( 0 );
        XMMATRIX matLightWVPTight = matModelWorld * matLightVPTight;
        matLightWVPTight *= m_matShadowTextureOffset;
        XMMATRIX matLightVPScene = pDirLight->GetLightViewProjection( 1 );
        XMMATRIX matLightWVPScene = matModelWorld * matLightVPScene;
        matLightWVPScene *= m_matShadowTextureOffset;
        m_SampleParameterPool.SetDirLightProjMatrix( dwShaderSlot, matLightWVPTight, matLightWVPScene );
        m_SampleParameterPool.SetDirLightShadowDepthTexture( dwShaderSlot,
                                                             m_ShadowSceneState.DirShadowMaps[dwDirLightIndex * 2],
                                                             m_ShadowSceneState.DirShadowMaps[dwDirLightIndex * 2 +
                                                             1] );
        ++dwDirLightIndex;
        ++dwShaderSlot;
    }
    while( dwShaderSlot < g_dwMaxDirLightCount )
    {
        /*
          m_SampleParameterPool.SetDirLightColor( dwShaderSlot, XMVectorZero() );
          m_SampleParameterPool.SetDirLightObjectDir( dwShaderSlot, XMVectorZero() );
          m_SampleParameterPool.SetDirLightShadowDepthTexture( dwShaderSlot, NULL );
         */
        ++dwShaderSlot;
    }

    dwPointLightCount = min( dwPointLightCount, m_dwMaxPointLightsToSet );
    dwSpotLightCount = min( dwSpotLightCount, m_dwMaxSpotLightsToSet );
    dwDirLightCount = min( dwDirLightCount, m_dwMaxDirLightsToSet );

    if( m_RenderMode == SVRM_SHADERLIB )
    {
        m_hCurrentShaderLibTechnique = m_SampleParameterPool.GetShaderLibraryHandle( dwDirLightCount,
                                                                                     dwPointLightCount,
                                                                                     dwSpotLightCount );
    }
    else
    {
        m_SampleParameterPool.SetPointLightCount( dwPointLightCount );
        m_SampleParameterPool.SetSpotLightCount( dwSpotLightCount );
        m_SampleParameterPool.SetDirLightCount( dwDirLightCount );
    }

    return dwPointLightCount + dwSpotLightCount + dwDirLightCount;
}


VOID SceneViewer::RenderManualZPass()
{
    PIXBeginNamedEvent( 0xFF00FF00, "Manual Z Pass" );
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_CULLMODE, m_dwTriangleCullingMode );
    DWORD dwModelCount = m_ShadowSceneState.VisibleModels.size();
    PIXBeginNamedEvent( 0xFF00FF00, "Opaque Objects" );
    for( DWORD i = 0; i < dwModelCount; ++i )
    {
        ATG::Model* pModel = m_ShadowSceneState.VisibleModels[i];
        XMMATRIX matWVP = m_ShadowSceneState.VisibleModelWorldTransforms[i] * m_ShadowSceneState.matVP;
        ATG::SimpleShaders::BeginShader_Transformed_DepthOnly( matWVP );
        DWORD dwMeshMapCount = pModel->GetNumMeshMappings();
        for( DWORD dwMeshMap = 0; dwMeshMap < dwMeshMapCount; ++dwMeshMap )
        {
            ATG::MeshMapping& meshmap = pModel->GetMeshMapping( dwMeshMap );
            DWORD dwSubsetCount = meshmap.pMesh->GetNumSubsets();
            for( DWORD dwSubset = 0; dwSubset < dwSubsetCount; ++dwSubset )
            {
                ATG::MaterialInstance* pMaterial = meshmap.Materials[ dwSubset ];
                if( pMaterial != NULL && !pMaterial->IsTransparent() )
                    meshmap.pMesh->RenderSubset( dwSubset );
            }
        }
        ATG::SimpleShaders::EndShader();
    }
    PIXEndNamedEvent();
    if( m_bDrawTransparentObjects )
    {
        PIXBeginNamedEvent( 0xFF00FF00, "Transparent Objects" );
        m_pd3dDevice->SetRenderState( D3DRS_ALPHATESTENABLE, TRUE );
        m_pd3dDevice->SetRenderState( D3DRS_ALPHAFUNC, D3DCMP_GREATEREQUAL );
        m_pd3dDevice->SetRenderState( D3DRS_ALPHAREF, 0xfe );
        m_pd3dDevice->SetRenderState( D3DRS_COLORWRITEENABLE, 0 );
        m_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_NONE );
        dwModelCount = m_ShadowSceneState.VisibleTransparentModels.size();
        for( DWORD i = 0; i < dwModelCount; ++i )
        {
            ATG::Model* pModel = m_ShadowSceneState.VisibleTransparentModels[i];
            XMMATRIX matWVP = m_ShadowSceneState.VisibleTransparentModelWorldTransforms[i] * m_ShadowSceneState.matVP;
            DWORD dwMeshMapCount = pModel->GetNumMeshMappings();
            for( DWORD dwMeshMap = 0; dwMeshMap < dwMeshMapCount; ++dwMeshMap )
            {
                ATG::MeshMapping& meshmap = pModel->GetMeshMapping( dwMeshMap );
                DWORD dwSubsetCount = meshmap.pMesh->GetNumSubsets();
                for( DWORD dwSubset = 0; dwSubset < dwSubsetCount; ++dwSubset )
                {
                    ATG::MaterialInstance* pMaterial = meshmap.Materials[ dwSubset ];
                    if( pMaterial != NULL && pMaterial->IsTransparent() &&
                        pMaterial->GetBaseMaterial() != m_pLayeredBaseMaterial )
                    {
                        ATG::MaterialParameter& matParam = pMaterial->GetRawParameter( 0 );
                        if( matParam.Type == ATG::MaterialParameter::RPT_Texture2D && matParam.pValue != NULL )
                        {
                            ATG::Texture2D* pTexture = ( ATG::Texture2D* )matParam.pValue;
                            ATG::SimpleShaders::BeginShader_Transformed_Textured( matWVP, pTexture->GetD3DTexture() );
                            meshmap.pMesh->RenderSubset( dwSubset );
                            ATG::SimpleShaders::EndShader();
                        }
                    }
                }
            }
        }
        m_pd3dDevice->SetRenderState( D3DRS_ALPHATESTENABLE, FALSE );
        m_pd3dDevice->SetRenderState( D3DRS_COLORWRITEENABLE, 0xF );
        m_pd3dDevice->SetRenderState( D3DRS_CULLMODE, m_dwTriangleCullingMode );
        PIXEndNamedEvent();
    }
    PIXEndNamedEvent();
}


HRESULT SceneViewer::RenderDebugObjects()
{
    // Draw a ground plane.
    if( m_bDrawGroundPlane )
    {
        ATG::DebugDraw::DrawGrid( XMFLOAT3( 2, 0, 0 ), XMFLOAT3( 0, 0, 2 ), XMFLOAT3( 0, 0, 0 ), 40, 40, 0xFF202020 );
        ATG::DebugDraw::DrawGrid( XMFLOAT3( 10, 0, 0 ), XMFLOAT3( 0, 0, 10 ), XMFLOAT3( 0, 0, 0 ), 20, 20,
                                  0xFF202020 );
        ATG::DebugDraw::DrawAxes( XMMatrixIdentity() );
    }

    BOOL bDebugRender = m_bDisplayBounds ||
        m_bDisplayLights ||
        m_bDisplayFrames ||
        m_bDisplayCameras ||
        m_fBoneRadius > 0 ||
        ( m_dwDebugRenderMode == 4 || m_dwDebugRenderMode == 5 );

    if( !bDebugRender )
        return E_FAIL;

    ATG::Model* pIsolatedModel = NULL;
    if( m_iIsolatedModelIndex != -1 && m_ShadowSceneState.VisibleModels.size() == 1 )
    {
        pIsolatedModel = m_ShadowSceneState.VisibleModels[0];
    }

    ATG::NameIndexedCollection::iterator i;
    // Loop over all scene objects.
    for( i = m_pScene->GetInstanceList()->begin(); i != m_pScene->GetInstanceList()->end(); i++ )
    {
        // Draw model bounds.
        if( ( *i )->IsDerivedFrom( ATG::Model::TypeID ) )
        {
            ATG::Model* pModel = ( ATG::Model* )( *i );
            if( m_bDisplayBounds )
            {
                ATG::DebugDraw::DrawBound( pModel->GetWorldBound(), 0xFFFFFF00 );
            }
            if( m_dwDebugRenderMode == 4 || m_dwDebugRenderMode == 5 )
            {
                if( pIsolatedModel != NULL && pModel != pIsolatedModel )
                    continue;
                XMMATRIX matWVP = pModel->GetWorldTransform() * m_ShadowSceneState.matVP;
                ATG::BaseMesh* pMesh = pModel->GetMeshMapping( 0 ).pMesh;
                RenderMeshNormals( pMesh, matWVP );
            }
        }

        // Draw lights.
        if( ( *i )->IsDerivedFrom( ATG::Light::TypeID ) )
        {
            ATG::Light* pLight = ( ATG::Light* )( *i );
            if( m_bDisplayLights )
                DebugRenderLight( pLight );
            if( m_bDisplayBounds )
                ATG::DebugDraw::DrawBound( pLight->GetWorldBound(), 0xFFFF8000 );
        }

        // Draw frames.
        if( ( *i )->IsDerivedFrom( ATG::Frame::TypeID ) )
        {
            ATG::Frame* pFrame = ( ATG::Frame* )( *i );
            if( m_bDisplayFrames )
            {
                DebugRenderFrame( pFrame );
            }
            if( m_fBoneRadius > 0 )
            {
                DebugRenderBone( pFrame );
            }
        }

        // Draw cameras.
        if( m_bDisplayCameras && ( *i )->IsDerivedFrom( ATG::Camera::TypeID ) )
        {
            ATG::Camera* pCamera = ( ATG::Camera* )( *i );
            DebugRenderCamera( pCamera );
        }
    }
    return S_OK;
}


VOID SceneViewer::SetSamplerOverrides()
{
    if( m_dwTextureOverrideMode == 0 )
        return;

    D3DBaseTexture* pBlueTexture = m_pScene->GetResourceDatabase()->GetBlueTexture()->GetD3DTexture();
    D3DBaseTexture* pWhiteTexture = m_pScene->GetResourceDatabase()->GetWhiteTexture()->GetD3DTexture();
    D3DBaseTexture* pBlackTexture = m_pScene->GetResourceDatabase()->GetBlackTexture()->GetD3DTexture();
    assert( pWhiteTexture != NULL && pBlueTexture != NULL && pBlackTexture != NULL );

    D3DBaseTexture* pNormalMapOverride = NULL;
    D3DBaseTexture* pSpecularMapOverride = NULL;
    D3DBaseTexture* pShadowMapOverride = NULL;
    D3DBaseTexture* pDiffuseOverride = NULL;

    switch( m_dwTextureOverrideMode )
    {
        case 1:
            pNormalMapOverride = pBlackTexture;
            pShadowMapOverride = pBlackTexture;
            pDiffuseOverride = pBlackTexture;
            pSpecularMapOverride = pBlackTexture;
            break;
        case 2:
            pDiffuseOverride = pWhiteTexture;
            break;
        case 3:
            pDiffuseOverride = pWhiteTexture;
            pNormalMapOverride = pBlueTexture;
            break;
        case 4:
            pDiffuseOverride = pWhiteTexture;
            pNormalMapOverride = pBlueTexture;
            pShadowMapOverride = pWhiteTexture;
            pSpecularMapOverride = pWhiteTexture;
            break;
        case 5:
            pDiffuseOverride = pWhiteTexture;
            pShadowMapOverride = pWhiteTexture;
            break;
        case 6:
            pSpecularMapOverride = pBlackTexture;
            break;
    }

    if( pDiffuseOverride != NULL )
    {
        // diffuse maps are samplers 0-3
        for( DWORD i = 0; i < 4; ++i )
        {
            D3DBaseTexture* pExistingTexture = NULL;
            m_pd3dDevice->GetTexture( i, &pExistingTexture );
            if( pExistingTexture == NULL )
                continue;
            m_pd3dDevice->SetTexture( i, pDiffuseOverride );
            pExistingTexture->Release();
        }
    }

    if( pSpecularMapOverride != NULL )
    {
        // specular map is sampler 4
        for( DWORD i = 4; i < 5; ++i )
        {
            D3DBaseTexture* pExistingTexture = NULL;
            m_pd3dDevice->GetTexture( i, &pExistingTexture );
            if( pExistingTexture == NULL )
                continue;
            m_pd3dDevice->SetTexture( i, pSpecularMapOverride );
            pExistingTexture->Release();
        }
    }

    if( pNormalMapOverride != NULL )
    {
        // normal maps are samplers 5-7
        for( DWORD i = 5; i < 8; ++i )
        {
            D3DBaseTexture* pExistingTexture = NULL;
            m_pd3dDevice->GetTexture( i, &pExistingTexture );
            if( pExistingTexture == NULL )
                continue;
            m_pd3dDevice->SetTexture( i, pNormalMapOverride );
            pExistingTexture->Release();
        }
    }

    if( pShadowMapOverride != NULL )
    {
        // shadow maps are samplers 8-15
        for( DWORD i = 8; i < 16; ++i )
        {
            D3DBaseTexture* pExistingTexture = NULL;
            m_pd3dDevice->GetTexture( i, &pExistingTexture );
            if( pExistingTexture == NULL )
                continue;
            m_pd3dDevice->SetTexture( i, pShadowMapOverride );
            pExistingTexture->Release();
        }
    }
}


inline VOID FindElementOffsetAndType( D3DDECLUSAGE Usage, D3DVERTEXELEMENT9* pElements, DWORD* pdwOffset,
                                      DWORD* pdwType )
{
    *pdwOffset = ( DWORD )-1;
    assert( pElements != NULL );

    while( pElements->Stream != 0xFF )
    {
        if( pElements->Usage == Usage )
        {
            *pdwOffset = pElements->Offset;
            *pdwType = pElements->Type;
            return;
        }
        pElements++;
    }
}


inline VOID DecodeFloat3( XMFLOAT3* pFloat3, BYTE* pVertexData, DWORD dwOffset, DWORD dwType )
{
    assert( pVertexData != NULL && pFloat3 != NULL );
    assert( dwOffset != ( DWORD )-1 );
    switch( dwType )
    {
        case D3DDECLTYPE_FLOAT3:
            memcpy( pFloat3, pVertexData + dwOffset, 12 );
            return;
        case D3DDECLTYPE_DEC3N:
        {
            XMXDECN4 DecN4( *( DWORD* )( pVertexData + dwOffset ) );
            pFloat3->x = ( FLOAT )DecN4.x / 511.0f;
            pFloat3->y = ( FLOAT )DecN4.y / 511.0f;
            pFloat3->z = ( FLOAT )DecN4.z / 511.0f;
            return;
        }
    }
}


VOID SceneViewer::RenderMeshNormals( ATG::BaseMesh* pMesh, const XMMATRIX& matWVP )
{
    ATG::VertexData* pVertexData = pMesh->GetVertexData( 0 );

    if( pVertexData->GetNumVertexStreams() > 1 )
        return;

    DWORD dwVertexCount = pVertexData->GetNumVertices();

    LPDIRECT3DVERTEXDECLARATION9 pDecl = pVertexData->GetVertexDecl();

    D3DVERTEXELEMENT9 Elements[30];
    DWORD dwElementCount = 30;
    pDecl->GetDeclaration( Elements, ( UINT* )&dwElementCount );

    const DWORD dwVertexSize = D3DXGetDeclVertexSize( Elements, 0 );

    DWORD dwPositionOffset = 0;
    DWORD dwNormalOffset = 0;
    DWORD dwTangentOffset = 0;
    DWORD dwBinormalOffset = 0;

    DWORD dwPositionType = D3DDECLTYPE_FLOAT3;
    DWORD dwNormalType = D3DDECLTYPE_FLOAT3;
    DWORD dwTangentType = D3DDECLTYPE_FLOAT3;
    DWORD dwBinormalType = D3DDECLTYPE_FLOAT3;

    FindElementOffsetAndType( D3DDECLUSAGE_POSITION, Elements, &dwPositionOffset, &dwPositionType );
    FindElementOffsetAndType( D3DDECLUSAGE_NORMAL, Elements, &dwNormalOffset, &dwNormalType );
    FindElementOffsetAndType( D3DDECLUSAGE_TANGENT, Elements, &dwTangentOffset, &dwTangentType );
    FindElementOffsetAndType( D3DDECLUSAGE_BINORMAL, Elements, &dwBinormalOffset, &dwBinormalType );

    if( dwPositionOffset == ( DWORD )-1 || dwNormalOffset == ( DWORD )-1 )
        return;

    DWORD dwLineCountPerVertex = 1;
    if( dwTangentOffset != ( DWORD )-1 && dwBinormalOffset != ( DWORD )-1 )
        dwLineCountPerVertex = 3;

    LPDIRECT3DVERTEXBUFFER9 pSrcVB = pVertexData->GetVertexStream( 0 )->pVertexBuffer;
    assert( pSrcVB != NULL );
    BYTE* pSrcVBData = NULL;
    pSrcVB->Lock( 0, 0, ( VOID** )&pSrcVBData, D3DLOCK_READONLY );

    ATG::SimpleShaders::SetDeclPosColor();
    ATG::SimpleShaders::BeginShader_Transformed_VertexColor( matWVP );

    struct ThreeLines
    {
        XMFLOAT3 vPositionA0;
        D3DCOLOR ColorA0;
        XMFLOAT3 vPositionA1;
        D3DCOLOR ColorA1;

        XMFLOAT3 vPositionB0;
        D3DCOLOR ColorB0;
        XMFLOAT3 vPositionB1;
        D3DCOLOR ColorB1;

        XMFLOAT3 vPositionC0;
        D3DCOLOR ColorC0;
        XMFLOAT3 vPositionC1;
        D3DCOLOR ColorC1;
    };

    const DWORD dwDestStride = 16 * 2 * dwLineCountPerVertex;
    BYTE* pDestVBDataStart = new BYTE[ dwDestStride * dwVertexCount ];
    BYTE* pDestVBData = pDestVBDataStart;

    for( DWORD i = 0; i < dwVertexCount; i++ )
    {
        ThreeLines* pLineStruct = ( ThreeLines* )pDestVBData;

        XMVECTOR vPosition;
        XMVECTOR vLineEnd;

        XMFLOAT3 Position;
        DecodeFloat3( &Position, pSrcVBData, dwPositionOffset, dwPositionType );
        vPosition = XMLoadFloat3( &Position );
        XMFLOAT3 Normal;
        DecodeFloat3( &Normal, pSrcVBData, dwNormalOffset, dwNormalType );
        pLineStruct->vPositionA0 = Position;
        pLineStruct->ColorA0 = 0xFFFFFFFF;
        pLineStruct->ColorA1 = 0xFFFFFFFF;
        vLineEnd = XMVectorScale( XMLoadFloat3( &Normal ), m_fDebugNormalsScale );
        vLineEnd += vPosition;
        XMStoreFloat3( &pLineStruct->vPositionA1, vLineEnd );

        if( dwLineCountPerVertex == 3 )
        {
            pLineStruct->vPositionB0 = Position;
            pLineStruct->ColorB0 = 0xFFFF0000;
            pLineStruct->ColorB1 = 0xFFFF0000;
            pLineStruct->vPositionC0 = Position;
            pLineStruct->ColorC0 = 0xFF00FF00;
            pLineStruct->ColorC1 = 0xFF00FF00;

            XMFLOAT3 Tangent;
            DecodeFloat3( &Tangent, pSrcVBData, dwTangentOffset, dwTangentType );
            vLineEnd = XMVectorScale( XMLoadFloat3( &Tangent ), m_fDebugNormalsScale );
            vLineEnd += vPosition;
            XMStoreFloat3( &pLineStruct->vPositionB1, vLineEnd );

            XMFLOAT3 Binormal;
            DecodeFloat3( &Binormal, pSrcVBData, dwBinormalOffset, dwBinormalType );
            vLineEnd = XMVectorScale( XMLoadFloat3( &Binormal ), m_fDebugNormalsScale );
            vLineEnd += vPosition;
            XMStoreFloat3( &pLineStruct->vPositionC1, vLineEnd );
        }

        pSrcVBData += dwVertexSize;
        pDestVBData += dwDestStride;
    }

    DWORD dwLineVertexCount = dwVertexCount * 2 * dwLineCountPerVertex;
    BYTE* pCurrentVBPos = pDestVBDataStart;
    const DWORD dwBatchSize = 6000;
    do
    {
        DWORD dwBatchVertexCount = min( dwLineVertexCount, dwBatchSize );
        m_pd3dDevice->DrawPrimitiveUP( D3DPT_LINELIST, dwBatchVertexCount / 2, pCurrentVBPos, 16 );
        pCurrentVBPos += ( dwBatchSize * 16 );
        dwLineVertexCount -= dwBatchVertexCount;
    } while( dwLineVertexCount > 0 );
    ATG::SimpleShaders::EndShader();

    delete[] pDestVBDataStart;

    pSrcVB->Unlock();
}


VOID SceneViewer::DebugRenderText3D( const XMVECTOR vWorldPos, const WCHAR* strText, D3DCOLOR Color, FLOAT fSize )
{
    XMVECTOR vScreenPos = XMVector3TransformCoord( vWorldPos, m_ShadowSceneState.matVP );
    vScreenPos *= XMVectorSet( 0.5f, -0.5f, 1, 1 );
    vScreenPos += XMVectorSet( 0.5f, 0.5f, 0, 0 );
    vScreenPos *= XMVectorSet( ( FLOAT )m_d3dpp.BackBufferWidth, ( FLOAT )m_d3dpp.BackBufferHeight, 1, 1 );

    if( vScreenPos.y < 0 || vScreenPos.x < 0 || vScreenPos.z > 1 || vScreenPos.z < 0 )
        return;

    m_Font.SetWindow( 0, 0, m_d3dpp.BackBufferWidth, m_d3dpp.BackBufferHeight );
    m_Font.Begin();
    m_Font.SetScaleFactors( fSize, fSize );
    m_Font.DrawText( vScreenPos.x, vScreenPos.y, Color, strText );
    m_Font.End();
    m_Font.SetWindow( m_TitleSafeRect );
}


VOID SceneViewer::DebugRenderLight( ATG::Light* pLight )
{
    XMVECTOR vPos = pLight->GetWorldPosition();
    if( pLight->IsDerivedFrom( ATG::PointLight::TypeID ) )
    {
        ATG::PointLight* pPointLight = ( ATG::PointLight* )pLight;
        FLOAT fRange = pPointLight->GetWorldRange();
        fRange *= m_fLightRangeScale;
        XMFLOAT3 Pos;
        XMStoreFloat3( &Pos, vPos );
        ATG::DebugDraw::DrawSphere( Pos, fRange, 0xFF00FFFF );
    }
    else if( pLight->IsDerivedFrom( ATG::SpotLight::TypeID ) )
    {
        ATG::SpotLight* pSpotLight = ( ATG::SpotLight* )pLight;
        FLOAT fRange = pSpotLight->GetWorldRange();
        fRange *= m_fLightRangeScale;
        FLOAT fOuterAngle = pSpotLight->GetOuterAngle() * 0.5f;
        FLOAT fInnerAngle = pSpotLight->GetInnerAngle() * 0.5f;
        XMVECTOR vDir = pSpotLight->GetWorldDirection();

        FLOAT fTopRadius = 1000.0f;
        if( fOuterAngle < XM_PIDIV2 )
            fTopRadius = tanf( fOuterAngle ) * fRange;

        XMFLOAT3 Pos;
        XMStoreFloat3( &Pos, vPos );
        XMFLOAT3 Axis;
        XMStoreFloat3( &Axis, XMVectorScale( vDir, fRange ) );

        ATG::DebugDraw::DrawConeWireframe( Pos, Axis, 0.0f, fTopRadius, 0xFF00FFFF );

        if( fInnerAngle < fOuterAngle )
        {
            fTopRadius = 1000.0f;
            if( fInnerAngle < XM_PIDIV2 )
                fTopRadius = tanf( fInnerAngle ) * fRange;
            ATG::DebugDraw::DrawConeWireframe( Pos, Axis, 0.0f, fTopRadius, 0xFF008080 );
        }
    }
    DebugRenderText3D( vPos, pLight->GetName(), 0xFFFFC080, 0.6f );
}

VOID SceneViewer::DebugRenderFrame( ATG::Frame* pFrame )
{
    XMMATRIX matWorld = pFrame->GetWorldTransform();
    ATG::DebugDraw::DrawAxes( matWorld );
    DebugRenderText3D( pFrame->GetWorldPosition(), pFrame->GetName(), 0xFFFFC080, 0.6f );
}

VOID SceneViewer::DebugRenderBone( ATG::Frame* pFrame )
{
    assert( pFrame );
    if( pFrame->IsDerivedFrom( ATG::Camera::TypeID ) )
        return;
    if( pFrame->IsDerivedFrom( ATG::Light::TypeID ) )
        return;
    XMMATRIX matWorld = pFrame->GetWorldTransform();
    XMFLOAT3 Pos;
    XMVECTOR vBonePos = matWorld.r[3];
    XMStoreFloat3( &Pos, vBonePos );
    ATG::DebugDraw::DrawSphere( Pos, m_fBoneRadius, 0xFFFFFF80 );
    ATG::Frame* pParentFrame = pFrame->GetParent();
    if( pParentFrame != NULL )
    {
        XMMATRIX matParentWorld = pParentFrame->GetWorldTransform();
        XMVECTOR vParentPos = matParentWorld.r[3];
        XMVECTOR vConeAxis = vParentPos - vBonePos;
        XMFLOAT3 ConeAxis;
        XMStoreFloat3( &ConeAxis, vConeAxis );
        ATG::DebugDraw::DrawConeWireframe( Pos, ConeAxis, m_fBoneRadius, 0.0f, 0xFFFFFF80 );
    }
}

VOID SceneViewer::DebugRenderCamera( ATG::Camera* pCamera )
{
    // If we're rendering the current camera, return.
    if( pCamera == &m_ShadowSceneState.Camera )
        return;

    XMMATRIX matWorld = pCamera->GetWorldTransform();

    ATG::DebugDraw::DrawBound( pCamera->GetWorldBound(), 0xFFFF0000 );

    // Draw axes at camera location.
    ATG::DebugDraw::DrawAxes( matWorld );

    // Draw a little camera model.
    XMFLOAT3 CameraBodySize( 0.05f, 0.05f, 0.08f );
    XMMATRIX matCameraBody = XMMatrixScaling( CameraBodySize.x, CameraBodySize.y, CameraBodySize.z ) *
        XMMatrixTranslation( 0, 0, -CameraBodySize.z ) * matWorld;
    ATG::DebugDraw::DrawCubeWireframe( matCameraBody, 0xFFFFFFFF );
    XMVECTOR vCameraForward = pCamera->GetWorldDirection();
    XMVECTOR vCameraFocalPoint = pCamera->GetWorldPosition();
    XMFLOAT3 ConeBaseCenter, ConeAxis;
    XMStoreFloat3( &ConeBaseCenter, vCameraFocalPoint );
    XMStoreFloat3( &ConeAxis, XMVectorScale( vCameraForward, CameraBodySize.x ) );
    ATG::DebugDraw::DrawConeWireframe( ConeBaseCenter, ConeAxis, CameraBodySize.x * 0.6f, CameraBodySize.x,
                                       0xFFFFFFFF );

    // Draw the name of the camera.
    DebugRenderText3D( pCamera->GetWorldPosition(), pCamera->GetName(), 0xFFFFC080, 0.6f );
}

XMVECTOR EulerAnglesFromQuaternion( XMVECTOR vQuaternion )
{
    XMMATRIX mat = XMMatrixRotationQuaternion( vQuaternion );
    XMVECTOR vResult;

    FLOAT cy = sqrt( mat._11 * mat._11 + mat._21 * mat._21 );
    if( cy > ( 16 * 0.000001f ) )
    {
        vResult.x = atan2f( -mat._32, mat._33 );
        vResult.y = atan2f( mat._31, cy );
        vResult.z = atan2f( -mat._21, mat._11 );
    }
    else
    {
        vResult.x = atan2f( mat._23, mat._22 );
        vResult.y = atan2f( mat._31, cy );
        vResult.z = 0;
    }

    vResult *= -1;
    return vResult;
}

VOID SceneViewer::DebugRenderAnimationTrack( FLOAT fStartTime, FLOAT fDuration, ATG::AnimationTransformTrack* pTrack )
{
    DWORD dwSteps = ( m_TitleSafeRect.x2 - m_TitleSafeRect.x1 ) / 2;

    DWORD dwCount = ( dwSteps + 1 ) * 3;
    XMFLOAT3* pTrackDataPos = new XMFLOAT3[ dwCount ];
    XMFLOAT3* pTrackDataOrientation = new XMFLOAT3[ dwCount ];
    XMFLOAT3* pTrackDataScale = new XMFLOAT3[ dwCount ];
    ZeroMemory( pTrackDataPos, dwCount * sizeof( XMFLOAT3 ) );
    ZeroMemory( pTrackDataOrientation, dwCount * sizeof( XMFLOAT3 ) );
    ZeroMemory( pTrackDataScale, dwCount * sizeof( XMFLOAT3 ) );

    FLOAT fTime = fStartTime;
    FLOAT fDeltaTime = ( fDuration / ( FLOAT )dwSteps );
    DWORD dwIndexPos = 0;
    //DWORD dwIndexOrientation = 0;
    DWORD dwIndexScale = 0;

    XMVECTOR vMaxPos = XMVectorZero();
    XMVECTOR vMinPos = XMVectorZero();

    XMVECTOR vMinScale = XMVectorZero();
    XMVECTOR vMaxScale = XMVectorZero();

    FLOAT fScaleX = ( FLOAT )( m_TitleSafeRect.x2 - m_TitleSafeRect.x1 );
    FLOAT fOffsetX = ( FLOAT )( m_TitleSafeRect.x1 );

    for( DWORD i = 0; i <= dwSteps; i++ )
    {
        FLOAT fNormTime = ( fTime - fStartTime ) / fDuration;
        XMVECTOR vPos = pTrack->SamplePosition( fTime, &dwIndexPos );
        //XMVECTOR vOrientation = pTrack->SampleOrientation( fTime, &dwIndexOrientation );
        XMVECTOR vScale = pTrack->SampleScale( fTime, &dwIndexScale );
        //XMVECTOR vEulerAngles = EulerAnglesFromQuaternion( vOrientation );

        vMaxPos = XMVectorMaximize( vPos, vMaxPos );
        vMinPos = XMVectorMinimize( vPos, vMinPos );

        vMaxScale = XMVectorMaximize( vScale, vMaxScale );
        vMinScale = XMVectorMinimize( vScale, vMinScale );

        pTrackDataPos[ i * 3 ].x = fNormTime;
        pTrackDataPos[ i * 3 ].y = vPos.x;
        pTrackDataPos[ i * 3 + 1 ].x = fNormTime;
        pTrackDataPos[ i * 3 + 1 ].y = vPos.y;
        pTrackDataPos[ i * 3 + 2 ].x = fNormTime;
        pTrackDataPos[ i * 3 + 2 ].y = vPos.z;

        fTime += fDeltaTime;
    }

    FLOAT fSixthHeightY = ( FLOAT )( m_TitleSafeRect.y2 - m_TitleSafeRect.y1 ) / 6.0f;
    FLOAT fScaleY = fSixthHeightY * 2.0f;
    FLOAT fOffsetYPos = fSixthHeightY + ( FLOAT )m_TitleSafeRect.y1;
    //FLOAT fOffsetYOrientation = fSixthHeightY * 3.0f + (FLOAT)m_TitleSafeRect.y1;
    //FLOAT fOffsetYScale = fSixthHeightY * 5.0f + (FLOAT)m_TitleSafeRect.y1;

    m_pd3dDevice->SetRenderState( D3DRS_VIEWPORTENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, FALSE );
    {
        XMMATRIX matDisplayPos;
        FLOAT fRange = max( vMaxPos.x, -vMinPos.x );
        matDisplayPos.r[0] = XMVectorSet( fScaleX, 0, 0, 0 );
        matDisplayPos.r[1] = XMVectorSet( 0, fScaleY / fRange, 0, 0 );
        matDisplayPos.r[2] = XMVectorZero();
        matDisplayPos.r[3] = XMVectorSet( fOffsetX, fOffsetYPos, 0, 1 );

        ATG::SimpleShaders::BeginShader_Transformed_ConstantColor( matDisplayPos, 0xFFFF0000 );
        ATG::SimpleShaders::SetDeclPos();
        m_pd3dDevice->DrawPrimitiveUP( D3DPT_LINESTRIP, dwSteps, pTrackDataPos, sizeof( XMFLOAT3 ) * 3 );
        ATG::SimpleShaders::EndShader();

        ATG::SimpleShaders::BeginShader_Transformed_ConstantColor( matDisplayPos, 0xFF00FF00 );
        ATG::SimpleShaders::SetDeclPosT();
        m_pd3dDevice->DrawPrimitiveUP( D3DPT_LINESTRIP, dwSteps, pTrackDataPos + 1, sizeof( XMFLOAT3 ) * 3 );
        ATG::SimpleShaders::EndShader();

        ATG::SimpleShaders::BeginShader_Transformed_ConstantColor( matDisplayPos, 0xFF0000FF );
        ATG::SimpleShaders::SetDeclPosT();
        m_pd3dDevice->DrawPrimitiveUP( D3DPT_LINESTRIP, dwSteps, pTrackDataPos + 2, sizeof( XMFLOAT3 ) * 3 );
        ATG::SimpleShaders::EndShader();
    }
    /*
      {
      XMMATRIX matDisplayYPos;
      matDisplayYPos.r[0] = XMVectorZero();
      matDisplayYPos.r[1] = XMVectorSet( 0, fScaleY, 0, fOffsetYPos );
      matDisplayYPos.r[2] = XMVectorZero();
      matDisplayYPos.r[3] = XMVectorSet( fScaleX, 0, 0, fOffsetX );
      
      }
      {
      XMMATRIX matDisplayZPos;
      matDisplayZPos.r[0] = XMVectorZero();
      matDisplayZPos.r[1] = XMVectorZero();
      matDisplayZPos.r[2] = XMVectorSet( 0, fScaleY, 0, fOffsetYPos );
      matDisplayZPos.r[3] = XMVectorSet( fScaleX, 0, 0, fOffsetX );
      
      ATG::SimpleShaders::BeginShader_Transformed_ConstantColor( matDisplayZPos, 0xFF0000FF );
      ATG::SimpleShaders::SetDeclPosT();
      m_pd3dDevice->DrawPrimitiveUP( D3DPT_LINESTRIP, dwSteps, pTrackDataPos, sizeof( XMFLOAT4 ) );
      ATG::SimpleShaders::EndShader();
      }
     */
    m_pd3dDevice->SetRenderState( D3DRS_VIEWPORTENABLE, TRUE );

    delete[] pTrackDataPos;
    delete[] pTrackDataOrientation;
    delete[] pTrackDataScale;
}


VOID SceneViewer::RenderAllSpotlightShadowMaps()
{
    DWORD dwSpotCount = ( DWORD )m_ShadowSceneState.SpotLights.Size();
    dwSpotCount = min( dwSpotCount, g_dwMaxSpotLightCount );

    m_ShadowSceneState.SpotShadowMaps.ReserveSpace( dwSpotCount );
    m_ShadowSceneState.SpotShadowMaps.Clear();

    for( DWORD i = 0; i < dwSpotCount; ++i )
    {
        ATG::SpotLight* pSpot = &m_ShadowSceneState.SpotLights[i];
        // Skip spotlights that do not intersect the view frustum.
        if( pSpot->GetWorldBound().Collide( m_ShadowSceneState.Camera.GetWorldBound() ) == FALSE )
        {
            // Assign no shadow map texture to this spotlight.
            m_ShadowSceneState.SpotShadowMaps.PushBack( NULL );
            continue;
        }
        // Allocate a shadow map texture for this spotlight from the shadow map bank.
        D3DTexture* pShadowMapTexture = AllocateShadowMap( 0 );

        // Assign the shadow map to the spotlight.
        m_ShadowSceneState.SpotShadowMaps.PushBack( pShadowMapTexture );

        // Render the shadow map.
        if( pShadowMapTexture != NULL )
        {
            XMMATRIX matLightVP;
            RenderShadowMap( pSpot, pShadowMapTexture, matLightVP );
        }
    }
}


VOID SceneViewer::RenderAllDirLightShadowMaps()
{
    DWORD dwDirLightCount = m_ShadowSceneState.DirLights.Size();
    dwDirLightCount = min( dwDirLightCount, g_dwMaxDirLightCount );

    m_ShadowSceneState.DirShadowMaps.ReserveSpace( dwDirLightCount );
    m_ShadowSceneState.DirShadowMaps.Clear();

    for( DWORD i = 0; i < dwDirLightCount; ++i )
    {
        ATG::DirectionalLight* pDirLight = &m_ShadowSceneState.DirLights[i];

        D3DTexture* pShadowMapTexture = AllocateShadowMap( 0 );
        m_ShadowSceneState.DirShadowMaps.PushBack( pShadowMapTexture );
        if( pShadowMapTexture != NULL )
        {
            XMMATRIX matLightVP;
            RenderShadowMap( pDirLight, TRUE, pShadowMapTexture, matLightVP );
        }
        pShadowMapTexture = AllocateShadowMap( 0 );
        m_ShadowSceneState.DirShadowMaps.PushBack( pShadowMapTexture );
        if( pShadowMapTexture != NULL )
        {
            XMMATRIX matLightVP;
            RenderShadowMap( pDirLight, FALSE, pShadowMapTexture, matLightVP );
        }
    }
}


VOID SceneViewer::CreateShadowMapMips( D3DTexture* pDepthTexture )
{
    PIXBeginNamedEvent( 0, "Shadow Map Mips" );

    D3DSurface* pBaseSurface = NULL;
    pDepthTexture->GetSurfaceLevel( 0, &pBaseSurface );
    D3DSURFACE_DESC SurfDesc;
    pBaseSurface->GetDesc( &SurfDesc );
    pBaseSurface->Release();

    m_pd3dDevice->SetRenderTarget( 0, NULL );
    m_pd3dDevice->SetDepthStencilSurface( m_pShadowMapRenderTarget );
    m_pd3dDevice->SetRenderState( D3DRS_VIEWPORTENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_ZFUNC, D3DCMP_ALWAYS );
    ATG::SimpleShaders::SetDeclPosTex();

    DWORD dwResolveLevel = 1;
    while( SurfDesc.Width > 32 )
    {
        SurfDesc.Width /= 2;
        FLOAT fWidth = ( FLOAT )SurfDesc.Width;
        ATG::SimpleShaders::BeginShader_PreTransformed_DownsampleDepth( pDepthTexture );

        m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MINMIPLEVEL, dwResolveLevel - 1 );
        m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MAXMIPLEVEL, dwResolveLevel - 1 );

        ATG::MeshVertexPT* pVerts = NULL;
        m_pd3dDevice->BeginVertices( D3DPT_RECTLIST, 3, ATG::MeshVertexPT::Size(), ( VOID** )&pVerts );
        pVerts[0].Position = XMFLOAT3( 0, 0, 0 );
        pVerts[1].Position = XMFLOAT3( fWidth, 0, 0 );
        pVerts[2].Position = XMFLOAT3( 0, fWidth, 0 );
        pVerts[0].TexCoord = XMFLOAT2( 0, 0 );
        pVerts[1].TexCoord = XMFLOAT2( 1, 0 );
        pVerts[2].TexCoord = XMFLOAT2( 0, 1 );
        m_pd3dDevice->EndVertices();
        ATG::SimpleShaders::EndShader();

        D3DRECT Rect = { 0, 0, SurfDesc.Width, SurfDesc.Width };

        m_pd3dDevice->Resolve( D3DRESOLVE_DEPTHSTENCIL, &Rect, pDepthTexture, NULL, dwResolveLevel, 0, NULL, 0, 0,
                               NULL );
        dwResolveLevel++;
    }

    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MINMIPLEVEL, 13 );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MAXMIPLEVEL, 0 );
    m_pd3dDevice->SetRenderState( D3DRS_VIEWPORTENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_ZFUNC, D3DCMP_LESSEQUAL );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MIPMAPLODBIAS, 0 );

    PIXEndNamedEvent();
}


VOID SceneViewer::RenderShadowMap( ATG::SpotLight* pSourceLight, D3DTexture* pDestTexture, XMMATRIX& matLightVP )
{
    assert( pSourceLight != NULL );
    assert( pDestTexture != NULL );

    // If there are no visible models, then making a shadow map is rather pointless.
    if( m_ShadowSceneState.VisibleModels.size() == 0 )
        return;

    PIXBeginNamedEvent( 0xFF404040, "Shadow Map (Spot Light)" );
    // Compute the light view-projection matrix.
    XMMATRIX matLightView = pSourceLight->GetLightView();
    XMMATRIX matLightProj = pSourceLight->GetLightProjection();
    matLightVP = matLightView * matLightProj;

    // Cache the light's world bounding volume (a frustum).
    ATG::Bound LightBound = pSourceLight->GetWorldBound();

    // Set up the viewports and rendertargets.
    m_pd3dDevice->SetViewport( &m_ShadowViewport );
    m_pd3dDevice->SetRenderTarget( 0, NULL );
    m_pd3dDevice->SetRenderTarget( 1, NULL );
    m_pd3dDevice->SetRenderTarget( 2, NULL );
    m_pd3dDevice->SetRenderTarget( 3, NULL );
    m_pd3dDevice->SetDepthStencilSurface( m_pShadowMapRenderTarget );

    // Clear the depth/stencil.
    m_pd3dDevice->Clear( 0, NULL, D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL, 0, 1.0f, 0 );

    // Set some render state.
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_STENCILENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_NONE );
    m_pd3dDevice->SetRenderState( D3DRS_ZFUNC, D3DCMP_LESSEQUAL );
    m_pd3dDevice->SetRenderState( D3DRS_COLORWRITEENABLE, 0 );
    m_pd3dDevice->SetRenderState( D3DRS_SLOPESCALEDEPTHBIAS, ATG::FtoDW( m_fShadowSlopedDepthBias ) );

    // Loop over all models in the scene, not just the visible models.  There could be
    // a model that is outside the view frustum that is casting a shadow into the view
    // frustum.
    DWORD dwModelCount = m_ShadowSceneState.AllModels.size();
    for( DWORD dwModelIndex = 0; dwModelIndex < dwModelCount; ++dwModelIndex )
    {
        // Check if the model intersects with the light frustum.
        if( m_ShadowSceneState.AllModelWorldBounds[dwModelIndex].Collide( LightBound ) == FALSE )
            continue;

        ATG::Model* pModel = m_ShadowSceneState.AllModels[dwModelIndex];
        DWORD dwFlags = pModel->GetMeshMappingFlagUnion();
        if( ( dwFlags & ATG::MeshMapping::IsShadowCaster ) == 0 )
            continue;

        // Start rendering with a depth-only shader (NULL pixel shader).
        XMMATRIX matWVP = m_ShadowSceneState.AllModelWorldTransforms[dwModelIndex] * matLightVP;
        ATG::SimpleShaders::BeginShader_Transformed_DepthOnly( matWVP );

        // Iterate over mesh subsets.
        DWORD dwMappingCount = pModel->GetNumMeshMappings();
        for( DWORD dwMappingIndex = 0; dwMappingIndex < dwMappingCount; ++dwMappingIndex )
        {
            ATG::MeshMapping& meshmap = pModel->GetMeshMapping( dwMappingIndex );
            DWORD dwSubsetCount = meshmap.pMesh->GetNumSubsets();
            for( DWORD dwSubsetIndex = 0; dwSubsetIndex < dwSubsetCount; ++dwSubsetIndex )
            {
                ATG::MaterialInstance* pMaterial = meshmap.Materials[dwSubsetIndex];
                if( pMaterial != NULL && !pMaterial->IsTransparent() )
                    meshmap.pMesh->RenderSubset( dwSubsetIndex );
            }
        }
        ATG::SimpleShaders::EndShader();
        ++m_dwModelsRenderedToShadowMaps;
    }

    // Resolve rendertarget to the shadow map texture.
    m_pd3dDevice->Resolve( D3DRESOLVE_DEPTHSTENCIL, NULL, pDestTexture, NULL,
                           0, 0, NULL, 1.0f, 0, NULL );

    // Restore renderstate.
    m_pd3dDevice->SetRenderState( D3DRS_SLOPESCALEDEPTHBIAS, 0 );
    m_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_CCW );
    m_pd3dDevice->SetRenderState( D3DRS_COLORWRITEENABLE, 0xF );
    m_pd3dDevice->SetViewport( &m_Viewport );

    if( m_bMipShadowMaps )
        CreateShadowMapMips( pDestTexture );

    PIXEndNamedEvent();

    ++m_dwShadowMapsRendered;
}



VOID SceneViewer::RenderShadowMap( ATG::DirectionalLight* pDirLight, BOOL bTightShadow, D3DTexture* pDestTexture,
                                   XMMATRIX& matLightVP )
{
    assert( pDirLight != NULL );
    assert( pDestTexture != NULL );

    // If there are no visible models, then making a shadow map is rather pointless.
    if( m_ShadowSceneState.VisibleModels.size() == 0 )
        return;

    // Get the entire scene's center point and radius.
    FLOAT fSceneRadius = m_ShadowSceneState.EntireSceneBounds.GetMaxRadius();
    if( fSceneRadius <= 0.0f )
        return;

    XMVECTOR vShadowCenter;
    FLOAT fShadowRadius = 1.0f;
    FLOAT fShadowDepth = 1.0f;
    if( bTightShadow )
    {
        const FLOAT fTightRadius = m_fTightDirShadowRadius;
        XMVECTOR vCameraPos = m_ShadowSceneState.Camera.GetWorldPosition();
        XMVECTOR vCameraDir = m_ShadowSceneState.Camera.GetWorldDirection();
        vShadowCenter = vCameraPos + ( vCameraDir * fTightRadius );
        fShadowDepth = fSceneRadius * 2.0f;
        fShadowRadius = fTightRadius;
    }
    else
    {
        XMFLOAT3 SceneCenter = m_ShadowSceneState.EntireSceneBounds.GetCenter();
        XMVECTOR vSceneCenter = XMLoadFloat3( &SceneCenter );
        vShadowCenter = vSceneCenter;
        fShadowRadius = fSceneRadius;
        fShadowDepth = fSceneRadius * 2.0f;
    }

    PIXBeginNamedEvent( 0xFF404040, "Shadow Map (Directional Light)" );

    // Get the directional light's world direction.
    XMVECTOR vWorldDir = pDirLight->GetWorldDirection();

    // Create an orthographic projection matrix for the light.
    FLOAT fZNear = 0.1f;
    FLOAT fZFar = fZNear + fShadowDepth;
    FLOAT fShadowProjSize = fShadowRadius*2;
    XMMATRIX matProj = XMMatrixOrthographicLH( fShadowProjSize, fShadowProjSize, fZNear, fZFar );

    // Create a view matrix on the scene.
    XMVECTOR vCameraPos = vShadowCenter - ( vWorldDir * ( fShadowDepth * 0.5f ) );
    static const XMVECTOR vUp = { 0, 1, 0, 0 };
    XMMATRIX matView = XMMatrixLookToLH( vCameraPos, vWorldDir, vUp );

    // Restrict the shadow map frustum to move in texel increments, this removes shadow flickering
    // caused by camera movement
    if( m_bReduceShadowShimmer )
    {
        if( fShadowProjSize > 0.0f && m_dwShadowMapSize > 0 )
        {
            FLOAT fWorldUnitsPerTexel = fShadowProjSize / m_dwShadowMapSize;
            matView._41 = floorf( matView._41 / fWorldUnitsPerTexel ) * fWorldUnitsPerTexel;
            matView._42 = floorf( matView._42 / fWorldUnitsPerTexel ) * fWorldUnitsPerTexel;
        }
    }

    matLightVP = matView * matProj;
    pDirLight->SetLightViewProjection( bTightShadow ? 0 : 1, matLightVP );

    // Set up the viewports and rendertargets.
    m_pd3dDevice->SetViewport( &m_ShadowViewport );
    m_pd3dDevice->SetRenderTarget( 0, NULL );
    m_pd3dDevice->SetRenderTarget( 1, NULL );
    m_pd3dDevice->SetRenderTarget( 2, NULL );
    m_pd3dDevice->SetRenderTarget( 3, NULL );
    m_pd3dDevice->SetDepthStencilSurface( m_pShadowMapRenderTarget );

    // Clear the depth/stencil.
    m_pd3dDevice->Clear( 0, NULL, D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL, 0, 1.0f, 0 );

    // Set some render state.
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_STENCILENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_NONE );
    m_pd3dDevice->SetRenderState( D3DRS_ZFUNC, D3DCMP_LESSEQUAL );
    m_pd3dDevice->SetRenderState( D3DRS_COLORWRITEENABLE, 0 );
    m_pd3dDevice->SetRenderState( D3DRS_SLOPESCALEDEPTHBIAS, ATG::FtoDW( m_fShadowSlopedDepthBias ) );

    // Loop over all models in the scene, not just the visible models.  There could be
    // a model that is outside the view frustum that is casting a shadow into the view
    // frustum.
    DWORD dwModelCount = m_ShadowSceneState.AllModels.size();
    for( DWORD dwModelIndex = 0; dwModelIndex < dwModelCount; ++dwModelIndex )
    {
        ATG::Model* pModel = m_ShadowSceneState.AllModels[dwModelIndex];
        DWORD dwFlags = pModel->GetMeshMappingFlagUnion();
        if( ( dwFlags & ATG::MeshMapping::IsShadowCaster ) == 0 )
            continue;

        // Start rendering with a depth-only shader (NULL pixel shader).
        XMMATRIX matWVP = m_ShadowSceneState.AllModelWorldTransforms[dwModelIndex] * matLightVP;
        ATG::SimpleShaders::BeginShader_Transformed_DepthOnly( matWVP );

        // Iterate over mesh subsets.
        DWORD dwMappingCount = pModel->GetNumMeshMappings();
        for( DWORD dwMappingIndex = 0; dwMappingIndex < dwMappingCount; ++dwMappingIndex )
        {
            ATG::MeshMapping& meshmap = pModel->GetMeshMapping( dwMappingIndex );
            DWORD dwSubsetCount = meshmap.pMesh->GetNumSubsets();
            for( DWORD dwSubsetIndex = 0; dwSubsetIndex < dwSubsetCount; ++dwSubsetIndex )
            {
                ATG::MaterialInstance* pMaterial = meshmap.Materials[dwSubsetIndex];
                if( pMaterial != NULL && !pMaterial->IsTransparent() )
                    meshmap.pMesh->RenderSubset( dwSubsetIndex );
            }
        }
        ATG::SimpleShaders::EndShader();
        ++m_dwModelsRenderedToShadowMaps;
    }

    // Resolve rendertarget to the shadow map texture.
    m_pd3dDevice->Resolve( D3DRESOLVE_DEPTHSTENCIL, NULL, pDestTexture, NULL,
                           0, 0, NULL, 1.0f, 0, NULL );

    // Restore renderstate.
    m_pd3dDevice->SetRenderState( D3DRS_SLOPESCALEDEPTHBIAS, 0 );
    m_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_CCW );
    m_pd3dDevice->SetRenderState( D3DRS_COLORWRITEENABLE, 0xF );
    m_pd3dDevice->SetViewport( &m_Viewport );

    if( m_bMipShadowMaps )
        CreateShadowMapMips( pDestTexture );

    PIXEndNamedEvent();

    ++m_dwShadowMapsRendered;
}

VOID SceneViewer::BuildPointLightRectCorners( ATG::PointLight* pLight, XMVECTOR3& vCorners )
{
    // Extract light position and radius from point light
    XMVECTOR vPos = pLight->GetWorldPosition();
    FLOAT fRadius = pLight->GetWorldRange() * m_fLightRangeScale;

    // Build vectors for the quad axes and the camera towards
    XMVECTOR vQuadTop = XMVector3Normalize( m_ShadowSceneState.Camera.GetWorldUp() );
    vQuadTop *= fRadius;
    XMVECTOR vQuadLeft = XMVector3Normalize( m_ShadowSceneState.Camera.GetWorldRight() );
    vQuadLeft *= -fRadius;
    XMVECTOR vTowardsCamera = XMVector3Normalize( m_ShadowSceneState.Camera.GetWorldPosition() -
                                                  pLight->GetWorldPosition() );
    vTowardsCamera *= fRadius;
    vPos += vTowardsCamera;

    // Build the quad corners
    vCorners.v[0] = vPos + vQuadLeft + vQuadTop;
    vCorners.v[1] = vPos - vQuadLeft + vQuadTop;
    vCorners.v[2] = vPos + vQuadLeft - vQuadTop;

    // Clamp the quad corners to view space ( -1, 1 ) - ( 1, -1 )
    // Rect drawing does not perform clipping, so we have to do it ourselves.
    const XMVECTOR vMin = XMVectorSet( -1, -1, 0, 1 );
    const XMVECTOR vMax = XMVectorSet( 1, 1, 0, 1 );
    vCorners.v[0] = XMVectorClamp( XMVector3TransformCoord( vCorners.v[0], m_ShadowSceneState.matVP ), vMin, vMax );
    vCorners.v[1] = XMVectorClamp( XMVector3TransformCoord( vCorners.v[1], m_ShadowSceneState.matVP ), vMin, vMax );
    vCorners.v[2] = XMVectorClamp( XMVector3TransformCoord( vCorners.v[2], m_ShadowSceneState.matVP ), vMin, vMax );

    // Unify the X and Y positions of the corners.
    // If the coordinates are not exactly equivalent, rect drawing will fail.
    vCorners.v[0] = XMVectorSelect( vCorners.v[0], vCorners.v[2], XMVectorSelectControl( 1, 0, 0, 0 ) );
    vCorners.v[0] = XMVectorSelect( vCorners.v[0], vCorners.v[1], XMVectorSelectControl( 0, 1, 0, 0 ) );
}


VOID SceneViewer::BuildFrustumCorners( const ATG::Frustum& frustum, const XMMATRIX& matTransform, XMVECTOR8& vCorners )
{
    /*
      FLOAT Near = frustum.Near;
      FLOAT Far = frustum.Far;
      FLOAT RightSlope = frustum.RightSlope;
      FLOAT LeftSlope = frustum.LeftSlope;
      FLOAT TopSlope = frustum.TopSlope;
      FLOAT BottomSlope = frustum.BottomSlope;
     */

    XMVECTOR Origin = XMLoadFloat3( &frustum.Origin );
    XMVECTOR vNear = XMVectorReplicate( frustum.Near );
    XMVECTOR vFar = XMVectorReplicate( frustum.Far );
    XMVECTOR vTopSlopes = XMVectorSet( frustum.TopSlope, frustum.LeftSlope, frustum.RightSlope, 1 );
    XMVECTOR vBottomSlopes = XMVectorSet( frustum.BottomSlope, frustum.LeftSlope, frustum.RightSlope, 1 );

    /*
      CornerPoints[0] = XMFLOAT3( RightSlope * Near, TopSlope    * Near, Near );
      CornerPoints[1] = XMFLOAT3( LeftSlope  * Near, TopSlope    * Near, Near );
      CornerPoints[2] = XMFLOAT3( LeftSlope  * Near, BottomSlope * Near, Near );
      CornerPoints[3] = XMFLOAT3( RightSlope * Near, BottomSlope * Near, Near );
      
      CornerPoints[4] = XMFLOAT3( RightSlope * Far, TopSlope    * Far, Far );
      CornerPoints[5] = XMFLOAT3( LeftSlope  * Far, TopSlope    * Far, Far );
      CornerPoints[6] = XMFLOAT3( LeftSlope  * Far, BottomSlope * Far, Far );
      CornerPoints[7] = XMFLOAT3( RightSlope * Far, BottomSlope * Far, Far );
     */

    vCorners.v[0] = vNear * XMVectorSwizzle( vTopSlopes, 2, 0, 3, 3 );
    vCorners.v[1] = vNear * XMVectorSwizzle( vTopSlopes, 1, 0, 3, 3 );
    vCorners.v[2] = vNear * XMVectorSwizzle( vBottomSlopes, 1, 0, 3, 3 );
    vCorners.v[3] = vNear * XMVectorSwizzle( vBottomSlopes, 2, 0, 3, 3 );

    vCorners.v[4] = vFar * XMVectorSwizzle( vTopSlopes, 2, 0, 3, 3 );
    vCorners.v[5] = vFar * XMVectorSwizzle( vTopSlopes, 1, 0, 3, 3 );
    vCorners.v[6] = vFar * XMVectorSwizzle( vBottomSlopes, 1, 0, 3, 3 );
    vCorners.v[7] = vFar * XMVectorSwizzle( vBottomSlopes, 2, 0, 3, 3 );

    XMVECTOR Orientation = XMLoadFloat4( &frustum.Orientation );
    XMMATRIX Mat = XMMatrixRotationQuaternion( Orientation );
    for( UINT i = 0; i < 8; i++ )
    {
        XMVECTOR Result = XMVector3Transform( vCorners.v[i], Mat );
        vCorners.v[i] = XMVectorAdd( Result, Origin );
    }
}
