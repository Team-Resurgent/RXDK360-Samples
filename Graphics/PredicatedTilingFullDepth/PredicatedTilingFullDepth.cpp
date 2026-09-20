//--------------------------------------------------------------------------------------
// PredicatedTilingFullDepth.cpp
//
// This sample shows how to use Predicated Tiling while keeping the full depth buffer
// in EDRAM at all times.  This eliminates the need to resolve and restore the
// depth buffer if more rendering must be performed after tiling.
//
// This does not eliminate the EDRAM requirement of the depth buffer.  There must be
// enough EDRAM to contain the full depth buffer and any render target tiles required
// for rendering.  One situation where this might be practical is deferred rendering 
// where multiple render targets and 1xMSAA is used.  After performing the deferred 
// rendering with tiling, a forward rendering pass for transparent objects can be
// performed without having to restore the scene's depth information.
//
// This sample implements a simple deferred renderer using predicated tiling.  The
// red robot is rendered with deferred lighting using two buffers, one for color and
// one for normals.  The two render targets plus a depth buffer requires more than
// the available 10MB of EDRAM, so predicated tiling is used.  After the first robot
// is rendered, a second white robot is rendered with transparency.  Because of the
// transparency, it uses a simple forward rendering lighting calculation.  For correct
// results, the forward renderer must use the depth information from the first portion
// of the scene.  Two possible techniques are demonstrated for dealing with this
// situation.
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
#include <AtgUtil.h>

#define MAX_TILES 15

//--------------------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//--------------------------------------------------------------------------------------
ATG::HELP_CALLOUT g_HelpCallouts[] =
{
    { ATG::HELP_A_BUTTON,     ATG::HELP_PLACEMENT_1, L"Cycle scenario" },
    { ATG::HELP_Y_BUTTON,     ATG::HELP_PLACEMENT_2, L"Force\npredication" },
    { ATG::HELP_X_BUTTON,     ATG::HELP_PLACEMENT_2, L"Camera\ndistance" },
    { ATG::HELP_B_BUTTON,     ATG::HELP_PLACEMENT_1, L"Rotation" },
    { ATG::HELP_BACK_BUTTON,  ATG::HELP_PLACEMENT_2, L"Display\nhelp"  },
};
#define NUM_HELP_CALLOUTS (sizeof(g_HelpCallouts) / sizeof(g_HelpCallouts[0]))


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
        XMMATRIX matWorld = pFrame->m_matTransform * m_matWorld;

        // ((M^-1)^T)^-1 = M^T
        XMMATRIX matWorldTranspose = XMMatrixTranspose( matWorld );

        XMVECTOR vLocalLightDir;
        vLocalLightDir = XMVector3TransformNormal( m_vWorldLightDirection, matWorldTranspose );
        vLocalLightDir = XMVector3Normalize( vLocalLightDir );

        ATG::g_pd3dDevice->SetVertexShaderConstantF( 8, ( FLOAT* )&vLocalLightDir, 1 );

        XMMATRIX matWVP = matWorld * m_matViewProj;
        ATG::g_pd3dDevice->SetVertexShaderConstantF( 0, ( FLOAT* )&matWVP, 4 );

        ATG::g_pd3dDevice->SetVertexShaderConstantF( 4, ( FLOAT* )&matWorld, 4 );
    }
};


//--------------------------------------------------------------------------------------
// Name: class Sample
// Desc: The predicated tiling sample class.
//--------------------------------------------------------------------------------------
class Sample : public ATG::Application
{
public:
            Sample();
    HRESULT Initialize();
    HRESULT Update();
    HRESULT Render();
private:
    VOID    LoadResources();
    VOID    RenderScene(XMMATRIX matWorld);
    VOID    RenderDeferred();
    VOID    FastDepthRestore();
    VOID    RenderForward();
    VOID    RenderUI();

    // Scene rendering variables
    XMMATRIX m_matWorld;
    XMMATRIX m_matView;
    XMMATRIX m_matProj;
    XMVECTOR m_vWorldLightDirection;
    FLOAT m_fEyeRadius;
    BOOL m_bUseFullDepthStencil;
    BOOL m_bRotating;
    BOOL m_bForcedPredication;
    CRobotMesh m_RobotMesh;

    // Textures
    D3DTexture* m_pFrontBufferTexture;
    D3DTexture* m_pColorResolveTexture;
    D3DTexture* m_pNormalsResolveTexture;
    D3DTexture  m_ColorResolveTextureAs16SRGB;
    D3DTexture* m_pSceneDepthTexture;
    D3DTexture  m_SceneDepthTextureAs8888;

    // Render Targets
    D3DSurface* m_pColorRenderTarget;
    D3DSurface* m_pNormalsRenderTarget;
    D3DSurface* m_pFullDepthStencil;
    D3DSurface* m_pFullDepthStencilAs8888;
    D3DSurface* m_pDepthRestore4x;
    D3DSurface* m_pTilingDepthStencil[MAX_TILES];
    D3DSurface* m_pPostRenderTarget;
    D3DDevice*  m_pCommandBufferDevice;
    D3DCommandBuffer* m_pDepthCommandBuffer[MAX_TILES];

    // Shaders and Vertex Declarations
    IDirect3DVertexShader9* m_pForwardRenderVS;
    IDirect3DPixelShader9*  m_pForwardRenderPS;
    IDirect3DVertexShader9* m_pDeferredRenderVS;
    IDirect3DPixelShader9*  m_pDeferredRenderPS;
    IDirect3DVertexDeclaration9* m_pVertexDecl;

