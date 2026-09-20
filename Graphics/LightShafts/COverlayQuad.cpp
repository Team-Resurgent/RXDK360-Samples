//--------------------------------------------------------------------------------------
// COverlayQuad.cpp
//
// Simple quad overlay for displaying textures 
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
// ATI 3D Application Research Group. 
// Copyright (C) ATI Research, Inc. All rights reserved. 
//--------------------------------------------------------------------------------------
#include <xtl.h>
#include <xgraphics.h>
#include <xboxmath.h>
#include <AtgUtil.h>
#include "COverlayQuad.h"
#include <AtgApp.h>


const DWORD                     NUM_BLUR_TAPS = 12;
static FLOAT g_vFilterTaps[NUM_BLUR_TAPS][4] =
{
    {-0.326212f, -0.405805f },
    {-0.840144f, -0.073580f },
    {-0.695914f,  0.457137f },
    {-0.203345f,  0.620716f },
    { 0.962340f, -0.194983f },
    { 0.473434f, -0.480026f },
    { 0.519456f,  0.767022f },
    { 0.185461f, -0.893124f },
    { 0.507431f,  0.064425f },
    { 0.896420f,  0.412458f },
    {-0.321940f, -0.932615f },
    {-0.791559f, -0.597705f }
};


//--------------------------------------------------------------------------------------
// Types
//--------------------------------------------------------------------------------------
struct OVERLAY_VERTEX
{
    XMFLOAT4 vPos;
    XMFLOAT2 vTex;
};

LPDIRECT3DVERTEXBUFFER9         COverlayQuad::m_pOverlayVB = NULL;
LPDIRECT3DVERTEXDECLARATION9    COverlayQuad::m_pOverlayVertexDecl = NULL;
LPDIRECT3DVERTEXSHADER9         COverlayQuad::m_pOverlayVS = NULL;
LPDIRECT3DPIXELSHADER9          CShadowOverlayQuad::m_pShadowBlurPS = NULL;
LPDIRECT3DPIXELSHADER9          CFogOverlayQuad::m_pCompositeFilteredFogPS = NULL;


//--------------------------------------------------------------------------------------
// Name: COverlayQuad()
// Desc: 
//--------------------------------------------------------------------------------------
COverlayQuad::COverlayQuad()
{
}


