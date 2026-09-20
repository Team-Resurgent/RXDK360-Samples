//--------------------------------------------------------------------------------------
// ParticleSystem.cpp
//
// XNA Developer Connection Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "ParticleSystem.h"
#include <AtgApp.h>
#include <AtgUtil.h>
#include <assert.h>

// One particle system object supports up to 4.1 million particles.  This limit ensures
// that the single draw call to draw the particle system doesn't exceed the maximum
// allowable vertex count.  With a bit of rearchitecting, even this limitation could
// be removed.
const DWORD g_dwMaxSupportedParticleCount = GPU_MAX_VERTEX_BUFFER_DIMENSION / 4;
// The default spawn rate is 750 particles per second.
const FLOAT g_fDefaultSpawnRate = 750.0f;
// Gravity is 9.8 meters per second per second.
const FLOAT g_fGravity = -9.8f;

// ParticleRenderDataDeclElements must match ParticleRenderData above.
const D3DVERTEXELEMENT9 g_ParticleRenderDataDeclElements[] =
{
    { 0,     0, D3DDECLTYPE_FLOAT3,     0,  D3DDECLUSAGE_POSITION,  0 },
    { 0,    12, D3DDECLTYPE_FLOAT16_2,  0,  D3DDECLUSAGE_TEXCOORD,  0 },
    { 0,    16, D3DDECLTYPE_FLOAT16_4,  0,  D3DDECLUSAGE_TEXCOORD,  1 },
    { 0,    24, D3DDECLTYPE_FLOAT16_4,  0,  D3DDECLUSAGE_TEXCOORD,  2 },
    { 0,    32, D3DDECLTYPE_D3DCOLOR,   0,  D3DDECLUSAGE_COLOR,     0 },
    { 0,    36, D3DDECLTYPE_FLOAT16_2,  0,  D3DDECLUSAGE_TEXCOORD,  3 },
    D3DDECL_END()
};

//--------------------------------------------------------------------------------------
// Name: Initialize()
// Desc: Set up vertex buffers, vertex decl for particles, and the FXLite effect.
//--------------------------------------------------------------------------------------
VOID ParticleSystem::Initialize( DWORD dwMaxParticleCount, D3DBaseTexture* pTexture, DWORD dwWorkerHardwareThread )
{
    m_fSpawnRate = g_fDefaultSpawnRate;
    m_bAsynchronous = TRUE;
    m_dwMaxParticleCount = min( dwMaxParticleCount, g_dwMaxSupportedParticleCount );
    m_dwFrameCount = 0;
    m_vEmitterPos = XMFLOAT3( 0, 0, 0 );
    m_dwNumBuffers = m_dwMaxBuffers;
    m_dwDeathCount = 0;
    m_dwActiveParticleCount = 0;
    m_dwRenderParticleCount = 0;
    m_fSpawnAccumulator = 0;

    // Create 2 vertex buffers to hold particle render data.
    DWORD dwMemSizeVB = m_dwMaxParticleCount * sizeof( ParticleRenderData );

    HRESULT hr = S_OK;

    for( DWORD i = 0; i < m_dwMaxBuffers; ++i )
    {
        hr = ATG::g_pd3dDevice->CreateVertexBuffer( dwMemSizeVB, 0, 0,
                                                    D3DPOOL_DEFAULT,
                                                    &m_pParticleVB[i],
                                                    NULL );

        // Zero out the contents of the vertex buffer.
        // Since VB contents are used as previous frame state for particle updates,
        // it's important that the VB starts out with known zero data.
        VOID* pData = NULL;
        m_pParticleVB[i]->Lock( 0, 0, &pData, 0 );
        ZeroMemory( pData, dwMemSizeVB );
        m_pParticleVB[i]->Unlock();
    }

    // Create the particle update data - this data is not needed at render time,
    // but is needed to update the particles each frame.  We don't need to double
    // buffer this data.
    m_pParticleUpdateData = new ParticleUpdateData[m_dwMaxParticleCount];
    ZeroMemory( m_pParticleUpdateData, m_dwMaxParticleCount * sizeof( ParticleUpdateData ) );

    // Create the particle vertex declaration.
    hr = ATG::g_pd3dDevice->CreateVertexDeclaration( g_ParticleRenderDataDeclElements,
                                                     &m_pParticleVertexDecl );

    // Load the particle rendering FXLite effect.
    BYTE* pEffectData = NULL;
    hr = ATG::LoadFile( "game:\\media\\effects\\billboard.fxobj",
                        ( VOID** )&pEffectData, NULL );
    if( FAILED( hr ) )
        ATG::FatalError( "Could not load particle rendering effect file." );

    FXLCreateEffect( ATG::g_pd3dDevice, pEffectData, NULL, &m_pEffect );

    // Cache parameter handles for the effect.
    m_hWVPMatrix = m_pEffect->GetParameterHandle( "world_view_proj_matrix" );
    m_hCameraUpVector = m_pEffect->GetParameterHandle( "camera_up_vector" );
    m_hCameraRightVector = m_pEffect->GetParameterHandle( "camera_right_vector" );
    m_hTexture = m_pEffect->GetParameterHandle( "diffuse_texture" );

    // Set the particle texture into the effect.
    m_pParticleTexture = pTexture;
    m_pEffect->SetSampler( m_hTexture, pTexture );

    m_pWorkerThread = new WorkerThread( this, dwWorkerHardwareThread );

    m_pPendingAsynchronousUpdate = NULL;
}

