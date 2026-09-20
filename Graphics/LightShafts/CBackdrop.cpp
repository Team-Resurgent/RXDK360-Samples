//--------------------------------------------------------------------------------------
// CBackdrop.cpp
//
// Simple backdrop
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
#include "CBackdrop.h"
#include <AtgApp.h>

#define FLOOR_TINT               0xff7dd5Fa
#define BACK_WALL_TINT           0xfff39b9b
#define LEFT_WALL_TINT           0xff95fcb0

#define ROOM_SIZE                    400.0f
#define ROOM_TEXTURE_REPEAT_RATE       2.0f


//--------------------------------------------------------------------------------------
// Types
//--------------------------------------------------------------------------------------

struct BACKDROP_VERTEX
{
    XMFLOAT3 m_vPos;
    XMFLOAT3 m_vNorm;
    DWORD m_dwColor;
    XMFLOAT2 m_vTex;
};


static LPDIRECT3DVERTEXDECLARATION9 g_pBackdropVertexDecl = NULL;
static LPDIRECT3DVERTEXSHADER9      g_pBackdropMainVS = NULL;
static LPDIRECT3DVERTEXSHADER9      g_pBackdropDepthVS = NULL;
static LPDIRECT3DVERTEXSHADER9      g_pBackdropAmbientOnlyVS = NULL;
static LPDIRECT3DPIXELSHADER9       g_pBackdropNoiseShadowPS = NULL;
static LPDIRECT3DPIXELSHADER9       g_pBackdropAmbientOnlyPS = NULL;


//----------------------------------------------------------------------------
// Name: CBackdrop()
// Desc: Constructor
//----------------------------------------------------------------------------
CBackdrop::CBackdrop()
{
    m_pVB = NULL;
    m_pTexture = NULL;
}