//--------------------------------------------------------------------------------------
// Name: Initialize()
// Desc: 
//--------------------------------------------------------------------------------------
HRESULT COverlayQuad::Initialize()
{
    HRESULT hr;

    // Create vertex buffer
    if( NULL == m_pOverlayVB )
    {
        if( FAILED( hr = ATG::g_pd3dDevice->CreateVertexBuffer( 4 * sizeof( OVERLAY_VERTEX ),
                                                                D3DUSAGE_WRITEONLY, 0L, D3DPOOL_DEFAULT,
                                                                &m_pOverlayVB, NULL ) ) )
            return hr;

        OVERLAY_VERTEX* pVertices;
        m_pOverlayVB->Lock( 0, 0, ( VOID** )&pVertices, 0 );
        pVertices[0].vPos = XMFLOAT4( -1.0f, -1.0f, 0.0f, 1.0f );
        pVertices[1].vPos = XMFLOAT4( -1.0f, 1.0f, 0.0f, 1.0f );
        pVertices[2].vPos = XMFLOAT4( 1.0f, 1.0f, 0.0f, 1.0f );
        pVertices[3].vPos = XMFLOAT4( 1.0f, -1.0f, 0.0f, 1.0f );
        pVertices[0].vTex = XMFLOAT2( 0.0f, 1.0f );
        pVertices[1].vTex = XMFLOAT2( 0.0f, 0.0f );
        pVertices[2].vTex = XMFLOAT2( 1.0f, 0.0f );
        pVertices[3].vTex = XMFLOAT2( 1.0f, 1.0f );
        m_pOverlayVB->Unlock();
    }

    // Create vertex declaration
    if( NULL == m_pOverlayVertexDecl )
    {
        static const D3DVERTEXELEMENT9 decl[] =
        {
            { 0,  0, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
            { 0, 16, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
            D3DDECL_END()
        };
        ATG::g_pd3dDevice->CreateVertexDeclaration( decl, &m_pOverlayVertexDecl );
    }

    // Create shaders
    if( NULL == m_pOverlayVS )
    {
        if( FAILED( ATG::LoadVertexShader( "game:\\Media\\Shaders\\OverlayMainVS.xvu", &m_pOverlayVS ) ) ) return
                E_FAIL;
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: CShadowOverlayQuad()
// Desc: 
//--------------------------------------------------------------------------------------
CShadowOverlayQuad::CShadowOverlayQuad() : COverlayQuad()
{
}


//--------------------------------------------------------------------------------------
// Name: Initialize()
// Desc: 
//--------------------------------------------------------------------------------------
HRESULT CShadowOverlayQuad::Initialize()
{
    HRESULT hr;

    if( FAILED( hr = COverlayQuad::Initialize() ) )
        return hr;

    // Create shaders
    if( FAILED( ATG::LoadPixelShader( "game:\\Media\\Shaders\\ShadowBlurPS.xpu", &m_pShadowBlurPS ) ) ) return E_FAIL;

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Draw()
// Desc: 
//--------------------------------------------------------------------------------------
HRESULT CShadowOverlayQuad::Draw( LPDIRECT3DTEXTURE9 pShadowMap )
{
    ATG::g_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_NONE );
    ATG::g_pd3dDevice->SetRenderState( D3DRS_ZFUNC, D3DCMP_ALWAYS );
    ATG::g_pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE, FALSE );
    ATG::g_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
    ATG::g_pd3dDevice->SetRenderState( D3DRS_ALPHATESTENABLE, FALSE );

    const DWORD tShadowMap = 0;
    ATG::g_pd3dDevice->SetTexture( tShadowMap, pShadowMap );
    ATG::g_pd3dDevice->SetSamplerState( tShadowMap, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
    ATG::g_pd3dDevice->SetSamplerState( tShadowMap, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
    ATG::g_pd3dDevice->SetSamplerState( tShadowMap, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR );
    ATG::g_pd3dDevice->SetSamplerState( tShadowMap, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP );
    ATG::g_pd3dDevice->SetSamplerState( tShadowMap, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP );
    ATG::g_pd3dDevice->SetSamplerState( tShadowMap, D3DSAMP_ADDRESSW, D3DTADDRESS_CLAMP );

    ATG::g_pd3dDevice->SetPixelShader( m_pShadowBlurPS );
    ATG::g_pd3dDevice->SetPixelShaderConstantF( 0, ( FLOAT* )g_vFilterTaps, NUM_BLUR_TAPS );

    // Draw the overlay
    ATG::g_pd3dDevice->SetVertexDeclaration( m_pOverlayVertexDecl );
    ATG::g_pd3dDevice->SetVertexShader( m_pOverlayVS );
    ATG::g_pd3dDevice->SetStreamSource( 0, m_pOverlayVB, 0, sizeof( OVERLAY_VERTEX ) );
    ATG::g_pd3dDevice->DrawPrimitive( D3DPT_QUADLIST, 0, 1 );

    // Restore state
    ATG::g_pd3dDevice->SetVertexShader( NULL );
    ATG::g_pd3dDevice->SetPixelShader( NULL );
    ATG::g_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_CCW );
    ATG::g_pd3dDevice->SetRenderState( D3DRS_ZFUNC, D3DCMP_LESSEQUAL );
    ATG::g_pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE, TRUE );
    ATG::g_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
    ATG::g_pd3dDevice->SetRenderState( D3DRS_ALPHATESTENABLE, FALSE );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: CFogOverlayQuad()
// Desc: 
//--------------------------------------------------------------------------------------
CFogOverlayQuad::CFogOverlayQuad()
{
}


//--------------------------------------------------------------------------------------
// Name: Initialize()
// Desc: 
//--------------------------------------------------------------------------------------
HRESULT CFogOverlayQuad::Initialize()
{
    HRESULT hr;

    if( FAILED( hr = COverlayQuad::Initialize() ) )
        return hr;

    // Create shaders
    if( FAILED( ATG::LoadPixelShader( "game:\\Media\\Shaders\\CompositeFilteredFogPS.xpu",
                                      &m_pCompositeFilteredFogPS ) ) ) return E_FAIL;

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Draw()
// Desc: 
//--------------------------------------------------------------------------------------
HRESULT CFogOverlayQuad::Draw( LPDIRECT3DTEXTURE9 pFogTexture )
{
    ATG::g_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_NONE );
    ATG::g_pd3dDevice->SetRenderState( D3DRS_ZFUNC, D3DCMP_ALWAYS );
    ATG::g_pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE, FALSE );
    ATG::g_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
    ATG::g_pd3dDevice->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_ONE );
    ATG::g_pd3dDevice->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_ONE );
    ATG::g_pd3dDevice->SetRenderState( D3DRS_ALPHATESTENABLE, TRUE );
    ATG::g_pd3dDevice->SetRenderState( D3DRS_ALPHAREF, 0x00 );
    ATG::g_pd3dDevice->SetRenderState( D3DRS_ALPHAFUNC, D3DCMP_GREATER );

    const DWORD tFogTexture = 0;
    ATG::g_pd3dDevice->SetTexture( tFogTexture, pFogTexture );
    ATG::g_pd3dDevice->SetSamplerState( tFogTexture, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
    ATG::g_pd3dDevice->SetSamplerState( tFogTexture, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
    ATG::g_pd3dDevice->SetSamplerState( tFogTexture, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR );
    ATG::g_pd3dDevice->SetSamplerState( tFogTexture, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP );
    ATG::g_pd3dDevice->SetSamplerState( tFogTexture, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP );
    ATG::g_pd3dDevice->SetSamplerState( tFogTexture, D3DSAMP_ADDRESSW, D3DTADDRESS_CLAMP );

    ATG::g_pd3dDevice->SetPixelShader( m_pCompositeFilteredFogPS );
    ATG::g_pd3dDevice->SetPixelShaderConstantF( 0, ( FLOAT* )g_vFilterTaps, NUM_BLUR_TAPS );

    // Draw the overlay
    ATG::g_pd3dDevice->SetVertexDeclaration( m_pOverlayVertexDecl );
    ATG::g_pd3dDevice->SetVertexShader( m_pOverlayVS );
    ATG::g_pd3dDevice->SetStreamSource( 0, m_pOverlayVB, 0, sizeof( OVERLAY_VERTEX ) );
    ATG::g_pd3dDevice->DrawPrimitive( D3DPT_QUADLIST, 0, 1 );

    // Restore state
    ATG::g_pd3dDevice->SetVertexShader( NULL );
    ATG::g_pd3dDevice->SetPixelShader( NULL );
    ATG::g_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_CCW );
    ATG::g_pd3dDevice->SetRenderState( D3DRS_ZFUNC, D3DCMP_LESSEQUAL );
    ATG::g_pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE, TRUE );
    ATG::g_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
    ATG::g_pd3dDevice->SetRenderState( D3DRS_ALPHATESTENABLE, FALSE );

    return S_OK;
}

