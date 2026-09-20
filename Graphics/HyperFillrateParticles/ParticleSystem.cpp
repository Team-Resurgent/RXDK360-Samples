//--------------------------------------------------------------------------------------
// ParticleSystem.cpp
//
// XNA Developer Connection
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "ParticleSystem.h"
#include <AtgApp.h>
#include <AtgUtil.h>
#include <assert.h>
#include "GPUTimer.h"

// Use a write barrier so the compiler and CPU cannot re-order writes.  Write-combined memory
// (GPU buffer) is sensitive to write order.
extern "C" void _WriteBarrier();

// One particle system object supports up to 64K particles.
const DWORD     g_dwMaxSupportedParticleCount = 65536;
// The default spawn rate per second
const FLOAT     g_fDefaultSpawnRate = 80.0;

//--------------------------------------------------------------------------------------
// Name: ParticleSystem constructor
//--------------------------------------------------------------------------------------
ParticleSystem::ParticleSystem()
{
    XMemSet( this, 0, sizeof( ParticleSystem ) );
    m_fSpawnRate = g_fDefaultSpawnRate;
}


//--------------------------------------------------------------------------------------
// Name: ParticleSystem destructor
//--------------------------------------------------------------------------------------
ParticleSystem::~ParticleSystem()
{
}


//--------------------------------------------------------------------------------------
// Name: Initialize()
// Desc: Set up vertex buffers, vertex decl for particles, and the FXLite effect.
//--------------------------------------------------------------------------------------
VOID ParticleSystem::Initialize( DWORD dwMaxParticleCount, D3DBaseTexture* pTexture, const INT iTechnique )
{
    m_iTechnique = iTechnique;
    m_fCounter = 0.0f;
    m_dwMaxParticleCount = min( dwMaxParticleCount, g_dwMaxSupportedParticleCount );
    m_dwFrameCount = 0;

    // Create 2 vertex buffers to hold particle render data.
    DWORD dwMemSizeVB = m_dwMaxParticleCount * sizeof( ParticleRenderData );
    HRESULT hr = ATG::g_pd3dDevice->CreateVertexBuffer( dwMemSizeVB, 0, 0,
                                                        D3DPOOL_DEFAULT,
                                                        &m_pParticleVB[0],
                                                        NULL );
    hr = ATG::g_pd3dDevice->CreateVertexBuffer( dwMemSizeVB, 0, 0,
                                                D3DPOOL_DEFAULT,
                                                &m_pParticleVB[1],
                                                NULL );

    // Create the particle update data - this data is not needed at render time, 
    // but is needed to update the particles each frame.  This algorithm would execute faster
    // if this memory was allocated as write-combined and all memory access was continous.
    m_pParticleUpdateData[0] = new ParticleUpdateData[ m_dwMaxParticleCount ];
    m_pParticleUpdateData[1] = new ParticleUpdateData[ m_dwMaxParticleCount ];

    ZeroMemory( m_pParticleUpdateData[0], m_dwMaxParticleCount * sizeof( ParticleUpdateData ) );
    ZeroMemory( m_pParticleUpdateData[1], m_dwMaxParticleCount * sizeof( ParticleUpdateData ) );

    // Create the particle vertex declaration.  This must match the ParticleRenderData
    // structure.
    static const D3DVERTEXELEMENT9 VertexElements[] =
    {
        { 0,     0, D3DDECLTYPE_FLOAT4,     0,  D3DDECLUSAGE_POSITION,  0 },
        { 0,    16, D3DDECLTYPE_FLOAT16_2,  0,  D3DDECLUSAGE_TEXCOORD,  0 },
        { 0,    20, D3DDECLTYPE_FLOAT16_4,  0,  D3DDECLUSAGE_TEXCOORD,  1 },
        { 0,    28, D3DDECLTYPE_FLOAT16_2,  0,  D3DDECLUSAGE_TEXCOORD,  2 },

        D3DDECL_END()
    };

    hr = ATG::g_pd3dDevice->CreateVertexDeclaration( VertexElements,
                                                     &m_pParticleVertexDecl );

    // Load the particle rendering FXLite effect.
    BYTE* pEffectData = NULL;
    ATG::LoadFile( "game:\\media\\effects\\HyperFillrateParticles.fxobj",
                   ( VOID** )&pEffectData, NULL );
    FXLCreateEffect( ATG::g_pd3dDevice, pEffectData, NULL, &m_pEffect );

    // Cache parameter handles for the effect.
    m_hWVPMatrix = m_pEffect->GetParameterHandle( "world_view_proj_matrix" );
    m_hCameraUpVector = m_pEffect->GetParameterHandle( "camera_up_vector" );
    m_hCameraRightVector = m_pEffect->GetParameterHandle( "camera_right_vector" );
    m_hTexture = m_pEffect->GetParameterHandle( "diffuse_texture" );

    // Set the particle texture into the effect.
    m_pParticleTexture = pTexture;
    m_pEffect->SetSampler( m_hTexture, pTexture );
}


