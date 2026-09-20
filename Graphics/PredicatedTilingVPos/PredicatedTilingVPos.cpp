//--------------------------------------------------------------------------------------
// PredicatedTilingVPos.cpp
//
// This sample demonstrates the use of manually predicated shader constant loads to send
// per-tile screenspace offsets to a VPOS pixel shader.  Since VPOS inputs to a pixel
// shader only report the offset from the upper left corner of the rendertarget, the
// VPOS values are "reset" for each tile, resulting in incorrect results.  The
// screenspace offsets in the pixel shader constant are added to the VPOS input,
// resulting in correct screen-relative pixel coordinates.
//
// XNA Developer Connection.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include <xtl.h>
#include <xgraphics.h>
#include <xboxmath.h>
#include <AtgFont.h>
#include <AtgInput.h>
#include <AtgUtil.h>
#include <AtgMesh.h>
#include <AtgHelp.h>
#include <AtgApp.h>
#include <AtgSimpleShaders.h>
#include <AtgDebugDraw.h>


//--------------------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//--------------------------------------------------------------------------------------
ATG::HELP_CALLOUT g_HelpCallouts[] =
{
    { ATG::HELP_A_BUTTON,     ATG::HELP_PLACEMENT_1, L"Toggle predicated\nshader constants" },
    { ATG::HELP_BACK_BUTTON,  ATG::HELP_PLACEMENT_2, L"Display\nhelp"  },
};
#define NUM_HELP_CALLOUTS (sizeof(g_HelpCallouts) / sizeof(g_HelpCallouts[0]))


// Starting index of the pixel shader constants that we will use for tiling offsets.
const DWORD g_dwTilingOffsetPixelConstantIndex = 4;


//--------------------------------------------------------------------------------------
// Name: class CRobotMesh
// Desc: This subclass of ATG::Mesh contains code that properly sends the transform
//       matrix to the vertex shader constant table before each mesh is rendered.  The
//       robot contains a hierarchy of frames, each with their own object-space
//       transform to place each body part.
//--------------------------------------------------------------------------------------
class CRobotMesh : public ATG::Mesh
{
public:
    XMMATRIX m_matWorld;
    XMMATRIX m_matViewProj;
    XMVECTOR m_vWorldLightDirection;

    virtual VOID RenderMeshCallback( DWORD dwFrame, const ATG::MESH_FRAME* pFrame, DWORD dwFlags )
    {
        XMMATRIX matWorld = m_matWorld * pFrame->m_matTransform;

        // ((M^-1)^T)^-1 = M^T
        XMMATRIX matWorldTranspose = XMMatrixTranspose( matWorld );

        XMVECTOR vLocalLightDir;
        vLocalLightDir = XMVector3TransformNormal( m_vWorldLightDirection, matWorldTranspose );
        vLocalLightDir = XMVector3Normalize( vLocalLightDir );

        ATG::g_pd3dDevice->SetPixelShaderConstantF( 0, ( FLOAT* )&vLocalLightDir, 1 );

        XMMATRIX matWVP = matWorld * m_matViewProj;
        ATG::g_pd3dDevice->SetVertexShaderConstantF( 0, ( FLOAT* )&matWVP, 4 );
    }
};


//--------------------------------------------------------------------------------------
// Name: class Sample
// Desc: The predicated tiling sample class.
//--------------------------------------------------------------------------------------
class Sample : public ATG::Application
{
public:
    HRESULT Initialize();
    HRESULT Update();
    HRESULT Render();
private:
    VOID    RenderScene();
    VOID    RenderUI();

    XMMATRIX m_matView;
    XMMATRIX m_matProj;
    XMVECTOR m_vWorldLightDirection;

    D3DTexture* m_pFrontBufferTexture;
    D3DTexture* m_pSceneResolveTexture;
    D3DTexture  m_SceneResolveTextureAs16SRGB;
    D3DSurface* m_pTilingRenderTarget;
    D3DSurface* m_pTilingDepthStencil;
    D3DSurface* m_pPostRenderTarget;
    IDirect3DVertexShader9* m_pVertexShader;
    IDirect3DPixelShader9* m_pPixelShader;
    IDirect3DVertexDeclaration9* m_pVertexDecl;

    D3DRECT m_pTilingRects[15];
    DWORD m_dwTileCount;
    BOOL m_bUseScreenSpaceOffsets;

    ATG::Font m_Font;
    ATG::Timer m_Timer;
    ATG::Help m_Help;
    BOOL m_bDrawHelp;

    CRobotMesh m_RobotMesh;
};


