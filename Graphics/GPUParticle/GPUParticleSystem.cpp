//-----------------------------------------------------------------------------
// GPUParticleSystem.cpp
//
// Particle System in GPU
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#include <xtl.h>
#include <xgraphics.h>
#include <xboxmath.h>
#include <AtgApp.h>
#include <AtgFont.h>
#include <AtgMesh.h>
#include <AtgResource.h>
#include <AtgUtil.h>
#include "AtgTerrain.h"
#include "GPUParticleSystem.h"


//-----------------------------------------------------------------------------
// Name: Initialize()
// Desc: Initialize the GPU particle system
//-----------------------------------------------------------------------------
HRESULT CGPUParticle::Initialize( LPDIRECT3DDEVICE9 pd3dDevice, DWORD dwNumParticles,
                                  LPDIRECT3DTEXTURE9 pHeightNormalTexture,
                                  LPDIRECT3DTEXTURE9 pParticleTex )
{
    m_pd3dDevice = pd3dDevice;
    m_dwNumParticles = dwNumParticles;
    m_dwTextureWidth = 128;
    m_dwTextureHeight = dwNumParticles / m_dwTextureWidth;

    // Create render targets and textures
    RETURN_ON_FAIL( m_pd3dDevice->CreateRenderTarget( m_dwTextureWidth, m_dwTextureHeight, PARTICLEPOSFORMAT,
                                      D3DMULTISAMPLE_NONE, 0, 0, &m_pPositionDestSurfaceXY, 0 ) );
    RETURN_ON_FAIL( m_pd3dDevice->CreateRenderTarget( m_dwTextureWidth, m_dwTextureHeight, PARTICLEPOSFORMAT,
                                      D3DMULTISAMPLE_NONE, 0, 0, &m_pPositionDestSurfaceZW, 0 ) );
    RETURN_ON_FAIL( m_pd3dDevice->CreateRenderTarget( m_dwTextureWidth, m_dwTextureHeight, PARTICLEVELFORMAT,
                                      D3DMULTISAMPLE_NONE, 0, 0, &m_pVelocityDestSurfaceXY, 0 ) );
    RETURN_ON_FAIL( m_pd3dDevice->CreateRenderTarget( m_dwTextureWidth, m_dwTextureHeight, PARTICLEVELFORMAT,
                                      D3DMULTISAMPLE_NONE, 0, 0, &m_pVelocityDestSurfaceZW, 0 ) );
    RETURN_ON_FAIL( m_pd3dDevice->CreateTexture( m_dwTextureWidth, m_dwTextureHeight, 1, 0, PARTICLEPOSFORMAT,
                                 D3DPOOL_DEFAULT, &m_pPositionSourceTextureXY, 0 ) );
    RETURN_ON_FAIL( m_pd3dDevice->CreateTexture( m_dwTextureWidth, m_dwTextureHeight, 1, 0, PARTICLEPOSFORMAT,
                                 D3DPOOL_DEFAULT, &m_pPositionSourceTextureZW, 0 ) );
    RETURN_ON_FAIL( m_pd3dDevice->CreateTexture( m_dwTextureWidth, m_dwTextureHeight, 1, 0, PARTICLEVELFORMAT,
                                 D3DPOOL_DEFAULT, &m_pVelocitySourceTextureXY, 0 ) );
    RETURN_ON_FAIL( m_pd3dDevice->CreateTexture( m_dwTextureWidth, m_dwTextureHeight, 1, 0, PARTICLEVELFORMAT,
                                 D3DPOOL_DEFAULT, &m_pVelocitySourceTextureZW, 0 ) );

    XGSetVertexBufferHeader( SIZEOFPOS * dwNumParticles, D3DUSAGE_POINTS, D3DPOOL_SYSTEMMEM,
                             ( m_pPositionSourceTextureXY->Format.BaseAddress << TEXTURE_ADDRESS_SHIFT ),
                             &m_DummyVertexBufferXY );

    XGSetVertexBufferHeader( SIZEOFPOS * dwNumParticles, D3DUSAGE_POINTS, D3DPOOL_SYSTEMMEM,
                             ( m_pPositionSourceTextureZW->Format.BaseAddress << TEXTURE_ADDRESS_SHIFT ),
                             &m_DummyVertexBufferZW );

    const UPDATEVERTEX v[4] =
    {
        { -1.0f,  1.0f, 0.0f, 0.0f },
        {  1.0f,  1.0f, 1.0f, 0.0f },
        {  1.0f, -1.0f, 1.0f, 1.0f },
        { -1.0f, -1.0f, 0.0f, 1.0f },

    };

    RETURN_ON_FAIL( m_pd3dDevice->CreateVertexBuffer( sizeof( UPDATEVERTEX ) * 4, D3DUSAGE_WRITEONLY, 0,
                                      D3DPOOL_DEFAULT, &m_pUpdateVertexBuffer, 0 ) );
    VOID* pData;
    m_pUpdateVertexBuffer->Lock( 0, 0, &pData, 0 );
    memcpy( pData, v, sizeof( UPDATEVERTEX ) * 4 );
    m_pUpdateVertexBuffer->Unlock();

    VOID* pCode = NULL;

    HRESULT hr;

    // Create vertex shader for the Update
    RETURN_ON_FAIL( ATG::LoadFile( "game:\\Media\\Shaders\\ParticleUpdate.xvu", &pCode ) );
    if( FAILED( hr = m_pd3dDevice->CreateVertexShader( ( DWORD* )pCode, &m_pUpdateVertexShader ) ) )
    {
        ATG::UnloadFile( pCode );
        return hr;
    }
    ATG::UnloadFile( pCode );

    static const D3DVERTEXELEMENT9 declUpdate[] =
    {
        // First stream is first mesh
        { 0,  0, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_POSITION, 0 },
        { 0,  8, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_TEXCOORD, 0 },
        D3DDECL_END()
    };

    RETURN_ON_FAIL( m_pd3dDevice->CreateVertexDeclaration( declUpdate, &m_pUpdateVertexDeclaration ) );

    // Create diffuse pixel shader
    RETURN_ON_FAIL( ATG::LoadFile( "game:\\Media\\Shaders\\ParticleUpdate.xpu", &pCode ) );
    if( FAILED( hr = m_pd3dDevice->CreatePixelShader( ( DWORD* )pCode, &m_pUpdatePixelShader ) ) )
    {
        ATG::UnloadFile( pCode );
        return hr;
    }
    ATG::UnloadFile( pCode );

    // Create vertex shader for the Render
    RETURN_ON_FAIL( ATG::LoadFile( "game:\\Media\\Shaders\\ParticleRender.xvu", &pCode ) );
    if( FAILED( hr = m_pd3dDevice->CreateVertexShader( ( DWORD* )pCode, &m_pRenderVertexShader ) ) )
    {
        ATG::UnloadFile( pCode );
        return hr;
    }
    ATG::UnloadFile( pCode );

    static const D3DVERTEXELEMENT9 declRender[] =
    {
        { 0,  0, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_POSITION, 0 },
        { 1,  0, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_POSITION, 1 },
        D3DDECL_END()
    };

    if( FAILED( hr = m_pd3dDevice->CreateVertexDeclaration( declRender, &m_pRenderVertexDeclaration ) ) )
        return hr;

    // Create diffuse pixel shader
    RETURN_ON_FAIL( ATG::LoadPixelShader( "game:\\Media\\Shaders\\ParticleRender.xpu", &m_pRenderPixelShader ) );

    // Create Noise texture
    RETURN_ON_FAIL( m_pd3dDevice->CreateTexture( NOISETEXSIZE, NOISETEXSIZE, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT,
                                 &m_pNoiseTexture, 0 ) );
    D3DLOCKED_RECT rect;
    m_pNoiseTexture->LockRect( 0, &rect, NULL, 0 );
    DWORD* pBits = ( DWORD* )rect.pBits;
    for( DWORD i = 0; i < NOISETEXSIZE * NOISETEXSIZE; ++i )
    {
        *pBits++ = rand() << 30 | rand() << 15 | rand();
    }
    m_pNoiseTexture->UnlockRect( 0 );

    m_pParticleTexture = pParticleTex;
    m_pPlanemapTexture = pHeightNormalTexture;

    QueryPerformanceCounter( &m_iLast );
    return S_OK;
}


