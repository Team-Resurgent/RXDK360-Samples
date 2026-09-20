//--------------------------------------------------------------------------------------
// ParticleSystem.h
//
// This file contains a particle system.  It combines a very flexible particle update
// system on the CPU with billboard setup and amplification on the GPU side.  Custom
// vertex fetching on the GPU is used to convert the input vertex data (a point list)
// into a quad list.
// 
// This code was originally written for the CustomVFetch sample, and was modified with
// asynchronous resource updating for the AsyncResources sample.
//
// XNA Developer Connection Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#pragma once

#include <xtl.h>
#include <fxl.h>
#include "AtgWorkerThread.h"

//--------------------------------------------------------------------------------------
// Name: struct ParticleRenderData
// Desc: Particle data that will be sent to the GPU.  It will also be used during the
//       updates.
//--------------------------------------------------------------------------------------
struct ParticleRenderData
{
    XMFLOAT3 m_vPosition;
    XMHALF2 m_SizeAspectRatio;
    XMHALF4 m_UVRectangle;
    XMHALF4 m_AxisAngle;
    D3DCOLOR m_Color;
    XMHALF2 m_CenterOffset;
};

// ParticleRenderDataDeclElements must match ParticleRenderData above.
extern const D3DVERTEXELEMENT9 g_ParticleRenderDataDeclElements[];

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
    XMFLOAT3 m_vVelocity;
    FLOAT m_fAngularVelocity;
};


//--------------------------------------------------------------------------------------
// Name: struct UpdateTick
// Desc: Contains the state involved in updating the particle system for one frame.
//--------------------------------------------------------------------------------------
struct UpdateTick
{
    FLOAT m_fDeltaTime;
    DWORD m_dwSpawnCount;
    ParticleUpdateData* m_pParticleUpdateData;
    DWORD m_dwMaxParticleCount;
    DWORD* m_pActiveParticleCount;
    volatile DWORD* m_pDeathCount;
    XMFLOAT3 m_vEmitterPos;
};


//--------------------------------------------------------------------------------------
// Name: struct WorkerData
// Desc: Describes a unit of work to be performed by a worker thread.
//--------------------------------------------------------------------------------------
struct WorkerData
{
    D3DASYNCBLOCK m_AsyncBlock;

    D3DVertexBuffer* m_pLastFrameVB;
    D3DVertexBuffer* m_pCurrentFrameVB;

    UpdateTick m_Tick;
};


//--------------------------------------------------------------------------------------
// Name: class ParticleSystem
// Desc: This class manages a particle system, including updating and rendering.
//--------------------------------------------------------------------------------------
class ParticleSystem : public IWorkerThreadContext
{
public:
    VOID            Initialize( DWORD dwMaxParticleCount, D3DBaseTexture* pTexture, DWORD dwWorkerHWThread );
    VOID            Update( FLOAT fDeltaTime );
    VOID            Render( const XMMATRIX& matWorld, const XMMATRIX& matView, const XMMATRIX& matProj );

    VOID            SetAsynchronous( BOOL bAsync )
    {
        m_bAsynchronous = bAsync;
    }
    BOOL            IsAsynchronous() const
    {
        return m_bAsynchronous;
    }

    VOID            SetBufferCount( DWORD dwCount )
    {
        m_dwNumBuffers = min( max( dwCount, 1 ), m_dwMaxBuffers );
    }
    DWORD           GetBufferCount() const
    {
        return m_dwNumBuffers;
    }
    DWORD           GetMaxBufferCount() const
    {
        return m_dwMaxBuffers;
    }

    VOID            SetEmitterPos( XMVECTOR vEmitterPos )
    {
        XMStoreFloat3( &m_vEmitterPos, vEmitterPos );
    }
    XMVECTOR        GetEmitterPos() const
    {
        return XMLoadFloat3( &m_vEmitterPos );
    }

    DWORD           GetActiveCount() const
    {
        return m_dwActiveParticleCount;
    }
    DWORD           GetMaxCount() const
    {
        return m_dwMaxParticleCount;
    }

    FLOAT           GetSpawnRate() const
    {
        return m_fSpawnRate;
    }
    VOID            SetSpawnRate( FLOAT fNewRate )
    {
        m_fSpawnRate = fNewRate;
    }
    VOID            SpawnExtra( FLOAT fAmountToSpawn )
    {
        m_fSpawnAccumulator += fAmountToSpawn;
    }

    virtual VOID    DoWork( VOID* pData );

private:
    // Buffering and frame count
    static const DWORD m_dwMaxBuffers = 2;
    DWORD m_dwNumBuffers;
    BOOL m_bAsynchronous;
    DWORD m_dwFrameCount;

    // Particle counts and spawn rate
    DWORD m_dwMaxParticleCount;
    DWORD m_dwActiveParticleCount;
    DWORD m_dwRenderParticleCount;
    FLOAT m_fSpawnRate;
    FLOAT m_fSpawnAccumulator;
    volatile DWORD m_dwDeathCount;

    // FXLite effect members
    FXLEffect* m_pEffect;
    FXLHANDLE m_hWVPMatrix;
    FXLHANDLE m_hCameraUpVector;
    FXLHANDLE m_hCameraRightVector;
    FXLHANDLE m_hTexture;

    // Particle texture
    D3DBaseTexture* m_pParticleTexture;

    // Particle buffers
    D3DVertexDeclaration* m_pParticleVertexDecl;
    D3DVertexBuffer* m_pParticleVB[m_dwMaxBuffers];
    ParticleUpdateData* m_pParticleUpdateData;

    // Emitter origin position
    XMFLOAT3 m_vEmitterPos;

    static VOID     UpdateParticleSystem( UpdateTick const& Tick, ParticleRenderData* pLastFrameData,
                                          ParticleRenderData* pCurrentFrameData );
    static VOID     UpdateParticleSystemBuffers( UpdateTick const& Tick, D3DASYNCBLOCK AsyncBlock,
                                                 D3DVertexBuffer* pLastFrameVB, D3DVertexBuffer* pCurrentFrameVB );

    WorkerThread* m_pWorkerThread;

    WorkerData* m_pPendingAsynchronousUpdate;
};

