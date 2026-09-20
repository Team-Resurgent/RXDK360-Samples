//--------------------------------------------------------------------------------------
// TerrainView.cpp
//
// Rendering code to draw a textured 3D heightfield, using tiled textures as inputs.
//
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include <assert.h>
#include "TerrainView.h"
#include "PageLoaders.h"
#include "TitleResidencyManager.h"
#include "ResidencySampleRender.h"
#include "AtgUtil.h"

#pragma warning( disable : 6211 ) //"Leaking memory due to an exception"

//--------------------------------------------------------------------------------------
// Name: TerrainView constructor
//--------------------------------------------------------------------------------------
TerrainView::TerrainView( D3DDevice* pd3dDevice, D3DTiledResourceDevice* pTiledResourceDevice, D3DTilePool* pTilePool, TitleResidencyManager* pResidencyManager )
{
    m_pd3dDevice = pd3dDevice;
    m_pTiledResourceDevice = pTiledResourceDevice;
    m_pTilePool = pTilePool;
    m_pResidencyManager = pResidencyManager;

    // Load the tiled textures from files on disk:
    const CHAR* strFileNames[] = { "game:\\media\\s_diffuse.sp", "game:\\media\\s_normalmap.sp", "game:\\media\\s_heightmap.sp" };
    BoundTiledTexture* pBoundTiledTextures[] = { &m_DiffuseMapTexture, &m_NormalMapTexture, &m_HeightMapTexture };

    for( UINT i = 0; i < ARRAYSIZE( strFileNames ); ++i )
    {
        TiledFileLoader* pLoader = new TiledFileLoader();
        HRESULT hr = pLoader->LoadFile( strFileNames[i] );
        if( FAILED(hr) )
        {
            delete pLoader;
            pBoundTiledTextures[i]->pTexture = NULL;
            pBoundTiledTextures[i]->pSamplingQualityManager = NULL;
            pBoundTiledTextures[i]->pTileLoader = NULL;
            continue;
        }

        D3DTiledTexture* pTexture = NULL;
        hr = pLoader->CreateTiledTexture2D( m_pTiledResourceDevice, &pTexture );
        assert( SUCCEEDED(hr) );

        SamplingQualityManager* pSQM = new SamplingQualityManager( pTexture, m_pd3dDevice );
        m_pResidencyManager->RegisterTileActivityHandler( pSQM );

        pBoundTiledTextures[i]->pTileLoader = pLoader;
        pBoundTiledTextures[i]->pSamplingQualityManager = pSQM;
        pBoundTiledTextures[i]->pTexture = pTexture;

        pTexture->GetLevelDesc( 0, &pBoundTiledTextures[i]->BaseLevelDesc );
    }

    if( m_DiffuseMapTexture.pTexture != NULL &&
        m_NormalMapTexture.pTexture != NULL &&
        m_HeightMapTexture.pTexture != NULL )
    {
        const D3DTiledTexture* pTextureSet[] = { m_DiffuseMapTexture.pTexture, m_NormalMapTexture.pTexture, m_HeightMapTexture.pTexture };
        ITileLoader* pLoaderSet[] = { m_DiffuseMapTexture.pTileLoader, m_NormalMapTexture.pTileLoader, m_HeightMapTexture.pTileLoader };

        m_RSID = m_pResidencyManager->CreateResourceSet( pTextureSet, pLoaderSet, ARRAYSIZE(pTextureSet) );
    }

    UINT DrawGridWidth = 32;

    m_matScaling = XMMatrixScalingFromVector( XMVectorReplicate( 163.84f ) );

    m_QuadCount = DrawGridWidth * DrawGridWidth;
    m_DrawGridSize.cx = 1;
    m_DrawGridSize.cy = 1;

    m_QuadLayoutConstant[0] = (FLOAT)DrawGridWidth;
    m_QuadLayoutConstant[1] = 0.2f;
    m_QuadLayoutConstant[2] = 1.0f / (FLOAT)DrawGridWidth;
    m_QuadLayoutConstant[3] = 1.0f / (FLOAT)DrawGridWidth;

    m_QuadUVTransformConstant[0] = 1.0f;
    m_QuadUVTransformConstant[1] = 1.0f;
    m_QuadUVTransformConstant[2] = 0.0f;
    m_QuadUVTransformConstant[3] = 0.0f;

    ATG::LoadVertexShader( "game:\\media\\shaders\\VSTerrain.xvu", &m_pVSTerrain );
    ATG::LoadPixelShader( "game:\\media\\shaders\\PSTerrain.xpu", &m_pPSTerrainRender );

    ATG::LoadVertexShader( "game:\\media\\shaders\\VSTransform.xvu", &m_pVSTransform );
    ATG::LoadPixelShader( "game:\\media\\shaders\\PSColor.xpu", &m_pPSColor );

    XMStoreFloat4( &m_LightDirectionWorld, XMVector3Normalize( XMVectorSet( 1, -2, 0, 0 ) ) );
    XMStoreFloat4( &m_AmbientLight, XMVectorSet( 0.01f, 0.01f, 0.01f, 1 ) );

    const D3DVERTEXELEMENT9 TerrainVertexElements[] =
    {
        { 0,     0, D3DDECLTYPE_FLOAT3,     0,  D3DDECLUSAGE_POSITION,  0 },
        { 0,    12, D3DDECLTYPE_FLOAT2,     0,  D3DDECLUSAGE_TEXCOORD,  0 },
        D3DDECL_END()
    };

    m_pd3dDevice->CreateVertexDeclaration( TerrainVertexElements, &m_pDeclTerrainEdges );

    const FLOAT EdgePos = 25.0f;

    UINT WaterVertexCount = 4;
    m_pd3dDevice->CreateVertexBuffer( WaterVertexCount * sizeof(TerrainVertex), 0, 0, D3DPOOL_DEFAULT, &m_pVBWater, NULL );

    TerrainVertex* pWaterVerts = NULL;
    m_pVBWater->Lock( 0, 0, (VOID**)&pWaterVerts, 0 );

    pWaterVerts[0].Position = XMFLOAT3( -EdgePos, 0, -EdgePos );
    pWaterVerts[1].Position = XMFLOAT3( EdgePos, 0, -EdgePos );
    pWaterVerts[2].Position = XMFLOAT3( EdgePos, 0, EdgePos );
    pWaterVerts[3].Position = XMFLOAT3( -EdgePos, 0, EdgePos );

    m_pVBWater->Unlock();
}

