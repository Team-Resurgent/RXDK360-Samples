//--------------------------------------------------------------------------------------
// CShadowMap.cpp
//
// A helper class for rendering shadowmaps.
//
// XNA Developer Connection.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include <xtl.h>
#include <xgraphics.h>
#include <xboxmath.h>
#include <AtgUtil.h>
#include <AtgApp.h>
#include "AdvancedLighting.h"
#include "CShadowMap.h"


//--------------------------------------------------------------------------------------
// Name: CShadowMap() constructor
//--------------------------------------------------------------------------------------
CShadowMap::CShadowMap()
{
    m_pDepthOnlyVS = NULL;
    m_pShadowMapTexture = NULL;
    m_pShadowMapDepthSurface = NULL;
    m_pCurrentBackBufferSurface = NULL;
    m_pCurrentDepthBufferSurface = NULL;
    m_dwShadowMapSize = 0;
}


//--------------------------------------------------------------------------------------
// Name: Initialize()
// Desc: Load shaders, create textures and rendertargets
//--------------------------------------------------------------------------------------
HRESULT CShadowMap::Initialize( const DWORD dwShadowMapSize )
{
    m_dwShadowMapSize = dwShadowMapSize;

    // Load the shaders
    RETURN_ON_FAIL( ATG::LoadVertexShader( "game:\\Media\\Shaders\\DepthOnly.xvu", &m_pDepthOnlyVS ) );

    // Create textures
    RETURN_ON_FAIL( CreateTextures() );

    // Create rendertargets
    RETURN_ON_FAIL( CreateRenderTargets() );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: CreateTextures()
// Desc: Create any textures needed for shadows.
//--------------------------------------------------------------------------------------
HRESULT CShadowMap::CreateTextures()
{
    // Make sure the GPU is not using any surfaces we're about to release
    ATG::g_pd3dDevice->BlockUntilIdle();

    // Create the shadowmap texture
    if( m_pShadowMapTexture )
    {
        m_pShadowMapTexture->Release();
    }

    RETURN_ON_FAIL( ATG::g_pd3dDevice->CreateTexture( m_dwShadowMapSize, m_dwShadowMapSize, 1, 0,
     D3DFMT_D24S8, D3DPOOL_DEFAULT, &m_pShadowMapTexture, NULL ) );
    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: CreateRenderTargets()
// Desc: Create any rendertargets needed for shadows.
//--------------------------------------------------------------------------------------
HRESULT CShadowMap::CreateRenderTargets()
{
    // Make sure the GPU is not using any surfaces we're about to release
    ATG::g_pd3dDevice->BlockUntilIdle();

    // Create the shadowmap depth surface
    if( m_pShadowMapDepthSurface )
    {
        m_pShadowMapDepthSurface->Release();
    }

    RETURN_ON_FAIL( ATG::g_pd3dDevice->CreateDepthStencilSurface( m_dwShadowMapSize, m_dwShadowMapSize,
                                                                  D3DFMT_D24S8, D3DMULTISAMPLE_NONE, 0,
                                                                   FALSE, &m_pShadowMapDepthSurface, NULL ) );
    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: PreRender()
// Desc: Prepares for rendering geometry into a shadowmap by setting up the
//       rendertargets and renderstates
//--------------------------------------------------------------------------------------
HRESULT CShadowMap::PreRender()
{
    PIXBeginNamedEvent( 0, __FUNCTION__ );

    // Save the back buffer and depth buffer
    ATG::g_pd3dDevice->GetRenderTarget( 0, &m_pCurrentBackBufferSurface );
    ATG::g_pd3dDevice->GetDepthStencilSurface( &m_pCurrentDepthBufferSurface );

    // Set render targets
    ATG::g_pd3dDevice->SetRenderTarget( 0, NULL );
    ATG::g_pd3dDevice->SetDepthStencilSurface( m_pShadowMapDepthSurface );
    ATG::g_pd3dDevice->ClearF( D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL, NULL, NULL, 1.0f, 0L );

    // Initialize some render states
    ATG::g_pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );
    ATG::g_pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE, TRUE );
    ATG::g_pd3dDevice->SetRenderState( D3DRS_ZFUNC, D3DCMP_LESSEQUAL );
    ATG::g_pd3dDevice->SetRenderState( D3DRS_DEPTHBIAS, ATG::FtoDW( 0.001f ) );
    ATG::g_pd3dDevice->SetRenderState( D3DRS_SLOPESCALEDEPTHBIAS, ATG::FtoDW( 2.0f ) );
    ATG::g_pd3dDevice->SetRenderState( D3DRS_COLORWRITEENABLE, 0 );

    // Set the shaders
    ATG::g_pd3dDevice->SetVertexShader( m_pDepthOnlyVS );
    ATG::g_pd3dDevice->SetPixelShader( NULL );

    PIXEndNamedEvent(); // CShadowMap::PreRender

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: PostRender()
// Desc: Restores rendertargets and renderstates after shadowmap rendering is done
//--------------------------------------------------------------------------------------
HRESULT CShadowMap::PostRender()
{
    PIXBeginNamedEvent( 0, __FUNCTION__ );

    // Resolve depth to texture
    ATG::g_pd3dDevice->Resolve( D3DRESOLVE_DEPTHSTENCIL, NULL, m_pShadowMapTexture,
                                NULL, 0, 0, NULL, 1.0f, 0, NULL );

    // Restore states
    ATG::g_pd3dDevice->SetRenderState( D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_ALL );
    ATG::g_pd3dDevice->SetRenderState( D3DRS_DEPTHBIAS, 0 );
    ATG::g_pd3dDevice->SetRenderState( D3DRS_SLOPESCALEDEPTHBIAS, 0 );

    // Restore the main render targets
    ATG::g_pd3dDevice->SetRenderTarget( 0, m_pCurrentBackBufferSurface );
    ATG::g_pd3dDevice->SetDepthStencilSurface( m_pCurrentDepthBufferSurface );

    if( m_pCurrentBackBufferSurface )
    {
        m_pCurrentBackBufferSurface->Release();
    }

    if( m_pCurrentDepthBufferSurface )
    {
        m_pCurrentDepthBufferSurface->Release();
    }

    PIXEndNamedEvent(); // CShadowMap::PostRender

    return S_OK;
}