//-----------------------------------------------------------------------------
// Name: Release()
// Desc: Release all the objects owned by this object
//-----------------------------------------------------------------------------
HRESULT CGPUParticle::Release()
{
    m_pPositionDestSurfaceXY->Release();
    m_pPositionDestSurfaceZW->Release();
    m_pVelocityDestSurfaceXY->Release();
    m_pVelocityDestSurfaceZW->Release();
    m_pPositionSourceTextureXY->Release();
    m_pPositionSourceTextureZW->Release();
    m_pVelocitySourceTextureXY->Release();
    m_pVelocitySourceTextureZW->Release();
    m_DummyVertexBufferXY.Release();
    m_DummyVertexBufferZW.Release();

    m_pUpdateVertexBuffer->Release();
    m_pUpdateVertexDeclaration->Release();
    m_pUpdateVertexShader->Release();
    m_pUpdatePixelShader->Release();

    m_pRenderVertexDeclaration->Release();
    m_pRenderVertexShader->Release();
    m_pRenderPixelShader->Release();

    m_pNoiseTexture->Release();

    return S_OK;
}


//-----------------------------------------------------------------------------
// Name: Update()
// Desc: Update the particle system
//-----------------------------------------------------------------------------
HRESULT CGPUParticle::Update()
{
    // Get time delta
    LARGE_INTEGER iFreq;
    LARGE_INTEGER iCurrent;
    QueryPerformanceFrequency( &iFreq ); // Ticks per second
    QueryPerformanceCounter( &iCurrent );
    FLOAT fDelta = FLOAT( iCurrent.QuadPart - m_iLast.QuadPart ) / FLOAT( iFreq.QuadPart );
    m_iLast.QuadPart = iCurrent.QuadPart;

    FLOAT fAcceleration_Delta[] =
    {
        0.0f, -0.98f, 0.0f, fDelta
    };

    // Set the pixel shader constants
    m_pd3dDevice->SetPixelShaderConstantF( 0, ( FLOAT* )&fAcceleration_Delta, 1 );
    m_pd3dDevice->SetPixelShaderConstantF( 1, ( FLOAT* )&m_EmitterParamPos, 1 );
    m_pd3dDevice->SetPixelShaderConstantF( 2, ( FLOAT* )&m_EmitterParamPosRange, 1 );
    m_pd3dDevice->SetPixelShaderConstantF( 3, ( FLOAT* )&m_EmitterParamVelocityParam1, 1 );
    m_pd3dDevice->SetPixelShaderConstantF( 4, ( FLOAT* )&m_EmitterParamVelocityParam2, 1 );

    // Set the update vertex shader
    m_pd3dDevice->SetVertexShader( m_pUpdateVertexShader );
    m_pd3dDevice->SetVertexDeclaration( m_pUpdateVertexDeclaration );
    m_pd3dDevice->SetStreamSource( 0, m_pUpdateVertexBuffer, 0, sizeof( UPDATEVERTEX ) );

    // Set the update pixel shader
    m_pd3dDevice->SetPixelShader( m_pUpdatePixelShader );

    m_pd3dDevice->SetTexture( 0, m_pPositionSourceTextureXY );
    m_pd3dDevice->SetTexture( 1, m_pPositionSourceTextureZW );
    m_pd3dDevice->SetTexture( 2, m_pVelocitySourceTextureXY );
    m_pd3dDevice->SetTexture( 3, m_pVelocitySourceTextureZW );

    m_pd3dDevice->SetTexture( 4, m_pPlanemapTexture );
    m_pd3dDevice->SetTexture( 5, m_pNoiseTexture );

    m_pd3dDevice->SetRenderTarget( 0, m_pPositionDestSurfaceXY );
    m_pd3dDevice->SetRenderTarget( 1, m_pPositionDestSurfaceZW );
    m_pd3dDevice->SetRenderTarget( 2, m_pVelocityDestSurfaceXY );
    m_pd3dDevice->SetRenderTarget( 3, m_pVelocityDestSurfaceZW );

    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_CCW );

    for( DWORD i = 0; i < 5; i++ )
    {
        m_pd3dDevice->SetSamplerState( i, D3DSAMP_MIPFILTER, D3DTEXF_POINT );
        m_pd3dDevice->SetSamplerState( i, D3DSAMP_MINFILTER, D3DTEXF_POINT );
        m_pd3dDevice->SetSamplerState( i, D3DSAMP_MAGFILTER, D3DTEXF_POINT );
    }
    m_pd3dDevice->SetSamplerState( 5, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP );
    m_pd3dDevice->SetSamplerState( 5, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP );
    m_pd3dDevice->SetSamplerState( 5, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState( 5, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );

    m_pd3dDevice->DrawPrimitive( D3DPT_QUADLIST, 0, 1 );

    m_pd3dDevice->Resolve( D3DRESOLVE_RENDERTARGET0 | D3DRESOLVE_ALLFRAGMENTS, NULL,
                           m_pPositionSourceTextureXY, NULL, 0, 0, NULL, 0, 0, NULL );
    m_pd3dDevice->Resolve( D3DRESOLVE_RENDERTARGET1 | D3DRESOLVE_ALLFRAGMENTS, NULL,
                           m_pPositionSourceTextureZW, NULL, 0, 0, NULL, 0, 0, NULL );
    m_pd3dDevice->Resolve( D3DRESOLVE_RENDERTARGET2 | D3DRESOLVE_ALLFRAGMENTS, NULL,
                           m_pVelocitySourceTextureXY, NULL, 0, 0, NULL, 0, 0, NULL );
    m_pd3dDevice->Resolve( D3DRESOLVE_RENDERTARGET3 | D3DRESOLVE_ALLFRAGMENTS, NULL,
                           m_pVelocitySourceTextureZW, NULL, 0, 0, NULL, 0, 0, NULL );

    m_pd3dDevice->SetTexture( 0, NULL );
    m_pd3dDevice->SetTexture( 1, NULL );
    m_pd3dDevice->SetTexture( 2, NULL );
    m_pd3dDevice->SetTexture( 3, NULL );
    m_pd3dDevice->SetTexture( 4, NULL );
    m_pd3dDevice->SetTexture( 5, NULL );

    // Set the render target back to the back buffer
    D3DSurface* pBackBuffer;
    m_pd3dDevice->GetBackBuffer( 0, 0, 0, &pBackBuffer );
    m_pd3dDevice->SetRenderTarget( 0, pBackBuffer );
    pBackBuffer->Release();
    m_pd3dDevice->SetRenderTarget( 1, NULL );
    m_pd3dDevice->SetRenderTarget( 2, NULL );
    m_pd3dDevice->SetRenderTarget( 3, NULL );

    return S_OK;
}


