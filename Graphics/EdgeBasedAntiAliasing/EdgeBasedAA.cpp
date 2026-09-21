//--------------------------------------------------------------------------------------
// EdgeBasedAA.cpp
//
// Demonstrates a method of performing antialiasing by redrawing mesh edges.
//
// Most aliasing is attributable to two types of edges, edges on the silhouette of the 
// mesh, and edges on the interior of the mesh that exhibit a shading discontinuity.  
// The method outlined in this sample determines the silhouette edges each frame on the 
// CPU.  This is similar to the silhouette determination used in creating shadow volumes.  
// Interior edges that need to be antialiased are determined when the mesh is loaded.
//
// Each of the edges requiring antialiasing are redrawn into the frame buffer as 
// blurred edges.  The amount of antialiasing to perform is determined per pixel by the 
// percentage of the pixel that is covered by the polygon edge.
//
// XNA Developer Connection
// Copyright (C) Microsoft Corporation. All rights reserved
//--------------------------------------------------------------------------------------

#include <xtl.h>
#include <xgraphics.h>
#include <xboxmath.h>
#include <AtgApp.h>
#include <AtgFont.h>
#include <AtgHelp.h>
#include <AtgInput.h>
#include <AtgMesh.h>
#include <AtgResource.h>
#include <AtgUtil.h>
#include "AAMesh.h"


#if defined( NDEBUG ) && ( !defined( PROFILE ) || defined( FASTCAP ) )
#define _RELEASED3D
#endif

//--------------------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//--------------------------------------------------------------------------------------
ATG::HELP_CALLOUT g_HelpCallouts[] =
{
    { ATG::HELP_LEFTSTICK,    ATG::HELP_PLACEMENT_1, L"Rotate mesh" },
    { ATG::HELP_A_BUTTON,     ATG::HELP_PLACEMENT_1, L"Previous antialiasing method" },
    { ATG::HELP_B_BUTTON,     ATG::HELP_PLACEMENT_1, L"Next antialiasing method" },
    { ATG::HELP_Y_BUTTON,     ATG::HELP_PLACEMENT_1, L"Toggle texturing" },
    { ATG::HELP_BACK_BUTTON,  ATG::HELP_PLACEMENT_1, L"Display help" },
};
#define NUM_HELP_CALLOUTS (sizeof(g_HelpCallouts)/sizeof(g_HelpCallouts[0]))

const XMVECTOR    g_vLightDir = { 0.0f, .707f, -.707f, 0.0f};

const DWORD VSCONT_matWorldViewProj = 0;  // World-view-projection matrix
const DWORD VSCONT_vLightDir = 4;         // Light direction
const DWORD VSCONT_vScreenSize = 9;       // ScreenSize
const DWORD PSCONT_vDiffuseColor = 8;     // Diffuse color
const DWORD PSCONT_vScreenSize = 9;       // ScreenSize

DWORD g_dwScenarioID = 0;
const DWORD g_dwNumScenarios = 4;

struct AA_SCENARIO  // Describes parameters for a type of anti-aliasing
{
    const WCHAR* strScenarioName;
    DWORD dwScreenWidth;
    DWORD dwScreenHeight;
    D3DMULTISAMPLE_TYPE MSAAType;
    DWORD dwTileCount;
    D3DRECT TilingRects[15];
};

const AA_SCENARIO g_AAScenarios[] =
{
    {
        L"No Anti-Aliasing",
        1280,
        720,
        D3DMULTISAMPLE_NONE,
        1,
        {
            { 0,   0, 1280, 720 },
        }
    },
    {
        L"Edge-based anti-aliasing",
        1280,
        720,
        D3DMULTISAMPLE_NONE,
        1,
        {
            { 0,   0, 1280, 720 },
        }
    },
    {
        L"2x MSAA",
        1280,
        720,
        D3DMULTISAMPLE_2_SAMPLES,
        2,
        {
            { 0,   0, 1280, 384 },
            { 0, 384, 1280, 720 }
        }
    },
    {
        L"4x MSAA",
        1280,
        720,
        D3DMULTISAMPLE_4_SAMPLES,
        3,
        {
            { 0,   0, 1280, 256 },
            { 0, 256, 1280, 512 },
            { 0, 512, 1280, 720 },
        }
    },
    { NULL }
};