    IDirect3DVertexShader9* m_pPostProcessVS;
    IDirect3DPixelShader9*  m_pDeferredLightingPS;
    IDirect3DPixelShader9*  m_pFastDepthRestorePS;
    IDirect3DVertexDeclaration9* m_pVertexDeclPost;

    // Sample framework and options flags
    ATG::Font m_Font;
    ATG::Timer m_Timer;
    ATG::Help m_Help;
    BOOL m_bDrawHelp;

    // Memory usage statistics
    DWORD m_dwRenderTargetTileSizeBytes;
    DWORD m_dwDepthStencilTileSizeBytes;
    DWORD m_dwDepthStencilFullSizeBytes;

    // Tiling rectangles
    D3DRECT m_pTilingRects[MAX_TILES];
    DWORD m_dwTilingRectCount;
};


Sample::Sample() : m_pCommandBufferDevice( NULL ),
                   m_pFrontBufferTexture( NULL ),
                   m_pSceneDepthTexture( NULL ),
                   m_pFullDepthStencil( NULL ),
                   m_pFullDepthStencilAs8888( NULL ),
                   m_pPostRenderTarget( NULL ),
                   m_pForwardRenderVS( NULL ),
                   m_pForwardRenderPS( NULL ),
                   m_pDeferredRenderVS( NULL ),
                   m_pDeferredRenderPS( NULL ),
                   m_pVertexDecl( NULL ),
                   m_pPostProcessVS( NULL ),
                   m_pDeferredLightingPS( NULL ),
                   m_pFastDepthRestorePS( NULL ),
                   m_pVertexDeclPost( NULL ),
                   m_bDrawHelp( FALSE ),
                   m_fEyeRadius( 8.0f ),
                   m_bUseFullDepthStencil( TRUE ),
                   m_bRotating( TRUE ),
                   m_bForcedPredication( FALSE ),
                   m_dwRenderTargetTileSizeBytes( 0 ),
                   m_dwDepthStencilTileSizeBytes( 0 ),
                   m_dwDepthStencilFullSizeBytes( 0 )
{
}


//--------------------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the program
//--------------------------------------------------------------------------------------
VOID __cdecl main()
{
    Sample atgApp;

    // Set up the structure used to create the D3DDevice.
    // We don't need any back buffers for predicated tiling, since we will be creating
    // our own.  We also don't need depth/stencil created for us automatically.
    D3DPRESENT_PARAMETERS& d3dpp = atgApp.m_d3dpp;
    ZeroMemory( &d3dpp, sizeof( D3DPRESENT_PARAMETERS ) );
    d3dpp.BackBufferWidth = 1280;
    d3dpp.BackBufferHeight = 720;
    d3dpp.BackBufferFormat = ( D3DFORMAT )MAKESRGBFMT( D3DFMT_A8R8G8B8 );
    d3dpp.MultiSampleType = D3DMULTISAMPLE_NONE;
    d3dpp.MultiSampleQuality = 0;
    d3dpp.BackBufferCount = 0;
    d3dpp.EnableAutoDepthStencil = FALSE;
    d3dpp.DisableAutoBackBuffer = TRUE;
    d3dpp.DisableAutoFrontBuffer = TRUE;
    d3dpp.AutoDepthStencilFormat = D3DFMT_D24S8;
    d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;

    atgApp.m_dwDeviceCreationFlags = D3DCREATE_BUFFER_2_FRAMES;

    atgApp.Run();
}


