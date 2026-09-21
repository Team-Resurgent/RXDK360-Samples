//--------------------------------------------------------------------------------------
// LightShafts.cpp
//
// Sample application which illustrates a variety of advanced visual effects including:                                                      //
// - Volumetric light shafts with projective noise
// - Ambient occlusion
// - Use of a cube map for directional ambient term
// - Depth-based shadow mapping & novel filtering for variable penumbra
// - Procedural marble texture
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
// ATI 3D Application Research Group.
// Copyright (C) ATI Research, Inc. All rights reserved.
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
#include "CSpotlight.h"
#include "CBackdrop.h"
#include "COverlayQuad.h"
#include "CVolVizShells.h"


//--------------------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//--------------------------------------------------------------------------------------
ATG::HELP_CALLOUT g_HelpCallouts[] =
{
    { ATG::HELP_A_BUTTON,     ATG::HELP_PLACEMENT_2, L"Cycle spotlight\ncookie" },
    { ATG::HELP_B_BUTTON,     ATG::HELP_PLACEMENT_2, L"Toggle light\nfrustum" },
    { ATG::HELP_X_BUTTON,     ATG::HELP_PLACEMENT_1, L"Toggle atmospheric shader" },
    { ATG::HELP_RIGHT_SHOULDER, ATG::HELP_PLACEMENT_2, L"Toggle shadow\ncomputation"  },
    { ATG::HELP_LEFT_STICK,   ATG::HELP_PLACEMENT_1, L"Move camera" },
    { ATG::HELP_RIGHT_STICK,  ATG::HELP_PLACEMENT_1, L"Aim camera" },
    { ATG::HELP_DPAD,         ATG::HELP_PLACEMENT_1, L"Change spotlight size" },
    { ATG::HELP_START_BUTTON, ATG::HELP_PLACEMENT_2, L"Toggle light\nanimation"  },
    { ATG::HELP_BACK_BUTTON,  ATG::HELP_PLACEMENT_1, L"Toggle help"  },
};
#define NUM_HELP_CALLOUTS (sizeof(g_HelpCallouts) / sizeof(g_HelpCallouts[0]))


//-----------------------------------------------------------------------------
// Defines
//-----------------------------------------------------------------------------
#define SHADOW_MAP_SIZE                       512

// Initial spotlight parameters
#define SPOTLIGHT_INITIAL_WIDTH                0.25f
#define SPOTLIGHT_INITIAL_HEIGHT               0.25f
#define SPOTLIGHT_INITIAL_NEAR_PLANE           10.0f
#define SPOTLIGHT_INITIAL_FAR_PLANE           400.0f


//-----------------------------------------------------------------------------
// Globals
//-----------------------------------------------------------------------------
CHAR*                   g_strCookieFiles[] =
{
    "RadialFade",
    "HarshCircle",
    "Star",
    "TreeCanopy",
    "Holes",
    "MoreHoles"
};
const DWORD             NUM_COOKIES = ( sizeof( g_strCookieFiles ) / sizeof( g_strCookieFiles[0] ) );

LPDIRECT3DVERTEXSHADER9 g_pObjectDepthVS = NULL;
LPDIRECT3DVERTEXSHADER9 g_pObjectMainVS = NULL;
LPDIRECT3DPIXELSHADER9  g_pObjectNoiseShadowPS = NULL;

// Some global matrices accessed by other modules
XMMATRIX                g_matWorld;
XMMATRIX                g_matView;
XMMATRIX                g_matViewInv;
XMMATRIX                g_matProj;
XMMATRIX                g_matWorldView;
XMMATRIX                g_matWorldViewProj;

XMMATRIX                g_matWorldLight;
XMMATRIX                g_matWorldLightProj;
XMMATRIX                g_matWorldLightProjBias;
XMMATRIX                g_matWorldLightProjScroll1;
XMMATRIX                g_matWorldLightProjScroll2;


//--------------------------------------------------------------------------------------
// Name: class Sample
// Desc: Main class to run this application. Most functionality is inherited
//       from the ATG::Application base class.
//--------------------------------------------------------------------------------------
class Sample : public ATG::Application
{
    ATG::Timer m_Timer;    // Timer
    ATG::Font m_Font;     // Font for drawing text
    ATG::PackedResource m_Resource; // Bundled textures in a packed resource
    ATG::Help m_Help;     // Help screen

    // Scene Objects
    CSpotlight m_Spotlight;
    CBackdrop m_Backdrop;
    CShadowOverlayQuad m_ShadowMapOverlayQuad;
    CFogOverlayQuad m_FinalFogOverlayQuad;
    CVolVizShells m_VolVizShells;

    ATG::Mesh m_ObjectMesh;

    XMVECTOR m_vSceneLightPos;           // Global position for lighting the light widget(s) only
    XMVECTOR m_vModelV;                  // Viewer in model space
    XMVECTOR m_vModelL;                  // Light in model space

    // Light parameters
    XMVECTOR m_vLightPos;
    FLOAT m_fLightFOV;
    FLOAT m_fWidth;
    FLOAT m_fHeight;
    FLOAT m_fNearPlane;
    FLOAT m_fFarPlane;
    FLOAT               m_Pad0[3];

    XMVECTOR            m_WorldSpaceLightFrustumPlanes[6]; // Read these from each projective light
    XMVECTOR            m_ClipSpaceLightFrustumPlanes[6];  // Transform into here and pass these in to vol viz shells

    // Scene state
    BOOL m_bShowClippingFrustum;
    BOOL m_bShowScrollingNoise;
    BOOL m_bShowLightShafts;
    BOOL m_bAnimateLights;
    BOOL m_bShadowMapDirty;
    BOOL m_bShadowMapping;
    BOOL m_bShowHelp;

