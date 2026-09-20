//--------------------------------------------------------------------------------------
// Main.cpp
//
// Entry point and update/render loop for the TiledResources sample.
//
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include <xtl.h>
#include <xgraphics.h>
#include <xboxmath.h>
#include <AtgFont.h>
#include <AtgInput.h>
#include <AtgUtil.h>
#include <AtgHelp.h>
#include <AtgApp.h>
#include <AtgSimpleShaders.h>
#include <AtgDebugDraw.h>
#include <AtgSceneAll.h>

#include "SceneObject.h"
#include "ResidencySampleRender.h"
#include "TitleResidencyManager.h"
#include "SamplingQualityManager.h"
#include "PageLoaders.h"
#include "TerrainView.h"

// Piecewise linear gamma table for converting linear RGB to SRGB:
D3DPWLGAMMA g_PWL_LineartoSRGB =
{
    {{0,87},{87,48},{135,35},{170,28},{198,25},{223,22},{245,20},{265,19},{284,17},{301,16},{317,15},{332,14},{346,14},{360,13},{373,12},{385,13},{398,11},{409,11},{420,11},{431,11},{442,10},{452,10},{462,10},{472,9},{481,9},{490,9},{499,9},{508,9},{517,8},{525,8},{533,8},{541,8},{549,8},{557,8},{565,7},{572,8},{580,7},{587,7},{594,7},{601,7},{608,7},{615,7},{622,7},{629,6},{635,7},{642,6},{648,7},{655,6},{661,6},{667,6},{673,6},{679,6},{685,6},{691,6},{697,6},{703,6},{709,5},{714,6},{720,5},{725,6},{731,5},{736,6},{742,5},{747,5},{752,6},{758,5},{763,5},{768,5},{773,5},{778,5},{783,5},{788,5},{793,5},{798,5},{803,5},{808,4},{812,5},{817,5},{822,4},{826,5},{831,5},{836,4},{840,5},{845,4},{849,5},{854,4},{858,5},{863,4},{867,4},{871,5},{876,4},{880,4},{884,5},{889,4},{893,4},{897,4},{901,4},{905,4},{909,4},{913,5},{918,4},{922,4},{926,4},{930,4},{934,4},{938,3},{941,4},{945,4},{949,4},{953,4},{957,4},{961,4},{965,3},{968,4},{972,4},{976,4},{980,3},{983,4},{987,4},{991,3},{994,4},{998,4},{1002,3},{1005,4},{1009,3},{1012,4},{1016,3},{1019,4}},
    {{0,87},{87,48},{135,35},{170,28},{198,25},{223,22},{245,20},{265,19},{284,17},{301,16},{317,15},{332,14},{346,14},{360,13},{373,12},{385,13},{398,11},{409,11},{420,11},{431,11},{442,10},{452,10},{462,10},{472,9},{481,9},{490,9},{499,9},{508,9},{517,8},{525,8},{533,8},{541,8},{549,8},{557,8},{565,7},{572,8},{580,7},{587,7},{594,7},{601,7},{608,7},{615,7},{622,7},{629,6},{635,7},{642,6},{648,7},{655,6},{661,6},{667,6},{673,6},{679,6},{685,6},{691,6},{697,6},{703,6},{709,5},{714,6},{720,5},{725,6},{731,5},{736,6},{742,5},{747,5},{752,6},{758,5},{763,5},{768,5},{773,5},{778,5},{783,5},{788,5},{793,5},{798,5},{803,5},{808,4},{812,5},{817,5},{822,4},{826,5},{831,5},{836,4},{840,5},{845,4},{849,5},{854,4},{858,5},{863,4},{867,4},{871,5},{876,4},{880,4},{884,5},{889,4},{893,4},{897,4},{901,4},{905,4},{909,4},{913,5},{918,4},{922,4},{926,4},{930,4},{934,4},{938,3},{941,4},{945,4},{949,4},{953,4},{957,4},{961,4},{965,3},{968,4},{972,4},{976,4},{980,3},{983,4},{987,4},{991,3},{994,4},{998,4},{1002,3},{1005,4},{1009,3},{1012,4},{1016,3},{1019,4}},
    {{0,87},{87,48},{135,35},{170,28},{198,25},{223,22},{245,20},{265,19},{284,17},{301,16},{317,15},{332,14},{346,14},{360,13},{373,12},{385,13},{398,11},{409,11},{420,11},{431,11},{442,10},{452,10},{462,10},{472,9},{481,9},{490,9},{499,9},{508,9},{517,8},{525,8},{533,8},{541,8},{549,8},{557,8},{565,7},{572,8},{580,7},{587,7},{594,7},{601,7},{608,7},{615,7},{622,7},{629,6},{635,7},{642,6},{648,7},{655,6},{661,6},{667,6},{673,6},{679,6},{685,6},{691,6},{697,6},{703,6},{709,5},{714,6},{720,5},{725,6},{731,5},{736,6},{742,5},{747,5},{752,6},{758,5},{763,5},{768,5},{773,5},{778,5},{783,5},{788,5},{793,5},{798,5},{803,5},{808,4},{812,5},{817,5},{822,4},{826,5},{831,5},{836,4},{840,5},{845,4},{849,5},{854,4},{858,5},{863,4},{867,4},{871,5},{876,4},{880,4},{884,5},{889,4},{893,4},{897,4},{901,4},{905,4},{909,4},{913,5},{918,4},{922,4},{926,4},{930,4},{934,4},{938,3},{941,4},{945,4},{949,4},{953,4},{957,4},{961,4},{965,3},{968,4},{972,4},{976,4},{980,3},{983,4},{987,4},{991,3},{994,4},{998,4},{1002,3},{1005,4},{1009,3},{1012,4},{1016,3},{1019,4}}
};

//--------------------------------------------------------------------------------------
// Name: TextureDesc
// Desc: Describes a single texture used to draw the "swatches" in the sample.
//--------------------------------------------------------------------------------------
struct TextureDesc
{
    const CHAR* strTiledFileName;
    D3DFORMAT Format;
    UINT TextureWidth;
    UINT TextureHeight;
    UINT TextureSlices;
    UINT TextureQuiltWidth;
    UINT TextureQuiltHeight;
};

//--------------------------------------------------------------------------------------
// Name: QuadDesc
// Desc: Describes one of the "swatches" in the sample, including its position and
//       the textures used to draw the object.
//--------------------------------------------------------------------------------------
struct QuadDesc
{
    XMFLOAT2 CenterPosXZ;
    TextureDesc Textures[MAX_TEXTURES_PER_OBJECT];
};