//--------------------------------------------------------------------------------------
// Name: Update()
// Desc: Selects and locks the current and last frame VBs and sends the data pointers
//       to the particle update method.
//--------------------------------------------------------------------------------------
VOID ParticleSystem::Update( FLOAT fDeltaTime )
{
    // Get pointers to the current and last frame vertex buffers.
    D3DVertexBuffer* pLastFrameVB = m_pParticleVB[ ( m_dwFrameCount + 1 ) % 2 ];
    D3DVertexBuffer* pCurrentFrameVB = m_pParticleVB[ m_dwFrameCount % 2 ];

    ParticleUpdateData* pLastFrameUpdate = m_pParticleUpdateData[ ( m_dwFrameCount + 1 ) % 2 ];
    ParticleUpdateData* pCurrentFrameUpdate = m_pParticleUpdateData[ m_dwFrameCount % 2 ];

    // Get a cached read-only pointer to the last frame's vertex data.
    // This data is probably being used by the GPU right now, and it will not be
    // modified, so a read-only lock is necessary here.
    ParticleRenderData* pLastFrameData = NULL;
    HRESULT hr = pLastFrameVB->Lock( 0, 0, ( VOID** )&pLastFrameData, D3DLOCK_READONLY );

    // Get a write combined pointer to the current frame's vertex data.
    ParticleRenderData* pCurrentFrameData = NULL;
    hr = pCurrentFrameVB->Lock( 0, 0, ( VOID** )&pCurrentFrameData, 0 );

    // Update the particles.
    UpdateParticles( pLastFrameData, pCurrentFrameData, pLastFrameUpdate, pCurrentFrameUpdate, fDeltaTime );

    // Release the VB locks.
    pLastFrameVB->Unlock();
    pCurrentFrameVB->Unlock();
}