//--------------------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the program
//--------------------------------------------------------------------------------------
VOID __cdecl main()
{
    Sample SampleApp;

    // Set up the structure used to create the D3DDevice.
    // We don't need any back buffers for predicated tiling, since we will be creating
    // our own.  We also don't need depth/stencil created for us automatically.
    D3DPRESENT_PARAMETERS& d3dpp = SampleApp.m_d3dpp;
    ZeroMemory( &d3dpp, sizeof( D3DPRESENT_PARAMETERS ) );
    d3dpp.BackBufferWidth = 1280;
    d3dpp.BackBufferHeight = 720;
    d3dpp.BackBufferFormat = ( D3DFORMAT )MAKESRGBFMT( D3DFMT_A8R8G8B8 );
    d3dpp.FrontBufferFormat = ( D3DFORMAT )MAKESRGBFMT( D3DFMT_LE_X8R8G8B8 );
    d3dpp.MultiSampleType = D3DMULTISAMPLE_NONE;
    d3dpp.MultiSampleQuality = 0;
    d3dpp.BackBufferCount = 0;
    d3dpp.EnableAutoDepthStencil = FALSE;
    d3dpp.DisableAutoBackBuffer = TRUE;
    d3dpp.DisableAutoFrontBuffer = TRUE;
    d3dpp.AutoDepthStencilFormat = D3DFMT_D24S8;
    d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;

    SampleApp.m_dwDeviceCreationFlags = D3DCREATE_BUFFER_2_FRAMES;

    SampleApp.Run();
}