// The swatches drawn in the sample:
const QuadDesc g_QuadDescs[] =
{
    // 32bpp texture size variations
    { XMFLOAT2(  0,  0 ), { { NULL, D3DFMT_A8R8G8B8, 16384, 16384, 1 } } },
    { XMFLOAT2(  0,  1 ), { { NULL, D3DFMT_A8R8G8B8, 8192, 16384, 1 } } },

    // 32bpp array size variations and layering
    { XMFLOAT2(  1,  0 ), { { NULL, D3DFMT_A8R8G8B8, 16384, 16384, 64 } } },
    { XMFLOAT2(  1,  1 ), { { NULL, D3DFMT_A8R8G8B8, 16384, 16384, 0, 8, 8 } } },
    { XMFLOAT2(  1,  2 ), { { NULL, D3DFMT_A8R8G8B8, 16384, 16384, 1 }, { NULL, D3DFMT_A8R8G8B8, 16384, 16384, 1 } } },

    // 8bpp, 16bpp, 32bpp formats
    { XMFLOAT2(  2,  0 ), { { NULL, D3DFMT_L8,          16384, 16384, 1 } } },
    { XMFLOAT2(  2,  1 ), { { NULL, D3DFMT_R5G6B5,      16384, 16384, 1 } } },
    { XMFLOAT2(  2,  2 ), { { NULL, D3DFMT_A4R4G4B4,    16384, 16384, 1 } } },
    { XMFLOAT2(  2,  3 ), { { NULL, D3DFMT_A2B10G10R10, 16384, 16384, 1 } } },

    // Compressed formats 1
    { XMFLOAT2(  3,  0 ), { { NULL, D3DFMT_DXT1,        16384, 16384, 1 } } },
    { XMFLOAT2(  3,  1 ), { { NULL, D3DFMT_DXT3,        16384, 16384, 1 } } },
    { XMFLOAT2(  3,  2 ), { { NULL, D3DFMT_DXT5,        16384, 16384, 1 } } },
    { XMFLOAT2(  3,  3 ), { { NULL, D3DFMT_DXN,         16384, 16384, 1 } } },

    // Compressed formats 2 and 64bpp
    { XMFLOAT2(  4,  0 ), { { NULL, D3DFMT_DXT3A,           16384, 16384, 1 } } },
    { XMFLOAT2(  4,  1 ), { { NULL, D3DFMT_DXT5A,           16384, 16384, 1 } } },
    { XMFLOAT2(  4,  2 ), { { NULL, D3DFMT_CTX1,            16384, 16384, 1 } } },
    { XMFLOAT2(  4,  3 ), { { NULL, D3DFMT_A16B16G16R16,    16384, 16384, 1 } } },
};

// Spacing between the swatch quads:
const FLOAT g_QuadSpacing = 1.25f;

// Initial camera position:
const XMFLOAT2 g_CenterCameraPos( 2.0f, 1.5f );
const FLOAT g_CenterCameraHeight = 7.0f;

//--------------------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//--------------------------------------------------------------------------------------
ATG::HELP_CALLOUT g_HelpCallouts[] =
{
    { ATG::HELP_BACK_BUTTON,  ATG::HELP_PLACEMENT_2, L"Display\nhelp" },
    { ATG::HELP_START_BUTTON, ATG::HELP_PLACEMENT_2, L"Display tile\nstreaming view" },
    { ATG::HELP_Y_BUTTON,     ATG::HELP_PLACEMENT_2, L"Display\nresidency views" },
    { ATG::HELP_B_BUTTON,     ATG::HELP_PLACEMENT_2, L"Display sampling\nquality map" },
    { ATG::HELP_A_BUTTON,     ATG::HELP_PLACEMENT_1, L"Pause streaming" },
    { ATG::HELP_X_BUTTON,     ATG::HELP_PLACEMENT_1, L"Toggle Terrain Mode" },
    { ATG::HELP_DPAD,         ATG::HELP_PLACEMENT_2, L"Navigate streaming view" },
    { ATG::HELP_RIGHT_STICK,  ATG::HELP_PLACEMENT_2, L"Move camera" },
    { ATG::HELP_LEFT_STICK,   ATG::HELP_PLACEMENT_2, L"Move camera" },
    { ATG::HELP_LEFT_SHOULDER,ATG::HELP_PLACEMENT_2, L"Decrease\ncamera speed" },
    { ATG::HELP_RIGHT_SHOULDER,ATG::HELP_PLACEMENT_2, L"Increase\ncamera speed" },
};
static const DWORD NUM_HELP_CALLOUTS = ARRAYSIZE( g_HelpCallouts );

//--------------------------------------------------------------------------------------
// Name: class Sample
// Desc: The multiple device render sample class.
//--------------------------------------------------------------------------------------
class Sample : public ATG::Application
{
public:
    HRESULT                         Initialize();
    HRESULT                         Update();
    HRESULT                         Render();

private:
    VOID                            CreateSceneGeometry();
    XMMATRIX                        MoveFrame( ATG::GAMEPAD* pGamepad, FLOAT fDeltaTime, XMMATRIX matWorld, BOOL ForcePan, BOOL DisableRotation, FLOAT fSpeed );
    VOID                            RenderObject( D3DDevice* pd3dDevice, XMMATRIX matCameraVP, const SceneObject* pSceneObject );
    HRESULT                         SetGamma( const D3DPWLGAMMA& gammaRamp );
    XMMATRIX                        ComputeCameraWorldMatrix( const UINT QuadIndex );
    BOOL                            GenerateTextureLabel( WCHAR* strText, const UINT BufferSize );

private:
    // Sample framework objects
    ATG::Timer                      m_Timer;
    FLOAT                           m_fDeltaTime;
    ATG::Font                       m_Font;
    ATG::Help                       m_Help;
    BOOL                            m_bDrawHelp;

    // Texture inspection camera
    XMMATRIX                        m_matCameraWorld;
    XMMATRIX                        m_matProjection;
    UINT                            m_CameraIndex;

    // Terrain camera
    BOOL                            m_bTerrainCamera;
    XMMATRIX                        m_matTerrainCameraWorld;
    XMMATRIX                        m_matTerrainProjection;

    // Geometry
    SceneObjectVector               m_SceneObjects;

    // Scene rendering shaders
    D3DVertexDeclaration*           m_pDeclRender;
    D3DVertexShader*                m_pVSRender;
    D3DPixelShader*                 m_pPSRender;
    D3DPixelShader*                 m_pPSRender3D;
    D3DPixelShader*                 m_pPSRenderDoubleTex2D;
    D3DPixelShader*                 m_pPSRenderQuilt;

    // Surfaces and viewport
    D3DSurface*                     m_pRenderTarget;
    D3DSurface*                     m_pDepthStencil;
    D3DTexture*                     m_pFrontBuffer;
    D3DVIEWPORT9                    m_Viewport;

    // Tiled resources
    D3DTiledResourceDevice*         m_pTiledResourceDevice;
    D3DTilePool*                    m_pTilePool;

    // Title systems for tiled resources
    TitleResidencyManager*          m_pResidencyManager;
    MandelbrotTileLoader            m_MandelbrotTileLoader;
    MandelbrotTileLoader            m_JuliaTileLoader;
    ColorTileLoader                 m_ColorTileLoader;

    BOOL                            m_bPauseStreaming;
    BOOL                            m_bShowTiledDebug;
    D3DTiledTexture*                m_pDebugTexture;
    D3DBaseTexture*                 m_pDebugSamplingQualityTexture;
    FLOAT                           m_TiledDebugScroll;
    UINT                            m_TiledDebugTileSize;
    BOOL                            m_bShowResidencyViewDebug;
    BOOL                            m_bShowSamplingQualityDebug;

    // Terrain rendering
    TerrainView*                    m_pTerrainView;
};