//--------------------------------------------------------------------------------------
// Name: LoadResources()
// Desc: Creates all of the render targets and textures for this sample.
//--------------------------------------------------------------------------------------
VOID Sample::LoadResources()
{
    HRESULT hr = S_OK;

    DWORD dwFrontBufferWidth = m_d3dpp.BackBufferWidth;
    DWORD dwFrontBufferHeight = m_d3dpp.BackBufferHeight;

    // Set up tiling front buffer texture.
    // These are the size of your desired rendering surface (in this case, the whole
    // screen).
    m_pd3dDevice->CreateTexture( dwFrontBufferWidth, dwFrontBufferHeight,
                                 1, 0, ( D3DFORMAT )MAKESRGBFMT( D3DFMT_LE_X8R8G8B8 ),
                                 D3DPOOL_DEFAULT, &m_pFrontBufferTexture, NULL );
    if( NULL == m_pFrontBufferTexture )
    {
        ATG::FatalError( "Could not create front buffer." );
    }

    // Create resolve target textures.
    // One for the color and one for the normals
    m_pd3dDevice->CreateTexture( dwFrontBufferWidth, dwFrontBufferHeight,
                                 1, 0, ( D3DFORMAT )MAKESRGBFMT( D3DFMT_A8R8G8B8 ),
                                 D3DPOOL_DEFAULT, &m_pColorResolveTexture, NULL );
    m_pd3dDevice->CreateTexture( dwFrontBufferWidth, dwFrontBufferHeight,
                                 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT,
                                 &m_pNormalsResolveTexture, NULL );
    if( NULL == m_pColorResolveTexture || NULL == m_pNormalsResolveTexture )
    {
        ATG::FatalError( "Could not create scene resolve textures." );
    }

    // Set up an alias of the resolve texture as an AS_16 sRGB format, since the GPU can't resolve 
    // to an AS_16_16_16_16 format. 
    // Alias this texture so there's no loss of precision when sampling the texture in the shader. If using
    // a standard SRGB format, the sRGB->Linear conversion happens with 8 bit precision, resulting 
    // in a loss of data. Using an AS_16 sRGB format causes the conversion to happen at 16-bit precision,
    // meaning no precision is lost.
    m_ColorResolveTextureAs16SRGB = *m_pColorResolveTexture;
    ATG::ConvertTextureToAs16SRGBFormat( &m_ColorResolveTextureAs16SRGB );

    // Create the depth buffer texture for resolving the depth information during
    // tiling.  This is used for the fast depth restore technique when not keeping the
    // full depth buffer in EDRAM.  Instead, each tile of the depth buffer will be
    // resolved and the full depth buffer will be restored to EDRAM once complete.
    m_pd3dDevice->CreateTexture( dwFrontBufferWidth, dwFrontBufferHeight,
                                 1, 0, D3DFMT_D24S8, D3DPOOL_DEFAULT,
                                 &m_pSceneDepthTexture, NULL );
    if( NULL == m_pSceneDepthTexture )
    {
        ATG::FatalError( "Could not create scene depth texture." );
    }

    // Create an alias of the depth texture as 8888 for the fast depth restore
    XGSetTextureHeader( dwFrontBufferWidth, dwFrontBufferHeight, 1, 0,
                        D3DFMT_A8R8G8B8, 0, 0, 0, 0,
                        &m_SceneDepthTextureAs8888, 0, 0 );
    XGOffsetResourceAddress( &m_SceneDepthTextureAs8888,
                             (VOID*)(m_pSceneDepthTexture->Format.BaseAddress << GPU_TEXTURE_ADDRESS_SHIFT) );

    // Compute tile width and height.  The tiling render targets will be created using
    // these dimensions.
    DWORD dwTileWidth = m_pTilingRects[0].x2;
    DWORD dwTileHeight = m_pTilingRects[0].y2;

    // Expand tile surface dimensions to texture tile size, if it isn't already
    dwTileWidth = XGNextMultiple( dwTileWidth, GPU_TEXTURE_TILE_DIMENSION );
    dwTileHeight = XGNextMultiple( dwTileHeight, GPU_TEXTURE_TILE_DIMENSION );

    // Use custom EDRAM allocation to create the rendertargets.
    // The depth buffer is placed at address 0 in EDRAM.
    D3DSURFACE_PARAMETERS SurfaceParams;
    memset( &SurfaceParams, 0, sizeof( D3DSURFACE_PARAMETERS ) );
    SurfaceParams.Base = 0;
    SurfaceParams.HierarchicalZBase = 0;

    // Create the full depth buffer in EDRAM
    // This is used for the full depth technique and the forward renderer
    hr = m_pd3dDevice->CreateDepthStencilSurface( dwFrontBufferWidth,
                                                  dwFrontBufferHeight,
                                                  D3DFMT_D24S8,
                                                  D3DMULTISAMPLE_NONE,
                                                  0, FALSE,
                                                  &m_pFullDepthStencil,
                                                  &SurfaceParams );
    if( FAILED( hr ) )
    {
        ATG::FatalError( "Cannot create full depth/stencil.\n" );
    }

    // The 8888 version of the depth buffer is placed at the same address
    // in EDRAM for the fast depth restore
    hr = m_pd3dDevice->CreateRenderTarget( dwFrontBufferWidth,
                                           dwFrontBufferHeight,
                                           D3DFMT_A8R8G8B8,
                                           D3DMULTISAMPLE_NONE,
                                           0, FALSE,
                                           &m_pFullDepthStencilAs8888,
                                           &SurfaceParams );
    if( FAILED( hr ) )
    {
        ATG::FatalError( "Cannot create full depth/stencil as 8888.\n" );
    }

    // Create a 4xMSAA depth target of half size to serve as a full screen
    // quad when restoring the Hi-Z
    hr = m_pd3dDevice->CreateDepthStencilSurface( dwFrontBufferWidth >> 1,
                                                  dwFrontBufferHeight >> 1,
                                                  D3DFMT_D24S8,
                                                  D3DMULTISAMPLE_4_SAMPLES,
                                                  0, FALSE,
                                                  &m_pDepthRestore4x,
                                                  &SurfaceParams );
    if( FAILED( hr ) )
    {
        ATG::FatalError( "Cannot create half size depth/stencil.\n" );
    }

    m_dwDepthStencilFullSizeBytes = m_pFullDepthStencil->Size;
    
    // Create depth buffer tiles that overlap the full depth buffer
    // at the correct locations. These are used for the full depth technique.
    // When rendering tile 0, the depth is written into the top depth tile.
    // When rendering tile 1, the next depth tile is used and so on.
    for( DWORD i = 0; i < m_dwTilingRectCount; ++i )
    {
        // Each depth tile lines up in EDRAM at the correct location with the full
        // depth buffer.  Tile 0 is at the top. Tile 1 lines up right below it, etc.
        SurfaceParams.Base = i * XGSurfaceSize( dwTileWidth, dwTileHeight, D3DFMT_D24S8, D3DMULTISAMPLE_NONE );
        SurfaceParams.HierarchicalZBase = i * XGHierarchicalZSize( dwTileWidth, dwTileHeight, D3DMULTISAMPLE_NONE );

        hr = m_pd3dDevice->CreateDepthStencilSurface( dwTileWidth,
                                                      dwTileHeight,
                                                      D3DFMT_D24S8,
                                                      D3DMULTISAMPLE_NONE,
                                                      0, FALSE,
                                                      &m_pTilingDepthStencil[i],
                                                      &SurfaceParams );
        if( FAILED( hr ) )
        {
            ATG::FatalError( "Cannot create depth/stencil tile.\n" );
        }

        // Create the state only command buffers for each depth tile.
        // This will allow for each switching of the depth tiles during
        // the predicated tiling rendering.
        // SetPredication() cannot be used with SetDepthStencilSurface()
        D3DTAGCOLLECTION InheritTags = { 0 };
        D3DTAGCOLLECTION PersistTags = { 0 };
        D3DTagCollection_SetAll( &InheritTags );
        // Persist the states associated with the depth buffer after the command buffer
        // has run.
        D3DTagCollection_Set( &PersistTags, D3DTag_Index(D3DTAG_DEPTHINFO),
                              D3DTag_Mask(D3DTAG_DEPTHINFO) );
        D3DTagCollection_Set( &PersistTags, D3DTag_Index(D3DTAG_DEPTHCONTROL),
                              D3DTag_Mask(D3DTAG_DEPTHCONTROL) );

        // The depth buffer must be non-NULL before calling BeginCommandBuffer
        // with D3DBEGINCB_ONE_PASS_ZPASS.
        m_pCommandBufferDevice->SetDepthStencilSurface( m_pTilingDepthStencil[i] );

        // Create the command buffer to record the SetDepthStencilSurface command
        m_pCommandBufferDevice->CreateCommandBuffer( 1024, 0, &m_pDepthCommandBuffer[i] );
        m_pCommandBufferDevice->BeginCommandBuffer( m_pDepthCommandBuffer[i],
            D3DBEGINCB_ONE_PASS_ZPASS | D3DBEGINCB_TILING_PREDICATE_WHOLE | D3DBEGINCB_RECORD_ALL_SET_STATE | D3DBEGINCB_OVERWRITE_INHERITED_STATE,
            &InheritTags, &PersistTags, m_pTilingRects, m_dwTilingRectCount );
        // The command buffer will record the states for swithing the depth buffer
        // so that it can be replayed during tiling in a SetPredication() bracket
        m_pCommandBufferDevice->SetDepthStencilSurface( m_pTilingDepthStencil[i] );
        m_pCommandBufferDevice->EndCommandBuffer();
    }

    // Record the size of the created rendertarget, and then set up allocation
    // for the next rendertarget right after the end of the first rendertarget.
    m_dwDepthStencilTileSizeBytes += m_pTilingDepthStencil[0]->Size;
    SurfaceParams.Base = m_pFullDepthStencil->Size / GPU_EDRAM_TILE_SIZE;

    // Create a full screen size render target for producing the final image.  This
    // render target will not be used with predicated tiling, and does not require
    // hardware MSAA.  It is also used for the forward renderer
    hr = m_pd3dDevice->CreateRenderTarget( dwFrontBufferWidth,
                                           dwFrontBufferHeight,
                                           ( D3DFORMAT )MAKESRGBFMT( D3DFMT_A8R8G8B8 ),
                                           D3DMULTISAMPLE_NONE,
                                           0, FALSE,
                                           &m_pPostRenderTarget,
                                           &SurfaceParams );
    if( FAILED( hr ) )
    {
        ATG::FatalError( "Cannot create post effects rendertarget.\n" );
    }

    // Create the render targets for the MRT deferred rendering.
    hr = m_pd3dDevice->CreateRenderTarget( dwTileWidth, dwTileHeight,
                                           ( D3DFORMAT )MAKESRGBFMT( D3DFMT_A8R8G8B8 ),
                                           D3DMULTISAMPLE_NONE, 0, FALSE,
                                           &m_pColorRenderTarget, &SurfaceParams );
    if( FAILED( hr ) )
    {
        ATG::FatalError( "Cannot create tiling rendertarget.\n" );
    }

    // Record the size of the created render target, and then set up allocation
    // for the next render target right after the end of the first render target.
    m_dwRenderTargetTileSizeBytes = m_pColorRenderTarget->Size;
    SurfaceParams.Base += m_pColorRenderTarget->Size / GPU_EDRAM_TILE_SIZE;

    hr = m_pd3dDevice->CreateRenderTarget( dwTileWidth, dwTileHeight,
                                           D3DFMT_A8R8G8B8,
                                           D3DMULTISAMPLE_NONE, 0, FALSE,
                                           &m_pNormalsRenderTarget, &SurfaceParams );
    if( FAILED( hr ) )
    {
        ATG::FatalError( "Cannot create tiling rendertarget.\n" );
    }
    m_dwRenderTargetTileSizeBytes += m_pNormalsRenderTarget->Size;
}