//--------------------------------------------------------------------------------------
// Name: Update()
// Desc: Selects and locks the current and last frame VBs and sends the data pointers
//       to the particle update method.
//--------------------------------------------------------------------------------------
VOID ParticleSystem::Update( FLOAT fDeltaTime )
{
    // In order to keep a sane update loop, we need to make any decisions
    // in the synchronous part of the update (which is right here).
    // Specifically, how many new particles we're going to create.
    // In this case, the only influence that the asynchronous update
    // has on this code happens when the maximum amount of particles is reached,
    // so that we will schedule more or less particle creations depending on
    // how many particle deaths we've seen so far. This is unavoidable because
    // particle deaths are tallied in the asynchronous update.

    // The asynchronous update function keeps a tally of dead particles here.
    const DWORD dwDeathCount = InterlockedExchange( ( volatile LONG* )&m_dwDeathCount, 0 );

    // Compute number of particles to spawn this frame
    m_fSpawnAccumulator += ( fDeltaTime * m_fSpawnRate );

    // Subtract a whole number of particles from the spawn accumulator
    const FLOAT fSpawnMax = floor( m_fSpawnAccumulator );
    m_fSpawnAccumulator -= fSpawnMax;

    // Figure out how many particles we want to render.
    const DWORD dwSpawnMax = ( DWORD )fSpawnMax;
    const DWORD dwRenderParticleMax = m_dwRenderParticleCount - dwDeathCount + dwSpawnMax;

    // And limit the actual particles rendered (and spawned, of course)
    // so they will fit the statically-sized buffers.
    const DWORD dwRenderParticleCount = min( m_dwMaxParticleCount, dwRenderParticleMax );
    const DWORD dwSpawnCount = dwSpawnMax - ( dwRenderParticleMax - dwRenderParticleCount );

    m_dwRenderParticleCount = dwRenderParticleCount;

    // Get pointers to the current and last frame vertex buffers.
    DWORD dwIndexPrevious = ( m_dwFrameCount + m_dwNumBuffers - 1 ) % m_dwNumBuffers;
    D3DVertexBuffer* pLastFrameVB = m_pParticleVB[dwIndexPrevious];
    DWORD dwIndexCurrent = m_dwFrameCount % m_dwNumBuffers;
    D3DVertexBuffer* pCurrentFrameVB = m_pParticleVB[dwIndexCurrent];

    if( m_bAsynchronous )
    {
        // Asynchronous update.  Construct a WorkerData packet and fill it in with the
        // parameters for the update.
        WorkerData* pData = new WorkerData;

        pData->m_pLastFrameVB = pLastFrameVB;
        pData->m_pCurrentFrameVB = pCurrentFrameVB;
        pData->m_Tick.m_fDeltaTime = fDeltaTime;
        pData->m_Tick.m_dwSpawnCount = dwSpawnCount;
        pData->m_Tick.m_pParticleUpdateData = m_pParticleUpdateData;
        pData->m_Tick.m_dwMaxParticleCount = dwRenderParticleCount;
        pData->m_Tick.m_pActiveParticleCount = &m_dwActiveParticleCount;
        pData->m_Tick.m_pDeathCount = &m_dwDeathCount;
        pData->m_Tick.m_vEmitterPos = m_vEmitterPos;

        // Store the WorkerData packet, as it will be used in Render().
        m_pPendingAsynchronousUpdate = pData;
    }
    else
    {
        // Synchronous update.  Fill in update parameters into an UpdateTick packet, and
        // send it directly to the particle update code right now.
        UpdateTick Tick;

        Tick.m_fDeltaTime = fDeltaTime;
        Tick.m_dwSpawnCount = dwSpawnCount;
        Tick.m_pParticleUpdateData = m_pParticleUpdateData;
        Tick.m_dwMaxParticleCount = dwRenderParticleCount;
        Tick.m_pActiveParticleCount = &m_dwActiveParticleCount;
        Tick.m_pDeathCount = &m_dwDeathCount;
        Tick.m_vEmitterPos = m_vEmitterPos;

        // Update the particles.
        UpdateParticleSystemBuffers( Tick, NULL, pLastFrameVB, pCurrentFrameVB );

        // Make sure the asynchronous update member is empty.
        m_pPendingAsynchronousUpdate = NULL;
    }
}


