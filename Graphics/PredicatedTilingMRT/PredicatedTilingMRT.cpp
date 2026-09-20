//--------------------------------------------------------------------------------------
// PredicatedTilingMRT.cpp
//
// This sample shows how to use Predicated Tiling while rendering to multiple render
// targets simultaneously.  It uses a multiple-output pixel shader to write to three
// different render targets.  After tiled rendering is complete, the resolved textures
// are displayed onscreen.  The tiled render targets use 2x hardware MSAA.
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include <xtl.h>
#include <xgraphics.h>
#include <xboxmath.h>
#include <AtgFont.h>
#include <AtgHelp.h>
#include <AtgInput.h>
#include <AtgMesh.h>
#include <AtgUtil.h>
#include <AtgApp.h>
#include <AtgSimpleShaders.h>
#include <AtgDebugDraw.h>

//--------------------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//--------------------------------------------------------------------------------------
ATG::HELP_CALLOUT g_HelpCallouts[] =
{
    { ATG::HELP_X_BUTTON,     ATG::HELP_PLACEMENT_2, L"Camera\ndistance" },
    { ATG::HELP_B_BUTTON,     ATG::HELP_PLACEMENT_1, L"Rotation" },
    { ATG::HELP_BACK_BUTTON,  ATG::HELP_PLACEMENT_2, L"Display\nhelp"  },
};
#define NUM_HELP_CALLOUTS (sizeof(g_HelpCallouts) / sizeof(g_HelpCallouts[0]))


//--------------------------------------------------------------------------------------
// Name: class RobotMesh
// Desc: This subclass of ATG::Mesh contains code that properly sends the transform
//       matrix to the vertex shader constant table before each mesh is rendered.  The
//       robot contains a hierarchy of frames, each with their own object-space
//       transform to place each body part.
//--------------------------------------------------------------------------------------
class RobotMesh : public ATG::Mesh
{
public:
    XMMATRIX m_matWorld;
    XMMATRIX m_matViewProj;

    virtual VOID RenderMeshCallback( DWORD dwFrame, const ATG::MESH_FRAME* pFrame, DWORD dwFlags )
    {
        XMMATRIX matWorld = pFrame->m_matTransform * m_matWorld;
        XMMATRIX matWVP = matWorld * m_matViewProj;

        XMMATRIX matWVPt = XMMatrixTranspose( matWVP );
        XMMATRIX matWt = XMMatrixTranspose( matWorld );

        ATG::g_pd3dDevice->SetVertexShaderConstantF( 0, ( FLOAT* )&matWVPt, 4 );
        ATG::g_pd3dDevice->SetVertexShaderConstantF( 4, ( FLOAT* )&matWt, 4 );
    }
};


//--------------------------------------------------------------------------------------
// Name: class Sample
// Desc: Main class for the PredicatedTilingMRT sample.
//--------------------------------------------------------------------------------------
class Sample : public ATG::Application
{
public:
                Sample();

    HRESULT     Initialize();
    HRESULT     Update();
    HRESULT     Render();

protected:
    VOID        LoadResources();
    VOID        AliasTextures();
    VOID        RenderResolvedTextures();
    VOID        RenderUI();

    // Scene rendering variables
    XMMATRIX m_matWorld;
    XMMATRIX m_matView;
    XMMATRIX m_matProj;
    FLOAT m_fAspectRatio;
    FLOAT m_fEyeRadius;
    BOOL m_bRotating;
    RobotMesh m_RobotMesh;
    XMFLOAT4    m_vLightPos[4];
    XMFLOAT4    m_vLightColor[4];

    // Textures
    D3DTexture* m_pResolveTexture[4];
    D3DTexture  m_ResolveTextureAs16SRGB[3];
    D3DTexture* m_pFrontBufferTexture;

    // Render targets
    D3DSurface* m_pTilingRenderTarget[3];
    D3DSurface* m_pTilingDepthStencil;
    D3DSurface* m_pFinalRenderTarget;

    // Shaders
    IDirect3DVertexShader9* m_pVertexShaderMesh;
    IDirect3DPixelShader9* m_pPixelShaderMesh;
    IDirect3DVertexDeclaration9* m_pVertexDecl;