//--------------------------------------------------------------------------------------
// Name: Render
// Desc: Renders the particle system.
//--------------------------------------------------------------------------------------
VOID ParticleSystem::Render( const XMMATRIX& matWorld, const XMMATRIX& matView, const XMMATRIX& matProj )
{
    // Select the current vertex buffer.
    D3DVertexBuffer* pCurrentFrameVB = m_pParticleVB[ m_dwFrameCount % 2 ];

    // Compute WVP matrix and camera vectors.
    XMMATRIX matWVP = matWorld * matView * matProj;

    XMVECTOR vCamRight;
    XMVECTOR vCamUp;
    XMVECTOR vDummy;
    XMMATRIX matInvV = XMMatrixInverse( &vDummy, matView );
    vCamRight = matInvV.r[0];
    vCamUp = matInvV.r[1];

    // Start the particle rendering effect.
    m_pEffect->BeginTechniqueFromIndex( m_iTechnique, 0 );
    m_pEffect->BeginPassFromIndex( 0 );

    // Set the camera data into the effect.
    m_pEffect->SetMatrixF4x4A( m_hWVPMatrix, ( const FXLFLOATA* )&matWVP );
    m_pEffect->SetVectorFA( m_hCameraRightVector, ( const FXLFLOATA* )&vCamRight );
    m_pEffect->SetVectorFA( m_hCameraUpVector, ( const FXLFLOATA* )&vCamUp );

    m_pEffect->Commit();

    // Set up the vertex stream and decl.
    ATG::g_pd3dDevice->SetStreamSource( 0, pCurrentFrameVB, 0, sizeof( ParticleRenderData ) );
    ATG::g_pd3dDevice->SetVertexDeclaration( m_pParticleVertexDecl );

    // Draw the particles using DrawVertices.  The vertex shader will amplify the
    // vertex buffer (point list) into a quad list.
    ATG::g_pd3dDevice->DrawVertices( D3DPT_QUADLIST, 0, m_dwActiveParticleCount * 4 );

    // Clear vertex stream so GPU will free up the vertex buffer for subsequent Lock()
    ATG::g_pd3dDevice->SetStreamSource( 0, NULL, 0, 0 );

    // End the effect.
    m_pEffect->EndPass();
    m_pEffect->EndTechnique();

    ++m_dwFrameCount;
}


// Optimize for speed, not size.  Improves performance by about 5%!  The compiler will unroll more loops 
// at cost of generating more code.  Use this setting only for code that is hit frequently (many times per frame) 
#ifdef NDEBUG
#pragma optimize("t", on)
#pragma optimize("s", off)
#endif


//--------------------------------------------------------------------------------------
// Name: XMRand
// Desc: Quickly generate 4 random floating point values in a VMX register.  Using frand()
//       should be avoided in inner loops as it is expensive and causes a 100% LHS.
//       Generates values in range of 1.0 to 2.0.  
//       NOTE: vPerm and vSeed must be maintained together.
//       WARNING:  Use with caution, this algorithm has not undergone thorough testing
//       and is optimized for speed.
//--------------------------------------------------------------------------------------
static const XMVECTORI vPerm  = { 0x0c080f06, 0x0d0b0103, 0x0e050700, 0x020a0409 };
__inline XMVECTOR XMRand( XMVECTOR& seed, XMVECTOR& perm )
{
    const XMVECTORI vAdder = { 0x592fa04b, 0x9e03756c, 0xf883c2da, 0x14e136b7 };
    const XMVECTORI vSwiz0 = { 0x10000a06, 0x10010b0e, 0x10020c08, 0x10030d0f };
    const XMVECTOR  vOne   = { 1.f, 1.f, 1.f, 1.f };

    XMVECTOR m;
    // grab a byte from the middle of the last dword of the seed
    XMVECTOR temp = __vspltb( seed, 0xd );
    // generate our 4 floats
    m = __vperm( seed, vOne, *(XMVECTOR*)&vSwiz0 );
    // scramble the seed
    seed   = __vperm( seed, seed, perm );
    // change the swizzle-pattern based on temp
    perm   = __vxor( perm, temp );
    // 1.0f is 0x3f800000. the vperm above gets the 0x3f right, but the next-highest bit also needs to be set.
    m = __vor( m, vOne );
    // add a somewhat random (but unchanging) value to the seed
    seed   = __vadduwm( seed, *(XMVECTOR*)&vAdder );
    
    return m;
}

// Initial random seed for XMRand().  To be thread safe, this pair must be
// maintained per-thread.
static XMVECTOR vRandomSeed = { 1,7,17,23 };
static XMVECTOR vPermLocal = vPerm;

