//--------------------------------------------------------------------------------------
// CFluidFlowSim.cpp
//
// Fluid flow simulation based on Stam's "Stable Fluids" SIGGRAPH 99 paper  and Harris'
// flow implementation in OpenGL
//
// Authored by Pedro Sander, ATI Research
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
#include <AtgPostProcess.h>
#include "CFluidFlowSim.h"


// Access to the global D3D device
namespace ATG
{
extern D3DDevice* g_pd3dDevice;
}

// Helper class for doing post-process effects
ATG::PostProcess    g_PostProcess;


// Attributes for the fluid flow simulator
const D3DFORMAT     RENDERTARGETFLOWFORMAT = D3DFMT_A16B16G16R16F;
const D3DFORMAT     TEXTUREFLOWFORMAT = D3DFMT_A16B16G16R16F_EXPAND;
const D3DFORMAT     TEXTUREFLOWFORMAT_ALPHA = D3DFMT_A16B16G16R16F;
const D3DFORMAT     PARTICLEPOSFORMAT = D3DFMT_G32R32F;
const DWORD         NUM_PARTICLES = 65536;

// Shaders used by the fluid flow simulator
D3DVertexShader*    g_pParticleRepelRenderVS = NULL;
D3DVertexShader*    g_pParticleRenderVS = NULL;
D3DPixelShader*     g_pParticleInitPS = NULL;
D3DPixelShader*     g_pParticleAdvectPS = NULL;
D3DPixelShader*     g_pParticleRepelPS = NULL;
D3DPixelShader*     g_pParticleRepelRenderPS = NULL;
D3DPixelShader*     g_pParticleRenderPS = NULL;
D3DPixelShader*     g_pFlowAddImpulsePS = NULL;
D3DPixelShader*     g_pFlowAdvectPS = NULL;
D3DPixelShader*     g_pFlowBCStripPS = NULL;
D3DPixelShader*     g_pFlowDivergencePS = NULL;
D3DPixelShader*     g_pFlowJacobiPS = NULL;
D3DPixelShader*     g_pFlowSubtractGradientPS = NULL;
D3DPixelShader*     g_pFlowDisplayPS = NULL;

// Shader constant assignments for the above shaders
const DWORD         PSCONST_vFlowDims = 4;
const DWORD         PSCONST_vInvFlowDims = 5;
const DWORD         PSCONST_fTimeStep = 6;
const DWORD         PSCONST_fDissipation = 7;
const DWORD         PSCONST_fScale = 8;
const DWORD         PSCONST_vParticleDims = 15;
const DWORD         PSCONST_fRootVisParticles = 17;
const DWORD         PSCONST_fCenterFactor = 18;
const DWORD         PSCONST_fStencilFactor = 19;
const DWORD         PSCONST_vPosition = 20;
const DWORD         PSCONST_fRadius = 22;
const DWORD         PSCONST_vColor = 24;

// Global variables assigned to the above shader constants
XMFLOAT2            g_vFlowDims;
XMFLOAT2            g_vInvFlowDims;
FLOAT               g_fScale;
FLOAT               g_fCenterFactor;
FLOAT               g_fStencilFactor;
FLOAT               g_fDissipation;
FLOAT               g_fRootVisParticles;
DWORD               g_dwParticleDim;

#define NUM_INJECTORS 2
XMVECTOR g_vPosition[NUM_INJECTORS];
FLOAT g_fRadius[NUM_INJECTORS];
XMVECTOR g_vStrength[NUM_INJECTORS];
XMFLOAT4 g_vPaintRGB[NUM_INJECTORS];


//--------------------------------------------------------------------------------------
// Name: CFluidFlow()
// Desc: 
//--------------------------------------------------------------------------------------
CFluidFlow::CFluidFlow()
{
    m_pVelocityTexture = NULL;
    m_pDivergenceTexture = NULL;
    m_pDensityTexture = NULL;
    m_pPressureTexture = NULL;
    m_pRepelTexture = NULL;
    m_dwFlowWidth = 0L;
    m_dwFlowHeight = 0L;
    m_bImpulseToProcess = FALSE;
    m_bMassToAdd = FALSE;
    m_dwNumPoissonSteps = 25;
    m_fLongevity = 0.99f;
    m_fTimeStep = 1.0f;
    m_fRadius = 0.05f;
    m_fParticleResPower = 8.0f;

    for( DWORD i = 0; i < NUM_INJECTORS; i++ )
    {
        g_vPaintRGB[i].x = 0.5f;
        g_vPaintRGB[i].y = 0.2f;
        g_vPaintRGB[i].z = 0.0f;
        g_vPaintRGB[i].w = 1.0f;
    }
}


