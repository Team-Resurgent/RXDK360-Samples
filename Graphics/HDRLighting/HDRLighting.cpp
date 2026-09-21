//--------------------------------------------------------------------------------------
// HDRLighting.cpp
//
// This sample demonstrates some high dynamic range lighting effects using floating
// point textures.
//
// The algorithms described in this sample are based very closely on the lighting
// effects implemented in Masaki Kawase's Rthdribl sample and the tone mapping process
// described in the whitepaper "Tone Reproduction for Digital Images"
//
//    Real-Time High Dynamic Range Image-Based Lighting (Rthdribl)
//    Masaki Kawase
//    http://www.daionet.gr.jp/~masa/rthdribl
//
//    "Photographic Tone Reproduction for Digital Images"
//    Erik Reinhard, Mike Stark, Peter Shirley and Jim Ferwerda
//    http://www.cs.utah.edu/~reinhard/cdrom
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include <xtl.h>
#include <xgraphics.h>
#include <xboxmath.h>
#include <AtgApp.h>
#include <AtgFont.h>
#include <AtgInput.h>
#include <AtgHelp.h>
#include <AtgMesh.h>
#include <AtgPostProcess.h>
#include <AtgResource.h>
#include <AtgUtil.h>
#include "GlareDefD3D.h"


//--------------------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//--------------------------------------------------------------------------------------
ATG::HELP_CALLOUT m_HelpCallouts[] =
{
    { ATG::HELP_LEFT_STICK,     ATG::HELP_PLACEMENT_2, L"Move\ncamera" },
    { ATG::HELP_RIGHT_STICK,    ATG::HELP_PLACEMENT_2, L"Rotate\ncamera" },
    { ATG::HELP_A_BUTTON,       ATG::HELP_PLACEMENT_2, L"Adjust bloom\n(w/triggers)" },
    { ATG::HELP_B_BUTTON,       ATG::HELP_PLACEMENT_2, L"Adjust star\n(w/triggers)" },
    { ATG::HELP_X_BUTTON,       ATG::HELP_PLACEMENT_2, L"Adjust key value\n(w/triggers)" },
    { ATG::HELP_DPAD,           ATG::HELP_PLACEMENT_2, L"Adjust lights\n(w/triggers)" },
    { ATG::HELP_BACK_BUTTON,    ATG::HELP_PLACEMENT_2, L"Display\nhelp" },
    { ATG::HELP_START_BUTTON,   ATG::HELP_PLACEMENT_2, L"Adapt\nluminance" },
    { ATG::HELP_LEFT_SHOULDER,  ATG::HELP_PLACEMENT_2, L"Toggle\ntone mapping" },
    { ATG::HELP_RIGHT_SHOULDER, ATG::HELP_PLACEMENT_2, L"Toggle\nblue shifting" },
};
#define NUM_HELP_CALLOUTS (sizeof(m_HelpCallouts) / sizeof(m_HelpCallouts[0]))


//--------------------------------------------------------------------------------------
// Mappings to shader constants that are used in the HLSL shaders
//--------------------------------------------------------------------------------------
const DWORD VSCONST_mObjectToView = 0;
const DWORD VSCONST_mProjection = 4;

const DWORD PSCONST_fEmissive = 4;
const DWORD PSCONST_fMiddleGray = 5;
//const DWORD PSCONST_fElapsedTime      =  7;
const DWORD PSCONST_bEnableBlueShift = 8;
const DWORD PSCONST_bEnableToneMap = 9;
const DWORD PSCONST_fBloomScale = 10;
const DWORD PSCONST_fStarScale = 11;

const DWORD PSCONST_fLightingCoeffs = 12;

const DWORD PSCONST_avLightPositionView = 20;
const DWORD PSCONST_afLightIntensity = 22;


//--------------------------------------------------------------------------------------
// Constants
//--------------------------------------------------------------------------------------
const DWORD NUM_LIGHTS = 2;       // Number of lights in the scene
const FLOAT EMISSIVE_COEFFICIENT = 39.78f;  // Emissive color multiplier for each lumen of light intensity
const DWORD NUM_TONEMAP_TEXTURES = 4;       // Number of stages in the 4x4 down-scaling of average luminance textures
const DWORD NUM_STAR_TEXTURES = 12;      // Number of textures used for the star post-processing effect
const DWORD NUM_BLOOM_TEXTURES = 3;       // Number of textures used for the bloom post-processing effect

//--------------------------------------------------------------------------------------
// Helper functions
//--------------------------------------------------------------------------------------
#define V_RETURN(fn)    { if(FAILED(hr=(fn)))return hr;}


//--------------------------------------------------------------------------------------
// Name: class Sample
// Desc: Main class to run this application.
//--------------------------------------------------------------------------------------
class Sample : public ATG::Application
{
    ATG::Timer m_Timer;                  // Application timer
    ATG::Font m_Font;                   // Application font
    ATG::PackedResource m_Resource;               // Packed texture resources
    ATG::PostProcess m_PostProcess;            // Commonly used effects (blur, etc.)
    ATG::Help m_Help;                   // Help class
    BOOL m_bDrawHelp;              // Whether to draw the help screen

    BOOL m_bPaused;                // True when the application is paused

    DWORD m_dwNumTiles;             // Tile information for predicated tiling
    D3DRECT             m_TilingRects[4];

    LPDIRECT3DTEXTURE9 m_pFrontBuffer;
    LPDIRECT3DSURFACE9 m_pBackBuffer;
    LPDIRECT3DSURFACE9 m_pDepthStencilBuffer;

    LPDIRECT3DVERTEXDECLARATION9 m_pTransformSceneVtxDecl;
    LPDIRECT3DVERTEXSHADER9 m_pTransformSceneVS;
    LPDIRECT3DPIXELSHADER9 m_pPointLightPS;
    LPDIRECT3DPIXELSHADER9 m_pLightSpherePS;
    LPDIRECT3DPIXELSHADER9 m_pFinalScenePassPS;

