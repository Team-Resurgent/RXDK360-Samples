//--------------------------------------------------------------------------------------
// CVarianceShadowMap.cpp
//
// A helper class for rendering variance shadowmaps (VSM).
//
// XNA Developer Connection.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include <xtl.h>
#include <xgraphics.h>
#include <xboxmath.h>
#include <AtgUtil.h>
#include "AdvancedLighting.h"
#include "CVarianceShadowMap.h"


//--------------------------------------------------------------------------------------
// Name: CVarianceShadowMap() constructor
//--------------------------------------------------------------------------------------
CVarianceShadowMap::CVarianceShadowMap() : CShadowMap()
{
    m_pCopyDepthToVariancePS = NULL;
    m_pHorizontalBlurDepthToVariancePS = NULL;
    m_pVerticalBlurDepthToVariancePS = NULL;
    m_pVarianceShadowMapTexture = NULL;
    m_pPostProcess = NULL;
}


//--------------------------------------------------------------------------------------
// Name: Initialize()
// Desc: Load shaders, create textures and rendertargets
//--------------------------------------------------------------------------------------
HRESULT CVarianceShadowMap::Initialize( ATG::PostProcess* pPostProcess,
                                        const DWORD dwShadowMapSize )
{
    m_pPostProcess = pPostProcess;

    // Load the shaders
    RETURN_ON_FAIL( ATG::LoadPixelShader( "game:\\Media\\Shaders\\CopyDepthToVariance.xpu", 
                                          &m_pCopyDepthToVariancePS ) );

    RETURN_ON_FAIL( ATG::LoadPixelShader( "game:\\Media\\Shaders\\HorizontalBlurDepthToVariance.xpu", 
                                          &m_pHorizontalBlurDepthToVariancePS ) );

    RETURN_ON_FAIL( ATG::LoadPixelShader( "game:\\Media\\Shaders\\VerticalBlurDepthToVariance.xpu", 
                                          &m_pVerticalBlurDepthToVariancePS ) );

    // Initialize the base class
    RETURN_ON_FAIL( CShadowMap::Initialize( dwShadowMapSize ) );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: CreateTextures()
// Desc: Create any textures needed for shadows.
//--------------------------------------------------------------------------------------
HRESULT CVarianceShadowMap::CreateTextures()
{
    // Make sure the GPU is not using any surfaces we're about to release
    ATG::g_pd3dDevice->BlockUntilIdle();

    // Note: The GPU cannot resolve to packed mip levels. Therefore, we need to either
    // (1) Create the texture manually with the XGHEADEREX_NONPACKED flag, or
    // (2) Make sure the smallest mip level we use is at least 32x32 in size

    // Create the variance shadow map, with a mip chain
    if( m_pVarianceShadowMapTexture )
    {
        m_pVarianceShadowMapTexture->Release();
    }

    // Note: Using D3DFMT_R16G16 will only give 10bit precision, so we're rather using
    // ATG::D3DFMT_R16G16_SIGNED_INTEGER with ExpAdjust = -15 to get 15bit precesion.
    // ATG::PostProcess will automatically create the appropriate rendertarget for this
    // format which is D3DFMT_G16R16_EDRAM, with Exp bias of +5 on write to render target,
    // Exp bias of +10 on resolve and Exp bias of -15 on sample

    // Compute the number of mip levels from the max shadow size down to 32x32
    DWORD dwNumMipLevels = 1 + ( DWORD )( logf( m_dwShadowMapSize / 32.0f ) / logf( 2.0f ) );

    // Create the variance shadow map (that holds depth and depth-squared) with a mip chain
    RETURN_ON_FAIL( ATG::g_pd3dDevice->CreateTexture( m_dwShadowMapSize, m_dwShadowMapSize, dwNumMipLevels,
                                                      0, ATG::D3DFMT_G16R16_SIGNED_INTEGER, D3DPOOL_DEFAULT, 
                                                      &m_pVarianceShadowMapTexture, NULL ) );

    // convert int to [-1,1]
    m_pVarianceShadowMapTexture->Format.ExpAdjust = -15;

    // Clear the shadowmap texture with 1.0
    m_pPostProcess->ClearTexture( m_pVarianceShadowMapTexture, 0xFFFFFFFF );

    // Note: for the conventional shadow map, we can either create it normally, with its own
    // separate memory, or we can use XGSetTextureHeader to have it share memory with the
    // variance shadow map

    // Create the conventional shadow map, sharing memory with the variance shadow map
    if( m_pShadowMapTexture == NULL )
    {
        m_pShadowMapTexture = new D3DTexture;
    }

    XGSetTextureHeaderEx( m_dwShadowMapSize, m_dwShadowMapSize, 1, 0, D3DFMT_D24S8, 0, 0, 0, 0,
                          0, m_pShadowMapTexture, NULL, NULL );

    DWORD dwBaseAddress = m_pVarianceShadowMapTexture->Format.BaseAddress;
    XGOffsetResourceAddress( m_pShadowMapTexture, ( VOID* )( dwBaseAddress << GPU_TEXTURE_ADDRESS_SHIFT ) );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: PreRender()
// Desc: Prepares for rendering geometry into a shadowmap by setting up the
//       rendertargets and renderstates
//--------------------------------------------------------------------------------------
HRESULT CVarianceShadowMap::PreRender()
{
    PIXBeginNamedEvent( 0, __FUNCTION__ );

    CShadowMap::PreRender();

    PIXEndNamedEvent(); // CVarianceShadowMap::PreRender

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: PostRender()
// Desc: Restores rendertargets and renderstates after shadowmap rendering is done
//--------------------------------------------------------------------------------------
HRESULT CVarianceShadowMap::PostRender()
{
    PIXBeginNamedEvent( 0, __FUNCTION__ );

    CShadowMap::PostRender();

    // Perf note: The steps here, which build and blur the variance shadow map, 
    // account for most of the performance difference from using a conventional
    // shadow map.

    BlurShadowMap();

    MipMapShadowMap();

    PIXEndNamedEvent(); // CVarianceShadowMap::PostRender

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: BlurShadowMap()
// Desc: Blur the variance shadow map using a 2-pass separable 5x5 Gaussian blur
//--------------------------------------------------------------------------------------
VOID CVarianceShadowMap::BlurShadowMap()
{
    PIXBeginNamedEvent( 0, "BlurShadowMap" );

    // Convert the D24S8 shadow map to the 2-channel (depth and depth-squared) VSM
    // and blur the results. We can combine the copy and blur operations in the
    // pixel shaders, as long as we compute depth and depth-squared before any
    // filtering.

    // 1st Pass: Copy the shadow map to the variance shadow map and while performing
    // a 5-tap horizontal Gaussian blur
    m_pPostProcess->CopyTexture( m_pShadowMapTexture, m_pVarianceShadowMapTexture,
                                 m_pHorizontalBlurDepthToVariancePS );

    // 2nd pass: Perform a 5-tap vertical Gaussian blur
    m_pPostProcess->CopyTexture( m_pVarianceShadowMapTexture, m_pVarianceShadowMapTexture,
                                 m_pVerticalBlurDepthToVariancePS );

    PIXEndNamedEvent(); // BlurShadowMap
}


//--------------------------------------------------------------------------------------
// Name: MipMapShadowMap()
// Desc: Build mip-maps for the variance shadow map
//--------------------------------------------------------------------------------------
VOID CVarianceShadowMap::MipMapShadowMap()
{
    PIXBeginNamedEvent( 0, "MipMapShadowMap" );

    m_pPostProcess->BuildMipMaps( m_pVarianceShadowMapTexture );

    PIXEndNamedEvent(); // MipMapShadowMap
}
