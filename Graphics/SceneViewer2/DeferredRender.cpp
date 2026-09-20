//--------------------------------------------------------------------------------------
// DeferredRender.cpp
//
// This file contains all of SceneViewer2's render functions that perform deferred
// rendering.  SceneViewer2.h is the header file.
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "SceneViewer2.h"

HRESULT SceneViewer::RenderSceneDeferred()
{
    if( !IsSceneRenderable() )
        return E_FAIL;

    PIXBeginNamedEvent( 0xFFFFFF00, "Deferred Construction" );
    // Setup some basic renderstate, in case the shaders don't do this.
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_FILLMODE, D3DFILL_SOLID );
    m_pd3dDevice->SetRenderState( D3DRS_CULLMODE, m_dwTriangleCullingMode );
    m_pd3dDevice->SetRenderState( D3DRS_VIEWPORTENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_ALL );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );

    FXLHANDLE hTechniqueBuild = m_DeferredParameterPool.GetTechniqueBufferConstruction();

    static ATG::StringID strDiffuseParamName( L"DiffuseTexture" );
    static ATG::StringID strNormalMapParamName( L"NormalMapTexture" );
    static ATG::StringID strSpecularMapParamName( L"SpecularMapTexture" );

    // Render models front to back.
    DWORD dwModelCount = m_ShadowSceneState.VisibleModels.size();
    for( DWORD dwModelIndex = 0; dwModelIndex < dwModelCount; ++dwModelIndex )
    {
        ATG::Model* pModel = m_ShadowSceneState.VisibleModels[dwModelIndex];
        XMMATRIX matWorld = m_ShadowSceneState.VisibleModelWorldTransforms[dwModelIndex];
        m_DeferredParameterPool.SetWorldMatrix( matWorld );
        m_DeferredParameterPool.SetWorldViewProjMatrix( matWorld * m_ShadowSceneState.matVP );
        DWORD dwMeshMappingCount = pModel->GetNumMeshMappings();
        for( DWORD dwMapIndex = 0; dwMapIndex < dwMeshMappingCount; ++dwMapIndex )
        {
            ATG::MeshMapping& mapping = pModel->GetMeshMapping( dwMapIndex );
            ATG::BaseMesh* pMesh = mapping.pMesh;
            DWORD dwSubsetCount = pMesh->GetNumSubsets();
            for( DWORD dwSubsetIndex = 0; dwSubsetIndex < dwSubsetCount; ++dwSubsetIndex )
            {
                ATG::MaterialInstance* pMaterial = mapping.Materials[dwSubsetIndex];
                if( pMaterial->GetBaseMaterial() != m_pDeferredBaseMaterial )
                    continue;
                pMaterial->BeginMaterial( m_pd3dDevice, hTechniqueBuild );
                pMaterial->BeginPass( 0 );
                // Wireframe override - overrides renderstate set in shader.
                if( m_bWireframe )
                {
                    m_pd3dDevice->SetRenderState( D3DRS_FILLMODE, D3DFILL_WIREFRAME );
                }
                SetSamplerOverrides();
                pMesh->RenderSubset( dwSubsetIndex );
                pMaterial->EndPass();
                pMaterial->EndMaterial();
                if( m_bCapturePerfData )
                {
                    m_dwPrimitivesRendered += pMesh->GetSubsetDesc( dwSubsetIndex )->GetNumPrimitives();
                    ++m_dwSubsetsRendered;
                }
            }
        }
    }
    m_dwModelsRendered = dwModelCount;
    PIXEndNamedEvent();

    return S_OK;
}