    LPDIRECT3DTEXTURE9 m_pWhiteTexture;                // A blank texture
    LPDIRECT3DTEXTURE9 m_pSceneTexture;                // HDR texture containing the scene
    LPDIRECT3DTEXTURE9 m_pScaledSceneTexture;          // Scaled copy of the HDR scene
    LPDIRECT3DTEXTURE9 m_pBrightPassTexture;           // Bright-pass filtered copy of the scene
    LPDIRECT3DTEXTURE9 m_pAdaptedLuminanceTexture;     // The luminance the user is adapted to
    LPDIRECT3DTEXTURE9 m_pStarSourceTexture;           // Star effect source texture
    LPDIRECT3DTEXTURE9 m_pBloomSourceTexture;          // Bloom effect source texture
    LPDIRECT3DTEXTURE9 m_pBloomTexture;                // Blooming effect texture
    LPDIRECT3DTEXTURE9  m_apStarTextures[NUM_STAR_TEXTURES]; // Star effect working textures
    LPDIRECT3DTEXTURE9 m_pToneMapTexture64x64;         // Average luminance samples from the HDR render target
    LPDIRECT3DTEXTURE9 m_pToneMapTexture16x16;         // Average luminance samples from the HDR render target
    LPDIRECT3DTEXTURE9 m_pToneMapTexture4x4;           // Average luminance samples from the HDR render target
    LPDIRECT3DTEXTURE9 m_pToneMapTexture1x1;           // Average luminance samples from the HDR render target

    LPDIRECT3DSURFACE9 m_pSceneRT;                // Render target for the HDR texture containing the scene (in 16f:16f:16f:16f format)

    XMMATRIX m_matWorld;
    XMMATRIX m_matView;
    XMMATRIX m_matProj;

    ATG::Mesh m_SphereMesh;              // Mesh of a sphere, used to represent the lights
    ATG::Mesh m_WorldMesh;               // Mesh to contain world objects
    XMVECTOR m_vMinWorldCoord;          // Bounding box coordinates for the world mesh
    XMVECTOR m_vMaxWorldCoord;

    CGlareDef m_GlareDef;                // Glare defintion
    FLOAT               m_Pad0[3];

    XMVECTOR            m_avLightPosition[NUM_LIGHTS];   // Light positions in world space
    FLOAT               m_fLightIntensity[NUM_LIGHTS];   // Light floating point intensities

    FLOAT m_fMiddleGrayKeyValue;     // Middle gray key value for tone mapping
    FLOAT m_fBloomScale;             // Scale factor for bloom
    FLOAT m_fStarScale;              // Scale factor for star

    BOOL m_bToneMap;                // True when scene is to be tone mapped
    BOOL m_bBlueShift;              // True when blue shift is to be factored in

    // Tone mapping and post-process lighting effects
    HRESULT             MeasureLuminance();
    HRESULT             RenderStar();
    HRESULT             RenderBloom();

    HRESULT             RenderScene();

public:
    virtual HRESULT     Initialize();
    virtual HRESULT     Update();
    virtual HRESULT     Render();
};


//--------------------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the program
//--------------------------------------------------------------------------------------
INT __cdecl main()
{
    Sample atgApp;
    atgApp.m_d3dpp.BackBufferWidth = 1280;   // Force to 640x480 to fit in EDRAM
    atgApp.m_d3dpp.BackBufferHeight = 720;
    atgApp.m_d3dpp.DisableAutoBackBuffer = TRUE;  // Disable automatic buffer creation
    atgApp.m_d3dpp.DisableAutoFrontBuffer = TRUE;  // since we will create our own
    atgApp.m_d3dpp.EnableAutoDepthStencil = FALSE;
    atgApp.Run();
}


