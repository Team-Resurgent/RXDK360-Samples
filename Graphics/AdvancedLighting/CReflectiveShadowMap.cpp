//--------------------------------------------------------------------------------------
// CReflectiveShadowMap.cpp
//
// A helper class for rendering reflective shadowmaps (RSM) that can be used for
// calculating indirect lighting.
//
// This class is derived from CVarianceShadowMap, but could just as well have been
// derived from CShadowMap. RSM has no dependancy on VSM. The sample just looks much
// nicer with VSM than regular shadowmaps.
//
// XNA Developer Connection.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include <xtl.h>
#include <xgraphics.h>
#include <xboxmath.h>
#include <AtgUtil.h>
#include "CReflectiveShadowMap.h"


// Texture formats for RSM textures
#define POSITIONS_FORMAT    D3DFMT_X2R10G10B10
#define DIRECTION_FORMAT    D3DFMT_X2R10G10B10
#define FLUX_FORMAT         D3DFMT_R5G6B5
#define DEPTH_FORMAT        D3DFMT_D24S8
#define TEXCOORD_FORMAT     D3DFMT_G16R16


//--------------------------------------------------------------------------------------
// Name: CReflectiveShadowMap() constructor
//--------------------------------------------------------------------------------------
CReflectiveShadowMap::CReflectiveShadowMap() : CVarianceShadowMap()
{
    m_pReflectiveShadowmapVS = NULL;
    m_pReflectiveShadowmapPS = NULL;
    m_pReflectiveShadowmapDebugPS = NULL;
    m_pImportanceSamplingPS = NULL;
    m_pGenerateSmallRSMTexturesPS = NULL;

    m_pWorldSpacePositionsTexture = NULL;
    m_pLightDirectionTexture = NULL;
    m_pFluxTexture = NULL;

    m_pWorldSpacePositionsSmallTexture = NULL;
    m_pLightDirectionSmallTexture = NULL;
    m_pFluxSmallTexture = NULL;
    m_pTexCoordTexture = NULL;

    m_pHammersleySamplingTexture = NULL;
    m_pImportanceSamplingTexture[ 0 ] = NULL;
    m_pImportanceSamplingTexture[ 1 ] = NULL;

    m_pWorldSpacePositionsRT = NULL;
    m_pLightDirectionRT = NULL;
    m_pFluxRT = NULL;
    m_pTexCoordRT = NULL;
    m_pImportanceSamplingRT = NULL;

    m_dwDoubleBufferCounter = 0;

    ZeroMemory( m_fHammersleySampling, sizeof( FLOAT ) * RSM_NUMSAMPLES * 4 );
    ZeroMemory( m_fImportanceSampling, sizeof( FLOAT ) * RSM_NUMSAMPLES * 4 );
    ZeroMemory( m_fLinearSampling, sizeof( FLOAT ) * RSM_NUMSAMPLES * 4 );

}