//--------------------------------------------------------------------------------------
// Name: LargestTileRectSize
// Desc: Determines the largest tile rectangle that can be used for the given tiling
//       scenario
//--------------------------------------------------------------------------------------
VOID LargestTileRectSize( const AA_SCENARIO& Scenario, D3DPOINT* pMaxSize )
{
    pMaxSize->x = 0;
    pMaxSize->y = 0;
    for( DWORD i = 0; i < Scenario.dwTileCount; i++ )
    {
        DWORD dwWidth = Scenario.TilingRects[i].x2 - Scenario.TilingRects[i].x1;
        DWORD dwHeight = Scenario.TilingRects[i].y2 - Scenario.TilingRects[i].y1;
        if( dwWidth > ( DWORD )pMaxSize->x )
            pMaxSize->x = dwWidth;
        if( dwHeight > ( DWORD )pMaxSize->y )
            pMaxSize->y = dwHeight;
    }
}

//--------------------------------------------------------------------------------------
// Name: class CMesh
// Desc: Derived from ATG::Mesh class to support selecting an appropriate pixel shader
//       based on the current subset's material settings, and to correctly support skinning
//--------------------------------------------------------------------------------------
class CMesh : public ATG::Mesh
{
public:
    LPDIRECT3DDEVICE9 m_pd3dDevice;
    LPDIRECT3DPIXELSHADER9 m_pTexturedPS;
    LPDIRECT3DPIXELSHADER9 m_pNonTexturedPS;

    XMMATRIX m_matWorld;
    XMMATRIX m_matViewProj;
    XMVECTOR m_vWorldLightDirection;

public:
    virtual VOID    RenderMeshCallback( DWORD dwFrame, const ATG::MESH_FRAME* pFrame, DWORD dwFlags )
    {
        XMMATRIX matWorld = pFrame->m_matTransform * m_matWorld;

        // ((M^-1)^T)^-1 = M^T
        XMMATRIX matWorldTranspose = XMMatrixTranspose( matWorld );

        XMVECTOR vLocalLightDir;
        vLocalLightDir = XMVector3TransformNormal( m_vWorldLightDirection, matWorldTranspose );
        vLocalLightDir = XMVector3Normalize( vLocalLightDir );

        m_pd3dDevice->SetVertexShaderConstantF( VSCONT_vLightDir, ( FLOAT* )&vLocalLightDir, 1 );

        XMMATRIX matWVP = matWorld * m_matViewProj;
        matWVP = XMMatrixTranspose( matWVP );

        m_pd3dDevice->SetVertexShaderConstantF( VSCONT_matWorldViewProj, ( FLOAT* )&matWVP, 4 );
    }

    virtual BOOL    RenderCallback( DWORD dwSubset, const ATG::MESH_SUBSET* pSubset, DWORD dwFlags )
    {
        if( pSubset->pTexture != NULL )
        {
            m_pd3dDevice->SetPixelShader( m_pTexturedPS );
        }
        else
        {
            m_pd3dDevice->SetPixelShader( m_pNonTexturedPS );
            m_pd3dDevice->SetPixelShaderConstantF( PSCONT_vDiffuseColor, ( FLOAT* )&pSubset->mtrl.Diffuse, 1 );
        }

        return TRUE;
    }
};


//--------------------------------------------------------------------------------------
// Name: class Sample
// Desc: Main class to run this application
//--------------------------------------------------------------------------------------
class Sample : public ATG::Application
{
private:
    HRESULT Initialize();
    VOID    ResetDevice();
    HRESULT Update();
    HRESULT Render();
    HRESULT RenderAA();

private:
    ATG::Timer m_Timer;
    ATG::PackedResource m_xprResource;   // Packed resources for the app
    ATG::Font m_Font_Large;              // Large font
    ATG::Font m_Font_Small;              // Small font
    ATG::Help m_Help;                    // Help class
    BOOL m_bDrawHelp;                    // Whether to draw help