    // Sample framework and options flags
    ATG::Font m_Font;
    ATG::Timer m_Timer;
    ATG::Help m_Help;
    BOOL m_bDrawHelp;

    // Tiling rectangles
    D3DRECT     m_pTilingRects[8];
    INT m_iTilingRectCount;
};


//--------------------------------------------------------------------------------------
// Name: Sample() constructor
//--------------------------------------------------------------------------------------
Sample::Sample() : m_pFrontBufferTexture( NULL ),
                   m_pTilingDepthStencil( NULL ),
                   m_pVertexShaderMesh( NULL ),
                   m_pPixelShaderMesh( NULL ),
                   m_pVertexDecl( NULL ),
                   m_bDrawHelp( FALSE ),
                   m_fEyeRadius( 8.0f ),
                   m_fAspectRatio( 16.0f / 9.0f ),
                   m_bRotating( TRUE )
{
}


//--------------------------------------------------------------------------------------
// Name: Initialize()
// Desc: Sets up the tiling rectangles and calls LoadResources(), which creates buffers
//       and loads resources.
//--------------------------------------------------------------------------------------
HRESULT Sample::Initialize()
{
    // Set up tiling rectangles and copy them to the tiling rect array.
    const D3DRECT pTilingRects[] =
    {
        { 0,   0, 1280, 256 },
        { 0, 256, 1280, 512 },
        { 0, 512, 1280, 720 }
    };

    m_iTilingRectCount = ARRAYSIZE( pTilingRects );
    memcpy( m_pTilingRects, pTilingRects, m_iTilingRectCount * sizeof( D3DRECT ) );

    // Load resources and create buffers and rendertargets.
    LoadResources();

    m_matWorld = XMMatrixIdentity();

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Update()
// Desc: Processes input and updates rendering variables, including point lights and
//       the view matrix.
//--------------------------------------------------------------------------------------
HRESULT Sample::Update()
{
    // Update FPS in the timer class.
    m_Timer.MarkFrame();

    // Get input from controllers (check for exit button sequence).
    ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput();

    // The Back button toggles the help screen.
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
        m_bDrawHelp = !m_bDrawHelp;

    // The X button changes camera distance.
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_X )
    {
        if( m_fEyeRadius == 8.0f )
            m_fEyeRadius = 4.0f;
        else
            m_fEyeRadius = 8.0f;
    }

    // The B button toggles camera rotation.
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_B )
        m_bRotating = !m_bRotating;

    // Animate four point lights.
    FLOAT fLightTime = ( FLOAT )m_Timer.GetAppTime();
    for( int i = 0; i < 4; ++i )
    {
        FLOAT fSin = sinf( fLightTime + 1.5f * ( FLOAT )i );
        FLOAT fCos = cosf( fLightTime + 1.5f * ( FLOAT )i );
        FLOAT fHeight = 1.5f * sinf( fLightTime * ( FLOAT )i * 0.75f ) + 1;
        m_vLightPos[i] = XMFLOAT4( 2.5f * fCos, fHeight, 1.5f * fSin, 0 );
    }
    m_vLightColor[0] = XMFLOAT4( 1.0f, 0.0f, 0.5f, 1.0f );
    m_vLightColor[1] = XMFLOAT4( 0.0f, 0.5f, 0.0f, 1.0f );
    m_vLightColor[2] = XMFLOAT4( 0.0f, 0.0f, 0.5f, 1.0f );
    m_vLightColor[3] = XMFLOAT4( 1.0f, 1.0f, 0.0f, 1.0f );

    // View matrix
    FLOAT fTime = ( FLOAT )m_Timer.GetAppTime();
    if( !m_bRotating )
        fTime = -XM_PIDIV2 / 0.3f;
    XMVECTOR vEyePt = XMVectorSet( cosf( 0.3f * fTime ) * m_fEyeRadius, 0.0f,
                                   sinf( 0.3f * fTime ) * m_fEyeRadius, 0.0f );
    XMVECTOR vLookatPt = XMVectorSet( 0.0f, 0.0f, 0.0f, 0.0f );
    XMVECTOR vUp = XMVectorSet( 0.0f, 1.0f, 0.0f, 0.0f );
    m_matView = XMMatrixLookAtLH( vEyePt, vLookatPt, vUp );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Render()