//--------------------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the sample.
//--------------------------------------------------------------------------------------
VOID __cdecl main()
{
    Sample App;

    // Set up the structure used to create the D3DDevice.
    D3DPRESENT_PARAMETERS& d3dpp = App.m_d3dpp;
    ZeroMemory( &d3dpp, sizeof( D3DPRESENT_PARAMETERS ) );
    d3dpp.BackBufferWidth        = 1280;
    d3dpp.BackBufferHeight       = 720;
    d3dpp.BackBufferFormat       = D3DFMT_X2R10G10B10;
    d3dpp.FrontBufferFormat      = D3DFMT_LE_X2R10G10B10;
    d3dpp.FrontBufferColorSpace  = D3DCOLORSPACE_RGB;
    d3dpp.MultiSampleType        = D3DMULTISAMPLE_NONE;
    d3dpp.MultiSampleQuality     = 0;
    d3dpp.BackBufferCount        = 0;
    d3dpp.EnableAutoDepthStencil = FALSE;
    d3dpp.DisableAutoBackBuffer  = TRUE;
    d3dpp.DisableAutoFrontBuffer = TRUE;
    d3dpp.AutoDepthStencilFormat = D3DFMT_D24FS8;
    d3dpp.SwapEffect             = D3DSWAPEFFECT_DISCARD;
    d3dpp.PresentationInterval   = D3DPRESENT_INTERVAL_IMMEDIATE;

    App.Run();
}


//--------------------------------------------------------------------------------------
// Name: Initialize()
// Desc: Initializes the objects for the sample, and loads various resources.
//--------------------------------------------------------------------------------------
HRESULT Sample::Initialize()
{
    m_bPauseStreaming = FALSE;
    m_bShowTiledDebug = FALSE;
    m_TiledDebugScroll = 0;
    m_TiledDebugTileSize = 3;
    m_pDebugTexture = NULL;
    m_pDebugSamplingQualityTexture = NULL;
    m_bTerrainCamera = FALSE;
    m_bDrawHelp = FALSE;
    m_bShowResidencyViewDebug = FALSE;
    m_bShowSamplingQualityDebug = FALSE;

    // Set up gamma correction for this sample:
    SetGamma( g_PWL_LineartoSRGB );

    // The entire sample expects half pixel offset semantics:
    m_pd3dDevice->SetRenderState( D3DRS_HALFPIXELOFFSET, TRUE );

    // Create render target and depth/stencil surfaces.
    D3DSURFACE_PARAMETERS SurfParams = { 0 };
    RETURN_ON_FAIL( m_pd3dDevice->CreateRenderTarget( m_d3dpp.BackBufferWidth, m_d3dpp.BackBufferHeight, m_d3dpp.BackBufferFormat, D3DMULTISAMPLE_NONE, 0, FALSE, &m_pRenderTarget, &SurfParams ) );
    SurfParams.Base = GPU_EDRAM_TILES / 2;
    RETURN_ON_FAIL( m_pd3dDevice->CreateDepthStencilSurface( m_d3dpp.BackBufferWidth, m_d3dpp.BackBufferHeight, m_d3dpp.AutoDepthStencilFormat, D3DMULTISAMPLE_NONE, 0, FALSE, &m_pDepthStencil, &SurfParams ) );

    // Create front buffer texture.
    RETURN_ON_FAIL( m_pd3dDevice->CreateTexture( m_d3dpp.BackBufferWidth, m_d3dpp.BackBufferHeight, 1, 0, m_d3dpp.FrontBufferFormat, D3DPOOL_DEFAULT, &m_pFrontBuffer, NULL ) );

    // Create viewport.
    m_Viewport.Width = m_d3dpp.BackBufferWidth;
    m_Viewport.Height = m_d3dpp.BackBufferHeight;
    m_Viewport.X = 0;
    m_Viewport.Y = 0;
    m_Viewport.MinZ = 1.0f;
    m_Viewport.MaxZ = 0.0f;

    // Create tiled resource D3D device and tile pool.
    D3DTILED_EMULATION_PARAMETERS EmulationParams;
    EmulationParams.DefaultPhysicalTileFormat = D3DFMT_A8R8G8B8;

    // We're choosing 600 tiles for our maximum tile count.
    // This number is derived from the number of tiles that are needed to fully cover 
    // the screen resolution (1280 x 720) times 5 (1 coverage for the active mip level, 
    // plus 4 coverages for the next lower mip level).
    // With a 128 x 128 tile size, we get 282 tiles (921600 pixels divided by 16384 times 5).
    // We also assume 2x for multitexturing.
    // Considering the desire to have a few more tiles in memory than absolutely needed
    // at once, 600 might actually be a bit conservative.
    EmulationParams.MaxPhysicalTileCount = 600;
    RETURN_ON_FAIL( Direct3D_CreateTiledResourceDevice( m_pd3dDevice, &EmulationParams, &m_pTiledResourceDevice ) );
    RETURN_ON_FAIL( m_pTiledResourceDevice->CreateTilePool( &m_pTilePool ) );

    // Create tiled resource title systems.
    m_pResidencyManager = new TitleResidencyManager( m_pd3dDevice, 1, EmulationParams.MaxPhysicalTileCount, m_pTilePool );
    RETURN_ON_NULL( m_pResidencyManager );

    m_pTerrainView = new TerrainView( m_pd3dDevice, m_pTiledResourceDevice, m_pTilePool, m_pResidencyManager );
    RETURN_ON_NULL( m_pTerrainView );

    CreateSceneGeometry();

    // Initialize residency sample renderer.
    ResidencySampleRender::Initialize( m_pd3dDevice );

    // Initialize simple shaders.
    ATG::SimpleShaders::Initialize( NULL, NULL );

    // Load scene shaders.
    RETURN_ON_FAIL( ATG::LoadVertexShader( "game:\\media\\shaders\\VSTransform.xvu", &m_pVSRender ) );
    RETURN_ON_FAIL( ATG::LoadPixelShader( "game:\\media\\shaders\\PSSceneRender.xpu", &m_pPSRender ) );
    RETURN_ON_FAIL( ATG::LoadPixelShader( "game:\\media\\shaders\\PSSceneRender3D.xpu", &m_pPSRender3D ) );
    RETURN_ON_FAIL( ATG::LoadPixelShader( "game:\\media\\shaders\\PSSceneRenderDoubleTex2D.xpu", &m_pPSRenderDoubleTex2D ) );
    RETURN_ON_FAIL( ATG::LoadPixelShader( "game:\\media\\shaders\\PSSceneRenderQuilt.xpu", &m_pPSRenderQuilt ) );

    // Load font.
    RETURN_ON_FAIL( m_Font.Create( "game:\\media\\fonts\\Arial_16.xpr" ) );
    D3DRECT SafeRect = ATG::GetTitleSafeArea();
    m_Font.SetWindow( SafeRect );

    // Load help.
    RETURN_ON_FAIL( m_Help.Create( "game:\\media\\help\\Help.xpr" ) );

    // Initialize camera.
    m_CameraIndex = ARRAYSIZE(g_QuadDescs);
    m_matCameraWorld = ComputeCameraWorldMatrix( m_CameraIndex );
    m_matProjection = XMMatrixPerspectiveFovLH( XM_PIDIV4, (FLOAT)m_d3dpp.BackBufferWidth / (FLOAT)m_d3dpp.BackBufferHeight, 0.0001f, 100.0f );

    m_matTerrainCameraWorld = XMMatrixLookAtLH( XMVectorSet( 50, 70, 250, 0 ), XMVectorSet( 80, 0, 100, 0 ), XMVectorSet( 0, 1, 0, 0 ) );
    XMVECTOR vDeterminant;
    m_matTerrainCameraWorld = XMMatrixInverse( &vDeterminant, m_matTerrainCameraWorld );
    m_matTerrainProjection = XMMatrixPerspectiveFovLH( XM_PIDIV4, (FLOAT)m_d3dpp.BackBufferWidth / (FLOAT)m_d3dpp.BackBufferHeight, 0.5f, 3000.0f );

    return S_OK;    
}