//--------------------------------------------------------------------------------------
// Name: RenderScene()
// Desc: Sets some renderstate and draws the robot mesh.
//--------------------------------------------------------------------------------------
VOID Sample::RenderScene(XMMATRIX matWorld)
{
    // Set the renderstates and vertex declaration
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );
    m_pd3dDevice->SetVertexDeclaration( m_pVertexDecl );

    // Optionally force the robot to only render on tile 1.
    // This is for illustratating the different tiles in the sample.
    if( m_bForcedPredication )
    {
        m_pd3dDevice->SetPredication( D3DPRED_TILE( 1 ) );
    }

    // Set the world matrix for the mesh
    m_RobotMesh.m_matWorld = matWorld;

    // Render the robot.
    m_RobotMesh.Render( ATG::MESH_NOTEXTURES | ATG::MESH_NOVERTEXDECL | ATG::MESH_NOFVF );

    // Reset predication to default.
    // This is for illustratating the different tiles in the sample.
    if( m_bForcedPredication )
    {
        m_pd3dDevice->SetPredication( 0 );
    }
}


//--------------------------------------------------------------------------------------
// Name: RenderDeferred()
// Desc: Renders the deferred portion of the scene - the red robot
//--------------------------------------------------------------------------------------
VOID Sample::RenderDeferred()
{
    // Set our tiled render target.
    m_pd3dDevice->SetRenderTarget( 0, m_pColorRenderTarget );
    m_pd3dDevice->SetRenderTarget( 1, m_pNormalsRenderTarget );
    m_pd3dDevice->SetDepthStencilSurface( m_pFullDepthStencil );

    PIXBeginNamedEvent( 0xFFFFFFFF, "Deferred Render Buffers" );

    D3DVECTOR4 ClearColor = { 0, 0, 0, 0 };

    // Clear the additional render targets
    // Render target 0 is cleared with BeginTiling
    m_pd3dDevice->ClearF( D3DCLEAR_TARGET1, &m_pTilingRects[0], &ClearColor, 1.0f, 0L );

    // Begin tiling.  We specify the tile count, the tiling rectangles, and the
    // color and Z/stencil values to clear the tiling rendertarget with for each tile.
    m_pd3dDevice->BeginTiling( D3DTILING_ONE_PASS_ZPASS | D3DTILING_FIRST_TILE_INHERITS_DEPTH_BUFFER,
                               m_dwTilingRectCount, m_pTilingRects,
                               &ClearColor, 1.0f, 0L );

    // Begin Z pass.  The Z pass is a subset of the full scene rendering.
    m_pd3dDevice->BeginZPass( 0 );

    // If we are keeping the full depth buffer in EDRAM
    if( m_bUseFullDepthStencil )
    {
        for( DWORD i = 0 ; i < m_dwTilingRectCount ; ++i )
        {
            // Replay the command buffers to switch depth buffer for each tile.
            // This allows us to wrap a SetDepthStencilSurface() call inside of
            // a SetPredication() block.
            m_pd3dDevice->SetPredication( D3DPRED_TILE(i) );
            m_pd3dDevice->RunCommandBuffer( m_pDepthCommandBuffer[i], 0 );
        }
        m_pd3dDevice->SetPredication(0);
    }

    // Set the world matrix for the mesh
    XMMATRIX matWorld = m_matWorld * XMMatrixTranslation(0, 0, -1);

    // Render the first part of the scene
    float colorRed[4] = { 1.0f, 0.0f, 0.0f, 1.0f };
    m_pd3dDevice->SetVertexShaderConstantF( 12, colorRed, 1 );

    // Render the robot with the deferred rendering shaders
    m_pd3dDevice->SetVertexShader( m_pDeferredRenderVS );
    m_pd3dDevice->SetPixelShader( m_pDeferredRenderPS );
    RenderScene(matWorld);

    // End Z pass.
    m_pd3dDevice->EndZPass();

    for( DWORD i = 0; i < m_dwTilingRectCount ; ++i )
    {
        m_pd3dDevice->SetPredication( D3DPRED_TILE_RENDER(i) );

        // If we are only keeping a single tile of the depth buffer in EDRAM
        if( !m_bUseFullDepthStencil )
        {
            // Resolve the depth buffer to a texture so it can be restored later
            m_pd3dDevice->Resolve( D3DRESOLVE_DEPTHSTENCIL,
                                   NULL, m_pSceneDepthTexture, NULL, 0, 0, NULL, 1.0f, 0L, NULL );
        }

        // Resolve render target 1 and clear it.
        m_pd3dDevice->Resolve( D3DRESOLVE_RENDERTARGET1 | D3DRESOLVE_CLEARRENDERTARGET,
                               NULL, m_pNormalsResolveTexture, NULL,
                               0, 0, &ClearColor, 1.0f, 0L, NULL );
    }
    m_pd3dDevice->SetPredication(0);

    if( m_bUseFullDepthStencil )
    {
        // Set the depth buffer to NULL before calling EndTiling, otherwise the
        // EndTiling() resolve will always clear the depth buffer if there is one.
        // SetSurfaces() allows us to change the surface during predicated tiling
        // by using the D3DSETSURFACES_SET_AS_TILING_SURFACES flag.
        D3DSURFACES surfaces = { NULL, m_pColorRenderTarget, m_pNormalsRenderTarget, NULL, NULL };
	    m_pd3dDevice->SetSurfaces(&surfaces, D3DSETSURFACES_SET_AS_TILING_SURFACES );
    }

    // End tiling.  This will cause the accumulated command buffer to be replayed
    // for each tile, predicated upon the boundaries of each tile.  At the end of
    // each tile's rendering, it will be resolved into a section of the front buffer
    // texture.
    m_pd3dDevice->EndTiling( D3DRESOLVE_RENDERTARGET0 |
                             D3DRESOLVE_ALLFRAGMENTS |
                             D3DRESOLVE_CLEARRENDERTARGET,
                             NULL, m_pColorResolveTexture,
                             &ClearColor, 1.0f, 0L, NULL );

    PIXEndNamedEvent();

    PIXBeginNamedEvent( 0xFFFFFFFF, "Deferred Render Lighting" );

    // Set the full screen rendertarget.
    m_pd3dDevice->SetRenderTarget( 0, m_pPostRenderTarget );
    m_pd3dDevice->SetRenderTarget( 1, NULL );
    m_pd3dDevice->SetDepthStencilSurface( NULL );

    // Render the deferred lighting scene textures back into EDRAM.
    // In this case, we're just doing a simple N*L deferred lighting
    m_pd3dDevice->SetVertexShader( m_pPostProcessVS );
    m_pd3dDevice->SetPixelShader( m_pDeferredLightingPS );
    m_pd3dDevice->SetVertexDeclaration( m_pVertexDeclPost );
    m_pd3dDevice->SetPixelShaderConstantF( 8, ( FLOAT* )&m_vWorldLightDirection, 1 );
    m_pd3dDevice->SetTexture( 0, &m_ColorResolveTextureAs16SRGB );
    m_pd3dDevice->SetTexture( 1, m_pNormalsResolveTexture );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_POINT );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_POINT );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MIPFILTER, D3DTEXF_NONE );
    m_pd3dDevice->SetSamplerState( 1, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP );
    m_pd3dDevice->SetSamplerState( 1, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP );
    m_pd3dDevice->SetSamplerState( 1, D3DSAMP_MAGFILTER, D3DTEXF_POINT );
    m_pd3dDevice->SetSamplerState( 1, D3DSAMP_MINFILTER, D3DTEXF_POINT );
    m_pd3dDevice->SetSamplerState( 1, D3DSAMP_MIPFILTER, D3DTEXF_NONE );

    // 3 2D corners are needed to draw a rect primitive.  The vertex shader will
    // generate proper 4D positions and 2D texture coordinates from this data.
    FLOAT fRectCorners[] = { -1, 1, 1, 1, -1, -1 };
    m_pd3dDevice->DrawPrimitiveUP( D3DPT_RECTLIST,
                                   1,
                                   ( const VOID* )fRectCorners,
                                   2 * sizeof( FLOAT ) );

    PIXEndNamedEvent();
}


