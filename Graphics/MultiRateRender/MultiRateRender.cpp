//--------------------------------------------------------------------------------------
// MultiRateRender.cpp
//
// This sample demonstrates how to render a low latency overlay user interface at 60Hz,
// while rendering the scene at 30Hz. Latency is measured from the time input is read on
// the CPU to the VBlank where the GPU presents the frame. By using asynchronous swaps,
// the workload for the CPU and GPU can efficiently be distributed into two frames. The
// first frame is a short frame rendering only the overlay, and the second frame is a
// longer frame rendering the scene and the overlay. Even if the first frame takes only
// a few milliseconds to finish, it will automatically be presented during the next VBlank
// asynchronously without blocking the CPU or GPU. This effectively means that the
// second frame can be longer than 16.6ms. Without using async swaps, the first frame
// will take 16.6ms and the second frame will take 33.3ms, for a total of 49.9ms.
//
// The following diagram shows the advantage of using async swaps for a composite
// dual frame.
//
// Async Swaps ON: 30Hz
//
//          VBlank N            VBlank N+1          VBlank N+2
//            |-------------------|-------------------|
//            [Frame1][       Frame2          ][Block ]
//
// Async Swaps OFF: 20Hz
//
//          VBlank N        VBlank N+1      VBlank N+2      VBlank N+3
//            |-------------------|-------------------|-------------------|
//            [Frame1][Block     ][       Frame2          ][Block         ]
//
// Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include <xtl.h>
#include <AtgApp.h>
#include <AtgFont.h>
#include <AtgHelp.h>
#include <AtgInput.h>
#include <AtgMesh.h>
#include <AtgResource.h>
#include <AtgUtil.h>
#include <AtgSimpleShaders.h>

#include "DualFrameAsyncSwaps.h"


//--------------------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//--------------------------------------------------------------------------------------

ATG::HELP_CALLOUT g_HelpCallouts[] =
{
    { ATG::HELP_A_BUTTON,       ATG::HELP_PLACEMENT_1, L"Toggle 60Hz/30Hz Overlay" },
    { ATG::HELP_B_BUTTON,       ATG::HELP_PLACEMENT_1, L"Toggle Async Swaps On/Off" },
    { ATG::HELP_BACK_BUTTON,    ATG::HELP_PLACEMENT_1, L"Display Help" },
};

#define NUM_HELP_CALLOUTS ( ARRAYSIZE( g_HelpCallouts ) )


//--------------------------------------------------------------------------------------
// Globals variables and definitions
//--------------------------------------------------------------------------------------

const DWORD g_dwWaterColor          = 0x00004080;
const FLOAT g_fWrapOverlayUpdate    = 1.25f;
const FLOAT g_fOverlayScrollSpeed   = 0.02f;


//--------------------------------------------------------------------------------------
// Name: class Sample
// Desc: Main class to run this application. Most functionality is inherited
//       from the ATG::Application base class.
//--------------------------------------------------------------------------------------

class Sample : public ATG::Application
{    
    BOOL                m_bAsyncSwaps;          // Render using async swaps
    BOOL                m_bRenderOverlayAt60Hz; // Render overlay at 60Hz
    DualFrameAsyncSwaps m_DualFrameAsyncSwaps;  // Handling duel frame async swaps
    FLOAT               m_fOverlayUpdate;       // Update overlay scrolling
    D3DTexture*         m_pRestoreBuffer;       // Texture for restoring 30Hz buffer during 60Hz rendering

    VOID Render60HzFrame();
    VOID Render30HzFrame();
    VOID RenderSeaFloor();
    VOID RenderDolphin();
    VOID RenderUI();
    VOID RenderOverlay();
    VOID RestoreBackBuffer();
    VOID UpdateOverlay();

    BOOL        m_bDrawHelp;        // Draw help
    ATG::Timer  m_Timer;            // Timer
    ATG::Font   m_Font;             // Font for drawing text
    ATG::Help   m_Help;             // Help
    ATG::PackedResource m_Resource; // Bundled textures in a packed resource

    // Transform matrices
    XMMATRIX    m_matWorld;
    XMMATRIX    m_matView;
    XMMATRIX    m_matProj;