//--------------------------------------------------------------------------------------
// Name: Sample::ComputeCameraWorldMatrix
// Desc: Creates an updated camera world matrix given the swatch index to focus upon.
//       If the camera index is higher than the number of swatches, use the free camera
//       positioning.
//--------------------------------------------------------------------------------------
XMMATRIX Sample::ComputeCameraWorldMatrix( const UINT CameraIndex )
{
    XMFLOAT2 TargetCenterPosXZ = g_CenterCameraPos;
    FLOAT CameraHeight = g_CenterCameraHeight;
    if( CameraIndex < ARRAYSIZE(g_QuadDescs) )
    {
        TargetCenterPosXZ = g_QuadDescs[CameraIndex].CenterPosXZ;
        CameraHeight = 1.5f;
    }

    XMVECTOR vCameraTarget = XMVectorSet( TargetCenterPosXZ.x * g_QuadSpacing, 0, TargetCenterPosXZ.y * g_QuadSpacing, 0 );
    XMVECTOR vCameraPos = vCameraTarget + XMVectorSet( 0, CameraHeight, 0, 0 );

    XMMATRIX matView = XMMatrixLookAtLH( vCameraPos, vCameraTarget, XMVectorSet( 0, 0, -1, 0 ) );
    XMVECTOR vDeterminant;
    return XMMatrixInverse( &vDeterminant, matView );
}


//--------------------------------------------------------------------------------------
// Name: Sample::SetGamma
// Desc: Uploads a PWL gamma curve to the video scaler.
//--------------------------------------------------------------------------------------
HRESULT Sample::SetGamma( const D3DPWLGAMMA& gammaRamp )
{
    HRESULT hr = S_OK;

    D3DPWLGAMMA gamma = { 0 };
    memcpy( &gamma, &gammaRamp, sizeof(gamma) );
    for( UINT i=0; i < 128; i++ )
    {
        gamma.red[i].Base <<= 6;
        gamma.red[i].Delta <<= 6;
        gamma.green[i].Base <<= 6;
        gamma.green[i].Delta <<= 6;
        gamma.blue[i].Base <<= 6;
        gamma.blue[i].Delta <<= 6;
    }
    m_pd3dDevice->SetPWLGamma( 0, &gamma );

    return hr;
}


