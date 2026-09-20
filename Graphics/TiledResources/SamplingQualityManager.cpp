//--------------------------------------------------------------------------------------
// SamplingQualityManager.cpp
//
// This class tracks the residency status of a single tiled resource, and generates a
// 2D texture where each texel represents the minimum allowed LOD for that region of the
// tiled resource.  The texture is regenerated frequently (every frame), so that
// residency changes can be smoothed over time.
//
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "SamplingQualityManager.h"
#include <AtgUtil.h>

D3DVertexShader* g_pVSQualityPassThru = NULL;
D3DPixelShader* g_pPSQualitySample2D = NULL;
D3DPixelShader* g_pPSQualitySample3D = NULL;
D3DVertexDeclaration* g_pQualityDecl = NULL;

const D3DFORMAT g_QualityTextureFormat = D3DFMT_L16;
const D3DFORMAT g_QualitySurfaceFormat = D3DFMT_G16R16_EDRAM;
const DWORD g_ResolveFlags = (DWORD)D3DRESOLVE_EXPONENTBIAS(-5);

//--------------------------------------------------------------------------------------
// Name: SamplingQualityManager constructor
//--------------------------------------------------------------------------------------
SamplingQualityManager::SamplingQualityManager( D3DTiledTexture* pResource, D3DDevice* pd3dDevice )
{
    m_pResource = pResource;

    // Get the page dimensions of the tiled resource's base level:
    D3DTILED_SURFACE_DESC BaseDesc;
    m_pResource->GetLevelDesc( 0, &BaseDesc );

    // Compute the width and height of the sampling quality map:
    const UINT TexWidth = BaseDesc.TileWidth;
    const UINT TexHeight = BaseDesc.TileHeight;
    if( m_pResource->GetType() == D3DSRTYPE_ARRAYTEXTURE )
    {
        D3DTiledArrayTexture* pSATex = (D3DTiledArrayTexture*)m_pResource;
        UINT TexArraySize = pSATex->GetArraySize();
        pd3dDevice->CreateArrayTexture( TexWidth, TexHeight, TexArraySize, 1, 0, g_QualityTextureFormat, D3DPOOL_DEFAULT, &m_pPageLODArrayTexture, NULL );
        m_pPageLODTexture = m_pPageLODArrayTexture;
        m_SliceCount = TexArraySize;
    }
    else
    {
        pd3dDevice->CreateTexture( TexWidth, TexHeight, 1, 0, g_QualityTextureFormat, D3DPOOL_DEFAULT, (D3DTexture**)&m_pPageLODTexture, NULL );
        m_pPageLODArrayTexture = NULL;
        m_SliceCount = 1;
    }

    m_UVScaleRender.x = (FLOAT)( BaseDesc.TileWidth * BaseDesc.TileTexelWidth ) / (FLOAT)BaseDesc.TexelWidth;
    m_UVScaleRender.y = (FLOAT)( BaseDesc.TileHeight * BaseDesc.TileTexelHeight ) / (FLOAT)BaseDesc.TexelHeight;
    m_UVScaleConstant[0] = 1.0f / m_UVScaleRender.x;
    m_UVScaleConstant[1] = 1.0f / m_UVScaleRender.y;
    m_UVScaleConstant[2] = 0;
    m_UVScaleConstant[3] = 0;

    D3DSURFACE_PARAMETERS SurfParams = { 0 };
    pd3dDevice->CreateRenderTarget( TexWidth, TexHeight, g_QualitySurfaceFormat, D3DMULTISAMPLE_NONE, 0, FALSE, &m_pPageLODSurface, &SurfParams );

    if( g_pVSQualityPassThru == NULL )
    {
        ATG::LoadVertexShader( "game:\\media\\shaders\\VSQualityPassThru.xvu", &g_pVSQualityPassThru, NULL );
        ATG::LoadPixelShader( "game:\\media\\shaders\\PSQualitySample2D.xpu", &g_pPSQualitySample2D, NULL );
        ATG::LoadPixelShader( "game:\\media\\shaders\\PSQualitySample3D.xpu", &g_pPSQualitySample3D, NULL );
        static const D3DVERTEXELEMENT9 VertexElements[] =
        {
            { 0,     0, D3DDECLTYPE_FLOAT2,     0,  D3DDECLUSAGE_POSITION,  0 },
            { 0,     8, D3DDECLTYPE_FLOAT2,     0,  D3DDECLUSAGE_TEXCOORD,  0 },
            D3DDECL_END()
        };

        pd3dDevice->CreateVertexDeclaration( VertexElements, &g_pQualityDecl );
    }

    m_pSliceChangingTime = new FLOAT[m_SliceCount];
    ZeroMemory( m_pSliceChangingTime, m_SliceCount * sizeof(FLOAT) );

    m_MipTransitionDuration = MIP_TRANSITION_TIME_SECONDS;

    m_FirstFrame = TRUE;
}

//--------------------------------------------------------------------------------------
// Name: SamplingQualityManager destructor
//--------------------------------------------------------------------------------------
SamplingQualityManager::~SamplingQualityManager()
{

}

