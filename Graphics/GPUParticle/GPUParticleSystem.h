//-----------------------------------------------------------------------------
// GPUParticle.h
//
// Header file for doing per-particle calculations on the GPU.
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#pragma once


//-----------------------------------------------------------------------------
// Globals variables and definitions
//-----------------------------------------------------------------------------
const FLOAT     GPUFLOAT_MAX = +32.0f;  // R300 float max value
const FLOAT     GPUFLOAT_MIN = -32.0f;  // R300 float min value

const D3DFORMAT PARTICLEPOSFORMAT = D3DFMT_G32R32F;
const DWORD     SIZEOFPOS = 8;
const D3DFORMAT PARTICLEVELFORMAT = D3DFMT_G32R32F;
const DWORD     SIZEOFVEL = 8;

//const D3DFORMAT PARTICLEDATAFORMAT = D3DFMT_A16B16G16R16;
//const DWORD     SIZEOFPOS  = 8;

//const D3DFORMAT PARTICLEDATAFORMAT = D3DFMT_A8B8G8R8;
//const DWORD     SIZEOFPOS  = 4;

#define VERTEXBUFFER_ADDRESS_SHIFT 2
#define VERTEXBUFFER_SIZE_SHIFT    2
#define TEXTURE_ADDRESS_SHIFT     12

const DWORD     NOISETEXSIZE = 128;


//-----------------------------------------------------------------------------
// Class and structures
//-----------------------------------------------------------------------------
struct UPDATEVERTEX
{
    FLOAT sx, sy;
    FLOAT u, v;
};

struct EMITTERPARAMS
{
    XMFLOAT3 EmitterParamPos;            // Position (3 floats)
    XMFLOAT3 EmitterParamPosRange;       // Position range (3 floats)
    XMFLOAT4 EmitterParamVelocityParam1; // Velocity range in XZ plane, offset in YZ, Velocity range in XY plane, offset in XY,
    XMFLOAT4 EmitterParamVelocityParam2; // Speed range, speed offset, Life range, life offset
};


//-----------------------------------------------------------------------------
// Name: class CCPUParticle
// Desc: Class for the GPU-accelerated particle system
//-----------------------------------------------------------------------------
class CGPUParticle
{
public:
            CGPUParticle()
            {
            };
            ~CGPUParticle()
            {
            };
    HRESULT Initialize( LPDIRECT3DDEVICE9, DWORD dwNumParticles,
                        LPDIRECT3DTEXTURE9 pTexMap, LPDIRECT3DTEXTURE9 pTex );
    HRESULT Release();
    HRESULT Update();
    HRESULT Render();

    HRESULT SetEmitter( EMITTERPARAMS* pEmitterParams );

    HRESULT SetPlanemap( LPDIRECT3DTEXTURE9 pTexMap )
    {
        m_pPlanemapTexture = pTexMap;
    }

    VOID    GetParticleBuffer( LPDIRECT3DTEXTURE9* ppTexturePos1,
                               LPDIRECT3DTEXTURE9* ppTexturePos2,
                               LPDIRECT3DTEXTURE9* ppTextureVel1,
                               LPDIRECT3DTEXTURE9* ppTextureVel2 )
    {
        ( *ppTexturePos1 ) = m_pPositionSourceTextureXY;
        ( *ppTexturePos2 ) = m_pPositionSourceTextureZW;
        ( *ppTextureVel1 ) = m_pVelocitySourceTextureXY;
        ( *ppTextureVel2 ) = m_pVelocitySourceTextureZW;
    };

    DWORD   GetNumParticles()
    {
        return m_dwNumParticles;
    }

private:
    LPDIRECT3DDEVICE9 m_pd3dDevice;
    LPDIRECT3DSURFACE9 m_pPositionDestSurfaceXY;
    LPDIRECT3DSURFACE9 m_pPositionDestSurfaceZW;
    LPDIRECT3DSURFACE9 m_pVelocityDestSurfaceXY;
    LPDIRECT3DSURFACE9 m_pVelocityDestSurfaceZW;

    LPDIRECT3DTEXTURE9 m_pPositionSourceTextureXY;
    LPDIRECT3DTEXTURE9 m_pPositionSourceTextureZW;
    LPDIRECT3DTEXTURE9 m_pVelocitySourceTextureXY;
    LPDIRECT3DTEXTURE9 m_pVelocitySourceTextureZW;

    D3DVertexBuffer m_DummyVertexBufferXY;
    D3DVertexBuffer m_DummyVertexBufferZW;
    LPDIRECT3DVERTEXBUFFER9 m_pUpdateVertexBuffer;

    LPDIRECT3DVERTEXDECLARATION9 m_pUpdateVertexDeclaration;
    LPDIRECT3DVERTEXSHADER9 m_pUpdateVertexShader;
    LPDIRECT3DPIXELSHADER9 m_pUpdatePixelShader;

    LPDIRECT3DVERTEXDECLARATION9 m_pRenderVertexDeclaration;
    LPDIRECT3DVERTEXSHADER9 m_pRenderVertexShader;
    LPDIRECT3DPIXELSHADER9 m_pRenderPixelShader;

    DWORD m_dwNumParticles;
    DWORD m_dwTextureWidth;
    DWORD m_dwTextureHeight;

    // Render stuff
    LPDIRECT3DTEXTURE9 m_pParticleTexture;
    LPDIRECT3DTEXTURE9 m_pPlanemapTexture;
    LPDIRECT3DTEXTURE9 m_pNoiseTexture;

    LARGE_INTEGER m_iLast;
    XMFLOAT3 m_EmitterParamPos;
    XMFLOAT3 m_EmitterParamPosRange;
    XMFLOAT4 m_EmitterParamVelocityParam1;
    XMFLOAT4 m_EmitterParamVelocityParam2;
};