//--------------------------------------------------------------------------------------
// Name: Initialize()
// Desc: 
//--------------------------------------------------------------------------------------
HRESULT CFluidFlow::Initialize( DWORD dwWidth, DWORD dwHeight )
{
    g_dwParticleDim = ( DWORD )sqrtf( ( FLOAT )NUM_PARTICLES );
    m_dwFlowWidth = dwWidth;
    m_dwFlowHeight = dwHeight;
    g_vFlowDims.x = ( FLOAT )m_dwFlowWidth;
    g_vFlowDims.y = ( FLOAT )m_dwFlowHeight;
    g_vInvFlowDims.x = 1.0f / ( FLOAT )m_dwFlowWidth;
    g_vInvFlowDims.y = 1.0f / ( FLOAT )m_dwFlowHeight;

    // Initialize the post-process helper library
    g_PostProcess.Initialize();

    // Determine the texture format
    D3DFORMAT d3dTextureFlowFormat = TEXTUREFLOWFORMAT;

    // Create the shaders
    if( FAILED( ATG::LoadVertexShader( "game:\\Media\\Shaders\\ParticleRepelRender.xvu",
                                       &g_pParticleRepelRenderVS ) ) ) return E_FAIL;
    if( FAILED( ATG::LoadVertexShader( "game:\\Media\\Shaders\\ParticleRender.xvu", &g_pParticleRenderVS ) ) ) return
            E_FAIL;
    if( FAILED( ATG::LoadPixelShader( "game:\\Media\\Shaders\\ParticleInit.xpu", &g_pParticleInitPS ) ) ) return
            E_FAIL;
    if( FAILED( ATG::LoadPixelShader( "game:\\Media\\Shaders\\ParticleAdvect.xpu", &g_pParticleAdvectPS ) ) ) return
            E_FAIL;
    if( FAILED( ATG::LoadPixelShader( "game:\\Media\\Shaders\\ParticleRepel.xpu", &g_pParticleRepelPS ) ) ) return
            E_FAIL;
    if( FAILED( ATG::LoadPixelShader( "game:\\Media\\Shaders\\ParticleRepelRender.xpu",
                                      &g_pParticleRepelRenderPS ) ) ) return E_FAIL;
    if( FAILED( ATG::LoadPixelShader( "game:\\Media\\Shaders\\ParticleRender.xpu", &g_pParticleRenderPS ) ) ) return
            E_FAIL;
    if( FAILED( ATG::LoadPixelShader( "game:\\Media\\Shaders\\FlowAddImpulse.xpu", &g_pFlowAddImpulsePS ) ) ) return
            E_FAIL;
    if( FAILED( ATG::LoadPixelShader( "game:\\Media\\Shaders\\FlowAdvect.xpu", &g_pFlowAdvectPS ) ) ) return E_FAIL;
    if( FAILED( ATG::LoadPixelShader( "game:\\Media\\Shaders\\FlowBCStrip.xpu", &g_pFlowBCStripPS ) ) ) return E_FAIL;
    if( FAILED( ATG::LoadPixelShader( "game:\\Media\\Shaders\\FlowDivergence.xpu", &g_pFlowDivergencePS ) ) ) return
            E_FAIL;
    if( FAILED( ATG::LoadPixelShader( "game:\\Media\\Shaders\\FlowJacobi.xpu", &g_pFlowJacobiPS ) ) ) return E_FAIL;
    if( FAILED( ATG::LoadPixelShader( "game:\\Media\\Shaders\\FlowSubtractGradient.xpu",
                                      &g_pFlowSubtractGradientPS ) ) ) return E_FAIL;
    if( FAILED( ATG::LoadPixelShader( "game:\\Media\\Shaders\\FlowDisplay.xpu", &g_pFlowDisplayPS ) ) ) return E_FAIL;

    // Create the intermediate textures for the fluidflow effect
    ATG::g_pd3dDevice->CreateTexture( m_dwFlowWidth, m_dwFlowHeight, 1, 0, d3dTextureFlowFormat,
                                      D3DPOOL_DEFAULT, &m_pDensityTexture, NULL );
    ATG::g_pd3dDevice->CreateTexture( m_dwFlowWidth, m_dwFlowHeight, 1, 0, d3dTextureFlowFormat,
                                      D3DPOOL_DEFAULT, &m_pVelocityTexture, NULL );
    ATG::g_pd3dDevice->CreateTexture( m_dwFlowWidth, m_dwFlowHeight, 1, 0, d3dTextureFlowFormat,
                                      D3DPOOL_DEFAULT, &m_pDivergenceTexture, NULL );
    ATG::g_pd3dDevice->CreateTexture( m_dwFlowWidth, m_dwFlowHeight, 1, 0, d3dTextureFlowFormat,
                                      D3DPOOL_DEFAULT, &m_pPressureTexture, NULL );
    ATG::g_pd3dDevice->CreateRenderTarget( m_dwFlowWidth, m_dwFlowHeight, RENDERTARGETFLOWFORMAT, D3DMULTISAMPLE_NONE,
                                           0, FALSE, &m_pTextureRT, NULL );

    // Create the textures for the particles
    ATG::g_pd3dDevice->CreateTexture( g_dwParticleDim, g_dwParticleDim, 1, 0, PARTICLEPOSFORMAT,
                                      D3DPOOL_DEFAULT, &m_pRepelTexture, NULL );
    ATG::g_pd3dDevice->CreateTexture( g_dwParticleDim, g_dwParticleDim, 1, 0, PARTICLEPOSFORMAT,
                                      D3DPOOL_DEFAULT, &m_pPositionSourceTextureXY, NULL );
    ATG::g_pd3dDevice->CreateRenderTarget( g_dwParticleDim, g_dwParticleDim, PARTICLEPOSFORMAT, D3DMULTISAMPLE_NONE, 0,
                                           FALSE, &m_pParticleRT, NULL );

    // Create a header for using the m_pPositionSourceTextureXY texture as a vertex buffer
    XGSetVertexBufferHeader( sizeof( XMFLOAT2 ) * NUM_PARTICLES, D3DUSAGE_POINTS, D3DPOOL_DEFAULT,
                             ( m_pPositionSourceTextureXY->Format.BaseAddress << GPU_TEXTURE_ADDRESS_SHIFT ),
                             &m_ParticleVB );

    // Create a vertex declaration for the particle's vertex buffer
    static const D3DVERTEXELEMENT9 declRender[] =
    {
        { 0, 0, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
        D3DDECL_END()
    };
    ATG::g_pd3dDevice->CreateVertexDeclaration( declRender, &m_pParticleVtxDecl );

    Reinit( 0.0f );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Reinit()
// Desc: 
//--------------------------------------------------------------------------------------
VOID CFluidFlow::Reinit( FLOAT res )
{
    m_fParticleResPower += res;
    if( m_fParticleResPower > 8.0f ) m_fParticleResPower = 8.0f;
    if( m_fParticleResPower < 1.0f ) m_fParticleResPower = 1.0f;
    FLOAT fParticleRes = powf( 2.0f, m_fParticleResPower * 2.0f );
    g_fRootVisParticles = sqrtf( fParticleRes );

    // Clear the textures
    g_PostProcess.ClearTexture( m_pPositionSourceTextureXY );
    g_PostProcess.ClearTexture( m_pDensityTexture );
    g_PostProcess.ClearTexture( m_pVelocityTexture );
    g_PostProcess.ClearTexture( m_pDivergenceTexture );
    g_PostProcess.ClearTexture( m_pPressureTexture );
    g_PostProcess.ClearTexture( m_pRepelTexture );

    // Initialize the particle position texture
    ATG::g_pd3dDevice->SetPixelShader( g_pParticleInitPS );
    ATG::g_pd3dDevice->SetPixelShaderConstantF( PSCONST_fRootVisParticles, ( FLOAT* )&g_fRootVisParticles, 1 );
    ATG::g_pd3dDevice->SetTexture( 0, m_pPositionSourceTextureXY );

    ATG::PushRenderTarget( 0, m_pParticleRT );
    RenderQuadToTexture( m_pParticleRT, m_pPositionSourceTextureXY );
    ATG::PopRenderTarget( 0 );
}


//--------------------------------------------------------------------------------------
// Name: Render()
// Desc: 
//--------------------------------------------------------------------------------------
VOID CFluidFlow::Render()
{
    // Render the density to visualize the flow
    ATG::g_pd3dDevice->SetPixelShader( g_pFlowDisplayPS );
    ATG::g_pd3dDevice->SetPixelShaderConstantF( PSCONST_vFlowDims, ( FLOAT* )&g_vFlowDims, 1 );
    ATG::g_pd3dDevice->SetTexture( 0, m_pDensityTexture );
    ATG::g_pd3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
    ATG::g_pd3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
    g_PostProcess.DrawFullScreenQuad();
}


//--------------------------------------------------------------------------------------
// Name: AddImpulse()
// Desc: 
//--------------------------------------------------------------------------------------
VOID CFluidFlow::AddImpulse()
{
    if( m_bImpulseToProcess )
    {
        // Add to the velocity
        ATG::g_pd3dDevice->SetPixelShader( g_pFlowAddImpulsePS );
        for( DWORD i = 0; i < NUM_INJECTORS; i++ )
        {
            ATG::g_pd3dDevice->SetPixelShaderConstantF( PSCONST_vPosition + i, ( FLOAT* )&g_vPosition[i], 1 );
            ATG::g_pd3dDevice->SetPixelShaderConstantF( PSCONST_vColor + i, ( FLOAT* )&g_vStrength[i], 1 );
            ATG::g_pd3dDevice->SetPixelShaderConstantF( PSCONST_fRadius + i, ( FLOAT* )&g_fRadius[i], 1 );
        }
        ATG::g_pd3dDevice->SetTexture( 0, m_pVelocityTexture );
        RenderQuadToTexture( m_pTextureRT, m_pVelocityTexture );

        // Set boundary values
        g_fScale = -1.0f;
        ATG::g_pd3dDevice->SetPixelShader( g_pFlowBCStripPS );
        ATG::g_pd3dDevice->SetPixelShaderConstantF( PSCONST_vFlowDims, ( FLOAT* )&g_vFlowDims, 1 );
        ATG::g_pd3dDevice->SetPixelShaderConstantF( PSCONST_vInvFlowDims, ( FLOAT* )&g_vInvFlowDims, 1 );
        ATG::g_pd3dDevice->SetPixelShaderConstantF( PSCONST_fScale, ( FLOAT* )&g_fScale, 1 );
        ATG::g_pd3dDevice->SetTexture( 0, m_pVelocityTexture );
        RenderQuadToTexture( m_pTextureRT, m_pVelocityTexture );

        m_bImpulseToProcess = FALSE;

        if( m_bMassToAdd )
        {
            // Add to the density
            ATG::g_pd3dDevice->SetPixelShader( g_pFlowAddImpulsePS );
            for( DWORD i = 0; i < NUM_INJECTORS; i++ )
            {
                ATG::g_pd3dDevice->SetPixelShaderConstantF( PSCONST_vPosition + i, ( FLOAT* )&g_vPosition[i], 1 );
                ATG::g_pd3dDevice->SetPixelShaderConstantF( PSCONST_vColor + i, ( FLOAT* )&g_vPaintRGB[i], 1 );
                ATG::g_pd3dDevice->SetPixelShaderConstantF( PSCONST_fRadius + i, ( FLOAT* )&g_fRadius[i], 1 );
            }
            ATG::g_pd3dDevice->SetTexture( 0, m_pDensityTexture );
            RenderQuadToTexture( m_pTextureRT, m_pDensityTexture );

            // Set boundary values
            g_fScale = 1.0f;
            ATG::g_pd3dDevice->SetPixelShader( g_pFlowBCStripPS );
            ATG::g_pd3dDevice->SetPixelShaderConstantF( PSCONST_vFlowDims, ( FLOAT* )&g_vFlowDims, 1 );
            ATG::g_pd3dDevice->SetPixelShaderConstantF( PSCONST_vInvFlowDims, ( FLOAT* )&g_vInvFlowDims, 1 );
            ATG::g_pd3dDevice->SetPixelShaderConstantF( PSCONST_fScale, ( FLOAT* )&g_fScale, 1 );
            ATG::g_pd3dDevice->SetTexture( 0, m_pDensityTexture );
            RenderQuadToTexture( m_pTextureRT, m_pDensityTexture );

            m_bMassToAdd = FALSE;
        }
    }
}


//--------------------------------------------------------------------------------------
// Name: AdvectDensity()
// Desc: 
//--------------------------------------------------------------------------------------
VOID CFluidFlow::AdvectDensity()
{
    // Advect density based on velocity
    g_fDissipation = m_fLongevity;
    ATG::g_pd3dDevice->SetPixelShader( g_pFlowAdvectPS );
    ATG::g_pd3dDevice->SetPixelShaderConstantF( PSCONST_vInvFlowDims, ( FLOAT* )&g_vInvFlowDims, 1 );
    ATG::g_pd3dDevice->SetPixelShaderConstantF( PSCONST_fDissipation, ( FLOAT* )&g_fDissipation, 1 );
    ATG::g_pd3dDevice->SetPixelShaderConstantF( PSCONST_fTimeStep, ( FLOAT* )&m_fTimeStep, 1 );
    ATG::g_pd3dDevice->SetTexture( 0, m_pVelocityTexture );
    ATG::g_pd3dDevice->SetTexture( 1, m_pDensityTexture );
    RenderQuadToTexture( m_pTextureRT, m_pDensityTexture, FALSE );

    // Set boundary values
    g_fScale = 1.0f;
    ATG::g_pd3dDevice->SetPixelShader( g_pFlowBCStripPS );
    ATG::g_pd3dDevice->SetPixelShaderConstantF( PSCONST_vFlowDims, ( FLOAT* )&g_vFlowDims, 1 );
    ATG::g_pd3dDevice->SetPixelShaderConstantF( PSCONST_vInvFlowDims, ( FLOAT* )&g_vInvFlowDims, 1 );
    ATG::g_pd3dDevice->SetPixelShaderConstantF( PSCONST_fScale, ( FLOAT* )&g_fScale, 1 );
    ATG::g_pd3dDevice->SetTexture( 0, m_pDensityTexture );
    RenderQuadToTexture( m_pTextureRT, m_pDensityTexture );
}


//--------------------------------------------------------------------------------------
// Name: AdvectVelocity()
// Desc: 
//--------------------------------------------------------------------------------------
VOID CFluidFlow::AdvectVelocity()
{
    // Advect velocity based on velocity
    g_fDissipation = 1.0f;
    ATG::g_pd3dDevice->SetPixelShader( g_pFlowAdvectPS );
    ATG::g_pd3dDevice->SetPixelShaderConstantF( PSCONST_vInvFlowDims, ( FLOAT* )&g_vInvFlowDims, 1 );
    ATG::g_pd3dDevice->SetPixelShaderConstantF( PSCONST_fDissipation, ( FLOAT* )&g_fDissipation, 1 );
    ATG::g_pd3dDevice->SetPixelShaderConstantF( PSCONST_fTimeStep, ( FLOAT* )&m_fTimeStep, 1 );
    ATG::g_pd3dDevice->SetTexture( 0, m_pVelocityTexture );
    ATG::g_pd3dDevice->SetTexture( 1, m_pVelocityTexture );
    RenderQuadToTexture( m_pTextureRT, m_pVelocityTexture, FALSE );

    // Set boundary values
    g_fScale = -1.0f;
    ATG::g_pd3dDevice->SetPixelShader( g_pFlowBCStripPS );
    ATG::g_pd3dDevice->SetPixelShaderConstantF( PSCONST_vFlowDims, ( FLOAT* )&g_vFlowDims, 1 );
    ATG::g_pd3dDevice->SetPixelShaderConstantF( PSCONST_vInvFlowDims, ( FLOAT* )&g_vInvFlowDims, 1 );
    ATG::g_pd3dDevice->SetPixelShaderConstantF( PSCONST_fScale, ( FLOAT* )&g_fScale, 1 );
    ATG::g_pd3dDevice->SetTexture( 0, m_pVelocityTexture );
    RenderQuadToTexture( m_pTextureRT, m_pVelocityTexture );
}


//--------------------------------------------------------------------------------------
// Name: AdvectParticles()
// Desc: 
//--------------------------------------------------------------------------------------
VOID CFluidFlow::AdvectParticles()
{
    // Advect particles based on velocity
    ATG::g_pd3dDevice->SetPixelShader( g_pParticleAdvectPS );
    ATG::g_pd3dDevice->SetPixelShaderConstantF( PSCONST_fTimeStep, ( FLOAT* )&m_fTimeStep, 1 );
    ATG::g_pd3dDevice->SetPixelShaderConstantF( PSCONST_vInvFlowDims, ( FLOAT* )&g_vInvFlowDims, 1 );
    ATG::g_pd3dDevice->SetTexture( 0, m_pVelocityTexture );
    ATG::g_pd3dDevice->SetTexture( 1, m_pPositionSourceTextureXY );
    ATG::PushRenderTarget( 0, m_pParticleRT );
    RenderQuadToTexture( m_pParticleRT, m_pPositionSourceTextureXY );
    ATG::PopRenderTarget( 0 );
}


//--------------------------------------------------------------------------------------
// Name: RepelParticles()
// Desc: 
//--------------------------------------------------------------------------------------
VOID CFluidFlow::RepelParticles()
{
    // First render the particle sprites to the repel buffer

    // Set the render states for using point sprites
    ATG::g_pd3dDevice->SetRenderState( D3DRS_POINTSPRITEENABLE, TRUE );

    // Create and set a render target for the texture
    ATG::PushRenderTarget( 0, ATG::CreateRenderTarget( m_pRepelTexture ) );

    ATG::g_pd3dDevice->SetVertexShader( g_pParticleRepelRenderVS );
    ATG::g_pd3dDevice->SetPixelShader( g_pParticleRepelRenderPS );

    // Render the m_pPositionSourceTextureXY texture as a point list of particles
    ATG::g_pd3dDevice->SetVertexDeclaration( m_pParticleVtxDecl );
    ATG::g_pd3dDevice->SetStreamSource( 0, &m_ParticleVB, 0, sizeof( XMFLOAT2 ) );
    ATG::g_pd3dDevice->DrawPrimitive( D3DPT_POINTLIST, 0, NUM_PARTICLES );

    ATG::g_pd3dDevice->Resolve( D3DRESOLVE_RENDERTARGET0, NULL, m_pRepelTexture, NULL,
                                0, 0, NULL, 1.0f, 0, NULL );

    // Restore render state
    ATG::PopRenderTarget( 0 )->Release();
    ATG::g_pd3dDevice->SetRenderState( D3DRS_POINTSPRITEENABLE, FALSE );

    // Now that particles are rendered to repel buffer, perform the quad-
    // rendering pass to repel based on contents of buffer
    ATG::g_pd3dDevice->SetPixelShader( g_pParticleRepelPS );
    ATG::g_pd3dDevice->SetTexture( 0, m_pRepelTexture );
    ATG::g_pd3dDevice->SetTexture( 1, m_pPositionSourceTextureXY );
    ATG::PushRenderTarget( 0, m_pParticleRT );
    RenderQuadToTexture( m_pParticleRT, m_pPositionSourceTextureXY );
    ATG::PopRenderTarget( 0 );
}


//--------------------------------------------------------------------------------------
// Name: Project()
// Desc: 
//--------------------------------------------------------------------------------------
VOID CFluidFlow::Project()
{
    // Set common pixel shader constants
    g_fCenterFactor = -1.00f;
    g_fStencilFactor = 0.25f;
    g_fScale = 1.00f;
    ATG::g_pd3dDevice->SetPixelShaderConstantF( PSCONST_vFlowDims, ( FLOAT* )&g_vFlowDims, 1 );
    ATG::g_pd3dDevice->SetPixelShaderConstantF( PSCONST_vInvFlowDims, ( FLOAT* )&g_vInvFlowDims, 1 );
    ATG::g_pd3dDevice->SetPixelShaderConstantF( PSCONST_fScale, ( FLOAT* )&g_fScale, 1 );
    ATG::g_pd3dDevice->SetPixelShaderConstantF( PSCONST_fCenterFactor, ( FLOAT* )&g_fCenterFactor, 1 );
    ATG::g_pd3dDevice->SetPixelShaderConstantF( PSCONST_fStencilFactor, ( FLOAT* )&g_fStencilFactor, 1 );

    // Compute divergence of velocity field
    ATG::g_pd3dDevice->SetPixelShader( g_pFlowDivergencePS );
    ATG::g_pd3dDevice->SetTexture( 0, m_pVelocityTexture );
    RenderQuadToTexture( m_pTextureRT, m_pDivergenceTexture );

    // Poisson steps
    for( DWORD i = 0; i < m_dwNumPoissonSteps; i++ )
    {
        // Set boundary values
        ATG::g_pd3dDevice->SetPixelShader( g_pFlowBCStripPS );
        ATG::g_pd3dDevice->SetTexture( 0, m_pPressureTexture );
        RenderQuadToTexture( m_pTextureRT, m_pPressureTexture );

        // Perform Jacobi integration
        ATG::g_pd3dDevice->SetPixelShader( g_pFlowJacobiPS );
        ATG::g_pd3dDevice->SetTexture( 0, m_pPressureTexture );
        ATG::g_pd3dDevice->SetTexture( 1, m_pDivergenceTexture );
        RenderQuadToTexture( m_pTextureRT, m_pPressureTexture, FALSE );
    }

    // Set boundary values
    ATG::g_pd3dDevice->SetPixelShader( g_pFlowBCStripPS );
    ATG::g_pd3dDevice->SetTexture( 0, m_pPressureTexture );
    RenderQuadToTexture( m_pTextureRT, m_pPressureTexture );

    // Subtract gradient of pressure from velocity
    ATG::g_pd3dDevice->SetPixelShader( g_pFlowSubtractGradientPS );
    ATG::g_pd3dDevice->SetTexture( 0, m_pPressureTexture );
    ATG::g_pd3dDevice->SetTexture( 1, m_pVelocityTexture );
    RenderQuadToTexture( m_pTextureRT, m_pVelocityTexture );
}


//--------------------------------------------------------------------------------------
// Name: Update()
// Desc: 
//--------------------------------------------------------------------------------------
VOID CFluidFlow::Update()
{
    // Set common states for updating the textures
    ATG::PushRenderTarget( 0, m_pTextureRT );
    ATG::g_pd3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_POINT );
    ATG::g_pd3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_POINT );
    ATG::g_pd3dDevice->SetSamplerState( 1, D3DSAMP_MINFILTER, D3DTEXF_POINT );
    ATG::g_pd3dDevice->SetSamplerState( 1, D3DSAMP_MAGFILTER, D3DTEXF_POINT );
    ATG::g_pd3dDevice->SetRenderState( D3DRS_ZENABLE, FALSE );
    ATG::g_pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE, FALSE );
    ATG::g_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );

    // Run the fluid flow simulation
    AddImpulse();
    AdvectDensity();
    AdvectParticles();
    RepelParticles();
    AdvectVelocity();
    Project();

    // Restore state
    ATG::PopRenderTarget( 0 );
}


