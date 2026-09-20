//--------------------------------------------------------------------------------------
// HyperFillrateParticles.cpp
//
// XNA Developer Connection
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include <xtl.h>
#include <xgraphics.h>
#include <AtgApp.h>
#include <AtgFont.h>
#include <AtgHelp.h>
#include <AtgInput.h>
#include <AtgUtil.h>
#include <AtgSimpleShaders.h>
#include <AtgDebugDraw.h>
#include <AtgSceneAll.h>

#include "GpuTimer.h"
#include "ParticleSystem.h"

//--------------------------------------------------------------------------------------
// GPU Timing
//--------------------------------------------------------------------------------------
GpuTimer*           GpuTimer::m_pListHead = 0;
GpuTimer            ParticleGPUTimer( "Particle_Render" );
GpuTimer            SceneGPUTimer( "Scene_Render" );

//--------------------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//--------------------------------------------------------------------------------------
ATG::HELP_CALLOUT g_HelpCallouts[] =
{
    { ATG::HELP_BACK_BUTTON,    ATG::HELP_PLACEMENT_2, L"Display\nhelp" },
    { ATG::HELP_X_BUTTON,       ATG::HELP_PLACEMENT_2, L"Toggle Hyper Fillrate Renderer" },
    { ATG::HELP_DPAD,           ATG::HELP_PLACEMENT_2, L"Up/Down = Increase/Decrease Particle Count" },
    { ATG::HELP_LEFTSTICK,      ATG::HELP_PLACEMENT_2, L"Move Camera" },
    { ATG::HELP_RIGHTSTICK,     ATG::HELP_PLACEMENT_2, L"Rotate Camera" },
};
static const DWORD  NUM_HELP_CALLOUTS = _countof( g_HelpCallouts );

//--------------------------------------------------------------------------------------
// Maximum particle count.
//--------------------------------------------------------------------------------------
static const DWORD  g_dwMaxParticleCount = 30000;

//--------------------------------------------------------------------------------------
// World positions of particle systems
//--------------------------------------------------------------------------------------
XMVECTOR g_vParticleSystemPositions[] =
{
    { -1.7f, 4.1f, 19.6f },
    { 16.8f, 4.0f, 19.6f },
    { 14.7f, 2.9f,  7.8f },
    {  0.5f, 3.0f,  3.5f },
    {  6.0f, 0.0f,  1.0f },
};

//--------------------------------------------------------------------------------------
// Particle system render types
//--------------------------------------------------------------------------------------
enum EParticleSystemType
{
    eFire,
    eSmoke
};
EParticleSystemType g_iParticleSystemTypes[] =
{
    { eSmoke },
    { eFire },
    { eFire },
    { eSmoke },
    { eFire },
};
#define NUM_PARTICLE_SYSTEMS _countof(g_vParticleSystemPositions)


//--------------------------------------------------------------------------------------
// Name: class Sample
// Desc: Main class to run this application. Most functionality is inherited
//       from the ATG::Application base class.
//--------------------------------------------------------------------------------------
class Sample : public ATG::Application
{
protected:
    ATG::Timer  m_Timer;
    ATG::Font   m_Font;
    ATG::Help   m_Help;
    BOOL        m_bDrawHelp;

    // Transform matrices
    XMMATRIX    m_matWorld;
    XMMATRIX    m_matView;
    XMMATRIX    m_matProj;

    // Camera
    XMVECTOR    m_vEyePt;
    XMVECTOR    m_vLookatDir;
    XMVECTOR    m_vUpVec;
    FLOAT       m_fTheta;
    FLOAT       m_fPhi;

    // Scene object
    ATG::Scene*             m_pScene;
    IDirect3DVertexShader9* m_pSceneVS;
    IDirect3DPixelShader9*  m_pScenePS;
    IDirect3DPixelShader9*  m_pSkyPS;

    // Timers
    DOUBLE m_fCPUVMXUpdateTime;