//--------------------------------------------------------------------------------------
// Name: FastDepthRestore()
// Desc: Restores the full depth buffer to EDRAM from a texture
//--------------------------------------------------------------------------------------
VOID Sample::FastDepthRestore()
{
    PIXBeginNamedEvent( 0xFFFFFFFF, "Fast Depth Restore" );

    // Render to the 8888 alias of the depth buffer.
    m_pd3dDevice->SetRenderTarget( 0, m_pFullDepthStencilAs8888 );
    m_pd3dDevice->SetTexture( 0, &m_SceneDepthTextureAs8888 );
    m_pd3dDevice->SetPixelShader( m_pFastDepthRestorePS );

    // Turn off Z before restoring the depth to prevent any accidental depth culling.
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, FALSE );

    // 3 2D corners are needed to draw a rect primitive.  The vertex shader will
    // generate proper 4D positions and 2D texture coordinates from this data.
    FLOAT fRectCorners[] = { -1, 1, 1, 1, -1, -1 };

    // Draw a full screen quad to copy the depth data
    m_pd3dDevice->DrawPrimitiveUP( D3DPT_RECTLIST,
                                   1,
                                   ( const VOID* )fRectCorners,
                                   2 * sizeof( FLOAT ) );

    // Update the Hi-Z data by touching all tiles but
    // not overwriting them using D3DRS_ZFUNC = D3DCMP_NEVER
    m_pd3dDevice->SetRenderTarget( 0, NULL );
    m_pd3dDevice->SetDepthStencilSurface( m_pDepthRestore4x );
    m_pd3dDevice->SetPixelShader( NULL );

    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_HIZENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_HIZWRITEENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_ZFUNC, D3DCMP_NEVER );

    m_pd3dDevice->DrawPrimitiveUP( D3DPT_RECTLIST,
                                   1,
                                   ( const VOID* )fRectCorners,
                                   2 * sizeof( FLOAT ) );

    // Restore the depth states
    m_pd3dDevice->SetDepthStencilSurface( m_pFullDepthStencil );
    m_pd3dDevice->SetRenderState( D3DRS_ZFUNC, D3DCMP_LESSEQUAL );
    m_pd3dDevice->SetRenderState( D3DRS_HIZENABLE, TRUE );

    // Flush the Hi-Z data
    // In this case, the Hi-Z contains the one-pass Z data which is conservative for
    // this scene.  If Hi-Z did not contain conservative data, use D3DFHZS_SYNCHRONOUS
    m_pd3dDevice->FlushHiZStencil( D3DFHZS_ASYNCHRONOUS );

    PIXEndNamedEvent();
}


