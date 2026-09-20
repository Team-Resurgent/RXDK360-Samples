//--------------------------------------------------------------------------------------
// DefaultRender.cpp
//
// Contains methods for scene rendering with the ubershader and ubershader library.
// 
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "SceneViewer2.h"

//--------------------------------------------------------------------------------------
// Name: RenderSceneNormal()
// Desc: Walks the scene graph and renders all of the models.  If debugging options are
//       enabled, bounding volumes, lights, and frames are rendered with debug draw
//       primitives.
//--------------------------------------------------------------------------------------
HRESULT SceneViewer::RenderSceneNormal()
{
    if( !IsSceneRenderable() )
        return E_FAIL;

    PIXBeginNamedEvent( 0xFFFFFF00, "Scene Render" );
    // Setup some basic renderstate, in case the shaders don't do this.
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_FILLMODE, D3DFILL_SOLID );
    m_pd3dDevice->SetRenderState( D3DRS_VIEWPORTENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_ALL );

    // Count active lights.
    if( m_bEnableLighting && m_bCapturePerfData )
    {
        DWORD dwSize = m_ShadowSceneState.PointLights.Size();
        for( DWORD i = 0; i < dwSize; ++i )
        {
            if( !m_ShadowSceneState.PointLights[i].TestFlag( ATG::Light::IsDisabled ) )
                ++m_dwActiveLights;
        }
        dwSize = m_ShadowSceneState.SpotLights.Size();
        for( DWORD i = 0; i < dwSize; ++i )
        {
            if( !m_ShadowSceneState.SpotLights[i].TestFlag( ATG::Light::IsDisabled ) )
                ++m_dwActiveLights;
        }
    }

    // Set up ambient light.
    FLOAT fAmbient = m_fAmbient;
    if( !m_bEnableLighting )
        fAmbient = 1.0f;
    XMVECTOR vAmbient = XMVectorReplicate( fAmbient );
    m_SampleParameterPool.SetAmbient( vAmbient );

    // Set default textures for all of the shadow maps.
    D3DBaseTexture* pBlackTexture = m_pScene->GetResourceDatabase()->GetBlackTexture()->GetD3DTexture();
    for( DWORD i = 0; i < g_dwMaxSpotLightCount; ++i )
    {
        m_SampleParameterPool.SetSpotLightShadowDepthTexture( i, pBlackTexture );
    }
    m_SampleParameterPool.SetDirLightShadowDepthTexture( 0, pBlackTexture, pBlackTexture );

    PIXBeginNamedEvent( 0xFFFFFF00, "Opaque Models" );

    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_CULLMODE, m_dwTriangleCullingMode );

    // Render models front to back.
    DWORD dwModelCount = m_ShadowSceneState.VisibleModels.size();
    for( DWORD dwModelIndex = 0; dwModelIndex < dwModelCount; ++dwModelIndex )
    {
        ATG::Model* pModel = m_ShadowSceneState.VisibleModels[dwModelIndex];
        RenderModel( pModel, FALSE,
                     m_ShadowSceneState.VisibleModelWorldTransforms[dwModelIndex],
                     m_ShadowSceneState.VisibleModelWorldBounds[dwModelIndex],
                     &m_ShadowSceneState.Camera );
    }

    PIXEndNamedEvent();

    if( m_bDrawTransparentObjects )
    {
        PIXBeginNamedEvent( 0xFFFFFF00, "Transparent Models" );

        m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
        m_pd3dDevice->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
        m_pd3dDevice->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );
        m_pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE, FALSE );
        m_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_NONE );

        // Render transparent models back to front.
        dwModelCount = m_ShadowSceneState.VisibleTransparentModels.size();
        for( DWORD dwModelIndex = 0; dwModelIndex < dwModelCount; ++dwModelIndex )
        {
            ATG::Model* pModel = m_ShadowSceneState.VisibleTransparentModels[dwModelIndex];
            RenderModel( pModel, TRUE,
                         m_ShadowSceneState.VisibleTransparentModelWorldTransforms[dwModelIndex],
                         m_ShadowSceneState.VisibleTransparentModelWorldBounds[dwModelIndex],
                         &m_ShadowSceneState.Camera );
        }

        m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
        m_pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE, TRUE );
        m_pd3dDevice->SetRenderState( D3DRS_CULLMODE, m_dwTriangleCullingMode );

        PIXEndNamedEvent();
    }

    if( m_bCapturePerfData )
        m_dwModelsRendered = dwModelCount;
    PIXEndNamedEvent();

    return S_OK;
}