    // Heap allocated particle systems
    ParticleSystem* m_ParticleSystems[NUM_PARTICLE_SYSTEMS];
    // Depth sorting necessary for rendering correctness
    DWORD           m_dwRenderOrder[NUM_PARTICLE_SYSTEMS];

    // Resources for the hyperfillrate renderer trick
    IDirect3DSurface9*	m_pScratchRenderTargets;
    IDirect3DSurface9*	m_pScratchRenderTargets4xMSAA;
    D3DFORMAT           m_d3dfmtScratchRenderTargets;
    // Depth buffers for standard and hyperfillrate rendering
    D3DSurface*         m_pDepthStencilNoMSAA,*m_pDepthStencil4XMSAA;
    // Track whether we are currently using hyperfillrate rendering mode
    BOOL                m_bEnableHyperFillrateRendering;
    // Graphics resources
    D3DTexture*         m_pBackgroundTexture;       // Background image
    ATG::PackedResource m_xprResource;              // Packed resources (textures)

    VOID    SetHyperFillrateBuffers( IDirect3DSurface9* pDstTexture, D3DSurface* pDepth );
    VOID    DepthSortParticleSystems();

private:
    virtual HRESULT Initialize();
    virtual HRESULT Update();
    virtual HRESULT Render();
    HRESULT         DrawHUD();
    HRESULT         CreateResources();
};


//--------------------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the program
//--------------------------------------------------------------------------------------
VOID __cdecl main()
{
    Sample atgApp;
    atgApp.m_d3dpp.BackBufferWidth = 1280;
    atgApp.m_d3dpp.BackBufferHeight = 720;

    atgApp.m_d3dpp.BackBufferFormat = ( D3DFORMAT )MAKESRGBFMT( D3DFMT_A8R8G8B8 );
    atgApp.m_d3dpp.FrontBufferFormat = ( D3DFORMAT )MAKESRGBFMT( D3DFMT_LE_X8R8G8B8 );
    atgApp.m_d3dpp.AutoDepthStencilFormat = D3DFMT_D24FS8;

    atgApp.Run();
}