    DWORD m_dwCurrentCookie;

    FLOAT m_fSamplingDelta;     // Spacing of sampling planes

    // Transforms
    XMMATRIX m_matObject;
    XMMATRIX m_matObjectWorld;
    XMMATRIX m_matObjectWorldView;
    XMMATRIX m_matObjectWorldViewProj;
    XMMATRIX m_matObjectWorldLight;
    XMMATRIX m_matObjectWorldLightProj;
    XMMATRIX m_matObjectWorldLightProjBias;
    XMMATRIX m_matObjectWorldLightProjScroll1;
    XMMATRIX m_matObjectWorldLightProjScroll2;

    XMMATRIX m_matWorldViewProjInvT; // For transforming clip planes

    XMMATRIX m_matLight;

    // Shadow map textures and related surfaces
    LPDIRECT3DSURFACE9 m_pShadowMapZ;                          // Depth-Stencil surface for shadow map
    LPDIRECT3DTEXTURE9 m_pShadowMap;
    LPDIRECT3DTEXTURE9 m_pFilteredBlackAndWhiteShadowMap;
    LPDIRECT3DSURFACE9 m_pFilteredBlackAndWhiteShadowMapRT;

    // Fog buffer for accumulating and filtering fog
    LPDIRECT3DTEXTURE9 m_pFogBuffer;                           // RGBA - 1/4 of the planes per channel
    LPDIRECT3DSURFACE9 m_pFogBufferRT;

    // Other textures
    LPDIRECT3DTEXTURE9  m_pCookies[NUM_COOKIES];                // RGBA cookie textures
    LPDIRECT3DTEXTURE9 m_pScrollingNoise;                      // Tilable RGBA noise texture

    // Textures for shaded object
    LPDIRECT3DTEXTURE9 m_pMarbleColorSplineTexture;       // 1D marble spline texture
    LPDIRECT3DTEXTURE9 m_pAmbientOcclusion;               // 2D texture generated offline which encodes occlusion due to local geometry
    LPDIRECT3DVOLUMETEXTURE9 m_pVolumeNoise;                    // Tilable grayscale 3D noise texture
    LPDIRECT3DCUBETEXTURE9 m_pAmbientCube;                    // Diffuse cube map for low-frequency color bleed from environment

    HRESULT             InitNoiseTexture();

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
    ATG::GetVideoSettings( &atgApp.m_d3dpp.BackBufferWidth, &atgApp.m_d3dpp.BackBufferHeight );

    // Make sure display is gamma correct.
    atgApp.m_d3dpp.BackBufferFormat =  ( D3DFORMAT )MAKESRGBFMT( D3DFMT_A8R8G8B8 );
    atgApp.m_d3dpp.FrontBufferFormat = ( D3DFORMAT )MAKESRGBFMT( D3DFMT_LE_X8R8G8B8 );

    atgApp.Run();
}