//-----------------------------------------------------------------------------
// Name: Render()
// Desc: Render the particle system
//-----------------------------------------------------------------------------
HRESULT CGPUParticle::Render()
{
    // Set the update vertex shader
    m_pd3dDevice->SetVertexShader( m_pRenderVertexShader );
    m_pd3dDevice->SetVertexDeclaration( m_pRenderVertexDeclaration );
    m_pd3dDevice->SetStreamSource( 0, &m_DummyVertexBufferXY, 0, SIZEOFPOS );
    m_pd3dDevice->SetStreamSource( 1, &m_DummyVertexBufferZW, 0, SIZEOFPOS );

    // Set the update pixel shader
    m_pd3dDevice->SetPixelShader( m_pRenderPixelShader );

    // Set the render states for using point sprites
    m_pd3dDevice->SetRenderState( D3DRS_POINTSPRITEENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_BLENDOP, D3DBLENDOP_ADD );
    m_pd3dDevice->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_ONE );
    m_pd3dDevice->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_ONE );

    m_pd3dDevice->SetTexture( 0, m_pParticleTexture );

    // Render particles
    m_pd3dDevice->DrawPrimitive( D3DPT_POINTLIST, 0, m_dwNumParticles );

    // Reset render states
    m_pd3dDevice->SetRenderState( D3DRS_POINTSPRITEENABLE, FALSE );
    m_pd3dDevice->SetTexture( 0, NULL );

    return S_OK;
}


//-----------------------------------------------------------------------------
// Name: SetEmitter()
// Desc: Sets parameters for the emitter
//-----------------------------------------------------------------------------
HRESULT CGPUParticle::SetEmitter( EMITTERPARAMS* pEmitterParams )
{
    m_EmitterParamPos = pEmitterParams->EmitterParamPos;
    m_EmitterParamPosRange = pEmitterParams->EmitterParamPosRange;
    m_EmitterParamVelocityParam1 = pEmitterParams->EmitterParamVelocityParam1;
    m_EmitterParamVelocityParam2 = pEmitterParams->EmitterParamVelocityParam2;
    return S_OK;
}