VOID SceneViewer::RenderModel( ATG::Model* pModel, BOOL bRenderTransparent, const XMMATRIX matWorld,
                               const ATG::Bound& ModelBound, ATG::Camera* pCamera )
{
    XMVECTOR vDeterminant;
    XMMATRIX matInvWorld = XMMatrixInverse( &vDeterminant, matWorld );

    // Setup lighting for this model.
    if( m_bEnableLighting )
    {
        m_dwLightInfluences += SetupLighting( pModel, matWorld, matInvWorld, ModelBound );
    }
    else
    {
        m_hCurrentShaderLibTechnique = m_SampleParameterPool.GetShaderLibraryHandle( 0, 0, 0 );
    }

    // Setup model-specific shader constants.
    XMVECTOR vObjViewDir, vObjViewPos;
    vObjViewDir = XMVector3Normalize( XMVector3Transform( pCamera->GetWorldDirection(), matInvWorld ) );
    m_SampleParameterPool.SetObjectViewDirection( vObjViewDir );
    vObjViewPos = XMVector3TransformCoord( pCamera->GetWorldPosition(), matInvWorld );
    m_SampleParameterPool.SetObjectViewPosition( vObjViewPos );

    // Load camera matrix into shader constants.
    m_ShadowSceneState.matWVP = matWorld * m_ShadowSceneState.matVP;
    m_SampleParameterPool.SetWorldViewProjMatrix( m_ShadowSceneState.matWVP );

    // Iterate over meshes within the model.
    for( DWORD dwMeshMapIndex = 0; dwMeshMapIndex < pModel->GetNumMeshMappings(); ++dwMeshMapIndex )
    {
        ATG::MeshMapping& meshmap = pModel->GetMeshMapping( dwMeshMapIndex );

        if( meshmap.pMesh->GetFlags() & ATG::BaseMesh::IsRenderable )
        {
            // Iterate over mesh subsets - each mesh subset corresponds to a material.
            for( DWORD dwSubsetIndex = 0; dwSubsetIndex < meshmap.pMesh->GetNumSubsets(); ++dwSubsetIndex )
            {
                ATG::BaseMesh* pMesh = meshmap.pMesh;
                ATG::MaterialInstance* pMaterial = meshmap.Materials[dwSubsetIndex];
                if( bRenderTransparent && !pMaterial->IsTransparent() )
                    continue;
                if( !bRenderTransparent && pMaterial->IsTransparent() )
                    continue;
                switch( m_dwDebugRenderMode )
                {
                    case 0:
                        RenderMeshSubset( pMesh, pMaterial, dwSubsetIndex );
                        break;
                    case 1:
                        // Bright white wireframe rendering - useful when shaders are busted.
                        ATG::SimpleShaders::BeginShader_Transformed_ConstantColor( m_ShadowSceneState.matWVP,
                                                                                   0xFFFFFFFF );
                        m_pd3dDevice->SetRenderState( D3DRS_FILLMODE, D3DFILL_WIREFRAME );
                        pMesh->RenderSubset( dwSubsetIndex );
                        ATG::SimpleShaders::EndShader();
                        // Update stats.
                        if( m_bCapturePerfData )
                        {
                            m_dwPrimitivesRendered += pMesh->GetSubsetDesc( dwSubsetIndex )->GetNumPrimitives();
                            ++m_dwSubsetsRendered;
                        }
                        break;
                    case 2:
                        // Bright white rendering.
                        ATG::SimpleShaders::BeginShader_Transformed_ConstantColor( m_ShadowSceneState.matWVP,
                                                                                   0xFFFFFFFF );
                        m_pd3dDevice->SetRenderState( D3DRS_FILLMODE, D3DFILL_SOLID );
                        pMesh->RenderSubset( dwSubsetIndex );
                        ATG::SimpleShaders::EndShader();
                        // Update stats.
                        if( m_bCapturePerfData )
                        {
                            m_dwPrimitivesRendered += pMesh->GetSubsetDesc( dwSubsetIndex )->GetNumPrimitives();
                            ++m_dwSubsetsRendered;
                        }
                        break;
                    case 3:
                        // Overlay wireframe.
                        RenderMeshSubset( pMesh, pMaterial, dwSubsetIndex );
                        ATG::SimpleShaders::BeginShader_Transformed_ConstantColor( m_ShadowSceneState.matWVP,
                                                                                   0xFFFFFFFF );
                        m_pd3dDevice->SetRenderState( D3DRS_FILLMODE, D3DFILL_WIREFRAME );
                        pMesh->RenderSubset( dwSubsetIndex );
                        ATG::SimpleShaders::EndShader();
                        break;
                    case 4:
                        // Overlay normals.
                        RenderMeshSubset( pMesh, pMaterial, dwSubsetIndex );
                        break;
                    case 5:
                        // Transparent white rendering.
                        ATG::SimpleShaders::BeginShader_Transformed_ConstantColor( m_ShadowSceneState.matWVP,
                                                                                   0x40FFFFFF );
                        m_pd3dDevice->SetRenderState( D3DRS_FILLMODE, D3DFILL_SOLID );
                        m_pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE, FALSE );
                        pMesh->RenderSubset( dwSubsetIndex );
                        ATG::SimpleShaders::EndShader();
                        // Update stats.
                        if( m_bCapturePerfData )
                        {
                            m_dwPrimitivesRendered += pMesh->GetSubsetDesc( dwSubsetIndex )->GetNumPrimitives();
                            ++m_dwSubsetsRendered;
                        }
                        break;
                    default:
                        break;
                }
            }
        }
    }
}