//--------------------------------------------------------------------------------------
// Name: TerrainView destructor
//--------------------------------------------------------------------------------------
TerrainView::~TerrainView(void)
{
}

//--------------------------------------------------------------------------------------
// Name: TerrainView::PreSceneRender
// Desc: Updates the sampling quality managers for the terrain textures.
//--------------------------------------------------------------------------------------
VOID TerrainView::PreSceneRender( FLOAT fDeltaTime )
{
    if( m_DiffuseMapTexture.pSamplingQualityManager != NULL )
    {
        m_DiffuseMapTexture.pSamplingQualityManager->Render( m_pd3dDevice, m_pTiledResourceDevice, fDeltaTime );
    }

    if( m_NormalMapTexture.pSamplingQualityManager != NULL )
    {
        m_NormalMapTexture.pSamplingQualityManager->Render( m_pd3dDevice, m_pTiledResourceDevice, fDeltaTime );
    }

    if( m_HeightMapTexture.pSamplingQualityManager != NULL )
    {
        m_HeightMapTexture.pSamplingQualityManager->Render( m_pd3dDevice, m_pTiledResourceDevice, fDeltaTime );
    }
}

//--------------------------------------------------------------------------------------
// Name: TerrainView::RenderResidencyView
// Desc: Renders a residency sample view for the terrain, using the terrain vertex
//       shader and the standard residency sample render pixel shader.
//--------------------------------------------------------------------------------------
VOID TerrainView::RenderResidencyView( XMMATRIX matView, XMMATRIX matProjection )
{
    UINT BeginSceneID = ResidencySampleRender::BeginScene( m_pd3dDevice, m_pResidencyManager );

    m_pd3dDevice->SetVertexShader( m_pVSTerrain );
    ResidencySampleRender::SetPixelShader( m_pd3dDevice, m_pResidencyManager, m_RSID );

    m_pd3dDevice->SetIndices( NULL );

    if( m_HeightMapTexture.pTexture != NULL )
    {
        m_pTiledResourceDevice->SetTexture( D3DVERTEXTEXTURESAMPLER0, m_HeightMapTexture.pTexture );
        m_pd3dDevice->SetTexture( D3DVERTEXTEXTURESAMPLER0, m_HeightMapTexture.pSamplingQualityManager->GetLODQualityTexture() );
        m_pd3dDevice->SetVertexShaderConstantF( 4, m_HeightMapTexture.pSamplingQualityManager->GetUVScalingConstant(), 1 );
        m_pd3dDevice->SetVertexShaderConstantF( 5, m_QuadLayoutConstant, 1 );
        m_pd3dDevice->SetSamplerState( D3DVERTEXTEXTURESAMPLER0, D3DSAMP_MAGFILTER, D3DTEXF_POINT );
        m_pd3dDevice->SetSamplerState( D3DVERTEXTEXTURESAMPLER0, D3DSAMP_MINFILTER, D3DTEXF_POINT );
    }
    else
    {
        m_pTiledResourceDevice->SetTexture( D3DVERTEXTEXTURESAMPLER0, NULL );
        m_pd3dDevice->SetTexture( D3DVERTEXTEXTURESAMPLER0, NULL );
    }

    m_pd3dDevice->SetRenderState( D3DRS_TESSELLATIONMODE, D3DTM_CONTINUOUS );
    m_pd3dDevice->SetRenderState( D3DRS_MINTESSELLATIONLEVEL, ATG::FtoDW( 1.0f ) );
    m_pd3dDevice->SetRenderState( D3DRS_MAXTESSELLATIONLEVEL, ATG::FtoDW( 1.0f ) );

    m_QuadUVTransformConstant[0] = 1.0f / (FLOAT)m_DrawGridSize.cx;
    m_QuadUVTransformConstant[1] = 1.0f / (FLOAT)m_DrawGridSize.cy;
    for( INT y = 0; y < m_DrawGridSize.cx; ++y )
    {
        m_QuadUVTransformConstant[3] = (FLOAT)y / (FLOAT)m_DrawGridSize.cy;

        for( INT x = 0; x < m_DrawGridSize.cx; ++x )
        {
            m_QuadUVTransformConstant[2] = (FLOAT)x / (FLOAT)m_DrawGridSize.cx;

            XMMATRIX matWorld = XMMatrixTranslation( (FLOAT)x, 0, (FLOAT)y ) * m_matScaling;

            XMMATRIX matVP = matWorld * matView * matProjection;
            matVP = XMMatrixTranspose( matVP );
            m_pd3dDevice->SetVertexShaderConstantF( 0, (FLOAT*)&matVP, 4 );
            m_pd3dDevice->SetVertexShaderConstantF( 6, m_QuadUVTransformConstant, 1 );
            m_pd3dDevice->DrawTessellatedPrimitive( D3DTPT_QUADPATCH, 0, m_QuadCount );
        }
    }

    ResidencySampleRender::EndScene( BeginSceneID, m_pd3dDevice, m_pResidencyManager );
}