// Desc: Renders the scene using predicated tiling and multiple render targets.
//       Notice how extra Clears and Resolves are manually predicated to support tiled
//       rendering on targets 1 and 2 and the depth/stencil, in addition to target 0.
//--------------------------------------------------------------------------------------
HRESULT Sample::Render()
{
    // Set our tiled render targets.
    m_pd3dDevice->SetRenderTarget( 0, m_pTilingRenderTarget[0] );
    m_pd3dDevice->SetRenderTarget( 1, m_pTilingRenderTarget[1] );
    m_pd3dDevice->SetRenderTarget( 2, m_pTilingRenderTarget[2] );
    m_pd3dDevice->SetRenderTarget( 3, NULL );
    m_pd3dDevice->SetDepthStencilSurface( m_pTilingDepthStencil );

    D3DVECTOR4 ClearColor = { 0.0f, 0.0f, 0.3f, 1.0f };
    D3DVECTOR4 ClearColorBlack = { 0.0f, 0.0f, 0.0f, 0.0f };

    // Begin tiling.  Since we need to handle the first tile clear ourselves for 
    // rendertargets 1,2, we'll do it for target 0 as well.
    m_pd3dDevice->BeginTiling( D3DTILING_SKIP_FIRST_TILE_CLEAR,
                               m_iTilingRectCount, m_pTilingRects,
                               &ClearColor, 1.0f, 0L );

    // Clear render targets 0, 1 and 2 on the first tile.  We use SetPredication() to
    // make these clears occur only on tile 0.
    // On tiles 1 and later, the clear comes from the resolve after rendering the
    // previous tile.  Each resolve operation includes an optional clear that has
    // no performance penalty.
    m_pd3dDevice->SetPredication( D3DPRED_TILE( 0 ) );
    m_pd3dDevice->ClearF( D3DCLEAR_TARGET0 | D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL, &m_pTilingRects[0], &ClearColor,
                          1.0f, 0L );
    m_pd3dDevice->ClearF( D3DCLEAR_TARGET1, &m_pTilingRects[0], &ClearColorBlack, 1.0f, 0L );
    m_pd3dDevice->ClearF( D3DCLEAR_TARGET2, &m_pTilingRects[0], &ClearColorBlack, 1.0f, 0L );
    m_pd3dDevice->SetPredication( 0 );

    // Begin Z pass.  The Z pass is a subset of the full scene rendering.
    m_pd3dDevice->BeginZPass( 0 );

    // Set the point lights into pixel shader constants.
    m_pd3dDevice->SetPixelShaderConstantF( 0, ( FLOAT* )m_vLightPos, 4 );
    m_pd3dDevice->SetPixelShaderConstantF( 4, ( FLOAT* )m_vLightColor, 4 );

    // Set renderstate.
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHATESTENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );

    // Set shaders.
    m_pd3dDevice->SetVertexShader( m_pVertexShaderMesh );
    m_pd3dDevice->SetPixelShader( m_pPixelShaderMesh );
    m_pd3dDevice->SetVertexDeclaration( m_pVertexDecl );

    // Render the robot.
    m_RobotMesh.m_matViewProj = m_matView * m_matProj;
    m_RobotMesh.m_matWorld = m_matWorld;
    m_RobotMesh.Render( ATG::MESH_NOTEXTURES | ATG::MESH_NOVERTEXDECL | ATG::MESH_NOFVF );

    // End Z pass.
    m_pd3dDevice->EndZPass();

    // Resolve a rect from the three render targets to three textures.
    // Normally, the EndTiling() API resolves target 0 automatically, but since we need to
    // resolve from render targets 1 and 2 as well, we will do all resolves ourself.
    // We need to do this resolve after each tile's rendering is complete.
    for( UINT i = 0; i < ( UINT )m_iTilingRectCount; ++i )
    {
        // Set predication to tile i.
        m_pd3dDevice->SetPredication( D3DPRED_TILE( i ) );

        // Destination point is the upper left corner of the tiling rect.
        D3DPOINT* pDestPoint = ( D3DPOINT* )&m_pTilingRects[i];

        // Resolve fragment 0 of every pixel in the depth/stencil buffer.
        m_pd3dDevice->Resolve( D3DRESOLVE_DEPTHSTENCIL | D3DRESOLVE_FRAGMENT0,
                               &m_pTilingRects[i],
                               m_pResolveTexture[3],
                               pDestPoint,
                               0, 0,
                               &ClearColorBlack,
                               1.0f, 0L, NULL );

        // Resolve render target 0 and clear it and the depth/stencil buffer.
        m_pd3dDevice->Resolve( D3DRESOLVE_RENDERTARGET0 | D3DRESOLVE_CLEARRENDERTARGET | D3DRESOLVE_CLEARDEPTHSTENCIL,
                               &m_pTilingRects[i],
                               m_pResolveTexture[0],
                               pDestPoint,
                               0, 0,
                               &ClearColor,
                               1.0f, 0L, NULL );

        // Resolve render target 1 and clear it.
        m_pd3dDevice->Resolve( D3DRESOLVE_RENDERTARGET1 | D3DRESOLVE_CLEARRENDERTARGET,
                               &m_pTilingRects[i],
                               m_pResolveTexture[1],
                               pDestPoint,
                               0, 0,
                               &ClearColorBlack,
                               1.0f, 0L, NULL );

        // Resolve render target 2 and clear it.
        m_pd3dDevice->Resolve( D3DRESOLVE_RENDERTARGET2 | D3DRESOLVE_CLEARRENDERTARGET,
                               &m_pTilingRects[i],
                               m_pResolveTexture[2],
                               pDestPoint,
                               0, 0,
                               &ClearColorBlack,
                               1.0f, 0L, NULL );
    }
    // Restore predication to default.
    m_pd3dDevice->SetPredication( 0 );

    // End tiling.
    // When we pass NULL as the pDestTexture parameter to EndTiling(), it disables
    // the automatic resolve that EndTiling() normally performs.
    m_pd3dDevice->EndTiling( 0, NULL, NULL, &ClearColor, 1.0f, 0L, NULL );

    // Set up the full-screen rendertarget.
    m_pd3dDevice->SetRenderTarget( 0, m_pFinalRenderTarget );
    m_pd3dDevice->SetRenderTarget( 1, NULL );
    m_pd3dDevice->SetRenderTarget( 2, NULL );
    m_pd3dDevice->SetRenderTarget( 3, NULL );
    m_pd3dDevice->SetDepthStencilSurface( NULL );

    // Render the four resolved textures to the screen.
    RenderResolvedTextures();

    // Render UI and help.
    RenderUI();

    // Wait for the vertical blank before we resolve to the front buffer to avoid tearing.
    m_pd3dDevice->SynchronizeToPresentationInterval();

    // Resolve the final image to the front buffer.
    m_pd3dDevice->Resolve( D3DRESOLVE_RENDERTARGET0,
                           NULL,
                           m_pFrontBufferTexture,
                           NULL, 0, 0,
                           &ClearColor, 1.0f, 0, NULL );

    // Present the scene.
    m_pd3dDevice->Swap( m_pFrontBufferTexture, NULL );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: LoadResources()