//--------------------------------------------------------------------------------------
// Name: Initialize()
// Desc: Creates all graphics resources and initializes the particle system.
//       
//--------------------------------------------------------------------------------------
HRESULT Sample::Initialize()
{
    // Create the font.
    if( FAILED( m_Font.Create( "d:\\Media\\Fonts\\Arial_16.xpr" ) ) )
    {
        ATG::FatalError( "Couldn't find media file.\n" );
    }
    
    m_bDrawHelp = FALSE;
    m_fCPUVMXUpdateTime = 0.0f;

    // Confine text drawing to the title safe area.
    m_Font.SetWindow( ATG::GetTitleSafeArea() );

    // Create the help.
    if( FAILED( m_Help.Create( "d:\\Media\\Help\\Help.xpr" ) ) )
    {
        ATG::FatalError( "Couldn't find media file.\n" );
    }
    // Build the projection and world matrices.
    m_matWorld = XMMatrixIdentity();

    // Determine the aspect ratio
    FLOAT fAspect = ( FLOAT )m_d3dpp.BackBufferWidth / ( FLOAT )m_d3dpp.BackBufferHeight;

    // Set the transform matrices
    m_vEyePt = XMVectorSet( 0.0f, 1.0f, -5.0f, 0.0f );
    m_vLookatDir = XMVectorSet( 0.0f, 0.0f, 1.0f, 0.0f );
    m_vUpVec = XMVectorSet( 0.0f, 1.0f, 0.0f, 0.0f );
    m_matProj = XMMatrixPerspectiveFovLH( XM_PI / 3, fAspect, 0.1f, 500.0f );
    m_fTheta = 0.0f;
    m_fPhi = 0.0f;

    // Initialize the simple shaders library.
    ATG::SimpleShaders::Initialize( NULL, NULL );

    m_d3dfmtScratchRenderTargets = ( D3DFORMAT )MAKESRGBFMT( D3DFMT_A8B8G8R8 );

    m_pScratchRenderTargets = NULL;
    m_pScratchRenderTargets4xMSAA = NULL;

    HRESULT hr;
    if( FAILED( hr = CreateResources() ) )
    {
        return hr;
    }

    m_bEnableHyperFillrateRendering = TRUE;

    // Create the resources
    if( FAILED( m_xprResource.Create( "d:\\Media\\Resource.xpr" ) ) )
    {
        ATG::FatalError( "Couldn't find media file.\n" );
    }
    // Load the particle image
    m_pBackgroundTexture = m_xprResource.GetTexture( "MatteGray.bmp" );
    if( m_pBackgroundTexture == NULL )
    {
        ATG::FatalError( "Couldn't find media file.\n" );
    }
    for( INT i = 0; i < NUM_PARTICLE_SYSTEMS; i++ )
    {
        m_ParticleSystems[i] = new ParticleSystem();
        m_ParticleSystems[i]->Initialize( g_dwMaxParticleCount, m_pBackgroundTexture, g_iParticleSystemTypes[i] );
    }

    // Create scene object
    m_pScene = new ATG::Scene();
    m_pScene->GetResourceDatabase()->AddBundledResources( &m_xprResource );
    if( FAILED( ATG::SceneFileParser::LoadXATGFile( "d:\\media\\scenes\\SoftParticles.xatg", m_pScene, NULL,
                                                    ATG::XATGLOADER_DONOTINITIALIZEMATERIALS, NULL ) ) )
    {
        ATG::FatalError( "Could not load scene file." );
    }

    // Create vertex shader
    if( FAILED( hr = ATG::LoadVertexShader( "d:\\Media\\Shaders\\SceneVS.xvu", &m_pSceneVS ) ) )
    {
        ATG::FatalError( "Couldn't create SceneVS.xvu\n" );
    }

    // Create vertex shader
    if( FAILED( hr = ATG::LoadVertexShader( "d:\\Media\\Shaders\\SceneVS.xvu", &m_pSceneVS ) ) )
    {
        ATG::FatalError( "Couldn't create SceneVS.xvu\n" );
    }

    // Create pixel shaders
    if( FAILED( hr = ATG::LoadPixelShader( "d:\\Media\\Shaders\\ScenePS.xpu", &m_pScenePS ) ) )
    {
        ATG::FatalError( "Couldn't create ScenePS.xpu\n" );
    }

    if( FAILED( hr = ATG::LoadPixelShader( "d:\\Media\\Shaders\\SkyPS.xpu", &m_pSkyPS ) ) )
    {
        ATG::FatalError( "Couldn't create SkyPS.xpu\n" );
    }

    return S_OK;
}



//--------------------------------------------------------------------------------------
// Name: Update()
// Desc: Called once per frame, the call is the entry point for updating the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Update()
{
    // Get the current time.
    FLOAT fDeltaTime = ( FLOAT )m_Timer.GetElapsedTime();

    // Get the current gamepad state.
    ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput();

    // Toggle help
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
        m_bDrawHelp = !m_bDrawHelp;

    // Change particle softness
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_DOWN )
    {
        for( INT i = 0; i < NUM_PARTICLE_SYSTEMS; i++ )
            m_ParticleSystems[i]->SetSpawnRate( m_ParticleSystems[i]->GetSpawnRate() / 1.2f );
    }

    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_UP )
    {
        for( INT i = 0; i < NUM_PARTICLE_SYSTEMS; i++ )
            m_ParticleSystems[i]->SetSpawnRate( m_ParticleSystems[i]->GetSpawnRate() * 1.2f );
    }

    // Set the view matrix for camera control
    m_fPhi += pGamepad->fX2 * fDeltaTime * 0.3f * XM_PI;
    m_fTheta += pGamepad->fY2 * fDeltaTime * 0.3f * XM_PI;

    m_vLookatDir = XMVectorSet( cosf( m_fTheta ) * sinf( m_fPhi ), sinf( m_fTheta ), cosf( m_fTheta ) * cosf( m_fPhi ),
                                0 );
    XMVECTOR vCrossDir = XMVectorSet( cosf( m_fPhi ), 0, -sinf( m_fPhi ), 0 );

    m_vEyePt += m_vLookatDir * pGamepad->fY1 * fDeltaTime * 2.0f;
    m_vEyePt += vCrossDir * pGamepad->fX1 * fDeltaTime * 2.0f;
    m_matView = XMMatrixLookAtLH( m_vEyePt, m_vEyePt + m_vLookatDir, m_vUpVec );

    ATG::DebugDraw::SetViewProjection( m_matView );

    DepthSortParticleSystems();


    // Update the particle system (VMX optimized)
    PIXBeginNamedEvent( 0, "UpdateParticleSystemsVMX" );
    ATG::Timer CPUTimer;
    CPUTimer.Start();

    for( INT i = 0; i < NUM_PARTICLE_SYSTEMS; i++ )
    {
        m_ParticleSystems[i]->Update( fDeltaTime );
    }
    m_fCPUVMXUpdateTime = CPUTimer.GetElapsedTime();
    PIXEndNamedEvent();

    // The Back button toggles the help screen.
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_X )
    {
        m_bEnableHyperFillrateRendering = !m_bEnableHyperFillrateRendering;
    }

    return S_OK;
}