//--------------------------------------------------------------------------------------
// Name: Initialize()
// Desc: Load shaders, create textures and rendertargets
//--------------------------------------------------------------------------------------
HRESULT CReflectiveShadowMap::Initialize( ATG::PostProcess* pPostProcess,
                                          const DWORD dwShadowMapSize )
{
    // Load shaders
    RETURN_ON_FAIL( ATG::LoadVertexShader( "game:\\Media\\Shaders\\ReflectiveShadowmap.xvu",
                             &m_pReflectiveShadowmapVS ) );

    RETURN_ON_FAIL( ATG::LoadPixelShader( "game:\\Media\\Shaders\\ReflectiveShadowmap.xpu",
                             &m_pReflectiveShadowmapPS ) );

    RETURN_ON_FAIL( ATG::LoadPixelShader( "game:\\Media\\Shaders\\ReflectiveShadowmapDebug.xpu",
                             &m_pReflectiveShadowmapDebugPS ) );

    RETURN_ON_FAIL( ATG::LoadPixelShader( "game:\\Media\\Shaders\\ImportanceSampling.xpu",
                             &m_pImportanceSamplingPS ) );

    RETURN_ON_FAIL( ATG::LoadPixelShader( "game:\\Media\\Shaders\\GenerateSmallRSMTextures.xpu",
                             &m_pGenerateSmallRSMTexturesPS ) );

    // Initialize base class which will call this class's virtual methods for
    // CreateTexture() and CreateRenderTargets()
    RETURN_ON_FAIL( CVarianceShadowMap::Initialize( pPostProcess, dwShadowMapSize ) );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: CreateTextures()
// Desc: Create any textures needed for RSM
//--------------------------------------------------------------------------------------
HRESULT CReflectiveShadowMap::CreateTextures()
{
    // Create base class textures
    RETURN_ON_FAIL( CVarianceShadowMap::CreateTextures() );

    // Create RSM textures
    if( m_pWorldSpacePositionsTexture )
    {
        m_pWorldSpacePositionsTexture->Release();
    }

    RETURN_ON_FAIL( ATG::g_pd3dDevice->CreateTexture( m_dwShadowMapSize, m_dwShadowMapSize, 1,
                                                      0, POSITIONS_FORMAT, D3DPOOL_DEFAULT, 
                                                      &m_pWorldSpacePositionsTexture, NULL ) );

    // ClearTexture() clears the texture on the GPU
    m_pPostProcess->ClearTexture( m_pWorldSpacePositionsTexture );


    if( m_pLightDirectionTexture )
    {
        m_pLightDirectionTexture->Release();
    }

    RETURN_ON_FAIL( ATG::g_pd3dDevice->CreateTexture( m_dwShadowMapSize, m_dwShadowMapSize, 1,
                                                      0, DIRECTION_FORMAT, D3DPOOL_DEFAULT, 
                                                      &m_pLightDirectionTexture, NULL ) );

    m_pPostProcess->ClearTexture( m_pLightDirectionTexture );


    // Create mipmapped flux texture used for importance sampling
    if( m_pFluxTexture )
    {
        m_pFluxTexture->Release();
    }

    m_pFluxTexture = new D3DTexture;

    DWORD dwTextureSize = XGSetTextureHeaderEx( m_dwShadowMapSize, m_dwShadowMapSize, 0, 0,
                                                FLUX_FORMAT, 0, XGHEADEREX_NONPACKED, 0,
                                                XGHEADER_CONTIGUOUS_MIP_OFFSET, 0,
                                                m_pFluxTexture, NULL, NULL );

    VOID* pBuffer = XPhysicalAlloc( dwTextureSize, MAXULONG_PTR, 0,
                                    PAGE_READWRITE | PAGE_WRITECOMBINE );

    if( !pBuffer )
    {
        return E_OUTOFMEMORY;
    }

    XGOffsetResourceAddress( m_pFluxTexture, pBuffer );

    m_pPostProcess->ClearTexture( m_pFluxTexture );

    // Create the small 1D textures
    if( m_pWorldSpacePositionsSmallTexture )
    {
        m_pWorldSpacePositionsSmallTexture->Release();
    }

    RETURN_ON_FAIL( ATG::g_pd3dDevice->CreateTexture( RSM_NUMSAMPLES, 1, 1, 0, POSITIONS_FORMAT, D3DPOOL_DEFAULT, 
                                                      &m_pWorldSpacePositionsSmallTexture, NULL ) );

    m_pPostProcess->ClearTexture( m_pWorldSpacePositionsSmallTexture );


    if( m_pLightDirectionSmallTexture )
    {
        m_pLightDirectionSmallTexture->Release();
    }

    RETURN_ON_FAIL( ATG::g_pd3dDevice->CreateTexture( RSM_NUMSAMPLES, 1, 1, 0, DIRECTION_FORMAT, D3DPOOL_DEFAULT, 
                                                      &m_pLightDirectionSmallTexture, NULL ) );

    m_pPostProcess->ClearTexture( m_pLightDirectionSmallTexture );


    if( m_pFluxSmallTexture )
    {
        m_pFluxSmallTexture->Release();
    }

    RETURN_ON_FAIL( ATG::g_pd3dDevice->CreateTexture( RSM_NUMSAMPLES, 1, 1, 0, FLUX_FORMAT, D3DPOOL_DEFAULT, 
                                                      &m_pFluxSmallTexture, NULL ) );

    m_pPostProcess->ClearTexture( m_pFluxSmallTexture );


    if( m_pTexCoordTexture )
    {
        m_pTexCoordTexture->Release();
    }

    RETURN_ON_FAIL( ATG::g_pd3dDevice->CreateTexture( RSM_NUMSAMPLES, 1, 1, 0, TEXCOORD_FORMAT, D3DPOOL_DEFAULT, 
                                                      &m_pTexCoordTexture, NULL ) );

    m_pPostProcess->ClearTexture( m_pTexCoordTexture );


    if( m_pImportanceSamplingTexture[ 0 ] )
    {
        m_pImportanceSamplingTexture[ 0 ]->Release();
    }

    RETURN_ON_FAIL( ATG::g_pd3dDevice->CreateTexture( RSM_SAMPLESIZE, RSM_SAMPLESIZE, 1, 0, TEXCOORD_FORMAT, 0,
                                                      &m_pImportanceSamplingTexture[ 0 ], NULL ) );

    m_pPostProcess->ClearTexture( m_pImportanceSamplingTexture[ 0 ] );


    if( m_pImportanceSamplingTexture[ 1 ] )
    {
        m_pImportanceSamplingTexture[ 1 ]->Release();
    }

    RETURN_ON_FAIL( ATG::g_pd3dDevice->CreateTexture( RSM_SAMPLESIZE, RSM_SAMPLESIZE, 1, 0, TEXCOORD_FORMAT, 0,
                                                      &m_pImportanceSamplingTexture[ 1 ], NULL ) );

    m_pPostProcess->ClearTexture( m_pImportanceSamplingTexture[ 1 ] );


    RETURN_ON_FAIL( GenerateHammersleySamplingTexture() );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: GenerateHammersleySamplingTexture()
// Desc: Generates a uniformly distributed and stochastic-looking sampling pattern.
//       See "Sampling with Hammersley and Halton Points" in Journal of Graphics Tools,
//       vol.2, no.2, 1997, pp 9-24
//--------------------------------------------------------------------------------------
HRESULT CReflectiveShadowMap::GenerateHammersleySamplingTexture()
{
    // Generate the Hammersley sampling points
    FLOAT p, u, v;
    DWORD k, kk;
    DWORD dwPos = 0;

    for( k = 0; k < RSM_NUMSAMPLES; ++k )
    {
        u = 0;

        for( p = 0.5f, kk = k; kk; p *= 0.5f, kk >>= 1 )
        {
            if( kk & 1 )   // kk mod 2 == 1
            {
                u += p;
            }
        }

        v = ( k + 0.5f ) / RSM_NUMSAMPLES;

        m_fHammersleySampling[ dwPos ][ 0 ] = u;
        m_fHammersleySampling[ dwPos ][ 1 ] = v;
        m_fHammersleySampling[ dwPos ][ 2 ] = 1.0f;
        m_fHammersleySampling[ dwPos ][ 3 ] = RSM_NUMSAMPLES;

        dwPos++;
    }

    // Make sure the GPU is not using any surfaces we're about to release
    ATG::g_pd3dDevice->BlockUntilIdle();

    // Create the texture
    if( m_pHammersleySamplingTexture )
    {
        m_pHammersleySamplingTexture->Release();
    }

    RETURN_ON_FAIL( ATG::g_pd3dDevice->CreateTexture( RSM_SAMPLESIZE, RSM_SAMPLESIZE, 1, 0, D3DFMT_LIN_G16R16, 0,
     &m_pHammersleySamplingTexture, NULL ) );

    D3DLOCKED_RECT LockRect;
    m_pHammersleySamplingTexture->LockRect( 0, &LockRect, NULL, 0 );

    BYTE* pBits = ( BYTE* )LockRect.pBits;
    dwPos = 0;

    // Fill the D3DFMT_LIN_G16R16 texture with the created Hammersley sampling points
    for( DWORD j = 0; j < RSM_SAMPLESIZE; ++j )
    {
        for( DWORD i = 0; i < RSM_SAMPLESIZE; ++i )
        {
            WORD* pPixel = ( WORD* )( pBits + j * LockRect.Pitch + i * 4 );

            pPixel[ 0 ] = WORD( m_fHammersleySampling[ dwPos ][ 1 ] * 65535.0f );
            pPixel[ 1 ] = WORD( m_fHammersleySampling[ dwPos ][ 0 ] * 65535.0f );

            dwPos++;
        }
    }

    m_pHammersleySamplingTexture->UnlockRect( 0 );

    // Generate the texture coordinates for sampling a 1D texture
    for( DWORD i = 0; i < RSM_NUMSAMPLES; ++i )
    {
        m_fLinearSampling[ i ][ 0 ] = i / ( FLOAT )( RSM_NUMSAMPLES );
        m_fLinearSampling[ i ][ 1 ] = 0;
        m_fLinearSampling[ i ][ 2 ] = 0;
        m_fLinearSampling[ i ][ 3 ] = RSM_NUMSAMPLES;
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: CreateRenderTargets()
// Desc: Create any rendertargets needed for RSM
//--------------------------------------------------------------------------------------
HRESULT CReflectiveShadowMap::CreateRenderTargets()
{
    D3DSURFACE_PARAMETERS SurfaceParams = { 0 };

    RETURN_ON_FAIL( ATG::g_pd3dDevice->CreateDepthStencilSurface( m_dwShadowMapSize, m_dwShadowMapSize, DEPTH_FORMAT, 
                                                                  D3DMULTISAMPLE_NONE, 0, FALSE, 
                                                                  &m_pShadowMapDepthSurface, &SurfaceParams ) );

    SurfaceParams.Base += XGSurfaceSize( m_dwShadowMapSize, m_dwShadowMapSize, DEPTH_FORMAT, D3DMULTISAMPLE_NONE );

    RETURN_ON_FAIL( ATG::g_pd3dDevice->CreateRenderTarget( m_dwShadowMapSize, m_dwShadowMapSize, POSITIONS_FORMAT,
                                                           D3DMULTISAMPLE_NONE, 0, FALSE, 
                                                           &m_pWorldSpacePositionsRT, &SurfaceParams ) );

    SurfaceParams.Base += XGSurfaceSize( m_dwShadowMapSize, m_dwShadowMapSize, POSITIONS_FORMAT, D3DMULTISAMPLE_NONE );

    RETURN_ON_FAIL( ATG::g_pd3dDevice->CreateRenderTarget( m_dwShadowMapSize, m_dwShadowMapSize, DIRECTION_FORMAT, 
                                                           D3DMULTISAMPLE_NONE, 0, FALSE, 
                                                           &m_pLightDirectionRT, &SurfaceParams ) );

    SurfaceParams.Base += XGSurfaceSize( m_dwShadowMapSize, m_dwShadowMapSize, DIRECTION_FORMAT, D3DMULTISAMPLE_NONE );

    RETURN_ON_FAIL( ATG::g_pd3dDevice->CreateRenderTarget( m_dwShadowMapSize, m_dwShadowMapSize, D3DFMT_X8R8G8B8,
                                                           D3DMULTISAMPLE_NONE, 0, FALSE, 
                                                           &m_pFluxRT, &SurfaceParams ) );

    SurfaceParams.Base += XGSurfaceSize( m_dwShadowMapSize, m_dwShadowMapSize, D3DFMT_X8R8G8B8, D3DMULTISAMPLE_NONE );

    RETURN_ON_FAIL( ATG::g_pd3dDevice->CreateRenderTarget( 100, 32, D3DFMT_G16R16_EDRAM,
                                                           D3DMULTISAMPLE_NONE, 0, FALSE, 
                                                           &m_pTexCoordRT, &SurfaceParams ) );

    RETURN_ON_FAIL( ATG::g_pd3dDevice->CreateRenderTarget( 32, 32, D3DFMT_G16R16_EDRAM,
                                                           D3DMULTISAMPLE_NONE, 0, FALSE, 
                                                           &m_pImportanceSamplingRT, NULL ) );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: PreRender()
// Desc: Prepares for rendering geometry into a shadowmap by setting up the
//       rendertargets and renderstates
//--------------------------------------------------------------------------------------
HRESULT CReflectiveShadowMap::PreRender()
{
    PIXBeginNamedEvent( 0, __FUNCTION__ );

    // Save the back buffer and depth buffer
    ATG::g_pd3dDevice->GetRenderTarget( 0, &m_pCurrentBackBufferSurface );
    ATG::g_pd3dDevice->GetDepthStencilSurface( &m_pCurrentDepthBufferSurface );

    // Set render targets
    ATG::g_pd3dDevice->SetRenderTarget( 0, m_pWorldSpacePositionsRT );
    ATG::g_pd3dDevice->SetRenderTarget( 1, m_pLightDirectionRT );
    ATG::g_pd3dDevice->SetRenderTarget( 2, m_pFluxRT );
    ATG::g_pd3dDevice->SetDepthStencilSurface( m_pShadowMapDepthSurface );
    ATG::g_pd3dDevice->ClearF( D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL, NULL, NULL, 1.0f, 0L );

    // Initialize some render states
    ATG::g_pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );
    ATG::g_pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE, TRUE );
    ATG::g_pd3dDevice->SetRenderState( D3DRS_ZFUNC, D3DCMP_LESSEQUAL );
    ATG::g_pd3dDevice->SetRenderState( D3DRS_HIZENABLE, D3DHIZ_ENABLE );
    ATG::g_pd3dDevice->SetRenderState( D3DRS_DEPTHBIAS, ATG::FtoDW( 0.001f ) );
    ATG::g_pd3dDevice->SetRenderState( D3DRS_SLOPESCALEDEPTHBIAS, ATG::FtoDW( 2.0f ) );
    ATG::g_pd3dDevice->SetRenderState( D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_ALL );

    // Set the shaders
    ATG::g_pd3dDevice->SetVertexShader( m_pReflectiveShadowmapVS );
    ATG::g_pd3dDevice->SetPixelShader( m_pReflectiveShadowmapPS );

    PIXEndNamedEvent(); // CReflectiveShadowMap::PreRender

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: PostRender()
// Desc: Restores rendertargets and renderstates after shadowmap rendering is done
//--------------------------------------------------------------------------------------
HRESULT CReflectiveShadowMap::PostRender()
{
    PIXBeginNamedEvent( 0, __FUNCTION__ );

    // Bias shader registers towards pixel shader, since we're going to do a bit of
    // pixel shader work now. This is the highest that we can bias towards the
    // pixel shader, since we need at least 16 GPRs on the vertex shader
    ATG::g_pd3dDevice->SetShaderGPRAllocation( 0, 16, GPU_GPRS - 16 );

    ATG::g_pd3dDevice->SetRenderState( D3DRS_HIZENABLE, D3DHIZ_AUTOMATIC );

    // Resolve the rendertargets to textures
    ATG::g_pd3dDevice->Resolve( D3DRESOLVE_RENDERTARGET0, NULL, m_pWorldSpacePositionsTexture,
                                NULL, 0, 0, NULL, 1.0f, 0, NULL );

    ATG::g_pd3dDevice->Resolve( D3DRESOLVE_RENDERTARGET1, NULL, m_pLightDirectionTexture,
                                NULL, 0, 0, NULL, 1.0f, 0, NULL );

    ATG::g_pd3dDevice->Resolve( D3DRESOLVE_RENDERTARGET2, NULL, m_pFluxTexture,
                                NULL, 0, 0, NULL, 1.0f, 0, NULL );

    // Resolve the depth buffer to a texture
    CShadowMap::PostRender();

    // Warp the Hammersley sampling points based on flux intensity
    ImportanceSampling();

    // We need the original depth buffer for generating the small RSM textures, so this step
    // needs to be before BlurShadowMap()
    GenerateSmallRSMTextures();

    BlurShadowMap();

    MipMapShadowMap();

    PIXEndNamedEvent(); // CReflectiveShadowMap::PostRender

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: ImportanceSampling()
// Desc: Warp the uniformly distributed Hammersly sampling points using the intensity
//       of the flux. See "Splatting of Diffuse and Glossy Indirect Illumination" in
//       ShaderX5 for more details.
//--------------------------------------------------------------------------------------
HRESULT CReflectiveShadowMap::ImportanceSampling()
{
    PIXBeginNamedEvent( 0, "ImportanceSampling" );

    PIXBeginNamedEvent( 0, "Mipmap Flux Texture" );
    m_pPostProcess->BuildMipMaps( m_pFluxTexture );
    PIXEndNamedEvent(); // Mipmap Flux Texture

    ATG::g_pd3dDevice->SetRenderTarget( 0, m_pImportanceSamplingRT );

    ATG::g_pd3dDevice->SetTexture( SAMPLER_RSMFluxTexture, m_pFluxTexture );
    ATG::g_pd3dDevice->SetTexture( SAMPLER_HammersleyTexture, m_pHammersleySamplingTexture );

    ATG::g_pd3dDevice->SetSamplerState( SAMPLER_RSMFluxTexture, D3DSAMP_MINFILTER, D3DTEXF_POINT );
    ATG::g_pd3dDevice->SetSamplerState( SAMPLER_RSMFluxTexture, D3DSAMP_MAGFILTER, D3DTEXF_POINT );
    ATG::g_pd3dDevice->SetSamplerState( SAMPLER_RSMFluxTexture, D3DSAMP_MIPFILTER, D3DTEXF_POINT );

    ATG::g_pd3dDevice->SetSamplerState( SAMPLER_HammersleyTexture, D3DSAMP_MINFILTER, D3DTEXF_POINT );
    ATG::g_pd3dDevice->SetSamplerState( SAMPLER_HammersleyTexture, D3DSAMP_MAGFILTER, D3DTEXF_POINT );
    ATG::g_pd3dDevice->SetSamplerState( SAMPLER_HammersleyTexture, D3DSAMP_MIPFILTER, D3DTEXF_POINT );

    ATG::g_pd3dDevice->SetPixelShader( m_pImportanceSamplingPS );

    // Do the importance sampling on the GPU
    m_pPostProcess->DrawScreenSpaceQuad( ( FLOAT )RSM_SAMPLESIZE, ( FLOAT )RSM_SAMPLESIZE, 1.0f, 1.0f );

    ATG::g_pd3dDevice->Resolve( D3DRESOLVE_RENDERTARGET0, NULL,
                                m_pImportanceSamplingTexture[ m_dwDoubleBufferCounter ],
                                NULL, 0, 0, NULL, 1.0f, 0, NULL );

    m_dwDoubleBufferCounter = 1 - m_dwDoubleBufferCounter;

    // Copy the warped sampling points to memory to be used as shader constants
    D3DLOCKED_RECT LockRect;
    m_pImportanceSamplingTexture[ m_dwDoubleBufferCounter ]->LockRect( 0, &LockRect, NULL, D3DLOCK_READONLY );

    DWORD* pBits = ( DWORD* )LockRect.pBits;
    DWORD dwPos = 0;

    for( DWORD y = 0; y < RSM_SAMPLESIZE; ++y )
    {
        for( DWORD x = 0; x < RSM_SAMPLESIZE; ++x )
        {
            // This is not a linear texture, so we need to use XGAddress2DTiledOffset()
            // to get the correct offset for each pixel
            DWORD dwOffset = XGAddress2DTiledOffset( x, y, RSM_SAMPLESIZE, 4 );
            DWORD* pPixel = pBits + dwOffset;
            DWORD dwPixel = *pPixel;

            m_fImportanceSampling[ dwPos ][ 0 ] = ( ( dwPixel & 0x0000ffff ) >> 0 ) / 65535.0f;
            m_fImportanceSampling[ dwPos ][ 1 ] = ( ( dwPixel & 0xffff0000 ) >> 16 ) / 65535.0f;
            m_fImportanceSampling[ dwPos ][ 2 ] = 0;
            m_fImportanceSampling[ dwPos ][ 3 ] = RSM_NUMSAMPLES;

            dwPos++;
        }
    }

    m_pImportanceSamplingTexture[ m_dwDoubleBufferCounter ]->UnlockRect( 0 );

    PIXEndNamedEvent(); // ImportanceSampling

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: GenerateSmallRSMTextures()
// Desc: To calcuate the indirect lighting, we need to loop and do a texture fetch for
//       each virtual point light's world position, light direction and flux. It is much
//       more efficient to copy these into small 1D textures for better cache utilization.
//--------------------------------------------------------------------------------------
HRESULT CReflectiveShadowMap::GenerateSmallRSMTextures()
{
    PIXBeginNamedEvent( 0, "GenerateSmallRSMTextures" );

    // Set rendertargets
    ATG::g_pd3dDevice->SetRenderTarget( 0, m_pWorldSpacePositionsRT );
    ATG::g_pd3dDevice->SetRenderTarget( 1, m_pLightDirectionRT );
    ATG::g_pd3dDevice->SetRenderTarget( 2, m_pFluxRT );
    ATG::g_pd3dDevice->SetRenderTarget( 3, m_pTexCoordRT );

    // Set textures
    ATG::g_pd3dDevice->SetTexture( SAMPLER_RSMPositionTexture, m_pWorldSpacePositionsTexture );
    ATG::g_pd3dDevice->SetTexture( SAMPLER_RSMLightDirTexture, m_pLightDirectionTexture );
    ATG::g_pd3dDevice->SetTexture( SAMPLER_RSMFluxTexture, m_pFluxTexture );
    ATG::g_pd3dDevice->SetTexture( SAMPLER_RSMPositionSmallTexture, m_pWorldSpacePositionsSmallTexture );
    ATG::g_pd3dDevice->SetTexture( SAMPLER_RSMLightDirSmallTexture, m_pLightDirectionSmallTexture );
    ATG::g_pd3dDevice->SetTexture( SAMPLER_RSMFluxSmallTexture, m_pFluxSmallTexture );
    ATG::g_pd3dDevice->SetTexture( SAMPLER_RSMDepthTexture, m_pShadowMapTexture );

    // Set sampler states
    ATG::g_pd3dDevice->SetSamplerState( SAMPLER_RSMPositionTexture, D3DSAMP_MINFILTER, D3DTEXF_POINT );
    ATG::g_pd3dDevice->SetSamplerState( SAMPLER_RSMPositionTexture, D3DSAMP_MAGFILTER, D3DTEXF_POINT );
    ATG::g_pd3dDevice->SetSamplerState( SAMPLER_RSMPositionTexture, D3DSAMP_MIPFILTER, D3DTEXF_POINT );

    ATG::g_pd3dDevice->SetSamplerState( SAMPLER_RSMLightDirTexture, D3DSAMP_MINFILTER, D3DTEXF_POINT );
    ATG::g_pd3dDevice->SetSamplerState( SAMPLER_RSMLightDirTexture, D3DSAMP_MAGFILTER, D3DTEXF_POINT );
    ATG::g_pd3dDevice->SetSamplerState( SAMPLER_RSMLightDirTexture, D3DSAMP_MIPFILTER, D3DTEXF_POINT );

    ATG::g_pd3dDevice->SetSamplerState( SAMPLER_RSMFluxTexture, D3DSAMP_MINFILTER, D3DTEXF_POINT );
    ATG::g_pd3dDevice->SetSamplerState( SAMPLER_RSMFluxTexture, D3DSAMP_MAGFILTER, D3DTEXF_POINT );
    ATG::g_pd3dDevice->SetSamplerState( SAMPLER_RSMFluxTexture, D3DSAMP_MIPFILTER, D3DTEXF_POINT );

    ATG::g_pd3dDevice->SetSamplerState( SAMPLER_RSMPositionSmallTexture, D3DSAMP_MINFILTER, D3DTEXF_POINT );
    ATG::g_pd3dDevice->SetSamplerState( SAMPLER_RSMPositionSmallTexture, D3DSAMP_MAGFILTER, D3DTEXF_POINT );
    ATG::g_pd3dDevice->SetSamplerState( SAMPLER_RSMPositionSmallTexture, D3DSAMP_MIPFILTER, D3DTEXF_POINT );

    ATG::g_pd3dDevice->SetSamplerState( SAMPLER_RSMLightDirSmallTexture, D3DSAMP_MINFILTER, D3DTEXF_POINT );
    ATG::g_pd3dDevice->SetSamplerState( SAMPLER_RSMLightDirSmallTexture, D3DSAMP_MAGFILTER, D3DTEXF_POINT );
    ATG::g_pd3dDevice->SetSamplerState( SAMPLER_RSMLightDirSmallTexture, D3DSAMP_MIPFILTER, D3DTEXF_POINT );

    ATG::g_pd3dDevice->SetSamplerState( SAMPLER_RSMFluxSmallTexture, D3DSAMP_MINFILTER, D3DTEXF_POINT );
    ATG::g_pd3dDevice->SetSamplerState( SAMPLER_RSMFluxSmallTexture, D3DSAMP_MAGFILTER, D3DTEXF_POINT );
    ATG::g_pd3dDevice->SetSamplerState( SAMPLER_RSMFluxSmallTexture, D3DSAMP_MIPFILTER, D3DTEXF_POINT );

    ATG::g_pd3dDevice->SetSamplerState( SAMPLER_RSMDepthTexture, D3DSAMP_MINFILTER, D3DTEXF_POINT );
    ATG::g_pd3dDevice->SetSamplerState( SAMPLER_RSMDepthTexture, D3DSAMP_MAGFILTER, D3DTEXF_POINT );
    ATG::g_pd3dDevice->SetSamplerState( SAMPLER_RSMDepthTexture, D3DSAMP_MIPFILTER, D3DTEXF_POINT );

    ATG::g_pd3dDevice->SetPixelShaderConstantF( PSCONST_fSampling, ( FLOAT* )&m_fImportanceSampling, RSM_NUMSAMPLES );

    ATG::g_pd3dDevice->SetPixelShader( m_pGenerateSmallRSMTexturesPS );

    m_pPostProcess->DrawScreenSpaceQuad( ( FLOAT )RSM_NUMSAMPLES, ( FLOAT )1, 1.0f, 1.0f );

    // Resolve the rendertargets to textures
    ATG::g_pd3dDevice->Resolve( D3DRESOLVE_RENDERTARGET0, NULL, m_pWorldSpacePositionsSmallTexture,
                                NULL, 0, 0, NULL, 1.0f, 0, NULL );

    ATG::g_pd3dDevice->Resolve( D3DRESOLVE_RENDERTARGET1, NULL, m_pLightDirectionSmallTexture,
                                NULL, 0, 0, NULL, 1.0f, 0, NULL );

    ATG::g_pd3dDevice->Resolve( D3DRESOLVE_RENDERTARGET2, NULL, m_pFluxSmallTexture,
                                NULL, 0, 0, NULL, 1.0f, 0, NULL );

    ATG::g_pd3dDevice->Resolve( D3DRESOLVE_RENDERTARGET3, NULL, m_pTexCoordTexture,
                                NULL, 0, 0, NULL, 1.0f, 0, NULL );


    PIXEndNamedEvent(); // GenerateSmallRSMTextures

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: RenderSamplePoints()
// Desc: A debug helper function to render the sampling points. Blue points are the
//       Hammersley distrubution points, red points are the warped importance sampling
//       points and green points are the currently used sampling points.
//--------------------------------------------------------------------------------------
VOID CReflectiveShadowMap::RenderSamplePoints( const FLOAT fWidth, const FLOAT fHeight )
{
    ATG::g_pd3dDevice->SetSamplerState( SAMPLER_RSMTexCoordTexture, D3DSAMP_MINFILTER, D3DTEXF_POINT );
    ATG::g_pd3dDevice->SetSamplerState( SAMPLER_RSMTexCoordTexture, D3DSAMP_MAGFILTER, D3DTEXF_POINT );
    ATG::g_pd3dDevice->SetSamplerState( SAMPLER_RSMTexCoordTexture, D3DSAMP_MIPFILTER, D3DTEXF_POINT );
    ATG::g_pd3dDevice->SetTexture( SAMPLER_RSMTexCoordTexture, m_pTexCoordTexture );

    ATG::g_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
    ATG::g_pd3dDevice->SetRenderState( D3DRS_BLENDOP, D3DBLENDOP_ADD );
    ATG::g_pd3dDevice->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_ONE );
    ATG::g_pd3dDevice->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_ONE );

    ATG::g_pd3dDevice->SetPixelShaderConstantF( PSCONST_fSampling, ( FLOAT* )&m_fImportanceSampling, RSM_NUMSAMPLES );
    ATG::g_pd3dDevice->SetPixelShaderConstantF( PSCONST_fHammersleySampling, ( FLOAT* )&m_fHammersleySampling,
                                                RSM_NUMSAMPLES );

    ATG::g_pd3dDevice->SetPixelShader( m_pReflectiveShadowmapDebugPS );

    m_pPostProcess->DrawScreenSpaceQuad( fWidth, fHeight, 1.0f, 1.0f );

    ATG::g_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
}