// Desc: Creates all of the render targets, textures, and resources for this sample.
//--------------------------------------------------------------------------------------
VOID Sample::LoadResources()
{
    HRESULT hr = S_OK;

    DWORD dwFrontBufferWidth = m_d3dpp.BackBufferWidth;
    DWORD dwFrontBufferHeight = m_d3dpp.BackBufferHeight;

    // Create front buffer texture.
    m_pd3dDevice->CreateTexture( dwFrontBufferWidth, dwFrontBufferHeight,
                                 1, 0, ( D3DFORMAT )MAKESRGBFMT( D3DFMT_LE_X8R8G8B8 ), D3DPOOL_DEFAULT,
                                 &m_pFrontBufferTexture, NULL );

    // Create resolve target textures.
    m_pd3dDevice->CreateTexture( dwFrontBufferWidth, dwFrontBufferHeight,
                                 1, 0, ( D3DFORMAT )MAKESRGBFMT( D3DFMT_A8R8G8B8 ), D3DPOOL_DEFAULT,
                                 &m_pResolveTexture[0], NULL );
    m_pd3dDevice->CreateTexture( dwFrontBufferWidth, dwFrontBufferHeight,
                                 1, 0, ( D3DFORMAT )MAKESRGBFMT( D3DFMT_A8R8G8B8 ), D3DPOOL_DEFAULT,
                                 &m_pResolveTexture[1], NULL );
    m_pd3dDevice->CreateTexture( dwFrontBufferWidth, dwFrontBufferHeight,
                                 1, 0, ( D3DFORMAT )MAKESRGBFMT( D3DFMT_A8R8G8B8 ), D3DPOOL_DEFAULT,
                                 &m_pResolveTexture[2], NULL );

    // Alias these textures so there's no loss of precision when sampling the texture
    // in shader. If using a standard SRGB format, the sRGB->Linear conversion happens
    // with 8 bit precision, resulting in a loss of data. Using an AS_16 sRGB format
    // causes the conversion to happen at 16-bit precision, meaning no precision is lost.
    AliasTextures();

    // Create depth resolve target texture.
    D3DFORMAT DepthTextureFormat = D3DFMT_D24S8;

    m_pd3dDevice->CreateTexture( dwFrontBufferWidth, dwFrontBufferHeight,
                                 1, 0, DepthTextureFormat, D3DPOOL_DEFAULT,
                                 &m_pResolveTexture[3], NULL );

    // Set up projection matrix
    const FLOAT fZNear = 1.0f;
    const FLOAT fZFar = 15.0f;
    m_matProj = XMMatrixPerspectiveFovLH( XM_PIDIV4, m_fAspectRatio, fZNear, fZFar );

    // Compute tile width and height.  The tiling render targets will be created using
    // these dimensions.
    DWORD dwTileWidth = m_pTilingRects[ 0 ].x2;
    DWORD dwTileHeight = m_pTilingRects[ 0 ].y2;

    // Expand tile surface dimensions to texture tile size
    dwTileWidth = XGNextMultiple( dwTileWidth, GPU_TEXTURE_TILE_DIMENSION );
    dwTileHeight = XGNextMultiple( dwTileHeight, GPU_TEXTURE_TILE_DIMENSION );

    // Note that the tiled render targets created here use 2x MSAA.

    // Use custom EDRAM allocation to create the render targets.
    // The first render target is placed at address 0 in EDRAM.
    D3DSURFACE_PARAMETERS TileSurfaceParams;
    memset( &TileSurfaceParams, 0, sizeof( D3DSURFACE_PARAMETERS ) );
    TileSurfaceParams.Base = 0;
    hr = m_pd3dDevice->CreateRenderTarget( dwTileWidth, dwTileHeight,
                                           ( D3DFORMAT )MAKESRGBFMT( D3DFMT_A8R8G8B8 ), D3DMULTISAMPLE_2_SAMPLES,
                                           0, FALSE, &m_pTilingRenderTarget[0],
                                           &TileSurfaceParams );
    if( FAILED( hr ) )
    {
        ATG::FatalError( "Cannot create tiling render target 0.\n" );
    }

    // Record the size of the created render target, and then set up allocation
    // for the next render target right after the end of the first render target.
    TileSurfaceParams.Base += m_pTilingRenderTarget[ 0 ]->Size / GPU_EDRAM_TILE_SIZE;

    hr = m_pd3dDevice->CreateRenderTarget( dwTileWidth, dwTileHeight,
                                           ( D3DFORMAT )MAKESRGBFMT( D3DFMT_A8R8G8B8 ), D3DMULTISAMPLE_2_SAMPLES,
                                           0, FALSE, &m_pTilingRenderTarget[1],
                                           &TileSurfaceParams );
    if( FAILED( hr ) )
    {
        ATG::FatalError( "Cannot create tiling rendertarget 1.\n" );
    }

    TileSurfaceParams.Base += m_pTilingRenderTarget[ 1 ]->Size / GPU_EDRAM_TILE_SIZE;

    hr = m_pd3dDevice->CreateRenderTarget( dwTileWidth, dwTileHeight,
                                           ( D3DFORMAT )MAKESRGBFMT( D3DFMT_A8R8G8B8 ), D3DMULTISAMPLE_2_SAMPLES,
                                           0, FALSE, &m_pTilingRenderTarget[2],
                                           &TileSurfaceParams );
    if( FAILED( hr ) )
    {
        ATG::FatalError( "Cannot create tiling rendertarget 2.\n" );
    }

    TileSurfaceParams.Base += m_pTilingRenderTarget[ 2 ]->Size / GPU_EDRAM_TILE_SIZE;

    // Put the hierarchical Z buffer at the start of hierarchical Z memory.
    TileSurfaceParams.HierarchicalZBase = 0;

    hr = m_pd3dDevice->CreateDepthStencilSurface( dwTileWidth, dwTileHeight,
                                                  D3DFMT_D24S8, D3DMULTISAMPLE_2_SAMPLES,
                                                  0, FALSE, &m_pTilingDepthStencil,
                                                  &TileSurfaceParams );
    if( FAILED( hr ) )
    {
        ATG::FatalError( "Cannot create tiling depth/stencil surface.\n" );
    }

    // Create a full screen size render target for producing the final image.  This
    // render target will not be used with predicated tiling, and does not require
    // hardware MSAA.
    TileSurfaceParams.Base = 0;
    TileSurfaceParams.HierarchicalZBase = 0;
    hr = m_pd3dDevice->CreateRenderTarget( dwFrontBufferWidth, dwFrontBufferHeight,
                                           ( D3DFORMAT )MAKESRGBFMT( D3DFMT_A8R8G8B8 ), D3DMULTISAMPLE_NONE,
                                           0, FALSE, &m_pFinalRenderTarget,
                                           &TileSurfaceParams );
    if( FAILED( hr ) )
    {
        ATG::FatalError( "Cannot create final surface rendertarget.\n" );
    }

    // Create font.
    if( FAILED( m_Font.Create( "game:\\Media\\Fonts\\Arial_16.xpr" ) ) )
        ATG::FatalError( "Could not load font." );

    // Confine text drawing to the title safe area
    m_Font.SetWindow( ATG::GetTitleSafeArea() );

    // Create mesh.
    if( FAILED( m_RobotMesh.Create( "game:\\Media\\Meshes\\Robot.xbg" ) ) )
        ATG::FatalError( "Could not load mesh." );

    // Create help screen.
    if( FAILED( m_Help.Create( "game:\\Media\\Help\\Help.xpr" ) ) )
        ATG::FatalError( "Could not load help resources." );

    // Create vertex and pixel shaders.
    hr = ATG::LoadVertexShader( "game:\\Media\\Shaders\\PtMrt.xvu", &m_pVertexShaderMesh );
    if( FAILED( hr ) )
        ATG::FatalError( "Could not load shader." );
    hr = ATG::LoadPixelShader( "game:\\Media\\Shaders\\PtMrt.xpu", &m_pPixelShaderMesh );
    if( FAILED( hr ) )
        ATG::FatalError( "Could not load shader." );

    // Initialize simple shaders.  These are used in the DebugDraw class.
    ATG::SimpleShaders::Initialize( NULL, NULL );

    // Define the vertex elements.
    static const D3DVERTEXELEMENT9 VertexElements[ 4 ] =
    {
        { 0,  0, D3DDECLTYPE_FLOAT3,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
        { 0, 12, D3DDECLTYPE_FLOAT3,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL,   0 },
        { 0, 24, D3DDECLTYPE_FLOAT2,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
        D3DDECL_END()
    };

    // Create a vertex declaration from the element descriptions.
    m_pd3dDevice->CreateVertexDeclaration( VertexElements, &m_pVertexDecl );
}


//--------------------------------------------------------------------------------------
// Name: AliasTextures()
// Desc: Set up aliases of the resolve textures as AS_16 sRGB formats, since the GPU
//       can't resolve to AS_16_16_16_16 format textures. 
//--------------------------------------------------------------------------------------
void Sample::AliasTextures()
{
    for( int i = 0; i < 3; i++ )
    {
        m_ResolveTextureAs16SRGB[i] = *m_pResolveTexture[i];
        ATG::ConvertTextureToAs16SRGBFormat( &m_ResolveTextureAs16SRGB[i] );
    }
}


//--------------------------------------------------------------------------------------
// Name: RenderResolvedTextures()
// Desc: Draws the four resolved textures from the tiled rendering.  This illustrates 
//       the multiple render target output.
//--------------------------------------------------------------------------------------
VOID Sample::RenderResolvedTextures()
{
    m_pd3dDevice->Clear( 0, NULL, D3DCLEAR_TARGET, 0, 1.0f, 0 );

    DWORD dwScreenWidth = m_d3dpp.BackBufferWidth;
    DWORD dwScreenHeight = m_d3dpp.BackBufferHeight;

    // Draw texture 0.
    D3DRECT rect0 = { 0, 0, dwScreenWidth / 2, dwScreenHeight / 2 };
    ATG::DebugDraw::DrawScreenSpaceTexturedRect( rect0, &m_ResolveTextureAs16SRGB[0], FALSE );

    // Draw texture 1.
    D3DRECT rect1 = { dwScreenWidth / 2, 0, dwScreenWidth, dwScreenHeight / 2 };
    ATG::DebugDraw::DrawScreenSpaceTexturedRect( rect1, &m_ResolveTextureAs16SRGB[1], FALSE );

    // Draw texture 2.
    D3DRECT rect2 = { 0, dwScreenHeight / 2, dwScreenWidth / 2, dwScreenHeight };
    ATG::DebugDraw::DrawScreenSpaceTexturedRect( rect2, &m_ResolveTextureAs16SRGB[2], FALSE );

    // Draw the depth buffer texture.
    D3DRECT rect3 = { dwScreenWidth / 2, dwScreenHeight / 2, dwScreenWidth, dwScreenHeight };
    ATG::DebugDraw::DrawScreenSpaceTexturedRect( rect3, m_pResolveTexture[3], TRUE );
}


//--------------------------------------------------------------------------------------
// Name: RenderUI()
// Desc: Render the help, title, and framerate
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
        m_Font.DrawText( 0, 0, 0xffffffff, L"PredicatedTilingMRT" );
        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( 0, 0, 0xffffff00, m_Timer.GetFrameRate(), ATGFONT_RIGHT );
        m_Font.End();
    }
}