//----------------------------------------------------------------------------
// Name: Initialize()
// Desc: 
//----------------------------------------------------------------------------
HRESULT CBackdrop::Initialize( LPDIRECT3DTEXTURE9 pBackdropTexture )
{
    HRESULT hr;

    // Create vertices
    m_dwNumVertices = 4 * 3;
    if( FAILED( hr = ATG::g_pd3dDevice->CreateVertexBuffer( m_dwNumVertices * sizeof( BACKDROP_VERTEX ),
                                                            D3DUSAGE_WRITEONLY, 0L, D3DPOOL_DEFAULT,
                                                            &m_pVB, NULL ) ) )
        return hr;

    BACKDROP_VERTEX* pVertices;
    m_pVB->Lock( 0, 0, ( VOID** )&pVertices, 0 );

    // Floor
    pVertices[0].m_vPos = XMFLOAT3( -ROOM_SIZE / 2, 0.0f, -ROOM_SIZE / 2 );
    pVertices[0].m_vNorm = XMFLOAT3( 0.0f, 1.0f, 0.0f );
    pVertices[0].m_vTex = XMFLOAT2( -ROOM_TEXTURE_REPEAT_RATE, -ROOM_TEXTURE_REPEAT_RATE );
    pVertices[0].m_dwColor = FLOOR_TINT;

    pVertices[1].m_vPos = XMFLOAT3( -ROOM_SIZE / 2, 0.0f, ROOM_SIZE / 2 );
    pVertices[1].m_vNorm = XMFLOAT3( 0.0f, 1.0f, 0.0f );
    pVertices[1].m_vTex = XMFLOAT2( -ROOM_TEXTURE_REPEAT_RATE, ROOM_TEXTURE_REPEAT_RATE );
    pVertices[1].m_dwColor = FLOOR_TINT;

    pVertices[2].m_vPos = XMFLOAT3( ROOM_SIZE / 2, 0.0f, ROOM_SIZE / 2 );
    pVertices[2].m_vNorm = XMFLOAT3( 0.0f, 1.0f, 0.0f );
    pVertices[2].m_vTex = XMFLOAT2( ROOM_TEXTURE_REPEAT_RATE, ROOM_TEXTURE_REPEAT_RATE );
    pVertices[2].m_dwColor = FLOOR_TINT;

    pVertices[3].m_vPos = XMFLOAT3( ROOM_SIZE / 2, 0.0f, -ROOM_SIZE / 2 );
    pVertices[3].m_vNorm = XMFLOAT3( 0.0f, 1.0f, 0.0f );
    pVertices[3].m_vTex = XMFLOAT2( ROOM_TEXTURE_REPEAT_RATE, -ROOM_TEXTURE_REPEAT_RATE );
    pVertices[3].m_dwColor = FLOOR_TINT;

    // +z wall (back wall)
    pVertices[4].m_vPos = XMFLOAT3( -ROOM_SIZE / 2, 0.0f, ROOM_SIZE / 2 );
    pVertices[4].m_vNorm = XMFLOAT3( 0.0f, 0.0f, -1.0f );
    pVertices[4].m_vTex = XMFLOAT2( -ROOM_TEXTURE_REPEAT_RATE, -ROOM_TEXTURE_REPEAT_RATE );
    pVertices[4].m_dwColor = BACK_WALL_TINT;

    pVertices[5].m_vPos = XMFLOAT3( -ROOM_SIZE / 2, ROOM_SIZE / 2, ROOM_SIZE / 2 );
    pVertices[5].m_vNorm = XMFLOAT3( 0.0f, 0.0f, -1.0f );
    pVertices[5].m_vTex = XMFLOAT2( -ROOM_TEXTURE_REPEAT_RATE, ROOM_TEXTURE_REPEAT_RATE );
    pVertices[5].m_dwColor = BACK_WALL_TINT;

    pVertices[6].m_vPos = XMFLOAT3( ROOM_SIZE / 2, ROOM_SIZE / 2, ROOM_SIZE / 2 );
    pVertices[6].m_vNorm = XMFLOAT3( 0.0f, 0.0f, -1.0f );
    pVertices[6].m_vTex = XMFLOAT2( ROOM_TEXTURE_REPEAT_RATE, ROOM_TEXTURE_REPEAT_RATE );
    pVertices[6].m_dwColor = BACK_WALL_TINT;

    pVertices[7].m_vPos = XMFLOAT3( ROOM_SIZE / 2, 0.0f, ROOM_SIZE / 2 );
    pVertices[7].m_vNorm = XMFLOAT3( 0.0f, 0.0f, -1.0f );
    pVertices[7].m_vTex = XMFLOAT2( ROOM_TEXTURE_REPEAT_RATE, -ROOM_TEXTURE_REPEAT_RATE );
    pVertices[7].m_dwColor = BACK_WALL_TINT;

    // -x wall (left wall)
    pVertices[8].m_vPos = XMFLOAT3( -ROOM_SIZE / 2, 0.0f, -ROOM_SIZE / 2 );
    pVertices[8].m_vNorm = XMFLOAT3( 1.0f, 0.0f, 0.0f );
    pVertices[8].m_vTex = XMFLOAT2( -ROOM_TEXTURE_REPEAT_RATE, -ROOM_TEXTURE_REPEAT_RATE );
    pVertices[8].m_dwColor = LEFT_WALL_TINT;

    pVertices[9].m_vPos = XMFLOAT3( -ROOM_SIZE / 2, ROOM_SIZE / 2, -ROOM_SIZE / 2 );
    pVertices[9].m_vNorm = XMFLOAT3( 1.0f, 0.0f, 0.0f );
    pVertices[9].m_vTex = XMFLOAT2( -ROOM_TEXTURE_REPEAT_RATE, ROOM_TEXTURE_REPEAT_RATE );
    pVertices[9].m_dwColor = LEFT_WALL_TINT;

    pVertices[10].m_vPos = XMFLOAT3( -ROOM_SIZE / 2, ROOM_SIZE / 2, ROOM_SIZE / 2 );
    pVertices[10].m_vNorm = XMFLOAT3( 1.0f, 0.0f, 0.0f );
    pVertices[10].m_vTex = XMFLOAT2( ROOM_TEXTURE_REPEAT_RATE, ROOM_TEXTURE_REPEAT_RATE );
    pVertices[10].m_dwColor = LEFT_WALL_TINT;

    pVertices[11].m_vPos = XMFLOAT3( -ROOM_SIZE / 2, 0.0f, ROOM_SIZE / 2 );
    pVertices[11].m_vNorm = XMFLOAT3( 1.0f, 0.0f, 0.0f );
    pVertices[11].m_vTex = XMFLOAT2( ROOM_TEXTURE_REPEAT_RATE, -ROOM_TEXTURE_REPEAT_RATE );
    pVertices[11].m_dwColor = LEFT_WALL_TINT;

    m_pVB->Unlock();

    // Assign the texture
    m_pTexture = pBackdropTexture;

    // Create vertex declaration
    static const D3DVERTEXELEMENT9 decl[] =
    {
        { 0,  0, D3DDECLTYPE_FLOAT3,   D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_POSITION, 0 },
        { 0, 12, D3DDECLTYPE_FLOAT3,   D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_NORMAL,   0 },
        { 0, 24, D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_COLOR,    0 },
        { 0, 28, D3DDECLTYPE_FLOAT2,   D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_TEXCOORD, 0 },
        D3DDECL_END()
    };
    ATG::g_pd3dDevice->CreateVertexDeclaration( decl, &g_pBackdropVertexDecl );

    // Create the shaders
    if( FAILED( ATG::LoadVertexShader( "game:\\Media\\Shaders\\BackdropMainVS.xvu",
                                       &g_pBackdropMainVS ) ) )        return E_FAIL;
    if( FAILED( ATG::LoadVertexShader( "game:\\Media\\Shaders\\BackdropDepthVS.xvu",
                                       &g_pBackdropDepthVS ) ) )       return E_FAIL;
    if( FAILED( ATG::LoadVertexShader( "game:\\Media\\Shaders\\BackdropAmbientOnlyVS.xvu",
                                       &g_pBackdropAmbientOnlyVS ) ) ) return E_FAIL;
    if( FAILED( ATG::LoadPixelShader( "game:\\Media\\Shaders\\BackdropNoiseShadowPS.xpu",
                                      &g_pBackdropNoiseShadowPS ) ) ) return E_FAIL;
    if( FAILED( ATG::LoadPixelShader( "game:\\Media\\Shaders\\BackdropAmbientOnlyPS.xpu",
                                      &g_pBackdropAmbientOnlyPS ) ) ) return E_FAIL;

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: SetFarPlane()
// Desc: 
//--------------------------------------------------------------------------------------
VOID CBackdrop::SetFarPlane( FLOAT fFarPlane )
{
    m_fFarPlane = fFarPlane;
}


//----------------------------------------------------------------------------
// Name: SetLightProjBiasMatrix()
// Desc: 
//----------------------------------------------------------------------------
VOID CBackdrop::SetLightProjBiasMatrix( XMMATRIX matLightProjBias )
{
    m_matLightProjBias = matLightProjBias;
}


//----------------------------------------------------------------------------
// Name: SetLightProjScrollMatrices()
// Desc: 
//----------------------------------------------------------------------------
VOID CBackdrop::SetLightProjScrollMatrices( XMMATRIX matLightProjScroll1,
                                            XMMATRIX matLightProjScroll2 )
{
    m_matLightProjScroll1 = matLightProjScroll1;
    m_matLightProjScroll2 = matLightProjScroll2;
}


//----------------------------------------------------------------------------
// Name: SetWorldViewProjMatrix()
// Desc: 
//----------------------------------------------------------------------------
VOID CBackdrop::SetWorldViewProjMatrix( XMMATRIX matWorldViewProj )
{
    m_matWorldViewProj = matWorldViewProj;
}


//----------------------------------------------------------------------------
// Name: SetWorldLightMatrix()
// Desc: 
//----------------------------------------------------------------------------
VOID CBackdrop::SetWorldLightMatrix( XMMATRIX matWorldLight )
{
    m_matWorldLight = matWorldLight;
}


//----------------------------------------------------------------------------
// Name: SetTextureHandles()
// Desc: 
//----------------------------------------------------------------------------
VOID CBackdrop::SetTextureHandles( LPDIRECT3DTEXTURE9 pCookie,
                                   LPDIRECT3DTEXTURE9 pScrollingNoise,
                                   LPDIRECT3DTEXTURE9 pShadowMap )
{
    m_pCookie = pCookie;
    m_pScrollingNoise = pScrollingNoise;
    m_pShadowMap = pShadowMap;
}


//----------------------------------------------------------------------------
// Name: DrawDepthOnly()
// Desc: 
//----------------------------------------------------------------------------
HRESULT CBackdrop::DrawDepthOnly()
{
    XMMATRIX m = XMMatrixTranspose( m_matWorldViewProj );
    ATG::g_pd3dDevice->SetVertexShaderConstantF( 0, ( FLOAT* )&m, 4 );

    ATG::g_pd3dDevice->SetVertexShaderConstantF( 32, ( FLOAT* )&m_fFarPlane, 1 );

    ATG::g_pd3dDevice->SetVertexShader( g_pBackdropDepthVS );
    ATG::g_pd3dDevice->SetPixelShader( NULL );
    ATG::g_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_NONE );

    ATG::g_pd3dDevice->SetVertexDeclaration( g_pBackdropVertexDecl );
    ATG::g_pd3dDevice->SetStreamSource( 0, m_pVB, 0, sizeof( BACKDROP_VERTEX ) );
    ATG::g_pd3dDevice->DrawPrimitive( D3DPT_QUADLIST, 0, 3 );

    // Restore state
    ATG::g_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_CCW );

    return S_OK;
}


