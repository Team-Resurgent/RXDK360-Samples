//--------------------------------------------------------------------------------------
// ParticleSystem.h
//
// This file contains a particle system.  It combines a very flexible particle update
// system on the CPU with billboard setup and amplification on the GPU side.  Custom
// vertex fetching on the GPU is used to convert the input vertex data (a point list)
// into a quad list.
//
// Xbox Advanced Technology Group.
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
    XMFLOAT3 m_Position;
    XMHALF2 m_SizeAspectRatio;
    XMHALF4 m_UVRectangle;
    XMHALF4 m_AxisAngle;
    D3DCOLOR m_Color;
    XMHALF2 m_CenterOffset;
};


//--------------------------------------------------------------------------------------
// Name: struct ParticleUpdateData
// Desc: Particle data that will not be sent to the GPU, but is still needed for
//       updates.  This struct makes it easy to add new non-rendering data needed for
//       particle simulation.
//--------------------------------------------------------------------------------------
struct ParticleUpdateData
{
    FLOAT m_fLifetimeRemaining;
    FLOAT m_fInvTotalLifetime;
    XMFLOAT3 m_Velocity;
    FLOAT m_fAngularVelocity;
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

    VOID    Initialize( DWORD dwMaxParticleCount, D3DBaseTexture* pTexture );
    VOID    Update( FLOAT fDeltaTime );
    VOID    Render( const XMMATRIX& matWorld, const XMMATRIX& matView, const XMMATRIX& matProj );

    VOID UpdateParticles( const ParticleRenderData* __restrict pPreviousRenderData,
                          ParticleRenderData* __restrict pCurrentRenderData,
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
    VOID    SpawnExtra( FLOAT fAmountToSpawn )
    {
        m_fSpawnAccumulator += fAmountToSpawn;
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
    ParticleUpdateData* m_pParticleUpdateData;
};