//--------------------------------------------------------------------------------------
// Name: TerrainView::RenderScene
// Desc: Renders the terrain scene.
//--------------------------------------------------------------------------------------
VOID TerrainView::RenderScene( XMMATRIX matView, XMMATRIX matProjection )
{
    m_pd3dDevice->BeginZPass( 0 );

    m_pd3dDevice->SetVertexShader( m_pVSTerrain );
    m_pd3dDevice->SetPixelShader( m_pPSTerrainRender );

    m_pd3dDevice->SetIndices( NULL );

    if( m_HeightMapTexture.pTexture != NULL )
    {
        m_pTiledResourceDevice->SetTexture( D3DVERTEXTEXTURESAMPLER0, m_HeightMapTexture.pTexture );
        m_pd3dDevice->SetTexture( D3DVERTEXTEXTURESAMPLER0, m_HeightMapTexture.pSamplingQualityManager->GetLODQualityTexture() );
        m_pd3dDevice->SetVertexShaderConstantF( 4, m_HeightMapTexture.pSamplingQualityManager->GetUVScalingConstant(), 1 );
        m_pd3dDevice->SetVertexShaderConstantF( 5, m_QuadLayoutConstant, 1 );
        m_pd3dDevice->SetSamplerState( D3DVERTEXTEXTURESAMPLER0, D3DSAMP_MAGFILTER, D3DTEXF_POINT );
        m_pd3dDevice->SetSamplerState( D3DVERTEXTEXTURESAMPLER0, D3DSAMP_MINFILTER, D3DTEXF_POINT );
    }
    else
    {
        m_pTiledResourceDevice->SetTexture( D3DVERTEXTEXTURESAMPLER0, NULL );
        m_pd3dDevice->SetTexture( D3DVERTEXTEXTURESAMPLER0, NULL );
    }

    if( m_DiffuseMapTexture.pTexture != NULL )
    {
        m_pTiledResourceDevice->SetTexture( 0, m_DiffuseMapTexture.pTexture );
        m_pd3dDevice->SetTexture( 0, m_DiffuseMapTexture.pSamplingQualityManager->GetLODQualityTexture() );
        m_pd3dDevice->SetPixelShaderConstantF( 0, m_DiffuseMapTexture.pSamplingQualityManager->GetUVScalingConstant(), 1 );
        m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_POINT );
        m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_POINT );
    }
    else
    {
        m_pTiledResourceDevice->SetTexture( 0, NULL );
        m_pd3dDevice->SetTexture( 0, NULL );
    }

    if( m_NormalMapTexture.pTexture != NULL )
    {
        m_pTiledResourceDevice->SetTexture( 1, m_NormalMapTexture.pTexture );
        m_pd3dDevice->SetTexture( 1, m_NormalMapTexture.pSamplingQualityManager->GetLODQualityTexture() );
        m_pd3dDevice->SetPixelShaderConstantF( 1, m_NormalMapTexture.pSamplingQualityManager->GetUVScalingConstant(), 1 );
        m_pd3dDevice->SetSamplerState( 1, D3DSAMP_MAGFILTER, D3DTEXF_POINT );
        m_pd3dDevice->SetSamplerState( 1, D3DSAMP_MINFILTER, D3DTEXF_POINT );
    }
    else
    {
        m_pTiledResourceDevice->SetTexture( 1, NULL );
        m_pd3dDevice->SetTexture( 1, NULL );
    }

    m_pd3dDevice->SetPixelShaderConstantF( 4, (FLOAT*)&m_LightDirectionWorld, 1 );
    m_pd3dDevice->SetPixelShaderConstantF( 5, (FLOAT*)&m_AmbientLight, 1 );

    m_pd3dDevice->SetRenderState( D3DRS_TESSELLATIONMODE, D3DTM_CONTINUOUS );
    m_pd3dDevice->SetRenderState( D3DRS_MINTESSELLATIONLEVEL, ATG::FtoDW( 1.0f ) );
    m_pd3dDevice->SetRenderState( D3DRS_MAXTESSELLATIONLEVEL, ATG::FtoDW( 8.0f ) );

    m_QuadUVTransformConstant[0] = 1.0f / (FLOAT)m_DrawGridSize.cx;
    m_QuadUVTransformConstant[1] = 1.0f / (FLOAT)m_DrawGridSize.cy;
    for( INT y = 0; y < m_DrawGridSize.cy; ++y )
    {
        m_QuadUVTransformConstant[3] = (FLOAT)y / (FLOAT)m_DrawGridSize.cy;

        for( INT x = 0; x < m_DrawGridSize.cx; ++x )
        {
            m_QuadUVTransformConstant[2] = (FLOAT)x / (FLOAT)m_DrawGridSize.cx;

            XMMATRIX matWorld = XMMatrixTranslation( (FLOAT)x, 0, (FLOAT)y ) * m_matScaling;

            XMMATRIX matVP = matWorld * matView * matProjection;
            matVP = XMMatrixTranspose( matVP );
            m_pd3dDevice->SetVertexShaderConstantF( 0, (FLOAT*)&matVP, 4 );
            m_pd3dDevice->SetVertexShaderConstantF( 6, m_QuadUVTransformConstant, 1 );
            m_pd3dDevice->DrawTessellatedPrimitive( D3DTPT_QUADPATCH, 0, m_QuadCount );
        }
    }

    XMMATRIX matWorld = XMMatrixTranslation( 0, -0.1f, 0 ) * m_matScaling;
    XMMATRIX matVP = matWorld * matView * matProjection;
    matVP = XMMatrixTranspose( matVP );
    m_pd3dDevice->SetVertexShaderConstantF( 0, (FLOAT*)&matVP, 4 );

    XMFLOAT4 EdgeColor( 0.05f, 0.025f, 0, 1 );
    EdgeColor = XMFLOAT4( 0, 0, 0.1f, 0.75f );
    m_pd3dDevice->SetPixelShaderConstantF( 0, (FLOAT*)&EdgeColor, 1 );
    m_pd3dDevice->SetStreamSource( 0, m_pVBWater, 0, sizeof(TerrainVertex) );
    m_pd3dDevice->SetVertexDeclaration( m_pDeclTerrainEdges );
    m_pd3dDevice->SetVertexShader( m_pVSTransform );
    m_pd3dDevice->SetPixelShader( m_pPSColor );

    m_pd3dDevice->DrawPrimitive( D3DPT_QUADLIST, 0, 1 );

    m_pd3dDevice->EndZPass();

    const FLOAT WaterHeight = 0.0015f;
    matWorld = XMMatrixTranslation( 0, WaterHeight, 0 ) * m_matScaling;
    matVP = matWorld * matView * matProjection;
    matVP = XMMatrixTranspose( matVP );
    m_pd3dDevice->SetVertexShaderConstantF( 0, (FLOAT*)&matVP, 4 );

    m_pd3dDevice->SetPixelShaderConstantF( 0, (FLOAT*)&EdgeColor, 1 );
    m_pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_HIGHPRECISIONBLENDENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
    m_pd3dDevice->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );

    m_pd3dDevice->SetStreamSource( 0, m_pVBWater, 0, sizeof(TerrainVertex) );
    m_pd3dDevice->DrawPrimitive( D3DPT_QUADLIST, 0, 1 );

    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE, TRUE );
}