//----------------------------------------------------------------------------
// Name: Draw()
// Desc: 
//----------------------------------------------------------------------------
HRESULT CBackdrop::Draw( BOOL bScrollingNoise, BOOL bShadowMapping )
{
    XMMATRIX m;

    // Matrices for getting primitives lit and on screen
    m = XMMatrixTranspose( m_matWorldViewProj );    ATG::g_pd3dDevice->SetVertexShaderConstantF( 0, ( FLOAT* )&m, 4 );

    // Matrices for working with light space 
    m = XMMatrixTranspose( m_matWorldLight );       ATG::g_pd3dDevice->SetVertexShaderConstantF( 8, ( FLOAT* )&m, 4 );
    m = XMMatrixTranspose( m_matLightProjBias );    ATG::g_pd3dDevice->SetVertexShaderConstantF( 20, ( FLOAT* )&m, 4 );
    m = XMMatrixTranspose( m_matLightProjScroll1 ); ATG::g_pd3dDevice->SetVertexShaderConstantF( 24, ( FLOAT* )&m, 4 );
    m = XMMatrixTranspose( m_matLightProjScroll2 ); ATG::g_pd3dDevice->SetVertexShaderConstantF( 28, ( FLOAT* )&m, 4 );

    ATG::g_pd3dDevice->SetVertexShaderConstantF( 32, ( FLOAT* )&m_fFarPlane, 1 );

    ATG::g_pd3dDevice->SetVertexDeclaration( g_pBackdropVertexDecl );
    ATG::g_pd3dDevice->SetStreamSource( 0, m_pVB, 0, sizeof( BACKDROP_VERTEX ) );

    const DWORD tBaseTexture = 0;
    const DWORD tCookie = 1;
    const DWORD tScrollingNoise = 2;
    const DWORD tShadowMap = 3;

    ATG::g_pd3dDevice->SetTexture( tBaseTexture, m_pTexture );
    ATG::g_pd3dDevice->SetSamplerState( tBaseTexture, D3DSAMP_MAXANISOTROPY, 8 );
    ATG::g_pd3dDevice->SetSamplerState( tBaseTexture, D3DSAMP_MINFILTER, D3DTEXF_ANISOTROPIC );
    ATG::g_pd3dDevice->SetSamplerState( tBaseTexture, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
    ATG::g_pd3dDevice->SetSamplerState( tBaseTexture, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR );
    ATG::g_pd3dDevice->SetSamplerState( tBaseTexture, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP );
    ATG::g_pd3dDevice->SetSamplerState( tBaseTexture, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP );

    ATG::g_pd3dDevice->SetTexture( tCookie, m_pCookie );
    ATG::g_pd3dDevice->SetSamplerState( tCookie, D3DSAMP_MAXANISOTROPY, 8 );
    ATG::g_pd3dDevice->SetSamplerState( tCookie, D3DSAMP_MINFILTER, D3DTEXF_ANISOTROPIC );
    ATG::g_pd3dDevice->SetSamplerState( tCookie, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
    ATG::g_pd3dDevice->SetSamplerState( tCookie, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR );
    ATG::g_pd3dDevice->SetSamplerState( tCookie, D3DSAMP_ADDRESSU, D3DTADDRESS_BORDER );
    ATG::g_pd3dDevice->SetSamplerState( tCookie, D3DSAMP_ADDRESSV, D3DTADDRESS_BORDER );

    ATG::g_pd3dDevice->SetTexture( tScrollingNoise, m_pScrollingNoise );
    ATG::g_pd3dDevice->SetSamplerState( tScrollingNoise, D3DSAMP_MAXANISOTROPY, 8 );
    ATG::g_pd3dDevice->SetSamplerState( tScrollingNoise, D3DSAMP_MINFILTER, D3DTEXF_ANISOTROPIC );
    ATG::g_pd3dDevice->SetSamplerState( tScrollingNoise, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
    ATG::g_pd3dDevice->SetSamplerState( tScrollingNoise, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR );
    ATG::g_pd3dDevice->SetSamplerState( tScrollingNoise, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP );
    ATG::g_pd3dDevice->SetSamplerState( tScrollingNoise, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP );

    ATG::g_pd3dDevice->SetTexture( tShadowMap, m_pShadowMap );
    ATG::g_pd3dDevice->SetSamplerState( tShadowMap, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
    ATG::g_pd3dDevice->SetSamplerState( tShadowMap, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
    ATG::g_pd3dDevice->SetSamplerState( tShadowMap, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR );
    ATG::g_pd3dDevice->SetSamplerState( tShadowMap, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP );
    ATG::g_pd3dDevice->SetSamplerState( tShadowMap, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP );

    ATG::g_pd3dDevice->SetPixelShaderConstantB( 2, &bScrollingNoise, 1 );
    ATG::g_pd3dDevice->SetPixelShaderConstantB( 3, &bShadowMapping, 1 );
    ATG::g_pd3dDevice->SetPixelShader( g_pBackdropNoiseShadowPS );

    // Draw front sides with the (noisy, shadowed) spotlight
    ATG::g_pd3dDevice->SetVertexShader( g_pBackdropMainVS );
    ATG::g_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_CCW );
    ATG::g_pd3dDevice->DrawPrimitive( D3DPT_QUADLIST, 0, 3 );

    // Draw back sides using ambient only
    ATG::g_pd3dDevice->SetVertexShader( g_pBackdropAmbientOnlyVS );
    ATG::g_pd3dDevice->SetPixelShader( g_pBackdropAmbientOnlyPS );
    ATG::g_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_CW );
    ATG::g_pd3dDevice->DrawPrimitive( D3DPT_QUADLIST, 0, 3 );

    // Restore state
    ATG::g_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_CCW );

    return S_OK;
}