//--------------------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the program
//--------------------------------------------------------------------------------------
VOID __cdecl main()
{
    Sample sample;

    // The D3DCREATE_BUFFER_2_FRAMES flag is required for predicated tiling.  It allows
    // the D3D device to buffer up to 2 full frames in the command buffer, instead of
    // just one.  This allows predicated tiling to play back one frame while the next
    // frame is being generated.
    sample.m_dwDeviceCreationFlags = D3DCREATE_BUFFER_2_FRAMES;

    // Set up presentation parameters.  No automatic buffers are created.
    ZeroMemory( &sample.m_d3dpp, sizeof( D3DPRESENT_PARAMETERS ) );
    sample.m_d3dpp.BackBufferWidth = 1280;
    sample.m_d3dpp.BackBufferHeight = 720;
    sample.m_d3dpp.BackBufferFormat = ( D3DFORMAT )MAKESRGBFMT( D3DFMT_LE_X8R8G8B8 );
    sample.m_d3dpp.FrontBufferFormat = ( D3DFORMAT )MAKESRGBFMT( D3DFMT_LE_X8R8G8B8 );
    sample.m_d3dpp.MultiSampleType = D3DMULTISAMPLE_NONE;
    sample.m_d3dpp.MultiSampleQuality = 0;
    sample.m_d3dpp.BackBufferCount = 0;
    sample.m_d3dpp.EnableAutoDepthStencil = FALSE;
    sample.m_d3dpp.AutoDepthStencilFormat = D3DFMT_D24S8;
    sample.m_d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    sample.m_d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
    sample.m_d3dpp.DisableAutoBackBuffer = TRUE;
    sample.m_d3dpp.DisableAutoFrontBuffer = TRUE;

    sample.Run();
}
