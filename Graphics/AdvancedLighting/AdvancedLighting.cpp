//--------------------------------------------------------------------------------------
// AdvancedLighting.cpp
//
// This sample demonstrates the use of reflective shadowmaps to calculate one bounce of
// indirect lighting for dynamic scenes.
//
// The sample runs at 1280x720 with 4x multisampling at more than 100fps with a complex
// lighting scene, and encourages game developers to implement dynamic indirect lighting
// to drastically enhance the visual look of games.
//
// Reflective shadowmaps are used at runtime, so no asset pipeline changes are needed, no
// off-line precalculations are needed, almost no extra CPU cost is added and it's easy to
// implement a LOD system for complex scenes since the bulk of the work is done per vertex.
//
// This sample specifically implements the calculation of indirect lighting in a forward
// renderer, not a deferred renderer as all the papers do. The forward rendering implementation
// is almost twice as fast, gives better visual results and doesn't need to deal with the usual
// deferred rendering problems, i.e. multisampling, alpha blending, etc.
//
// This sample is based on the following papers:
//      "Instant radiosity", Keller, 1997
//      "Sampling with Hammersley and Halton Points", Wong et al., 1997
//      "Reflective shadow maps", Dachsbacher and Stamminger, 2005
//      "Splatting indirect illumination", Dachsbacher and Stamminger, 2006
//      "Second-order illumination in real-time", Cochran and Steele, 2007
//      "Incremental wavelet importance sampling for direct illumination", Huang et al., 2007
//      "Incremental instant radiosity for real-time indirect illumination", Laine et al., 2007
//
// Special thanks to Marko Dabrovic for the original Sponza Atrium model and Matt Collins for helping
// with the generation of normalmaps.
//
// XNA Developer Connection.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "AdvancedLighting.h"

#include <xtl.h>
#include <xgraphics.h>
#include <xboxmath.h>
#include <AtgFont.h>
#include <AtgHelp.h>
#include <AtgInput.h>
#include <AtgUtil.h>
#include <AtgApp.h>
#include <AtgSimpleShaders.h>
#include <AtgDebugDraw.h>
#include <AtgSceneAll.h>
#include <AtgPostProcess.h>
#include "CVarianceShadowMap.h"
#include "CReflectiveShadowMap.h"


//--------------------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//--------------------------------------------------------------------------------------
ATG::HELP_CALLOUT g_HelpCallouts[] =
{
    { ATG::HELP_A_BUTTON,       ATG::HELP_PLACEMENT_1, L"Switch Lighting Mode forward" },
    { ATG::HELP_B_BUTTON,       ATG::HELP_PLACEMENT_1, L"Switch Lighting Mode backward" },
    { ATG::HELP_X_BUTTON,       ATG::HELP_PLACEMENT_1, L"Switch Debug Render Mode forward" },
    { ATG::HELP_Y_BUTTON,       ATG::HELP_PLACEMENT_1, L"Switch Debug Render Mode backward" },
    { ATG::HELP_DPAD,           ATG::HELP_PLACEMENT_1, L"Rotate light" },
    { ATG::HELP_LEFT_STICK,     ATG::HELP_PLACEMENT_1, L"Move camera" },
    { ATG::HELP_RIGHT_STICK,    ATG::HELP_PLACEMENT_1, L"Rotate camera" },
    { ATG::HELP_START_BUTTON,   ATG::HELP_PLACEMENT_1, L"Toggle Flicker Reduction" },
    { ATG::HELP_BACK_BUTTON,    ATG::HELP_PLACEMENT_1, L"Display help" },
};

#define NUM_HELP_CALLOUTS ( sizeof( g_HelpCallouts ) / sizeof( g_HelpCallouts[ 0 ] ) )


// Our scene's world scale
const FLOAT g_fWorldScale = 32.0f;

// Lighting modes. The sample depends on the order of the definitions for this enum.
enum LIGHTINGMODE
{
    LIGHTINGMODE_PERVERTEX = 0,
    LIGHTINGMODE_PERPIXEL,
    LIGHTINGMODE_SHADOWED,
    LIGHTINGMODE_INDIRECT_PERVERTEX_SELFOCCLUDE,
    LIGHTINGMODE_INDIRECT_PERVERTEX,
    LIGHTINGMODE_INDIRECT_PERPIXEL,
    LIGHTINGMODE_NUMMODES,
};

// Render modes. The sample depends on the order of the definitions for this enum.
enum RENDERMODE
{
    RENDERMODE_NORMAL = 0,
    RENDERMODE_NOLIGHTING,
    RENDERMODE_DIRECTLIGHTING,
    RENDERMODE_INDIRECTLIGHTING,
    RENDERMODE_LIGHTING,
    RENDERMODE_RSM_POSITIONS,
    RENDERMODE_RSM_NORMALS,
    RENDERMODE_RSM_FLUX,
    RENDERMODE_NUMMODES,
};


//--------------------------------------------------------------------------------------
// Name: class Sample
// Desc: Main class for the AdvancedLighting sample.
//--------------------------------------------------------------------------------------
class Sample : public ATG::Application
{
public:
    HRESULT Initialize();
    HRESULT Update();
    HRESULT Render();

private:
    HRESULT CreateTextures();
    HRESULT CreateRenderTargets();
    HRESULT RenderShadowmap();
    HRESULT RenderBackBuffer();
    HRESULT PreRender();
    HRESULT PostRender();

    VOID    RenderScene( const BOOL bRenderShadowPass );
    VOID    RenderSkyDome();
    VOID    RenderDebugTextures();
    VOID    RenderUI();
    VOID    SetGlobalShaderConstants();

private:
    // Shaders for per vertex lighting
    IDirect3DVertexShader9* m_pShadeScenePerVertexVS;
    IDirect3DPixelShader9* m_pShadeScenePerVertexPS;
    IDirect3DPixelShader9* m_pShadeScenePerVertexDebugPS;

    // Shaders for per pixel lighting
    IDirect3DVertexShader9* m_pShadeScenePerPixelVS;
    IDirect3DPixelShader9* m_pShadeScenePerPixelPS;
    IDirect3DPixelShader9* m_pShadeScenePerPixelDebugPS;

    // Shaders for per pixel lighting with a variance shadowmap
    IDirect3DVertexShader9* m_pShadeSceneShadowedVS;
    IDirect3DPixelShader9* m_pShadeSceneShadowedPS;
    IDirect3DPixelShader9* m_pShadeSceneShadowedDebugPS;

    // Shaders for per pixel lighting with variance shadowmap and per vertex indirect ligthing
    IDirect3DVertexShader9* m_pShadeSceneIndirectPerVertexVS;
    IDirect3DPixelShader9* m_pShadeSceneIndirectPerVertexPS;
    IDirect3DPixelShader9* m_pShadeSceneIndirectPerVertexDebugPS;
    IDirect3DVertexShader9* m_pShadeSceneIndirectPerVertexSelfOccludeVS;

    // Shaders for per pixel lighting with variance shadowmap and per pixel indirect ligthing
    IDirect3DPixelShader9* m_pShadeSceneIndirectPerPixelPS;
    IDirect3DPixelShader9* m_pShadeSceneIndirectPerPixelDebugPS;

    // Shaders for simple skydome
    IDirect3DVertexShader9* m_pShadeSkyVS;
    IDirect3DPixelShader9* m_pShadeSkyPS;

    // Textures
    D3DTexture* m_pFrontBufferTexture;
    D3DTexture* m_pBackBufferTexture;
    D3DTexture  m_BackBufferTextureAs16SRGB;

    // Render targets
    D3DSurface* m_pTiledBackBufferRT;
    D3DSurface* m_pTiledDepthStencilSurface;
    D3DSurface* m_pBackBufferRT;