HRESULT SceneViewer::RenderLightsDeferred()
{
    PIXBeginNamedEvent( 0xFFFFFF80, "Deferred Lighting" );
    // Clear deferred lighting render target to black.
    m_pd3dDevice->Clear( 0, NULL, D3DCLEAR_TARGET0, 0, 1.0f, 0 );

    // Set the deferred buffers as textures.
    m_DeferredParameterPool.SetDepthBufferTexture( m_pDepthTexture );
    m_DeferredParameterPool.SetColorBufferTexture( m_pDeferredColorBuffer );
    m_DeferredParameterPool.SetNormalBufferTexture( m_pDeferredNormalBuffer );

    static const FLOAT fLightRect[15] =
    {
        -1,  1, 0, 0,  0,
        1,  1, 0, 1,  0,
        -1, -1, 0, 0,  1
    };

    assert( m_pDeferredBaseMaterial != NULL );
    ATG::FXLiteMaterialImplementation* pFXLiteMaterialImpl = ( ATG::FXLiteMaterialImplementation* )
        m_pDeferredBaseMaterial->GetMaterialImplementation();
    FXLEffect* pEffect = pFXLiteMaterialImpl->m_pEffect;
    pEffect->ChangeDevice( m_pd3dDevice );

    // Render one pass for ambient lighting.
    FXLHANDLE hAmbientTechnique = m_DeferredParameterPool.GetTechniqueAmbientLight();
    pEffect->BeginTechnique( hAmbientTechnique, 0 );
    pEffect->BeginPassFromIndex( 0 );
    XMVECTOR vAmbient = XMVectorReplicate( m_fAmbient );
    if( m_pScene == NULL || m_bEnableLighting == FALSE )
        vAmbient = XMVectorReplicate( 1.0f );
    m_DeferredParameterPool.SetLightColor( vAmbient );
    pEffect->Commit();
    ATG::SimpleShaders::SetDeclPosTex();
    m_pd3dDevice->DrawPrimitiveUP( D3DPT_RECTLIST, 1, fLightRect, 5 * sizeof( FLOAT ) );
    pEffect->EndPass();
    pEffect->EndTechnique();

    // If the scene is not loaded, we can stop here.
    if( !IsSceneRenderable() || m_bEnableLighting == FALSE )
    {
        PIXEndNamedEvent();
        return S_OK;
    }

    // Grab the camera frustum world bound.
    ATG::Bound CameraBound = m_ShadowSceneState.Camera.GetWorldBound();

    // Compute the inverse camera matrix for use in the lighting shaders.
    XMVECTOR vDet;
    XMMATRIX matInvViewProj = XMMatrixInverse( &vDet, m_ShadowSceneState.matVP );
    m_DeferredParameterPool.SetInvViewProjMatrix( matInvViewProj );

    m_DeferredParameterPool.SetWorldViewProjMatrix( m_ShadowSceneState.matVP );
    m_DeferredParameterPool.SetWorldViewDirection( m_ShadowSceneState.Camera.GetWorldDirection() );

    // Render one pass over the scene for each spotlight.
    FXLHANDLE hSpotTechnique = m_DeferredParameterPool.GetTechniqueSpotLight();
    DWORD dwSpotLightCount = m_ShadowSceneState.SpotLights.Size();
    for( DWORD i = 0; i < dwSpotLightCount; ++i )
    {
        ATG::SpotLight* pSpot = &m_ShadowSceneState.SpotLights[i];
        if( pSpot->TestFlag( ATG::Light::IsDisabled ) )
            continue;
        ++m_dwActiveLights;
        if( CameraBound.Collide( pSpot->GetWorldBound() ) == FALSE )
            continue;
        ++m_dwLightInfluences;

        // Render spotlight shadow map if necessary.
        D3DTexture* pDepthTexture = m_ShadowSceneState.SpotShadowMaps[i];
        XMMATRIX matLightVP = pSpot->GetLightViewProjection();
        m_DeferredParameterPool.SetShadowParameters( pDepthTexture, matLightVP * m_matShadowTextureOffset );

        XMVECTOR vPos = pSpot->GetWorldPosition();
        vPos.w = pSpot->GetWorldRange() * m_fLightRangeScale;
        m_DeferredParameterPool.SetLightColor( pSpot->GetColor() * m_fLightIntensityScale );
        m_DeferredParameterPool.SetLightWorldPosRange( vPos );
        m_DeferredParameterPool.SetLightWorldDir( pSpot->GetWorldDirection() );
        m_DeferredParameterPool.SetSpotLightAngles( pSpot->GetOuterAngle(), pSpot->GetInnerAngle() );
        ATG::SimpleShaders::SetDeclPos();
        pEffect->BeginTechnique( hSpotTechnique, 0 );
        pEffect->BeginPassFromIndex( 0 );
        pEffect->Commit();

        XMVECTOR8 vCorners;
        BuildFrustumCorners( pSpot->GetWorldBound().GetFrustum(), XMMatrixIdentity(), vCorners );
        ATG::MeshVertexP pVerts[8];
        for( DWORD j = 0; j < 8; j++ )
        {
            XMStoreFloat3( &pVerts[j].Position, vCorners.v[j] );
        }
        const WORD pFrustumIB[] =
        {
            0, 1, 2, 3,
            5, 4, 7, 6,
            0, 4, 5, 1,
            1, 5, 6, 2,
            2, 6, 7, 3,
            3, 7, 4, 0
        };
        m_pd3dDevice->DrawIndexedPrimitiveUP( D3DPT_QUADLIST, 0, 8, 6, ( VOID* )pFrustumIB,
                                              D3DFMT_INDEX16, ( VOID* )pVerts, ATG::MeshVertexP::Size() );

        pEffect->EndPass();
        pEffect->EndTechnique();
    }


    // Render one pass over the scene for each directional light.
    FXLHANDLE hDirTechnique = m_DeferredParameterPool.GetTechniqueDirLight();
    DWORD dwDirLightCount = m_ShadowSceneState.DirLights.Size();
    for( DWORD i = 0; i < dwDirLightCount; ++i )
    {
        ATG::DirectionalLight* pDir = &m_ShadowSceneState.DirLights[i];
        if( pDir->TestFlag( ATG::Light::IsDisabled ) )
            continue;
        ++m_dwActiveLights;
        ++m_dwLightInfluences;

        D3DTexture* pDepthTextureTight = m_ShadowSceneState.DirShadowMaps[i * 2];
        XMMATRIX matLightVPTight = pDir->GetLightViewProjection( 0 );
        m_DeferredParameterPool.SetShadowParameters( pDepthTextureTight, matLightVPTight * m_matShadowTextureOffset );
        D3DTexture* pDepthTextureScene = m_ShadowSceneState.DirShadowMaps[i * 2 + 1];
        XMMATRIX matLightVPScene = pDir->GetLightViewProjection( 1 );
        m_DeferredParameterPool.SetShadowParametersScene( pDepthTextureScene,
                                                          matLightVPScene * m_matShadowTextureOffset );

        m_DeferredParameterPool.SetLightColor( pDir->GetColor() * m_fLightIntensityScale );
        m_DeferredParameterPool.SetLightWorldDir( pDir->GetWorldDirection() );
        ATG::SimpleShaders::SetDeclPos();
        pEffect->BeginTechnique( hDirTechnique, 0 );
        pEffect->BeginPassFromIndex( 0 );
        pEffect->Commit();

        m_pd3dDevice->DrawPrimitiveUP( D3DPT_RECTLIST, 1, fLightRect, 5 * sizeof( FLOAT ) );

        pEffect->EndPass();
        pEffect->EndTechnique();
    }

    // Render one pass over the scene for each point light.
    FXLHANDLE hPointLightTechnique = m_DeferredParameterPool.GetTechniquePointLight();
    DWORD dwPointLightCount = m_ShadowSceneState.PointLights.Size();
    for( DWORD i = 0; i < dwPointLightCount; ++i )
    {
        ATG::PointLight* pPoint = &m_ShadowSceneState.PointLights[i];
        if( pPoint->TestFlag( ATG::Light::IsDisabled ) )
            continue;
        ++m_dwActiveLights;
        if( CameraBound.Collide( pPoint->GetWorldBound() ) == FALSE )
            continue;
        ++m_dwLightInfluences;

        // Extract light position and radius from point light
        XMVECTOR vPosRange = pPoint->GetWorldPosition();
        FLOAT fRadius = pPoint->GetWorldRange() * m_fLightRangeScale;
        vPosRange.w = fRadius;

        pEffect->BeginTechnique( hPointLightTechnique, 0 );
        pEffect->BeginPassFromIndex( 0 );
        m_DeferredParameterPool.SetLightColor( pPoint->GetColor() * m_fLightIntensityScale );
        m_DeferredParameterPool.SetLightWorldPosRange( vPosRange );
        pEffect->Commit();

        XMVECTOR3 vCorners;
        BuildPointLightRectCorners( pPoint, vCorners );

        // Compute texture UV coordinates from the screen space coordinates.
        const XMVECTOR vScaleUV = XMVectorSet( 0.5f, -0.5f, 0, 0 );
        const XMVECTOR vShiftUV = XMVectorSet( 0.5f, 0.5f, 0, 0 );
        XMVECTOR vUpperLeftUV = ( vCorners.v[0] * vScaleUV + vShiftUV );
        XMVECTOR vUpperRightUV = ( vCorners.v[1] * vScaleUV + vShiftUV );
        XMVECTOR vLowerLeftUV = ( vCorners.v[2] * vScaleUV + vShiftUV );

        ATG::SimpleShaders::SetDeclPosTex();

        ATG::MeshVertexPT* pVerts = NULL;
        m_pd3dDevice->BeginVertices( D3DPT_RECTLIST, 3, sizeof( ATG::MeshVertexPT ), ( VOID** )&pVerts );
        XMStoreFloat3( &pVerts[0].Position, vCorners.v[0] );
        XMStoreFloat2( &pVerts[0].TexCoord, vUpperLeftUV );
        XMStoreFloat3( &pVerts[1].Position, vCorners.v[1] );
        XMStoreFloat2( &pVerts[1].TexCoord, vUpperRightUV );
        XMStoreFloat3( &pVerts[2].Position, vCorners.v[2] );
        XMStoreFloat2( &pVerts[2].TexCoord, vLowerLeftUV );
        m_pd3dDevice->EndVertices();

        pEffect->EndPass();
        pEffect->EndTechnique();
    }

    PIXEndNamedEvent();
    return S_OK;
}