//--------------------------------------------------------------------------------------
// Name: Render
// Desc: Renders the particle system.  When asynchronous updates are enabled, the work
//       item to update particles will be kicked off inside this method.
//--------------------------------------------------------------------------------------
VOID ParticleSystem::Render( const XMMATRIX& matWorld, const XMMATRIX& matView, const XMMATRIX& matProj )
{
    // Schedule any pending asynchronous update work now.
    if( m_bAsynchronous )
    {
        // Obtain the WorkerData packet that was prepared earlier in the Update() method.
        WorkerData* pData = m_pPendingAsynchronousUpdate;
        assert( pData != NULL );

        PIXBeginNamedEvent( 0, "BlockOnAsyncRes(%p, %x)", pData->m_pCurrentFrameVB, pData->m_pCurrentFrameVB->Fence );

        // Insert the GPU asynchronous block here.  This command instructs the GPU to
        // spin wait if it encounters the following draw call before the dynamic
        // resources are completed and unlocked on another thread.
        // Note that pLastFrameVB will be locked read-only, but we still need to
        // lock it asynchronously in the "read resources" parameter.
        IDirect3DResource9* writeResources[] = { pData->m_pCurrentFrameVB };
        IDirect3DResource9* readResources[] = { pData->m_pLastFrameVB };
        pData->m_AsyncBlock = ATG::g_pd3dDevice->InsertBlockOnAsyncResources( ARRAYSIZE( writeResources ),
                                                                              writeResources,
                                                                              ARRAYSIZE( readResources ),
                                                                              readResources, 0 );

        PIXEndNamedEvent();

        PIXBeginNamedEvent( 0, "AddWork(%x)", DWORD( pData->m_AsyncBlock ) );

        // Kick off the asynchronous resource update on the worker thread.
        m_pWorkerThread->AddWork( pData );

        PIXEndNamedEvent();

        m_pPendingAsynchronousUpdate = NULL;
    }

    // Select the current vertex buffer.
    D3DVertexBuffer* pCurrentFrameVB = m_pParticleVB[ m_dwFrameCount % m_dwNumBuffers ];

    PIXBeginNamedEvent( 0, "ParticleRender(%p, %x)", pCurrentFrameVB, pCurrentFrameVB->Fence );

    // Compute WVP matrix and camera vectors.
    XMMATRIX matWVP = matWorld * matView * matProj;

    XMVECTOR vDummy;
    XMMATRIX matInvV = XMMatrixInverse( &vDummy, matView );
    XMVECTOR vCamRight = matInvV.r[0];
    XMVECTOR vCamUp = matInvV.r[1];

    // Start the particle rendering effect.
    m_pEffect->BeginTechniqueFromIndex( 0, 0 );
    m_pEffect->BeginPassFromIndex( 0 );

    // Set the camera data into the effect.
    m_pEffect->SetMatrixF4x4A( m_hWVPMatrix, ( const FXLFLOATA* )&matWVP );
    m_pEffect->SetVectorFA( m_hCameraRightVector, ( const FXLFLOATA* )&vCamRight );
    m_pEffect->SetVectorFA( m_hCameraUpVector, ( const FXLFLOATA* )&vCamUp );

    m_pEffect->Commit();

    // Set up the vertex stream and decl.
    ATG::g_pd3dDevice->SetIndices( NULL );
    ATG::g_pd3dDevice->SetStreamSource( 0, pCurrentFrameVB, 0, sizeof( ParticleRenderData ) );
    ATG::g_pd3dDevice->SetVertexDeclaration( m_pParticleVertexDecl );

    // Draw the particles using DrawVertices.  The vertex shader will amplify the
    // vertex buffer (point list) into a quad list.
    ATG::g_pd3dDevice->DrawVertices( D3DPT_QUADLIST, 0, m_dwRenderParticleCount * 4 );

    // Unselect dynamic resources promptly.
    ATG::g_pd3dDevice->SetStreamSource( 0, NULL, 0, sizeof( ParticleRenderData ) );

    // End the effect.
    m_pEffect->EndPass();
    m_pEffect->EndTechnique();

    PIXEndNamedEvent();

    ++m_dwFrameCount;
}