    // Sample resources
    ATG::PackedResource m_pResource;
    ATG::Scene* m_pScene;
    ATG::Scene* m_pSkyDome;

    // Sample framework
    ATG::Font m_Font;
    ATG::Help m_Help;
    ATG::PostProcess m_PostProcess;
    ATG::Timer m_Timer;
    FLOAT m_fElapsedTime;

    // Sample options
    BOOL m_bDrawHelp;
    BOOL m_bReduceFlicker;
    LIGHTINGMODE m_LightingMode;
    RENDERMODE m_RenderMode;
    D3DMULTISAMPLE_TYPE m_MultiSample;

    // Tiling rectangles
    D3DRECT m_pTilingRects[ 3 ];
    INT m_iTilingRectCount;

    // Camera variables
    FLOAT m_fAspectRatio;
    XMMATRIX m_matWorld;
    XMMATRIX m_matCameraView;
    XMMATRIX m_matCameraProj;
    XMVECTOR m_vCameraPosition;
    XMVECTOR m_vLookatDir;
    XMVECTOR m_vUp;

    // Light variables
    XMVECTOR m_vLightDirection;
    XMVECTOR m_vLightPosition;
    XMMATRIX m_matLightView;
    XMMATRIX m_matLightProj;

    // Variance Shadowmap variables
    FLOAT m_fEpsilonVSM;
    CVarianceShadowMap m_VarianceShadowMap;

    // Reflective Shadowmap variables
    CReflectiveShadowMap m_ReflectiveShadowMap;

    // Depending on the current lighting mode, we'll either use a variance shadowmap
    // or a reflective shadowmap. This variable holds the pointer to the shadowmap we
    // we are currently using.
    CVarianceShadowMap* m_pShadowMap;

};


//--------------------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the sample
//--------------------------------------------------------------------------------------
INT __cdecl main()
{
    Sample sample;

    // We create our own rendertargets and depthbuffer
    sample.m_d3dpp.BackBufferWidth = 1280;
    sample.m_d3dpp.BackBufferHeight = 720;
    sample.m_d3dpp.EnableAutoDepthStencil = FALSE;
    sample.m_d3dpp.DisableAutoBackBuffer = TRUE;
    sample.m_d3dpp.DisableAutoFrontBuffer = TRUE;
    sample.m_d3dpp.FrontBufferFormat = ( D3DFORMAT )MAKESRGBFMT( D3DFMT_LE_X8R8G8B8 );
    sample.m_dwDeviceCreationFlags |= D3DCREATE_BUFFER_2_FRAMES;

    sample.Run();
}


