//--------------------------------------------------------------------------------------
// NormalMap.cpp
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
// Copyright (C) Tomohide Kano. All rights reserved.
//--------------------------------------------------------------------------------------
#include <xtl.h>
#include <xboxmath.h>
#include <AtgUtil.h>
#include "FurMesh.h"
#include "NormalMap.h"
#include <AtgApp.h>

const DWORD VSCONST_FORCE = 8;
const DWORD VSCONST_OMEGA = 9;
const DWORD VSCONST_DAMPING = 10;


//--------------------------------------------------------------------------------------
// Name: NormalMap()
// Desc: 
//--------------------------------------------------------------------------------------
NormalMap::NormalMap()
{
    m_pUpdateRT = NULL;

    m_dwWidth = 0;
    m_dwHeight = 0;
    m_pOffsetTexture = NULL;

    m_pUpdateOffsetVS = NULL;
    m_pUpdateOffsetPS = NULL;
    m_pUpdateOffsetVertexDecl = NULL;

    m_vPrevVel = XMVectorSet( 0, 0, 0, 0 );
    m_vAccel = XMVectorSet( 0, 0, 0, 0 );
    m_vPrevOmega = XMVectorSet( 0, 0, 0, 0 );
    m_vOmegaAccel = XMVectorSet( 0, 0, 0, 0 );
}


//--------------------------------------------------------------------------------------
// Name: ~NormalMap()
// Desc: 
//--------------------------------------------------------------------------------------
NormalMap::~NormalMap()
{
    m_pUpdateRT->Release();
    m_pOffsetTexture->Release();
    m_pUpdateOffsetVS->Release();
    m_pUpdateOffsetPS->Release();
    m_pUpdateOffsetVertexDecl->Release();
}