//--------------------------------------------------------------------------------------
// Name: frand
// Desc: Conveniently converts the integer rand function into a float rand.
//       This function is used at particle spawn time.
//--------------------------------------------------------------------------------------
inline FLOAT frand( FLOAT fMin, FLOAT fMax )
{
    FLOAT fRange = fMax - fMin;
    FLOAT fVal = ( ( FLOAT )rand() * fRange ) / ( FLOAT )RAND_MAX;
    return ( FLOAT )fVal + fMin;
}


//--------------------------------------------------------------------------------------
// Name: HSVAtoARGB
// Desc: Converts a hue/saturation/value/alpha color to alpha/red/green/blue color.
//       Note: This function is not intended for a fast inner loop; it is slow due to
//       float -> int conversions and unpredictable branching.
//--------------------------------------------------------------------------------------
inline XMVECTOR HSVAtoARGB( FLOAT fHue, FLOAT fSaturation, FLOAT fValue, FLOAT fAlpha )
{
    FLOAT fHueSector = fHue / 60.0f;
    FLOAT fHueFragment = fHueSector - floor( fHueSector );
    FLOAT fP = fValue * ( 1.0f - fSaturation );
    FLOAT fQ = fValue * ( 1.0f - fHueFragment * fSaturation );
    FLOAT fT = fValue * ( 1.0f - ( 1.0f - fHueFragment ) * fSaturation );
    DWORD dwHueSector = ( DWORD )floor( fHueSector );
    switch( dwHueSector )
    {
        case 0:
            return XMVectorSet( fAlpha, fValue, fT, fP );
        case 1:
            return XMVectorSet( fAlpha, fQ, fValue, fP );
        case 2:
            return XMVectorSet( fAlpha, fP, fValue, fT );
        case 3:
            return XMVectorSet( fAlpha, fP, fQ, fValue );
        case 4:
            return XMVectorSet( fAlpha, fT, fP, fValue );
        case 5:
            return XMVectorSet( fAlpha, fValue, fP, fQ );
    }
    return XMVectorZero();
}

//--------------------------------------------------------------------------------------
// Name: XMLoadFloat
// Desc: This function loads a value from a float pointer into all components of a vector.
//--------------------------------------------------------------------------------------
inline XMVECTOR XMVectorReplicate( const FLOAT* pFloat )
{
    return __vspltw( __lvlx( pFloat, 0 ), 0 );
}

//--------------------------------------------------------------------------------------
// Name: IsFloatNegative
// Desc: This function compares a float as an int against zero, to avoid float branching.
//--------------------------------------------------------------------------------------
inline BOOL IsFloatNegative( const FLOAT* pFloat )
{
    return ( *( INT* )pFloat < 0 );
}