//--------------------------------------------------------------------------------------
// Name: RenderParticles()
// Desc: 
//--------------------------------------------------------------------------------------
VOID CFluidFlow::RenderParticles()
{
    // Set the render states for rendering the particles using point sprites
    ATG::g_pd3dDevice->SetRenderState( D3DRS_POINTSPRITEENABLE, TRUE );
    ATG::g_pd3dDevice->SetRenderState( D3DRS_ZENABLE, FALSE );
    ATG::g_pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE, FALSE );
    ATG::g_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
    ATG::g_pd3dDevice->SetRenderState( D3DRS_BLENDOP, D3DBLENDOP_ADD );
    ATG::g_pd3dDevice->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
    ATG::g_pd3dDevice->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );
    ATG::g_pd3dDevice->SetVertexShader( g_pParticleRenderVS );
    ATG::g_pd3dDevice->SetPixelShader( g_pParticleRenderPS );

    // Render particles
    ATG::g_pd3dDevice->SetVertexDeclaration( m_pParticleVtxDecl );
    ATG::g_pd3dDevice->SetStreamSource( 0, &m_ParticleVB, 0, sizeof( XMFLOAT2 ) );
    ATG::g_pd3dDevice->DrawPrimitive( D3DPT_POINTLIST, 0, NUM_PARTICLES );

    // Restore state
    ATG::g_pd3dDevice->SetRenderState( D3DRS_POINTSPRITEENABLE, FALSE );
    ATG::g_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
}