    // Dolphin object
    ATG::Mesh2  m_DolphinMesh1;
    ATG::Mesh2  m_DolphinMesh2;
    ATG::Mesh2  m_DolphinMesh3;
    DWORD       m_dwDolphinVertexSize;
    DWORD       m_dwNumDolphinVertices;
    DWORD       m_dwNumDolphinPrimitives;
    LPDIRECT3DTEXTURE9 m_pDolphinTexture;
    LPDIRECT3DVERTEXBUFFER9 m_pDolphinVB1;
    LPDIRECT3DVERTEXBUFFER9 m_pDolphinVB2;
    LPDIRECT3DVERTEXBUFFER9 m_pDolphinVB3;
    LPDIRECT3DINDEXBUFFER9 m_pDolphinIB;
    D3DPRIMITIVETYPE m_dwDolphinPrimType;
    LPDIRECT3DVERTEXDECLARATION9 m_pDolphinVertexDeclaration;
    LPDIRECT3DVERTEXSHADER9 m_pDolphinVertexShader;

    // Dolphin animation
    FLOAT   m_fKickFreq;
    FLOAT   m_fPhase;

    // Seafloor object
    ATG::Mesh2  m_SeaFloorMesh;
    DWORD       m_dwSeaFloorVertexSize;
    DWORD       m_dwNumSeaFloorVertices;
    DWORD       m_dwNumSeaFloorPrimitives;
    LPDIRECT3DTEXTURE9 m_pSeaFloorTexture;
    LPDIRECT3DVERTEXBUFFER9 m_pSeaFloorVB;
    LPDIRECT3DINDEXBUFFER9 m_pSeaFloorIB;
    D3DPRIMITIVETYPE m_dwSeaFloorPrimType;
    LPDIRECT3DVERTEXDECLARATION9 m_pSeaFloorVertexDeclaration;
    LPDIRECT3DVERTEXSHADER9 m_pSeaFloorVertexShader;

    // Water caustics
    LPDIRECT3DTEXTURE9 m_pCausticTextures[ 32 ];
    LPDIRECT3DTEXTURE9 m_pCurrentCausticTexture;
    LPDIRECT3DPIXELSHADER9 m_pPixelShader;

    // Sine wave overlay
    LPDIRECT3DVERTEXDECLARATION9 m_pOverlayDecl;
    LPDIRECT3DVERTEXSHADER9 m_pOverlayVertexShader;
    LPDIRECT3DPIXELSHADER9 m_pOverlayPixelShader;

public:
    virtual HRESULT Initialize();
    virtual HRESULT Update();
    virtual HRESULT Render();
};


//--------------------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the program
//--------------------------------------------------------------------------------------

VOID __cdecl main()
{
    Sample atgApp;
    ATG::GetVideoSettings( &atgApp.m_d3dpp.BackBufferWidth, &atgApp.m_d3dpp.BackBufferHeight );

    // Make sure display is gamma correct.
    atgApp.m_d3dpp.BackBufferFormat =  ( D3DFORMAT )MAKESRGBFMT( D3DFMT_A8R8G8B8 );
    atgApp.m_d3dpp.FrontBufferFormat = ( D3DFORMAT )MAKESRGBFMT( D3DFMT_LE_X8R8G8B8 );
    atgApp.m_d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_ONE;

    // Since we're using async swaps, we need to manage our own front buffers
    atgApp.m_d3dpp.DisableAutoFrontBuffer = TRUE;

    atgApp.Run();
}


//--------------------------------------------------------------------------------------
// Name: Initialize()
// Desc: Initialize app-dependent objects
//--------------------------------------------------------------------------------------

