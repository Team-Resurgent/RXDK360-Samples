//-----------------------------------------------------------------------------
// LeanDetection.cpp
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------

#include <xtl.h>
#include <AtgApp.h>
#include <AtgHelp.h>
#include <AtgUtil.h>
#include <AtgInput.h>
#include <AtgSceneAll.h>
#include <AtgNuiCommon.h>

#include <NuiApi.h>

#include "Detector.h"

//--------------------------------------------------------------------------------------
// Callouts for labeling the gamepad on the help screen
//--------------------------------------------------------------------------------------
ATG::HELP_CALLOUT g_HelpCallouts_SceneMode[] =
{
    { ATG::HELP_A_BUTTON,   ATG::HELP_PLACEMENT_1, L"Toggle display modes" },
    { ATG::HELP_Y_BUTTON,   ATG::HELP_PLACEMENT_1, L"Toggle stream display" },
    { ATG::HELP_B_BUTTON,   ATG::HELP_PLACEMENT_1, L"Reset camera" },
};

ATG::HELP_CALLOUT g_HelpCallouts_DepthMode[] =
{
    { ATG::HELP_A_BUTTON,   ATG::HELP_PLACEMENT_1, L"Toggle display modes" },
    { ATG::HELP_X_BUTTON,   ATG::HELP_PLACEMENT_1, L"Debug mode on" },
};

ATG::HELP_CALLOUT g_HelpCallouts_DepthDebugMode[] =
{
    { ATG::HELP_A_BUTTON,   ATG::HELP_PLACEMENT_1, L"Toggle display modes" },
    { ATG::HELP_X_BUTTON,   ATG::HELP_PLACEMENT_1, L"Debug mode off" },
    { ATG::HELP_RIGHT_SHOULDER, ATG::HELP_PLACEMENT_1, L"Next debug island" },
    { ATG::HELP_LEFT_SHOULDER,  ATG::HELP_PLACEMENT_1, L"Previous debug island" },
};

#define NUM_HELP_CALLOUTS_SCENE_MODE (sizeof(g_HelpCallouts_SceneMode) / sizeof(g_HelpCallouts_SceneMode[0]))
#define NUM_HELP_CALLOUTS_DEPTH_MODE (sizeof(g_HelpCallouts_DepthMode) / sizeof(g_HelpCallouts_DepthMode[0]))
#define NUM_HELP_CALLOUTS_DEPTH_DEBUG_MODE (sizeof(g_HelpCallouts_DepthDebugMode) / sizeof(g_HelpCallouts_DepthDebugMode[0]))

//--------------------------------------------------------------------------------------
// Rect used for displaying the colour and depth stream
//--------------------------------------------------------------------------------------
struct Rect
{
    FLOAT fX;
    FLOAT fY;
    FLOAT fWidth;
    FLOAT fHeight;
};

//--------------------------------------------------------------------------------------
// Sample mode
//--------------------------------------------------------------------------------------
enum DemoMode
{
    DEMOMODE_NORMAL,
    DEMOMODE_FULL_DEPTH,
    DEMOMODE_COUNT
};

//-------------------------------------------------------------------------------------
// Simple vertex for our debug rendering
//-------------------------------------------------------------------------------------
struct VideoFeedVertex
{
    FLOAT   vPosition[ 3 ];
    FLOAT   vTexCoords[ 2 ];
};

//--------------------------------------------------------------------------------------
// Name: class Sample
// Desc: Main class to run this application.
//--------------------------------------------------------------------------------------
class Sample : public ATG::Application
{
    ATG::Timer  m_timer;
    ATG::Font   m_font;
    ATG::Help   m_help;
    BOOL        m_bDrawHelp;

    HANDLE m_hFrameEndEvent;
    
    HANDLE m_hDepthStream;
    HANDLE m_hColourStream;

    IDirect3DVertexShader9*         m_pVideoVertexShader;
    IDirect3DPixelShader9*          m_pVideoPixelShaderRGB;
    IDirect3DPixelShader9*          m_pVideoPixelShaderClr;
    IDirect3DVertexDeclaration9*    m_pVideoVertexDecl;
    IDirect3DTexture9*              m_pDepthTexture[ 2 ];       // double buffer to avoid lock stalls
    IDirect3DTexture9*              m_pColourTexture[ 2 ];      // ditto

    DWORD         m_dwColourFrameIdx;
    DWORD         m_dwDepthFrameIdx;

    // Scene object
    ATG::PackedResource     m_xprResource;              // Packed resources (textures)
    ATG::Scene*             m_pScene;
    IDirect3DVertexShader9* m_pSceneVS;
    IDirect3DPixelShader9*  m_pScenePS;
    IDirect3DPixelShader9*  m_pSkyPS;
    
    // Transform matrices
    XMMATRIX    m_matView;
    XMMATRIX    m_matProj;

    // Camera
    XMVECTOR    m_vEyePt;
    XMVECTOR    m_vLookatPt;
    XMVECTOR    m_vUpVec;

    Rect m_colourWindow;
    Rect m_depthWindow;

    DemoMode m_demoMode;

    Detector::LeanResult  m_leanResult;
    Detector::LeanResult  m_prevLeanResult;

    FLOAT   m_fCurrentLean;
    FLOAT   m_fDeltaLean;

    BOOL    m_bShowStreams;
    BOOL    m_bDebugMode;
    INT     m_iDebugLayer;
    INT     m_iDebugIsland;

    Detector::DepthVector   m_depthFull;
    Detector::DepthVector   m_depth80x60;
    Detector::MaskVector    m_markedMask;

private:
    HRESULT Initialize();
    HRESULT InitializeVisualization( D3DDevice* pd3dDevice );

    HRESULT Update();
    HRESULT Render();
    HRESULT StartupCamera();
    
    void    DrawQuad( const Rect& rc );
    void    DrawRect( const Rect& rc );
    void    RenderStreams( const Rect& rcColour, const Rect& rcDepth );

    void    CalcRect( Rect& rc, const Rect& rcViewport, const Detector::Box& box );
    void    CalcRectFromPoint( Rect& rc, const Rect& rcViewport, XMVECTOR vScreen, FLOAT sz );

    void    SetupMatrices();

    void    CopyDownsampledDepth( const void* pSrcImage, const DWORD dwSrcPitch );

    void    DrawLayerDebugInfo( DWORD dwLayer, DWORD dwIsland );

    void    WriteDepthToTexture();
    void    ReadInput();
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
    atgApp.m_d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_TWO;
    atgApp.Run();
}