//--------------------------------------------------------------------------------------
// Name: RenderQuadToTexture()
// Desc: Render to render target and then resolve to texture
//--------------------------------------------------------------------------------------
VOID CFluidFlow::RenderQuadToTexture( LPDIRECT3DSURFACE9 pRenderTarget,
                                      LPDIRECT3DTEXTURE9 pTexture,
                                      BOOL bRenderBoundaries )
{
    // Set a render target for rendering to the texture
    ATG::g_pd3dDevice->SetRenderTarget( 0, pRenderTarget );

#if 0
    // Shrink the viewport by one pixel if we want to avoid rendering the boundaries
    if( FALSE == bRenderBoundaries )
    {
        D3DVIEWPORT9 vp;
        ATG::g_pd3dDevice->GetViewport( &vp );
        vp.X+=1; vp.Width -=2;
        vp.Y+=1; vp.Height-=2;
        ATG::g_pd3dDevice->SetViewport( &vp );

        XGTEXTURE_DESC Desc;
        XGGetTextureDesc( pTexture, 0, &Desc );
        g_PostProcess.DrawScreenSpaceQuad( (FLOAT)Desc.Width, (FLOAT)Desc.Height );
    }
    else
#endif
    {
        g_PostProcess.DrawFullScreenQuad();
    }

    ATG::g_pd3dDevice->Resolve( D3DRESOLVE_RENDERTARGET0, NULL, pTexture, NULL,
                                0, 0, NULL, 1.0f, 0, NULL );
}