//--------------------------------------------------------------------------------------
// Name: Sample::CreateSceneGeometry
// Desc: Creates the quad "swatches" that are drawn in the sample.
//--------------------------------------------------------------------------------------
VOID Sample::CreateSceneGeometry()
{
    struct SceneVertex
    {
        XMFLOAT3 Pos;
        XMFLOAT3 TexCoord;
    };

    // Create a vertex decl that all scene geometry uses:
    const D3DVERTEXELEMENT9 SceneVertexElements[] =
    {
        { 0,     0, D3DDECLTYPE_FLOAT3,     0,  D3DDECLUSAGE_POSITION,  0 },
        { 0,    12, D3DDECLTYPE_FLOAT3,     0,  D3DDECLUSAGE_TEXCOORD,  0 },
        D3DDECL_END()
    };

    D3DVertexDeclaration* pSceneVertexDecl = NULL;
    m_pd3dDevice->CreateVertexDeclaration( SceneVertexElements, &pSceneVertexDecl );

    // Create a single quad VB that has texture coordinates from 0 to 1:
    const SceneVertex QuadVerts[] = 
    {
        { XMFLOAT3(  0.5f, 0, -0.5f ), XMFLOAT3( 0, 0, 0 ) },
        { XMFLOAT3( -0.5f, 0, -0.5f ), XMFLOAT3( 1, 0, 0 ) },
        { XMFLOAT3(  0.5f, 0,  0.5f ), XMFLOAT3( 0, 1, 0 ) },
        { XMFLOAT3( -0.5f, 0,  0.5f ), XMFLOAT3( 1, 1, 0 ) }
    };

    D3DVertexBuffer* pQuadVB = NULL;
    UINT VBSizeBytes = ARRAYSIZE(QuadVerts) * sizeof(SceneVertex);
    m_pd3dDevice->CreateVertexBuffer( VBSizeBytes, 0, 0, D3DPOOL_DEFAULT, &pQuadVB, NULL );
    SceneVertex* pData = NULL;
    pQuadVB->Lock( 0, 0, (VOID**)&pData, 0 );
    XMemCpy( pData, QuadVerts, VBSizeBytes );
    pQuadVB->Unlock();

    // Create a quilted quad VB that has texture coordinates from 0 to 8:
    const SceneVertex QuiltedQuadVerts[] = 
    {
        { XMFLOAT3(  0.5f, 0, -0.5f ), XMFLOAT3( 0, 0, 0 ) },
        { XMFLOAT3( -0.5f, 0, -0.5f ), XMFLOAT3( 8, 0, 0 ) },
        { XMFLOAT3(  0.5f, 0,  0.5f ), XMFLOAT3( 0, 8, 0 ) },
        { XMFLOAT3( -0.5f, 0,  0.5f ), XMFLOAT3( 8, 8, 0 ) }
    };

    D3DVertexBuffer* pQuiltedQuadVB = NULL;
    VBSizeBytes = ARRAYSIZE(QuadVerts) * sizeof(SceneVertex);
    m_pd3dDevice->CreateVertexBuffer( VBSizeBytes, 0, 0, D3DPOOL_DEFAULT, &pQuiltedQuadVB, NULL );
    pQuiltedQuadVB->Lock( 0, 0, (VOID**)&pData, 0 );
    XMemCpy( pData, QuiltedQuadVerts, VBSizeBytes );
    pQuiltedQuadVB->Unlock();

    // Create an array texture VB that consists of 64 tightly packed quads that
    // each have texcoords from 0 to 1:
    D3DVertexBuffer* pArrayQuadVB = NULL;
    UINT VertexCount = 8 * 8 * 4;
    VBSizeBytes = VertexCount * sizeof(SceneVertex);
    m_pd3dDevice->CreateVertexBuffer( VBSizeBytes, 0, 0, D3DPOOL_DEFAULT, &pArrayQuadVB, NULL );
    pArrayQuadVB->Lock( 0, 0, (VOID**)&pData, 0 );

    FLOAT fScale = 1.0f / 8.0f;
    FLOAT fOffset = -0.5f;

    for( UINT y = 0; y < 8; ++y )
    {
        for( UINT x = 0; x < 8; ++x )
        {
            FLOAT ArrayIndex = ( (FLOAT)( y * 8 + x ) + 0.5f ) / 64.0f;

            pData[1].Pos = XMFLOAT3( (FLOAT)x * fScale + fOffset, 0, (FLOAT)y * fScale + fOffset );
            pData[1].TexCoord = XMFLOAT3( 0, 0, ArrayIndex );
            pData[0].Pos = XMFLOAT3( (FLOAT)( x + 1 ) * fScale + fOffset, 0, (FLOAT)y * fScale + fOffset );
            pData[0].TexCoord = XMFLOAT3( 1, 0, ArrayIndex );
            pData[2].Pos = XMFLOAT3( (FLOAT)x * fScale + fOffset, 0, (FLOAT)( y + 1 ) * fScale + fOffset );
            pData[2].TexCoord = XMFLOAT3( 0, 1, ArrayIndex );
            pData[3].Pos = XMFLOAT3( (FLOAT)( x + 1 ) * fScale + fOffset, 0, (FLOAT)( y + 1 ) * fScale + fOffset );
            pData[3].TexCoord = XMFLOAT3( 1, 1, ArrayIndex );

            pData += 4;
        }
    }

    pArrayQuadVB->Unlock();

    // Initialize the Julia set tile loader:
    m_JuliaTileLoader.m_Julia = TRUE;
    m_JuliaTileLoader.m_JuliaCoordinate = XMVectorSet( -0.726895347709114071439f, 0.188887129043845954792f, 0, 0 );

    // Loop over the swatch initialization data:
    for( UINT i = 0; i < ARRAYSIZE(g_QuadDescs); ++i )
    {
        const QuadDesc& QD = g_QuadDescs[i];

        SceneObject* pQuad = new SceneObject();

        pQuad->matWorld = XMMatrixTranslation( QD.CenterPosXZ.x * g_QuadSpacing, 0, QD.CenterPosXZ.y * g_QuadSpacing );
        pQuad->pVertexDeclaration = pSceneVertexDecl;
        pQuad->VertexStrideBytes = sizeof(SceneVertex);
        pQuad->pIndexBuffer = NULL;

        D3DTiledTexture* pTiledTextures[MAX_TEXTURES_PER_OBJECT] = { NULL };
        ITileLoader* pTileLoaders[MAX_TEXTURES_PER_OBJECT] = { NULL };

        UINT TextureCount = 0;

        for( UINT TextureIndex = 0; TextureIndex < ARRAYSIZE(QD.Textures); ++TextureIndex )
        {
            const TextureDesc& TD = QD.Textures[TextureIndex];

            // create a tiled texture
            pTileLoaders[TextureIndex] = &m_MandelbrotTileLoader;
            if( TextureIndex > 0 )
            {
                pTileLoaders[TextureIndex] = &m_JuliaTileLoader;
            }

            if( TD.TextureQuiltWidth > 0 && TD.TextureQuiltHeight > 0 )
            {
                D3DTiledArrayTexture* pTiledArrayTexture = NULL;
                m_pTiledResourceDevice->CreateQuiltedArrayTexture( m_pTilePool, TD.TextureWidth, TD.TextureHeight, TD.TextureQuiltWidth, TD.TextureQuiltHeight, 0, TD.Format, &pTiledArrayTexture );
                ASSERT( pTiledArrayTexture != NULL );
                pTiledTextures[TextureIndex] = pTiledArrayTexture;

                pQuad->pVertexBuffer = pQuiltedQuadVB;
                pQuad->PrimitiveType = D3DPT_TRIANGLESTRIP;
                pQuad->PrimitiveCount = 2;
                ++TextureCount;
            }
            else if( TD.TextureSlices > 1 )
            {
                D3DTiledArrayTexture* pTiledArrayTexture = NULL;
                m_pTiledResourceDevice->CreateArrayTexture( m_pTilePool, TD.TextureWidth, TD.TextureHeight, TD.TextureSlices, 0, TD.Format, &pTiledArrayTexture );
                ASSERT( pTiledArrayTexture != NULL );
                pTiledTextures[TextureIndex] = pTiledArrayTexture;

                pQuad->pVertexBuffer = pArrayQuadVB;
                pQuad->PrimitiveType = D3DPT_QUADLIST;
                pQuad->PrimitiveCount = 64;
                ++TextureCount;
            }
            else if( TD.TextureWidth > 0 && TD.TextureHeight > 0 )
            {
                m_pTiledResourceDevice->CreateTexture( m_pTilePool, TD.TextureWidth, TD.TextureHeight, 0, TD.Format, &pTiledTextures[TextureIndex] );
                ASSERT( pTiledTextures[TextureIndex] != NULL );

                pQuad->pVertexBuffer = pQuadVB;
                pQuad->PrimitiveType = D3DPT_TRIANGLESTRIP;
                pQuad->PrimitiveCount = 2;
                ++TextureCount;
            }
        }

        if( TextureCount > 0 )
        {
            for( UINT TextureIndex = 0; TextureIndex < TextureCount; ++TextureIndex )
            {
                // create a sampling quality manager for the tiled texture
                SamplingQualityManager* pSQM = new SamplingQualityManager( pTiledTextures[TextureIndex], m_pd3dDevice );
                m_pResidencyManager->RegisterTileActivityHandler( pSQM );

                pQuad->Textures[TextureIndex].pTexture = pTiledTextures[TextureIndex];
                pQuad->Textures[TextureIndex].pSamplingQualityManager = pSQM;
                pQuad->Textures[TextureIndex].pTileLoader = pTileLoaders[TextureIndex];
            }

            // register the tiled textures with the residency manager
            ResourceSetID RSID = m_pResidencyManager->CreateResourceSet( (const D3DTiledTexture**)pTiledTextures, pTileLoaders, TextureCount );
            pQuad->RSID = RSID;

            pQuad->TextureCount = TextureCount;

            m_SceneObjects.push_back( pQuad );
        }
        else
        {
            delete pQuad;
            ATG::FatalError( "Could not initialize a scene object." );
        }
    }
}