//--------------------------------------------------------------------------------------
// Name: SamplingQualityManager::Render
// Desc: Renders a single update of the sampling quality map.
//--------------------------------------------------------------------------------------
VOID SamplingQualityManager::Render( D3DDevice* pd3dDevice, D3DTiledResourceDevice* pTiledResourceDevice, FLOAT fDeltaTime )
{
    PIXBeginNamedEvent( 0, "Sampling Quality Manager" );

    // Compute the amount of LOD change that will be made during this update, using the delta time.
    FLOAT LODIncrement = fDeltaTime / m_MipTransitionDuration;
    LODIncrement = min( 0.5f, LODIncrement );
    
    // Determine the maximum LOD for this resource:
    FLOAT MaxLOD = (FLOAT)( m_pResource->GetLevelCount() - 1 );

    XMFLOAT4 LODConstant( LODIncrement, MaxLOD, 0, 0 );

    const FLOAT RectVertices[] =
    {
        -1,  1, 0, 0,
         1,  1, m_UVScaleRender.x, 0,
        -1, -1, 0, m_UVScaleRender.y
    };

    pd3dDevice->SetRenderTarget( 0, m_pPageLODSurface );
    pd3dDevice->SetRenderTarget( 1, NULL );
    pd3dDevice->SetRenderTarget( 2, NULL );
    pd3dDevice->SetRenderTarget( 3, NULL );
    pd3dDevice->SetDepthStencilSurface( NULL );

    UINT SliceCount = 1;
    if( m_pPageLODArrayTexture != NULL )
    {
        SliceCount = m_pPageLODArrayTexture->GetArraySize();
    }

    if( m_FirstFrame )
    {
        pd3dDevice->Clear( 0, NULL, D3DCLEAR_TARGET0, 0xFFFFFFFF, 0, 0 );
        for( UINT i = 0; i < SliceCount; ++i )
        {
            pd3dDevice->Resolve( D3DRESOLVE_RENDERTARGET0, NULL, m_pPageLODTexture, NULL, 0, i, NULL, 0, 0, NULL );
        }
        m_FirstFrame = FALSE;
    }

    pd3dDevice->SetRenderState( D3DRS_ZENABLE, FALSE );
    pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE, FALSE );
    pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
    pd3dDevice->SetRenderState( D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_ALL );
    pd3dDevice->SetRenderState( D3DRS_HALFPIXELOFFSET, TRUE );

    pd3dDevice->SetVertexDeclaration( g_pQualityDecl );
    pd3dDevice->SetVertexShader( g_pVSQualityPassThru );

    if( SliceCount > 1 )
    {
        pd3dDevice->SetPixelShader( g_pPSQualitySample3D );
    }
    else
    {
        pd3dDevice->SetPixelShader( g_pPSQualitySample2D );
    }

    pd3dDevice->SetTexture( 0, m_pPageLODTexture );
    pd3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_POINT );
    pd3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_POINT );
    pd3dDevice->SetSamplerState( 0, D3DSAMP_MIPFILTER, D3DTEXF_POINT );
    pTiledResourceDevice->SetTexture( 1, m_pResource );

    for( UINT i = 0; i < SliceCount; ++i )
    {
        if( m_pSliceChangingTime[i] <= 0.0f )
        {
            continue;
        }

        m_pSliceChangingTime[i] = max( 0.0f, m_pSliceChangingTime[i] - fDeltaTime ); 

        LODConstant.z = ( (FLOAT)i + 0.5f ) / (FLOAT)SliceCount;
        pd3dDevice->SetPixelShaderConstantF( 0, (FLOAT*)&LODConstant, 1 );

        pd3dDevice->DrawPrimitiveUP( D3DPT_RECTLIST, 1, RectVertices, 4 * sizeof(FLOAT) );

        pd3dDevice->Resolve( D3DRESOLVE_RENDERTARGET0 | g_ResolveFlags, NULL, m_pPageLODTexture, NULL, 0, i, NULL, 0, 0, NULL );
    }

    PIXEndNamedEvent();
}

//--------------------------------------------------------------------------------------
// Name: SamplingQualityManager::TileLoaded
// Desc: Listens for page updates to the tiled texture that is attached to this sampling
//       quality manager.  If this tiled texture is being updated, increment the countdown
//       timer for the affected slice of this tiled texture.
//--------------------------------------------------------------------------------------
VOID SamplingQualityManager::TileLoaded( const TrackedTileID* pPageID )
{
    if( pPageID->pResource == m_pResource )
    {
        UINT SliceIndex = pPageID->ArraySlice;
        ASSERT( SliceIndex < m_SliceCount );
        m_pSliceChangingTime[SliceIndex] = max( m_pSliceChangingTime[SliceIndex], m_MipTransitionDuration * 3.0f );
    }
}

//--------------------------------------------------------------------------------------
// Name: SamplingQualityManager::TileUnloaded
// Desc: Listens for page updates to the tiled texture that is attached to this sampling
//       quality manager.  If this tiled texture is being updated, increment the countdown
//       timer for the affected slice of this tiled texture.
//--------------------------------------------------------------------------------------
VOID SamplingQualityManager::TileUnloaded( const TrackedTileID* pPageID )
{
    if( pPageID->pResource == m_pResource )
    {
        UINT SliceIndex = pPageID->ArraySlice;
        ASSERT( SliceIndex < m_SliceCount );
        m_pSliceChangingTime[SliceIndex] = max( m_pSliceChangingTime[SliceIndex], m_MipTransitionDuration * 1.0f );
    }
}