//--------------------------------------------------------------------------------------
// Name: Initialize()
// Desc: Initialize app-dependent objects
//--------------------------------------------------------------------------------------
HRESULT Sample::Initialize()
{
    HRESULT hr;

    // Default scene state
    m_bShowClippingFrustum = FALSE;
    m_bAnimateLights = TRUE;
    m_bShowScrollingNoise = TRUE;
    m_bShowLightShafts = TRUE;
    m_bShadowMapping = TRUE;
    m_bShadowMapDirty = FALSE;
    m_bShowHelp = FALSE;

    // Null out textures and surfaces
    m_pShadowMap = NULL;
    m_pShadowMapZ = NULL;

    m_pFilteredBlackAndWhiteShadowMapRT = NULL;

    m_pFogBuffer = NULL;
    m_pFogBufferRT = NULL;

    m_pFilteredBlackAndWhiteShadowMap = NULL;
    m_pVolumeNoise = NULL;
    m_pAmbientCube = NULL;
    m_pMarbleColorSplineTexture = NULL;
    m_pAmbientOcclusion = NULL;
    m_pScrollingNoise = NULL;

    for( DWORD i = 0; i < NUM_COOKIES; i++ )
        m_pCookies[i] = NULL;

    m_dwCurrentCookie = 0;

    m_vSceneLightPos = XMVectorSet( 500.0f, 500.0f, -300.0f, 1.0f );

    m_fLightFOV = XM_PI / 4.0f;
    m_fWidth = SPOTLIGHT_INITIAL_WIDTH;
    m_fHeight = SPOTLIGHT_INITIAL_HEIGHT;
    m_fNearPlane = SPOTLIGHT_INITIAL_NEAR_PLANE;
    m_fFarPlane = SPOTLIGHT_INITIAL_FAR_PLANE;

    // Sampling planes spaced this distance apart
    m_fSamplingDelta = 0.01f;

    // Create the font
    if( FAILED( hr = m_Font.Create( "game:\\Media\\Fonts\\Arial_16.xpr" ) ) )
    {
        ATG_PrintError( "Couldn't create font\n" );
        return hr;
    }

    // Confine text drawing to the title safe area
    m_Font.SetWindow( ATG::GetTitleSafeArea() );

    // Create the help resource
    if( FAILED( hr = m_Help.Create( "game:\\Media\\Help\\Help.xpr" ) ) )
    {
        ATG_PrintError( "Couldn't create help\n" );
        return hr;
    }

    // Create the textures resource
    if( FAILED( hr = m_Resource.Create( "game:\\Media\\Resource.xpr" ) ) )
    {
        ATG_PrintError( "Couldn't create Resource.xpr\n" );
        return hr;
    }

    if( FAILED( hr = m_ObjectMesh.Create( "game:\\Media\\Meshes\\Hebe.xbg" ) ) )
    {
        ATG_PrintError( "Couldn't create object mesh\n" );
        return hr;
    }

    // Set light position used to light the little frustum geometry
    m_Spotlight.SetSceneLightPos( m_vSceneLightPos );

    // Initialize the objects
    if( FAILED( hr = m_Spotlight.Initialize() ) )
        return hr;
    if( FAILED( hr = m_Backdrop.Initialize( m_Resource.GetTexture( "Backdrop" ) ) ) )
        return hr;
    if( FAILED( hr = m_ShadowMapOverlayQuad.Initialize() ) )
        return hr;
    if( FAILED( hr = m_VolVizShells.Initialize() ) )
        return hr;
    if( FAILED( hr = m_FinalFogOverlayQuad.Initialize() ) )
        return hr;

    // Create textures
    for( DWORD i = 0; i < NUM_COOKIES; i++ )
    {
        m_pCookies[i] = m_Resource.GetTexture( g_strCookieFiles[i] );
    }

    m_pScrollingNoise = m_Resource.GetTexture( "Noise" );

    D3DSURFACE_PARAMETERS SurfaceParameters = {0};
    SurfaceParameters.Base = 0;

    // NOTE: we could try switching to a higher-precision backbuffer, like 2:10:10:10 or
    // 2:7e3:7e3:7e3. Then, remove the use of a fog buffer, altogether, and render the
    // VolVizShells directly into the backbuffer.
    if( FAILED( m_pd3dDevice->CreateTexture( m_d3dpp.BackBufferWidth, m_d3dpp.BackBufferHeight, 1,
                                             D3DUSAGE_RENDERTARGET, D3DFMT_L16, D3DPOOL_DEFAULT,
                                             //                                           D3DUSAGE_RENDERTARGET, D3DFMT_L8, D3DPOOL_DEFAULT,
                                             &m_pFogBuffer, NULL ) ) )
        return E_FAIL;

    if( FAILED( m_pd3dDevice->CreateRenderTarget( m_d3dpp.BackBufferWidth, m_d3dpp.BackBufferHeight,
                                                  D3DFMT_G16R16_EDRAM, D3DMULTISAMPLE_NONE, 0, TRUE,
                                                  &m_pFogBufferRT, &SurfaceParameters ) ) )
        return E_FAIL;

    if( FAILED( m_pd3dDevice->CreateTexture( SHADOW_MAP_SIZE, SHADOW_MAP_SIZE, 1,
                                             D3DUSAGE_RENDERTARGET, D3DFMT_D24S8, D3DPOOL_DEFAULT,
                                             &m_pShadowMap, NULL ) ) )
        return E_FAIL;

    if( FAILED( m_pd3dDevice->CreateDepthStencilSurface( SHADOW_MAP_SIZE, SHADOW_MAP_SIZE,
                                                         D3DFMT_D24S8, D3DMULTISAMPLE_NONE, 0,
                                                         TRUE, &m_pShadowMapZ, NULL ) ) )
        return E_FAIL;

    // The shadow map is filtered into m_pFilteredBlackAndWhiteShadowMap using a
    // growable Poisson filter to simulate a blurry penumbras farther from the object.
    // See "Poisson Shadow Blur" chapter in ShaderX3, Charles River Media 2004

    // TODO: Generate mipmaps for the m_pFilteredBlackAndWhiteShadowMap texture

    if( FAILED( m_pd3dDevice->CreateTexture( SHADOW_MAP_SIZE, SHADOW_MAP_SIZE, 1,
                                             D3DUSAGE_RENDERTARGET,
                                             //                                           D3DFMT_L8, D3DPOOL_DEFAULT, &m_pFilteredBlackAndWhiteShadowMap, NULL ) ) )
                                             D3DFMT_L16, D3DPOOL_DEFAULT, &m_pFilteredBlackAndWhiteShadowMap,
                                             NULL ) ) )
        return E_FAIL;

    if( FAILED( m_pd3dDevice->CreateRenderTarget( SHADOW_MAP_SIZE, SHADOW_MAP_SIZE,
                                                  D3DFMT_A8R8G8B8, D3DMULTISAMPLE_NONE, 0, TRUE,
                                                  &m_pFilteredBlackAndWhiteShadowMapRT, &SurfaceParameters ) ) )
        return E_FAIL;

    // Textures for procedural shading

    // Create and fill the spline textures
    m_pMarbleColorSplineTexture = m_Resource.GetTexture( "MarbleSpline" );

    // Create and load the volume noise texture
    if( FAILED( InitNoiseTexture() ) )
        return E_FAIL;

    // Maps for ambient radiosity and ambient occlusion

    // Create and load the ambient cube map
    m_pAmbientCube = m_Resource.GetCubemap( "DiffuseCubemap" );

    // Create the ambient occlusion map
    m_pAmbientOcclusion = m_Resource.GetTexture( "HebeWorldNmAmbiOccl1024" );

    // Set up the spotlight object...
    m_Spotlight.SetView( XMVectorSet( 0.0f, 210.0f, 0.0f, 1.0f ),   // vEyePt
                         XMVectorSet( 0.0f, 50.0f, 0.0f, 1.0f ),   // vLookAtPt
                         XMVectorSet( 0.0f, 1.0f, 0.0f, 1.0f ),   // vUpVec
                         m_fWidth, m_fHeight, m_fNearPlane, m_fFarPlane );

    // Initialize spotlight parameters
    m_Spotlight.SetWidth( m_fWidth );
    m_Spotlight.SetHeight( m_fHeight );
    m_Spotlight.SetCookie( m_pCookies[m_dwCurrentCookie] );

    // Create vertex declaration
    if( FAILED( ATG::LoadVertexShader( "game:\\Media\\Shaders\\ObjectMainVS.xvu", &g_pObjectMainVS ) ) ) return E_FAIL;
    if( FAILED( ATG::LoadVertexShader( "game:\\Media\\Shaders\\ObjectDepthVS.xvu", &g_pObjectDepthVS ) ) ) return
            E_FAIL;
    if( FAILED( ATG::LoadPixelShader( "game:\\Media\\Shaders\\ObjectNoiseShadowPS.xpu",
                                      &g_pObjectNoiseShadowPS ) ) ) return E_FAIL;

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: InitNoiseTexture()
// Desc: Creates mip-mapped volume noise texture and loads noise from pre-authored
//       volumetric noise map which is authored to tile correctly.
//--------------------------------------------------------------------------------------
HRESULT Sample::InitNoiseTexture()
{
    // TODO: Load the noise volume as a bundled, mip-mapped volume texture
#if 0
    m_pVolumeNoise = m_Resource.GetVolumeTexture( "NoiseVolume" );
#else
    if( FAILED( D3DXCreateVolumeTextureFromFileEx( m_pd3dDevice, "game:\\Media\\Textures\\LightShafts_NoiseVolume.dds",
                                                   D3DX_DEFAULT, D3DX_DEFAULT, D3DX_DEFAULT,
                                                   1, 0, D3DFMT_L8, D3DPOOL_MANAGED, D3DX_DEFAULT, D3DX_DEFAULT,
                                                   0xff000000, NULL, NULL, &m_pVolumeNoise ) ) )
        return E_FAIL;

    if( FAILED( D3DXFilterVolumeTexture( m_pVolumeNoise, NULL, 0, D3DX_DEFAULT ) ) )
        return E_FAIL;
#endif

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
    FLOAT fElapsedTime = ( FLOAT )m_Timer.GetElapsedTime();

    // Get the current gamepad state
    ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput();

    // Let the user pause the animation
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_START )
    {
        static BOOL bPaused = FALSE;
        bPaused = !bPaused;

        if( bPaused )   m_Timer.Stop();
        else
            m_Timer.Start();

        m_bAnimateLights = !m_bAnimateLights;
    }

    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
        m_bShowHelp = !m_bShowHelp;

    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_A )
    {
        m_dwCurrentCookie = ( m_dwCurrentCookie + 1 ) % NUM_COOKIES;
        m_Spotlight.SetCookie( m_pCookies[m_dwCurrentCookie] );
    }

    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_B )
        m_bShowClippingFrustum = !m_bShowClippingFrustum;

    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_X )
        m_bShowLightShafts = !m_bShowLightShafts;

    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_LEFT_SHOULDER )
        m_bShowScrollingNoise = !m_bShowScrollingNoise;

    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER )
    {
        m_bShadowMapping = !m_bShadowMapping;
        m_bShadowMapDirty = TRUE;
    }

    if( pGamepad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT )
    {
        m_bShadowMapDirty = TRUE;
        m_fWidth = min( m_fWidth + 1.0f * fElapsedTime, 1.5f );
        m_Spotlight.SetWidth( m_fWidth );
    }

    if( pGamepad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT )
    {
        m_bShadowMapDirty = TRUE;
        m_fWidth = max( 0.02f, m_fWidth - 1.0f * fElapsedTime );
        m_Spotlight.SetWidth( m_fWidth );
    }

    if( pGamepad->wButtons & XINPUT_GAMEPAD_DPAD_UP )
    {
        m_bShadowMapDirty = TRUE;
        m_fHeight = min( m_fHeight + 1.0f * fElapsedTime, 1.5f );
        m_Spotlight.SetHeight( m_fHeight );
    }

    if( pGamepad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN )
    {
        m_bShadowMapDirty = TRUE;
        m_fHeight = max( 0.02f, m_fHeight - 1.0f * fElapsedTime );
        m_Spotlight.SetHeight( m_fHeight );
    }

    // Static View Matrix
    XMMATRIX matStartView;
    XMVECTOR vEyePt = XMVectorSet( 0.0f, SPOTLIGHT_INITIAL_FAR_PLANE, -SPOTLIGHT_INITIAL_FAR_PLANE, 1.0f );
    XMVECTOR vLookatPt = XMVectorSet( 0.0f, 0.0f, 0.0f, 1.0f );
    XMVECTOR vUpVec = XMVectorSet( 0.0f, 1.0f, 0.0f, 1.0f );
    matStartView = XMMatrixLookAtLH( vEyePt, vLookatPt, vUpVec );

    // Make light orbit the scene
    m_vLightPos = XMVectorSet( 140 * sinf( fTime / 2.5f ), 260.0f, 140 * cosf( fTime / 2.5f ), 1.0f );

    m_Spotlight.SetView( m_vLightPos,                             // vEyePt
                         XMVectorSet( 0.0f, 100.0f, 0.0f, 1.0f ), // vLookAtPt
                         XMVectorSet( 0.0f, 1.0f, 0.0f, 1.0f ), // vUpVec
                         m_fWidth, m_fHeight, m_fNearPlane, m_fFarPlane );

    // Static Projection Matrix for main viewport
    FLOAT fAspectRatio = ( ( FLOAT )m_d3dpp.BackBufferWidth ) / m_d3dpp.BackBufferHeight;
    g_matProj = XMMatrixPerspectiveFovLH( XM_PI / 4.0f, fAspectRatio, 5.0f, 2000.0f );

    // Move the camera
    static XMVECTOR vFromPt = XMVectorSet( 0.0f, SPOTLIGHT_INITIAL_FAR_PLANE, -SPOTLIGHT_INITIAL_FAR_PLANE, 1.0f );
    static FLOAT fCameraTheta = XM_PI / 4;
    static FLOAT fCameraPhi = 0.0f;
    const  FLOAT fCameraSpeed = 200.0f;
    {
        // Rotate the camera
        fCameraTheta -= pGamepad->fY2 * fElapsedTime;
        fCameraPhi += pGamepad->fX2 * fElapsedTime;
        fCameraTheta = min( max( fCameraTheta, -XM_PI / 2.5f ), +XM_PI / 2.5f );
        XMMATRIX matRotateX = XMMatrixRotationX( fCameraTheta );
        XMMATRIX matRotateY = XMMatrixRotationY( fCameraPhi );
        XMMATRIX matRotate = XMMatrixMultiply( matRotateX, matRotateY );
        XMVECTOR vForward = XMVector3TransformCoord( XMVectorSet( 0, 0, 1, 1 ), matRotate );
        XMVECTOR vUp = XMVector3TransformCoord( XMVectorSet( 0, 1, 0, 1 ), matRotate );
        XMVECTOR vCross = XMVector3TransformCoord( XMVectorSet( 1, 0, 0, 1 ), matRotate );
        vForward = XMVector3Normalize( vForward );
        vUp = XMVector3Normalize( vUp );
        vCross = XMVector3Normalize( vCross );

        // Translate the camera
        vFromPt += pGamepad->fX1 * fCameraSpeed * fElapsedTime * vCross;
        vFromPt += pGamepad->fY1 * fCameraSpeed * fElapsedTime * vForward;

        // Build the view matrix
        vLookatPt = vFromPt + vForward;
        g_matView = XMMatrixLookAtLH( vFromPt, vLookatPt, vUp );
    }

    // Global matrices for the scene
    XMVECTOR vDeterminant;
    g_matWorld = XMMatrixIdentity();
    g_matWorldView = XMMatrixMultiply( g_matWorld, g_matView );
    g_matWorldViewProj = XMMatrixMultiply( g_matWorldView, g_matProj );
    g_matViewInv = XMMatrixInverse( &vDeterminant, g_matView );       // Transformation from view space to world space

    // Compose scale and bias matrix for projective texture
    static FLOAT fNoiseTime = 0.0f;
    fNoiseTime += fElapsedTime;
    XMMATRIX matHalfScale = XMMatrixScaling( 0.5f, -0.5f, 0.5f );  // Funny hack to fix up projection...not 100% sure why this works
    XMMATRIX matHalfBias = XMMatrixTranslation( 0.5f, 0.5f, 0.5f );
    XMMATRIX matBias = XMMatrixMultiply( matHalfScale, matHalfBias );
    XMMATRIX matScroll1 = XMMatrixTranslation( +fmodf( 0.0117f * fNoiseTime, 1.0f ), -fmodf( 0.029f * fNoiseTime,
                                                                                             1.0f ), 0.5f );
    XMMATRIX matScroll2 = XMMatrixTranslation( -fmodf( 0.0113f * fNoiseTime, 1.0f ), -fmodf( 0.027f * fNoiseTime,
                                                                                             1.0f ), 0.5f );
    matScroll1 = XMMatrixMultiply( XMMatrixScaling( 0.6f, 0.6f, 0.1f ), matScroll1 );
    matScroll2 = XMMatrixMultiply( XMMatrixScaling( 0.6f, 0.6f, 0.1f ), matScroll2 );

    // Light matrices for scene
    m_matLight = m_Spotlight.GetViewMatrix();
    g_matWorldLight = XMMatrixMultiply( g_matWorld, m_matLight );
    g_matWorldLightProj = XMMatrixMultiply( g_matWorldLight, m_Spotlight.GetProjectionMatrix() );
    g_matWorldLightProjBias = XMMatrixMultiply( g_matWorldLightProj, matBias );