//--------------------------------------------------------------------------------------
// Name: Initialize()
// Desc: Loads and initialize shaders, resources, etc.
//--------------------------------------------------------------------------------------
HRESULT Sample::Initialize()
{
    // Load shaders
    RETURN_ON_FAIL( ATG::LoadVertexShader( "game:\\Media\\Shaders\\ShadeScenePerVertex.xvu", &m_pShadeScenePerVertexVS ) );
    RETURN_ON_FAIL( ATG::LoadPixelShader( "game:\\Media\\Shaders\\ShadeScenePerVertex.xpu", &m_pShadeScenePerVertexPS ) );
    RETURN_ON_FAIL( ATG::LoadPixelShader( "game:\\Media\\Shaders\\ShadeScenePerVertexDebug.xpu", &m_pShadeScenePerVertexDebugPS ) );

    RETURN_ON_FAIL( ATG::LoadVertexShader( "game:\\Media\\Shaders\\ShadeScenePerPixel.xvu", &m_pShadeScenePerPixelVS ) );
    RETURN_ON_FAIL( ATG::LoadPixelShader( "game:\\Media\\Shaders\\ShadeScenePerPixel.xpu", &m_pShadeScenePerPixelPS ) );
    RETURN_ON_FAIL( ATG::LoadPixelShader( "game:\\Media\\Shaders\\ShadeScenePerPixelDebug.xpu", &m_pShadeScenePerPixelDebugPS ) );

    RETURN_ON_FAIL( ATG::LoadVertexShader( "game:\\Media\\Shaders\\ShadeSceneShadowed.xvu", &m_pShadeSceneShadowedVS ) );
    RETURN_ON_FAIL( ATG::LoadPixelShader( "game:\\Media\\Shaders\\ShadeSceneShadowed.xpu", &m_pShadeSceneShadowedPS ) );
    RETURN_ON_FAIL( ATG::LoadPixelShader( "game:\\Media\\Shaders\\ShadeSceneShadowedDebug.xpu", &m_pShadeSceneShadowedDebugPS ) );

    RETURN_ON_FAIL( ATG::LoadVertexShader( "game:\\Media\\Shaders\\ShadeSceneIndirectPerVertex.xvu", &m_pShadeSceneIndirectPerVertexVS ) );
    RETURN_ON_FAIL( ATG::LoadPixelShader( "game:\\Media\\Shaders\\ShadeSceneIndirectPerVertex.xpu", &m_pShadeSceneIndirectPerVertexPS ) );
    RETURN_ON_FAIL( ATG::LoadPixelShader( "game:\\Media\\Shaders\\ShadeSceneIndirectPerVertexDebug.xpu", &m_pShadeSceneIndirectPerVertexDebugPS ) );
    RETURN_ON_FAIL( ATG::LoadVertexShader( "game:\\Media\\Shaders\\ShadeSceneIndirectPerVtxSelfOcclude.xvu", &m_pShadeSceneIndirectPerVertexSelfOccludeVS ) );

    RETURN_ON_FAIL( ATG::LoadPixelShader( "game:\\Media\\Shaders\\ShadeSceneIndirectPerPixel.xpu", &m_pShadeSceneIndirectPerPixelPS ) );
    RETURN_ON_FAIL( ATG::LoadPixelShader( "game:\\Media\\Shaders\\ShadeSceneIndirectPerPixelDebug.xpu", &m_pShadeSceneIndirectPerPixelDebugPS ) );

    RETURN_ON_FAIL( ATG::LoadVertexShader( "game:\\Media\\Shaders\\ShadeSky.xvu", &m_pShadeSkyVS ) );
    RETURN_ON_FAIL( ATG::LoadPixelShader( "game:\\Media\\Shaders\\ShadeSky.xpu", &m_pShadeSkyPS ) );

    // Create textures and rendertargets
    RETURN_ON_FAIL( CreateTextures() );
    RETURN_ON_FAIL( CreateRenderTargets() );

    // Initialize postprocessing and shadowmaps
    RETURN_ON_FAIL( m_PostProcess.Initialize() );
    RETURN_ON_FAIL( m_VarianceShadowMap.Initialize( &m_PostProcess ) );
    RETURN_ON_FAIL( m_ReflectiveShadowMap.Initialize( &m_PostProcess ) );

    // Create font and confine text drawing to the title safe area
    RETURN_ON_FAIL( m_Font.Create( "game:\\Media\\Fonts\\Arial_16.xpr" ) );
    m_Font.SetWindow( ATG::GetTitleSafeArea() );

    // Create help screen.
    RETURN_ON_FAIL( m_Help.Create( "game:\\Media\\Help\\Help.xpr" ) );

    // Create the resources
    RETURN_ON_FAIL( m_pResource.Create( "game:\\Media\\Resource.xpr" ) );

    // Create and load SponzaAtrium scene
    m_pScene = new ATG::Scene();
    assert( m_pScene );
    m_pScene->GetResourceDatabase()->AddBundledResources( &m_pResource );
    RETURN_ON_FAIL( ATG::SceneFileParser::LoadXATGFile( "game:\\media\\scenes\\SponzaAtrium.xatg", m_pScene, NULL,
     ATG::XATGLOADER_DONOTINITIALIZEMATERIALS, NULL ) );

    // Create and load skydome scene
    m_pSkyDome = new ATG::Scene();
    assert( m_pSkyDome );
    m_pSkyDome->GetResourceDatabase()->AddBundledResources( &m_pResource );
    RETURN_ON_FAIL( ATG::SceneFileParser::LoadXATGFile( "game:\\media\\scenes\\SkyDome.xatg", m_pSkyDome, NULL,
     ATG::XATGLOADER_DONOTINITIALIZEMATERIALS, NULL ) );

    // Initialize simple shaders used in the DebugDraw class.
    ATG::SimpleShaders::Initialize( NULL, NULL );

    // Set the view matrix with our starting camera position and direction
    m_vCameraPosition = XMVectorSet( 15.3775139, 7.30931854, -1.50854290, 0.0f );
    m_vLookatDir = XMVectorSet( -0.96f, -0.05f, 0.27f, 0.0f );
    m_vUp = XMVectorSet( 0.0f, 1.0f, 0.0f, 0.0f );
    m_matCameraView = XMMatrixLookAtLH( m_vCameraPosition, m_vCameraPosition + m_vLookatDir, m_vUp );
    m_matWorld = XMMatrixIdentity();

    // Set up projection matrix
    const FLOAT fZNear = 0.1f;
    const FLOAT fZFar = 100.0f;
    m_fAspectRatio = ( FLOAT )m_d3dpp.BackBufferWidth / ( FLOAT )m_d3dpp.BackBufferHeight;
    m_matCameraProj = XMMatrixPerspectiveFovLH( XM_PI / 2.5f, m_fAspectRatio, fZNear, fZFar );

    m_bDrawHelp = FALSE;
    m_bReduceFlicker = TRUE;
    m_LightingMode = LIGHTINGMODE_INDIRECT_PERVERTEX;
    m_RenderMode = RENDERMODE_NORMAL;
    m_pShadowMap = &m_ReflectiveShadowMap;
    m_fEpsilonVSM = 0.005f;
    m_vLightDirection = XMVectorSet( 0.0568098351, 0.918891609, -0.373118997, 0.0f );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: CreateTextures()
// Desc: Creates the textures needed for this sample
//--------------------------------------------------------------------------------------
HRESULT Sample::CreateTextures()
{
    DWORD dwWidth = m_d3dpp.BackBufferWidth;
    DWORD dwHeight = m_d3dpp.BackBufferHeight;

    // Create front buffer texture.
    RETURN_ON_FAIL( m_pd3dDevice->CreateTexture( dwWidth, dwHeight, 1, 0, ( D3DFORMAT )MAKESRGBFMT( D3DFMT_LE_X8R8G8B8 ),
     D3DPOOL_DEFAULT, &m_pFrontBufferTexture, NULL ) );

    // Create backbuffer textures.
    RETURN_ON_FAIL( m_pd3dDevice->CreateTexture( dwWidth, dwHeight, 1, 0, ( D3DFORMAT )MAKESRGBFMT( D3DFMT_A8R8G8B8 ),
     D3DPOOL_DEFAULT, &m_pBackBufferTexture, NULL ) );

    // Alias the backbuffer texture to an sRGB AS_16 format texture, so we can sample from it without losing any
    // precision.
    m_BackBufferTextureAs16SRGB = *m_pBackBufferTexture;
    ATG::ConvertTextureToAs16SRGBFormat( &m_BackBufferTextureAs16SRGB );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: CreateRenderTargets()
// Desc: Creates all of the render targets needed for this sample
//--------------------------------------------------------------------------------------
HRESULT Sample::CreateRenderTargets()
{
    m_MultiSample = D3DMULTISAMPLE_4_SAMPLES;

    // Set up tiling rectangles and copy them to the tiling rect array.
    const D3DRECT pTilingRects[] =
    {
        { 0,   0, 1280, 256 },
        { 0, 256, 1280, 512 },
        { 0, 512, 1280, 720 }
    };

    m_iTilingRectCount = ARRAYSIZE( pTilingRects );

    memcpy( m_pTilingRects, pTilingRects, m_iTilingRectCount * sizeof( D3DRECT ) );

    // Compute tile width and height.  The tiling render targets will be created using
    // these dimensions.
    DWORD dwTileWidth = m_pTilingRects[ 0 ].x2;
    DWORD dwTileHeight = m_pTilingRects[ 0 ].y2;

    // Expand tile surface dimensions to texture tile size
    dwTileWidth = XGNextMultiple( dwTileWidth, GPU_TEXTURE_TILE_DIMENSION );
    dwTileHeight = XGNextMultiple( dwTileHeight, GPU_TEXTURE_TILE_DIMENSION );

    // Use custom EDRAM allocation to create the render targets.
    // The first render target is placed at address 0 in EDRAM.
    D3DSURFACE_PARAMETERS TileSurfaceParams = { 0 };

    RETURN_ON_FAIL( m_pd3dDevice->CreateRenderTarget( dwTileWidth, dwTileHeight, ( D3DFORMAT )MAKESRGBFMT( D3DFMT_A8R8G8B8 ),
                                                          m_MultiSample, 0, FALSE,
     &m_pTiledBackBufferRT, &TileSurfaceParams ) );

    // Record the size of the created render target, and then set up allocation
    // for the next render target right after the end of the first render target.
    TileSurfaceParams.Base = XGSurfaceSize( dwTileWidth, dwTileHeight, ( D3DFORMAT )MAKESRGBFMT( D3DFMT_A8R8G8B8 ), m_MultiSample );

    // Put the hierarchical Z buffer at the start of hierarchical Z memory.
    TileSurfaceParams.HierarchicalZBase = 0;

    // Create floating point depth target, so remeber to reverse viewport near and far, and depth compare function
    RETURN_ON_FAIL( m_pd3dDevice->CreateDepthStencilSurface( dwTileWidth, dwTileHeight, D3DFMT_D24FS8,
                                                                 m_MultiSample, 0, FALSE,
     &m_pTiledDepthStencilSurface, &TileSurfaceParams ) );

    // Create a full screen size render target for producing the final image.  This
    // render target will not be used with predicated tiling, and does not require
    // hardware MSAA.
    RETURN_ON_FAIL( m_pd3dDevice->CreateRenderTarget( m_d3dpp.BackBufferWidth, m_d3dpp.BackBufferHeight,
                                                          ( D3DFORMAT )MAKESRGBFMT( D3DFMT_A8R8G8B8 ), D3DMULTISAMPLE_NONE, 0, FALSE,
     &m_pBackBufferRT, NULL ) );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Update()
// Desc: Processes input and updates rendering variables
//--------------------------------------------------------------------------------------
HRESULT Sample::Update()
{
    PIXBeginNamedEvent( 0, __FUNCTION__ );

    // Update FPS in the timer class.
    m_Timer.MarkFrame();

    // Calculate the elapsed time and clamp it to to 30fps
    m_fElapsedTime = ( FLOAT )m_Timer.GetElapsedTime();
    m_fElapsedTime = min( 1.0f / 30.0f, m_fElapsedTime );

    // Get input from controllers
    ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput();

    // The Back button toggles the help screen.
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
    {
        m_bDrawHelp = !m_bDrawHelp;
    }

    // The Start button toggles the flicker reduction mode on/off.
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_START )
    {
        m_bReduceFlicker = !m_bReduceFlicker;
    }

    // The A button toggles the lighting mode forward.
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_A )
    {
        m_LightingMode = ( LIGHTINGMODE )( ( m_LightingMode + 1 ) % LIGHTINGMODE_NUMMODES );
    }

    // The B button toggles the lighting mode backwards.
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_B )
    {
        m_LightingMode = ( LIGHTINGMODE )( ( m_LightingMode + LIGHTINGMODE_NUMMODES - 1 ) % LIGHTINGMODE_NUMMODES );
    }

    // Set the current shadow method based on the selected lighting mode
    if( m_LightingMode >= LIGHTINGMODE_INDIRECT_PERVERTEX_SELFOCCLUDE )
    {
        m_pShadowMap = &m_ReflectiveShadowMap;
    }
    else
    {
        m_pShadowMap = &m_VarianceShadowMap;
    }

    // The X button toggles the render mode forward.
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_X )
    {
        m_RenderMode = ( RENDERMODE )( ( m_RenderMode + 1 ) % RENDERMODE_NUMMODES );
    }

    // The Y button toggles the render mode backwards.
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_Y )
    {
        m_RenderMode = ( RENDERMODE )( ( m_RenderMode + RENDERMODE_NUMMODES - 1 ) % RENDERMODE_NUMMODES );
    }

    // Set the view matrix form the left and right sticks
    static FLOAT fTheta = 0.107840881;
    static FLOAT fPhi = -7.60428810;

    fPhi += pGamepad->fX2 * m_fElapsedTime * 0.3f * XM_PI;
    fTheta += pGamepad->fY2 * m_fElapsedTime * 0.3f * XM_PI;

    m_vLookatDir.x = cosf( fTheta ) * sinf( fPhi );
    m_vLookatDir.y = sinf( fTheta );
    m_vLookatDir.z = cosf( fTheta ) * cosf( fPhi );

    XMVECTOR vCrossDir;
    vCrossDir.x = cosf( fPhi );
    vCrossDir.y = 0.0;
    vCrossDir.z = -sinf( fPhi );

    XMVECTOR vOldCameraPosition = m_vCameraPosition;
    m_vCameraPosition += m_vLookatDir * pGamepad->fY1 * m_fElapsedTime * 5.0f;
    m_vCameraPosition += vCrossDir * pGamepad->fX1 * m_fElapsedTime * 5.0f;

    // Constrain the camera position within our world size. This is just a very simple
    // collision detection to keep the camera from going outside the building.
    if( m_vCameraPosition.x > 16 )
        m_vCameraPosition.x = 16;
    if( m_vCameraPosition.x < -16 )
        m_vCameraPosition.x = -16;
    if( m_vCameraPosition.y > 8.5 )
        m_vCameraPosition.y = 8.5;
    if( m_vCameraPosition.y < 4.9 )
        m_vCameraPosition.y = 4.9;
    if( m_vCameraPosition.z > 7 )
        m_vCameraPosition.z = 7;
    if( m_vCameraPosition.z < -7 )
        m_vCameraPosition.z = -7;

    if( ( m_vCameraPosition.x < 11.75 ) && ( m_vCameraPosition.x > -11.75 ) &&
        ( m_vCameraPosition.z < 3.4 ) && ( m_vCameraPosition.z > -3.4 ) )
    {
        m_vCameraPosition = vOldCameraPosition;
    }

    // Update the camera view matrix
    m_matCameraView = XMMatrixLookAtLH( m_vCameraPosition, m_vCameraPosition + m_vLookatDir, m_vUp );

    // Rotate the light direction based on the dpad
    FLOAT fMoveLightX = ( pGamepad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT ) ? -1.0f :
        ( pGamepad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT ) ? 1.0f : 0.0f;
    FLOAT fMoveLightY = ( pGamepad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN ) ? -1.0f :
        ( pGamepad->wButtons & XINPUT_GAMEPAD_DPAD_UP ) ? 1.0f : 0.0f;
    XMMATRIX matRotate = XMMatrixRotationAxis( m_vUp, fMoveLightX * m_fElapsedTime );
    m_vLightDirection = XMVector3TransformNormal( m_vLightDirection, matRotate );

    // Place limits so we don't go over the top or under the bottom
    FLOAT dot = XMVector3Dot( m_vLightDirection, m_vUp ).x;

    if( ( dot > 0.875f || fMoveLightY > 0.0f ) &&
        ( dot < 0.975f || fMoveLightY < 0.0f ) )
    {
        XMVECTOR vAxis = XMVector3Cross( m_vLightDirection, m_vUp );
        matRotate = XMMatrixRotationAxis( vAxis, fMoveLightY * m_fElapsedTime );
        m_vLightDirection = XMVector3TransformNormal( m_vLightDirection, matRotate );
    }

    // Compute the light view and light projection matrices.
    const FLOAT fWorldSize = g_fWorldScale + 2.0f;
    const FLOAT fRadius = sqrt( ( fWorldSize * 0.5f ) * ( fWorldSize * 0.5f ) * 2.0f );
    const FLOAT fNear = 0.01f;
    const FLOAT fFar = fNear + fRadius * 2.0f;

    // Calculate light orientation
    m_vLightPosition = m_vLightDirection * ( fRadius + fNear );
    XMVECTOR vTo = XMVectorSet( 0.0f, 0.0f, 0.0f, 1.0f );
    XMVECTOR vUp = XMVectorSet( 0.0f, 1.0f, 0.0001f, 0.0f );
    m_matLightView = XMMatrixLookAtLH( m_vLightPosition, vTo, vUp );

    // Include the entire world in the orthographic projection.
    m_matLightProj = XMMatrixOrthographicLH( fRadius * 2.0f, fRadius * 2.0f, fNear, fFar );

    PIXEndNamedEvent(); // Sample::Update

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Render()
// Desc: Renders the scene
//--------------------------------------------------------------------------------------
HRESULT Sample::Render()
{
    PIXBeginNamedEvent( 0, __FUNCTION__ );

    // Setup shader constants
    SetGlobalShaderConstants();

    // Render the shadowmap
    RenderShadowmap();

    // Render the backbuffer
    RenderBackBuffer();

    // Render debug textures if needed
    RenderDebugTextures();

    // Render UI and info
    RenderUI();

    // Wait for the vertical blank before we resolve to the front buffer to avoid tearing.
    m_pd3dDevice->SynchronizeToPresentationInterval();

    // Resolve the final image to the front buffer.
    m_pd3dDevice->Resolve( D3DRESOLVE_RENDERTARGET0, NULL, m_pFrontBufferTexture, NULL, 0, 0, NULL, 1.0f, 0, NULL );

    // Present the scene.
    m_pd3dDevice->Swap( m_pFrontBufferTexture, NULL );

    PIXEndNamedEvent(); // Sample::Render

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: SetGlobalShaderConstants()
// Desc: Setup most of the shader constants for the sample
//--------------------------------------------------------------------------------------
VOID Sample::SetGlobalShaderConstants()
{
    XMMATRIX matWorld = XMMatrixTranspose( m_matWorld );
    XMMATRIX matCameraWVP = XMMatrixTranspose( m_matWorld * m_matCameraView * m_matCameraProj );
    XMMATRIX matLightWVP = XMMatrixTranspose( m_matWorld * m_matLightView * m_matLightProj );

    // The light view matrix for sampling from the shadow texture. Rather do this calculation
    // on the CPU than for every vertex in the vertex shader
    XMMATRIX matSampleLightWVP = m_matWorld * m_matLightView * m_matLightProj;

    FLOAT fShadowMapSize = ( FLOAT )m_pShadowMap->GetShadowMapSize();
    FLOAT fOffsetX = 0.5f + ( 0.5f / fShadowMapSize );
    FLOAT fOffsetY = 0.5f + ( 0.5f / fShadowMapSize );

    XMMATRIX matScaleOffset( 0.5f,     0.0f,      0.0f,  0.0f,
                             0.0f,    -0.5f,      0.0f,  0.0f,
                             0.0f,     0.0f,      1.0f,  0.0f,
                             fOffsetX, fOffsetY,  0.0f,  1.0f );

    matSampleLightWVP = XMMatrixTranspose( matSampleLightWVP * matScaleOffset );

    m_pd3dDevice->SetVertexShaderConstantF( VSCONST_matCameraWVP, ( FLOAT* )&matCameraWVP, 4 );
    m_pd3dDevice->SetVertexShaderConstantF( VSCONST_matLightWVP, ( FLOAT* )&matLightWVP, 4 );
    m_pd3dDevice->SetVertexShaderConstantF( VSCONST_matSampleLightWVP, ( FLOAT* )&matSampleLightWVP, 4 );
    m_pd3dDevice->SetVertexShaderConstantF( VSCONST_matWorld, ( FLOAT* )&matWorld, 4 );
    m_pd3dDevice->SetVertexShaderConstantF( VSCONST_vWorldSpaceLightDirection, ( FLOAT* )&m_vLightDirection, 1 );
    m_pd3dDevice->SetVertexShaderConstantF( VSCONST_vWorldSpaceCameraPosition, ( FLOAT* )&m_vCameraPosition, 1 );

    m_pd3dDevice->SetPixelShaderConstantF( PSCONST_matLightWVP, ( FLOAT* )&matLightWVP, 4 );
    m_pd3dDevice->SetPixelShaderConstantF( PSCONST_matSampleLightWVP, ( FLOAT* )&matSampleLightWVP, 4 );
    m_pd3dDevice->SetPixelShaderConstantF( PSCONST_vWorldSpaceLightPos, ( FLOAT* )&m_vLightPosition, 1 );
    m_pd3dDevice->SetPixelShaderConstantF( PSCONST_vWorldSpaceLightDirection, ( FLOAT* )&m_vLightDirection, 1 );
    m_pd3dDevice->SetPixelShaderConstantF( PSCONST_vWorldSpaceCameraPosition, ( FLOAT* )&m_vCameraPosition, 1 );

    BOOL bDebugShowNoLighting = ( m_RenderMode == RENDERMODE_NOLIGHTING );
    BOOL bDebugShowDirectLighting = ( m_RenderMode == RENDERMODE_LIGHTING ) ||
        ( m_RenderMode == RENDERMODE_DIRECTLIGHTING );
    BOOL bDebugShowIndirectLighting = ( m_RenderMode == RENDERMODE_LIGHTING ) ||
        ( m_RenderMode == RENDERMODE_INDIRECTLIGHTING );

    m_pd3dDevice->SetPixelShaderConstantB( PSCONST_bDebugShowNoLighting, &bDebugShowNoLighting, 1 );
    m_pd3dDevice->SetPixelShaderConstantB( PSCONST_bDebugShowDirectLighting, &bDebugShowDirectLighting, 1 );
    m_pd3dDevice->SetPixelShaderConstantB( PSCONST_bDebugShowIndirectLighting, &bDebugShowIndirectLighting, 1 );
    m_pd3dDevice->SetPixelShaderConstantB( PSCONST_bDebugReduceFlicker, &m_bReduceFlicker, 1 );

    const FLOAT vWorldScale[ 4 ] = { 1.0f / g_fWorldScale, g_fWorldScale, g_fWorldScale / 2.0f, 1 };
    m_pd3dDevice->SetPixelShaderConstantF( PSCONST_vWorldScale, ( FLOAT* )&vWorldScale, 1 );
    m_pd3dDevice->SetVertexShaderConstantF( VSCONST_vWorldScale, ( FLOAT* )&vWorldScale, 1 );

    const FLOAT vEpsilonVSM[ 4 ] = { m_fEpsilonVSM, 0, 0, 1 };
    m_pd3dDevice->SetPixelShaderConstantF( PSCONST_fEpsilonVSM, ( FLOAT* )&vEpsilonVSM, 1 );

    const FLOAT vDiffuseLightColor[ 4 ] = { 1.0f, 0.85f, 0.85f, 1.0f };     // a dash of red
    m_pd3dDevice->SetPixelShaderConstantF( PSCONST_vDiffuseLightColor, ( FLOAT* )&vDiffuseLightColor, 1 );

    const FLOAT vSpecularLightColor[ 4 ] = { 0.95f, 0.95f, 0.75f, 32.0f };  // a dash of yellow
    m_pd3dDevice->SetPixelShaderConstantF( PSCONST_vSpecularLightColor, ( FLOAT* )&vSpecularLightColor, 1 );

    const FLOAT vAmbientLightColor[ 4 ] = { 0.08f, 0.08f, 0.08f, 1.0f };
    m_pd3dDevice->SetPixelShaderConstantF( PSCONST_vAmbientLightColor, ( FLOAT* )&vAmbientLightColor, 1 );
    m_pd3dDevice->SetVertexShaderConstantF( VSCONST_vAmbientLightColor, ( FLOAT* )&vAmbientLightColor, 1 );

    // x - scale the result of the distance falloff
    // y - clamp the result of distance square ( In our case we clamp the distance to 10 meters )
    // z - scale the result of the irradiance
    // w - scale the result of the self-occlusion
    const FLOAT vIndirectLightingRadius[ 4 ] = { 2.0f, 100.0f, 2.5f, 0.04f };
    m_pd3dDevice->SetPixelShaderConstantF( PSCONST_vIndirectLightingRadius, ( FLOAT* )&vIndirectLightingRadius, 1 );
    m_pd3dDevice->SetVertexShaderConstantF( VSCONST_vIndirectLightingRadius, ( FLOAT* )&vIndirectLightingRadius, 1 );
}


//--------------------------------------------------------------------------------------
// Name: RenderShadowmap()
// Desc: Render the scene into the shadowmap
//--------------------------------------------------------------------------------------
HRESULT Sample::RenderShadowmap()
{
    if( m_LightingMode < LIGHTINGMODE_SHADOWED )
    {
        return S_OK;
    }

    PIXBeginNamedEvent( 0, __FUNCTION__ );

    m_pShadowMap->PreRender();

    RenderScene( TRUE );

    m_pShadowMap->PostRender();

    PIXEndNamedEvent(); // Sample::RenderShadowMap

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: RenderBackBuffer()
// Desc: Render the scene into the backbuffer
//--------------------------------------------------------------------------------------
HRESULT Sample::RenderBackBuffer()
{
    PIXBeginNamedEvent( 0, __FUNCTION__ );

    PreRender();

    RenderScene( FALSE );
    RenderSkyDome();

    PostRender();

    // Since we resolve the texture from a tiled backbuffer, we need to copy
    // the result back to the backbuffer
    D3DRECT rect0 = { 0, 0, m_d3dpp.BackBufferWidth, m_d3dpp.BackBufferHeight };
    // Use the sRGB _AS_16 format, so we don't lose any precision 
    ATG::DebugDraw::DrawScreenSpaceTexturedRect( rect0, &m_BackBufferTextureAs16SRGB, FALSE );

    PIXEndNamedEvent(); // Sample::RenderBackBuffer

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: RenderScene()
// Desc: Renders the scene with the current render mode and lighting mode
//--------------------------------------------------------------------------------------
VOID Sample::RenderScene( const BOOL bRenderShadowPass )
{
    PIXBeginNamedEvent( 0, __FUNCTION__ );

    // Select the vertex and pixel shaders from the modes
    if( !bRenderShadowPass )
    {
        if( m_RenderMode == RENDERMODE_NORMAL )
        {
            switch( m_LightingMode )
            {
                    // Render the scene using per vertex lighting
                case LIGHTINGMODE_PERVERTEX:
                    m_pd3dDevice->SetVertexShader( m_pShadeScenePerVertexVS );
                    m_pd3dDevice->SetPixelShader( m_pShadeScenePerVertexPS );
                    break;

                    // Render the scene using per pixel lighting
                case LIGHTINGMODE_PERPIXEL:
                    m_pd3dDevice->SetVertexShader( m_pShadeScenePerPixelVS );
                    m_pd3dDevice->SetPixelShader( m_pShadeScenePerPixelPS );
                    break;

                    // Render the scene using per pixel lighting with shadows
                case LIGHTINGMODE_SHADOWED:
                    m_pd3dDevice->SetVertexShader( m_pShadeSceneShadowedVS );
                    m_pd3dDevice->SetPixelShader( m_pShadeSceneShadowedPS );
                    break;

                    // Render the scene with indirect lighting, but only with self occlusion
                case LIGHTINGMODE_INDIRECT_PERVERTEX_SELFOCCLUDE:
                    m_pd3dDevice->SetVertexShader( m_pShadeSceneIndirectPerVertexSelfOccludeVS );
                    m_pd3dDevice->SetPixelShader( m_pShadeSceneIndirectPerVertexPS );
                    break;

                    // Render the scene with per vertex indirect lighting
                case LIGHTINGMODE_INDIRECT_PERVERTEX:
                    m_pd3dDevice->SetVertexShader( m_pShadeSceneIndirectPerVertexVS );
                    m_pd3dDevice->SetPixelShader( m_pShadeSceneIndirectPerVertexPS );
                    break;

                    // Render the scene with per pixel indirect lighting
                case LIGHTINGMODE_INDIRECT_PERPIXEL:
                    m_pd3dDevice->SetVertexShader( m_pShadeSceneShadowedVS );
                    m_pd3dDevice->SetPixelShader( m_pShadeSceneIndirectPerPixelPS );
                    break;
            }
        }
        else    // Render debug modes
        {
            switch( m_LightingMode )
            {
                case LIGHTINGMODE_PERVERTEX:
                    m_pd3dDevice->SetVertexShader( m_pShadeScenePerVertexVS );
                    m_pd3dDevice->SetPixelShader( m_pShadeScenePerVertexDebugPS );
                    break;

                case LIGHTINGMODE_PERPIXEL:
                    m_pd3dDevice->SetVertexShader( m_pShadeScenePerPixelVS );
                    m_pd3dDevice->SetPixelShader( m_pShadeScenePerPixelDebugPS );
                    break;

                case LIGHTINGMODE_SHADOWED:
                    m_pd3dDevice->SetVertexShader( m_pShadeSceneShadowedVS );
                    m_pd3dDevice->SetPixelShader( m_pShadeSceneShadowedDebugPS );
                    break;

                case LIGHTINGMODE_INDIRECT_PERVERTEX_SELFOCCLUDE:
                    m_pd3dDevice->SetVertexShader( m_pShadeSceneIndirectPerVertexSelfOccludeVS );
                    m_pd3dDevice->SetPixelShader( m_pShadeSceneIndirectPerVertexDebugPS );
                    break;

                case LIGHTINGMODE_INDIRECT_PERVERTEX:
                    m_pd3dDevice->SetVertexShader( m_pShadeSceneIndirectPerVertexVS );
                    m_pd3dDevice->SetPixelShader( m_pShadeSceneIndirectPerVertexDebugPS );
                    break;

                case LIGHTINGMODE_INDIRECT_PERPIXEL:
                    m_pd3dDevice->SetVertexShader( m_pShadeSceneShadowedVS );
                    m_pd3dDevice->SetPixelShader( m_pShadeSceneIndirectPerPixelDebugPS );
                    break;
            }
        }

        // Shadowmap texture
        m_pd3dDevice->SetTexture( SAMPLER_ShadowmapTexture, m_pShadowMap->GetShadowMapTexture() );
        m_pd3dDevice->SetSamplerState( SAMPLER_ShadowmapTexture, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
        m_pd3dDevice->SetSamplerState( SAMPLER_ShadowmapTexture, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
        m_pd3dDevice->SetSamplerState( SAMPLER_ShadowmapTexture, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR );
        m_pd3dDevice->SetSamplerState( SAMPLER_ShadowmapTexture, D3DSAMP_TRILINEARTHRESHOLD,
                                       D3DTRILINEAR_THREEEIGHTHS );
        m_pd3dDevice->SetSamplerState( SAMPLER_ShadowmapTexture, D3DSAMP_ADDRESSU, D3DTADDRESS_BORDER );
        m_pd3dDevice->SetSamplerState( SAMPLER_ShadowmapTexture, D3DSAMP_ADDRESSV, D3DTADDRESS_BORDER );
        m_pd3dDevice->SetSamplerState( SAMPLER_ShadowmapTexture, D3DSAMP_BORDERCOLOR, 0xffffffff );
    }

    // Diffuse texture
    m_pd3dDevice->SetSamplerState( SAMPLER_DiffuseTexture, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState( SAMPLER_DiffuseTexture, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState( SAMPLER_DiffuseTexture, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState( SAMPLER_DiffuseTexture, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP );
    m_pd3dDevice->SetSamplerState( SAMPLER_DiffuseTexture, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP );
    m_pd3dDevice->SetSamplerState( SAMPLER_DiffuseTexture, D3DSAMP_TRILINEARTHRESHOLD, D3DTRILINEAR_THREEEIGHTHS );

    // Normalmap texture
    m_pd3dDevice->SetSamplerState( SAMPLER_NormalmapTexture, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState( SAMPLER_NormalmapTexture, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState( SAMPLER_NormalmapTexture, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState( SAMPLER_NormalmapTexture, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP );
    m_pd3dDevice->SetSamplerState( SAMPLER_NormalmapTexture, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP );
    m_pd3dDevice->SetSamplerState( SAMPLER_NormalmapTexture, D3DSAMP_TRILINEARTHRESHOLD, D3DTRILINEAR_THREEEIGHTHS );

    // Set the RSM texture for the pixel shader, only needed for per pixel indirect lighting
    if( m_LightingMode >= LIGHTINGMODE_INDIRECT_PERPIXEL )
    {
        m_pd3dDevice->SetSamplerState( SAMPLER_RSMPositionTexture, D3DSAMP_MINFILTER, D3DTEXF_POINT );
        m_pd3dDevice->SetSamplerState( SAMPLER_RSMPositionTexture, D3DSAMP_MAGFILTER, D3DTEXF_POINT );
        m_pd3dDevice->SetSamplerState( SAMPLER_RSMPositionTexture, D3DSAMP_MIPFILTER, D3DTEXF_POINT );

        m_pd3dDevice->SetSamplerState( SAMPLER_RSMLightDirTexture, D3DSAMP_MINFILTER, D3DTEXF_POINT );
        m_pd3dDevice->SetSamplerState( SAMPLER_RSMLightDirTexture, D3DSAMP_MAGFILTER, D3DTEXF_POINT );
        m_pd3dDevice->SetSamplerState( SAMPLER_RSMLightDirTexture, D3DSAMP_MIPFILTER, D3DTEXF_POINT );

        m_pd3dDevice->SetSamplerState( SAMPLER_RSMFluxTexture, D3DSAMP_MINFILTER, D3DTEXF_POINT );
        m_pd3dDevice->SetSamplerState( SAMPLER_RSMFluxTexture, D3DSAMP_MAGFILTER, D3DTEXF_POINT );
        m_pd3dDevice->SetSamplerState( SAMPLER_RSMFluxTexture, D3DSAMP_MIPFILTER, D3DTEXF_POINT );

        m_pd3dDevice->SetTexture( SAMPLER_RSMPositionTexture,
                                  m_ReflectiveShadowMap.GetWorldSpacePositionsSmallTexture() );
        m_pd3dDevice->SetTexture( SAMPLER_RSMLightDirTexture, m_ReflectiveShadowMap.GetLightDirectionSmallTexture() );
        m_pd3dDevice->SetTexture( SAMPLER_RSMFluxTexture, m_ReflectiveShadowMap.GetFluxSmallTexture() );

        m_pd3dDevice->SetPixelShaderConstantF( PSCONST_fSampling, ( FLOAT* )m_ReflectiveShadowMap.GetLinearSampling(),
                                               RSM_NUMSAMPLES );
    }
    else if( m_LightingMode >= LIGHTINGMODE_INDIRECT_PERVERTEX_SELFOCCLUDE )
    {
        // Set the RSM textures for the vertex shader, only needed for per vertex indirect lighting
        m_pd3dDevice->SetSamplerState( SAMPLER_RSMPositionTextureVS, D3DSAMP_MINFILTER, D3DTEXF_POINT );
        m_pd3dDevice->SetSamplerState( SAMPLER_RSMPositionTextureVS, D3DSAMP_MAGFILTER, D3DTEXF_POINT );
        m_pd3dDevice->SetSamplerState( SAMPLER_RSMPositionTextureVS, D3DSAMP_MIPFILTER, D3DTEXF_POINT );

        m_pd3dDevice->SetSamplerState( SAMPLER_RSMLightDirTextureVS, D3DSAMP_MINFILTER, D3DTEXF_POINT );
        m_pd3dDevice->SetSamplerState( SAMPLER_RSMLightDirTextureVS, D3DSAMP_MAGFILTER, D3DTEXF_POINT );
        m_pd3dDevice->SetSamplerState( SAMPLER_RSMLightDirTextureVS, D3DSAMP_MIPFILTER, D3DTEXF_POINT );

        m_pd3dDevice->SetSamplerState( SAMPLER_RSMFluxTextureVS, D3DSAMP_MINFILTER, D3DTEXF_POINT );
        m_pd3dDevice->SetSamplerState( SAMPLER_RSMFluxTextureVS, D3DSAMP_MAGFILTER, D3DTEXF_POINT );
        m_pd3dDevice->SetSamplerState( SAMPLER_RSMFluxTextureVS, D3DSAMP_MIPFILTER, D3DTEXF_POINT );

        m_pd3dDevice->SetTexture( SAMPLER_RSMPositionTextureVS,
                                  m_ReflectiveShadowMap.GetWorldSpacePositionsSmallTexture() );
        m_pd3dDevice->SetTexture( SAMPLER_RSMLightDirTextureVS,
                                  m_ReflectiveShadowMap.GetLightDirectionSmallTexture() );
        m_pd3dDevice->SetTexture( SAMPLER_RSMFluxTextureVS, m_ReflectiveShadowMap.GetFluxSmallTexture() );

        m_pd3dDevice->SetVertexShaderConstantF( VSCONST_fSampling, ( FLOAT* )m_ReflectiveShadowMap.GetLinearSampling(),
                                                RSM_NUMSAMPLES );
    }

    m_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_CCW );

    BOOL bSetTextures = ( m_LightingMode >= LIGHTINGMODE_INDIRECT_PERVERTEX_SELFOCCLUDE ) || ( !bRenderShadowPass );

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

                // Loop over mesh subsets.
                DWORD dwSubsetCount = pMesh->GetNumSubsets();
                for( DWORD dwSubsetIndex = 0; dwSubsetIndex < dwSubsetCount; ++dwSubsetIndex )
                {
                    if( bSetTextures )
                    {
                        ATG::MaterialInstance* pMaterial = mm.Materials[ dwSubsetIndex ];

                        for( DWORD j = 0; j < pMaterial->GetRawParameterCount(); ++j )
                        {
                            // Retrieve diffuse texture and normalmaps and set it
                            ATG::MaterialParameter& param = pMaterial->GetRawParameter( j );
                            if( param.pValue != NULL )
                            {
                                ATG::Texture2D* pTex2D = ( ATG::Texture2D* )param.pValue;

                                m_pd3dDevice->SetTexture( j, pTex2D->GetD3DTexture() );
                            }
                        }
                    }

                    // Render the mesh subset.
                    pMesh->RenderSubset( dwSubsetIndex, m_pd3dDevice );
                }
            }
        }
    }

    PIXEndNamedEvent(); // Sample::RenderScene
}


//--------------------------------------------------------------------------------------
// Name: RenderSkyDome()
// Desc: Render the skydome
//--------------------------------------------------------------------------------------
VOID Sample::RenderSkyDome()
{
    PIXBeginNamedEvent( 0, __FUNCTION__ );

    m_pd3dDevice->SetVertexShader( m_pShadeSkyVS );
    m_pd3dDevice->SetPixelShader( m_pShadeSkyPS );

    FLOAT fScale = g_fWorldScale * 2.0f;
    XMMATRIX matWorld = XMMatrixScaling( fScale, fScale, fScale );
    XMMATRIX matCameraWVP = XMMatrixTranspose( matWorld * m_matCameraView * m_matCameraProj );

    m_pd3dDevice->SetVertexShaderConstantF( VSCONST_matCameraWVP, ( FLOAT* )&matCameraWVP, 4 );

    m_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_CW );

    ATG::NameIndexedCollection::iterator i;

    for( i = m_pSkyDome->GetInstanceList()->begin(); i != m_pSkyDome->GetInstanceList()->end(); i++ )
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

    m_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_CCW );

    PIXEndNamedEvent(); // Sample::RenderSkyDome
}


//--------------------------------------------------------------------------------------
// Name: PreRender()
// Desc: Setup rendertargets and renderstates before rendering the backbuffer
//--------------------------------------------------------------------------------------
HRESULT Sample::PreRender()
{
    PIXBeginNamedEvent( 0, __FUNCTION__ );

    // Bias the register usage slightly towards vertex shader, since we have a lot of
    // calculations in the vertex shader for indirect lighting, but not too much, since
    // we have a lot of pixels to shade with per pixel lighting as well. This is where
    // PIX Analysis comes in handy to show where most of the work is done.
    m_pd3dDevice->SetShaderGPRAllocation( 0, 66, 62 );

    m_pd3dDevice->SetRenderTarget( 0, m_pTiledBackBufferRT );
    m_pd3dDevice->SetRenderTarget( 1, NULL );
    m_pd3dDevice->SetRenderTarget( 2, NULL );
    m_pd3dDevice->SetRenderTarget( 3, NULL );
    m_pd3dDevice->SetDepthStencilSurface( m_pTiledDepthStencilSurface );

    // Using floating point depth, so reverse the depth compare function and clear to 0.0f
    m_pd3dDevice->SetRenderState( D3DRS_ZFUNC, D3DCMP_GREATEREQUAL );

    m_pd3dDevice->BeginTiling( 0, m_iTilingRectCount, m_pTilingRects, NULL, 0.0f, 0L );

    m_pd3dDevice->BeginZPass( 0 );

    // Using floating point depth, so reverse the viewport near and far values
    D3DVIEWPORT9 viewport;
    m_pd3dDevice->GetViewport( &viewport );
    viewport.MinZ = 1.0f;
    viewport.MaxZ = 0.0f;
    m_pd3dDevice->SetViewport( &viewport );

    // Set renderstate.
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_HIZENABLE, D3DHIZ_ENABLE );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHATESTENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );   

    PIXEndNamedEvent(); // Sample::PreRender

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: PostRender()
// Desc: Resolves the rendertarget tiles to a texture and reset renderstates
//--------------------------------------------------------------------------------------
HRESULT Sample::PostRender()
{
    PIXBeginNamedEvent( 0, __FUNCTION__ );

    m_pd3dDevice->EndZPass();

    m_pd3dDevice->EndTiling( D3DRESOLVE_RENDERTARGET0 | D3DRESOLVE_CLEARDEPTHSTENCIL, NULL, m_pBackBufferTexture, NULL,
                             1.0f, 0L, NULL );

    m_pd3dDevice->SetRenderTarget( 0, m_pBackBufferRT );
    m_pd3dDevice->SetRenderTarget( 1, NULL );
    m_pd3dDevice->SetRenderTarget( 2, NULL );
    m_pd3dDevice->SetRenderTarget( 3, NULL );
    m_pd3dDevice->SetDepthStencilSurface( NULL );

    m_pd3dDevice->SetRenderState( D3DRS_HIZENABLE, D3DHIZ_AUTOMATIC );

    // Reset the GPR allocation to the default values. This is a requirement before calling Present()
    m_pd3dDevice->SetShaderGPRAllocation( 0, 0, 0 );

    PIXEndNamedEvent();   // Sample::PostRender

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: RenderDebugTextures()
// Desc: Renders the reflected shadowmap textures as debug
//--------------------------------------------------------------------------------------
VOID Sample::RenderDebugTextures()
{
    PIXBeginNamedEvent( 0, __FUNCTION__ );

    if( m_RenderMode >= RENDERMODE_RSM_POSITIONS )
    {
        m_pd3dDevice->Clear( 0, NULL, D3DCLEAR_TARGET, 0, 1.0f, 0 );
    }

    D3DRECT rect = { 0, 0, m_d3dpp.BackBufferWidth, m_d3dpp.BackBufferHeight };

    if( m_RenderMode == RENDERMODE_RSM_POSITIONS )
    {
        ATG::DebugDraw::DrawScreenSpaceTexturedRect( rect, m_ReflectiveShadowMap.GetWorldSpacePositionsTexture(),
                                                     FALSE );
    }

    if( m_RenderMode == RENDERMODE_RSM_NORMALS )
    {
        ATG::DebugDraw::DrawScreenSpaceTexturedRect( rect, m_ReflectiveShadowMap.GetLightDirectionTexture(), FALSE );
    }

    if( m_RenderMode == RENDERMODE_RSM_FLUX )
    {
        ATG::DebugDraw::DrawScreenSpaceTexturedRect( rect, m_ReflectiveShadowMap.GetFluxTexture(), FALSE );
    }

    if( m_RenderMode >= RENDERMODE_RSM_POSITIONS )
    {
        m_ReflectiveShadowMap.RenderSamplePoints( ( FLOAT )m_d3dpp.BackBufferWidth,
                                                  ( FLOAT )m_d3dpp.BackBufferHeight );
    }

    PIXEndNamedEvent();
}


//--------------------------------------------------------------------------------------
// Name: RenderUI()
// Desc: Render the help, title, and framerate
//--------------------------------------------------------------------------------------
VOID Sample::RenderUI()
{
    // Show information
    PIXBeginNamedEvent( 0, __FUNCTION__ );

    if( m_bDrawHelp )
    {
        m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
    }
    else
    {
        // Draw a title and FPS indicator.
        m_Font.Begin();

        m_Font.SetScaleFactors( 1.2f, 1.2f );
        m_Font.DrawText( 0, 0, 0xffffffff, L"AdvancedLighting" );

        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( 0, 0, 0xffffff00, m_Timer.GetFrameRate(), ATGFONT_RIGHT );

        WCHAR strText[ 256 ];
        m_Font.SetScaleFactors( 0.8f, 0.8f );

        WCHAR strEnabled[ 256 ];
        WCHAR strDisabled[ 256 ];
        swprintf_s( strEnabled, L"Enabled" );
        swprintf_s( strDisabled, L"Disabled" );

        FLOAT fXPos = 150;
        FLOAT fYPos = 40;

        m_Font.DrawText( 0, fYPos, 0xffffffff, L"Direct Lighting: " );
        m_Font.DrawText( fXPos, fYPos, 0xffffff00,
                         ( m_LightingMode >= LIGHTINGMODE_PERPIXEL ) ? L"Per Pixel" : L"Per Vertex" );
        fYPos += 20.0f;

        if( m_LightingMode < LIGHTINGMODE_INDIRECT_PERVERTEX_SELFOCCLUDE )
        {
            swprintf_s( strText, L"Constant" );
        }
        else if( m_LightingMode == LIGHTINGMODE_INDIRECT_PERVERTEX_SELFOCCLUDE )
        {
            swprintf_s( strText, L"Per Vertex, only self-occlusion" );
        }
        else if( m_LightingMode == LIGHTINGMODE_INDIRECT_PERVERTEX )
        {
            swprintf_s( strText, L"Per Vertex" );
        }
        else
        {
            swprintf_s( strText, L"Per Pixel" );
        }

        m_Font.DrawText( 0, fYPos, 0xffffffff, L"Indirect Lighting: " );
        m_Font.DrawText( fXPos, fYPos, 0xffffff00, strText );
        fYPos += 20.0f;

        m_Font.DrawText( 0, fYPos, 0xffffffff, L"Shadows: " );
        m_Font.DrawText( fXPos, fYPos, 0xffffff00,
                         ( m_LightingMode >= LIGHTINGMODE_SHADOWED ) ? strEnabled : strDisabled );
        fYPos += 20.0f;

        m_Font.DrawText( 0, fYPos, 0xffffffff, L"Flicker Reduction: " );
        m_Font.DrawText( fXPos, fYPos, 0xffffff00, m_bReduceFlicker ? strEnabled : strDisabled );
        fYPos += 20.0f;

        switch( m_RenderMode )
        {
            case RENDERMODE_NORMAL:
                swprintf_s( strText, strDisabled );
                break;

            case RENDERMODE_NOLIGHTING:
                swprintf_s( strText, L"No Lighting" );
                break;

            case RENDERMODE_DIRECTLIGHTING:
                swprintf_s( strText, L"Only Direct Lighting" );
                break;

            case RENDERMODE_INDIRECTLIGHTING:
                swprintf_s( strText, L"Only Indirect Lighting" );
                break;

            case RENDERMODE_LIGHTING:
                swprintf_s( strText, L"Direct and Indirect Lighting" );
                break;

            case RENDERMODE_RSM_POSITIONS:
                swprintf_s( strText, L"Reflective ShadowMap Positions" );
                break;

            case RENDERMODE_RSM_NORMALS:
                swprintf_s( strText, L"Reflective ShadowMap Normals" );
                break;

            case RENDERMODE_RSM_FLUX:
                swprintf_s( strText, L"Reflective ShadowMap Flux" );
                break;
        }

        m_Font.DrawText( 0, fYPos, 0xffffffff, L"Debug Mode: " );
        m_Font.DrawText( fXPos, fYPos, 0xffffff00, strText );
        fYPos += 20.0f;

        if( m_RenderMode >= RENDERMODE_RSM_POSITIONS )
        {
            m_Font.SetScaleFactors( 0.7f, 0.7f );
            m_Font.DrawText( fXPos, fYPos, 0xff0000ff, L"Uniform distributed sampling points" );
            fYPos += 15.0f;
            m_Font.DrawText( fXPos, fYPos, 0xffff0000, L"Warped Importance sampling points" );
            fYPos += 15.0f;
            m_Font.DrawText( fXPos, fYPos, 0xff00ff00, L"Currently used sampling points" );
        }

        m_Font.End();
    }

    PIXEndNamedEvent(); // Sample::RenderUI
}