//--------------------------------------------------------------------------------------
// Name: RenderForward()
// Desc: Renders the forward lighting portion of the scene - the transparent white robot
//--------------------------------------------------------------------------------------
VOID Sample::RenderForward()
{
    PIXBeginNamedEvent( 0xFFFFFFFF, "Forward Render" );

    m_pd3dDevice->SetRenderTarget( 0, m_pPostRenderTarget );
    m_pd3dDevice->SetDepthStencilSurface( m_pFullDepthStencil );

    // Set the world matrix for the mesh
    XMMATRIX matWorld = m_matWorld * XMMatrixTranslation(0, 0, 1);

    // Render the second part of the scene
    float colorWhite[4] = { 1.0f, 1.0f, 1.0f, 0.5f };
    m_pd3dDevice->SetVertexShaderConstantF( 12, colorWhite, 1 );
    
    // Enable alpha blending for transparency
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
    m_pd3dDevice->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );
    m_pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE, FALSE );

    // Render the object with a forward lighting renderer and transparency
    m_pd3dDevice->SetVertexShader( m_pForwardRenderVS );
    m_pd3dDevice->SetPixelShader( m_pForwardRenderPS );
    RenderScene(matWorld);

    m_pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );

    PIXEndNamedEvent();
}

//--------------------------------------------------------------------------------------
// Name: RenderUI()
// Desc: Draws some statistics and other UI elements
//--------------------------------------------------------------------------------------
VOID Sample::RenderUI()
{
    PIXBeginNamedEvent( 0xFFFFFFFF, "UI Render" );

    if( m_bDrawHelp )
    {
        m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
    }
    else
    {
        // Draw a title and FPS indicator.
        WCHAR strBuffer[100];
        m_Font.Begin();
        m_Font.SetScaleFactors( 1.2f, 1.2f );
        m_Font.DrawText( 0, 0, 0xffffffff, L"PredicatedTilingFullDepth" );
        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( 0, 0, 0xffffff00, m_Timer.GetFrameRate(), ATGFONT_RIGHT );

        // Draw EDRAM usage stats
        FLOAT sx1 = 16.0f;
        FLOAT sx2 = 200.0f;
        FLOAT sy = 40.0f;

        m_Font.DrawText( sx1, sy, 0xffffffff,
                         (m_bUseFullDepthStencil)? L"Full Depth Buffer in EDRAM" : L"Fast Depth Restore" );
        sy += 25;

        DWORD dwDepthStencilSizeBytes = ( m_bUseFullDepthStencil )? m_dwDepthStencilFullSizeBytes : m_dwDepthStencilTileSizeBytes;

        swprintf_s( strBuffer, L"%d out of %d tiles",
                    ( m_dwRenderTargetTileSizeBytes + dwDepthStencilSizeBytes ) / GPU_EDRAM_TILE_SIZE,
                    GPU_EDRAM_TILES );
        m_Font.DrawText( sx1, sy, 0xffffffff, L"EDRAM usage: " );
        m_Font.DrawText( sx2, sy, 0xffffff00, strBuffer );
        sy += 25;

        swprintf_s( strBuffer, L"(%d color + %d depth/stencil)",
                    m_dwRenderTargetTileSizeBytes / GPU_EDRAM_TILE_SIZE,
                    dwDepthStencilSizeBytes / GPU_EDRAM_TILE_SIZE );
        m_Font.DrawText( sx2, sy, 0xffffff00, strBuffer );
        sy += 25;

        m_Font.End();
    }

    PIXEndNamedEvent();
}