//--------------------------------------------------------------------------------------
// Name: GenerateRandomPos()
// Desc: Generates a random position for the fluid flow injector
//--------------------------------------------------------------------------------------
static XMVECTOR GenerateRandomPos()
{
    XMVECTOR vPos;
    vPos.x = ( rand() % 10000 ) / 20000.0f + 0.25f;
    vPos.y = ( rand() % 10000 ) / 20000.0f + 0.25f;
    return vPos;
}


//--------------------------------------------------------------------------------------
// Name: Inject()
// Desc: 
//--------------------------------------------------------------------------------------
VOID CFluidFlowInjector::Inject( XMVECTOR* pvStrength, XMVECTOR* pvPosition, FLOAT* pfRadius )
{
    if( m_bFirst )
    {
        m_vPos1 = GenerateRandomPos();
        m_vPos2 = GenerateRandomPos();
        m_vPos3 = GenerateRandomPos();
        m_vPos4 = GenerateRandomPos();
    }

    m_fAlpha += m_fAlphaSpeed;
    if( m_fAlpha > 1.0f )
    {
        m_vPos1 = m_vPos4;
        m_vPos2 = 2 * m_vPos1 - m_vPos3;
        m_vPos3 = GenerateRandomPos();
        m_vPos4 = GenerateRandomPos();

        for( DWORD i = 0; i < NUM_INJECTORS; i++ )
        {
            g_vPaintRGB[i].x = ( rand() % 10000 ) / 20000.0f + 0.25f;
            g_vPaintRGB[i].y = ( rand() % 10000 ) / 20000.0f + 0.25f;
            g_vPaintRGB[i].z = ( rand() % 10000 ) / 20000.0f + 0.25f;
        }

        m_fAlpha -= 1.0f;
    }

    XMVECTOR vOldPosition = m_vPosition;

    FLOAT mu = m_fAlpha;
    FLOAT mum1 = 1.0f - mu;
    FLOAT mum13 = mum1 * mum1 * mum1;
    FLOAT mu3 = mu * mu * mu;

    m_vPosition = mum13 * m_vPos1 + 3 * mu * mum1 * mum1 * m_vPos2 + 3 * mu * mu * mum1 * m_vPos3 + mu3 * m_vPos4;

    if( m_bFirst )
    {
        vOldPosition = m_vPosition;
        m_bFirst = FALSE;
    }

    XMVECTOR vDelta = m_vPosition - vOldPosition;

    // Clamp to some range
    pvStrength->x = -min( max( 512 * vDelta.x, -2 ), 2 );
    pvStrength->y = -min( max( 512 * vDelta.y, -2 ), 2 );
    pvStrength->z = 0.0f;
    ( *pvPosition ) = m_vPosition;
    ( *pfRadius ) = m_pFlow->m_fRadius / min( m_pFlow->m_dwFlowWidth, m_pFlow->m_dwFlowHeight );
}


//--------------------------------------------------------------------------------------
// Name: Update()
// Desc: 
//--------------------------------------------------------------------------------------
VOID CFluidFlowWithInjectors::Update()
{
    for( DWORD i = 0; i < NUM_INJECTORS; i++ )
    {
        m_Inject[i].Inject( &g_vStrength[i], &g_vPosition[i], &g_fRadius[i] );
    }

    m_bImpulseToProcess = TRUE;
    m_bMassToAdd = TRUE;

    CFluidFlow::Update();
}