#pragma warning( push )
// Ignore the C4700 warning about an uninitialized local variable that is used VMX code
#pragma warning (disable : 4700)
//--------------------------------------------------------------------------------------
// Name: UpdateParticles
// Desc: Updates an array of particles.  It removes expired particles, updates live
//       particles, and spawns new particles.  Source data is read from two read
//       pointers, and destination data is written to two write pointers.
//       This function is highly optimized and can process about 10,000,000 particles/s.
//--------------------------------------------------------------------------------------
VOID ParticleSystem::UpdateParticles( const ParticleRenderData* __restrict pPreviousRenderData,
                                      ParticleRenderData* __restrict pCurrentRenderData,
                                      const ParticleUpdateData* __restrict pPreviousUpdateData,
                                      ParticleUpdateData* __restrict pCurrentUpdateData,
                                      FLOAT fDeltaTime )
 {
    // Set up the particle render data pointers.  Each one points to a different VB.
    ParticleRenderData* __restrict pCurrentRenderWrite = pCurrentRenderData;
    const ParticleRenderData* __restrict pCurrentRenderRead = pPreviousRenderData;

    // Set up the particle update data pointers.  They both point to the same array.
    ParticleUpdateData* pCurrentUpdateWrite = pCurrentUpdateData;
    const ParticleUpdateData* pCurrentUpdateRead = pPreviousUpdateData;

    const FLOAT fCounter = m_fCounter + fDeltaTime;
    m_fCounter = fCounter;
    const XMVECTOR vDeltaTime = XMVectorReplicate( fDeltaTime );

    // Introduce some pseudo-random wind via 2 sin curves at different periods
    FLOAT iTechniques[] = { 0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f };
    const FLOAT fWindCounter = fCounter + ( iTechniques[m_iTechnique] * 4.5f );
    FLOAT fWind = 30.0f * sin( fWindCounter ) * sin( fWindCounter / 3.9f );
    const XMVECTOR vAcceleration = XMVectorSet( fWind * fDeltaTime, 0.0f, 0, 0 );

    // Build some select control constants
    static const XMVECTOR vSelectXYZ = XMVectorSelectControl( 0, 0, 0, 1 );
    static const XMVECTOR vSelectYZW = XMVectorSelectControl( 1, 0, 0, 0 );

    const XMVECTOR vOne = XMVectorSplatOne();
    const XMVECTOR vZero = XMVectorZero();

    // Step 1: Update existing particles
    DWORD dwNewActiveCount = 0;

    // don't loop over a member variable otherwise the compiler will reload from memory and cause many LHS
    const DWORD dwActiveParticleCount = m_dwActiveParticleCount;
    for( DWORD i = 0; i < dwActiveParticleCount; ++i )
    {
        // Prefetch the next particle data.
        __dcbt( 2*sizeof( ParticleRenderData ), pCurrentRenderRead );
        __dcbt( 2*sizeof( ParticleRenderData ), pCurrentRenderWrite );
        __dcbt( 2*sizeof( ParticleUpdateData ), pCurrentUpdateRead );
        __dcbt( 2*sizeof( ParticleUpdateData ), pCurrentUpdateWrite );

        // Compute lifetime remaining.  This vector could be used to sample a curve to
        // compute the value for another parameter.
        XMVECTOR vLife_InvTtlLfe_AngVel = XMLoadFloat4( &pCurrentUpdateRead->m_vLife_InvTtlLfe_AngVel );
        XMVECTOR vLifetimeRemaining = XMVectorSplatX(vLife_InvTtlLfe_AngVel);
     
        // Check particle's remaining lifetime.
        // Do the entire branch operation in VMX registers to avoid a LHS.  
        // This is still an expensive operation.  Pushing the XMVectorGreaterR() further back in time would help 
        // but at a cost of making the code significantly more complex.
        UINT uiResult;
        XMVectorGreaterR( &uiResult, vDeltaTime, vLifetimeRemaining );

        if( XMComparisonAllTrue( uiResult ) )
        {
            // Particle is dead; skip particle.  It is not written to the destination,
            // so it will be overwritten by the next live particle.
            ++pCurrentUpdateRead;
            ++pCurrentRenderRead;
            continue;
        }
        ++dwNewActiveCount;

        XMVECTOR vInvTotalLifetime = XMVectorSplatY(vLife_InvTtlLfe_AngVel);
        vLifetimeRemaining -= vDeltaTime;

        XMVECTOR vLifetimeFraction = vLifetimeRemaining * vInvTotalLifetime;
        vLifetimeFraction = vOne - vLifetimeFraction;
        vLifetimeFraction = XMVectorMax( vLifetimeFraction, vZero );

        // Load particle data from read pointer
        XMVECTOR vVelocity = XMLoadFloat4( &pCurrentUpdateRead->m_Velocity );
        XMVECTOR vPosition_Color = XMLoadFloat4( &pCurrentRenderRead->m_Position_Color );

        const FLOAT FADEOUT_SEC = 0.5f; // controls how long it takes particles to fade out
        // Store a "Fade out" timer (1 byte value) inside color. Later, it gets swizzled into tex coord.
        const XMVECTOR vFadeTimer = { FADEOUT_SEC, FADEOUT_SEC, FADEOUT_SEC, FADEOUT_SEC };
        const XMVECTOR v1_FadeTimer = { 1.0f / FADEOUT_SEC, 1.0f / FADEOUT_SEC, 1.0f / FADEOUT_SEC, 1.0f / FADEOUT_SEC };

        XMVECTOR vFadeoutComparison = XMVectorGreater( vFadeTimer, vLifetimeRemaining );
        XMVECTOR vFadeOutFract = XMVectorMultiply(vLifetimeRemaining, v1_FadeTimer);
        XMVECTOR vColor = XMVectorSelect(vOne, vFadeOutFract, vFadeoutComparison);

        XMVECTOR vSizeAspectRatio = XMLoadHalf2( &pCurrentRenderRead->m_SizeAspectRatio );
        XMVECTOR vUVRectangle = XMLoadHalf4( &pCurrentRenderRead->m_UVRectangle );
        XMVECTOR vCenterOffset = XMLoadHalf2( &pCurrentRenderRead->m_CenterOffset );

        // Update position
        vVelocity += vAcceleration;
        vPosition_Color += XMVectorMultiply( vVelocity, vDeltaTime );

        // store color in vPosition.w
        vPosition_Color = XMVectorSelect( vPosition_Color, vColor, vSelectXYZ );

        // Update position in new render data
        XMStoreFloat4( &pCurrentRenderWrite->m_Position_Color, vPosition_Color );
        _WriteBarrier();

        // Copy data we didn't touch.  Pack it into a single 128-bit register so it can be
        // written in a single instruction.  This is critical for write-combined memory.
        XMVECTOR vPack128;
        vPack128 = __vpkd3d( vPack128, vSizeAspectRatio, VPACK_FLOAT16_2, VPACK_32, 3  );
        vPack128 = __vpkd3d( vPack128, vUVRectangle, VPACK_FLOAT16_4, VPACK_64LO, 1  );
        vPack128 = __vpkd3d( vPack128, vCenterOffset, VPACK_FLOAT16_2, VPACK_32,  0 );
        XMStoreInt4( (UINT *)&pCurrentRenderWrite->m_SizeAspectRatio, vPack128 );
        _WriteBarrier();

        // Store update data to new update data
        XMVECTOR vLife_InvTtlLfe_AngVelStore = XMVectorSelect(vLife_InvTtlLfe_AngVel, vLifetimeRemaining, vSelectYZW);
        _WriteBarrier();
        XMStoreFloat4( &pCurrentUpdateWrite->m_vLife_InvTtlLfe_AngVel, vLife_InvTtlLfe_AngVelStore);
        _WriteBarrier();
        XMStoreFloat4( &pCurrentUpdateWrite->m_Velocity, vVelocity );

        // Increment pointers
        ++pCurrentUpdateRead;
        ++pCurrentUpdateWrite;
        ++pCurrentRenderRead;
        ++pCurrentRenderWrite;
    }

    m_dwActiveParticleCount = dwNewActiveCount;


    // Step 2: Spawn new particles

    // Compute number of particles to spawn this frame
    m_fSpawnAccumulator += ( fDeltaTime * m_fSpawnRate );

    // Subtract a whole number of particles from the spawn accumulator
    FLOAT fSpawnCount = floor( m_fSpawnAccumulator );
    m_fSpawnAccumulator -= fSpawnCount;
    DWORD dwSpawnCount = ( DWORD )fSpawnCount;

    // Cap the spawn count against the maximum particle count
    DWORD dwMaxParticlesToSpawn = m_dwMaxParticleCount - dwNewActiveCount;
    dwSpawnCount = min( dwSpawnCount, dwMaxParticlesToSpawn );

    // Spawn new particles
    for( DWORD i = 0; i < dwSpawnCount; ++i )
    {
        // Create new update data
        const FLOAT fLifetime = 0.8f;
        XMVECTOR v1_Lifetime = XMVectorReplicate(1.f / fLifetime);
        // the value above is 0.8 in numerical format
        XMVECTOR vLifeTimeRemaining = XMVectorReplicate(fLifetime);
       
        // Store life time remaining and total life time.
        XMVECTOR vLife_Store = XMVectorInsert(vLifeTimeRemaining, v1_Lifetime, 0, 0, 1, 0, 0);
        XMStoreFloat4( &pCurrentUpdateWrite->m_vLife_InvTtlLfe_AngVel, vLife_Store );
        
        // Generate a random initial velocity
        // Returns values from 1-2. 
        XMVECTOR vRandom = XMRand(vRandomSeed, *(XMVECTOR*)&vPermLocal);
        XMVECTOR vInitialVelocity = vRandom;
        
        // Scale X and Z from -1 to 1 and Y to 3.0.
        const XMVECTOR vInitVelScaleMul = { 2.0f, 0.0f, 2.0f };
        const XMVECTOR vInitVelScaleAdd = { -3.0f, 3.0f, -3.0f };
        vInitialVelocity = XMVectorMultiplyAdd(vInitialVelocity, vInitVelScaleMul, vInitVelScaleAdd );
        vInitialVelocity = XMVector3NormalizeEst( vInitialVelocity );
        // Scale to final velocity
        vInitialVelocity = XMVectorScale( vInitialVelocity, 10.0f );
        // Store the initial velocity
        XMStoreFloat4( &pCurrentUpdateWrite->m_Velocity, vInitialVelocity );
        _WriteBarrier();

        // Fill in the 'color' in W field as 1.0
        XMVECTOR vPosition_Color_Store = XMVectorSelect( vZero, vOne, vSelectXYZ );
        XMStoreFloat4( &pCurrentRenderWrite->m_Position_Color, vPosition_Color_Store );
        _WriteBarrier();

        // Generate a random size
        XMVECTOR vSizeAspectRatio = vRandom;
        vSizeAspectRatio = XMVectorSplatW(vSizeAspectRatio);
        vSizeAspectRatio = XMVectorScale( vSizeAspectRatio, 0.7f );

        // Pack all the HALF registers into a single 128 bit register because this buffer 
        // uses write combined memory and massive penalties occur if not written correctly.
        XMVECTOR vPack128 = XMVectorZero();        
        vPack128 = __vpkd3d( vPack128, vSizeAspectRatio, VPACK_FLOAT16_2, VPACK_32, 3  );
        const XMVECTOR vUVRectangles[] = { { 0, 0, 1.0f, 1.0f } };
        XMVECTOR vRect = vUVRectangles[ 0 ];
        vPack128 = __vpkd3d( vPack128, vRect, VPACK_FLOAT16_4, VPACK_64LO, 1  );
        XMStoreInt4( (UINT *)&pCurrentRenderWrite->m_SizeAspectRatio, vPack128 );

        // Increment write pointers
        ++pCurrentUpdateWrite;
        ++pCurrentRenderWrite;
    }

    m_dwActiveParticleCount += dwSpawnCount;
}
#pragma warning( pop )

#ifdef NDEBUG
// Restore initial compiler optimization settings
#pragma optimize("", on)
#endif