//--------------------------------------------------------------------------------------
// Name: UpdateParticleSystem
// Desc: Updates an array of particles.  It removes expired particles, updates live
//       particles, and spawns new particles.  Source data is read from two read
//       pointers, and destination data is written to two write pointers.
//       This function has been optimized to avoid float branching and utilize the
//       vector math library extensively.
//--------------------------------------------------------------------------------------
VOID ParticleSystem::UpdateParticleSystem( UpdateTick const& Tick, ParticleRenderData* __restrict pLastFrameData, ParticleRenderData* __restrict pCurrentFrameData )
 {
    PIXBeginNamedEvent( 0, "UpdateParticles" );

    FLOAT const fDeltaTime = Tick.m_fDeltaTime;

    // Set up the particle render data pointers.  Each one points to a different VB.
    ParticleRenderData* __restrict pCurrentRenderWrite = pCurrentFrameData;
    const ParticleRenderData* __restrict pCurrentRenderRead = pLastFrameData;

    // Set up the particle update data pointers.  They both point to the same array.
    ParticleUpdateData* pCurrentUpdateWrite = Tick.m_pParticleUpdateData;
    const ParticleUpdateData* pCurrentUpdateRead = Tick.m_pParticleUpdateData;

    const XMVECTOR vDeltaTime = XMVectorReplicate( fDeltaTime );

    // Constant acceleration, assuming all particles have the same mass
    const XMVECTOR vAcceleration = XMVectorSet( 0, g_fGravity * fDeltaTime, 0, 0 );

    // Constants for particle bounce
    static const XMVECTOR vVelocityBounceAmount = XMVectorSet( 1, -0.95f, 1, 1 );

    // Build some select control constants
    static const XMVECTOR vSelectXYZ = XMVectorSelectControl( 0, 0, 0, 1 );
    static const XMVECTOR vSelectYZW = XMVectorSelectControl( 1, 0, 0, 0 );

    static const XMVECTOR vOne = XMVectorReplicate( 1.0f );
    const XMVECTOR vZero = XMVectorZero();

    // Step 1: Update existing particles
    DWORD dwActiveParticleCount = *Tick.m_pActiveParticleCount;
    DWORD dwNewActiveCount = 0;
    for( DWORD i = 0; i < dwActiveParticleCount; ++i )
    {
        // Prefetch the next particle data.
        __dcbt( sizeof( ParticleRenderData ), pCurrentRenderRead );
        __dcbt( sizeof( ParticleUpdateData ), pCurrentUpdateRead );

        // Check particle's remaining lifetime.
        if( IsFloatNegative( &pCurrentUpdateRead->m_fLifetimeRemaining ) )
        {
            // Particle is dead; skip particle.  It is not written to the destination,
            // so it will be overwritten by the next live particle.
            ++pCurrentUpdateRead;
            ++pCurrentRenderRead;
            continue;
        }
        ++dwNewActiveCount;

        // Compute lifetime remaining.  This vector could be used to sample a curve to
        // compute the value for another parameter.
        XMVECTOR vLifetimeRemaining = XMVectorReplicate( pCurrentUpdateRead->m_fLifetimeRemaining );
        XMVECTOR vInvTotalLifetime = XMVectorReplicate( pCurrentUpdateRead->m_fInvTotalLifetime );
        vLifetimeRemaining -= vDeltaTime;
        XMVECTOR vLifetimeFraction = vLifetimeRemaining * vInvTotalLifetime;
        vLifetimeFraction = vOne - vLifetimeFraction;
        vLifetimeFraction = XMVectorMax( vLifetimeFraction, vZero );

        // Load particle data from read pointer
        XMVECTOR vAngularVelocity = XMVectorReplicate( pCurrentUpdateRead->m_fAngularVelocity );
        XMVECTOR vVelocity = XMLoadFloat3( &pCurrentUpdateRead->m_vVelocity );
        XMVECTOR vPosition = XMLoadFloat3( &pCurrentRenderRead->m_vPosition );
        XMVECTOR vAxisAngle = XMLoadHalf4( &pCurrentRenderRead->m_AxisAngle );
        XMVECTOR vColor = XMLoadUByteN4( (XMUBYTEN4*)&pCurrentRenderRead->m_Color );
        XMVECTOR vSizeAspectRatio = XMLoadHalf2( &pCurrentRenderRead->m_SizeAspectRatio );
        XMVECTOR vUVRectangle = XMLoadHalf4( &pCurrentRenderRead->m_UVRectangle );
        XMVECTOR vCenterOffset = XMLoadHalf2( &pCurrentRenderRead->m_CenterOffset );

        // Update position
        vVelocity = XMVectorSelect( vVelocity, vZero, vSelectXYZ );
        vVelocity += vAcceleration;
        vPosition += XMVectorMultiply( vVelocity, vDeltaTime );

        // Collide with ground plane
        XMVECTOR vControl = __vcmpgtfp( vPosition, vZero );
        XMVECTOR vVelocityBounce = XMVectorMultiply( vVelocity, vVelocityBounceAmount );
        vVelocity = XMVectorSelect( vVelocityBounce, vVelocity, vControl );

        // Update angle
        vAxisAngle += XMVectorSelect( vZero, vAngularVelocity * vDeltaTime, vSelectXYZ );
        vAxisAngle = XMVectorModAngles( vAxisAngle );

        // Update color
        XMVECTOR vAlpha = vLifetimeFraction * vLifetimeFraction;
        vAlpha = XMVectorSubtract( vOne, vAlpha );
        vColor = XMVectorSelect( vColor, vAlpha, vSelectYZW );

        // Update position in new render data
        XMStoreFloat3( &pCurrentRenderWrite->m_vPosition, vPosition );
        // Update axis angle in new render data
        XMVECTOR vAxis = XMVector3NormalizeEst( vVelocity );
        vAxisAngle = XMVectorSelect( vAxis, vAxisAngle, vSelectXYZ );
        XMStoreHalf4( &pCurrentRenderWrite->m_AxisAngle, vAxisAngle );
        // Update color in new render data
        XMStoreUByteN4( (XMUBYTEN4*)&pCurrentRenderWrite->m_Color, vColor );
        // Copy data we didn't touch
        XMStoreHalf2( &pCurrentRenderWrite->m_SizeAspectRatio, vSizeAspectRatio );
        XMStoreHalf4( &pCurrentRenderWrite->m_UVRectangle, vUVRectangle );
        XMStoreHalf2( &pCurrentRenderWrite->m_CenterOffset, vCenterOffset );

        // Store update data to new update data
        pCurrentUpdateWrite->m_fInvTotalLifetime = vInvTotalLifetime.x;
        pCurrentUpdateWrite->m_fLifetimeRemaining = vLifetimeRemaining.x;
        pCurrentUpdateWrite->m_fAngularVelocity = vAngularVelocity.x;
        XMStoreFloat3( &pCurrentUpdateWrite->m_vVelocity, vVelocity );

        // Increment pointers
        ++pCurrentUpdateRead;
        ++pCurrentUpdateWrite;
        ++pCurrentRenderRead;
        ++pCurrentRenderWrite;
    }

    // Reduce the number of particles rendered in future frames.
    DWORD const dwDeathCount = dwActiveParticleCount - dwNewActiveCount;
    InterlockedExchangeAdd( ( volatile LONG* )Tick.m_pDeathCount, dwDeathCount );

    dwActiveParticleCount = dwNewActiveCount;

    // Step 2: Spawn new particles

    // Cap the spawn count against the maximum particle count
    DWORD const dwSpawnCount = Tick.m_dwSpawnCount;

    XMVECTOR vEmitterPos = XMLoadFloat3( &Tick.m_vEmitterPos );

    // Spawn new particles
    for( DWORD i = 0; i < dwSpawnCount; ++i )
    {
        // Create new update data
        FLOAT fLifetime = frand( 0.5f, 4.0f );
        pCurrentUpdateWrite->m_fInvTotalLifetime = 1.0f / fLifetime;
        pCurrentUpdateWrite->m_fLifetimeRemaining = fLifetime;
        XMVECTOR vInitialVelocity = XMVectorSet( frand( -3.0f, 3.0f ),
                                                 10.0f,
                                                 frand( -3.0f, 3.0f ),
                                                 0 );
        vInitialVelocity = XMVector3NormalizeEst( vInitialVelocity );
        vInitialVelocity = XMVectorScale( vInitialVelocity, frand( 5.0f, 15.0f ) );
        XMStoreFloat3( &pCurrentUpdateWrite->m_vVelocity, vInitialVelocity );
        pCurrentUpdateWrite->m_fAngularVelocity = frand( -XM_PI, XM_PI );

        // Create new render data
        XMStoreFloat3( &pCurrentRenderWrite->m_vPosition, vEmitterPos );
        XMStoreHalf2( &pCurrentRenderWrite->m_SizeAspectRatio, XMVectorSet( frand( 0.05f, 0.2f ), frand( 0.5f, 1.5f ), 0, 0 ) );
        const XMVECTOR vUVRectangles[] = {
            { 0, 0, 0.5f, 0.5f },
            { 0.5f, 0, 0.5f, 0.5f },
            { 0, 0.5f, 0.5f, 0.5f },
            { 0.5f, 0.5f, 0.25f, 0.25f },
            { 0.75f, 0.5f, 0.25f, 0.25f },
            { 0.5f, 0.75f, 0.25f, 0.25f },
            { 0.75f, 0.75f, 0.25f, 0.25f }
        };
        DWORD dwIndex = rand() % ARRAYSIZE( vUVRectangles );
        XMVECTOR vRect = vUVRectangles[ dwIndex ];
        XMStoreHalf4( &pCurrentRenderWrite->m_UVRectangle, vRect );
        XMStoreHalf4( &pCurrentRenderWrite->m_AxisAngle, XMVector3NormalizeEst( vInitialVelocity ) );
        XMVECTOR vColor = HSVAtoARGB( frand( 0.0f, 360.0f ), frand( 0.0f, 1.0f ), 1.0f, 1.0f );
        XMStoreUByteN4( (XMUBYTEN4*)&pCurrentRenderWrite->m_Color, vColor );
        XMStoreHalf2( &pCurrentRenderWrite->m_CenterOffset, XMVectorZero() );

        // Increment write pointers
        ++pCurrentUpdateWrite;
        ++pCurrentRenderWrite;
    }

    dwActiveParticleCount += dwSpawnCount;

    *Tick.m_pActiveParticleCount = dwActiveParticleCount;

    PIXEndNamedEvent();
}