//-----------------------------------------------------------------------------
// Name: Initialize()
// Desc: Initialize app-dependent objects.
//-----------------------------------------------------------------------------
HRESULT Sample::Initialize()
{
    HRESULT hr = S_OK;

    // Set up tiling rectangles and copy them to the tiling rect array.
    // For this sample, the tiles must be horizontal.  That allows the second
    // depth buffer tile to be place adjacent to the first in EDRAM
    const D3DRECT pTilingRects[] =
    {
        { 0,   0, 1280, 384 },
        { 0, 384, 1280, 720 }
    };

    m_dwTilingRectCount = ARRAYSIZE( pTilingRects );
    memcpy( m_pTilingRects, pTilingRects, m_dwTilingRectCount * sizeof( D3DRECT ) );

    m_matWorld = XMMatrixIdentity();
    m_vWorldLightDirection = XMVectorSet( 1, 0, 1, 1 );

    // Aspect ratio computed from the front buffer dimensions.
    FLOAT fAspectRatio = ( FLOAT )m_d3dpp.BackBufferWidth / ( FLOAT )m_d3dpp.BackBufferHeight;

    // Set up projection matrix
    m_matProj = XMMatrixPerspectiveFovLH( XM_PIDIV4, fAspectRatio, 1.0f, 200.0f );

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

    // Create forward rendering vertex and pixel shaders.
    hr = ATG::LoadVertexShader( "game:\\Media\\Shaders\\ForwardRenderVS.xvu", &m_pForwardRenderVS );
    if( FAILED( hr ) )
        ATG::FatalError( "Could not load shader." );
    hr = ATG::LoadPixelShader( "game:\\Media\\Shaders\\ForwardRenderPS.xpu", &m_pForwardRenderPS );
    if( FAILED( hr ) )
        ATG::FatalError( "Could not load shader." );

    // Create deferred rendering vertex and pixel shaders.
    hr = ATG::LoadVertexShader( "game:\\Media\\Shaders\\DeferredRenderVS.xvu", &m_pDeferredRenderVS );
    if( FAILED( hr ) )
        ATG::FatalError( "Could not load shader." );
    hr = ATG::LoadPixelShader( "game:\\Media\\Shaders\\DeferredRenderPS.xpu", &m_pDeferredRenderPS );
    if( FAILED( hr ) )
        ATG::FatalError( "Could not load shader." );

    // Define the vertex elements.
    static const D3DVERTEXELEMENT9 VertexElements[] =
    {
        { 0,  0, D3DDECLTYPE_FLOAT3,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
        { 0, 12, D3DDECLTYPE_FLOAT3,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL,   0 },
        { 0, 24, D3DDECLTYPE_FLOAT2,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
        D3DDECL_END()
    };

    // Create a vertex declaration from the element descriptions.
    m_pd3dDevice->CreateVertexDeclaration( VertexElements, &m_pVertexDecl );

    // Create deferred lighting and fast depth restore vertex and pixel shaders.
    hr = ATG::LoadVertexShader( "game:\\Media\\Shaders\\PostProcessVS.xvu", &m_pPostProcessVS );
    if( FAILED( hr ) )
        ATG::FatalError( "Could not load shader." );
    hr = ATG::LoadPixelShader( "game:\\Media\\Shaders\\DeferredLightingPS.xpu", &m_pDeferredLightingPS );
    if( FAILED( hr ) )
        ATG::FatalError( "Could not load shader." );
    hr = ATG::LoadPixelShader( "game:\\Media\\Shaders\\FastDepthRestorePS.xpu", &m_pFastDepthRestorePS );
    if( FAILED( hr ) )
        ATG::FatalError( "Could not load shader." );

    // Create a vertex declaration for the post effects and fast depth restore
    static const D3DVERTEXELEMENT9 VertexElementsPost[] =
    {
        { 0,  0, D3DDECLTYPE_FLOAT2,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
        D3DDECL_END()
    };

    // Create a vertex declaration from the element descriptions.
    m_pd3dDevice->CreateVertexDeclaration( VertexElementsPost, &m_pVertexDeclPost );

    // Create a command buffer device for recording the depth buffer switch commands
    Direct3D_CreateDevice( 0, D3DDEVTYPE_COMMAND_BUFFER, NULL, 0, NULL, &m_pCommandBufferDevice );

    LoadResources();

    // Set up the screen extents query mode that tiling will use.
    m_pd3dDevice->SetScreenExtentQueryMode( D3DSEQM_CULLED );

    return S_OK;
}


//-----------------------------------------------------------------------------
// Name: Update()
// Desc: Called once per frame, the call is the entry point for animating
//       the scene.
//-----------------------------------------------------------------------------
HRESULT Sample::Update()
{
    // Update FPS in the timer class.
    m_Timer.MarkFrame();

    // Get input from controllers (check for exit button sequence).
    ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput();

    // The Back button toggles the help screen.
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
        m_bDrawHelp = !m_bDrawHelp;

    // The A button cycles the scenario.
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_A )
        m_bUseFullDepthStencil = !m_bUseFullDepthStencil;

    // The Y button toggles forced predication.
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_Y )
        m_bForcedPredication = !m_bForcedPredication;

    // The X button changes camera distance.
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_X )
    {
        if( m_fEyeRadius == 8.0f )
            m_fEyeRadius = 4.0f;
        else
            m_fEyeRadius = 8.0f;
    }

    // The B button toggles rotation.
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_B )
        m_bRotating = !m_bRotating;

    // View matrix
    FLOAT fTime = ( FLOAT )m_Timer.GetAppTime();
    if( !m_bRotating )
        fTime = -XM_PIDIV2 / 0.3f;
    XMVECTOR vEyePt = XMVectorSet( m_fEyeRadius, 0.0f, 0.0f, 0.0f );
    XMVECTOR vLookatPt = XMVectorSet( 0.0f, -0.2f, 0.0f, 0.0f );
    XMVECTOR vUp = XMVectorSet( 0.0f, 1.0f, 0.0f, 0.0f );
    m_matView = XMMatrixLookAtLH( vEyePt, vLookatPt, vUp );

    // World matrix
    m_matWorld = XMMatrixRotationY( 0.3f * fTime );

    // Update robot mesh constants.
    m_RobotMesh.m_matWorld = m_matWorld;
    m_RobotMesh.m_matViewProj = m_matView * m_matProj;
    m_RobotMesh.m_vWorldLightDirection = m_vWorldLightDirection;

    return S_OK;
}