VOID SceneViewer::RenderMeshSubset( ATG::BaseMesh* pMesh, ATG::MaterialInstance* pMaterial, DWORD dwSubsetIndex )
{
    assert( pMaterial != NULL && pMesh != NULL );
    assert( pMesh->GetSubsetDesc( dwSubsetIndex ) != NULL );

    if( pMaterial->GetBaseMaterial() == NULL || pMaterial->GetBaseMaterial()->GetMaterialImplementation() == NULL )
    {
        // Material is invalid or not loaded.
        // Draw object in pulsating red for visibility.
        DWORD dwRed = ( DWORD )( sinf( m_fAppTime * XM_2PI ) * 127.0f + 128.0f );
        ATG::SimpleShaders::BeginShader_Transformed_ConstantColor( m_ShadowSceneState.matWVP, D3DCOLOR_ARGB( 255,
                                                                                                             dwRed, 0,
                                                                                                             0 ) );
        pMesh->RenderSubset( dwSubsetIndex );
        ATG::SimpleShaders::EndShader();
        return;
    }

    // Update stats.
    if( m_bCapturePerfData )
    {
        m_dwPrimitivesRendered += pMesh->GetSubsetDesc( dwSubsetIndex )->GetNumPrimitives();
        ++m_dwSubsetsRendered;
    }

    // Extract technique description for pass count, and begin technique.
    FXLHANDLE hTechnique = NULL;
    if( m_RenderMode == SVRM_NORMAL )
    {
        if( pMaterial->GetBaseMaterial() == m_pUbershaderBaseMaterial )
        {
            ATG::FXLiteMaterialImplementation* pFXLMaterialImpl = ( ATG::FXLiteMaterialImplementation* )
                m_pUbershaderBaseMaterial->GetMaterialImplementation();
            hTechnique = pFXLMaterialImpl->m_pEffect->GetTechniqueHandleFromIndex( m_dwUbershaderTechniqueIndex );
        }
    }
    else
    {
        hTechnique = m_hCurrentShaderLibTechnique;
    }
    DWORD dwPassCount = pMaterial->GetPassCount();
    pMaterial->BeginMaterial( m_pd3dDevice, ( DWORD )hTechnique );
    // Iterate over passes.
    for( DWORD dwPassIndex = 0; dwPassIndex < dwPassCount; ++dwPassIndex )
    {
        // Begin pass and commit shader constants to D3D.
        pMaterial->BeginPass( dwPassIndex );

        // Wireframe override - overrides renderstate set in shader.
        if( m_bWireframe )
        {
            m_pd3dDevice->SetRenderState( D3DRS_FILLMODE, D3DFILL_WIREFRAME );
        }

        SetSamplerOverrides();

        // Render mesh subset.
        pMesh->RenderSubset( dwSubsetIndex );

        pMaterial->EndPass();
    }
    pMaterial->EndMaterial();
}