HRESULT Sample::Initialize()
{
    // Initial values
    m_bDrawHelp             = FALSE;
    m_bAsyncSwaps           = TRUE;
    m_bRenderOverlayAt60Hz  = TRUE;
    m_fOverlayUpdate        = 0.0f;
    
    // Initialize the object responsible for async swaps
    RETURN_ON_FAIL( m_DualFrameAsyncSwaps.Initialize( m_pd3dDevice, m_d3dpp ) );

    // Enable async swaps
    m_pd3dDevice->SetSwapMode( m_bAsyncSwaps );

    // Create the font
    RETURN_ON_FAIL( m_Font.Create( "game:\\Media\\Fonts\\Arial_16.xpr" ) );

    // Confine text drawing to the title safe area
    m_Font.SetWindow( ATG::GetTitleSafeArea() );

    // Create help screen.
    RETURN_ON_FAIL( m_Help.Create( "game:\\Media\\Help\\Help.xpr" ) );

    // Create the textures resource
    RETURN_ON_FAIL( m_Resource.Create( "game:\\Media\\Resource.xpr" ) );

    m_pDolphinTexture = m_Resource.GetTexture( "DolphinTexture" );
    m_pSeaFloorTexture = m_Resource.GetTexture( "SeafloorTexture" );

    for ( DWORD t = 0; t < ARRAYSIZE( m_pCausticTextures ); t++ )
    {
        CHAR strTextureName[80];
        sprintf_s( strTextureName, "WaterCaustic%02ld", t );
        m_pCausticTextures[t] = m_Resource.GetTexture( strTextureName );
    }

    RETURN_ON_FAIL( m_DolphinMesh1.Create( "game:\\Media\\Meshes\\dolphin1.xbg" ) );
    RETURN_ON_FAIL( m_DolphinMesh2.Create( "game:\\Media\\Meshes\\dolphin2.xbg" ) );
    RETURN_ON_FAIL( m_DolphinMesh3.Create( "game:\\Media\\Meshes\\dolphin3.xbg" ) );
    RETURN_ON_FAIL( m_SeaFloorMesh.Create( "game:\\Media\\Meshes\\Seafloor.xbg" ) );

    m_pDolphinVB1 = &m_DolphinMesh1.GetMesh()->m_VB;
    m_pDolphinVB2 = &m_DolphinMesh2.GetMesh()->m_VB;
    m_pDolphinVB3 = &m_DolphinMesh3.GetMesh()->m_VB;
    m_pDolphinIB = &m_DolphinMesh1.GetMesh()->m_IB;
    m_pSeaFloorVB = &m_SeaFloorMesh.GetMesh()->m_VB;
    m_pSeaFloorIB = &m_SeaFloorMesh.GetMesh()->m_IB;

    // Get the number of vertices and faces for the meshes
    m_dwDolphinPrimType = m_DolphinMesh1.GetMesh()->m_dwPrimType;
    m_dwNumDolphinVertices = m_DolphinMesh1.GetMesh()->m_pSubsets[0].dwVertexCount;
    m_dwNumDolphinPrimitives = m_DolphinMesh1.GetMesh()->m_pSubsets[0].dwPrimitiveCount;
    m_dwDolphinVertexSize = m_DolphinMesh1.GetMesh()->m_dwVertexSize;

    m_dwSeaFloorPrimType = m_SeaFloorMesh.GetMesh()->m_dwPrimType;
    m_dwNumSeaFloorVertices = m_SeaFloorMesh.GetMesh()->m_pSubsets[0].dwVertexCount;
    m_dwNumSeaFloorPrimitives = m_SeaFloorMesh.GetMesh()->m_pSubsets[0].dwPrimitiveCount;
    m_pSeaFloorVertexDeclaration = m_SeaFloorMesh.GetMesh()->m_pVertexDecl;
    m_dwSeaFloorVertexSize = m_SeaFloorMesh.GetMesh()->m_dwVertexSize;

    // Add some bumpiness to the seafloor
    srand( 5 );
    BYTE* pDst;
    m_pSeaFloorVB->Lock( 0, 0, ( VOID** )&pDst, 0 );
    for ( DWORD i = 0; i < m_dwNumSeaFloorVertices; i++ )
    {
        ( ( XMFLOAT3* )pDst )->y += ( rand() / ( FLOAT )RAND_MAX );
        ( ( XMFLOAT3* )pDst )->y += ( rand() / ( FLOAT )RAND_MAX );
        ( ( XMFLOAT3* )pDst )->y += ( rand() / ( FLOAT )RAND_MAX );
        pDst += m_dwSeaFloorVertexSize;
    }
    m_pSeaFloorVB->Unlock();

    // Build the vertex declaration for the dolphin
    D3DVERTEXELEMENT9 declDolphin[MAXD3DDECLLENGTH] = { 0 };
    ATG::AppendVertexElements( declDolphin, 0, m_DolphinMesh1.GetMesh()->m_VertexElements, 0 );
    ATG::AppendVertexElements( declDolphin, 1, m_DolphinMesh2.GetMesh()->m_VertexElements, 1 );
    ATG::AppendVertexElements( declDolphin, 2, m_DolphinMesh3.GetMesh()->m_VertexElements, 2 );

    // Create vertex shader for the dolphin
    RETURN_ON_FAIL( m_pd3dDevice->CreateVertexDeclaration( declDolphin, &m_pDolphinVertexDeclaration ) );
    RETURN_ON_FAIL( ATG::LoadVertexShader( "game:\\Media\\Shaders\\DolphinTween.xvu", &m_pDolphinVertexShader ) );
    RETURN_ON_FAIL( ATG::LoadVertexShader( "game:\\Media\\Shaders\\SeaFloor.xvu", &m_pSeaFloorVertexShader ) );

    // Create the common pixel shader
    RETURN_ON_FAIL( ATG::LoadPixelShader( "game:\\Media\\Shaders\\ShadeCausticsPixel.xpu", &m_pPixelShader ) );

    // Determine the aspect ratio
    FLOAT fAspectRatio = ( FLOAT )m_d3dpp.BackBufferWidth / ( FLOAT )m_d3dpp.BackBufferHeight;

    // Set the transform matrices
    XMVECTOR vEyePt = XMVectorSet( 0.0f, 0.0f, -5.0f, 0.0f );
    XMVECTOR vLookatPt = XMVectorSet( 0.0f, 0.0f, 0.0f, 0.0f );
    XMVECTOR vUpVec = XMVectorSet( 0.0f, 1.0f, 0.0f, 0.0f );
    m_matWorld = XMMatrixIdentity();
    m_matView = XMMatrixLookAtLH( vEyePt, vLookatPt, vUpVec );
    m_matProj = XMMatrixPerspectiveFovLH( XM_PI / 3, fAspectRatio, 1.0f, 10000.0f );

    // Create vertex declaration
    static const D3DVERTEXELEMENT9 declOverlay[] =
    {
        { 0,  0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_POSITION, 0 },
        D3DDECL_END()
    };

    RETURN_ON_FAIL( m_pd3dDevice->CreateVertexDeclaration( declOverlay, &m_pOverlayDecl ) );

    // Create vertex shader
    RETURN_ON_FAIL( ATG::LoadVertexShader( "game:\\Media\\Shaders\\SinWaveVS.xvu", &m_pOverlayVertexShader ) );

    // Create pixel shaders
    RETURN_ON_FAIL( ATG::LoadPixelShader( "game:\\Media\\Shaders\\SinWavePS.xpu", &m_pOverlayPixelShader ) );

    RETURN_ON_FAIL( m_pd3dDevice->CreateTexture( m_d3dpp.BackBufferWidth, m_d3dpp.BackBufferHeight, 1, 0, m_d3dpp.BackBufferFormat, 0, &m_pRestoreBuffer, NULL ) );

    ATG::SimpleShaders::Initialize( NULL, NULL );

    // Clear EDRAM to background color and save in the restore buffer for the first frame
    m_pd3dDevice->Clear( 0L, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL, g_dwWaterColor, 1.0f, 0L );
    m_pd3dDevice->Resolve(0, NULL, m_pRestoreBuffer, NULL, 0, 0, NULL, 0, 0, NULL);

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Update()
// Desc: Called once per frame, the call is the entry point for animating the scene.
//--------------------------------------------------------------------------------------

HRESULT Sample::Update()
{
    // Get the current time
    FLOAT fTime = ( FLOAT )m_Timer.GetAppTime();

    // Get the current gamepad state
    ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput();

    // The Back button toggles the help screen.
    if ( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
    {
        m_bDrawHelp = !m_bDrawHelp;
    }

    // Toggle between Sync/Async swaps.
    if ( pGamepad->wPressedButtons & XINPUT_GAMEPAD_B )
    {
        m_bAsyncSwaps = !m_bAsyncSwaps;
        m_pd3dDevice->SetSwapMode( m_bAsyncSwaps );
    }

    // Toggle 60Hz/30Hz overlay
    if ( pGamepad->wPressedButtons & XINPUT_GAMEPAD_A )
    {
        m_bRenderOverlayAt60Hz = !m_bRenderOverlayAt60Hz;
        m_bAsyncSwaps = m_bRenderOverlayAt60Hz;     // we need to switch async swaps off for 30Hz overlay
        m_pd3dDevice->SetSwapMode( m_bAsyncSwaps );
    }

    // We need PRESENT_INTERVAL_ONE for 60Hz overlay and PRESENT_INTERVAL_TWO for 30Hz overlay
    m_pd3dDevice->SetRenderState( D3DRS_PRESENTINTERVAL, m_bRenderOverlayAt60Hz ? D3DPRESENT_INTERVAL_ONE : D3DPRESENT_INTERVAL_TWO );

    // Animation attributes for the dolphin
    m_fKickFreq = 2.0f * fTime;
    m_fPhase    = fTime / 3.0f;

    // Animate the caustic textures
    const DWORD dwNumTextures = ARRAYSIZE( m_pCausticTextures );
    DWORD tex = ( ( DWORD )( fTime * dwNumTextures ) ) % dwNumTextures;
    m_pCurrentCausticTexture = m_pCausticTextures[ tex ];

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: UpdateOverlay()
// Desc: Update input data for scrolling of overlay. This update can be done in multiple
//       ways, e.g. update at a regular interval from a thread. For the sake of the 
//       sample we just keep it simple and increment the scrolling.
//--------------------------------------------------------------------------------------

VOID Sample::UpdateOverlay()
{
    PIXBeginNamedEvent( 0, __FUNCTION__ );

    // To show the same scroll speed when rendering at 30Hz, we need to use double the increment
    FLOAT fScrollIncrement = m_bRenderOverlayAt60Hz ? g_fOverlayScrollSpeed : ( 2 * g_fOverlayScrollSpeed );

    m_fOverlayUpdate += fScrollIncrement;

    // To ensure we don't run into floating point errors, wrap the float at some constant
    if ( m_fOverlayUpdate > g_fWrapOverlayUpdate )
    {
        m_fOverlayUpdate -= g_fWrapOverlayUpdate;
    }

    PIXEndNamedEvent();
}


//--------------------------------------------------------------------------------------
// Name: Render()
// Desc: Render the scene and overlays
//--------------------------------------------------------------------------------------

HRESULT Sample::Render()
{
    PIXBeginNamedEvent( 0, __FUNCTION__ );

    if ( m_bRenderOverlayAt60Hz )
    {
        // Render the first frame with only the overlay and present it
        Render60HzFrame();
        m_DualFrameAsyncSwaps.Present();

        // Render the second frame with the scene and overlay and present it
        Render30HzFrame();
        m_DualFrameAsyncSwaps.Present();

        // Throttle the CPU and GPU manually. This is really optional for this sample. The only
        // functional that Throttle() plays here is to reduce latency by keeping the CPU and GPU
        // in sync. By removing this call, an extra 16.6ms of latency will be added.
        m_DualFrameAsyncSwaps.Throttle();
    }
    else
    {
        // Render the scene with overlay and present it
        Render30HzFrame();
        m_DualFrameAsyncSwaps.Present();
    }

    PIXEndNamedEvent();

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Render60HzFrame()
// Desc: Render the 60Hz frame. This renders the UI and overlay.
//--------------------------------------------------------------------------------------

VOID Sample::Render60HzFrame()
{
    PIXBeginNamedEvent( 0, __FUNCTION__ );

    // Clear the viewport
    m_pd3dDevice->Clear( 0L, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL, g_dwWaterColor, 1.0f, 0L );

    // Restore the back buffer of the previous 30Hz frame
    RestoreBackBuffer();

    // Render the UI with overlay
    RenderUI();

    PIXEndNamedEvent();

}


//--------------------------------------------------------------------------------------
// Name: Render30HzFrame()
// Desc: Render the 30Hz frame. This renders the scene, UI and overlay.
//--------------------------------------------------------------------------------------

VOID Sample::Render30HzFrame()
{
    PIXBeginNamedEvent( 0, __FUNCTION__ );

    // Determine frame rate from the beginning of this frame
    m_Timer.MarkFrame();

    m_pd3dDevice->Clear( 0L, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL, g_dwWaterColor, 1.0f, 0L );

    // Render the scene
    RenderSeaFloor();
    RenderDolphin();

    // Save the current EDRAM so that we can restore it with the next 60Hz frame
    m_pd3dDevice->Resolve(0, NULL, m_pRestoreBuffer, NULL, 0, 0, NULL, 0, 0, NULL);

    // Render the UI with overlay
    RenderUI();

    PIXEndNamedEvent();
}


//--------------------------------------------------------------------------------------
// Name: RenderSeaFloor()
// Desc: Render the sea floor
//--------------------------------------------------------------------------------------

VOID Sample::RenderSeaFloor()
{
    PIXBeginNamedEvent( 0, __FUNCTION__ );

    // Use a loop to simulate some extra processing so we can show the benefit of using async swaps
    const UINT uNumIterations    = 125;
    const UINT uNumSubIterations = 2000;

    const FLOAT fWaterColor[] = { 0.0f, 0.25f, 0.5f, 1.0f };

    // Initialize default device states at the start of the frame
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MIPFILTER, D3DTEXF_POINT );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSU, D3DTADDRESS_MIRROR );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSV, D3DTADDRESS_MIRROR );
    m_pd3dDevice->SetSamplerState( 1, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState( 1, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState( 1, D3DSAMP_MIPFILTER, D3DTEXF_POINT );
    m_pd3dDevice->SetSamplerState( 1, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP );
    m_pd3dDevice->SetSamplerState( 1, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP );

    // Set the common pixel shader
    const FLOAT fAmbient[] = { 0.25f, 0.25f, 0.25f, 0.25f };
    m_pd3dDevice->SetPixelShader( m_pPixelShader );
    m_pd3dDevice->SetPixelShaderConstantF( 0, fWaterColor, 1 );
    m_pd3dDevice->SetPixelShaderConstantF( 1, fAmbient, 1 );

    // Set the water caustics texture.
    m_pd3dDevice->SetTexture( 1, m_pCurrentCausticTexture );

    // Render the seafloor
    m_pd3dDevice->SetTexture( 0, m_pSeaFloorTexture );
    m_pd3dDevice->SetVertexDeclaration( m_pSeaFloorVertexDeclaration );
    m_pd3dDevice->SetVertexShader( m_pSeaFloorVertexShader );
    m_pd3dDevice->SetStreamSource( 0, m_pSeaFloorVB, 0, m_dwSeaFloorVertexSize );
    m_pd3dDevice->SetIndices( m_pSeaFloorIB );

    // Use a loop to simulate some extra processing so we can show the benefit of using async swaps
    for ( UINT i = 0; i < uNumIterations; i++ )
    {
        for ( UINT j = 0; j < uNumSubIterations; j++ )
        {
            sqrt( 10.0f );
        }
        m_pd3dDevice->DrawIndexedPrimitive( m_dwSeaFloorPrimType, 0, 0, m_dwNumSeaFloorVertices, 0, m_dwNumSeaFloorPrimitives );
        m_pd3dDevice->InsertFence();
    }

    PIXEndNamedEvent();
}


//--------------------------------------------------------------------------------------
// Name: RenderDolphin()
// Desc: Render the dolphin
//--------------------------------------------------------------------------------------

VOID Sample::RenderDolphin()
{
    PIXBeginNamedEvent( 0, __FUNCTION__ );

    // Some basic constants
    const XMFLOAT4 vZero( 0.0f, 0.0f, 0.0f, 0.0f );
    const XMFLOAT4 vConstants( 1.0f, 0.5f, 0.2f, 0.05f );
    m_pd3dDevice->SetVertexShaderConstantF( 0, ( FLOAT* )&vZero, 1 );
    m_pd3dDevice->SetVertexShaderConstantF( 1, ( FLOAT* )&vConstants, 1 );
  
    // Move the dolphin in a circle
    XMMATRIX matDolphin, matTrans, matRotate1, matRotate2;
    matDolphin  = XMMatrixScaling( 0.01f, 0.01f, 0.01f );
    matRotate1  = XMMatrixRotationZ( -cosf( m_fKickFreq ) / 6 );
    matDolphin  = XMMatrixMultiply( matDolphin, matRotate1 );
    matRotate2  = XMMatrixRotationY( m_fPhase );
    matDolphin  = XMMatrixMultiply( matDolphin, matRotate2 );
    matTrans    = XMMatrixTranslation( -5 * sinf( m_fPhase ), sinf( m_fKickFreq ) / 2, 10 - 10 * cosf( m_fPhase ) );
    matDolphin  = XMMatrixMultiply( matDolphin, matTrans );

    FLOAT fWeight1;
    FLOAT fWeight2;
    FLOAT fWeight3;

    FLOAT fBlendWeight = sinf( m_fKickFreq );

    if ( fBlendWeight > 0.0f )
    {
        fWeight1 = fabsf( fBlendWeight );
        fWeight2 = 1.0f - fabsf( fBlendWeight );
        fWeight3 = 0.0f;
    }
    else
    {
        fWeight1 = 0.0f;
        fWeight2 = 1.0f - fabsf( fBlendWeight );
        fWeight3 = fabsf( fBlendWeight );
    }
    XMVECTOR vWeight = XMVectorSet( fWeight1, fWeight2, fWeight3, 0.0f );

    // Lighting vectors (in world space and in dolphin model space)
    // and other constants
    XMVECTOR vLight = XMVectorSet( 0.00f, 1.00f, 0.00f, 0.00f );
    XMVECTOR vLightDolphinSpace = XMVectorSet( 0.00f, 1.00f, 0.00f, 0.00f );
    XMVECTOR vDiffuse = XMVectorSet( 1.00f, 1.00f, 1.00f, 1.00f );
    XMVECTOR vAmbient = XMVectorSet( 0.25f, 0.25f, 0.25f, 0.25f );
    XMVECTOR vFog = XMVectorSet( 0.50f, 50.00f, 1.00f / ( 50.0f - 1.0f ), 0.00f );

    FLOAT fTime = ( FLOAT )m_Timer.GetAppTime();
    XMVECTOR vCaustics = XMVectorSet( 0.05f, 0.05f, sinf( fTime ) / 8, cosf( fTime ) / 10 );

    XMVECTOR vDeterminant;
    XMMATRIX matDolphinInv = XMMatrixInverse( &vDeterminant, matDolphin );
    vLightDolphinSpace = XMVector4Normalize( XMVector4Transform( vLight, matDolphinInv ) );

    // Vertex shader operations use transposed matrices
    XMMATRIX mat, matCamera, matTranspose, matCameraTranspose;
    XMMATRIX matViewTranspose, matProjTranspose;
    matCamera = XMMatrixMultiply( matDolphin, m_matView );
    mat = XMMatrixMultiply( matCamera, m_matProj );
    matTranspose = XMMatrixTranspose( mat );
    matCameraTranspose = XMMatrixTranspose( matCamera );
    matViewTranspose = XMMatrixTranspose( m_matView );
    matProjTranspose = XMMatrixTranspose( m_matProj );

    // Set the vertex shader constants
    m_pd3dDevice->SetVertexShaderConstantF( 0, ( FLOAT* )&vZero, 1 );
    m_pd3dDevice->SetVertexShaderConstantF( 1, ( FLOAT* )&vConstants, 1 );
    m_pd3dDevice->SetVertexShaderConstantF( 2, ( FLOAT* )&vWeight, 1 );
    m_pd3dDevice->SetVertexShaderConstantF( 4, ( FLOAT* )&matTranspose, 4 );
    m_pd3dDevice->SetVertexShaderConstantF( 8, ( FLOAT* )&matCameraTranspose, 4 );
    m_pd3dDevice->SetVertexShaderConstantF( 12, ( FLOAT* )&matViewTranspose, 4 );
    m_pd3dDevice->SetVertexShaderConstantF( 16, ( FLOAT* )&matProjTranspose, 4 );
    m_pd3dDevice->SetVertexShaderConstantF( 20, ( FLOAT* )&vLight, 1 );
    m_pd3dDevice->SetVertexShaderConstantF( 21, ( FLOAT* )&vLightDolphinSpace, 1 );
    m_pd3dDevice->SetVertexShaderConstantF( 22, ( FLOAT* )&vDiffuse, 1 );
    m_pd3dDevice->SetVertexShaderConstantF( 23, ( FLOAT* )&vAmbient, 1 );
    m_pd3dDevice->SetVertexShaderConstantF( 24, ( FLOAT* )&vFog, 1 );
    m_pd3dDevice->SetVertexShaderConstantF( 25, ( FLOAT* )&vCaustics, 1 );

    // Render the dolphin
    m_pd3dDevice->SetTexture( 0, m_pDolphinTexture );
    m_pd3dDevice->SetVertexDeclaration( m_pDolphinVertexDeclaration );
    m_pd3dDevice->SetVertexShader( m_pDolphinVertexShader );
    m_pd3dDevice->SetStreamSource( 0, m_pDolphinVB1, 0, m_dwDolphinVertexSize );
    m_pd3dDevice->SetStreamSource( 1, m_pDolphinVB2, 0, m_dwDolphinVertexSize );
    m_pd3dDevice->SetStreamSource( 2, m_pDolphinVB3, 0, m_dwDolphinVertexSize );
    m_pd3dDevice->SetIndices( m_pDolphinIB );

    m_pd3dDevice->DrawIndexedPrimitive( m_dwDolphinPrimType, 0, 0, m_dwNumDolphinVertices, 0, m_dwNumDolphinPrimitives );

    PIXEndNamedEvent();
}


//--------------------------------------------------------------------------------------
// Name: RenderUI()
// Desc: Render the UI
//--------------------------------------------------------------------------------------

VOID Sample::RenderUI()
{
    PIXBeginNamedEvent( 0, __FUNCTION__ );

    // Update and render the overlay
    UpdateOverlay();
    RenderOverlay();

    // Use a loop to simulate some extra processing so we can show the benefit of using async swaps
    m_Font.Begin();
    m_Font.SetScaleFactors( 1.2f, 1.2f );
    m_Font.DrawText( 0, 0, 0xffffffff, L"MultiRateRender" );
    m_Font.SetScaleFactors( 1.0f, 1.0f );
    m_Font.DrawText( 0, 0, 0xffffff00, m_Timer.GetFrameRate(), ATGFONT_RIGHT );
    m_Font.DrawText( 0, 25, 0xffffff00, m_bRenderOverlayAt60Hz ? L"Overlay at 60Hz" : L"Overlay at 30Hz", ATGFONT_RIGHT );
    m_Font.DrawText( 0, 50, 0xffffff00, m_bAsyncSwaps ? L"Async Swaps ON" : L"Async Swaps OFF", ATGFONT_RIGHT );
    m_Font.End();

    if ( m_bDrawHelp )
    {
        m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
    }

    PIXEndNamedEvent();
}


//--------------------------------------------------------------------------------------
// Name: RestoreBackBuffer()
// Desc: Restore the backbuffer from the 30Hz frame before rendering the 60Hz overlay
//--------------------------------------------------------------------------------------

VOID Sample::RestoreBackBuffer()
{
    PIXBeginNamedEvent( 0, __FUNCTION__ );

    ATG::MeshVertexPT* pVertexData = NULL;

    ATG::SimpleShaders::SetDeclPosTex();

    ATG::SimpleShaders::BeginShader_PreTransformed_Textured( m_pRestoreBuffer );
    
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_CCW );
    m_pd3dDevice->SetRenderState( D3DRS_VIEWPORTENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_HALFPIXELOFFSET, TRUE );
   
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP );

    HRESULT hr = m_pd3dDevice->BeginVertices( D3DPT_RECTLIST, 3, sizeof( ATG::MeshVertexPT ), ( VOID** )&pVertexData );

    // The ring buffer may run out of space when tiling, doing z-prepasses,
    // or using BeginCommandBuffer. If so, make the buffer larger.
    if( FAILED( hr ) )
    {
        ATG::FatalError( "Ring buffer out of memory.\n" );
    }

    assert( pVertexData );

    // Top Left
    pVertexData[0].Position = XMFLOAT3( 0.0f, 0.0f, 0 );
    pVertexData[0].TexCoord = XMFLOAT2( 0.0f, 0.0f );

    // Top Right
    pVertexData[1].Position = XMFLOAT3( ( FLOAT )m_d3dpp.BackBufferWidth, 0.0f, 0 );
    pVertexData[1].TexCoord = XMFLOAT2( 1.0, 0.0f );

    // Bottom Left
    pVertexData[2].Position = XMFLOAT3( ( FLOAT )0, ( FLOAT )m_d3dpp.BackBufferHeight, 0 );
    pVertexData[2].TexCoord = XMFLOAT2( 0.0f, 1.0f );

    m_pd3dDevice->EndVertices();

    ATG::SimpleShaders::EndShader();

    m_pd3dDevice->SetRenderState( D3DRS_VIEWPORTENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_HALFPIXELOFFSET, FALSE );

    PIXEndNamedEvent();
}


//--------------------------------------------------------------------------------------
// Name: RenderOverlay()
// Desc: Render the overlay
//--------------------------------------------------------------------------------------

VOID Sample::RenderOverlay()
{    
    PIXBeginNamedEvent( 0, "RenderOverlay (%f)", m_fOverlayUpdate );

    FLOAT fHeight = -0.75f;
    static XMFLOAT3 vUIRect[3];

    // Setup the overlay quad
    vUIRect[0] = XMFLOAT3(-1.0f, -0.5f, 1.0f);
    vUIRect[1] = XMFLOAT3( 1.0f, -0.5f, 1.0f);
    vUIRect[2] = XMFLOAT3(-1.0f, fHeight, 1.0f);

    D3DBLENDSTATE blendState;
    blendState.BlendOp = D3DBLENDOP_ADD;
    blendState.BlendOpAlpha = D3DBLENDOP_ADD;
    blendState.DestBlend = D3DBLEND_INVSRCALPHA;
    blendState.DestBlendAlpha = D3DBLEND_ZERO;
    blendState.SrcBlend = D3DBLEND_SRCALPHA;
    blendState.SrcBlendAlpha = D3DBLEND_ONE;

    m_pd3dDevice->SetBlendState( 0, blendState );

    m_pd3dDevice->SetVertexShader( m_pOverlayVertexShader );
    m_pd3dDevice->SetPixelShader( m_pOverlayPixelShader );
    m_pd3dDevice->SetVertexDeclaration( m_pOverlayDecl );
    m_pd3dDevice->SetPixelShaderConstantF( 0, &m_fOverlayUpdate, 1 );
    m_pd3dDevice->SetPixelShaderConstantF( 1, (FLOAT*)&fHeight, 1 );
    m_pd3dDevice->DrawPrimitiveUP(D3DPT_RECTLIST, 1, vUIRect, sizeof(XMFLOAT3)  );

    PIXEndNamedEvent();
}