//--------------------------------------------------------------------------------------
// Name: UpdateParticleSystemBuffers
// Desc: Locks the current frame's particle VB for writing, and the previous frame's VB
//       for reading.  Asynchronous locks are used since this function could be called
//       from a non-render thread.
//--------------------------------------------------------------------------------------
VOID ParticleSystem::UpdateParticleSystemBuffers( UpdateTick const& Tick, D3DASYNCBLOCK AsyncBlock,
                                                  D3DVertexBuffer* pLastFrameVB, D3DVertexBuffer* pCurrentFrameVB )
{
    PIXBeginNamedEvent( 0, "UpdateParticleSystemBuffers(%x)", DWORD( AsyncBlock ) );

    // Use asynchronous locks to gain access to the VB contents.
    // Note that if the AsyncBlock parameter is NULL, AsyncLock() behaves exactly the
    // same as a standard Lock().  This allows this method to be used both for
    // asynchronous and synchronous updates without any conditional logic.

    // Get a cached read-only pointer to the last frame's vertex data.
    // This data is probably being used by the GPU right now, and it will not be
    // modified, so a read-only lock is necessary here.
    ParticleRenderData* pLastFrameData = NULL;
    HRESULT hr = pLastFrameVB->AsyncLock( AsyncBlock, 0, 0, ( VOID** )&pLastFrameData, D3DLOCK_READONLY );

    // Get a write combined pointer to the current frame's vertex data.
    ParticleRenderData* pCurrentFrameData = NULL;
    hr = pCurrentFrameVB->AsyncLock( AsyncBlock, 0, 0, ( VOID** )&pCurrentFrameData, 0 );

    // Update the particles.
    UpdateParticleSystem( Tick, pLastFrameData, pCurrentFrameData );

    // Release the locks.
    pLastFrameVB->Unlock();
    pCurrentFrameVB->Unlock();

    PIXEndNamedEvent();
}