//--------------------------------------------------------------------------------------
// Name: Update()
// Desc: Gets controller input and updates the camera.
//--------------------------------------------------------------------------------------
HRESULT Sample::Update()
{
    // Update FPS in the timer class.
    m_Timer.MarkFrame();

    // Get frame delta time.
    m_fDeltaTime = (FLOAT)m_Timer.GetElapsedTime();


    // Get input from controllers (check for exit button sequence).
    ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput();

    // The Start button toggles the tiled streaming debug data:
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_START )
    {
        m_bShowTiledDebug = !m_bShowTiledDebug;
        if( !m_bShowTiledDebug )
        {
            m_pDebugTexture = NULL;
            m_pDebugSamplingQualityTexture = NULL;
        }
    }


    // The Y button toggles the residency debug view:
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_Y )
    {
        m_bShowResidencyViewDebug = !m_bShowResidencyViewDebug;
    }

    // Compute the scroll speed for the streaming debug view (one screenful per second):
    const FLOAT ScrollAmount = 720.0f * m_fDeltaTime;

    // The B button toggles the sampling quality debug view:
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_B )
    {
        m_bShowSamplingQualityDebug = !m_bShowSamplingQualityDebug;
    }
    // The dpad buttons manipulate the tiled streaming debug view:
    else if( m_bShowTiledDebug && pGamepad->wLastButtons & XINPUT_GAMEPAD_DPAD_UP )
    {
        m_TiledDebugScroll += ScrollAmount;
        m_TiledDebugScroll = min( 0.0f, m_TiledDebugScroll );
    }
    else if( m_bShowTiledDebug && pGamepad->wLastButtons & XINPUT_GAMEPAD_DPAD_DOWN )
    {
        m_TiledDebugScroll -= ScrollAmount;
    }
    else if( m_bShowTiledDebug && pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_RIGHT )
    {
        m_TiledDebugTileSize++;
    }
    else if( m_bShowTiledDebug && pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_LEFT )
    {
        if( m_TiledDebugTileSize > 1 )
        {
            --m_TiledDebugTileSize;
        }
    }

    // The Back button toggles help:
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
    {
        m_bDrawHelp = !m_bDrawHelp;
    }

    // The A button pauses and resumes streaming:
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_A )
    {
        m_bPauseStreaming = !m_bPauseStreaming;
    }

    // The X button toggles the terrain view:
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_X )
    {
        m_bTerrainCamera = !m_bTerrainCamera;
    }

    // Set the terrain diffuse texture as the debug texture:
    if( m_bTerrainCamera )
    {
        if( m_pDebugTexture != m_pTerrainView->GetDiffuseTexture() )
        {
            m_TiledDebugScroll = 0;
        }
        m_pDebugTexture = m_pTerrainView->GetDiffuseTexture();
        m_pDebugSamplingQualityTexture = m_pTerrainView->GetDiffuseSamplingQualityTexture();
    }

    if( m_bTerrainCamera )
    {
        // Update the terrain free camera:
        m_matTerrainCameraWorld = MoveFrame( pGamepad, m_fDeltaTime, m_matTerrainCameraWorld, FALSE, FALSE, 10.0f );
    }
    else
    {
        // Update the swatches camera, which always points in the -Y direction:

        // Compute a camera speed based on the logarithm of the Y altitude.
        // This ensures that the camera translates in the XZ direction slower 
        // as it approaches the XZ plane:
        XMVECTOR vCameraPos = m_matCameraWorld.r[3];
        FLOAT fAltitude = vCameraPos.y;
        FLOAT fLogAltitude = logf( fAltitude ) * 2.3f;
        FLOAT fSpeed = 1.0f / min( 10.0f, max( 1.0f, -fLogAltitude ) );

        // Move the camera:
        m_matCameraWorld = MoveFrame( pGamepad, m_fDeltaTime, m_matCameraWorld, TRUE, TRUE, fSpeed );

        // Clamp the camera's Y position to be slightly positive, so it never penetrates the XZ plane:
        m_matCameraWorld._42 = max( 0.005f, m_matCameraWorld._42 );
    }

    // Update residency manager.
    // Compute a streaming delta time, which can be controlled independently
    // of the render delta time:
    FLOAT StreamingDeltaTime = m_bPauseStreaming ? 0.0f : m_fDeltaTime;
    m_pResidencyManager->Update( StreamingDeltaTime );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Render()
// Desc: Renders the scene and UI.
//--------------------------------------------------------------------------------------
HRESULT Sample::Render()
{
    XMVECTOR vDeterminant;
    XMMATRIX matCameraView, matCameraVP;
    if( m_bTerrainCamera )
    {
        matCameraView = XMMatrixInverse( &vDeterminant, m_matTerrainCameraWorld );
        matCameraVP = matCameraView * m_matTerrainProjection;
    }
    else
    {
        matCameraView = XMMatrixInverse( &vDeterminant, m_matCameraWorld );
        matCameraVP = matCameraView * m_matProjection;
    }

    m_pd3dDevice->BeginScene();

    PIXBeginNamedEvent( 0, "Tiled Resources Pre-Scene Work" );

    m_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_NONE );
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_ZFUNC, D3DCMP_GREATEREQUAL );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHATESTENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_ALL );

    // render residency sample views
    if( !m_bPauseStreaming )
    {
        if( !m_bTerrainCamera )
        {
            ResidencySampleRender::Render( m_pd3dDevice, m_pResidencyManager, m_SceneObjects, matCameraView, m_matProjection );
        }
        else
        {
            m_pTerrainView->RenderResidencyView( matCameraView, m_matTerrainProjection );
        }
    }

    // execute tile mapping & tile border updates on the GPU
    m_pTiledResourceDevice->PreFrameRender();

    // update all sampling quality managers
    const UINT SceneObjectCount = m_SceneObjects.size();
    if( !m_bPauseStreaming )
    {
        for( UINT i = 0; i < SceneObjectCount; ++i )
        {
            SceneObject* pSO = m_SceneObjects[i];
            for( UINT j = 0; j < pSO->TextureCount; ++j )
            {
                SamplingQualityManager* pSQM = pSO->Textures[j].pSamplingQualityManager;
                pSQM->Render( m_pd3dDevice, m_pTiledResourceDevice, m_fDeltaTime );
            }
        }

        m_pTerrainView->PreSceneRender( m_fDeltaTime );
    }

    PIXEndNamedEvent();

    PIXBeginNamedEvent( 0, "Scene Render" );

    m_pd3dDevice->SetRenderTarget( 0, m_pRenderTarget );
    m_pd3dDevice->SetRenderTarget( 1, NULL );
    m_pd3dDevice->SetRenderTarget( 2, NULL );
    m_pd3dDevice->SetRenderTarget( 3, NULL );
    m_pd3dDevice->SetDepthStencilSurface( m_pDepthStencil );
    m_pd3dDevice->SetViewport( &m_Viewport );

    m_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_NONE );
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_ZFUNC, D3DCMP_GREATEREQUAL );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHATESTENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_ALL );

    // render scene
    if( m_bTerrainCamera )
    {
        m_pd3dDevice->Clear( 0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL, 0xFFA0A0FF, 0.0f, 0 );

        m_pTerrainView->RenderScene( matCameraView, m_matTerrainProjection );
    }
    else
    {
        m_pd3dDevice->Clear( 0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL, 0, 0.0f, 0 );

        for( UINT i = 0; i < SceneObjectCount; ++i )
        {
            RenderObject( m_pd3dDevice, matCameraVP, m_SceneObjects[i] );
        }
    }

    PIXEndNamedEvent();

    if( m_bDrawHelp )
    {
        m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
    }
    else
    {
        PIXBeginNamedEvent( 0, "UI Render" );

        WCHAR strText[300];

        m_Font.Begin();

        m_Font.SetScaleFactors( 0.9f, 0.9f );

        m_Font.DrawText( 0, 0, 0xFFFFFFFF, L"Tiled Resources", 0 );
        m_Font.DrawText( 0, 0, 0xFFFFFF00, m_Timer.GetFrameRate(), ATGFONT_RIGHT );

        if( !m_bTerrainCamera )
        {
            BOOL SelectedTexture = GenerateTextureLabel( strText, ARRAYSIZE(strText) );
            if( SelectedTexture )
            {
                m_Font.DrawText( 0, 20, 0xFF80FFFF, strText, 0 );
            }
        }

        const ResidencyStats& RStats = m_pResidencyManager->GetStats();
        swprintf_s( strText, L"Tiles Tracked: %d Loaded: %d QueuedForLoad: %d Unused: %d", RStats.NumTilesTracked, RStats.NumTilesLoaded, RStats.NumTilesQueuedForLoad, RStats.NumTilesUnused );
        m_Font.DrawText( 0, -20, 0xFFFFFFFF, strText, ATGFONT_RIGHT );

        D3DTILED_MEMORY_USAGE MemoryUsage;
        m_pTilePool->GetMemoryUsage( &MemoryUsage );

        UINT64 MappedTileMemoryBytes = MemoryUsage.TilesAllocated * 64 * 1024;
        DOUBLE ResidencyRatio = 100.0 * (DOUBLE)MappedTileMemoryBytes / (DOUBLE)MemoryUsage.ResourceVirtualBytesAllocated;

        swprintf_s( strText, L"Tile Capacity: %I64d across %d pools\nTiles Alloc'd: %I64d (%0.2f MB)\nResources: %d\nResource VM: %0.1f MB\nResidency: %0.1f%%\nTile Physical Mem: %0.2f MB\nResource Physical Mem: %0.2f KB\nResource Cached Mem: %0.2f KB\nOverhead: %0.2f KB",
            MemoryUsage.TileCapacity,
            MemoryUsage.FormatPoolsActive,
            MemoryUsage.TilesAllocated,
            ( (DOUBLE)MemoryUsage.TilesAllocated * 65536.0 ) / (DOUBLE)( 1024 * 1024 ),
            MemoryUsage.ResourceCount,
            (DOUBLE)MemoryUsage.ResourceVirtualBytesAllocated / (DOUBLE)( 1024 * 1024 ),
            ResidencyRatio,
            (DOUBLE)MemoryUsage.TileTextureMemoryBytesAllocated / (DOUBLE)( 1024 * 1024 ),
            (DOUBLE)MemoryUsage.ResourcePhysicalMemoryBytesAllocated / 1024.0,
            (DOUBLE)MemoryUsage.ResourceCachedMemoryBytesAllocated / 1024.0,
            (DOUBLE)MemoryUsage.OverheadMemoryBytesAllocated / 1024.0 );

        m_Font.SetScaleFactors( 0.7f, 0.7f );
        m_Font.DrawText( 0, -170, 0xFF808080, strText, ATGFONT_RIGHT );

        if( m_bTerrainCamera && !m_pTerrainView->TexturesLoaded() )
        {
            m_Font.SetScaleFactors( 1.0f, 1.0f );
            m_Font.DrawText( 0, 100, 0xFFFFFF00, L"Terrain textures not loaded.\nPlease run the TiledTexturePackTool and copy the following textures to game:\\\\media\\\ns_diffuse.sp\ns_normalmap.sp\ns_heightmap.sp" );
        }

        if( m_bPauseStreaming )
        {
            m_Font.SetScaleFactors( 1.2f, 1.2f );
            m_Font.DrawText( 200, 0, 0xFFFFFF00, L"Streaming Paused" );
        }

        m_Font.End();

        D3DRECT SafeWindow;
        m_Font.GetWindow( SafeWindow );

        if( m_bShowTiledDebug )
        {
            m_Font.SetWindow( 0, 0, m_d3dpp.BackBufferWidth, m_d3dpp.BackBufferHeight );

            m_pResidencyManager->DebugRenderTiles( m_pd3dDevice, &m_Font, m_pDebugTexture, m_TiledDebugTileSize, (INT)m_TiledDebugScroll );

            m_Font.SetWindow( SafeWindow );
        }

        if( m_bShowResidencyViewDebug )
        {
            m_pResidencyManager->DebugRenderResidencyView( SafeWindow.x2 - 256, SafeWindow.y2 - 480 );
        }

        if( m_bShowSamplingQualityDebug && m_pDebugSamplingQualityTexture != NULL )
        {
            XGTEXTURE_DESC TexDesc;
            XGGetTextureDesc( m_pDebugSamplingQualityTexture, 0, &TexDesc );

            D3DRECT ScreenRect = { SafeWindow.x2 - TexDesc.Width, SafeWindow.y1 + 100, SafeWindow.x2, SafeWindow.y1 + 100 + TexDesc.Height };
            ATG::DebugDraw::DrawScreenSpaceTexturedRect( ScreenRect, m_pDebugSamplingQualityTexture, FALSE );
        }

        PIXEndNamedEvent();
    }

    m_pd3dDevice->EndScene();

    // Sync to present interval, resolve, and swap.
    m_pd3dDevice->SynchronizeToPresentationInterval();
    m_pd3dDevice->Resolve( D3DRESOLVE_RENDERTARGET0, NULL, m_pFrontBuffer, NULL, 0, 0, NULL, 1.0, 0, NULL );
    m_pd3dDevice->Swap( m_pFrontBuffer, NULL );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Sample::RenderObject