    CMesh m_Object;                      // Object to render and anti-alias
    XMMATRIX m_matWorld;

    CAAMesh m_EdgeAAMesh;                // Edge AA object

    D3DVertexShader* m_pMeshVS;

    D3DPixelShader*  m_pTexModDiffusePS;
    D3DPixelShader*  m_pDiffuseOnlyPS;

    D3DSurface* m_pRenderTarget;
    D3DSurface* m_pDepthStencil;
    D3DTexture* m_pFrontBufferTexture;

    DWORD m_dwFrontBufferSizeBytes;
    DWORD m_dwTileTargetSizeBytes;
    DWORD m_dwTileDepthStencilSizeBytes;

    XMMATRIX    m_matView;
    XMMATRIX    m_matProj;

    FLOAT m_fUpdateTime;

    D3DPerfCounters* m_pPerfCounterStart[3];
    D3DPerfCounters* m_pPerfCounterEnd[3];

    // elapsed frames
    DWORD m_dwFrameCount;
};


//--------------------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the program.
//--------------------------------------------------------------------------------------
INT __cdecl main()
{
    Sample atgApp;

    // Use 1280x720 resolution.  The hardware scaler will handle other output resolutions.
    atgApp.m_d3dpp.BackBufferWidth = 1280;
    atgApp.m_d3dpp.BackBufferHeight = 720;

    atgApp.m_d3dpp.MultiSampleType = D3DMULTISAMPLE_NONE;
    atgApp.m_dwDeviceCreationFlags = D3DCREATE_BUFFER_2_FRAMES;
    atgApp.m_d3dpp.FrontBufferFormat = ( D3DFORMAT )MAKESRGBFMT( D3DFMT_LE_X8R8G8B8 );
    atgApp.m_d3dpp.BackBufferFormat =  ( D3DFORMAT )MAKESRGBFMT( D3DFMT_LE_X8R8G8B8 );

    atgApp.Run();
}