HRESULT Sample::Initialize()
{
    m_pFrontBufferTexture = NULL;
    m_pSceneResolveTexture = NULL;
    m_pTilingRenderTarget = NULL;
    m_pTilingDepthStencil = NULL;
    m_pPostRenderTarget = NULL;
    m_pVertexShader = NULL;
    m_pPixelShader = NULL;
    m_pVertexDecl = NULL;
    m_bDrawHelp = FALSE;
    m_bUseScreenSpaceOffsets = TRUE;

    m_vWorldLightDirection = XMVector3Normalize( XMVectorSet( 1, 0, 1, 1 ) );
    m_matProj = XMMatrixPerspectiveFovLH( XM_PIDIV4, 16.0f / 9.0f, 1.0f, 200.0f );

    // Create font.
    if( FAILED( m_Font.Create( "game:\\Media\\Fonts\\Arial_16.xpr" ) ) )
        ATG::FatalError( "Could not load font." );

    // Confine text drawing to the title safe area.
    m_Font.SetWindow( ATG::GetTitleSafeArea() );

    // Create mesh.
    if( FAILED( m_RobotMesh.Create( "game:\\Media\\Meshes\\Robot.xbg" ) ) )
        ATG::FatalError( "Could not load mesh." );

    // Create help screen.
    if( FAILED( m_Help.Create( "game:\\Media\\Help\\Help.xpr" ) ) )
        ATG::FatalError( "Could not load help resources." );

    ATG::SimpleShaders::Initialize( NULL, NULL );

    // Create rendertargets.
    D3DSURFACE_PARAMETERS SurfParams = { 0 };
    HRESULT hr = m_pd3dDevice->CreateRenderTarget( 1280, 256, ( D3DFORMAT )MAKESRGBFMT( D3DFMT_A8R8G8B8 ), D3DMULTISAMPLE_4_SAMPLES, 0,
                                                   FALSE, &m_pTilingRenderTarget, &SurfParams );
    if( FAILED( hr ) )
        ATG::FatalError( "Could not create tiling rendertarget." );
    SurfParams.Base = XGSurfaceSize( 1280, 256, D3DFMT_A8R8G8B8, D3DMULTISAMPLE_4_SAMPLES );

    hr = m_pd3dDevice->CreateDepthStencilSurface( 1280, 256, D3DFMT_D24S8, D3DMULTISAMPLE_4_SAMPLES, 0,
                                                  FALSE, &m_pTilingDepthStencil, &SurfParams );
    if( FAILED( hr ) )
        ATG::FatalError( "Could not create tiling depth/stencil." );

    SurfParams.Base = 0;
    hr = m_pd3dDevice->CreateRenderTarget( 1280, 720, ( D3DFORMAT )MAKESRGBFMT( D3DFMT_A8R8G8B8 ), D3DMULTISAMPLE_NONE, 0,
                                           FALSE, &m_pPostRenderTarget, &SurfParams );
    if( FAILED( hr ) )
        ATG::FatalError( "Could not create posteffects rendertarget." );

    // Create scene resolve and front buffer textures.
    hr = m_pd3dDevice->CreateTexture( 1280, 720, 1, 0, ( D3DFORMAT )MAKESRGBFMT( D3DFMT_A8R8G8B8 ), D3DPOOL_DEFAULT, &m_pSceneResolveTexture,
                                      NULL );
    if( FAILED( hr ) )
        ATG::FatalError( "Could not create scene resolve texture." );

    // Set up an alias of the resolve texture as an AS_16 sRGB format, since the GPU can't resolve 
    // to an AS_16_16_16_16 format. 
    // Alias this texture so there's no loss of precision when sampling the texture in the shader. If using
    // a standard SRGB format, the sRGB->Linear conversion happens with 8 bit precision, resulting 
    // in a loss of data. Using an AS_16 sRGB format causes the conversion to happen at 16-bit precision,
    // meaning no precision is lost.
    m_SceneResolveTextureAs16SRGB = *m_pSceneResolveTexture;
    ATG::ConvertTextureToAs16SRGBFormat( &m_SceneResolveTextureAs16SRGB );

    hr = m_pd3dDevice->CreateTexture( 1280, 720, 1, 0, ( D3DFORMAT )MAKESRGBFMT( D3DFMT_LE_X8R8G8B8 ), D3DPOOL_DEFAULT, &m_pFrontBufferTexture,
                                      NULL );
    if( FAILED( hr ) )
        ATG::FatalError( "Could not create front buffer texture." );

    // Load shaders.
    hr = ATG::LoadVertexShader( "game:\\media\\shaders\\ptvpos.xvu", &m_pVertexShader );
    if( FAILED( hr ) )
        ATG::FatalError( "Could not load vertex shader." );

    hr = ATG::LoadPixelShader( "game:\\media\\shaders\\ptvpos.xpu", &m_pPixelShader );
    if( FAILED( hr ) )
        ATG::FatalError( "Could not load pixel shader." );

    // Build a vertex decl.
    static const D3DVERTEXELEMENT9 VertexElements[] =
    {
        { 0,  0, D3DDECLTYPE_FLOAT3,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
        { 0, 12, D3DDECLTYPE_FLOAT3,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL,   0 },
        { 0, 24, D3DDECLTYPE_FLOAT2,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
        D3DDECL_END()
    };
    m_pd3dDevice->CreateVertexDeclaration( VertexElements, &m_pVertexDecl );

    // Create tiling rectangles for predicated tiling.
    const D3DRECT TilingRects[] =
    {
        { 0,   0, 1280, 256 },
        { 0, 256, 1280, 512 },
        { 0, 512, 1280, 720 }
    };
    m_dwTileCount = ARRAYSIZE( TilingRects );
    XMemCpy( m_pTilingRects, TilingRects, m_dwTileCount * sizeof( D3DRECT ) );

    return S_OK;
}


HRESULT Sample::Update()
{
    // Update FPS in the timer class.
    m_Timer.MarkFrame();

    // Get input from controllers (check for exit button sequence).
    ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput();

    // The A button toggles the screenspace offsets.
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_A )
    {
        m_bUseScreenSpaceOffsets = !m_bUseScreenSpaceOffsets;
    }

    // The Back button toggles the help screen.
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
        m_bDrawHelp = !m_bDrawHelp;

    // View matrix
    XMVECTOR vEyePt = XMVectorSet( 0.0f, 0.0f, -7.0f, 0.0f );
    XMVECTOR vLookatPt = XMVectorSet( 0.0f, -0.4f, 0.0f, 0.0f );
    XMVECTOR vUp = XMVectorSet( 0.0f, 1.0f, 0.0f, 0.0f );
    m_matView = XMMatrixLookAtLH( vEyePt, vLookatPt, vUp );

    // Update robot mesh constants.
    m_RobotMesh.m_matWorld = XMMatrixIdentity();
    m_RobotMesh.m_matViewProj = m_matView * m_matProj;
    m_RobotMesh.m_vWorldLightDirection = m_vWorldLightDirection;

    return S_OK;
}


HRESULT Sample::Render()
{
    // Set our tiled render target.
    m_pd3dDevice->SetRenderTarget( 0, m_pTilingRenderTarget );
    m_pd3dDevice->SetDepthStencilSurface( m_pTilingDepthStencil );

    PIXBeginNamedEvent( 0xFFFFFFFF, "Scene Render" );

    const DWORD dwTilingFlags = 0;
    D3DVECTOR4 ClearColor = { 0, 0, 0, 0 };
    m_pd3dDevice->BeginTiling( dwTilingFlags, m_dwTileCount, m_pTilingRects, &ClearColor, 1.0f, 0L );

    // Set the predicated pixel shader constants for screenspace offsets.
    if( m_bUseScreenSpaceOffsets )
    {
        // Request ownership of pixel shader constants 4-7.
        m_pd3dDevice->GpuOwnPixelShaderConstantF( g_dwTilingOffsetPixelConstantIndex, 4 );
        // Predicated on each tile, set pixel shader constants 4-7 from a temporary buffer allocated from the command buffer.
        for( DWORD i = 0; i < m_dwTileCount; ++i )
        {
            m_pd3dDevice->SetPredication( D3DPRED_TILE( i ) );
            // Request an allocation of pixel shader constants from the command buffer.
            D3DVECTOR4* pConstantData = NULL;
            HRESULT hr = m_pd3dDevice->GpuBeginPixelShaderConstantF4( g_dwTilingOffsetPixelConstantIndex,
                                                                      &pConstantData, 4 );
            if( SUCCEEDED( hr ) )
            {
                ZeroMemory( pConstantData, 4 * sizeof( D3DVECTOR4 ) );

                // Fill in the first constant with the tiling offset for this tile.
                pConstantData[0] = XMVectorSet( ( FLOAT )m_pTilingRects[i].x1, ( FLOAT )m_pTilingRects[i].y1, 0, 0 );

                m_pd3dDevice->GpuEndPixelShaderConstantF4();
            }
        }
        // Restore automatic predication.
        m_pd3dDevice->SetPredication( 0 );
    }

    // Render the scene.
    RenderScene();

    // Release ownership of the screenspace offset shader constants.
    m_pd3dDevice->GpuDisownAll();

    m_pd3dDevice->EndTiling( D3DRESOLVE_RENDERTARGET0 |
                             D3DRESOLVE_ALLFRAGMENTS |
                             D3DRESOLVE_CLEARRENDERTARGET |
                             D3DRESOLVE_CLEARDEPTHSTENCIL,
                             NULL, m_pSceneResolveTexture,
                             &ClearColor, 1.0f, 0L, NULL );

    PIXEndNamedEvent();

    PIXBeginNamedEvent( 0xFFFFFFFF, "Post Effects" );

    // Set the full screen rendertarget.
    m_pd3dDevice->SetRenderTarget( 0, m_pPostRenderTarget );
    m_pd3dDevice->SetDepthStencilSurface( NULL );

    // Restore the scene resolve texture to EDRAM.
    const D3DRECT ScreenRect = { 0, 0, 1280, 720 };
    ATG::DebugDraw::DrawScreenSpaceTexturedRect( ScreenRect, &m_SceneResolveTextureAs16SRGB );

    PIXEndNamedEvent();

    PIXBeginNamedEvent( 0xFFFFFFFF, "UI Render" );

    // Render UI.
    RenderUI();

    PIXEndNamedEvent();

    m_pd3dDevice->SynchronizeToPresentationInterval();

    // Resolve the rendered scene back to the front buffer.
    m_pd3dDevice->Resolve( D3DRESOLVE_RENDERTARGET0 | D3DRESOLVE_CLEARRENDERTARGET,
                           NULL,
                           m_pFrontBufferTexture,
                           NULL,
                           0, 0,
                           &ClearColor,
                           1.0f, 0, NULL );

    // Swap to the current front buffer, so we can see it on screen.
    m_pd3dDevice->Swap( m_pFrontBufferTexture, NULL );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: RenderScene()
// Desc: Sets some renderstate and draws the robot mesh.
//--------------------------------------------------------------------------------------
VOID Sample::RenderScene()
{
    // Set renderstate.
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );

    // Set shaders.
    m_pd3dDevice->SetVertexShader( m_pVertexShader );
    m_pd3dDevice->SetPixelShader( m_pPixelShader );
    m_pd3dDevice->SetVertexDeclaration( m_pVertexDecl );

    // Render the robot.
    m_RobotMesh.Render( ATG::MESH_NOTEXTURES | ATG::MESH_NOVERTEXDECL | ATG::MESH_NOFVF );
}


//--------------------------------------------------------------------------------------
// Name: RenderUI()
// Desc: Draws some statistics and other UI elements
//--------------------------------------------------------------------------------------
VOID Sample::RenderUI()
{
    if( m_bDrawHelp )
    {
        m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
    }
    else
    {
        // Draw a title and FPS indicator.
        m_Font.Begin();
        m_Font.SetScaleFactors( 1.2f, 1.2f );
        m_Font.DrawText( 0, 0, 0xffffffff, L"PredicatedTilingVPos" );
        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( 0, 0, 0xffffff00, m_Timer.GetFrameRate(), ATGFONT_RIGHT );
        m_Font.DrawText( 0, 30, 0xffffffff, L"Screenspace Offset Constants: " );
        m_Font.DrawText( 320, 30, 0xffffffff, m_bUseScreenSpaceOffsets ? L"Enabled" : L"Disabled" );
        m_Font.End();
    }
}