//--------------------------------------------------------------------------------------
// Name: Init()
// Desc: 
//--------------------------------------------------------------------------------------
HRESULT NormalMap::Init( DWORD dwWidth, DWORD dwHeight )
{
    m_dwWidth = dwWidth;
    m_dwHeight = dwHeight;
    if( FAILED( InitTextures() ) )
        return E_FAIL;
    if( FAILED( InitShaders() ) )
        return E_FAIL;
    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: InitTextures()
// Desc: 
//--------------------------------------------------------------------------------------
HRESULT NormalMap::InitTextures()
{
    // Create the render target used to update the textures
    if( FAILED( ATG::g_pd3dDevice->CreateRenderTarget( m_dwWidth, m_dwHeight, D3DFMT_A8R8G8B8,
                                                       D3DMULTISAMPLE_NONE, 0L, FALSE,
                                                       &m_pUpdateRT, NULL ) ) )
        return E_FAIL;

    // Create the offset map
    {
        ATG::g_pd3dDevice->CreateTexture( m_dwWidth, m_dwHeight, 1, 0, D3DFMT_A8R8G8B8,
                                          D3DPOOL_DEFAULT, &m_pOffsetTexture, NULL );
        D3DLOCKED_RECT lock;
        m_pOffsetTexture->LockRect( 0, &lock, 0, 0 );
        FillMemory( ( BYTE* )lock.pBits, m_dwWidth * m_dwHeight * 4, 128 );
        m_pOffsetTexture->UnlockRect( 0 );
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: InitShaders()
// Desc: 
//--------------------------------------------------------------------------------------
HRESULT NormalMap::InitShaders()
{
    static const D3DVERTEXELEMENT9 decl[] =
    {
        { 0,  0, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
        { 0, 16, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
        D3DDECL_END()
    };
    ATG::g_pd3dDevice->CreateVertexDeclaration( decl, &m_pUpdateOffsetVertexDecl );

    if( FAILED( ATG::LoadVertexShader( "game:\\Media\\Shaders\\UpdateOffsetMapVS.xvu", &m_pUpdateOffsetVS ) ) )
        return E_FAIL;
    if( FAILED( ATG::LoadPixelShader( "game:\\Media\\Shaders\\UpdateOffsetMapPS.xpu", &m_pUpdateOffsetPS ) ) )
        return E_FAIL;

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Update()
// Desc: 
//--------------------------------------------------------------------------------------
VOID NormalMap::Update( FurMesh* pMesh, FLOAT dt, const XMVECTOR& vGravity,
                        const XMVECTOR& vVelocity, const XMVECTOR& vOmega )
{
    // Compute invariants
    {
        // Filter coefficient for calculating m_Acceleration
        FLOAT fFilter = min( 0.25f, dt / 0.1f );

        // Translational m_Acceleration
        m_vAccel = ( 1.0f - fFilter ) * m_vAccel + fFilter * ( ( vVelocity - m_vPrevVel ) / dt );
        m_vPrevVel = vVelocity;

        // Translational force = gravity + air resistance + inertia
        // (coefficients were determined experimentally)
        XMVECTOR vTRANSLATIONFORCE = vGravity - 0.12f * vVelocity - 0.018f * m_vAccel;
        ATG::g_pd3dDevice->SetVertexShaderConstantF( VSCONST_FORCE, ( FLOAT* )&vTRANSLATIONFORCE, 1 );

        // Angular m_Acceleration
        m_vOmegaAccel = ( 1.0f - fFilter ) * m_vOmegaAccel + fFilter * ( ( vOmega - m_vPrevOmega ) / dt );
        m_vPrevOmega = vOmega;

        // Angular force = -CrossProduct(I_OMEGA, vertex_pos)
        XMVECTOR vOMEGA = 0.12f * vOmega + 0.018f * m_vOmegaAccel;
        ATG::g_pd3dDevice->SetVertexShaderConstantF( VSCONST_OMEGA, ( FLOAT* )&vOMEGA, 1 );

        // Damping factor
        FLOAT fDAMPING = max( 0.0625f, min( 0.5f, dt / 0.3f ) );
        ATG::g_pd3dDevice->SetVertexShaderConstantF( VSCONST_DAMPING, ( FLOAT* )&fDAMPING, 1 );
    }

    ATG::g_pd3dDevice->SetRenderState( D3DRS_ZENABLE, FALSE );
    ATG::g_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
    ATG::g_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_NONE );

    LPDIRECT3DSURFACE9 pBackBuffer;
    ATG::g_pd3dDevice->GetRenderTarget( 0, &pBackBuffer );

    // Update offset map
    {
        ATG::g_pd3dDevice->SetRenderTarget( 0, m_pUpdateRT );

        ATG::g_pd3dDevice->SetVertexShader( m_pUpdateOffsetVS );
        ATG::g_pd3dDevice->SetPixelShader( m_pUpdateOffsetPS );

        // Set the offset texture
        ATG::g_pd3dDevice->SetTexture( 0, m_pOffsetTexture );
        ATG::g_pd3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
        ATG::g_pd3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
        ATG::g_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP );
        ATG::g_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP );

        // Draw the object, which gets flattened into texture space by the vertex shader
        pMesh->Draw();

        ATG::g_pd3dDevice->Resolve( D3DRESOLVE_RENDERTARGET0, NULL, m_pOffsetTexture,
                                    NULL, 0, 0, NULL, 1.0f, 0L, NULL );
    }

    ATG::g_pd3dDevice->SetRenderTarget( 0, pBackBuffer );
    pBackBuffer->Release();
}


//--------------------------------------------------------------------------------------
// Name: ResetInertia()
// Desc: 
//--------------------------------------------------------------------------------------
VOID NormalMap::ResetInertia()
{
    m_vPrevVel = XMVectorSet( 0.0f, 0.0f, 0.0f, 0.0f );
    m_vAccel = XMVectorSet( 0.0f, 0.0f, 0.0f, 0.0f );
    m_vPrevOmega = XMVectorSet( 0.0f, 0.0f, 0.0f, 0.0f );
    m_vOmegaAccel = XMVectorSet( 0.0f, 0.0f, 0.0f, 0.0f );
}