//--------------------------------------------------------------------------------------
// Name: Initialize()
// Desc: Initialize app-dependent objects
//--------------------------------------------------------------------------------------
HRESULT Sample::Initialize()
{
    HRESULT hr;

    m_bDrawHelp = FALSE;
    m_bPaused = FALSE;    // True when the application is paused

    m_avLightPosition[0] = XMVectorSet( 4.0f, 2.0f, 18.0f, 1.0f );
    m_avLightPosition[1] = XMVectorSet( 11.0f, 2.0f, 18.0f, 1.0f );
    m_fLightIntensity[0] = 16.0f;
    m_fLightIntensity[1] = 4.0f;
    m_fMiddleGrayKeyValue = 0.08f;    // Middle gray key value for tone mapping
    m_fBloomScale = 1.0f;     // Scale factor for bloom
    m_fStarScale = 0.5f;     // Scale factor for star
    m_bToneMap = TRUE;     // True when scene is to be tone mapped
    m_bBlueShift = TRUE;     // True when blue shift is to be factored in

    // Create the font
    if( FAILED( m_Font.Create( "game:\\Media\\Fonts\\Arial_16.xpr" ) ) )
    {
        ATG_PrintError( "DX9Sample: Couldn't create font\n" );
        return E_FAIL;
    }

    // Confine text drawing to the title safe area
    m_Font.SetWindow( ATG::GetTitleSafeArea() );

    // Create the help
    if( FAILED( m_Help.Create( "game:\\Media\\Help\\Help.xpr" ) ) )
    {
        ATG_PrintError( "DX9Sample: Couldn't create help\n" );
        return E_FAIL;
    }

    // Create the textures resource
    if( FAILED( m_Resource.Create( "game:\\Media\\Resource.xpr" ) ) )
    {
        ATG_PrintError( "DX9Sample: Couldn't create Resource.xpr\n" );
        return E_FAIL;
    }

    // Initialize the post-processing effects library (for blur, bloom, etc.)
    if( FAILED( m_PostProcess.Initialize() ) )
    {
        ATG_PrintError( "DX9Sample: Couldn't initialize the effects library\n" );
        return E_FAIL;
    }

    // Initialize the glare definition used for the star effect
    m_GlareDef.Initialize( GLT_FILTER_CROSSSCREEN );

    // Create the vertex declarations
    static const D3DVERTEXELEMENT9 decl[] =
    {
        { 0,  0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_POSITION, 0 },
        { 0, 12, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_NORMAL,   0 },
        { 0, 24, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT,  D3DDECLUSAGE_TEXCOORD, 0 },
        D3DDECL_END()
    };
    m_pd3dDevice->CreateVertexDeclaration( decl, &m_pTransformSceneVtxDecl );

    // Create vertex and pixel shaders
    V_RETURN( ATG::LoadVertexShader( "game:\\Media\\Shaders\\TransformScene.xvu", &m_pTransformSceneVS ) );
    V_RETURN( ATG::LoadPixelShader( "game:\\Media\\Shaders\\PointLight.xpu",      &m_pPointLightPS ) );
    V_RETURN( ATG::LoadPixelShader( "game:\\Media\\Shaders\\LightSphere.xpu",     &m_pLightSpherePS ) );
    V_RETURN( ATG::LoadPixelShader( "game:\\Media\\Shaders\\FinalScenePass.xpu",  &m_pFinalScenePassPS ) );

    // Use the 7e3 scene format
    D3DFORMAT d3dSceneFormat = D3DFMT_A2B10G10R10F_EDRAM;

    // Check need for predicated tiling
    DWORD dwTileWidth = m_d3dpp.BackBufferWidth;
    DWORD dwTileHeight = m_d3dpp.BackBufferHeight;
    m_dwNumTiles = 1;
    {
        dwTileWidth = XGNextMultiple( dwTileWidth, GPU_TEXTURE_TILE_DIMENSION );
        dwTileHeight = XGNextMultiple( dwTileHeight, GPU_TEXTURE_TILE_DIMENSION );

        DWORD dwNumBackBufferEDRAMTiles = XGSurfaceSize( dwTileWidth, dwTileHeight, d3dSceneFormat,
                                                         D3DMULTISAMPLE_NONE );
        DWORD dwNumZBufferEDRAMTiles = XGSurfaceSize( m_d3dpp.BackBufferWidth, m_d3dpp.BackBufferHeight, D3DFMT_D24S8,
                                                      D3DMULTISAMPLE_NONE );

        while( dwNumBackBufferEDRAMTiles + dwNumZBufferEDRAMTiles > GPU_EDRAM_TILES )
        {
            dwTileWidth = XGNextMultiple( dwTileWidth / 2, GPU_TEXTURE_TILE_DIMENSION );
            m_dwNumTiles <<= 1L;

            dwNumBackBufferEDRAMTiles = XGSurfaceSize( dwTileWidth, dwTileHeight, d3dSceneFormat,
                                                       D3DMULTISAMPLE_NONE );
        }

        for( DWORD i = 0; i < m_dwNumTiles; i++ )
        {
            m_TilingRects[i].x1 = ( i + 0 ) * dwTileWidth;
            m_TilingRects[i].x2 = ( i + 1 ) * dwTileWidth;
            m_TilingRects[i].y1 = 0;
            m_TilingRects[i].y2 = m_d3dpp.BackBufferHeight;
        }
    }

    // Create the front buffer
    m_pd3dDevice->CreateTexture( m_d3dpp.BackBufferWidth, m_d3dpp.BackBufferHeight,
                                 1, D3DUSAGE_RENDERTARGET, ( D3DFORMAT )MAKESRGBFMT( D3DFMT_LE_X8R8G8B8 ),
                                 D3DPOOL_DEFAULT, &m_pFrontBuffer, NULL );

    // Create the back buffer at base address 0
    D3DSURFACE_PARAMETERS SurfaceParameters = {0};
    SurfaceParameters.Base = 0;
    m_pd3dDevice->CreateRenderTarget( m_d3dpp.BackBufferWidth, m_d3dpp.BackBufferHeight,
                                      ( D3DFORMAT )MAKESRGBFMT( D3DFMT_X8R8G8B8 ), D3DMULTISAMPLE_NONE, 0, FALSE,
                                      &m_pBackBuffer, &SurfaceParameters );

    // Create the stencil buffer at an address that is beyond our largets render target
    // Put the hierarchical Z buffer at the start of hierarchical Z memory.
    SurfaceParameters.Base = XGSurfaceSize( dwTileWidth, dwTileHeight,
                                            d3dSceneFormat, D3DMULTISAMPLE_NONE );
    SurfaceParameters.HierarchicalZBase = 0;
    m_pd3dDevice->CreateDepthStencilSurface( m_d3dpp.BackBufferWidth, m_d3dpp.BackBufferHeight,
                                             D3DFMT_D24S8, D3DMULTISAMPLE_NONE, 0, FALSE,
                                             &m_pDepthStencilBuffer, &SurfaceParameters );

    // The rest of the render targets can be created as base 0 since they are not
    // used at the same time as the back buffer
    SurfaceParameters.Base = 0;

    // Initialize the camera
    XMVECTOR vFromPt = XMVectorSet( 7.5f, 1.8f, 2.0f, 0.0f );
    XMVECTOR vLookatPt = XMVectorSet( 7.5f, 1.5f, 10.0f, 0.0f );
    XMVECTOR vUp = XMVectorSet( 0.0f, 0.0f, 1.0f, 0.0f );

    // Determine the aspect ratio
    FLOAT fAspectRatio = ( FLOAT )m_d3dpp.BackBufferWidth / ( FLOAT )m_d3dpp.BackBufferHeight;

    m_matWorld = XMMatrixIdentity();
    m_matView = XMMatrixLookAtLH( vFromPt, vLookatPt, vUp );
    m_matProj = XMMatrixPerspectiveFovLH( XM_PI / 4, fAspectRatio, 0.2f, 30.0f );

    // Create the white texture
    V_RETURN( m_pd3dDevice->CreateTexture( 1, 1, 1, 0L, D3DFMT_A8R8G8B8,
     D3DPOOL_DEFAULT, &m_pWhiteTexture, NULL ) );
    m_PostProcess.ClearTexture( m_pWhiteTexture, 0xffffffff );

    // Create the HDR scene texture and a render target for it
    V_RETURN( m_pd3dDevice->CreateTexture( m_d3dpp.BackBufferWidth, m_d3dpp.BackBufferHeight,
                                               1, D3DUSAGE_RENDERTARGET, D3DFMT_A16B16G16R16F,
     D3DPOOL_DEFAULT, &m_pSceneTexture, NULL ) );

    V_RETURN( m_pd3dDevice->CreateRenderTarget( dwTileWidth, dwTileHeight,
                                                    d3dSceneFormat, D3DMULTISAMPLE_NONE, 0L,
     FALSE, &m_pSceneRT, &SurfaceParameters ) );

    // Scaled version of the HDR scene texture
    V_RETURN( m_pd3dDevice->CreateTexture( m_d3dpp.BackBufferWidth / 4, m_d3dpp.BackBufferHeight / 4,
                                               1, D3DUSAGE_RENDERTARGET,
                                               D3DFMT_A16B16G16R16F, D3DPOOL_DEFAULT,
     &m_pScaledSceneTexture, NULL ) );

    // Create a texture to hold the intermediate results of the luminance calculation
    V_RETURN( m_pd3dDevice->CreateTexture( 64, 64, 1, D3DUSAGE_RENDERTARGET,
                                               D3DFMT_R16F_EXPAND, D3DPOOL_DEFAULT,
     &m_pToneMapTexture64x64, NULL ) );
    V_RETURN( m_pd3dDevice->CreateTexture( 16, 16, 1, D3DUSAGE_RENDERTARGET,
                                               D3DFMT_R16F_EXPAND, D3DPOOL_DEFAULT,
     &m_pToneMapTexture16x16, NULL ) );
    V_RETURN( m_pd3dDevice->CreateTexture( 4, 4, 1, D3DUSAGE_RENDERTARGET,
                                               D3DFMT_R16F_EXPAND, D3DPOOL_DEFAULT,
     &m_pToneMapTexture4x4, NULL ) );
    V_RETURN( m_pd3dDevice->CreateTexture( 1, 1, 1, D3DUSAGE_RENDERTARGET,
                                               D3DFMT_R16F_EXPAND, D3DPOOL_DEFAULT,
     &m_pToneMapTexture1x1, NULL ) );

    // Create a 1x1 texture to hold the luminance that the user is currently adapted to.
    // This allows for a simple simulation of light adaptation.
    V_RETURN( m_pd3dDevice->CreateTexture( 1, 1, 1,
                                               D3DUSAGE_RENDERTARGET, D3DFMT_R16F_EXPAND, D3DPOOL_DEFAULT,
     &m_pAdaptedLuminanceTexture, NULL ) );

    // Create the bright-pass filter texture.
    // Texture has a black border of single texel thickness to fake border
    // addressing using clamp addressing
    V_RETURN( m_pd3dDevice->CreateTexture( m_d3dpp.BackBufferWidth / 4 + 2, m_d3dpp.BackBufferHeight / 4 + 2,
                                               1, D3DUSAGE_RENDERTARGET,
                                               D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT,
     &m_pBrightPassTexture, NULL ) );

    // Create a texture to be used as the source for the star effect
    // Texture has a black border of single texel thickness to fake border
    // addressing using clamp addressing
    V_RETURN( m_pd3dDevice->CreateTexture( m_d3dpp.BackBufferWidth / 4 + 2, m_d3dpp.BackBufferHeight / 4 + 2,
                                               1, D3DUSAGE_RENDERTARGET,
                                               D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT,
     &m_pStarSourceTexture, NULL ) );

    // Create a texture to be used as the source for the bloom effect
    V_RETURN( m_pd3dDevice->CreateTexture( m_d3dpp.BackBufferWidth / 8, m_d3dpp.BackBufferHeight / 8,
                                               1, D3DUSAGE_RENDERTARGET,
                                               D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT,
     &m_pBloomSourceTexture, NULL ) );

    // Create the temporary blooming effect textures
    // Texture has a black border of single texel thickness to fake border
    // addressing using clamp addressing
    V_RETURN( m_pd3dDevice->CreateTexture( m_d3dpp.BackBufferWidth/8, m_d3dpp.BackBufferHeight/8,
                                                1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8,
     D3DPOOL_DEFAULT, &m_pBloomTexture, NULL ) );

    // Create the star effect textures
    for( DWORD i = 0; i < NUM_STAR_TEXTURES; i++ )
    {
        V_RETURN( m_pd3dDevice->CreateTexture( m_d3dpp.BackBufferWidth /4, m_d3dpp.BackBufferHeight / 4,
                                                       1, D3DUSAGE_RENDERTARGET,
                                                       D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT,
         &m_apStarTextures[i], NULL ) );
    }

    // Textures with borders must be cleared since scissor rect testing will
    // be used to avoid rendering on top of the border
    m_PostProcess.ClearTexture( m_pBloomSourceTexture );
    m_PostProcess.ClearTexture( m_pAdaptedLuminanceTexture );
    m_PostProcess.ClearTexture( m_pBrightPassTexture );
    m_PostProcess.ClearTexture( m_pStarSourceTexture );

    m_PostProcess.ClearTexture( m_pBloomTexture );

    // Create the mesh for the room
    if( FAILED( hr = m_WorldMesh.Create( "game:\\Media\\Meshes\\Room1.xbg", &m_Resource ) ) )
        return hr;
    m_WorldMesh.ComputeBoundingBox( m_vMinWorldCoord, m_vMaxWorldCoord );

    // Create sphere mesh to represent the light
    if( FAILED( hr = m_SphereMesh.Create( "game:\\Media\\Meshes\\Sphere0.xbg" ) ) )
        return hr;

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Update()
// Desc: Called once per frame, the call is the entry point for animating the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Update()
{
    FLOAT m_fElapsedTime = ( FLOAT )m_Timer.GetElapsedTime();

    // Get the current gamepad status
    ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput();
    FLOAT fLeftTrigger = pGamepad->bLeftTrigger / 255.0f;
    FLOAT fRightTrigger = pGamepad->bRightTrigger / 255.0f;

    // Toggle help
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
        m_bDrawHelp = !m_bDrawHelp;

    // Allow the user to adjust light 0
    if( pGamepad->wLastButtons & XINPUT_GAMEPAD_DPAD_LEFT )
    {
        FLOAT fLogIntensity = log10f( m_fLightIntensity[0] );
        fLogIntensity += ( fRightTrigger - fLeftTrigger ) * m_fElapsedTime * 3.0f;
        fLogIntensity = max( -4.0f, min( +7.0f, fLogIntensity ) );
        m_fLightIntensity[0] = powf( 10.0f, fLogIntensity );
    }

    // Allow the user to adjust light 1
    if( pGamepad->wLastButtons & XINPUT_GAMEPAD_DPAD_RIGHT )
    {
        FLOAT fLogIntensity = log10f( m_fLightIntensity[1] );
        fLogIntensity += ( fRightTrigger - fLeftTrigger ) * m_fElapsedTime * 3.0f;
        fLogIntensity = max( -4.0f, min( +7.0f, fLogIntensity ) );
        m_fLightIntensity[1] = powf( 10.0f, fLogIntensity );
    }

    // Allow the user to adjust the key value
    if( pGamepad->wButtons & XINPUT_GAMEPAD_X )
    {
        m_fMiddleGrayKeyValue += ( fRightTrigger - fLeftTrigger ) * m_fElapsedTime * 0.1f;
        m_fMiddleGrayKeyValue = max( 0.0f, m_fMiddleGrayKeyValue );
    }

    // Allow the user to adjust the bloom scale
    if( pGamepad->wButtons & XINPUT_GAMEPAD_A )
    {
        m_fBloomScale += ( fRightTrigger - fLeftTrigger ) * m_fElapsedTime * 0.5f;
        m_fBloomScale = max( 0.0f, m_fBloomScale );
    }

    // Allow the user to adjust the star scale
    if( pGamepad->wButtons & XINPUT_GAMEPAD_B )
    {
        m_fStarScale += ( fRightTrigger - fLeftTrigger ) * m_fElapsedTime * 0.5f;
        m_fStarScale = max( 0.0f, m_fStarScale );
    }

    // Allow the user to toggle tone mapping
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_LEFT_SHOULDER )
        m_bToneMap = !m_bToneMap;

    // Allow the user to toggle tone mapping
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER )
        m_bBlueShift = !m_bBlueShift;

    // Allow the user to toggle tone mapping
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_START )
        m_bPaused = !m_bPaused;


    // Move the camera
    {
        static XMVECTOR vFromPt = XMVectorSet( 7.5f, 1.8f, 2.0f, 0.0f );
        static XMVECTOR vLookatDir = XMVectorSet( 0.0f, -0.3f, 8.0f, 0.0f );
        static XMVECTOR vUp = XMVectorSet( 0.0f, 1.0f, 0.0f, 0.0f );

        // Rotate the camera
        static FLOAT fRotateY = 0.0f;
        XMMATRIX matRotateY;
        matRotateY = XMMatrixRotationY( pGamepad->fX2 * m_fElapsedTime );
        vLookatDir = XMVector3TransformCoord( vLookatDir, matRotateY );

        // Translate the camera
        XMVECTOR vForward = XMVectorSet( vLookatDir.x, 0.0f, vLookatDir.z, 0.0f );
        XMVECTOR vCross = XMVector3Cross( vUp, vForward );
        vFromPt += pGamepad->fX1 * m_fElapsedTime * vCross;
        vFromPt += pGamepad->fY1 * m_fElapsedTime * vForward;

        // Confine the camera to the interior of the world mesh
        vFromPt.x = max( m_vMinWorldCoord.x + 1.0f, vFromPt.x );
        vFromPt.x = min( m_vMaxWorldCoord.x - 1.0f, vFromPt.x );
        vFromPt.z = max( m_vMinWorldCoord.z + 1.0f, vFromPt.z );
        vFromPt.z = min( m_vMaxWorldCoord.z - 1.0f, vFromPt.z );

        // Build the view matrix
        XMVECTOR vLookatPt = vFromPt + vLookatDir;
        m_matView = XMMatrixLookAtLH( vFromPt, vLookatPt, vUp );
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Render()
// Desc: Called once per frame, the call is the entry point for 3D rendering. This
//       function sets up render states, clears the viewport, and renders the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Render()
{
    FLOAT m_fElapsedTime = ( FLOAT )m_Timer.GetElapsedTime();
    m_fElapsedTime = max( 1 / 60.0f, m_fElapsedTime );

    // Set and clear the z-buffer
    m_pd3dDevice->SetDepthStencilSurface( m_pDepthStencilBuffer );
    m_pd3dDevice->Clear( 0L, NULL, D3DCLEAR_ZBUFFER, 0xff000000, 1.0f, 0L );

    // Render the HDR Scene
    RenderScene();

    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP );
    m_pd3dDevice->SetSamplerState( 1, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP );
    m_pd3dDevice->SetSamplerState( 1, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP );
    m_pd3dDevice->SetSamplerState( 1, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP );
    m_pd3dDevice->SetSamplerState( 2, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP );
    m_pd3dDevice->SetSamplerState( 3, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP );
    m_pd3dDevice->SetSamplerState( 3, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP );

    // Create a scaled copy of the scene
    m_PostProcess.Downsample4x4Texture( m_pSceneTexture, m_pScaledSceneTexture );

    // If Update() has been called, the user's adaptation level has also changed
    // and should be updated
    if( !m_bPaused )
    {
        // Setup tone mapping technique
        if( m_bToneMap )
            MeasureLuminance();

        // Calculate the current luminance adaptation level. This simulates the light
        // adaptation that occurs when moving from a dark area to a bright area, or vice
        // versa. The m_pTexAdaptedLuminance texture stores a single texel cooresponding
        // to the user's adapted level.
        m_PostProcess.AdaptLuminance( m_pAdaptedLuminanceTexture, m_pToneMapTexture1x1,
                                      m_fElapsedTime, m_pAdaptedLuminanceTexture );
    }

    // Now that luminance information has been gathered, the scene can be bright-pass filtered
    // to remove everything except bright lights and reflections.
    m_PostProcess.BrightPassFilterTexture( m_pScaledSceneTexture, m_pAdaptedLuminanceTexture,
                                           m_fMiddleGrayKeyValue, m_pBrightPassTexture );

    // Blur the bright-pass filtered image to create the source texture for the star effect
    m_PostProcess.GaussBlur5x5Texture( m_pBrightPassTexture, m_pStarSourceTexture );

    // Scale-down the source texture for the star effect to create the source texture
    // for the bloom effect
    m_PostProcess.Downsample2x2Texture( m_pStarSourceTexture, m_pBloomSourceTexture );

    // Render post-process lighting effects
    RenderBloom();
    RenderStar();

    // Draw the high dynamic range scene texture to the low dynamic range
    // back buffer. As part of this final pass, the scene will be tone-mapped
    // using the user's current adapted luminance, blue shift will occur
    // if the scene is determined to be very dark, and the post-process lighting
    // effect textures will be added to the scene.
    m_pd3dDevice->SetPixelShader( m_pFinalScenePassPS );
    m_pd3dDevice->SetPixelShaderConstantF( PSCONST_fMiddleGray, &m_fMiddleGrayKeyValue, 1 );

    m_pd3dDevice->SetPixelShaderConstantF( PSCONST_fBloomScale, &m_fBloomScale, 1 );
    m_pd3dDevice->SetPixelShaderConstantF( PSCONST_fStarScale, &m_fStarScale, 1 );
    FLOAT fValue;
    fValue = m_bToneMap ?  1.0f : 0.0f;
    m_pd3dDevice->SetPixelShaderConstantF( PSCONST_bEnableToneMap, &fValue, 1 );
    fValue = m_bBlueShift ?  1.0f : 0.0f;
    m_pd3dDevice->SetPixelShaderConstantF( PSCONST_bEnableBlueShift, &fValue, 1 );

    m_pd3dDevice->SetRenderTarget( 0, m_pBackBuffer );
    m_pd3dDevice->SetTexture( 0, m_pSceneTexture );
    m_pd3dDevice->SetTexture( 1, m_pBloomTexture );
    m_pd3dDevice->SetTexture( 2, m_apStarTextures[0] );
    m_pd3dDevice->SetTexture( 3, m_pAdaptedLuminanceTexture );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP );
    m_pd3dDevice->SetSamplerState( 1, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP );
    m_pd3dDevice->SetSamplerState( 1, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP );
    m_pd3dDevice->SetSamplerState( 1, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP );
    m_pd3dDevice->SetSamplerState( 2, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP );
    m_pd3dDevice->SetSamplerState( 3, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP );
    m_pd3dDevice->SetSamplerState( 3, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_POINT );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_POINT );
    m_pd3dDevice->SetSamplerState( 1, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState( 1, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState( 2, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState( 2, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState( 3, D3DSAMP_MAGFILTER, D3DTEXF_POINT );
    m_pd3dDevice->SetSamplerState( 3, D3DSAMP_MINFILTER, D3DTEXF_POINT );

    // Draw a full-screen quad
    D3DVIEWPORT9 vp;
    m_pd3dDevice->GetViewport( &vp );
    m_PostProcess.DrawScreenSpaceQuad( ( FLOAT )vp.Width, ( FLOAT )vp.Height, 1.0f, 1.0f );

    // Show title, frame rate, and help
    m_Timer.MarkFrame();
    if( m_bDrawHelp )
    {
        m_Help.Render( &m_Font, m_HelpCallouts, NUM_HELP_CALLOUTS );
    }
    else
    {
        m_Font.Begin();
        m_Font.SetScaleFactors( 1.2f, 1.2f );
        m_Font.DrawText( 0, 0, 0xffffffff, L"HDRLighting" );
        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( 0, 0, 0xffffff00, m_Timer.GetFrameRate(), ATGFONT_RIGHT );

        WCHAR str[80];
        FLOAT fYOffset = 40.0f;
        swprintf_s( str, L"%.2f", m_fLightIntensity[0] );
        m_Font.DrawText( 15.0f, fYOffset, 0xffffffff, L"Light 0 intensity:" );
        m_Font.DrawText( 200.0f, fYOffset, 0xffffff00, str );
        fYOffset += 25.0f;
        swprintf_s( str, L"%.2f", m_fLightIntensity[1] );
        m_Font.DrawText( 15.0f, fYOffset, 0xffffffff, L"Light 1 intensity:" );
        m_Font.DrawText( 200.0f, fYOffset, 0xffffff00, str );
        fYOffset += 25.0f;
        swprintf_s( str, L"%.2f", m_fMiddleGrayKeyValue );
        m_Font.DrawText( 15.0f, fYOffset, 0xffffffff, L"Key value:" );
        m_Font.DrawText( 200.0f, fYOffset, 0xffffff00, str );
        fYOffset += 25.0f;
        swprintf_s( str, L"%.2f", m_fBloomScale );
        m_Font.DrawText( 15.0f, fYOffset, 0xffffffff, L"Bloom scale:" );
        m_Font.DrawText( 200.0f, fYOffset, 0xffffff00, str );
        fYOffset += 25.0f;
        swprintf_s( str, L"%.2f", m_fStarScale );
        m_Font.DrawText( 15.0f, fYOffset, 0xffffffff, L"Star Scale:" );
        m_Font.DrawText( 200.0f, fYOffset, 0xffffff00, str );
        fYOffset += 25.0f;
        swprintf_s( str, L"%s", m_bToneMap ? L"On" : L"Off" );
        m_Font.DrawText( 15.0f, fYOffset, 0xffffffff, L"Tone map:" );
        m_Font.DrawText( 200.0f, fYOffset, 0xffffff00, str );
        fYOffset += 25.0f;
        swprintf_s( str, L"%s", m_bBlueShift ? L"On" : L"Off" );
        m_Font.DrawText( 15.0f, fYOffset, 0xffffffff, L"Blue shift:" );
        m_Font.DrawText( 200.0f, fYOffset, 0xffffff00, str );
        fYOffset += 25.0f;
        swprintf_s( str, L"%s", m_bPaused ? L"Off" : L"On" );
        m_Font.DrawText( 15.0f, fYOffset, 0xffffffff, L"Adapt luminance:" );
        m_Font.DrawText( 200.0f, fYOffset, 0xffffff00, str );

        m_Font.End();
    }

    // Wait for the vertical blank before we resolve to the front buffer to avoid tearing.
    m_pd3dDevice->SynchronizeToPresentationInterval();

    // Resolve to the front buffer
    m_pd3dDevice->Resolve( D3DRESOLVE_RENDERTARGET0, NULL, m_pFrontBuffer, NULL, 0, 0, NULL, 0.0f, 0, NULL );

    // Show the frame on the primary surface.
    m_pd3dDevice->Swap( m_pFrontBuffer, NULL );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: RenderScene()
// Desc: Render the world objects and lights
//--------------------------------------------------------------------------------------
HRESULT Sample::RenderScene()
{
    // Setup HDR render target to store intermediate floating point color values
    m_pd3dDevice->SetRenderTarget( 0, m_pSceneRT );

    // Begin tiling
    D3DXVECTOR4 ClearColor( 0, 0, 0, 0 );
    m_pd3dDevice->BeginTiling( 0, m_dwNumTiles, m_TilingRects, ( D3DVECTOR4* )&ClearColor, 1.0f, 0L );

    XMVECTOR fLightingCoeffs;

    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP );

    m_pd3dDevice->SetVertexDeclaration( m_pTransformSceneVtxDecl );
    m_pd3dDevice->SetVertexShader( m_pTransformSceneVS );
    m_pd3dDevice->SetPixelShader( m_pPointLightPS );

    {
        XMMATRIX matT;
        matT = XMMatrixTranspose( m_matView );
        m_pd3dDevice->SetVertexShaderConstantF( VSCONST_mObjectToView, ( FLOAT* )&matT, 4 );
    }

    {
        XMMATRIX matT;
        matT = XMMatrixTranspose( m_matProj );
        m_pd3dDevice->SetVertexShaderConstantF( VSCONST_mProjection, ( FLOAT* )&matT, 4 );
    }

    for( DWORD i = 0; i < NUM_LIGHTS; i++ )
    {
        XMVECTOR vLightViewPosition;
        vLightViewPosition = XMVector4Transform( m_avLightPosition[i], m_matView );
        m_pd3dDevice->SetPixelShaderConstantF( PSCONST_avLightPositionView + i, ( FLOAT* )&vLightViewPosition, 1 );

        m_pd3dDevice->SetPixelShaderConstantF( PSCONST_afLightIntensity + i, &m_fLightIntensity[i], 1 );
    }

    // Turn off emissive lighting
    FLOAT fNull = 0.0f;
    m_pd3dDevice->SetPixelShaderConstantF( PSCONST_fEmissive, &fNull, 1 );

    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );

    // Prepare the mesh for rendering
    ATG::MESH_DATA* pMeshData = m_WorldMesh.GetMesh( 0 );
    m_pd3dDevice->SetVertexDeclaration( pMeshData->m_pVertexDecl );
    m_pd3dDevice->SetStreamSource( 0, &pMeshData->m_VB, 0, pMeshData->m_dwVertexSize );
    m_pd3dDevice->SetIndices( &pMeshData->m_IB );

    // Render walls and columns
    fLightingCoeffs = XMVectorSet( 0.5f, 1.0f, 5.0f, 0.0f );
    m_pd3dDevice->SetPixelShaderConstantF( PSCONST_fLightingCoeffs, ( FLOAT* )&fLightingCoeffs, 1 );
    m_pd3dDevice->SetTexture( 0, pMeshData->m_pSubsets[0].pTexture );
    m_pd3dDevice->DrawIndexedPrimitive( pMeshData->m_dwPrimType, 0, 0, 0,
                                        pMeshData->m_pSubsets[0].dwIndexStart,
                                        pMeshData->m_pSubsets[0].dwPrimitiveCount );

    // Render floor
    fLightingCoeffs = XMVectorSet( 1.0f, 3.0f, 50.0f, 0.0f );
    m_pd3dDevice->SetPixelShaderConstantF( PSCONST_fLightingCoeffs, ( FLOAT* )&fLightingCoeffs, 1 );
    m_pd3dDevice->SetTexture( 0, pMeshData->m_pSubsets[1].pTexture );
    m_pd3dDevice->DrawIndexedPrimitive( pMeshData->m_dwPrimType, 0, 0, 0,
                                        pMeshData->m_pSubsets[1].dwIndexStart,
                                        pMeshData->m_pSubsets[1].dwPrimitiveCount );

    // Render ceiling
    fLightingCoeffs = XMVectorSet( 0.3f, 0.3f, 5.0f, 0.0f );
    m_pd3dDevice->SetPixelShaderConstantF( PSCONST_fLightingCoeffs, ( FLOAT* )&fLightingCoeffs, 1 );
    m_pd3dDevice->SetTexture( 0, pMeshData->m_pSubsets[2].pTexture );
    m_pd3dDevice->DrawIndexedPrimitive( pMeshData->m_dwPrimType, 0, 0, 0,
                                        pMeshData->m_pSubsets[2].dwIndexStart,
                                        pMeshData->m_pSubsets[2].dwPrimitiveCount );

    // Render paintings
    fLightingCoeffs = XMVectorSet( 1.0f, 0.3f, 5.0f, 0.0f );
    m_pd3dDevice->SetPixelShaderConstantF( PSCONST_fLightingCoeffs, ( FLOAT* )&fLightingCoeffs, 1 );
    m_pd3dDevice->SetTexture( 0, pMeshData->m_pSubsets[3].pTexture );
    m_pd3dDevice->DrawIndexedPrimitive( pMeshData->m_dwPrimType, 0, 0, 0,
                                        pMeshData->m_pSubsets[3].dwIndexStart,
                                        pMeshData->m_pSubsets[3].dwPrimitiveCount );

    // Draw the light spheres.
    m_pd3dDevice->SetPixelShader( m_pLightSpherePS );
    fLightingCoeffs = XMVectorSet( 1.0f, 1.0f, 5.0f, 0.0f );
    m_pd3dDevice->SetPixelShaderConstantF( PSCONST_fLightingCoeffs, ( FLOAT* )&fLightingCoeffs, 1 );
    m_pd3dDevice->SetTexture( 0, m_pWhiteTexture );

    for( DWORD i = 0; i < NUM_LIGHTS; i++ )
    {
        // Just position the point light -- no need to orient it
        XMMATRIX matScale;
        matScale = XMMatrixScaling( 0.05f, 0.05f, 0.05f );

        XMMATRIX matWorld;
        XMMATRIX matObjectToView;
        matWorld = XMMatrixTranslation( m_avLightPosition[i].x, m_avLightPosition[i].y, m_avLightPosition[i].z );
        matWorld = matScale * matWorld;
        matObjectToView = matWorld * m_matView;

        XMMATRIX matT;
        matT = XMMatrixTranspose( matObjectToView );
        m_pd3dDevice->SetVertexShaderConstantF( VSCONST_mObjectToView, ( FLOAT* )&matT, 4 );

        // A light which illuminates objects at 80 lum/sr should be drawn at 3183
        // lumens/meter^2/steradian, which equates to a multiplier of 39.78 per lumen.
        FLOAT fEmissive = EMISSIVE_COEFFICIENT * m_fLightIntensity[i];
        m_pd3dDevice->SetPixelShaderConstantF( PSCONST_fEmissive, &fEmissive, 1 );

        // Render the light
        m_SphereMesh.Render( ATG::MESH_NOFVF | ATG::MESH_NOVERTEXDECL );
    }

    // End tiling, which resolves the render target to the texture
    m_pd3dDevice->EndTiling( D3DRESOLVE_RENDERTARGET0 | D3DRESOLVE_ALLFRAGMENTS |
                             D3DRESOLVE_CLEARRENDERTARGET | D3DRESOLVE_CLEARDEPTHSTENCIL,
                             NULL, m_pSceneTexture, ( D3DVECTOR4* )&ClearColor, 1.0f, 0L, NULL );

    //m_pd3dDevice->SetVertexShader( NULL );
    //m_pd3dDevice->SetPixelShader( NULL );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: MeasureLuminance()
// Desc: Measure the average log luminance in the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::MeasureLuminance()
{
    // After this pass, the m_pToneMapTexture texture will contain a single-pixel 
    // grayscale value of the log of average luminance for the HDR scene.

    // Sample initial luminance
    m_PostProcess.SampleLuminance( m_pScaledSceneTexture, TRUE, m_pToneMapTexture64x64 );

    // Downsample to 16x16
    m_PostProcess.Downsample4x4Texture( m_pToneMapTexture64x64, m_pToneMapTexture16x16 );

    // Downsample to 4x4
    m_PostProcess.Downsample4x4Texture( m_pToneMapTexture16x16, m_pToneMapTexture4x4 );

    // Downsample to 1x1
    m_PostProcess.SampleLuminance( m_pToneMapTexture4x4, FALSE, m_pToneMapTexture1x1 );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: RenderBloom()
// Desc: Render the blooming effect
//--------------------------------------------------------------------------------------
HRESULT Sample::RenderBloom()
{
    if( m_GlareDef.m_fGlareLuminance <= 0.0f || m_GlareDef.m_fBloomLuminance <= 0.0f )
    {
        m_PostProcess.ClearTexture( m_pBloomTexture );
        return S_OK;
    }

    // Render to first bloom texture (Gaussian blur 5x5 m_pBloomSourceTexture to m_pBloomTexture)
    m_PostProcess.GaussBlur5x5Texture( m_pBloomSourceTexture, m_pBloomTexture );

    // Render to second bloom texture across width (m_pBloomTexture to m_pBloomTexture)
    m_PostProcess.BloomTexture( m_pBloomTexture, TRUE, m_pBloomTexture );

    // Render to final bloom texture and height (m_pBloomTexture to m_pBloomTexture)
    m_PostProcess.BloomTexture( m_pBloomTexture, FALSE, m_pBloomTexture );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: RenderStar()
// Desc: Render the blooming effect
//--------------------------------------------------------------------------------------
HRESULT Sample::RenderStar()
{
    // Clear the star texture
    m_PostProcess.ClearTexture( m_apStarTextures[0] );

    // Avoid rendering the star if it's not being used in the current glare
    if( m_GlareDef.m_fGlareLuminance <= 0.0f || m_GlareDef.m_fStarLuminance <= 0.0f )
        return S_OK;

    // Initialize the constants used during the effect
    const        CStarDef& starDef = m_GlareDef.m_starDef;
    const        FLOAT fTanFoV = atanf( XM_PI / 8 );
    static const DWORD MAX_PASSES = 3;
    static const DWORD NUM_SAMPLES = 8;
    static       XMVECTOR s_aaColor[MAX_PASSES][8];
    static const XMVECTOR COLOR_WHITE = XMVectorSet( 0.63f, 0.63f, 0.63f, 0.0f );

    m_pd3dDevice->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_ONE );
    m_pd3dDevice->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_ONE );

    // Get the source texture dimensions
    D3DSURFACE_DESC desc;
    m_pStarSourceTexture->GetLevelDesc( 0, &desc );
    FLOAT fTextureWidth = ( FLOAT )desc.Width;
    FLOAT fTextureHeight = ( FLOAT )desc.Height;

    for( DWORD p = 0; p < MAX_PASSES; p++ )
    {
        FLOAT fRatio = ( FLOAT )( p + 1 ) / MAX_PASSES;

        for( DWORD s = 0; s < NUM_SAMPLES; s++ )
        {
            XMVECTOR ChromaticAberrColor = XMLoadFloat4( &starDef.m_avChromaticAberrationColor[s] );
            ChromaticAberrColor = XMVectorLerp( ChromaticAberrColor, COLOR_WHITE,
                                                fRatio );

            s_aaColor[p][s] = XMVectorLerp( COLOR_WHITE, ChromaticAberrColor,
                                            m_GlareDef.m_fChromaticAberration );
        }
    }

    FLOAT radOffset = m_GlareDef.m_fStarInclination + starDef.m_fInclination;

    // Direction loop
    for( DWORD d = 0; d < starDef.m_dwNumStarLines; d++ )
    {
        CONST CStarDef::STARLINE& starLine = starDef.m_pStarLine[d];

        LPDIRECT3DTEXTURE9 pWorkTexture = m_pStarSourceTexture;

        FLOAT rad = radOffset + starLine.fInclination;
        FLOAT fStepU = sinf( rad ) / fTextureWidth * starLine.fSampleLength;
        FLOAT fStepV = cosf( rad ) / fTextureHeight * starLine.fSampleLength;

        FLOAT fAttnPowScale = ( fTanFoV + 0.1f ) * 1.0f * ( 160.0f + 120.0f ) / ( fTextureWidth + fTextureHeight ) *
            1.2f;

        // 1 direction expansion loop
        m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );

        int iWorkTexture = 1;
        for( DWORD p = 0; p < starLine.dwNumPasses; p++ )
        {
            LPDIRECT3DTEXTURE9 pDstTexture;

            if( p == starLine.dwNumPasses - 1 )
                pDstTexture = m_apStarTextures[d + 4];
            else
                pDstTexture = m_apStarTextures[iWorkTexture];

            m_PostProcess.RenderStarLine( pWorkTexture, NUM_SAMPLES,
                                          starLine.fAttenuation, fAttnPowScale,
                                          s_aaColor[starLine.dwNumPasses - 1 - p], p,
                                          fStepU, fStepV, pDstTexture );

            // Setup next expansion
            fStepU *= NUM_SAMPLES;
            fStepV *= NUM_SAMPLES;
            fAttnPowScale *= NUM_SAMPLES;

            // Set the work drawn just before to next texture source.
            pWorkTexture = m_apStarTextures[iWorkTexture];

            if( ++iWorkTexture > 2 )
                iWorkTexture = 1;
        }
    }

    m_PostProcess.MergeTextures( &m_apStarTextures[4], starDef.m_dwNumStarLines, m_apStarTextures[0] );

    return S_OK;
}

