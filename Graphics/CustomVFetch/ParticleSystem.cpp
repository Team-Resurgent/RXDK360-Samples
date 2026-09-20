//--------------------------------------------------------------------------------------
// ParticleSystem.cpp
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "ParticleSystem.h"
#include <AtgApp.h>
#include <AtgUtil.h>
#include <assert.h>

// One particle system object supports up to 64K particles.
const DWORD g_dwMaxSupportedParticleCount = 65536;
// The default spawn rate is 300 particles per second.
const FLOAT g_fDefaultSpawnRate = 300.0f;
// Gravity is 9.8 meters per second per second.
const FLOAT g_fGravity = -9.8f;

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
VOID ParticleSystem::Initialize( DWORD dwMaxParticleCount, D3DBaseTexture* pTexture )
{
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
    // but is needed to update the particles each frame.  We don't need to double
    // buffer this data.
    m_pParticleUpdateData = new ParticleUpdateData[ m_dwMaxParticleCount ];
    ZeroMemory( m_pParticleUpdateData, m_dwMaxParticleCount * sizeof( ParticleUpdateData ) );

    // Create the particle vertex declaration.  This must match the ParticleRenderData
    // structure.
    static const D3DVERTEXELEMENT9 VertexElements[] =
        {
            { 0,     0, D3DDECLTYPE_FLOAT3,     0,  D3DDECLUSAGE_POSITION,  0 },
            { 0,    12, D3DDECLTYPE_FLOAT16_2,  0,  D3DDECLUSAGE_TEXCOORD,  0 },
            { 0,    16, D3DDECLTYPE_FLOAT16_4,  0,  D3DDECLUSAGE_TEXCOORD,  1 },
            { 0,    24, D3DDECLTYPE_FLOAT16_4,  0,  D3DDECLUSAGE_TEXCOORD,  2 },
            { 0,    32, D3DDECLTYPE_D3DCOLOR,   0,  D3DDECLUSAGE_COLOR,     0 },
            { 0,    36, D3DDECLTYPE_FLOAT16_2,  0,  D3DDECLUSAGE_TEXCOORD,  3 },
            D3DDECL_END()
        };

    hr = ATG::g_pd3dDevice->CreateVertexDeclaration( VertexElements,
                                                     &m_pParticleVertexDecl );

    // Load the particle rendering FXLite effect.
    BYTE* pEffectData = NULL;
    ATG::LoadFile( "game:\\media\\effects\\billboard.fxobj",
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

    // Get a cached read-only pointer to the last frame's vertex data.
    // This data is probably being used by the GPU right now, and it will not be
    // modified, so a read-only lock is necessary here.
    ParticleRenderData* pLastFrameData = NULL;
    HRESULT hr = pLastFrameVB->Lock( 0, 0, ( VOID** )&pLastFrameData, D3DLOCK_READONLY );

    // Get a write combined pointer to the current frame's vertex data.
    ParticleRenderData* pCurrentFrameData = NULL;
    hr = pCurrentFrameVB->Lock( 0, 0, ( VOID** )&pCurrentFrameData, 0 );

    // Update the particles.
    UpdateParticles( pLastFrameData, pCurrentFrameData, fDeltaTime );

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
    m_pEffect->BeginTechniqueFromIndex( 0, 0 );
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


//--------------------------------------------------------------------------------------
// Name: frand
// Desc: Conveniently converts the integer rand function into a float rand.
//       This function is used at particle spawn time (near the end of Update()).
//--------------------------------------------------------------------------------------
inline FLOAT frand( FLOAT fMin, FLOAT fMax )
{
    DOUBLE fRange = fMax - fMin;
    DOUBLE fVal = ( ( DOUBLE )rand() * fRange ) / ( DOUBLE )RAND_MAX;
    return ( FLOAT )fVal + fMin;
}


//--------------------------------------------------------------------------------------
// Name: HSVAtoARGB
// Desc: Converts a hue/saturation/value/alpha color to alpha/red/green/blue color.
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
// Name: IsFloatNegative
// Desc: This function compares a float as an int against zero, to avoid float branching.
//--------------------------------------------------------------------------------------
inline BOOL IsFloatNegative( const FLOAT* pFloat )
{
    return ( *( INT* )pFloat < 0 );
}


//--------------------------------------------------------------------------------------
// Name: UpdateParticles
// Desc: Updates an array of particles.  It removes expired particles, updates live
//       particles, and spawns new particles.  Source data is read from two read
//       pointers, and destination data is written to two write pointers.
//       This function has been optimized to avoid float branching and utilize the
//       vector math library extensively.
//--------------------------------------------------------------------------------------
VOID ParticleSystem::UpdateParticles( const ParticleRenderData* __restrict pPreviousRenderData,
                                      ParticleRenderData* __restrict pCurrentRenderData,
                                      FLOAT fDeltaTime )
 {
    // Set up the particle render data pointers.  Each one points to a different VB.
    ParticleRenderData* __restrict pCurrentRenderWrite = pCurrentRenderData;
    const ParticleRenderData* __restrict pCurrentRenderRead = pPreviousRenderData;

    // Set up the particle update data pointers.  They both point to the same array.
    ParticleUpdateData* pCurrentUpdateWrite = m_pParticleUpdateData;
    const ParticleUpdateData* pCurrentUpdateRead = m_pParticleUpdateData;

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
    DWORD dwNewActiveCount = 0;
    for( DWORD i = 0; i < m_dwActiveParticleCount; ++i )
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
        XMVECTOR vLifetimeRemaining = XMVectorReplicatePtr( &pCurrentUpdateRead->m_fLifetimeRemaining );
        XMVECTOR vInvTotalLifetime = XMVectorReplicatePtr( &pCurrentUpdateRead->m_fInvTotalLifetime );
        vLifetimeRemaining -= vDeltaTime;
        XMVECTOR vLifetimeFraction = vLifetimeRemaining * vInvTotalLifetime;
        vLifetimeFraction = vOne - vLifetimeFraction;
        vLifetimeFraction = XMVectorMax( vLifetimeFraction, vZero );

        // Load particle data from read pointer
        XMVECTOR vAngularVelocity = XMVectorReplicatePtr( &pCurrentUpdateRead->m_fAngularVelocity );
        XMVECTOR vVelocity = XMLoadFloat3( &pCurrentUpdateRead->m_Velocity );
        XMVECTOR vPosition = XMLoadFloat3( &pCurrentRenderRead->m_Position );
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
        XMStoreFloat3( &pCurrentRenderWrite->m_Position, vPosition );
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
        XMStoreFloat3( &pCurrentUpdateWrite->m_Velocity, vVelocity );

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
    DWORD dwSpawnCount = (DWORD)fSpawnCount;

    // Cap the spawn count against the maximum particle count
    DWORD dwMaxParticlesToSpawn = m_dwMaxParticleCount - m_dwActiveParticleCount;
    dwSpawnCount = min( dwSpawnCount, dwMaxParticlesToSpawn );

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
        XMStoreFloat3( &pCurrentUpdateWrite->m_Velocity, vInitialVelocity );
        pCurrentUpdateWrite->m_fAngularVelocity = frand( -XM_PI, XM_PI );

        // Create new render data
        XMStoreFloat3( &pCurrentRenderWrite->m_Position, XMVectorZero() );
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
    m_dwActiveParticleCount += dwSpawnCount;
}
