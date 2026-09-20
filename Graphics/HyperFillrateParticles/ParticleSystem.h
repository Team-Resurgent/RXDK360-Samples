//--------------------------------------------------------------------------------------
// ParticleSystem.h
//
// This file contains a particle system.  It combines a very flexible particle update
// system on the CPU with billboard setup and amplification on the GPU side.  Custom
// vertex fetching on the GPU is used to convert the input vertex data (a point list)
// into a quad list.
//
// XNA Developer Connection
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma once

#include <xtl.h>
#include <fxl.h>

//--------------------------------------------------------------------------------------
// Name: struct ParticleRenderData
// Desc: Particle data that will be sent to the GPU.  It will also be used during the
//       updates.
//--------------------------------------------------------------------------------------
struct ParticleRenderData
{
    XMFLOAT4 m_Position_Color;
    XMHALF2  m_SizeAspectRatio;
    XMHALF4  m_UVRectangle;
    XMHALF2  m_CenterOffset;
};


//--------------------------------------------------------------------------------------
// Name: struct ParticleUpdateData
// Desc: Particle data that will not be sent to the GPU, but is still needed for
//       updates.  This struct makes it easy to add new non-rendering data needed for
//       particle simulation.
//--------------------------------------------------------------------------------------
struct ParticleUpdateData
{
    // We are sacrificing 8 bytes here so that the data struct can be aligned
    // and reduce cost of VMX processing. 
    XMFLOAT4 m_vLife_InvTtlLfe_AngVel; // w is unused
    XMFLOAT4 m_Velocity; // w is unused
};


//--------------------------------------------------------------------------------------
// Name: class ParticleSystem
// Desc: This class manages a particle system, including updating and rendering.
//--------------------------------------------------------------------------------------
class ParticleSystem
{
public:
            ParticleSystem();
            ~ParticleSystem();

    VOID    Initialize( DWORD dwMaxParticleCount, D3DBaseTexture* pTexture, const INT iTechnique );
    VOID    Update( FLOAT fDeltaTime );
    VOID    Render( const XMMATRIX& matWorld, const XMMATRIX& matView, const XMMATRIX& matProj );

    VOID UpdateParticles( const ParticleRenderData* __restrict pPreviousRenderData,
                          ParticleRenderData* __restrict pCurrentRenderData,
                          const ParticleUpdateData* __restrict pPreviousUpdateData,
                          ParticleUpdateData* __restrict pCurrentUpdateData,
                          FLOAT fDeltaTime );

    DWORD   GetActiveCount() const
    {
        return m_dwActiveParticleCount;
    }
    DWORD   GetMaxCount() const
    {
        return m_dwMaxParticleCount;
    }

    FLOAT   GetSpawnRate() const
    {
        return m_fSpawnRate;
    }
    VOID    SetSpawnRate( FLOAT fNewRate )
    {
        m_fSpawnRate = fNewRate;
    }

private:
    DWORD m_dwMaxParticleCount;
    DWORD m_dwActiveParticleCount;
    FLOAT m_fSpawnRate;
    DWORD m_dwFrameCount;
    FLOAT m_fSpawnAccumulator;
    FXLEffect* m_pEffect;
    FXLHANDLE m_hWVPMatrix;
    FXLHANDLE m_hCameraUpVector;
    FXLHANDLE m_hCameraRightVector;
    FXLHANDLE m_hTexture;
    D3DBaseTexture* m_pParticleTexture;
    D3DVertexDeclaration* m_pParticleVertexDecl;
    D3DVertexBuffer* m_pParticleVB[2];
    ParticleUpdateData* m_pParticleUpdateData[2];
    FLOAT m_fCounter;

    // Choose a render technique (See FX file)
    INT m_iTechnique;
};