//--------------------------------------------------------------------------------------
// Name: Initialize()
// Desc: Initializes the app
//--------------------------------------------------------------------------------------
HRESULT Sample::Initialize()
{
    m_dwFrameCount = 0;
    m_bDrawHelp = FALSE;
    
    m_pFrontBufferTexture = NULL;
    m_pRenderTarget = NULL;
    m_pDepthStencil = NULL;
    m_dwFrontBufferSizeBytes = 0;
    m_dwTileTargetSizeBytes = 0;
    m_dwTileDepthStencilSizeBytes = 0;

    // Create the font
    if( FAILED( m_Font_Large.Create( "game:\\Media\\Fonts\\Arial_20.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Confine text drawing to the title safe area
    m_Font_Large.SetWindow( ATG::GetTitleSafeArea() );

    // Create the font
    if( FAILED( m_Font_Small.Create( "game:\\Media\\Fonts\\Arial_16.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Confine text drawing to the title safe area
    m_Font_Small.SetWindow( ATG::GetTitleSafeArea() );

    // Create the help
    if( FAILED( m_Help.Create( "game:\\Media\\Help\\Help.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Create the resource
    if( FAILED( m_xprResource.Create( "game:\\Media\\Resource.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Load an object to anti-alias
    if( FAILED (m_Object.Create( "game:\\Media\\Meshes\\dwarf.xbg", &m_xprResource )))
        return ATGAPPERR_MEDIANOTFOUND;

    // Generate data for antialiasing
    if( FAILED( m_EdgeAAMesh.Create( &m_Object ) ) )
        return E_FAIL;

    static const D3DVERTEXELEMENT9 AADecl[] =
    {
       { 0,  0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
       { 0, 12, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 1 },
       D3DDECL_END()
    };
    m_pd3dDevice->CreateVertexDeclaration( AADecl, &m_EdgeAAMesh.m_pAAVertexDecl );
    m_pd3dDevice->CreateTexture( m_d3dpp.BackBufferWidth, m_d3dpp.BackBufferHeight, 0, 0, m_d3dpp.BackBufferFormat,
                                 0, &m_EdgeAAMesh.m_pFrameBuffer, NULL );

    // Determine the aspect ratio
    FLOAT fAspectRatio = ( FLOAT )m_d3dpp.BackBufferWidth / ( FLOAT )m_d3dpp.BackBufferHeight;

    // Set the transform matrices
    XMVECTOR vEyePt = XMVectorSet( 0.0f, 0.0f, -20.0f, 0.0f );
    XMVECTOR vLookatPt = XMVectorSet( 0.0f, 1.0f, 0.0f, 0.0f );
    XMVECTOR vUpVec = XMVectorSet( 0.0f, 1.0f, 0.0f, 0.0f );
    m_matView = XMMatrixLookAtLH( vEyePt, vLookatPt, vUpVec );
    const FLOAT fViewWidth = 5.0f;
    const FLOAT fViewHeight = 5.0f / fAspectRatio;
    m_matProj = XMMatrixOrthographicLH( fViewWidth, fViewHeight, 1.0f, 100.0f );

    // Load shaders for rendering meshes and visualizations
    ATG::LoadVertexShader( "game:\\Media\\Shaders\\MeshVS.xvu", &m_pMeshVS );
    ATG::LoadVertexShader( "game:\\Media\\Shaders\\EdgeAA_VS.xvu", &m_EdgeAAMesh.m_pVS );
    ATG::LoadPixelShader( "game:\\Media\\Shaders\\TexModDiffusePS.xpu", &m_pTexModDiffusePS );
    ATG::LoadPixelShader( "game:\\Media\\Shaders\\DiffuseOnlyPS.xpu", &m_pDiffuseOnlyPS );
    ATG::LoadPixelShader( "game:\\Media\\Shaders\\EdgeAA_PS.xpu", &m_EdgeAAMesh.m_pPS );
    
    m_Object.m_pd3dDevice = m_pd3dDevice;
    m_Object.m_pTexturedPS = m_pTexModDiffusePS;
    m_Object.m_pNonTexturedPS = m_pDiffuseOnlyPS;

    ResetDevice();

#ifndef _RELEASED3D
    // Set up GPU performance counter structures
    for( DWORD i = 0; i < 3; i++ )
    {
        m_pd3dDevice->CreatePerfCounters( &m_pPerfCounterStart[ i ], 1 );
        m_pd3dDevice->CreatePerfCounters( &m_pPerfCounterEnd[ i ], 1 );
    }

    m_pd3dDevice->EnablePerfCounters( TRUE );

    // Enable the performance counters we want
    D3DPERFCOUNTER_EVENTS PerfEvents;
    ZeroMemory( &PerfEvents, sizeof( D3DPERFCOUNTER_EVENTS ) );
    PerfEvents.CP[0] = GPUPE_CP_COUNT;      // Command Processor clock cycles

    m_pd3dDevice->SetPerfCounterEvents( &PerfEvents, 0 );
#endif

    return S_OK;
}


VOID Sample::ResetDevice()
{
    HRESULT hr;

    // Clean out the D3D device before destroying any resources.  D3D will
    // assert if a resource is destroyed while still selected into a D3D
    // device.
    m_pd3dDevice->UnsetAll();

    // Destroy resources
    SAFE_RELEASE( m_pRenderTarget );
    SAFE_RELEASE( m_pDepthStencil );

    // Destroy frontbuffer
    SAFE_RELEASE( m_pFrontBufferTexture );

    DWORD dwFrontBufferWidth = m_d3dpp.BackBufferWidth;
    DWORD dwFrontBufferHeight = m_d3dpp.BackBufferHeight;

    // Set viewport
    D3DVIEWPORT9 Viewport;
    Viewport.Width = dwFrontBufferWidth;
    Viewport.Height = dwFrontBufferHeight;
    Viewport.X = 0;
    Viewport.Y = 0;
    Viewport.MinZ = 0.0f;
    Viewport.MaxZ = 1.0f;
    m_pd3dDevice->SetViewport( &Viewport );

    // Set up tiling front buffer texture
    m_pd3dDevice->CreateTexture( dwFrontBufferWidth,
                                 dwFrontBufferHeight,
                                 1, 0,
                                 ( D3DFORMAT )MAKESRGBFMT( D3DFMT_LE_X8R8G8B8 ),
                                 D3DPOOL_DEFAULT,
                                 &m_pFrontBufferTexture,
                                 NULL );

    if( NULL == m_pFrontBufferTexture )
    {
        ATG::FatalError( "Could not create front buffer." );
    }

    // Record the size of the frontbuffer
    m_dwFrontBufferSizeBytes = dwFrontBufferWidth * dwFrontBufferHeight * sizeof( DWORD );

    // Find largest tiling rect size
    D3DPOINT LargestTileSize;
    const AA_SCENARIO& CurrentScenario = g_AAScenarios[g_dwScenarioID];
    LargestTileRectSize( CurrentScenario, &LargestTileSize );

    // Create color and depth/stencil rendertargets.
    DWORD dwTileWidth = 0;
    DWORD dwTileHeight = 0;
    switch( CurrentScenario.MSAAType )
    {
        case D3DMULTISAMPLE_NONE:
            dwTileWidth = XGNextMultiple( LargestTileSize.x, GPU_EDRAM_TILE_WIDTH_1X );
            dwTileHeight = XGNextMultiple( LargestTileSize.y, GPU_EDRAM_TILE_HEIGHT_1X );
            break;
        case D3DMULTISAMPLE_2_SAMPLES:
            dwTileWidth = XGNextMultiple( LargestTileSize.x, GPU_EDRAM_TILE_WIDTH_2X );
            dwTileHeight = XGNextMultiple( LargestTileSize.y, GPU_EDRAM_TILE_HEIGHT_2X );
            break;
        case D3DMULTISAMPLE_4_SAMPLES:
            dwTileWidth = XGNextMultiple( LargestTileSize.x, GPU_EDRAM_TILE_WIDTH_4X );
            dwTileHeight = XGNextMultiple( LargestTileSize.y, GPU_EDRAM_TILE_HEIGHT_4X );
            break;
    }

    if( CurrentScenario.dwTileCount > 1 )
    {
        // Expand tile surface dimensions to texture tile size, if it isn't already
        dwTileWidth = XGNextMultiple( dwTileWidth, GPU_TEXTURE_TILE_DIMENSION );
        dwTileHeight = XGNextMultiple( dwTileHeight, GPU_TEXTURE_TILE_DIMENSION );
    }

    // Use custom EDRAM allocation to create the rendertargets.
    // The color rendertarget is placed at address 0 in EDRAM.
    D3DSURFACE_PARAMETERS SurfaceParams;
    memset( &SurfaceParams, 0, sizeof( D3DSURFACE_PARAMETERS ) );
    SurfaceParams.Base = 0;
    hr = m_pd3dDevice->CreateRenderTarget( dwTileWidth,
                                           dwTileHeight,
                                           ( D3DFORMAT )MAKESRGBFMT( D3DFMT_A8R8G8B8 ),
                                           CurrentScenario.MSAAType,
                                           0, FALSE,
                                           &m_pRenderTarget,
                                           &SurfaceParams );
    if( FAILED( hr ) )
    {
        ATG::FatalError( "Cannot create tiling rendertarget.\n" );
    }

    // Record the size of the created rendertarget, and then set up allocation
    // for the next rendertarget right after the end of the first rendertarget.
    // Put the hierarchical Z buffer at the start of hierarchical Z memory.
    m_dwTileTargetSizeBytes = m_pRenderTarget->Size;
    SurfaceParams.Base = m_dwTileTargetSizeBytes / GPU_EDRAM_TILE_SIZE;
    SurfaceParams.HierarchicalZBase = 0;

    hr = m_pd3dDevice->CreateDepthStencilSurface( dwTileWidth,
                                                  dwTileHeight,
                                                  D3DFMT_D24S8,
                                                  CurrentScenario.MSAAType,
                                                  0, FALSE,
                                                  &m_pDepthStencil,
                                                  &SurfaceParams );
    if( FAILED( hr ) )
    {
        ATG::FatalError( "Cannot create tiling depth/stencil.\n" );
    }
    m_dwTileDepthStencilSizeBytes = m_pDepthStencil->Size;

    // If the front buffer size has changed, update the presentation parameters and
    // reset the D3D device.  Also set the font window to the new safe rectangle.
    if( dwFrontBufferHeight != m_d3dpp.BackBufferHeight ||
        dwFrontBufferWidth != m_d3dpp.BackBufferWidth )
    {
        m_d3dpp.BackBufferHeight = dwFrontBufferHeight;
        m_d3dpp.BackBufferWidth = dwFrontBufferWidth;
        m_pd3dDevice->Reset( &m_d3dpp );
        m_Font_Large.SetWindow( ATG::GetTitleSafeArea() );
        m_Font_Small.SetWindow( ATG::GetTitleSafeArea() );
    }

    // Set up the screen extents query mode that tiling will use.
    m_pd3dDevice->SetScreenExtentQueryMode( D3DSEQM_PRECLIP );
}

//--------------------------------------------------------------------------------------
// Name: Update()
// Desc: Called once per frame, the call is the entry point for animating the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Update()
{
    ATG::Timer CPUTimer;

    m_dwFrameCount++;
    FLOAT fElapsedTime = ( FLOAT )m_Timer.GetElapsedTime();

    m_EdgeAAMesh.Update( fElapsedTime );

    // Get the current gamepad status
    ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput();

    // Toggle help
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
        m_bDrawHelp = !m_bDrawHelp;

    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_B )
    {
        // Next AA Scenario
        g_dwScenarioID = (g_dwScenarioID + 1) % g_dwNumScenarios;
        ResetDevice();
    }

    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_A )
    {
        // Previous AA Scenario
        g_dwScenarioID = (g_dwScenarioID - 1 + g_dwNumScenarios) % g_dwNumScenarios;
        ResetDevice();
    }

    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_Y )
    {
        // Toggle texture maps
        if(m_Object.m_pTexturedPS == m_pTexModDiffusePS )
        {
            m_Object.m_pTexturedPS = m_pDiffuseOnlyPS;
        }
        else
        {
            m_Object.m_pTexturedPS = m_pTexModDiffusePS;
        }
    }


    // Setup viewing position from Gamepad
    static FLOAT fRotateX1 = 0.0f;
    static FLOAT fRotateY1 = 0.0f;
    fRotateX1 += pGamepad->fX1 * fElapsedTime * XM_PI * 0.5f;
    fRotateY1 += pGamepad->fY1 * fElapsedTime * XM_PI * 0.5f;
    m_matWorld = XMMatrixRotationRollPitchYaw( -fRotateY1, -fRotateX1, 0.0f );

    const XMVECTOR viewDir = XMVectorSet( 0.0f, 0.0f, -1.0f, 0.0f );

    if( g_dwScenarioID == 1 )
    {
        m_EdgeAAMesh.SetViewDir( viewDir );

        m_EdgeAAMesh.m_matView = m_matView;
        m_EdgeAAMesh.m_matProj = m_matProj;
    }

    m_fUpdateTime = ( FLOAT )CPUTimer.GetElapsedTime();

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: RenderAA()
// Desc: Renders the AA Mesh
//--------------------------------------------------------------------------------------
HRESULT Sample::RenderAA()
{
    // Draw the AA Edges
    m_EdgeAAMesh.m_matWorld = m_matWorld;
    m_EdgeAAMesh.RenderAAMesh( );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Render()
// Desc: Called once per frame, this call is the entry point for 3d rendering.
//--------------------------------------------------------------------------------------
HRESULT Sample::Render()
{
#ifndef _RELEASED3D
    m_pd3dDevice->QueryPerfCounters( m_pPerfCounterStart[ m_dwFrameCount % 3 ],
                                     D3DPERFQUERY_WAITGPUIDLE );
#endif

    const AA_SCENARIO& CurrentScenario = g_AAScenarios[g_dwScenarioID];
    const D3DVECTOR4 ClearColor = { 80.0/255.0f, 73.0/255.0f, 40.0/255.0f, 0 };
    bool bNeedTiling = (g_dwScenarioID > 1);

    m_pd3dDevice->SetRenderTarget( 0, m_pRenderTarget );
    m_pd3dDevice->SetDepthStencilSurface( m_pDepthStencil );

    if( bNeedTiling )
    {
        m_pd3dDevice->BeginTiling(  0,
                                    CurrentScenario.dwTileCount,
                                    CurrentScenario.TilingRects,
                                    &ClearColor, 1.0f, 0L );
    }
    else
    {
        m_pd3dDevice->ClearF( D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER |
                              D3DCLEAR_STENCIL | D3DCLEAR_HISTENCIL_CULL, NULL, 
                             &ClearColor, 1.0f, 0L );
    }

    // Set state
    FLOAT screenSize[4] = {(FLOAT)m_d3dpp.BackBufferWidth, (FLOAT)m_d3dpp.BackBufferHeight, 0.0f, 0.0f};
    m_pd3dDevice->SetVertexShaderConstantF( VSCONT_vScreenSize, screenSize, 1 );
    m_pd3dDevice->SetPixelShaderConstantF( PSCONT_vScreenSize, screenSize, 1 );

    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );

    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );

    // Draw the Mesh
    PIXBeginNamedEvent( 0xFFFFFFFF, "Mesh" );
    {
        m_pd3dDevice->SetVertexShader( m_pMeshVS );

        XMMATRIX matWVP = m_matWorld * m_matView * m_matProj;
        XMMATRIX matViewProj = m_matView * m_matProj;

        m_Object.m_matWorld = m_matWorld;
        m_Object.m_matViewProj = matViewProj;
        m_Object.m_vWorldLightDirection = XMVectorSet( g_vLightDir.x, g_vLightDir.y, g_vLightDir.z, 0.0f );
        matWVP = XMMatrixTranspose( matWVP );
        m_pd3dDevice->SetVertexShaderConstantF( VSCONT_matWorldViewProj, ( FLOAT* )&matWVP, 4 );
        m_pd3dDevice->SetVertexShaderConstantF( VSCONT_vLightDir, ( FLOAT* )&g_vLightDir, 1 );

        // Depth bias the object so that antialiasing edges will be visible
        const FLOAT fDepthBias = 0.0001f;
        const FLOAT fSSDepthBias = 2.0f;
        m_pd3dDevice->SetRenderState( D3DRS_DEPTHBIAS, ATG::FtoDW( fDepthBias ) );
        m_pd3dDevice->SetRenderState( D3DRS_SLOPESCALEDEPTHBIAS, ATG::FtoDW( fSSDepthBias ) );

        m_Object.Render();

        m_pd3dDevice->SetRenderState( D3DRS_DEPTHBIAS, ATG::FtoDW( 0.0f ) );
        m_pd3dDevice->SetRenderState( D3DRS_SLOPESCALEDEPTHBIAS, ATG::FtoDW( 0.0f ) );
    }
    PIXEndNamedEvent();

    m_pd3dDevice->Resolve( D3DRESOLVE_RENDERTARGET0, NULL, m_EdgeAAMesh.m_pFrameBuffer, NULL, 0, 0, NULL, 0, 0, NULL );

    if( g_dwScenarioID == 1)
    {
        // Perform edge-based antialiasing
        PIXBeginNamedEvent(0xFFFFFFF,"AA");
            RenderAA();
        PIXEndNamedEvent();
    }

#ifndef _RELEASED3D
    m_pd3dDevice->QueryPerfCounters( m_pPerfCounterEnd[ m_dwFrameCount % 3 ],
                                     D3DPERFQUERY_WAITGPUIDLE );
#endif

    // Show title, frame rate, and help
    m_Timer.MarkFrame();
    if( m_bDrawHelp )
    {
        m_Help.Render( &m_Font_Small, g_HelpCallouts, NUM_HELP_CALLOUTS );
    }
    else
    {
        m_Font_Large.Begin();
            m_Font_Large.SetScaleFactors( 1.0f, 1.0f );
            m_Font_Large.DrawText( 0, 0, 0xffffffff, L"Edge Based AA Sample" );
        m_Font_Large.End();
        m_Font_Small.Begin();
            m_Font_Small.SetScaleFactors( 1.0f, 1.0f );
            m_Font_Small.DrawText( 0, 0, 0xffffff00, m_Timer.GetFrameRate(), ATGFONT_RIGHT );
            m_Font_Small.DrawText( 0, 40, 0xffffffff, CurrentScenario.strScenarioName );

            WCHAR strText[100];

            FLOAT fYPos = 100;

            static FLOAT fAccumCPUTime = 0.0f;
            fAccumCPUTime = fAccumCPUTime + m_fUpdateTime;
            static FLOAT fAvgCPUTime = 0.0f;
            if( m_dwFrameCount % 100 == 0 )
            {
                fAvgCPUTime = fAccumCPUTime / 100.0f;
                fAccumCPUTime = 0.0f;
            }
            swprintf_s( strText, L"CPU update: %0.4f ms", fAvgCPUTime * 1000 );
            m_Font_Small.DrawText( 0, fYPos, 0xFFFFFF80, strText, ATGFONT_RIGHT );
            fYPos += 20;

#ifdef _RELEASED3D
            m_Font_Small.DrawText( 0, fYPos, 0xFF808080,
                             L"GPU render: not instrumented",
                             ATGFONT_RIGHT );
#else
            D3DPERFCOUNTER_VALUES StartValues;
            m_pPerfCounterStart[( m_dwFrameCount + 1 ) % 3]->GetValues( &StartValues, 0, NULL );
            D3DPERFCOUNTER_VALUES EndValues;
            m_pPerfCounterEnd[( m_dwFrameCount + 1 ) % 3]->GetValues( &EndValues, 0, NULL );

            // Subtract start values from end values
            UINT64* pStartValues = ( UINT64* )&StartValues;
            UINT64* pEndValues = ( UINT64* )&EndValues;
            const DWORD dwCount = sizeof( D3DPERFCOUNTER_VALUES ) / sizeof( UINT64 );
            for( DWORD i = 0; i < dwCount; ++i )
            {
                pEndValues[i] -= pStartValues[i];
            }

            // Display GPU time.  This only includes the time to render the instances
            static FLOAT fAccumGPUTime = 0.0f;
            fAccumGPUTime = fAccumGPUTime + ((FLOAT)EndValues.CP[0].QuadPart / 500000.0f);
            static FLOAT fAvgGPUTime = 0.0f;
            if( m_dwFrameCount % 100 == 0 )
            {
                fAvgGPUTime = fAccumGPUTime / 100.0f;
                fAccumGPUTime = 0.0f;
            }
            swprintf_s( strText, L"GPU render: %0.4f ms", fAvgGPUTime );
            m_Font_Small.DrawText( 0, fYPos, 0xFFFFFF80, strText, ATGFONT_RIGHT );
            fYPos += 20;
#endif
        m_Font_Small.End();
    }

    if( bNeedTiling )
    {
        m_pd3dDevice->SynchronizeToPresentationInterval();
        m_pd3dDevice->EndTiling( D3DRESOLVE_RENDERTARGET0 |
                                 D3DRESOLVE_ALLFRAGMENTS |
                                 D3DRESOLVE_CLEARRENDERTARGET |
                                 D3DRESOLVE_CLEARDEPTHSTENCIL,
                                 NULL, m_pFrontBufferTexture,
                                 &ClearColor, 1.0f, 0L, NULL );
        m_pd3dDevice->Swap( m_pFrontBufferTexture, NULL );
    }
    else
    {
        m_pd3dDevice->Present( NULL, NULL, NULL, NULL );
    }

    return S_OK;
}