//--------------------------------------------------------------------------------------
// Name: DoWork
// Desc: This is the task worker method for the worker thread, when a work item is being
//       executed.  The work to be done is extracted from the WorkerData struct, and
//       then it is passed off to UpdateParticleSystemBuffers.  When the update is
//       complete, the GPU is asynchronously notified that it may proceed with the
//       draw call corresponding to these dynamic resources.
//--------------------------------------------------------------------------------------
VOID ParticleSystem::DoWork( VOID* pData )
{
    WorkerData* pWorkerData = ( WorkerData* )pData;

    // Update the particles.
    UpdateParticleSystemBuffers( pWorkerData->m_Tick, pWorkerData->m_AsyncBlock, pWorkerData->m_pLastFrameVB,
                                 pWorkerData->m_pCurrentFrameVB );

    PIXBeginNamedEvent( 0, "SignalAsyncResources(%x)", DWORD( pWorkerData->m_AsyncBlock ) );

    // Release the asynchronous block on the GPU, allowing the GPU to proceed with
    // rendering.  In practice, this block is released before it is encountered,
    // resulting in zero stall time on the GPU.
    ATG::g_pd3dDevice->SignalAsyncResources( pWorkerData->m_AsyncBlock );

    PIXEndNamedEvent();

    delete pWorkerData;
}