// Desc: Renders a single swatch.
//--------------------------------------------------------------------------------------
VOID Sample::RenderObject( D3DDevice* pd3dDevice, XMMATRIX matCameraVP, const SceneObject* pSceneObject )
{
    // Compute world * view * projection matrix for this model and set into constants.
    XMMATRIX matWVP = pSceneObject->matWorld * matCameraVP;
    matWVP = XMMatrixTranspose( matWVP );

    pd3dDevice->SetVertexShaderConstantF( 0, (FLOAT*)&matWVP, 4 );

    pd3dDevice->SetVertexDeclaration( pSceneObject->pVertexDeclaration );
    pd3dDevice->SetVertexShader( m_pVSRender );
    pd3dDevice->SetStreamSource( 0, pSceneObject->pVertexBuffer, 0, pSceneObject->VertexStrideBytes );

    if( pSceneObject->TextureCount == 1 )
    {
        D3DTiledTexture* pTex = pSceneObject->Textures[0].pTexture;

        if( pTex->GetType() == D3DSRTYPE_ARRAYTEXTURE )
        {
            D3DTiledArrayTexture* pSATexture = (D3DTiledArrayTexture*)pTex;
            UINT QuiltWidth, QuiltHeight;
            if( pSATexture->GetQuiltSize( &QuiltWidth, &QuiltHeight ) )
            {
                pd3dDevice->SetPixelShader( m_pPSRenderQuilt );
            }
            else
            {
                pd3dDevice->SetPixelShader( m_pPSRender3D );
            }
        }
        else
        {
            pd3dDevice->SetPixelShader( m_pPSRender );
        }

        m_pTiledResourceDevice->SetTexture( 0, pTex );
        
        pd3dDevice->SetTexture( 0, pSceneObject->Textures[0].pSamplingQualityManager->GetLODQualityTexture() );
        pd3dDevice->SetPixelShaderConstantF( 0, pSceneObject->Textures[0].pSamplingQualityManager->GetUVScalingConstant(), 1 );
        pd3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_POINT );
        pd3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_POINT );
    }
    else if( pSceneObject->TextureCount == 2 )
    {
        pd3dDevice->SetPixelShader( m_pPSRenderDoubleTex2D );

        m_pTiledResourceDevice->SetTexture( 0, pSceneObject->Textures[0].pTexture );
        m_pTiledResourceDevice->SetTexture( 1, pSceneObject->Textures[1].pTexture );
        
        pd3dDevice->SetTexture( 0, pSceneObject->Textures[0].pSamplingQualityManager->GetLODQualityTexture() );
        pd3dDevice->SetPixelShaderConstantF( 0, pSceneObject->Textures[0].pSamplingQualityManager->GetUVScalingConstant(), 1 );
        pd3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_POINT );
        pd3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_POINT );

        pd3dDevice->SetTexture( 1, pSceneObject->Textures[1].pSamplingQualityManager->GetLODQualityTexture() );
        pd3dDevice->SetPixelShaderConstantF( 1, pSceneObject->Textures[1].pSamplingQualityManager->GetUVScalingConstant(), 1 );
        pd3dDevice->SetSamplerState( 1, D3DSAMP_MAGFILTER, D3DTEXF_POINT );
        pd3dDevice->SetSamplerState( 1, D3DSAMP_MINFILTER, D3DTEXF_POINT );
    }

    if( pSceneObject->pIndexBuffer != NULL )
    {
        pd3dDevice->SetIndices( pSceneObject->pIndexBuffer );
        pd3dDevice->DrawIndexedPrimitive( pSceneObject->PrimitiveType, 0, 0, 0, 0, pSceneObject->PrimitiveCount );
    }
    else
    {
        pd3dDevice->DrawPrimitive( pSceneObject->PrimitiveType, 0, pSceneObject->PrimitiveCount );
    }

    m_pTiledResourceDevice->SetTexture( 0, NULL );
}