//-----------------------------------------------------------------------------
// Name: Render()
// Desc: Called once per frame, to render the scene.
//-----------------------------------------------------------------------------
HRESULT Sample::Render()
{
    // Render the first red robot with deferred lighting
    RenderDeferred();

    // If we are not using the full depth buffer trick, then the depth buffer
    // must be restored to EDRAM for use in the second rendering phase.
    if( !m_bUseFullDepthStencil )
    {
        FastDepthRestore();
    }

    // Render the second white robot with forward lighting
    RenderForward();

    // Render UI.
    RenderUI();

    // Synchronize to presentation interval before the final resolve, so we do
    // not see tearing at 30Hz or 60Hz presentation intervals.
    m_pd3dDevice->SynchronizeToPresentationInterval();

    D3DVECTOR4 ClearColor = { 0, 0, 0, 0 };

    // Resolve the rendered scene back to the front buffer.
    m_pd3dDevice->Resolve( D3DRESOLVE_RENDERTARGET0 | D3DRESOLVE_CLEARRENDERTARGET,
                           NULL,
                           m_pFrontBufferTexture,
                           NULL,
                           0, 0,
                           &ClearColor, 1.0f, 0, NULL );

    // Swap to the current front buffer, so we can see it on screen.
    m_pd3dDevice->Swap( m_pFrontBufferTexture, NULL );

    return S_OK;
}