#if 0 // Radially symmetric noise about the light view direction
    g_matWorldLightProjScroll1 = XMMatrixMultiply( g_matWorldLightProj, matScroll1 );
    g_matWorldLightProjScroll2 = XMMatrixMultiply( g_matWorldLightProj, matScroll2 );
#else // Globally positioned noise independant of camera view and light view directions
    g_matWorldLightProjScroll1 = XMMatrixMultiply( g_matWorld, XMMatrixLookAtLH( XMVectorSet( 0, 400, -400, 1 ),
                                                                                 XMVectorSet( 0, 0, 0, 1 ),
                                                                                 XMVectorSet( 0, 1, 0, 1 ) ) );
    g_matWorldLightProjScroll2 = XMMatrixMultiply( g_matWorld, XMMatrixLookAtLH( XMVectorSet( 0, 400, -400, 1 ),
                                                                                 XMVectorSet( 0, 0, 0, 1 ),
                                                                                 XMVectorSet( 0, 1, 0, 1 ) ) );
    g_matWorldLightProjScroll1 = XMMatrixMultiply( g_matWorldLightProjScroll1, g_matProj );
    g_matWorldLightProjScroll2 = XMMatrixMultiply( g_matWorldLightProjScroll2, g_matProj );
    g_matWorldLightProjScroll1 = XMMatrixMultiply( g_matWorldLightProjScroll1, matScroll1 );
    g_matWorldLightProjScroll2 = XMMatrixMultiply( g_matWorldLightProjScroll2, matScroll2 );
