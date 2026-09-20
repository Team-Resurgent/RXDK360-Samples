//--------------------------------------------------------------------------------------
// PassPerLightRender.cpp
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "SceneViewer2.h"

HRESULT SceneViewer::RenderScenePassPerLight()
{
    if( !IsSceneRenderable() )
        return E_FAIL;

    PIXBeginNamedEvent( 0xFFFFFF00, "Pass-Per-Light Scene Render" );
    DWORD dwVisibleModelCount = m_ShadowSceneState.VisibleModels.size();

    static ATG::StringID strDiffuseParamName( L"DiffuseTexture" );
    static ATG::StringID strNormalMapParamName( L"NormalMapTexture" );
    static ATG::StringID strSpecularMapParamName( L"SpecularMapTexture" );

    // Set up ambient light.
    FLOAT fAmbient = m_fAmbient;
    XMVECTOR vAmbient = XMVectorReplicate( fAmbient );
    m_PassPerLightParameterPool.SetLightColor( vAmbient );

    FXLHANDLE hTechniqueAmbient = m_PassPerLightParameterPool.GetTechniqueAmbientLight();

    // Render each visible model with ambient light.  This will also act as a Z pass.
    for( DWORD i = 0; i < dwVisibleModelCount; ++i )
    {
        ATG::Model* pModel = m_ShadowSceneState.VisibleModels[i];

        ++m_dwModelsRendered;

        XMMATRIX matWorld = m_ShadowSceneState.VisibleModelWorldTransforms[i];
        m_PassPerLightParameterPool.SetWorldViewProjMatrix( matWorld * m_ShadowSceneState.matVP );

        DWORD dwMeshMapCount = pModel->GetNumMeshMappings();
        for( DWORD dwMeshMapIndex = 0; dwMeshMapIndex < dwMeshMapCount; ++dwMeshMapIndex )
        {
            ATG::MeshMapping& meshmapping = pModel->GetMeshMapping( dwMeshMapIndex );
            ATG::BaseMesh* pMesh = meshmapping.pMesh;

            DWORD dwSubsetCount = pMesh->GetNumSubsets();
            for( DWORD dwSubsetIndex = 0; dwSubsetIndex < dwSubsetCount; ++dwSubsetIndex )
            {
                ATG::MaterialInstance* pMaterial = meshmapping.Materials[ dwSubsetIndex ];
                if( pMaterial->GetBaseMaterial() != m_pPassPerLightBaseMaterial )
                    continue;
                pMaterial->BeginMaterial( m_pd3dDevice, hTechniqueAmbient );
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

    if( !m_bEnableLighting )
    {
        PIXEndNamedEvent();
        return S_OK;
    }

    ATG::Bound CameraBound = m_ShadowSceneState.Camera.GetWorldBound();

    // Render each point light, rendering each visible model as well.
    FXLHANDLE hTechniquePoint = m_PassPerLightParameterPool.GetTechniquePointLight();
    DWORD dwPointLightCount = m_ShadowSceneState.PointLights.Size();
    DWORD dwVisibleLightIndex = 0;
    for( DWORD i = 0; i < dwPointLightCount; ++i )
    {
        ATG::PointLight* pPoint = &m_ShadowSceneState.PointLights[i];
        if( pPoint->TestFlag( ATG::Light::IsDisabled ) )
            continue;
        ++m_dwActiveLights;
        ATG::Bound LightBound = pPoint->GetWorldBound();
        if( CameraBound.Collide( LightBound ) == FALSE )
            continue;

        ++dwVisibleLightIndex;

        DWORD dwStencilValue = 0;
        if( m_bStencilOptimization )
        {
            // dwStencilValue should range between 1-255 and start at 2
            dwStencilValue = ( dwVisibleLightIndex % 255 ) + 1;
            if( dwStencilValue == 1 )
            {
                m_pd3dDevice->Clear( 0, NULL, D3DCLEAR_STENCIL, 0, 1.0, 0 );
            }
            RenderPointLightQuadToStencil( pPoint, dwStencilValue );
        }

        XMVECTOR vPointPosWorld = pPoint->GetWorldPosition();
        FLOAT fRange = 1.0f / ( pPoint->GetWorldRange() * m_fLightRangeScale );

        m_PassPerLightParameterPool.SetLightColor( pPoint->GetColor() * m_fLightIntensityScale );

        for( DWORD dwModelIndex = 0; dwModelIndex < dwVisibleModelCount; ++dwModelIndex )
        {
            ATG::Model* pModel = m_ShadowSceneState.VisibleModels[ dwModelIndex ];
            if( LightBound.Collide( m_ShadowSceneState.VisibleModelWorldBounds[dwModelIndex] ) == FALSE )
                continue;

            ++m_dwModelsRendered;
            ++m_dwLightInfluences;

            // Setup model-specific shader constants.
            XMMATRIX matWorld = m_ShadowSceneState.VisibleModelWorldTransforms[dwModelIndex];
            m_PassPerLightParameterPool.SetWorldViewProjMatrix( matWorld * m_ShadowSceneState.matVP );
            XMVECTOR vDet;
            XMMATRIX matInvWorld = XMMatrixInverse( &vDet, matWorld );
            XMVECTOR vPointPosObj = XMVector3TransformCoord( vPointPosWorld, matInvWorld );
            vPointPosObj.w = fRange;
            m_PassPerLightParameterPool.SetLightObjPosRange( vPointPosObj );
            XMVECTOR vObjViewDir;
            vObjViewDir = XMVector3Normalize( XMVector3Transform( m_ShadowSceneState.Camera.GetWorldDirection(),
                                                                  matInvWorld ) );
            m_PassPerLightParameterPool.SetObjectViewDirection( vObjViewDir );

            DWORD dwMeshMapCount = pModel->GetNumMeshMappings();
            for( DWORD dwMeshMapIndex = 0; dwMeshMapIndex < dwMeshMapCount; ++dwMeshMapIndex )
            {
                ATG::MeshMapping& meshmapping = pModel->GetMeshMapping( dwMeshMapIndex );
                ATG::BaseMesh* pMesh = meshmapping.pMesh;

                DWORD dwSubsetCount = pMesh->GetNumSubsets();
                for( DWORD dwSubsetIndex = 0; dwSubsetIndex < dwSubsetCount; ++dwSubsetIndex )
                {
                    ATG::MaterialInstance* pMaterial = meshmapping.Materials[ dwSubsetIndex ];
                    if( pMaterial->GetBaseMaterial() != m_pPassPerLightBaseMaterial )
                        continue;
                    pMaterial->BeginMaterial( m_pd3dDevice, hTechniquePoint );
                    pMaterial->BeginPass( 0 );
                    if( m_bStencilOptimization )
                    {
                        m_pd3dDevice->SetRenderState( D3DRS_STENCILENABLE, TRUE );
                        m_pd3dDevice->SetRenderState( D3DRS_STENCILFUNC, D3DCMP_EQUAL );
                        m_pd3dDevice->SetRenderState( D3DRS_STENCILREF, dwStencilValue );
                        m_pd3dDevice->SetRenderState( D3DRS_STENCILPASS, D3DSTENCILOP_KEEP );
                    }
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
    }

    m_pd3dDevice->SetRenderState( D3DRS_STENCILENABLE, FALSE );

    // Render each spot light, rendering each visible model as well.
    FXLHANDLE hTechniqueSpot = m_PassPerLightParameterPool.GetTechniqueSpotLight();
    DWORD dwSpotLightCount = m_ShadowSceneState.SpotLights.Size();
    for( DWORD i = 0; i < dwSpotLightCount; ++i )
    {
        ATG::SpotLight* pSpot = &m_ShadowSceneState.SpotLights[i];
        if( pSpot->TestFlag( ATG::Light::IsDisabled ) )
            continue;
        ++m_dwActiveLights;
        ATG::Bound LightBound = pSpot->GetWorldBound();
        if( CameraBound.Collide( LightBound ) == FALSE )
            continue;

        XMVECTOR vSpotPosWorld = pSpot->GetWorldPosition();
        FLOAT fRange = 1.0f / ( pSpot->GetWorldRange() * m_fLightRangeScale );
        XMVECTOR vSpotDirWorld = pSpot->GetWorldDirection();

        m_PassPerLightParameterPool.SetLightColor( pSpot->GetColor() * m_fLightIntensityScale );
        m_PassPerLightParameterPool.SetSpotLightAngles( pSpot->GetOuterAngle(), pSpot->GetInnerAngle() );
        D3DTexture* pDepthTexture = m_ShadowSceneState.SpotShadowMaps[ i ];
        XMMATRIX matLightVP = pSpot->GetLightViewProjection();
        matLightVP *= m_matShadowTextureOffset;
        m_PassPerLightParameterPool.SetShadowParameters( pDepthTexture, matLightVP );

        for( DWORD dwModelIndex = 0; dwModelIndex < dwVisibleModelCount; ++dwModelIndex )
        {
            ATG::Model* pModel = m_ShadowSceneState.VisibleModels[ dwModelIndex ];
            if( LightBound.Collide( m_ShadowSceneState.VisibleModelWorldBounds[dwModelIndex] ) == FALSE )
                continue;

            ++m_dwModelsRendered;
            ++m_dwLightInfluences;

            // Setup model-specific shader constants.
            XMMATRIX matWorld = m_ShadowSceneState.VisibleModelWorldTransforms[dwModelIndex];
            m_PassPerLightParameterPool.SetWorldViewProjMatrix( matWorld * m_ShadowSceneState.matVP );
            m_PassPerLightParameterPool.SetShadowParameters( pDepthTexture, matWorld * matLightVP );
            XMVECTOR vDet;
            XMMATRIX matInvWorld = XMMatrixInverse( &vDet, matWorld );
            XMVECTOR vSpotPosObj = XMVector3TransformCoord( vSpotPosWorld, matInvWorld );
            vSpotPosObj.w = fRange;
            m_PassPerLightParameterPool.SetLightObjPosRange( vSpotPosObj );
            XMVECTOR vSpotDirObj = XMVector3TransformNormal( vSpotDirWorld, matInvWorld );
            m_PassPerLightParameterPool.SetLightObjDir( vSpotDirObj );
            XMVECTOR vObjViewDir;
            vObjViewDir = XMVector3Normalize( XMVector3Transform( m_ShadowSceneState.Camera.GetWorldDirection(),
                                                                  matInvWorld ) );
            m_PassPerLightParameterPool.SetObjectViewDirection( vObjViewDir );

            DWORD dwMeshMapCount = pModel->GetNumMeshMappings();
            for( DWORD dwMeshMapIndex = 0; dwMeshMapIndex < dwMeshMapCount; ++dwMeshMapIndex )
            {
                ATG::MeshMapping& meshmapping = pModel->GetMeshMapping( dwMeshMapIndex );
                ATG::BaseMesh* pMesh = meshmapping.pMesh;

                DWORD dwSubsetCount = pMesh->GetNumSubsets();
                for( DWORD dwSubsetIndex = 0; dwSubsetIndex < dwSubsetCount; ++dwSubsetIndex )
                {
                    ATG::MaterialInstance* pMaterial = meshmapping.Materials[ dwSubsetIndex ];
                    if( pMaterial->GetBaseMaterial() != m_pPassPerLightBaseMaterial )
                        continue;
                    pMaterial->BeginMaterial( m_pd3dDevice, hTechniqueSpot );
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
    }

    // Render each directional light, rendering each visible model as well.
    FXLHANDLE hTechniqueDir = m_PassPerLightParameterPool.GetTechniqueDirLight();
    DWORD dwDirLightCount = m_ShadowSceneState.DirLights.Size();
    for( DWORD i = 0; i < dwDirLightCount; ++i )
    {
        ATG::DirectionalLight* pDir = &m_ShadowSceneState.DirLights[i];
        if( pDir->TestFlag( ATG::Light::IsDisabled ) )
            continue;
        ++m_dwActiveLights;

        XMVECTOR vDirWorld = pDir->GetWorldDirection();

        m_PassPerLightParameterPool.SetLightColor( pDir->GetColor() * m_fLightIntensityScale );
        D3DTexture* pDepthTextureTight = m_ShadowSceneState.DirShadowMaps[ i * 2 ];
        XMMATRIX matLightVPTight = pDir->GetLightViewProjection( 0 );
        matLightVPTight *= m_matShadowTextureOffset;
        D3DTexture* pDepthTextureScene = m_ShadowSceneState.DirShadowMaps[ ( i * 2 ) + 1 ];
        XMMATRIX matLightVPScene = pDir->GetLightViewProjection( 1 );
        matLightVPScene *= m_matShadowTextureOffset;

        for( DWORD dwModelIndex = 0; dwModelIndex < dwVisibleModelCount; ++dwModelIndex )
        {
            ATG::Model* pModel = m_ShadowSceneState.VisibleModels[ dwModelIndex ];

            ++m_dwModelsRendered;
            ++m_dwLightInfluences;

            // Setup model-specific shader constants.
            XMMATRIX matWorld = m_ShadowSceneState.VisibleModelWorldTransforms[dwModelIndex];
            m_PassPerLightParameterPool.SetWorldViewProjMatrix( matWorld * m_ShadowSceneState.matVP );
            m_PassPerLightParameterPool.SetShadowParameters( pDepthTextureTight, matWorld * matLightVPTight );
            m_PassPerLightParameterPool.SetShadowParametersScene( pDepthTextureScene, matWorld * matLightVPScene );
            XMVECTOR vDet;
            XMMATRIX matInvWorld = XMMatrixInverse( &vDet, matWorld );
            XMVECTOR vDirObj = XMVector3TransformNormal( vDirWorld, matInvWorld );
            m_PassPerLightParameterPool.SetLightObjDir( vDirObj );
            XMVECTOR vObjViewDir;
            vObjViewDir = XMVector3Normalize( XMVector3Transform( m_ShadowSceneState.Camera.GetWorldDirection(),
                                                                  matInvWorld ) );
            m_PassPerLightParameterPool.SetObjectViewDirection( vObjViewDir );

            DWORD dwMeshMapCount = pModel->GetNumMeshMappings();
            for( DWORD dwMeshMapIndex = 0; dwMeshMapIndex < dwMeshMapCount; ++dwMeshMapIndex )
            {
                ATG::MeshMapping& meshmapping = pModel->GetMeshMapping( dwMeshMapIndex );
                ATG::BaseMesh* pMesh = meshmapping.pMesh;

                DWORD dwSubsetCount = pMesh->GetNumSubsets();
                for( DWORD dwSubsetIndex = 0; dwSubsetIndex < dwSubsetCount; ++dwSubsetIndex )
                {
                    ATG::MaterialInstance* pMaterial = meshmapping.Materials[ dwSubsetIndex ];
                    if( pMaterial->GetBaseMaterial() != m_pPassPerLightBaseMaterial )
                        continue;
                    pMaterial->BeginMaterial( m_pd3dDevice, hTechniqueDir );
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
    }

    PIXEndNamedEvent();

    return S_OK;
}

VOID SceneViewer::RenderPointLightQuadToStencil( ATG::PointLight* pLight, DWORD dwStencilValue )
{
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_STENCILENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_STENCILREF, dwStencilValue );
    m_pd3dDevice->SetRenderState( D3DRS_STENCILFUNC, D3DCMP_ALWAYS );
    m_pd3dDevice->SetRenderState( D3DRS_STENCILPASS, D3DSTENCILOP_REPLACE );

    XMVECTOR3 vCorners;
    BuildPointLightRectCorners( pLight, vCorners );

    ATG::SimpleShaders::SetDeclPos();
    ATG::SimpleShaders::BeginShader_PreTransformed_DepthOnly();
    XMFLOAT3* pVerts = NULL;
    m_pd3dDevice->BeginVertices( D3DPT_RECTLIST, 3, sizeof( XMFLOAT3 ), ( VOID** )&pVerts );

    XMStoreFloat3( &pVerts[0], vCorners.v[0] );
    XMStoreFloat3( &pVerts[1], vCorners.v[1] );
    XMStoreFloat3( &pVerts[2], vCorners.v[2] );

    m_pd3dDevice->EndVertices();
    ATG::SimpleShaders::EndShader();

    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_STENCILENABLE, FALSE );
}