//--------------------------------------------------------------------------------------
// Name: SetRectScreen()
// Desc: Helper function to convert from relative to absolute screen space coordinates
//--------------------------------------------------------------------------------------
VOID SetRectScreen(
    __out D3DRECT* lprc,
    __in  FLOAT xLeft,
    __in  FLOAT yTop,
    __in  FLOAT xRight,
    __in  FLOAT yBottom )
 {
    lprc->x1 = ( LONG )( xLeft * 1280.f );
    lprc->x2 = ( LONG )( xRight * 1280.f );
    lprc->y1 = ( LONG )( yTop * 720.f );
    lprc->y2 = ( LONG )( yBottom * 720.f );
}

//--------------------------------------------------------------------------------------
// Name: Render()
// Desc: Sets up render states, clears the viewport, and renders the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Render()
{
    // Draw the Scene
    PIXBeginNamedEvent( 0, "DrawMainScene" );
    SceneGPUTimer.StartTiming( ATG::g_pd3dDevice );

    // Clear the viewport
    D3DCOLOR D3D_BLACK = D3DCOLOR_ARGB( 0x00, 0x00, 0x00, 0x00 );
    m_pd3dDevice->Clear( 0L, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL,
                         D3D_BLACK, 0.0f, 0L );

    // Initialize default device states at the start of the frame
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );

    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MIPFILTER, D3DTEXF_POINT );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSU, D3DTADDRESS_MIRROR );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSV, D3DTADDRESS_MIRROR );

    m_pd3dDevice->SetSamplerState( 1, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState( 1, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState( 1, D3DSAMP_MIPFILTER, D3DTEXF_POINT );
    m_pd3dDevice->SetSamplerState( 1, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP );
    m_pd3dDevice->SetSamplerState( 1, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP );

    m_pd3dDevice->SetRenderState( D3DRS_ZFUNC, D3DCMP_GREATEREQUAL );

    // Use an inverted depth buffer where near is one, far is zero.  This results in improved early depth rejection
    // in the Hi-Z buffer because more precision is available closer 1.0 so it makes sense to put objects closer
    // to the near plane there.
    D3DVIEWPORT9 Viewport;
    m_pd3dDevice->GetViewport( &Viewport );
    Viewport.MinZ = 1.0f;
    Viewport.MaxZ = 0.0f;
    m_pd3dDevice->SetViewport( &Viewport );

    // Set the scene shaders
    m_pd3dDevice->SetVertexShader( m_pSceneVS );
    m_pd3dDevice->SetPixelShader( m_pScenePS );

    XMMATRIX matTransViewProj = XMMatrixTranspose( m_matView * m_matProj );
    m_pd3dDevice->SetVertexShaderConstantF( 0, ( FLOAT* )&matTransViewProj, 4 );

    // Lighting vector
    XMVECTOR vLight = XMVectorSet( 0.2, 0.8f, -0.2f, 0.0f );
    m_pd3dDevice->SetVertexShaderConstantF( 4, ( FLOAT* )&vLight, 1 );

    // Set ambient light
    XMVECTOR vAmbient = XMVectorSet( 0.25f, 0.25f, 0.25f, 1.0f );
    m_pd3dDevice->SetPixelShaderConstantF( 0, ( FLOAT* )&vAmbient, 1 );

    // Render the scene
    ATG::NameIndexedCollection::iterator i;
    for( i = m_pScene->GetInstanceList()->begin(); i != m_pScene->GetInstanceList()->end(); i++ )
    {
        // Select models from the object list.
        if( ( *i )->IsDerivedFrom( ATG::Model::TypeID ) )
        {
            ATG::Model* pModel = ( ATG::Model* )( *i );

            // Loop over mesh mappings.
            DWORD dwMeshMappingCount = pModel->GetNumMeshMappings();
            for( DWORD dwMapIndex = 0; dwMapIndex < dwMeshMappingCount; ++dwMapIndex )
            {
                ATG::MeshMapping& mm = pModel->GetMeshMapping( dwMapIndex );
                ATG::BaseMesh* pMesh = mm.pMesh;

                // Set pixel sky pixel shader if rendering the sky dome
                if( wcscmp( pMesh->GetName().GetSafeString(), L"sky_shpereShape" ) == 0 )
                {
                    m_pd3dDevice->SetPixelShader( m_pSkyPS );
                }
                else
                {
                    m_pd3dDevice->SetPixelShader( m_pScenePS );
                }

                // Loop over mesh subsets.
                DWORD dwSubsetCount = pMesh->GetNumSubsets();
                for( DWORD dwSubsetIndex = 0; dwSubsetIndex < dwSubsetCount; ++dwSubsetIndex )
                {
                    ATG::MaterialInstance* pMaterial = mm.Materials[ dwSubsetIndex ];

                    // Retrieve diffuse texture and set it
                    ATG::MaterialParameter& param = pMaterial->GetRawParameter( 0 );
                    if( param.pValue != NULL )
                    {
                        ATG::Texture2D* pTex2D = ( ATG::Texture2D* )param.pValue;
                        m_pd3dDevice->SetTexture( 0, pTex2D->GetD3DTexture() );
                    }

                    // Render the mesh subset.
                    pMesh->RenderSubset( dwSubsetIndex, m_pd3dDevice );
                }
            }
        }
    }

    SceneGPUTimer.StopTiming( ATG::g_pd3dDevice );
    PIXEndNamedEvent();
    
    // Draw the particle systems.
    PIXBeginNamedEvent( 0, "DrawParticleSystems" );

    // First alias new depth and color buffer if using hyperfillrate rendering mode
    if( m_bEnableHyperFillrateRendering )
    {
        SetHyperFillrateBuffers( m_pScratchRenderTargets4xMSAA, m_pDepthStencil4XMSAA );
        
        // D3D restores the viewport to defaults and overrides any values that are set by the title if 
        // SetRenderTarget is called.  If using an inverted depth buffer, be sure to restore viewport.
        m_pd3dDevice->GetViewport( &Viewport );
        Viewport.MinZ = 1.0f;
        Viewport.MaxZ = 0.0f;
        m_pd3dDevice->SetViewport( &Viewport );
    }

    ParticleGPUTimer.StartTiming( ATG::g_pd3dDevice );

    XMMATRIX world = m_matWorld;
    INT c;
    for( c = 0; c < NUM_PARTICLE_SYSTEMS; c++ )
    {
        INT iDrawIndex = m_dwRenderOrder[c];
        world._41 = g_vParticleSystemPositions[iDrawIndex].x;
        world._42 = g_vParticleSystemPositions[iDrawIndex].y;
        world._43 = g_vParticleSystemPositions[iDrawIndex].z;

        m_ParticleSystems[iDrawIndex]->Render( world, m_matView, m_matProj );
    }

    ParticleGPUTimer.StopTiming( ATG::g_pd3dDevice );

    // Restore to standard rendering
    if( m_bEnableHyperFillrateRendering )
        SetHyperFillrateBuffers( m_pScratchRenderTargets, m_pDepthStencilNoMSAA );

    PIXEndNamedEvent();

    // Draw HUD now
    HRESULT hr = DrawHUD();

    // Present the scene.
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return hr;
}


//--------------------------------------------------------------------------------------
// Name: DrawHUD()
// Desc: Draws HUD, help, overlay text
//--------------------------------------------------------------------------------------
HRESULT Sample::DrawHUD()
{
    // Show title, frame rate, and help.
    m_Timer.MarkFrame();
    if( m_bDrawHelp )
    {
        m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
        m_Font.Begin();
    }
    else
    {
        m_Font.Begin();
        m_Font.SetScaleFactors( 1.2f, 1.2f );
        m_Font.DrawText( 0, 0, 0xffffffff, L"Hyper Fillrate Particle Renderer" );

        if( !m_bEnableHyperFillrateRendering )
            m_Font.DrawText( 0, 30, 0xffff0000, L" - Render Mode: Standard  - " );
        else
            m_Font.DrawText( 0, 30, 0xffff0000, L" - Render Mode: Hyper Fillrate - " );

        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( 0, 0, 0xffffff00, m_Timer.GetFrameRate(), ATGFONT_RIGHT );
        m_Font.SetScaleFactors( 0.8f, 0.8f );
        WCHAR strText[100];

        // count particles
        INT iGlobalParticlesCount = 0, c;
        for( c = 0; c < NUM_PARTICLE_SYSTEMS; c++ )
        {
            iGlobalParticlesCount += m_ParticleSystems[c]->GetActiveCount();
        }

        swprintf_s( strText, L"Particles: %lu", iGlobalParticlesCount );
        m_Font.DrawText( 0, 30, 0xff00ffff, strText, ATGFONT_RIGHT );

        swprintf_s( strText, L"Particle CPU Time: %0.2f ms", m_fCPUVMXUpdateTime * 1000.0f );
        m_Font.DrawText( 0, 50, 0xff00ffff, strText, ATGFONT_RIGHT );
    }

    m_Font.SetWindow( 0, 0, 0, 0 );

    // Draw GPU stats
    #define TEXT_X 128.0f
    #define TEXT_Y 500.0f
    #define TEXT_SPACING 20.0f
    #define RECT_WID 18.0f

    const D3DCOLOR GRAPH_SCENE_COLOR = 0xc000FF00;
    const D3DCOLOR GRAPH_PARTICLE_COLOR = 0xc00000ff;

    const FLOAT fGPUParticleTime = ParticleGPUTimer.GetTime( m_pd3dDevice );
    const FLOAT fGPUSceneTime = SceneGPUTimer.GetTime( m_pd3dDevice );

    m_Font.SetScaleFactors( 0.8f, 0.8f );
    WCHAR strText[100];
    swprintf_s( strText, L"Scene Draw Cost: %0.3fms", fGPUSceneTime );
    m_Font.DrawText( TEXT_X, TEXT_Y, 0xffdfdfdf, strText );
    swprintf_s( strText, L"Particle Draw Cost: %0.3fms", fGPUParticleTime );
    m_Font.DrawText( TEXT_X, TEXT_Y + TEXT_SPACING, 0xffdfdfdf, strText );

    // Draw colored rectangles next to the text to identify with the graph
    D3DRECT rcColorRect;
    rcColorRect.x1 = ( LONG )( TEXT_X - RECT_WID - 2 );
    rcColorRect.y1 = ( LONG )TEXT_Y;
    rcColorRect.x2 = ( LONG )( rcColorRect.x1 + RECT_WID );
    rcColorRect.y2 = ( LONG )( TEXT_Y + RECT_WID );
    // Draw Particle rectangle
    ATG::DebugDraw::DrawScreenSpaceRect( rcColorRect, 0.f, GRAPH_SCENE_COLOR );

    // Draw scene rectangle
    rcColorRect.y1 += ( LONG )TEXT_SPACING;
    rcColorRect.y2 += ( LONG )TEXT_SPACING;
    ATG::DebugDraw::DrawScreenSpaceRect( rcColorRect, 0.f, GRAPH_PARTICLE_COLOR );
    m_Font.End();

    // Draw the bar graph

    // Define some charactersitics about graph - all units are in screen %.
    #define BAR_WIDTH 0.6f
    #define BAR_HEIGHT 0.05f
    #define BAR_Y 0.8f
    #define BAR_X (1.0f - BAR_WIDTH) / 2.0f
    #define BAR_MS_TIMED (1000.0f / 60.0f)

    // Set up the "bar" for particles
    FLOAT fDrawOffset = 0.f;
    FLOAT fDrawWidth = 0.f;
    D3DRECT rcOutline, rcGraphParticles, rcGraphScene;
    fDrawWidth = BAR_WIDTH * fGPUSceneTime / BAR_MS_TIMED;
    SetRectScreen( &rcGraphScene, BAR_X, BAR_Y, BAR_X + fDrawWidth, BAR_Y + BAR_HEIGHT );

    // Set up the bar for the scene
    fDrawOffset += fDrawWidth;
    fDrawWidth = BAR_WIDTH * fGPUParticleTime / BAR_MS_TIMED;
    SetRectScreen( &rcGraphParticles, BAR_X + fDrawOffset, BAR_Y, BAR_X + fDrawOffset + fDrawWidth,
                   BAR_Y + BAR_HEIGHT );

    // Set up the frame
    SetRectScreen( &rcOutline, BAR_X, BAR_Y, BAR_X + BAR_WIDTH, BAR_Y + BAR_HEIGHT );

    m_Font.Begin();

    // Draw "GPU" text to left of graph
    m_Font.SetScaleFactors( 1.5f, 1.5f );
    m_Font.DrawText( ( FLOAT )( rcOutline.x1 ) - 80.f, ( FLOAT )( rcOutline.y1 ), 0xffdfdfdf, L"GPU" );

    // Draw "MS" text to right of graph
    m_Font.SetScaleFactors( 1.0f, 1.0f );
    m_Font.DrawText( ( FLOAT )rcOutline.x2 - 30.f, ( FLOAT )rcOutline.y2 + 10.0f, 0xffdfdfdf, L"16.7ms" );

    // Draw the inside of the bar
    const D3DCOLOR GRAPH_BORDER_COLOR = 0xc0aa0020;

    // Draw graphs
    ATG::DebugDraw::DrawScreenSpaceRect( rcGraphParticles, 0.f, GRAPH_PARTICLE_COLOR );
    ATG::DebugDraw::DrawScreenSpaceRect( rcGraphScene, 0.f, GRAPH_SCENE_COLOR );
    // Draw the graph frame
    ATG::DebugDraw::DrawScreenSpaceRect( rcOutline, 3.f, GRAPH_BORDER_COLOR );

    // Restore font
    m_Font.End();
    m_Font.SetWindow( ATG::GetTitleSafeArea() );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: CreateResources()
// Desc: Create/Recreate the D3D resources for all rendering modes.
//--------------------------------------------------------------------------------------
HRESULT Sample::CreateResources()
{
    // Create references to our color buffers, then depth buffers. Don't actually 
    // allocate any memory here.  Just alias what already exista.

    // Create the render target at base address 0
    D3DSURFACE_PARAMETERS SurfaceParameters =
    {
        0
    };

    SurfaceParameters.HierarchicalZBase = 0;
    SurfaceParameters.HiZFunc = D3DHIZFUNC_DEFAULT;

    HRESULT hr;

    if( m_pScratchRenderTargets != NULL )
        m_pScratchRenderTargets->Release();

    hr = m_pd3dDevice->CreateRenderTarget( m_d3dpp.BackBufferWidth, m_d3dpp.BackBufferHeight,
                                           m_d3dfmtScratchRenderTargets, D3DMULTISAMPLE_NONE, 0,
                                           FALSE, &m_pScratchRenderTargets,
                                           &SurfaceParameters );
    if( FAILED( hr ) )
        return E_FAIL;

    if( m_pScratchRenderTargets4xMSAA != NULL )
        m_pScratchRenderTargets4xMSAA->Release();

    hr = m_pd3dDevice->CreateRenderTarget( m_d3dpp.BackBufferWidth / 2, m_d3dpp.BackBufferHeight / 2,
                                           m_d3dfmtScratchRenderTargets, D3DMULTISAMPLE_4_SAMPLES, 0,
                                           FALSE, &m_pScratchRenderTargets4xMSAA,
                                           &SurfaceParameters );

    if( FAILED( hr ) )
        return E_FAIL;

    // Also create a 640x480 depth buffer so we can alias the MSAA target
    // Set base address to where our Depth Buffer is stored in memory
    SurfaceParameters.Base = XGSurfaceSize( m_d3dpp.BackBufferWidth, m_d3dpp.BackBufferHeight,
                                            D3DFMT_A8R8G8B8, D3DMULTISAMPLE_NONE );

    hr = m_pd3dDevice->CreateDepthStencilSurface( m_d3dpp.BackBufferWidth / 2, m_d3dpp.BackBufferHeight / 2,
                                                  D3DFMT_D24FS8, D3DMULTISAMPLE_4_SAMPLES,
                                                  0, FALSE, &m_pDepthStencil4XMSAA, &SurfaceParameters );

    if( FAILED( hr ) )
        return E_FAIL;

    hr = m_pd3dDevice->CreateDepthStencilSurface( m_d3dpp.BackBufferWidth, m_d3dpp.BackBufferHeight,
                                                  D3DFMT_D24FS8, D3DMULTISAMPLE_NONE,
                                                  0, FALSE, &m_pDepthStencilNoMSAA, &SurfaceParameters );

    if( FAILED( hr ) )
        return E_FAIL;


    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: SetHyperFillrateBuffers()
// Desc: Aliases the back and depth buffers for different rendering modes.
//--------------------------------------------------------------------------------------
VOID Sample::SetHyperFillrateBuffers( IDirect3DSurface9* pDstTexture, D3DSurface* pDepth )
{
    // Make sure that the required resources exist
    assert( pDstTexture );

    // Set all the right D3D state and resources
    m_pd3dDevice->SetRenderTarget( 0L, pDstTexture );
    m_pd3dDevice->SetDepthStencilSurface( pDepth );
}



//--------------------------------------------------------------------------------------
// Name: ParticleDepthSort()
// Desc: Sort particle systems by depth. This is not optimized for speed, but written for
// clarity.  Use a more efficient sort algorithm.
//--------------------------------------------------------------------------------------
VOID Sample::DepthSortParticleSystems()
{
    FLOAT fDepths[NUM_PARTICLE_SYSTEMS];

    for( DWORD i = 0; i < NUM_PARTICLE_SYSTEMS; ++i )
    {
        m_dwRenderOrder[i] = i;
        XMVECTOR vPos = XMLoadVector3( &g_vParticleSystemPositions[i] );

        fDepths[i] = XMVector3Dot( m_vLookatDir, vPos - m_vEyePt ).x;
    }

    // A simple bubble sort is sufficient because the list is so small.
    for( DWORD i = 0; i < NUM_PARTICLE_SYSTEMS; ++i )
    {
        for( DWORD j = i + 1; j < NUM_PARTICLE_SYSTEMS; ++j )
        {
            if( fDepths[i] < fDepths[j] )
            {
                FLOAT fTemp;
                fTemp = fDepths[i];
                fDepths[i] = fDepths[j];
                fDepths[j] = fTemp;

                DWORD dwTemp;
                dwTemp = m_dwRenderOrder[i];
                m_dwRenderOrder[i] = m_dwRenderOrder[j];
                m_dwRenderOrder[j] = dwTemp;
            }
        }
    }
}
