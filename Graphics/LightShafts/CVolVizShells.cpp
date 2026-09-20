//--------------------------------------------------------------------------------------
// CVolVizShells.cpp
//
// Shells for visualizing Light Shafts 
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
#include "CVolVizShells.h"
#include <AtgApp.h>

// Global scene transforms
extern XMMATRIX                     g_matProj;
extern XMMATRIX                     g_matWorldView;
extern XMMATRIX                     g_matWorldViewProj;


//--------------------------------------------------------------------------------------
// Types
//--------------------------------------------------------------------------------------
struct SHELL_VERTEX
{
    XMFLOAT3 vPos;
};

static LPDIRECT3DVERTEXDECLARATION9 g_pShellVertexDecl = NULL;
static LPDIRECT3DVERTEXSHADER9      g_pVolVizShellsMainVS = NULL;
static LPDIRECT3DPIXELSHADER9       g_pVolVizShellsNoiseShadowPS = NULL;




//--------------------------------------------------------------------------------------
// Name: CVolVizShells()
// Desc: 
//--------------------------------------------------------------------------------------
CVolVizShells::CVolVizShells()
{
    m_dwNumShells = 100;
    m_pVB = NULL;
    m_pCookie = NULL;
    m_pScrollingNoise = NULL;
}


//--------------------------------------------------------------------------------------
// Name: Initialize()
// Desc: 
//--------------------------------------------------------------------------------------
HRESULT CVolVizShells::Initialize()
{
    HRESULT hr;

    // Create vertices
    if( FAILED( hr = ATG::g_pd3dDevice->CreateVertexBuffer( 4 * m_dwNumShells * sizeof( SHELL_VERTEX ),
                                                            D3DUSAGE_WRITEONLY, 0L, D3DPOOL_DEFAULT,
                                                            &m_pVB, NULL ) ) )
        return hr;

    SHELL_VERTEX* pVertices;
    m_pVB->Lock( 0, 0, ( VOID** )&pVertices, 0 );

    for( DWORD shell = 0; shell < m_dwNumShells; shell++ )
    {
        FLOAT fZ = 1.0f - ( ( ( FLOAT )shell ) / ( ( FLOAT )( m_dwNumShells - 1 ) ) );

        pVertices[0].vPos = XMFLOAT3( 0.0f, 1.0f, fZ );
        pVertices[1].vPos = XMFLOAT3( 0.0f, 0.0f, fZ );
        pVertices[2].vPos = XMFLOAT3( 1.0f, 0.0f, fZ );
        pVertices[3].vPos = XMFLOAT3( 1.0f, 1.0f, fZ );
        pVertices += 4;
    }

    m_pVB->Unlock();

    // Create vertex declaration
    {
        static const D3DVERTEXELEMENT9 decl[] =
        {
            { 0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
            D3DDECL_END()
        };

        hr = ATG::g_pd3dDevice->CreateVertexDeclaration( decl, &g_pShellVertexDecl );
        if( FAILED( hr ) )
            return hr;
    }

    // Create the shaders
    if( FAILED( ATG::LoadVertexShader( "game:\\Media\\Shaders\\VolVizShellsMainVS.xvu",
                                       &g_pVolVizShellsMainVS ) ) ) return E_FAIL;
    if( FAILED( ATG::LoadPixelShader( "game:\\Media\\Shaders\\VolVizShellsNoiseShadowPS.xpu",
                                      &g_pVolVizShellsNoiseShadowPS ) ) ) return E_FAIL;

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: SetVolVizBounds()
// Desc: 
//--------------------------------------------------------------------------------------
VOID CVolVizShells::SetVolVizBounds( XMVECTOR vMinBounds, XMVECTOR vMaxBounds,
                                     FLOAT fSamplingDelta )
{
    m_vMinBounds = vMinBounds;
    m_vMaxBounds = vMaxBounds;
    m_fSamplingDelta = fSamplingDelta;
}


//--------------------------------------------------------------------------------------
// Name: SetFarPlane()
// Desc: 
//--------------------------------------------------------------------------------------
VOID CVolVizShells::SetFarPlane( FLOAT fFarPlane )
{
    m_fFarPlane = fFarPlane;
}


//--------------------------------------------------------------------------------------
// Name: SetWorldLightMatrix()
// Desc: 
//--------------------------------------------------------------------------------------
VOID CVolVizShells::SetWorldLightMatrix( XMMATRIX matWorldLight )
{
    m_matWorldLight = matWorldLight;
}


//--------------------------------------------------------------------------------------
// Name: SetLightProjBiasMatrix()
// Desc: 
//--------------------------------------------------------------------------------------
VOID CVolVizShells::SetLightProjBiasMatrix( XMMATRIX matLightProjBias )
{
    m_matLightProjBias = matLightProjBias;
}


//--------------------------------------------------------------------------------------
// Name: SetLightProjScrollMatrices()
// Desc: 
//--------------------------------------------------------------------------------------
VOID CVolVizShells::SetLightProjScrollMatrices( XMMATRIX matLightProjScroll1,
                                                XMMATRIX matLightProjScroll2 )
{
    m_matLightProjScroll1 = matLightProjScroll1;
    m_matLightProjScroll2 = matLightProjScroll2;
}


//--------------------------------------------------------------------------------------
// Name: SetWorldViewProjMatrix()
// Desc: 
//--------------------------------------------------------------------------------------
VOID CVolVizShells::SetWorldViewProjMatrix( XMMATRIX matWorldViewProj )
{
    m_matWorldViewProj = matWorldViewProj;
}


//--------------------------------------------------------------------------------------
// Name: SetWorldLightProjMatrix()
// Desc: 
//--------------------------------------------------------------------------------------
VOID CVolVizShells::SetWorldLightProjMatrix( XMMATRIX matWorldLightProj )
{
    m_matWorldLightProj = matWorldLightProj;
}


//--------------------------------------------------------------------------------------
// Name: SetTextures()
// Desc: 
//--------------------------------------------------------------------------------------
VOID CVolVizShells::SetTextures( LPDIRECT3DTEXTURE9 pCookie,
                                 LPDIRECT3DTEXTURE9 pScrollingNoise,
                                 LPDIRECT3DTEXTURE9 pShadowMap )
{
    m_pCookie = pCookie;
    m_pScrollingNoise = pScrollingNoise;
    m_pShadowMap = pShadowMap;
}


//--------------------------------------------------------------------------------------
// Name: SetClipPlanes()
// Desc: 
//--------------------------------------------------------------------------------------
VOID CVolVizShells::SetClipPlanes( XMVECTOR* pClipSpaceFrustumPlanes )
{
    // Copy these in for later use during draw calls
    for( DWORD i = 0; i < 6; i++ )
        m_ClipSpaceLightFrustumPlanes[i] = pClipSpaceFrustumPlanes[i];
}


//--------------------------------------------------------------------------------------
// Name: Draw()
// Desc: 
//--------------------------------------------------------------------------------------
HRESULT CVolVizShells::Draw( BOOL bScrollingNoise, BOOL bShadowMapping )
{
    // Compute number of shells to actually be drawn
    FLOAT fZBounds = m_vMaxBounds.z - m_vMinBounds.z;
    DWORD dwNumShellsToDraw = min( m_dwNumShells, ( DWORD )( ( fZBounds / m_fSamplingDelta ) + 0.5f ) );

    // Fraction of shells in VB to actually be drawn
    FLOAT fFractionOfMaxShells = ( ( ( FLOAT )dwNumShellsToDraw ) / ( ( FLOAT )m_dwNumShells ) );

    const DWORD tCookie = 1;
    const DWORD tScrollingNoise = 2;
    const DWORD tShadowMap = 3;

    ATG::g_pd3dDevice->SetTexture( tCookie, m_pCookie );
    ATG::g_pd3dDevice->SetSamplerState( tCookie, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
    //  ATG::g_pd3dDevice->SetSamplerState( tCookie, D3DSAMP_MINFILTER, Anisotropic );
    ATG::g_pd3dDevice->SetSamplerState( tCookie, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
    ATG::g_pd3dDevice->SetSamplerState( tCookie, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR );
    ATG::g_pd3dDevice->SetSamplerState( tCookie, D3DSAMP_MAXANISOTROPY, 16 );
    ATG::g_pd3dDevice->SetSamplerState( tCookie, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP );
    ATG::g_pd3dDevice->SetSamplerState( tCookie, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP );
    ATG::g_pd3dDevice->SetSamplerState( tCookie, D3DSAMP_ADDRESSW, D3DTADDRESS_CLAMP );

    ATG::g_pd3dDevice->SetTexture( tScrollingNoise, m_pScrollingNoise );
    ATG::g_pd3dDevice->SetSamplerState( tScrollingNoise, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
    //  ATG::g_pd3dDevice->SetSamplerState( tScrollingNoise, D3DSAMP_MINFILTER, Anisotropic );
    ATG::g_pd3dDevice->SetSamplerState( tScrollingNoise, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
    ATG::g_pd3dDevice->SetSamplerState( tScrollingNoise, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR );
    ATG::g_pd3dDevice->SetSamplerState( tScrollingNoise, D3DSAMP_MAXANISOTROPY, 16 );
    ATG::g_pd3dDevice->SetSamplerState( tScrollingNoise, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP );
    ATG::g_pd3dDevice->SetSamplerState( tScrollingNoise, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP );
    ATG::g_pd3dDevice->SetSamplerState( tScrollingNoise, D3DSAMP_ADDRESSW, D3DTADDRESS_WRAP );

    ATG::g_pd3dDevice->SetTexture( tShadowMap, m_pShadowMap );
    ATG::g_pd3dDevice->SetSamplerState( tShadowMap, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
    ATG::g_pd3dDevice->SetSamplerState( tShadowMap, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
    ATG::g_pd3dDevice->SetSamplerState( tShadowMap, D3DSAMP_MIPFILTER, D3DTEXF_NONE );
    ATG::g_pd3dDevice->SetSamplerState( tShadowMap, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP );
    ATG::g_pd3dDevice->SetSamplerState( tShadowMap, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP );
    ATG::g_pd3dDevice->SetSamplerState( tShadowMap, D3DSAMP_ADDRESSW, D3DTADDRESS_CLAMP );

    XMMATRIX m;
    XMVECTOR vDeterminant;

    // Matrices for getting stuff on the screen
    m = XMMatrixTranspose( g_matWorldViewProj );    ATG::g_pd3dDevice->SetVertexShaderConstantF( 0, ( FLOAT* )&m, 4 );
    m = XMMatrixTranspose( m_matWorldLightProj );   ATG::g_pd3dDevice->SetVertexShaderConstantF( 4, ( FLOAT* )&m, 4 );
    m = XMMatrixTranspose( m_matWorldLight );       ATG::g_pd3dDevice->SetVertexShaderConstantF( 8, ( FLOAT* )&m, 4 );
    m = XMMatrixTranspose( XMMatrixInverse( &vDeterminant, g_matWorldView ) );
    ATG::g_pd3dDevice->SetVertexShaderConstantF( 12, ( FLOAT* )&m, 4 );

    // Matrices for working with light space
    m = XMMatrixTranspose( m_matLightProjBias );    ATG::g_pd3dDevice->SetVertexShaderConstantF( 20, ( FLOAT* )&m, 4 );
    m = XMMatrixTranspose( m_matLightProjScroll1 ); ATG::g_pd3dDevice->SetVertexShaderConstantF( 24, ( FLOAT* )&m, 4 );
    m = XMMatrixTranspose( m_matLightProjScroll2 ); ATG::g_pd3dDevice->SetVertexShaderConstantF( 28, ( FLOAT* )&m, 4 );

    ATG::g_pd3dDevice->SetVertexShaderConstantF( 32, ( FLOAT* )&m_fFarPlane, 1 );
    ATG::g_pd3dDevice->SetVertexShaderConstantF( 33, ( FLOAT* )&m_vMinBounds, 1 );
    ATG::g_pd3dDevice->SetVertexShaderConstantF( 34, ( FLOAT* )&m_vMaxBounds, 1 );

    ATG::g_pd3dDevice->SetVertexShaderConstantF( 40, &fFractionOfMaxShells, 1 );
    ATG::g_pd3dDevice->SetPixelShaderConstantF( 0, &fFractionOfMaxShells, 1 );

    ATG::g_pd3dDevice->SetPixelShaderConstantB( 2, &bScrollingNoise, 1 );
    ATG::g_pd3dDevice->SetPixelShaderConstantB( 3, &bShadowMapping, 1 );

    ATG::g_pd3dDevice->SetVertexShader( g_pVolVizShellsMainVS );
    ATG::g_pd3dDevice->SetPixelShader( g_pVolVizShellsNoiseShadowPS );
    ATG::g_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_NONE );
    ATG::g_pd3dDevice->SetRenderState( D3DRS_FILLMODE, D3DFILL_SOLID );
    ATG::g_pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE, FALSE );
    ATG::g_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
    ATG::g_pd3dDevice->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_ONE );
    ATG::g_pd3dDevice->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_ONE );
    ATG::g_pd3dDevice->SetRenderState( D3DRS_ALPHAREF, 0x00000000 );
    ATG::g_pd3dDevice->SetRenderState( D3DRS_ALPHAFUNC, D3DCMP_GREATER );

    // Set clipping planes
    ATG::g_pd3dDevice->SetRenderState( D3DRS_CLIPPLANEENABLE, D3DCLIPPLANE0 | D3DCLIPPLANE1 | D3DCLIPPLANE2 |
                                       D3DCLIPPLANE3 | D3DCLIPPLANE4 | D3DCLIPPLANE5 );
    for( DWORD i = 0; i < 6; i++ )
        ATG::g_pd3dDevice->SetClipPlane( i, ( FLOAT* )&m_ClipSpaceLightFrustumPlanes[i] );

    ATG::g_pd3dDevice->SetVertexDeclaration( g_pShellVertexDecl );
    ATG::g_pd3dDevice->SetStreamSource( 0, m_pVB, 0, sizeof( SHELL_VERTEX ) );
    ATG::g_pd3dDevice->DrawPrimitive( D3DPT_QUADLIST, 0, dwNumShellsToDraw );

    // Restore state
    ATG::g_pd3dDevice->SetTexture( tCookie, NULL );
    ATG::g_pd3dDevice->SetTexture( tScrollingNoise, NULL );
    ATG::g_pd3dDevice->SetTexture( tShadowMap, NULL );
    ATG::g_pd3dDevice->SetVertexShader( NULL );
    ATG::g_pd3dDevice->SetPixelShader( NULL );
    ATG::g_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_CCW );
    ATG::g_pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE, TRUE );
    ATG::g_pd3dDevice->SetRenderState( D3DRS_CLIPPLANEENABLE, 0 );
    ATG::g_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );

    return S_OK;
}