#endif

    // Global matrices for the object
    m_matObject = XMMatrixIdentity();
    m_matObjectWorld = XMMatrixMultiply( m_matObject, g_matWorld );
    m_matObjectWorldView = XMMatrixMultiply( m_matObject, g_matWorldView );
    m_matObjectWorldViewProj = XMMatrixMultiply( m_matObject, g_matWorldViewProj );

    // Light matrices for object
    m_matObjectWorldLight = XMMatrixMultiply( m_matObject, g_matWorldLight );
    m_matObjectWorldLightProj = XMMatrixMultiply( m_matObject, g_matWorldLightProj );

    // Texture matrices for the object
    m_matObjectWorldLightProjBias = XMMatrixMultiply( m_matObject, g_matWorldLightProjBias );
    m_matObjectWorldLightProjScroll1 = XMMatrixMultiply( m_matObject, g_matWorldLightProjScroll1 );
    m_matObjectWorldLightProjScroll2 = XMMatrixMultiply( m_matObject, g_matWorldLightProjScroll2 );

    // Model space light and viewer
    XMVECTOR vOrigin = XMVectorZero();
    XMVECTOR vWorldV = XMVector3TransformCoord( vOrigin, g_matViewInv );   // World space viewer
    XMMATRIX matObjectInv = XMMatrixInverse( &vDeterminant, m_matObjectWorld );
    m_vModelV = XMVector3TransformCoord( vWorldV, matObjectInv ); // Model space viewer
    m_vModelL = XMVector3TransformCoord( m_vLightPos, matObjectInv ); // Model space light

    // Matrix for transforming clip planes from worldspace to clip space
    m_matWorldViewProjInvT = XMMatrixInverse( &vDeterminant, g_matWorldViewProj );
    m_matWorldViewProjInvT = XMMatrixTranspose( m_matWorldViewProjInvT );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Render()