//--------------------------------------------------------------------------------------
// Setup the sensor array for RGB and depth streaming
//--------------------------------------------------------------------------------------
HRESULT Sample::StartupCamera()
{
    m_hFrameEndEvent = CreateEvent(NULL,
        FALSE,  // auto-reset
        FALSE,  // create unsignaled
        "NuiFrameEndEvent");
    if ( !m_hFrameEndEvent )
    {
        ATG_PrintError( "Failed to create NuiFrameEndEvent\n" );
        return E_FAIL;
    }
    
    // Initializes the Natural Input system on the default thread
    // the point here is to demonstrate that raw depth can be used without ST
    // which gives you low latency and reduces system reservation requirements
    // so we pass NUI_INITIALIZE_FLAG_NUI_GUIDE_DISABLED to turn ST off
    HRESULT hr = NuiInitialize( NUI_INITIALIZE_FLAG_USES_COLOR  |
                                NUI_INITIALIZE_FLAG_USES_DEPTH  |
                                NUI_INITIALIZE_FLAG_NUI_GUIDE_DISABLED,
                                NUI_INITIALIZE_DEFAULT_HARDWARE_THREAD );
    if( FAILED( hr ) )
    {
        ATG::NuiPrintError( hr, "NuiInitialize" );
        return E_FAIL;
    }

    hr = NuiSetFrameEndEvent( m_hFrameEndEvent, 0 );
    if ( FAILED (hr) )
    {
        ATG::NuiPrintError( hr, "NuiSetFrameEndEvent" );
        return E_FAIL;
    }

    hr = NuiImageStreamOpen( NUI_IMAGE_TYPE_COLOR,
                             NUI_IMAGE_RESOLUTION_640x480,
                             0,
                             1,
                             NULL,
                             &m_hColourStream );
    if( FAILED( hr ) )
    {
        ATG::NuiPrintError( hr, "NuiImageStreamOpen" );
        return E_FAIL;
    }

    hr = NuiImageStreamOpen( NUI_IMAGE_TYPE_DEPTH,
                             NUI_IMAGE_RESOLUTION_320x240,
                             0,
                             1,
                             NULL,
                             &m_hDepthStream );
    if( FAILED( hr ) )
    {
        ATG::NuiPrintError( hr, "NuiImageStreamOpen" );
        return E_FAIL;
    }

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Name: Initialize()
//--------------------------------------------------------------------------------------
HRESULT Sample::Initialize()
{
    m_fCurrentLean = 0;
    m_fDeltaLean = 0;
    m_pDepthTexture[ 0 ] = NULL;
    m_pDepthTexture[ 1 ] = NULL;
    m_pColourTexture[ 0 ] = NULL;
    m_pColourTexture[ 1 ] = NULL;
    m_demoMode = DEMOMODE_NORMAL;
    m_bDrawHelp = FALSE;
    m_bShowStreams = FALSE;
    m_bDebugMode = FALSE;
    m_iDebugLayer = m_iDebugIsland = 0;

    if( FAILED( StartupCamera() ) )
        return E_FAIL;

    if( FAILED( InitializeVisualization( m_pd3dDevice ) ) )
        return E_FAIL;

    if( FAILED( m_font.Create( "game:\\Media\\Fonts\\Arial_16.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    m_font.SetWindow( ATG::GetTitleSafeArea() );

    if( FAILED( m_help.Create( "game:\\Media\\Help\\Help.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    if( FAILED( m_xprResource.Create( "d:\\Media\\Resource.xpr" ) ) )
        ATG::FatalError( "Couldn't find media file.\fNear" );

    // Create scene object
    m_pScene = new ATG::Scene();
    m_pScene->GetResourceDatabase()->AddBundledResources( &m_xprResource );
    if( FAILED( ATG::SceneFileParser::LoadXATGFile( "d:\\media\\scenes\\headtrackScene.xatg", m_pScene, NULL,
                                                    ATG::XATGLOADER_DONOTINITIALIZEMATERIALS, NULL ) ) )
    {
        ATG::FatalError( "Could not load scene file." );
    }

    // games usually vsync at 60 if they can, but we only get data at 30Hz
    m_pd3dDevice->SetRenderState( D3DRS_PRESENTINTERVAL, D3DPRESENT_INTERVAL_TWO );

    m_depthFull.resize( Detector::NUI_DEPTH_W * Detector::NUI_DEPTH_H );
    m_depth80x60.resize( Detector::DEPTH_W * Detector::DEPTH_H );
    m_markedMask.resize( Detector::DEPTH_W * Detector::DEPTH_H );

    Detector::Initialize();

    return S_OK;
}



//--------------------------------------------------------------------------------------
// Vertex shader for rendering of streams
//--------------------------------------------------------------------------------------
static const char*  g_strVideoShaderHLSL =
    " struct VS_OUT                                                              "
    " {                                                                          "
    "     float4 Position : POSITION;                                            "
    "     float2 TexCoord : TEXCOORD0;                                           "
    " };                                                                         "
    "                                                                            "
    " sampler VideoTexture : register(s0);                                       "
    "                                                                            "
    " VS_OUT VideoVertexShader( const float3 Position : POSITION,                "
    "                           const float2 TexCoord : TEXCOORD0 )              "
    " {                                                                          "
    "     VS_OUT Output;                                                         "
    "     Output.Position.x  = ( Position.x-0.5);                                "
    "     Output.Position.y  = ( Position.y-0.5);                                "
    "     Output.Position.z  = ( 0.0 );                                          "
    "     Output.Position.w  = ( 1.0 );                                          "
    "     Output.TexCoord = TexCoord;                                            "
    "     return Output;                                                         "
    " }                                                                          "
    "                                                                            "
    " float4 VideoPixelShader( VS_OUT Input ) : COLOR                            "
    " {                                                                          "
    "     return tex2D( VideoTexture, Input.TexCoord );                          "
    " }                                                                          "
    "                                                                            "
    " float4 g_clr : register( c0 );                                             "
    " float4 VideoPixelShader0( VS_OUT Input ) : COLOR                           "
    " {                                                                          "
    "     return g_clr;                                                          "
    " }                                                                          ";


//--------------------------------------------------------------------------------------
// Vertex shader for the Environment
//--------------------------------------------------------------------------------------
const CHAR* g_strVertexShaderProgram =
    " row_major float4x4 matWVP : register(c0);    \n"
    "                                              \n"
    " struct VS_IN                                 \n"
    "                                              \n"
    " {                                            \n"
    "     float4 ObjPos   : POSITION;              \n"
    "     float2 Texture  : TEXCOORD0;             \n"
    "     float3 Normal   : NORMAL;                \n"
    " };                                           \n"
    "                                              \n"
    " struct VS_OUT                                \n"
    " {                                            \n"
    "     float4 ProjPos  : POSITION;              \n"
    "     float2 Texture  : TEXCOORD0;             \n"
    "     float3 Normal   : TEXCOORD1;             \n"
    "     float  Alpha   : TEXCOORD3;              \n"
    " };                                           \n"
    "                                              \n"
    " VS_OUT main( VS_IN In )                      \n"
    " {                                            \n"
    "     VS_OUT Out;                              \n"
    "     Out.ProjPos = mul( In.ObjPos, matWVP );  \n"
    "     Out.Normal = In.Normal;                  \n"
    "     Out.Texture = In.Texture;                \n"
    "     Out.Alpha = 1.0f;                        \n"
    "     if ( In.ObjPos.z < -1.3f ) Out.Alpha = 1.0f - ( ( -In.ObjPos.z - 1.3f )  / 1.3f );\n"
    "     return Out;                              \n"
    " }                                            \n";


//-------------------------------------------------------------------------------------
// Atrium Pixel shader
//-------------------------------------------------------------------------------------
const CHAR* g_strPixelShaderProgramScene =
    " struct PS_IN                                                          \n"
    " {                                                                     \n"
    "     float2 Texture : TEXCOORD0;                                       \n"
    "     float3 vNormal : TEXCOORD1;                                       \n"
    "     float  Alpha   : TEXCOORD3;                                       \n"
    " };                                                                    \n"
    "                                                                       \n"
    " sampler TextureSampler0 : register(s0);                               \n"
    "                                                                       \n"
    " float4 main( PS_IN In ) : COLOR                                       \n"
    " {                                                                     \n"
    "     float4 textureColor = tex2D( TextureSampler0, In.Texture );       \n"
    "     textureColor *= In.Alpha;                                         \n"
    "     float3 vLightDir1 = float3( -1.0f, 1.0f, -1.0f );                 \n"
    "     float3 vLightDir2 = float3( 1.0f, 1.0f, -1.0f );                  \n"
    "     float3 vLightDir3 = float3( 0.0f, -1.0f, 0.0f );                  \n"
    "     float3 vLightDir4 = float3( 1.0f, 1.0f, 1.0f );                   \n" 
    "     float fLighting = 0.1f +                                          \n"
    "                  saturate( dot( vLightDir1 , In.vNormal ) )*0.25f +   \n"
    "                  saturate( dot( vLightDir2 , In.vNormal ) )*0.25f +   \n"
    "                  saturate( dot( vLightDir3 , In.vNormal ) )*0.25f +   \n"
    "                  saturate( dot( vLightDir4 , In.vNormal ) )*0.25f ;   \n"
    " return textureColor * fLighting;                                      \n"
    " }                                                                     \n";



//--------------------------------------------------------------------------------------
// Name: InitializeVisualization()
//--------------------------------------------------------------------------------------
HRESULT Sample::InitializeVisualization( D3DDevice* pd3dDevice )
{
    // Compile vertex shader.
    ID3DXBuffer* pShaderCode;
    ID3DXBuffer* pErrorMsg;

    HRESULT hr = D3DXCompileShader( g_strVideoShaderHLSL,
                                    ( UINT )strlen( g_strVideoShaderHLSL ),
                                    NULL,
                                    NULL,
                                    "VideoVertexShader",
                                    "vs_2_0",
                                    0,
                                    &pShaderCode,
                                    &pErrorMsg,
                                    NULL );
    if( FAILED( hr ) )
    {
        if( pErrorMsg )
            ATG::DebugSpew( ( char* )pErrorMsg->GetBufferPointer() );
        return E_FAIL;
    }

    // Create vertex shader.
    pd3dDevice->CreateVertexShader( ( DWORD* )pShaderCode->GetBufferPointer(),
                                    &m_pVideoVertexShader );

    // Compile pixel shader.
    hr = D3DXCompileShader( g_strVideoShaderHLSL,
                            ( UINT )strlen( g_strVideoShaderHLSL ),
                            NULL,
                            NULL,
                            "VideoPixelShader",
                            "ps_2_0",
                            0,
                            &pShaderCode,
                            &pErrorMsg,
                            NULL );
    if( FAILED( hr ) )
    {
        if( pErrorMsg )
            ATG::DebugSpew( ( char* )pErrorMsg->GetBufferPointer() );
        return E_FAIL;
    }

    // Create pixel shader.
    pd3dDevice->CreatePixelShader( ( DWORD* )pShaderCode->GetBufferPointer(),
                                   &m_pVideoPixelShaderRGB );
    pShaderCode->Release();

    // 2nd
    hr = D3DXCompileShader( g_strVideoShaderHLSL,
                            ( UINT )strlen( g_strVideoShaderHLSL ),
                            NULL,
                            NULL,
                            "VideoPixelShader0",
                            "ps_2_0",
                            0,
                            &pShaderCode,
                            &pErrorMsg,
                            NULL );
    if( FAILED( hr ) )
    {
        if( pErrorMsg )
            ATG::DebugSpew( ( char* )pErrorMsg->GetBufferPointer() );
        return E_FAIL;
    }

    // Create pixel shader.
    pd3dDevice->CreatePixelShader( ( DWORD* )pShaderCode->GetBufferPointer(),
                                   &m_pVideoPixelShaderClr );
    pShaderCode->Release();

    // Create the vertex shader
    hr = D3DXCompileShader( g_strVertexShaderProgram, 
                            ( UINT )strlen( g_strVertexShaderProgram ),
                            NULL,
                            NULL,
                            "main",
                            "vs.3.0",
                            0, 
                            &pShaderCode,
                            &pErrorMsg,
                            NULL );
    if( FAILED( hr ) )
    {
        if( pErrorMsg )
            ATG::DebugSpew( ( char* )pErrorMsg->GetBufferPointer() );
        return E_FAIL;
    }

    m_pd3dDevice->CreateVertexShader(   ( DWORD* )pShaderCode->GetBufferPointer(),
                                        &m_pSceneVS );
    pShaderCode->Release();

    // Create the pixel shader
    hr = D3DXCompileShader( g_strPixelShaderProgramScene, 
                            ( UINT )strlen( g_strPixelShaderProgramScene ),
                            NULL,
                            NULL,
                            "main",
                            "ps.3.0",
                            0,
                            &pShaderCode,
                            &pErrorMsg,
                            NULL );        
    if( FAILED( hr ) )
    {
        if( pErrorMsg )
            ATG::DebugSpew( ( char* )pErrorMsg->GetBufferPointer() );
        return E_FAIL;
    }

    m_pd3dDevice->CreatePixelShader(    ( DWORD* )pShaderCode->GetBufferPointer(),
                                        &m_pScenePS );
    pShaderCode->Release();

    // Define the vertex elements and
    // Create a vertex declaration from the element descriptions.
    static const D3DVERTEXELEMENT9 VertexElements[] =
    {
        { 0,  0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
        { 0, 12, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
        D3DDECL_END()
    };
    pd3dDevice->CreateVertexDeclaration( VertexElements, &m_pVideoVertexDecl );

    // Create textures for displaying colour and depth streams
    for( UINT i=0; i < 2; ++i )
    {
        if( FAILED( pd3dDevice->CreateTexture( Detector::DEPTH_W, Detector::DEPTH_H, 1, 0, D3DFMT_LIN_X8R8G8B8, 0, &m_pDepthTexture[ i ],
                                               NULL ) ) )
            return E_FAIL;
        if( FAILED( pd3dDevice->CreateTexture( Detector::NUI_COLOR_W, Detector::NUI_COLOR_H, 1, 0, ATG::GetAs16SRGBFormat( D3DFMT_LIN_X8R8G8B8 ),
                                               0, &m_pColourTexture[ i ], NULL ) ) )
            return E_FAIL;
    }

    UINT uWidth;
    UINT uHeight;
    ATG::GetVideoSettings( &uWidth, &uHeight );

    XVIDEO_MODE VideoMode;
    ZeroMemory( &VideoMode, sizeof( VideoMode ) );
    XGetVideoMode( &VideoMode );

    // set up windows for display of raw input streams

    m_colourWindow.fWidth = uWidth / 2.f * 0.8f;
    m_colourWindow.fHeight = uWidth * 3.f / 4.f / 2.f * 0.8f;

    if( ( VideoMode.fIsWideScreen ) && ( uWidth == 640.f ) )
        m_colourWindow.fWidth /= 1.333f;

    m_colourWindow.fX = uWidth / 2.f - m_colourWindow.fWidth - 10.f;
    m_colourWindow.fY = ( uHeight - m_colourWindow.fHeight ) / 2.f - 30.f;

    m_depthWindow.fY = m_colourWindow.fY;
    m_depthWindow.fX = m_colourWindow.fX + m_colourWindow.fWidth;

    m_depthWindow.fWidth = uWidth / 2.f * 0.8f;
    m_depthWindow.fHeight = uWidth * 3.f / 4.f / 2.f * 0.8f;

    if( ( VideoMode.fIsWideScreen ) && ( uWidth == 640.f ) )
        m_depthWindow.fWidth /= 1.333f;

    return S_OK;
}



//--------------------------------------------------------------------------------------
// Name: SetupMatrices()
// Desc: we set an offset projection and view matrix here
//       note that using a usual projection matrix here looks a bit wrong as it doesn't
//       give the impression of looking through a window
//       it, however, might be suitable for your game, depends on what effect you're
//       trying to achieve
//--------------------------------------------------------------------------------------
void    Sample::SetupMatrices()
{
    // screen size in world space
    FLOAT   fAspect = ( FLOAT )m_d3dpp.BackBufferWidth / ( FLOAT )m_d3dpp.BackBufferHeight;
    FLOAT   fSY = 0.25f;             // size of the viewport in world space
    FLOAT   fSX = fSY * fAspect;
    FLOAT   fNear = 0.05;
    FLOAT   fFar = 100;

    // 3 corners of the screen in world space
    XMVECTOR    vViewportBottomLeft     = XMVectorSet( -fSX, -fSY,  0.f, 0 );
    XMVECTOR    vViewportBottomRight    = XMVectorSet(  fSX, -fSY,  0.f, 0 );
    XMVECTOR    vViewportTopLeft        = XMVectorSet( -fSX,  fSY,  0.f, 0 );
    XMVECTOR    vCameraOrg              = XMVectorSet( m_fCurrentLean, 0.f,  2, 0 );
    
    // Compute the screen extents and normal
    XMVECTOR    vViewportRight  = XMVector3Normalize( vViewportBottomRight - vViewportBottomLeft );
    XMVECTOR    vViewportUp     = XMVector3Normalize( vViewportTopLeft - vViewportBottomLeft );
    XMVECTOR    vViewportNormal = XMVector3Normalize( XMVector3Cross( vViewportUp, vViewportRight ) );

    // Compute the screen corner vectors.
    XMVECTOR    vToBottomLeftCorner     = vViewportBottomLeft   - vCameraOrg;
    XMVECTOR    vToBottomRightCorner    = vViewportBottomRight  - vCameraOrg;
    XMVECTOR    vToTopLeftCorner        = vViewportTopLeft      - vCameraOrg;

    // Find the distance from the eye to screen plane.
    XMVECTOR    vDiv = XMVectorSet( fNear, fNear, fNear, fNear ) / XMVector3Dot( vToBottomLeftCorner, vViewportNormal );

    // Find the extent of the perpendicular projection.
    XMVECTOR    vLeft   = XMVector3Dot( vViewportRight, vToBottomLeftCorner ) * vDiv;
    XMVECTOR    vRight  = XMVector3Dot( vViewportRight, vToBottomRightCorner ) * vDiv;
    XMVECTOR    vBottom = XMVector3Dot( vViewportUp, vToBottomLeftCorner ) * vDiv;
    XMVECTOR    vTop    = XMVector3Dot( vViewportUp, vToTopLeftCorner ) * vDiv;

    // Load the perpendicular projection.
    m_matProj = XMMatrixPerspectiveOffCenterLH( XMVectorGetX( vLeft ), XMVectorGetX( vRight ),
                                                XMVectorGetX( vBottom ), XMVectorGetX( vTop ),
                                                fNear, fFar );

    // Rotate the projection to be non-perpendicular.
    XMMATRIX    mRot = XMMatrixIdentity();

    mRot._11 = XMVectorGetX( vViewportRight );
    mRot._21 = XMVectorGetY( vViewportRight );
    mRot._31 = XMVectorGetZ( vViewportRight );

    mRot._12 = XMVectorGetX( vViewportUp );
    mRot._22 = XMVectorGetY( vViewportUp );
    mRot._32 = XMVectorGetZ( vViewportUp );

    mRot._13 = XMVectorGetX( vViewportNormal );
    mRot._23 = XMVectorGetY( vViewportNormal );
    mRot._33 = XMVectorGetZ( vViewportNormal );

    // Move the apex of the frustum to the origin.
    XMVECTOR    vNegCameraOrg = -vCameraOrg;
    XMMATRIX    mTrans = XMMatrixTranslation(   XMVectorGetX( vNegCameraOrg ),
                                                XMVectorGetY( vNegCameraOrg ),
                                                XMVectorGetZ( vNegCameraOrg ) );

    // final off centre projection matrix
    m_matView = mTrans * mRot;
}



//--------------------------------------------------------------------------------------
// Name: CopyDownsampledDepth()
// Desc: generates a downsampled depth buffer and copies the full res one
//--------------------------------------------------------------------------------------
void Sample::CopyDownsampledDepth( const void* pSrcImage, const DWORD dwSrcPitch )
{
    const DWORD dwSrcPitchShorts = dwSrcPitch / sizeof( USHORT );

    // 1x1 copy
    {
        const USHORT* __restrict pBitsSrcCur = ( const USHORT* )pSrcImage;
        WORD* __restrict pBitsDstCur = &m_depthFull[ 0 ];
        WORD* const __restrict pBitsDstEnd = pBitsDstCur + m_depthFull.size();

        for( UINT y = 0; y < Detector::NUI_DEPTH_H; ++y )
        {
            for( UINT r = 0; r < Detector::NUI_DEPTH_W / 64; ++r )
            {
                // 64 because the pixel is 2 bytes wide and the cache line is 128 bytes
                const UINT ofs = r * 64;

                __dcbt( 0x00, &pBitsSrcCur[ ofs + 64 ] );
                if( &pBitsDstCur[ ofs + 128 ] < pBitsDstEnd )
                    __dcbz128( 0x00, &pBitsDstCur[ ofs + 64 ] );

                for( UINT x = 0; x < 64; ++x )
                    pBitsDstCur[ ofs + x ] = pBitsSrcCur[ ofs + x ] >> 3;
            }

            // the texture could have an arbitrary pitch in memory
            pBitsSrcCur += dwSrcPitchShorts;
            pBitsDstCur += Detector::NUI_DEPTH_W;
        }
    }

    // 1/16 copy
    {
        const USHORT* __restrict pBitsSrcCur = ( const USHORT* )pSrcImage;
        WORD* __restrict pBitsDstCur = &m_depth80x60[ 0 ];

        // 0.24 ms

        for( UINT i=0; i < Detector::DEPTH_W * Detector::DEPTH_H; ++i )
            pBitsDstCur[ i ] = Detector::MAX_REAL_DEPTH;

        for( UINT y=0; y < Detector::NUI_DEPTH_H; ++y )
        {
            const USHORT* __restrict pRowSrc = pBitsSrcCur;
            USHORT* __restrict pDestDepth = &pBitsDstCur[ (y >> 2) * Detector::DEPTH_W ];

            // out of all samples find the nearest only if it's valid
            for( UINT cache=0; cache < Detector::DEPTH_W / 16; ++cache )
            {
                __dcbt( 128, pRowSrc );

                for( UINT x=0; x < 16; ++x )
                {
                    UINT  depth = *pDestDepth;

                    //UINT  d0 = ((USHORT)(pRowSrc[ 0 ]) >> 3);
                    //...

                    //depth = ( d0 > 0 && d0 < depth ) ? d0 : depth;
                    //...

                    // x1.5
                    UINT  d0 = ((USHORT)(pRowSrc[ 0 ] - 1) >> 3);   // 0 turns into 0xffff >> 3
                    UINT  d1 = ((USHORT)(pRowSrc[ 1 ] - 1) >> 3);
                    UINT  d2 = ((USHORT)(pRowSrc[ 2 ] - 1) >> 3);
                    UINT  d3 = ((USHORT)(pRowSrc[ 3 ] - 1) >> 3);

#define     SET_TO_MIN( d, a, b )   { INT delta = (INT)a - (INT)b; UINT mask = delta >> 31; delta &= mask; d = b + delta; }
                    SET_TO_MIN( d0, d0, d1 );
                    SET_TO_MIN( d2, d2, d3 );
                    SET_TO_MIN( depth, depth, d0 );
                    SET_TO_MIN( depth, depth, d2 );
#undef      SET_TO_MIN

                    *pDestDepth++ = static_cast< WORD >( depth );

                    pRowSrc += 4;
                }
            }

            pBitsSrcCur += dwSrcPitchShorts;
        }
    }
}


//--------------------------------------------------------------------------------------
// Name: WriteDepthToTexture
//--------------------------------------------------------------------------------------
void    Sample::WriteDepthToTexture()
{
    ++m_dwDepthFrameIdx;

    const auto* pBitsSrcCur = &m_depth80x60[ 0 ];
    const auto* pMaskSrcCur = &m_markedMask[ 0 ];

    D3DLOCKED_RECT Locked;
    m_pDepthTexture[ m_dwDepthFrameIdx & 1 ]->LockRect( 0, &Locked, NULL, 0 );
    DWORD* pBitsDst = ( DWORD* )Locked.pBits;

    for( UINT j = 0; j < Detector::DEPTH_H; ++j )
    {
        for( UINT i = 0; i < Detector::DEPTH_W; ++i )
        {
            const auto depth = *pBitsSrcCur++;
            const auto mask = *pMaskSrcCur++;

            pBitsDst[ i ] = D3DCOLOR_XRGB( depth >> 5, 64 * ( mask & 0xff ), 64 * ( mask >> 8 ) );
        }
        pBitsDst += Locked.Pitch / sizeof( DWORD );
    }

    m_pDepthTexture[ m_dwDepthFrameIdx & 1 ]->UnlockRect( 0 );
}



//--------------------------------------------------------------------------------------
// Name: ReadInput
//--------------------------------------------------------------------------------------
void    Sample::ReadInput()
{
    ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput();

    // Toggle help
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
        m_bDrawHelp = !m_bDrawHelp;

    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_A )
    {
        m_demoMode = ( DemoMode )( ( INT )m_demoMode + 1 );
        m_demoMode = ( DemoMode )( ( INT )m_demoMode % DEMOMODE_COUNT );
    }

    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_B && m_demoMode == DEMOMODE_NORMAL )
    {
        m_fCurrentLean = 0;
        m_fDeltaLean = 0;
    }

    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_Y && m_demoMode == DEMOMODE_NORMAL )
    {
        m_bShowStreams = !m_bShowStreams;
    }

    // debug display
    // you can choose the island to inspect
    {
        if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_X && m_demoMode == DEMOMODE_FULL_DEPTH  )
        {
            m_bDebugMode = !m_bDebugMode;
            m_iDebugLayer = 0;
            m_iDebugIsland = 0;
        }

        if( m_bDebugMode    &&
            !m_leanResult.m_layers.empty() )
        {
            const INT numLayers = m_leanResult.m_layers.size();

            if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_LEFT_SHOULDER )
                --m_iDebugIsland;

            if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER )
                ++m_iDebugIsland;

            if( m_iDebugIsland < 0 )
            {
                --m_iDebugLayer;
                if( m_iDebugLayer < 0 )
                    m_iDebugLayer = numLayers - 1;
                m_iDebugIsland = m_leanResult.m_layers[ m_iDebugLayer ].m_islands.size() - 1;
            }

            if( m_iDebugIsland >= static_cast< INT >( m_leanResult.m_layers[ m_iDebugLayer ].m_islands.size() ) )
            {
                m_iDebugIsland = 0;
                ++m_iDebugLayer;
                if( m_iDebugLayer >= numLayers )
                    m_iDebugLayer = 0;
            }
        }
    }    
}


//--------------------------------------------------------------------------------------
// Name: Update()
// Desc: Called once per frame to update the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Update()
{
    PIXBeginNamedEvent( 0, __FUNCTION__ );

    // Get the current gamepad status
    ReadInput();

    // do we need to process depth this frame?
    BOOL    bRunDetector = m_bDebugMode;

    // wait for frame end
    // if NUI stops this makes us synch to 30 fps instead of 60
    if( WAIT_OBJECT_0 == WaitForSingleObject( m_hFrameEndEvent, NUI_FRAME_END_TIMEOUT_DEFAULT ) )
    {
        const NUI_IMAGE_FRAME* pImageFrame;
        const NUI_IMAGE_FRAME* pDepthFrame;

        // get frame data
        HRESULT hrColour = NuiImageStreamGetNextFrame( m_hColourStream, 0, &pImageFrame );
        HRESULT hrDepth = NuiImageStreamGetNextFrame( m_hDepthStream, 0, &pDepthFrame );
        
        // note that we don't really need colour for this sample
        if( SUCCEEDED( hrColour ) )
        {
            ++m_dwColourFrameIdx;

            D3DLOCKED_RECT Locked;
            m_pColourTexture[ m_dwColourFrameIdx & 1 ]->LockRect( 0, &Locked, NULL, 0 );

            D3DLOCKED_RECT LockedSrc;
            pImageFrame->pFrameTexture->LockRect( 0, &LockedSrc, NULL, D3DLOCK_READONLY );

            PIXBeginNamedEvent( 0, "Copy colour" );
            XMemCpyStreaming( Locked.pBits, LockedSrc.pBits, NUI_IMAGE_COLOR_640x480_BUFFER_SIZE );
            PIXEndNamedEvent();

            pImageFrame->pFrameTexture->UnlockRect( 0 );
            m_pColourTexture[ m_dwColourFrameIdx & 1 ]->UnlockRect( 0 );

            NuiImageStreamReleaseFrame( m_hColourStream, pImageFrame );
        }

        // depth is all we really need
        if( SUCCEEDED( hrDepth ) )
        {
            // got a new depth frame -- run detector
            bRunDetector = TRUE;

            if( !m_bDebugMode )
            {
                D3DLOCKED_RECT LockedSrc;
                pDepthFrame->pFrameTexture->LockRect( 0, &LockedSrc, NULL, D3DLOCK_READONLY );

                PIXBeginNamedEvent( 0, "Copy depth" );
                CopyDownsampledDepth( LockedSrc.pBits, LockedSrc.Pitch );
                PIXEndNamedEvent();

                pDepthFrame->pFrameTexture->UnlockRect( 0 );

                // save the previous result
                m_prevLeanResult = m_leanResult;
            }

            NuiImageStreamReleaseFrame( m_hDepthStream, pDepthFrame );
        }
    }

    // run the detector on the depth to locate the head position
    if( bRunDetector )
    {
        PIXBeginNamedEvent( 0, "Detect lean" );
        DetectLean( m_leanResult, m_markedMask, m_depthFull, m_depth80x60, m_bDebugMode );
        PIXEndNamedEvent();

        // output depth + islands
        WriteDepthToTexture();

        // these constants depend on the scene and even on the distance to the player
        static const FLOAT  MOVE_WEIGHT = 2.f;
        static const FLOAT  LEAN_EXTENT = 0.5f;

        // do we know our position for certain?
        if( m_prevLeanResult.m_status != Detector::RS_INVALID   &&
            m_leanResult.m_status != Detector::RS_INVALID )
        {
            m_fDeltaLean = m_leanResult.m_vScreenPoint.x - m_prevLeanResult.m_vScreenPoint.x;
            m_fCurrentLean += m_fDeltaLean * MOVE_WEIGHT;
        } else
        {
            // some sort of dead reckoning i guess -- this helps to avoid jarring
            // abrupt stops when detector loses the head blob
            m_fCurrentLean += m_fDeltaLean * MOVE_WEIGHT;
            m_fDeltaLean *= 0.9f;
        }

        // limit the lean
        if( m_fCurrentLean > LEAN_EXTENT )
            m_fCurrentLean = LEAN_EXTENT;
        if( m_fCurrentLean < -LEAN_EXTENT )
            m_fCurrentLean = -LEAN_EXTENT;
    }

    // set view / projection matrices here
    SetupMatrices();

    PIXEndNamedEvent();

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: DrawQuad()
//--------------------------------------------------------------------------------------
void Sample::DrawQuad( const Rect& rc )
{
    VideoFeedVertex v[ 3 ];

    v[ 0 ].vPosition[ 0 ] = rc.fX;
    v[ 0 ].vPosition[ 1 ] = rc.fY;
    v[ 0 ].vTexCoords[ 0 ] = 0;
    v[ 0 ].vTexCoords[ 1 ] = 0;

    v[ 1 ].vPosition[ 0 ] = rc.fX + rc.fWidth;
    v[ 1 ].vPosition[ 1 ] = rc.fY;
    v[ 1 ].vTexCoords[ 0 ] = 1;
    v[ 1 ].vTexCoords[ 1 ] = 0;

    v[ 2 ].vPosition[ 0 ] = rc.fX + rc.fWidth;
    v[ 2 ].vPosition[ 1 ] = rc.fY + rc.fHeight;
    v[ 2 ].vTexCoords[ 0 ] = 1;
    v[ 2 ].vTexCoords[ 1 ] = 1;

    m_pd3dDevice->DrawPrimitiveUP( D3DPT_RECTLIST, 1, v, sizeof( v[ 0 ] ) );
}


//--------------------------------------------------------------------------------------
// Name: DrawQuad()
//--------------------------------------------------------------------------------------
void    Sample::DrawRect( const Rect& rc )
{
    VideoFeedVertex v[ 5 ];

    v[ 0 ].vPosition[ 0 ] = rc.fX;
    v[ 0 ].vPosition[ 1 ] = rc.fY;
    v[ 0 ].vTexCoords[ 0 ] = 0;
    v[ 0 ].vTexCoords[ 1 ] = 0;

    v[ 1 ].vPosition[ 0 ] = rc.fX + rc.fWidth;
    v[ 1 ].vPosition[ 1 ] = rc.fY;
    v[ 1 ].vTexCoords[ 0 ] = 1;
    v[ 1 ].vTexCoords[ 1 ] = 0;

    v[ 2 ].vPosition[ 0 ] = rc.fX + rc.fWidth;
    v[ 2 ].vPosition[ 1 ] = rc.fY + rc.fHeight;
    v[ 2 ].vTexCoords[ 0 ] = 1;
    v[ 2 ].vTexCoords[ 1 ] = 1;

    v[ 3 ].vPosition[ 0 ] = rc.fX;
    v[ 3 ].vPosition[ 1 ] = rc.fY + rc.fHeight;
    v[ 3 ].vTexCoords[ 0 ] = 0;
    v[ 3 ].vTexCoords[ 1 ] = 1;

    v[ 4 ] = v[ 0 ];

    m_pd3dDevice->DrawPrimitiveUP( D3DPT_LINESTRIP, 4, v, sizeof( v[ 0 ] ) );
}



//--------------------------------------------------------------------------------------
// Name: CalcRect
//--------------------------------------------------------------------------------------
void    Sample::CalcRect( Rect& rc, const Rect& rcViewport, const Detector::Box& box )
{
    const FLOAT fInvSx = 1.f / ( FLOAT )Detector::DEPTH_W;
    const FLOAT fInvSy = 1.f / ( FLOAT )Detector::DEPTH_H;

    rc.fX = rcViewport.fX + rcViewport.fWidth * ( ( FLOAT )box.left * fInvSx );
    rc.fY = rcViewport.fY + rcViewport.fHeight * ( ( FLOAT )box.top * fInvSy );
    rc.fWidth = rcViewport.fWidth * ( ( FLOAT )( box.right - box.left ) * fInvSx );
    rc.fHeight = rcViewport.fHeight * ( ( FLOAT )( box.bottom - box.top ) * fInvSy );
}

//--------------------------------------------------------------------------------------
// Name: CalcRectFromPoint
//--------------------------------------------------------------------------------------
void    Sample::CalcRectFromPoint( Rect& rc, const Rect& rcViewport, XMVECTOR vScreen, FLOAT sz )
{
    const FLOAT x = XMVectorGetX( vScreen );
    const FLOAT y = XMVectorGetY( vScreen );

    const FLOAT left = x - sz / ( FLOAT )Detector::DEPTH_W;
    const FLOAT top  = y - sz / ( FLOAT )Detector::DEPTH_H;

    rc.fX = rcViewport.fX + rcViewport.fWidth * left;
    rc.fY = rcViewport.fY + rcViewport.fHeight * top;
    rc.fWidth = rcViewport.fWidth * 2 * sz  / ( FLOAT )Detector::DEPTH_W;
    rc.fHeight = rcViewport.fHeight * 2 * sz / ( FLOAT )Detector::DEPTH_H;
}

//--------------------------------------------------------------------------------------
// Name: RenderStreams()
// Desc: Render colour and depth debug output
//--------------------------------------------------------------------------------------
void Sample::RenderStreams( const Rect& rcColour, const Rect& rcDepth )
{
    PIXBeginNamedEvent( 0, __FUNCTION__ );

    m_pd3dDevice->SetRenderState( D3DRS_VIEWPORTENABLE, FALSE );
    m_pd3dDevice->SetVertexDeclaration( m_pVideoVertexDecl );
    m_pd3dDevice->SetVertexShader( m_pVideoVertexShader );
    m_pd3dDevice->SetPixelShader( m_pVideoPixelShaderRGB );

    {
        // Render the colour video stream
        m_pd3dDevice->SetTexture( 0, m_pColourTexture[ m_dwColourFrameIdx & 1 ] );
        DrawQuad( rcColour );

        // Render the depth video stream
        m_pd3dDevice->SetTexture( 0, m_pDepthTexture[ m_dwDepthFrameIdx & 1 ] );
        DrawQuad( rcDepth );
    }

    // head being tracked
    {
        m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
        m_pd3dDevice->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
        m_pd3dDevice->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );

        m_pd3dDevice->SetVertexShader( m_pVideoVertexShader );
        m_pd3dDevice->SetPixelShader( m_pVideoPixelShaderClr );

        static const FLOAT clrBox[ 4 ] = { 1, 1, 0, 0.3f };
        static const FLOAT clrCentre[ 4 ] = { 0, 1, 0, 0.3f };

        Rect rcHead;
        CalcRect( rcHead, rcDepth, m_leanResult.m_headBox );
        m_pd3dDevice->SetPixelShaderConstantF( 0, clrBox, 1 );
        DrawQuad( rcHead );

        CalcRectFromPoint( rcHead, rcDepth, m_leanResult.m_vScreenPoint, 1 );
        m_pd3dDevice->SetPixelShaderConstantF( 0, clrCentre, 1 );
        DrawQuad( rcHead );

        m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
    }

    PIXEndNamedEvent();
}



//--------------------------------------------------------------------------------------
// Name: DrawLayerDebugInfo()
// Desc: Render debug info for the given layer/island
//--------------------------------------------------------------------------------------
void    Sample::DrawLayerDebugInfo( DWORD dwLayer, DWORD dwIsland )
{
    if( m_leanResult.m_layers.empty()  ||
        m_leanResult.m_layers[ m_iDebugLayer ].m_islands.empty() )
    {
        return;
    }

    using namespace Detector;
    using namespace Detector::Detail;

    const DepthSlice&   layer   = m_leanResult.m_layers[ dwLayer ];
    const Island&       island  = layer.m_islands[ dwIsland ];
    const DetectedHead& head    = m_leanResult.m_heads[ island.m_dwHeadIndex ];

    const WCHAR* statusText;
    switch( head.m_status )
    {
    default:                statusText = L"<unknown>";  break;
    case HS_DETECTED:       statusText = L"detected";   break;
    case HS_BOX_TOO_SMALL:  statusText = L"box small";  break;
    case HS_TOO_SMALL:      statusText = L"too small";  break;
    case HS_TOO_BIG:        statusText = L"too big";    break;
    case HS_LOW_SCORE:      statusText = L"low score";  break;
    case HS_WRONG_ASPECT:   statusText = L"aspect";     break;
    }

    WCHAR   temp[ 128 ];
    swprintf_s( temp, L"ID: %d-%d, Area: %d px", m_iDebugLayer, m_iDebugIsland, island.m_dwNumPixels );
    m_font.DrawText( 512, 500, ~0ul, temp, ATGFONT_LEFT );

    swprintf_s( temp, L"Status: %s", statusText );
    m_font.DrawText( 512, 520, (head.m_status != HS_DETECTED) ? D3DCOLOR_XRGB( 0xff, 0, 0 ) : D3DCOLOR_XRGB( 0, 0xff, 0 ), temp, ATGFONT_LEFT );
    
    swprintf_s( temp, L"Head size: %.2f", head.m_fHeadSizeAtDistance );
    m_font.DrawText( 512, 540, ~0ul, temp, ATGFONT_LEFT );
    
    swprintf_s( temp, L"Head size between: %.2f-%.2f", head.m_fHeadSizeAtDistanceMin, head.m_fHeadSizeAtDistanceMax );
    m_font.DrawText( 512, 560, ~0ul, temp, ATGFONT_LEFT );

    swprintf_s( temp, L"Layer distance: %d-%d", layer.m_nearDist, layer.m_farDist );
    m_font.DrawText( 512, 580, ~0ul, temp, ATGFONT_LEFT );

    swprintf_s( temp, L"Error: %f", head.m_fDifferenceMean );
    m_font.DrawText( 512, 600, ~0ul, temp, ATGFONT_LEFT );

    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
    m_pd3dDevice->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );

    m_pd3dDevice->SetVertexShader( m_pVideoVertexShader );
    m_pd3dDevice->SetPixelShader( m_pVideoPixelShaderClr );

    // outline island
    Rect rcIsland;
    CalcRect( rcIsland, m_depthWindow, island.m_Box );

    FLOAT clr[ 4 ] = { 1, 1, 1, 0.5f };
    m_pd3dDevice->SetPixelShaderConstantF( 0, clr, 1 );

    DrawRect( rcIsland );

    // min / max head size at this island
    Box tmpBox = island.m_Box;

    if( head.m_endShouldersScanMin > tmpBox.top )
    {
        tmpBox.bottom = head.m_endShouldersScanMin;
        CalcRect( rcIsland, m_depthWindow, tmpBox );
        DrawRect( rcIsland );
    }

    if( head.m_endShouldersScanMax > tmpBox.top )
    {
        tmpBox.bottom = head.m_endShouldersScanMax;
        CalcRect( rcIsland, m_depthWindow, tmpBox );
        DrawRect( rcIsland );
    }

    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
}

//--------------------------------------------------------------------------------------
// Name: Render()
// Desc: Called once per frame to render the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Render()
{
    PIXBeginNamedEvent( 0, __FUNCTION__ );

    // Draw a gradient filled background
    ATG::RenderBackground( 0xff0000ff, 0xff00ffff );

    // Set default states
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_HALFPIXELOFFSET, TRUE );

    // Show title, frame rate, and help
    m_timer.MarkFrame();
    if( m_bDrawHelp )
    {
        if( m_demoMode == DEMOMODE_NORMAL )
        {
            m_help.Render( &m_font, g_HelpCallouts_SceneMode, NUM_HELP_CALLOUTS_SCENE_MODE );
        }
        else if( m_demoMode == DEMOMODE_FULL_DEPTH )
        {
            if( m_bDebugMode == TRUE )
            {
                m_help.Render( &m_font, g_HelpCallouts_DepthDebugMode, NUM_HELP_CALLOUTS_DEPTH_DEBUG_MODE );
            }
            else
            {
                m_help.Render( &m_font, g_HelpCallouts_DepthMode, NUM_HELP_CALLOUTS_DEPTH_MODE );
            }
        }
    }
    else
    {
        // Render the streams
        if( m_demoMode != DEMOMODE_NORMAL )
        {
            RenderStreams( m_colourWindow, m_depthWindow );
        }
        else
        {
            // render a simple "game" using lean and depth
            m_pd3dDevice->Clear( 0, NULL, D3DCLEAR_TARGET | D3DCLEAR_STENCIL | D3DCLEAR_ZBUFFER, 0, 1, 0xff );

            // looks better with filtering
            m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
            m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR );
            m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
            
            // the model is a bit like this
            m_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_NONE );

            // Set the scene shaders
            m_pd3dDevice->SetVertexShader( m_pSceneVS );
            m_pd3dDevice->SetPixelShader( m_pScenePS );

            XMMATRIX matTransViewProj = m_matView * m_matProj;
            m_pd3dDevice->SetVertexShaderConstantF( 0, ( FLOAT* )&matTransViewProj, 4 );

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

            // put the streams in the corner somewhere
            if( m_bShowStreams )
            {
                Rect rcColour;
                rcColour.fX = 64;
                rcColour.fY = 128;
                rcColour.fWidth = 240;
                rcColour.fHeight = ( rcColour.fWidth / m_colourWindow.fWidth ) * m_colourWindow.fHeight;

                Rect rcDepth;
                rcDepth.fX = 64 + rcColour.fWidth;
                rcDepth.fY = rcColour.fY;
                rcDepth.fWidth = rcColour.fWidth;
                rcDepth.fHeight = ( rcDepth.fWidth / m_depthWindow.fWidth ) * m_depthWindow.fHeight;

                RenderStreams( rcColour, rcDepth );
            }
        }

        m_font.Begin();

        m_font.SetScaleFactors( 1.2f, 1.2f );
        switch( m_demoMode )
        {
            case DEMOMODE_FULL_DEPTH:
                m_font.DrawText( 0, 0, 0xffffffff, L"LeanDetection - showing depth" );
                break;

            case DEMOMODE_NORMAL:
                m_font.DrawText( 0, 0, 0xffffffff, L"LeanDetection - showing a scene" );
                break;
        }

        m_font.SetScaleFactors( 1.0f, 1.0f );
        m_font.DrawText( 180, 500, ~0ul, L"Toggle modes: ", ATGFONT_RIGHT );
        m_font.DrawText( 200, 500, ~0ul, GLYPH_A_BUTTON );

        if( DEMOMODE_NORMAL == m_demoMode )
        {
            m_font.DrawText( 180, 530, ~0ul, L"Centre: ", ATGFONT_RIGHT );
            m_font.DrawText( 200, 530, ~0ul, GLYPH_B_BUTTON );
            m_font.DrawText( 180, 560, ~0ul, L"Streams: ", ATGFONT_RIGHT );
            m_font.DrawText( 200, 560, ~0ul, GLYPH_Y_BUTTON );
        }

        if( m_demoMode == DEMOMODE_FULL_DEPTH )
        {
            m_font.DrawText( 180, 590, m_bDebugMode ? 0xff00ff00 : ~0ul, L"Debug: ", ATGFONT_RIGHT );
            m_font.DrawText( 200, 590, ~0ul, GLYPH_X_BUTTON );
        }

        if( m_bDebugMode )
            m_font.DrawText( 180, 470, ~0ul, L"Use shoulder buttons to switch islands", ATGFONT_LEFT );

        m_font.DrawText( 0, 0, ~0ul, m_timer.GetFrameRate(), ATGFONT_RIGHT );

        // draw extra debug information
        if( DEMOMODE_FULL_DEPTH == m_demoMode )
        {
            m_font.SetScaleFactors( 0.75f, 0.75f );

            if( m_bDebugMode )
            {
                DrawLayerDebugInfo( m_iDebugLayer, m_iDebugIsland );
            } else
            {
                // draw information about the first detected head
                for( UINT i=0; i < m_leanResult.m_layers.size(); ++i )
                {
                    for( UINT j=0; j < m_leanResult.m_layers[ i ].m_islands.size(); ++j )
                    {
                        if( m_leanResult.m_layers[ i ].m_islands[ j ].m_dwHeadIndex == m_leanResult.m_dwMatchedHead )
                        {
                            DrawLayerDebugInfo( i, j );
                            i = m_leanResult.m_layers.size();
                            break;
                        }
                    }
                }
            }
        }

        m_font.End();
    }

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    PIXEndNamedEvent();

    return S_OK;
}