//--------------------------------------------------------------------------------------
// Name: MoveFrame()
// Desc: Converts gamepad input into frame movement.  The input model is a free-cam
//       model, where the left stick controls movement, and the right stick controls
//       orientation.  The shoulder buttons modify the movement speed.
//--------------------------------------------------------------------------------------
XMMATRIX Sample::MoveFrame( ATG::GAMEPAD* pGamepad, FLOAT fDeltaTime, XMMATRIX matCameraWorld, BOOL ForcePan, BOOL DisableRotation, FLOAT fSpeed )
{
    assert( pGamepad != NULL );

    static const XMVECTOR vX = XMVectorSet( 1, 0, 0, 0 );
    static const XMVECTOR vY = XMVectorSet( 0, 1, 0, 0 );
    static const XMVECTOR vZ = XMVectorSet( 0, 0, 1, 0 );

    // Obtain the current vectors for the frame.
    XMVECTOR vForward = matCameraWorld.r[2];
    XMVECTOR vUp = matCameraWorld.r[1];
    XMVECTOR vRight = matCameraWorld.r[0];

    // Compute movement speed.
    if( pGamepad->wLastButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER )
        fSpeed *= 10.0f;
    else if( pGamepad->wLastButtons & XINPUT_GAMEPAD_LEFT_SHOULDER )
        fSpeed *= 0.1f;

    // Get the thumbstick settings.  Square them so small adjustments are easier.
    FLOAT fX1 = pGamepad->fX1 * fabs( pGamepad->fX1 );
    FLOAT fY1 = pGamepad->fY1 * fabs( pGamepad->fY1 );
    FLOAT fX2 = pGamepad->fX2 * fabs( pGamepad->fX2 );
    FLOAT fY2 = -pGamepad->fY2 * fabs( pGamepad->fY2 );

    BOOL bUpdateWorld = FALSE;

    // Adjust frame position based on the left thumbstick.
    const FLOAT fMovementAmount = fDeltaTime * fSpeed;
    XMVECTOR vPos = matCameraWorld.r[3];
    if( fY1 != 0 || fX1 != 0 )
    {
        bUpdateWorld = TRUE;
        if( ForcePan || pGamepad->wLastButtons & XINPUT_GAMEPAD_RIGHT_THUMB )
        {
            vPos += XMVectorScale( vUp, fMovementAmount * fY1 );
            vPos += XMVectorScale( vRight, fMovementAmount * fX1 );
        }
        else
        {
            vPos += XMVectorScale( vForward, fMovementAmount * fY1 );
            vPos += XMVectorScale( vRight, fMovementAmount * fX1 );
        }
    }

    // Adjust frame orientation based on right thumbstick.
    XMVECTOR RotationX = XMQuaternionIdentity();
    XMVECTOR RotationY = XMQuaternionIdentity();
    FLOAT fRotationSpeed = XM_PI * fDeltaTime;
    if( fX2 != 0 || fY2 != 0 )
    {
        bUpdateWorld = TRUE;
        if( DisableRotation )
        {
            vPos += XMVectorScale( vForward, fMovementAmount * -fY2 );
        }
        else
        {
            RotationX = XMQuaternionRotationAxis( vX, fRotationSpeed * fY2 );
            RotationY = XMQuaternionRotationAxis( vY, fRotationSpeed * fX2 );
        }
    }

    // Compose a new world matrix based on the rotation quaternion and position.
    if( bUpdateWorld )
    {
        XMMATRIX matWorld = matCameraWorld;
        XMMATRIX matRotX = XMMatrixRotationQuaternion( RotationX );
        XMMATRIX matRotY = XMMatrixRotationQuaternion( RotationY );
        matWorld = matRotX * matWorld * matRotY;
        matWorld.r[3] = XMVectorSelect( vPos, XMQuaternionIdentity(), XMVectorSelectControl( 0, 0, 0, 1 ) );
        matCameraWorld = matWorld;
    }

    return matCameraWorld;
}

//--------------------------------------------------------------------------------------
// Name: Sample::GenerateTextureLabel
// Desc: Given the camera's current location, determine which swatch is centered in the
//       camera's view, and generate a descriptive string for that swatch.
//--------------------------------------------------------------------------------------
BOOL Sample::GenerateTextureLabel( WCHAR* strText, const UINT BufferSize )
{
    XMVECTOR vCameraPosXYZ = m_matCameraWorld.r[3];
    XMFLOAT2 CameraPosXZ;
    XMStoreFloat2( &CameraPosXZ, XMVectorSwizzle( vCameraPosXYZ, 0, 2, 1, 3 ) );

    INT SelectedIndex = -1;
    for( INT i = 0; i < ARRAYSIZE(g_QuadDescs); ++i )
    {
        const QuadDesc& QD = g_QuadDescs[i];
        FLOAT XDistance = fabsf( CameraPosXZ.x - QD.CenterPosXZ.x * g_QuadSpacing );
        FLOAT YDistance = fabsf( CameraPosXZ.y - QD.CenterPosXZ.y * g_QuadSpacing );

        if( XDistance < 0.5f && YDistance < 0.5f )
        {
            SelectedIndex = i;
            break;
        }
    }

    if( SelectedIndex != -1 )
    {
        const QuadDesc& QD = g_QuadDescs[SelectedIndex];
        const WCHAR* strFormat = GetFormatName( QD.Textures[0].Format );
        UINT ArraySlices = QD.Textures[0].TextureSlices;
        UINT Width = QD.Textures[0].TextureWidth;
        UINT Height = QD.Textures[0].TextureHeight;
        UINT QuiltWidth = QD.Textures[0].TextureQuiltWidth;
        UINT QuiltHeight = QD.Textures[0].TextureQuiltHeight;
        const WCHAR* strSecondTexture = L"";
        if( QD.Textures[1].TextureWidth > 0 )
        {
            strSecondTexture = L", with second texture";
        }

        if( QuiltWidth > 1 || QuiltHeight > 1 )
        {
            swprintf_s( strText, BufferSize, L"%s format, %d x %d texels, %d x %d quilt (%d x %d effective texels)%s", strFormat, Width, Height, QuiltWidth, QuiltHeight, Width * QuiltWidth, Height * QuiltHeight, strSecondTexture );
        }
        else if( QD.Textures[0].strTiledFileName != NULL )
        {
            swprintf_s( strText, BufferSize, L"Loaded file \"%S\"", QD.Textures[0].strTiledFileName );
        }
        else
        {
            swprintf_s( strText, BufferSize, L"%s format, %d x %d texels, %d slices%s", strFormat, Width, Height, ArraySlices, strSecondTexture );
        }

        if( m_bShowTiledDebug || m_bShowSamplingQualityDebug )
        {
            D3DTiledTexture* pTex = m_SceneObjects[SelectedIndex]->Textures[0].pTexture;
            if( pTex != m_pDebugTexture )
            {
                m_TiledDebugScroll = 0;
            }
            m_pDebugTexture = pTex;
            m_pDebugSamplingQualityTexture = m_SceneObjects[SelectedIndex]->Textures[0].pSamplingQualityManager->GetLODQualityTexture();
        }

        return TRUE;
    }

    return FALSE;
}