// Desc: Called once per frame, the call is the entry point for 3D rendering. This
//       function sets up render states, clears the viewport, and renders the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Render()
{
    // Save old render target
    LPDIRECT3DSURFACE9 pBackBuffer, pDepthBuffer;
    m_pd3dDevice->GetRenderTarget( 0, &pBackBuffer );
    m_pd3dDevice->GetDepthStencilSurface( &pDepthBuffer );
    pBackBuffer->Release();
    pDepthBuffer->Release();

    //-----------------------------------------------------------------------------------
    // Draw to the shadow map if light or object is moving or FOV just changed
    //-----------------------------------------------------------------------------------
    if( m_bShadowMapping && ( m_bShadowMapDirty || m_bAnimateLights ) )
    {
        m_bShadowMapDirty = FALSE;

        //------------------------------------------------------------------------------
        // Render to the shadow map
        //------------------------------------------------------------------------------

        // TODO: On final hardware, we'd prefer to just render z-only, and resolve only
        // the z-buffer.
        m_pd3dDevice->SetRenderTarget( 0, NULL );
        m_pd3dDevice->SetDepthStencilSurface( m_pShadowMapZ );

        // Clear the surface to white
        m_pd3dDevice->Clear( 0, NULL, D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL, 0, 1.0f, 0L );

        // First render the object
        {
            m_pd3dDevice->SetVertexShader( g_pObjectDepthVS );
            m_pd3dDevice->SetPixelShader( NULL );
            m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );
            m_pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE, TRUE );

            XMMATRIX m = XMMatrixTranspose( m_matObjectWorldLightProj );
            m_pd3dDevice->SetVertexShaderConstantF( 0, ( FLOAT* )&m, 4 );
            m_ObjectMesh.Render( 0L );
        }

        m_pd3dDevice->Resolve( D3DRESOLVE_DEPTHSTENCIL, NULL, m_pShadowMap, NULL,
                               0, 0, NULL, 1.0f, 0L, NULL );

        m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, FALSE );

        //------------------------------------------------------------------------------
        // Render to Filtered Black And White Shadow Map
        //------------------------------------------------------------------------------

        m_pd3dDevice->SetRenderTarget( 0, m_pFilteredBlackAndWhiteShadowMapRT );

        // Clear the high-precision scalar buffer to the maximum value
        m_ShadowMapOverlayQuad.Draw( m_pShadowMap );

        m_pd3dDevice->Resolve( D3DRESOLVE_RENDERTARGET0, NULL, m_pFilteredBlackAndWhiteShadowMap, NULL,
                               0, 0, NULL, 1.0f, 0L, NULL );

        //TODO: Generate the miplevels for the m_pFilteredBlackAndWhiteShadowMap

        m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );
    }

    //-----------------------------------------------------------------------------
    // Z-Prepass: render the scene, depth only
    //-----------------------------------------------------------------------------
    {
        m_pd3dDevice->SetRenderTarget( 0, NULL );
        m_pd3dDevice->SetDepthStencilSurface( pDepthBuffer );

        // Clear the depth buffer
        m_pd3dDevice->Clear( 0, NULL, D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL, 0x00000000, 1.0f, 0 );

        {
            m_pd3dDevice->SetVertexShader( g_pObjectDepthVS );
            m_pd3dDevice->SetPixelShader( NULL );

            // Set up the necessary transforms
            XMMATRIX m = XMMatrixTranspose( m_matObjectWorldViewProj );
            m_pd3dDevice->SetVertexShaderConstantF( 0, ( FLOAT* )&m, 4 );
            m_ObjectMesh.Render( 0L );
        }

        // Draw the backdrop
        m_Backdrop.SetWorldViewProjMatrix( g_matWorldViewProj );
        m_Backdrop.SetFarPlane( m_fFarPlane );
        m_Backdrop.DrawDepthOnly();
    }

    //--------------------------------------------------------------------------------------
    // Draw to the fog buffer
    //--------------------------------------------------------------------------------------

    if( m_bShowLightShafts )
    {
        // Save old render target
        m_pd3dDevice->SetRenderTarget( 0, m_pFogBufferRT );

        // Clear the surface
        m_pd3dDevice->Clear( 0, NULL, D3DCLEAR_TARGET, 0x00000000, 1.0f, 0 );

        // Set volviz shells to span view-aligned parallelepiped which bounds the light frustum, spacing planes a fixed distance apart
        XMVECTOR vMinBounds, vMaxBounds;
        m_Spotlight.GetViewSpaceBounds( vMinBounds, vMaxBounds );
        m_VolVizShells.SetVolVizBounds( vMinBounds, vMaxBounds, m_fSamplingDelta );

        // Get the world-space clipping planes for this light
        m_Spotlight.GetWorldSpaceFrustumPlanes( m_WorldSpaceLightFrustumPlanes );

        // Transform the clip planes to clip-space for the viewer
        for( DWORD i = 0; i < 6; i++ )
        {
            m_ClipSpaceLightFrustumPlanes[i] = XMPlaneTransform( m_WorldSpaceLightFrustumPlanes[i],
                                                                 m_matWorldViewProjInvT );
        }

        // Draw the volviz shells which make up the light shafts
        m_VolVizShells.SetClipPlanes( m_ClipSpaceLightFrustumPlanes );
        m_VolVizShells.SetWorldViewProjMatrix( g_matWorldViewProj );
        m_VolVizShells.SetWorldLightProjMatrix( g_matWorldLightProj );
        m_VolVizShells.SetWorldLightMatrix( g_matWorldLight );
        m_VolVizShells.SetLightProjBiasMatrix( g_matWorldLightProjBias );
        m_VolVizShells.SetLightProjScrollMatrices( g_matWorldLightProjScroll1, g_matWorldLightProjScroll2 );
        m_VolVizShells.SetTextures( m_Spotlight.GetCookie(), m_pScrollingNoise, m_pShadowMap );
        m_VolVizShells.SetFarPlane( m_fFarPlane );

        m_VolVizShells.Draw( m_bShowScrollingNoise, m_bShadowMapping );

        m_pd3dDevice->Resolve( D3DRESOLVE_RENDERTARGET0, NULL, m_pFogBuffer, NULL,
                               0, 0, NULL, 1.0f, 0L, NULL );
    }

    //--------------------------------------------------------------------------------------
    // Draw to the back buffer
    //--------------------------------------------------------------------------------------

    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );
    m_pd3dDevice->SetRenderTarget( 0, pBackBuffer );

    // Clear the whole window
    m_pd3dDevice->Clear( 0L, NULL, D3DCLEAR_TARGET, 0x09090909, 1.0f, 0L );

    // First render the object

    // Select the proper technique
    m_pd3dDevice->SetPixelShaderConstantB( 1, &m_bShowScrollingNoise, 1 );
    m_pd3dDevice->SetPixelShaderConstantB( 2, &m_bShadowMapping, 1 );
    m_pd3dDevice->SetVertexShader( g_pObjectMainVS );
    m_pd3dDevice->SetPixelShader( g_pObjectNoiseShadowPS );

    m_pd3dDevice->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_DESTCOLOR );
    m_pd3dDevice->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_ZERO );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );

    // Set up the necessary transforms
    XMMATRIX m;
    m = XMMatrixTranspose( m_matObjectWorldViewProj );
    m_pd3dDevice->SetVertexShaderConstantF( 0, ( FLOAT* )&m, 4 );
    m = XMMatrixTranspose( m_matObjectWorldLightProj );
    m_pd3dDevice->SetVertexShaderConstantF( 4, ( FLOAT* )&m, 4 );
    m = XMMatrixTranspose( m_matObjectWorldLightProjBias );
    m_pd3dDevice->SetVertexShaderConstantF( 20, ( FLOAT* )&m, 4 );
    m = XMMatrixTranspose( m_matObjectWorldLightProjScroll1 );
    m_pd3dDevice->SetVertexShaderConstantF( 24, ( FLOAT* )&m, 4 );
    m = XMMatrixTranspose( m_matObjectWorldLightProjScroll2 );
    m_pd3dDevice->SetVertexShaderConstantF( 28, ( FLOAT* )&m, 4 );

    // Set up the model space quantities for lighting
    m_pd3dDevice->SetPixelShaderConstantF( 0, ( FLOAT* )&m_vModelL, 1 );
    m_pd3dDevice->SetPixelShaderConstantF( 1, ( FLOAT* )&m_vModelV, 1 );

    // Set up the necessary textures
    const DWORD tCookie = 1;
    const DWORD tScrollingNoise = 2;
    const DWORD tShadowMap = 3;
    const DWORD tAmbientOcclusion = 4;
    const DWORD tAmbientCube = 5;
    const DWORD tVolumeNoise = 6;
    const DWORD tMarbleSpline = 7;

    m_pd3dDevice->SetTexture( tCookie, m_Spotlight.GetCookie() );
    m_pd3dDevice->SetSamplerState( tCookie, D3DSAMP_MAXANISOTROPY, 8 );
    m_pd3dDevice->SetSamplerState( tCookie, D3DSAMP_MINFILTER, D3DTEXF_ANISOTROPIC );
    m_pd3dDevice->SetSamplerState( tCookie, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState( tCookie, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState( tCookie, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP );
    m_pd3dDevice->SetSamplerState( tCookie, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP );

    m_pd3dDevice->SetTexture( tScrollingNoise, m_pScrollingNoise );
    m_pd3dDevice->SetSamplerState( tScrollingNoise, D3DSAMP_MAXANISOTROPY, 8 );
    m_pd3dDevice->SetSamplerState( tScrollingNoise, D3DSAMP_MINFILTER, D3DTEXF_ANISOTROPIC );
    m_pd3dDevice->SetSamplerState( tScrollingNoise, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState( tScrollingNoise, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState( tScrollingNoise, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP );
    m_pd3dDevice->SetSamplerState( tScrollingNoise, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP );

    m_pd3dDevice->SetTexture( tShadowMap, m_pShadowMap );
    m_pd3dDevice->SetSamplerState( tShadowMap, D3DSAMP_MINFILTER, D3DTEXF_POINT );
    m_pd3dDevice->SetSamplerState( tShadowMap, D3DSAMP_MAGFILTER, D3DTEXF_POINT );
    m_pd3dDevice->SetSamplerState( tShadowMap, D3DSAMP_MIPFILTER, D3DTEXF_NONE );
    m_pd3dDevice->SetSamplerState( tShadowMap, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP );
    m_pd3dDevice->SetSamplerState( tShadowMap, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP );

    m_pd3dDevice->SetTexture( tAmbientOcclusion, m_pAmbientOcclusion );
    m_pd3dDevice->SetSamplerState( tAmbientOcclusion, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState( tAmbientOcclusion, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState( tAmbientOcclusion, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState( tAmbientOcclusion, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP );
    m_pd3dDevice->SetSamplerState( tAmbientOcclusion, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP );

    m_pd3dDevice->SetTexture( tAmbientCube, m_pAmbientCube );
    m_pd3dDevice->SetSamplerState( tAmbientCube, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState( tAmbientCube, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState( tAmbientCube, D3DSAMP_MIPFILTER, D3DTEXF_NONE );
    m_pd3dDevice->SetSamplerState( tAmbientCube, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP );
    m_pd3dDevice->SetSamplerState( tAmbientCube, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP );

    m_pd3dDevice->SetTexture( tVolumeNoise, m_pVolumeNoise );
    m_pd3dDevice->SetSamplerState( tVolumeNoise, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState( tVolumeNoise, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState( tVolumeNoise, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState( tVolumeNoise, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP );
    m_pd3dDevice->SetSamplerState( tVolumeNoise, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP );
    m_pd3dDevice->SetSamplerState( tVolumeNoise, D3DSAMP_ADDRESSW, D3DTADDRESS_WRAP );

    m_pd3dDevice->SetTexture( tMarbleSpline, m_pMarbleColorSplineTexture );
    m_pd3dDevice->SetSamplerState( tMarbleSpline, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState( tMarbleSpline, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState( tMarbleSpline, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetSamplerState( tMarbleSpline, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP );
    m_pd3dDevice->SetSamplerState( tMarbleSpline, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP );

    m_ObjectMesh.Render( 0L );

    // Draw the backdrop with texture projected on it
    m_Backdrop.SetWorldViewProjMatrix( g_matWorldViewProj );
    m_Backdrop.SetWorldLightMatrix( g_matWorldLight );
    m_Backdrop.SetLightProjBiasMatrix( g_matWorldLightProjBias );
    m_Backdrop.SetLightProjScrollMatrices( g_matWorldLightProjScroll1, g_matWorldLightProjScroll2 );
    m_Backdrop.SetTextureHandles( m_Spotlight.GetCookie(), m_pScrollingNoise, m_pFilteredBlackAndWhiteShadowMap );
    m_Backdrop.SetFarPlane( m_fFarPlane );

    // Draw the backdrop
    m_Backdrop.Draw( m_bShowScrollingNoise, m_bShadowMapping );

    // Draw the light
    m_Spotlight.Draw();

    if( m_bShowClippingFrustum )
        m_Spotlight.DrawClippingFrustum();

    //--------------------------------------------------------------------------------------
    // Draw overlay quads to the back buffer
    //--------------------------------------------------------------------------------------

    if( m_bShowLightShafts )
    {
        // Composite quad onto back buffer
        m_FinalFogOverlayQuad.Draw( m_pFogBuffer );
    }

    //--------------------------------------------------------------------------------------
    // Output title and framerate
    //--------------------------------------------------------------------------------------
    m_Timer.MarkFrame();

    if( m_bShowHelp )
    {
        m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
    }
    else
    {
        m_Font.Begin();
        m_Font.SetScaleFactors( 1.2f, 1.2f );
        m_Font.DrawText( 0, 0, 0xffffffff, L"LightShafts" );
        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( 0, 0, 0xffffff00, m_Timer.GetFrameRate(), ATGFONT_RIGHT );

        // Display the total time the app has been running
        DOUBLE fAppTimeInSeconds = m_Timer.GetAppTime();
        DOUBLE fAppTimeInMinutes = fAppTimeInSeconds / 60.0;
        DOUBLE fAppTimeInHours = fAppTimeInMinutes / 60.0;
        DOUBLE fAppTimeInDays = fAppTimeInHours / 24.0;

        DWORD dwSeconds = ( DWORD )( floor( fAppTimeInSeconds ) ) % 60;
        DWORD dwMinutes = ( DWORD )( floor( fAppTimeInMinutes ) ) % 60;
        DWORD dwHours = ( DWORD )( floor( fAppTimeInHours ) ) % 24;
        DWORD dwDays = ( DWORD )( floor( fAppTimeInDays ) );

        WCHAR strTime[80];
        swprintf_s( strTime, L"%02ldd%02ldh%02ldm%02lds",
                    dwDays, dwHours, dwMinutes, dwSeconds );
        m_Font.DrawText( 0, 20, 0xffffff00, strTime, ATGFONT_RIGHT );

        m_Font.End();
    }

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}

