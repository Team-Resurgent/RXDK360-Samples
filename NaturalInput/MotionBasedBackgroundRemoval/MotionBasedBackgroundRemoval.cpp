//--------------------------------------------------------------------------------------
// MotionBasedBackgroundRemoval.cpp
//
// Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include <xtl.h>
#include <xgraphics.h>
#include <AtgApp.h>
#include <AtgHelp.h>
#include <AtgUtil.h>
#include <AtgInput.h>
#include <AtgNuiVisualization.h>
#include <AtgPostProcess.h>
#include <AtgNuiCommon.h>

#include <NuiApi.h>

//--------------------------------------------------------------------------------------
// Callouts for labeling the gamepad on the help screen
//--------------------------------------------------------------------------------------
ATG::HELP_CALLOUT g_HelpCallouts[] =
{
    { ATG::HELP_A_BUTTON, ATG::HELP_PLACEMENT_1, L"Toggle demo modes" },
    { ATG::HELP_B_BUTTON, ATG::HELP_PLACEMENT_1, L"Toggle sub-demo modes" },
    { ATG::HELP_Y_BUTTON, ATG::HELP_PLACEMENT_1, L"Clear background" },
    { ATG::HELP_X_BUTTON, ATG::HELP_PLACEMENT_1, L"Pause/unpause updating background" },
};
#define NUM_HELP_CALLOUTS (sizeof(g_HelpCallouts) / sizeof(g_HelpCallouts[0]))

// Rect used for displaying the color or depth stream
typedef struct
{
    FLOAT fX;
    FLOAT fY;
    FLOAT fWidth;
    FLOAT fHeight;
} Rect;

enum DemoMode
{
    DEMOMODE_DEPTH,
    DEMOMODE_COLOR,    
    DEMOMODE_COUNT
};

enum DepthSubMode
{
    DEPTHSUBMODE_PLAYERREMOVAL,
    DEPTHSUBMODE_BACKGROUNDREMOVAL,
    DEPTHSUBMODE_COUNT,
};

enum ColorSubMode
{
    COLORSUBMODE_PLAYERREMOVAL,
    COLORSUBMODE_BACKGROUNDREMOVAL,
    COLORSUBMODE_TRANSPARENTBLEND,
    COLORSUBMODE_HEATDISTORTION,
    COLORSUBMODE_COUNT,
};

//--------------------------------------------------------------------------------------
// Name: class Sample
// Desc: Main class to run this application.
//--------------------------------------------------------------------------------------
class Sample : public ATG::Application
{
    ATG::Timer                   m_Timer;
    ATG::Font                    m_Font;
    ATG::Help                    m_Help;
    BOOL                         m_bDrawHelp;

    ATG::NuiVisualization        m_pip;
    HANDLE                       m_hDepthStream;
    HANDLE                       m_hColorStream;
    HANDLE                       m_hFrameEndEvent;

    IDirect3DVertexBuffer9*      m_pDisplayVB;
    IDirect3DVertexShader9*      m_pVideoVertexShader;
    IDirect3DPixelShader9*       m_pVideoPixelShaderRGB;
    IDirect3DPixelShader9*       m_pVideoPixelShaderF16;
    IDirect3DVertexDeclaration9* m_pVideoVertexDecl;

    // Textures for tracking and storing the background depth    
    IDirect3DTexture9*           m_pDepthBackgroundTexture;

    // Texture for tracking and storing the player depth
    IDirect3DTexture9*           m_pDepthPlayerTexture;

    // Texture for storing the player mask used in the heat distortion effect
    IDirect3DTexture9*           m_pDepthPlayerMaskTexture;

    // Textures for storing background color    
    IDirect3DTexture9*           m_pColorBackgroundTexture;

    // Texture for storing the player color
    IDirect3DTexture9*           m_pColorPlayerTexture;

    // Texture for storing background/player color blended result
    IDirect3DTexture9*           m_pColorBlendedTexture;

    // Noise texture
    IDirect3DTexture9*           m_pPerlinNoiseTexture;

    LPDIRECT3DPIXELSHADER9       m_pDepthPlayerRemovalPS;           
    LPDIRECT3DPIXELSHADER9       m_pUpdateDepthPlayerPS;
    LPDIRECT3DPIXELSHADER9       m_pUpdateColorBackgroundPS;
    LPDIRECT3DPIXELSHADER9       m_pUpdateColorPlayerPS;
    LPDIRECT3DPIXELSHADER9       m_pBlendColorPlayerBackgroundPS;
    LPDIRECT3DPIXELSHADER9       m_pPlayerMaskPS;    
    LPDIRECT3DPIXELSHADER9       m_pHeatDistortionPS;

    ATG::PostProcess             m_PostProcess;       

    ATG::PackedResource          m_Resource; 

    Rect                         m_DisplayWindow;

    DemoMode                     m_DemoMode;
    DepthSubMode                 m_DepthSubMode;
    ColorSubMode                 m_ColorSubMode;    

    INT                          m_ColorWidth;
    INT                          m_ColorHeight;
    INT                          m_DepthWidth;
    INT                          m_DepthHeight;    

    BOOL                         m_bPaused;

private:
    HRESULT Initialize();
    HRESULT InitializeVisualization( D3DDevice* pd3dDevice );

    HRESULT Update();
    HRESULT Render();

    HRESULT StartupCamera();    
    
    VOID RenderToTexture( LPDIRECT3DTEXTURE9 pDstTexture, LPDIRECT3DPIXELSHADER9 pPixelShader, DWORD dwEdramOffset = 0 );
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

    // Initializes the Natural Input system on the default thread without skeleton tracking
    HRESULT hr = NuiInitialize( NUI_INITIALIZE_FLAG_USES_COLOR |
                                NUI_INITIALIZE_FLAG_USES_DEPTH |
                                NUI_INITIALIZE_FLAG_NUI_GUIDE_DISABLED,
                                NUI_INITIALIZE_DEFAULT_HARDWARE_THREAD );
    if ( FAILED(hr) )
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
                             &m_hColorStream );
    if ( FAILED( hr ) )
    {
        ATG::NuiPrintError( hr, "NuiImageStreamOpen" );
        return E_FAIL;
    }

    hr = NuiImageStreamOpen( NUI_IMAGE_TYPE_DEPTH_IN_COLOR_SPACE,
                             NUI_IMAGE_RESOLUTION_320x240,
                             0,
                             1,     // This indicates how many times we can call NuiImageStreamGetNextFrame without releasing the frame
                             NULL,
                             &m_hDepthStream );
    if ( FAILED( hr ) )
    {
        ATG::NuiPrintError( hr, "NuiImageStreamOpen" );
        return E_FAIL;
    }

    hr = m_pip.Initialize( m_pd3dDevice, 
                           NUI_INITIALIZE_FLAG_USES_COLOR | NUI_INITIALIZE_FLAG_USES_DEPTH,
                           NUI_IMAGE_RESOLUTION_640x480 );
    if FAILED( hr )
    {
        ATG_PrintError ( "Picture in Picture initialization failed" );
	}
    
    return S_OK;
}

//--------------------------------------------------------------------------------------
// Name: Initialize()
//--------------------------------------------------------------------------------------
HRESULT Sample::Initialize()
{
    m_pDepthBackgroundTexture = NULL;
    m_pDepthPlayerTexture = NULL;
    m_pColorBackgroundTexture = NULL;
    m_pColorPlayerTexture = NULL;
    m_pColorBlendedTexture = NULL;
    m_DemoMode = DEMOMODE_COLOR;
    m_DepthSubMode = DEPTHSUBMODE_PLAYERREMOVAL;
    m_ColorSubMode = COLORSUBMODE_TRANSPARENTBLEND;
    m_bDrawHelp = FALSE;
    m_ColorWidth = 640;
    m_ColorHeight = 480;
    m_DepthWidth = 320;
    m_DepthHeight = 240;
    m_bPaused = FALSE;   
    
    if ( FAILED( StartupCamera() ) )
        return E_FAIL;

    if ( FAILED( InitializeVisualization( m_pd3dDevice ) ) )
        return E_FAIL;

    // Create the font
    if( FAILED( m_Font.Create( "game:\\Media\\Fonts\\Arial_16.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Confine text drawing to the title safe area
    m_Font.SetWindow( ATG::GetTitleSafeArea() );

    // Create the help
    if( FAILED( m_Help.Create( "game:\\Media\\Help\\Help.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;    

    if( FAILED( m_PostProcess.Initialize() ) )
    {
        ATG_PrintError( "DX9Sample: Couldn't initialize the effects library\n" );
        return E_FAIL;
    } 

    if( FAILED( m_Resource.Create( "game:\\Media\\Resource.xpr" ) ) )
    {
        ATG_PrintError( "Couldn't create Resource.xpr\n" );
        return E_FAIL;
    }
    m_pPerlinNoiseTexture = m_Resource.GetTexture("perlin_noise");    

    // Create the pixel shaders
    if( FAILED( ATG::LoadPixelShader( "game:\\Media\\Shaders\\DepthPlayerRemoval.xpu",
        &m_pDepthPlayerRemovalPS ) ) )
    {
        ATG_PrintError( "Couldn't create DepthPlayerRemoval.xpu\n" );
        return ATGAPPERR_MEDIANOTFOUND;
    }

    if( FAILED( ATG::LoadPixelShader( "game:\\Media\\Shaders\\UpdateDepthPlayer.xpu",
        &m_pUpdateDepthPlayerPS ) ) )
    {
        ATG_PrintError( "Couldn't create UpdateDepthPlayer.xpu\n" );
        return ATGAPPERR_MEDIANOTFOUND;
    }

    if( FAILED( ATG::LoadPixelShader( "game:\\Media\\Shaders\\UpdateColorBackground.xpu",
        &m_pUpdateColorBackgroundPS ) ) )
    {
        ATG_PrintError( "Couldn't create UpdateColorBackground.xpu\n" );
        return ATGAPPERR_MEDIANOTFOUND;
    }    

    if( FAILED( ATG::LoadPixelShader( "game:\\Media\\Shaders\\UpdateColorPlayer.xpu",
        &m_pUpdateColorPlayerPS ) ) )
    {
        ATG_PrintError( "Couldn't create UpdateColorPlayer.xpu\n" );
        return ATGAPPERR_MEDIANOTFOUND;
    }    

    if( FAILED( ATG::LoadPixelShader( "game:\\Media\\Shaders\\BlendColorPlayerBackground.xpu",
        &m_pBlendColorPlayerBackgroundPS ) ) )
    {
        ATG_PrintError( "Couldn't create BlendColorPlayerBackground.xpu\n" );
        return ATGAPPERR_MEDIANOTFOUND;
    }    

    if( FAILED( ATG::LoadPixelShader( "game:\\Media\\Shaders\\PlayerMask.xpu",
        &m_pPlayerMaskPS ) ) )
    {
        ATG_PrintError( "Couldn't create PlayerMask.xpu\n" );
        return ATGAPPERR_MEDIANOTFOUND;
    }  

    if( FAILED( ATG::LoadPixelShader( "game:\\Media\\Shaders\\HeatDistortion.xpu",
        &m_pHeatDistortionPS ) ) )
    {
        ATG_PrintError( "Couldn't create HeatDistortion.xpu\n" );
        return ATGAPPERR_MEDIANOTFOUND;
    }        

    return S_OK;
}

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
    " }                                                                          ";

struct VideoFeedVertex
{
    FLOAT vPosition[ 3 ];
    FLOAT vTexCoords[ 2 ];
};

//--------------------------------------------------------------------------------------
// Name: InitializeVisualization()
//--------------------------------------------------------------------------------------
HRESULT Sample::InitializeVisualization( D3DDevice* pd3dDevice )
{
    // Compile vertex shader.
    ID3DXBuffer* pVertexShaderCode;
    ID3DXBuffer* pVertexErrorMsg;
    HRESULT hr = D3DXCompileShader( g_strVideoShaderHLSL,
                                    ( UINT )strlen( g_strVideoShaderHLSL ),
                                    NULL,
                                    NULL,
                                    "VideoVertexShader",
                                    "vs_2_0",
                                    0,
                                    &pVertexShaderCode,
                                    &pVertexErrorMsg,
                                    NULL );
    if( FAILED( hr ) )
    {
        if( pVertexErrorMsg )
            ATG::DebugSpew( ( char* )pVertexErrorMsg->GetBufferPointer() );
        return E_FAIL;
    }

    // Create vertex shader.
    pd3dDevice->CreateVertexShader( ( DWORD* )pVertexShaderCode->GetBufferPointer(),
                                    &m_pVideoVertexShader );

    // Compile pixel shader.
    ID3DXBuffer* pPixelShaderCode;
    ID3DXBuffer* pPixelErrorMsg;

    hr = D3DXCompileShader( g_strVideoShaderHLSL,
                            ( UINT )strlen( g_strVideoShaderHLSL ),
                            NULL,
                            NULL,
                            "VideoPixelShader",
                            "ps_2_0",
                            0,
                            &pPixelShaderCode,
                            &pPixelErrorMsg,
                            NULL );
    if( FAILED( hr ) )
    {
        if( pPixelErrorMsg )
            ATG::DebugSpew( ( char* )pPixelErrorMsg->GetBufferPointer() );
        return E_FAIL;
    }

    // Create pixel shader.
    pd3dDevice->CreatePixelShader( ( DWORD* )pPixelShaderCode->GetBufferPointer(),
                                   &m_pVideoPixelShaderRGB );

    if( FAILED( ATG::LoadPixelShader( "game:\\Media\\Shaders\\DisplayF16.xpu",
        &m_pVideoPixelShaderF16 ) ) )
    {
        ATG_PrintError( "Couldn't create DisplayF16.xpu\n" );
        return ATGAPPERR_MEDIANOTFOUND;
    }

    // Define the vertex elements and
    // Create a vertex declaration from the element descriptions.
    static const D3DVERTEXELEMENT9 VertexElements[] =
    {
        { 0,  0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
        { 0, 12, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
        D3DDECL_END()
    };
    pd3dDevice->CreateVertexDeclaration( VertexElements, &m_pVideoVertexDecl );

    // Create textures for displaying color and depth streams
    if ( FAILED( pd3dDevice->CreateTexture( m_DepthWidth, m_DepthHeight, 1, 0, D3DFMT_G16R16F, 0, &m_pDepthBackgroundTexture, NULL ) ) )
        return E_FAIL;
    m_PostProcess.ClearTexture( m_pDepthBackgroundTexture );        

    if ( FAILED( pd3dDevice->CreateTexture( m_DepthWidth, m_DepthHeight, 1, 0, D3DFMT_G16R16F, 0, &m_pDepthPlayerTexture, NULL ) ) )
        return E_FAIL;
    m_PostProcess.ClearTexture( m_pDepthPlayerTexture );

    if ( FAILED( pd3dDevice->CreateTexture( m_DepthWidth, m_DepthHeight, 1, 0, D3DFMT_G16R16F, 0, &m_pDepthPlayerMaskTexture, NULL ) ) )
        return E_FAIL;
    m_PostProcess.ClearTexture( m_pDepthPlayerMaskTexture );
    
    if ( FAILED( pd3dDevice->CreateTexture( m_ColorWidth, m_ColorHeight, 1, 0, ATG::GetAs16SRGBFormat( D3DFMT_X8R8G8B8 ), 0, &m_pColorBackgroundTexture, NULL ) ) )
        return E_FAIL;
    m_PostProcess.ClearTexture( m_pColorBackgroundTexture );    

    if ( FAILED( pd3dDevice->CreateTexture( m_ColorWidth, m_ColorHeight, 1, 0, ATG::GetAs16SRGBFormat( D3DFMT_X8R8G8B8 ), 0, &m_pColorPlayerTexture, NULL ) ) )
        return E_FAIL;
    m_PostProcess.ClearTexture( m_pColorPlayerTexture );

    if ( FAILED( pd3dDevice->CreateTexture( m_ColorWidth, m_ColorHeight, 1, 0, ATG::GetAs16SRGBFormat( D3DFMT_X8R8G8B8 ), 0, &m_pColorBlendedTexture, NULL ) ) )
        return E_FAIL;
    m_PostProcess.ClearTexture( m_pColorBlendedTexture );
    
    // Create the vertex buffer. Here we are allocating enough memory
    // (from the default pool) to hold all our 3 custom vertices. 
    if( FAILED( pd3dDevice->CreateVertexBuffer( 4 * sizeof( VideoFeedVertex ),
                                                D3DUSAGE_WRITEONLY,
                                                NULL,
                                                D3DPOOL_MANAGED,
                                                &m_pDisplayVB,
                                                NULL ) ) )
        return E_FAIL;
    

    UINT uWidth;
    UINT uHeight;
    ATG::GetVideoSettings( &uWidth, &uHeight );



    XVIDEO_MODE VideoMode;
    ZeroMemory( &VideoMode, sizeof( VideoMode ) );
    XGetVideoMode( &VideoMode );

    m_DisplayWindow.fWidth = uWidth * 0.65f;
    m_DisplayWindow.fHeight = uWidth * 3.f / 4.f * 0.65f;

    if( ( VideoMode.fIsWideScreen ) && ( uWidth == 640.f ) )
        m_DisplayWindow.fWidth /= 1.333f;

    m_DisplayWindow.fX = (uWidth - m_DisplayWindow.fWidth - 210) / 2.f;
    m_DisplayWindow.fY = ( uHeight - m_DisplayWindow.fHeight ) / 2.f;

    // Now we fill the vertex buffer. To do this, we need to Lock() the VB to
    // gain access to the vertices. This mechanism is required because the
    // vertex buffer may still be in use by the GPU. This can happen if the
    // CPU gets ahead of the GPU. The GPU could still be rendering the previous
    // frame.
    VideoFeedVertex g_Vertices[] =
    {
        { m_DisplayWindow.fX,                                                         
          m_DisplayWindow.fY, 0,  0, 0 },
        { m_DisplayWindow.fX + m_DisplayWindow.fWidth,               
          m_DisplayWindow.fY, 0,  1, 0 },
        { m_DisplayWindow.fX,                                                         
          m_DisplayWindow.fY + m_DisplayWindow.fHeight, 0,  0, 1 },
        { m_DisplayWindow.fX + m_DisplayWindow.fWidth, 
          m_DisplayWindow.fY + m_DisplayWindow.fHeight, 0,  1, 1 },
    };

    VideoFeedVertex* pVertices;
    if( FAILED( m_pDisplayVB->Lock( 0, 0, ( void** )&pVertices, 0 ) ) )
        return E_FAIL;
    memcpy( pVertices, g_Vertices, 4 * sizeof( VideoFeedVertex ) );
    m_pDisplayVB->Unlock();       

    return S_OK;
}

VOID Sample::RenderToTexture( LPDIRECT3DTEXTURE9 pDstTexture, LPDIRECT3DPIXELSHADER9 pPixelShader, DWORD dwEdramOffset )
{
    // Make sure that the required shaders and objects exist
    assert( pPixelShader );
    assert( pDstTexture );    

    DWORD OldDstFormat = pDstTexture->Format.DataFormat;
    if( pDstTexture->Format.DataFormat == GPUTEXTUREFORMAT_8_8_8_8_AS_16_16_16_16 )
    {
        pDstTexture->Format.DataFormat = GPUTEXTUREFORMAT_8_8_8_8;
    }

    // Create and set a render target
    D3DSURFACE_PARAMETERS surfaceParams =
    {
        0
    };
    surfaceParams.Base = dwEdramOffset;
    ATG::PushRenderTarget( 0L, ATG::CreateRenderTarget( pDstTexture, &surfaceParams ) );

    // Scale and copy the src texture
    m_pd3dDevice->SetPixelShader( pPixelShader );    

    // Draw a fullscreen quad to sample the RT
    m_PostProcess.DrawFullScreenQuad();

    m_pd3dDevice->Resolve( D3DRESOLVE_RENDERTARGET0, NULL, pDstTexture, NULL,
        0, 0, NULL, 1.0f, 0L, NULL );

    // Cleanup and return
    ATG::PopRenderTarget( 0L )->Release();
    m_pd3dDevice->SetPixelShader( NULL );

    pDstTexture->Format.DataFormat = OldDstFormat;
}

//--------------------------------------------------------------------------------------
// Name: Update()
// Desc: Called once per frame to update the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Update()
{
    // Get the current gamepad status
    ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput();

    // Toggle help
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
        m_bDrawHelp = !m_bDrawHelp;

    if ( pGamepad->wPressedButtons & XINPUT_GAMEPAD_A )
    {
        m_DemoMode = (DemoMode)( (INT)m_DemoMode + 1 );
        m_DemoMode = (DemoMode)( (INT)m_DemoMode % DEMOMODE_COUNT );
    }

    if ( pGamepad->wPressedButtons & XINPUT_GAMEPAD_X )
    {
        m_bPaused = !m_bPaused;
    }    

    switch ( m_DemoMode )
    {
        case DEMOMODE_DEPTH:
            if ( pGamepad->wPressedButtons & XINPUT_GAMEPAD_B )
            {
                m_DepthSubMode = (DepthSubMode)( (INT)m_DepthSubMode + 1 );
                m_DepthSubMode = (DepthSubMode)( (INT)m_DepthSubMode % DEPTHSUBMODE_COUNT );
            }
            break;

        case DEMOMODE_COLOR:
            if ( pGamepad->wPressedButtons & XINPUT_GAMEPAD_B )
            {
                m_ColorSubMode = (ColorSubMode)( (INT)m_ColorSubMode + 1 );
                m_ColorSubMode = (ColorSubMode)( (INT)m_ColorSubMode % COLORSUBMODE_COUNT );
            }
            break;
    }    
    
    if ( pGamepad->wPressedButtons & XINPUT_GAMEPAD_Y )
    {
        m_PostProcess.ClearTexture( m_pDepthBackgroundTexture );
        m_PostProcess.ClearTexture( m_pColorBackgroundTexture );
    }

    const NUI_IMAGE_FRAME* pDepthImageFrame;
    const NUI_IMAGE_FRAME* pColorImageFrame;    

    if ( WAIT_OBJECT_0 != WaitForSingleObject( m_hFrameEndEvent, NUI_FRAME_END_TIMEOUT_DEFAULT ) )
    {
        return E_FAIL;
    }
    
    HRESULT hrColor = NuiImageStreamGetNextFrame( m_hColorStream, 0, &pColorImageFrame );
    HRESULT hrDepth = NuiImageStreamGetNextFrame( m_hDepthStream, 0, &pDepthImageFrame );

    if ( SUCCEEDED( hrDepth ) )
    {
        m_pd3dDevice->SetTexture( 0, pDepthImageFrame->pFrameTexture );
        m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_POINT );
        m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_POINT );
        m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MIPFILTER, D3DTEXF_POINT );
        m_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP );
        m_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP );

        m_pd3dDevice->SetTexture( 1, m_pDepthBackgroundTexture );
        m_pd3dDevice->SetSamplerState( 1, D3DSAMP_MINFILTER, D3DTEXF_POINT );
        m_pd3dDevice->SetSamplerState( 1, D3DSAMP_MAGFILTER, D3DTEXF_POINT );
        m_pd3dDevice->SetSamplerState( 1, D3DSAMP_MIPFILTER, D3DTEXF_POINT );
        m_pd3dDevice->SetSamplerState( 1, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP );
        m_pd3dDevice->SetSamplerState( 1, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP );

        RenderToTexture( m_pDepthBackgroundTexture, m_pDepthPlayerRemovalPS );

        if ( (m_DemoMode == DEMOMODE_DEPTH && m_DepthSubMode == DEPTHSUBMODE_BACKGROUNDREMOVAL) || m_DemoMode == DEMOMODE_COLOR )
        {
            RenderToTexture( m_pDepthPlayerTexture, m_pUpdateDepthPlayerPS );
        }        

        m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
        m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
        m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR );

        m_pd3dDevice->SetTexture( 0, NULL );
        m_pd3dDevice->SetTexture( 1, NULL );

        m_pip.SetDepthTexture( pDepthImageFrame->pFrameTexture );
        NuiImageStreamReleaseFrame( m_hDepthStream, pDepthImageFrame );
    }
    
    if ( SUCCEEDED( hrColor ) )
    {
        if ( m_DemoMode == DEMOMODE_COLOR )
        {            
            m_pd3dDevice->SetTexture( 0, m_pDepthPlayerTexture );
            m_pd3dDevice->SetTexture( 1, pColorImageFrame->pFrameTexture );                
            m_pd3dDevice->SetTexture( 2, m_pColorBackgroundTexture );

            m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
            m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
            m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR );
            m_pd3dDevice->SetSamplerState( 1, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
            m_pd3dDevice->SetSamplerState( 1, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
            m_pd3dDevice->SetSamplerState( 1, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR );
            m_pd3dDevice->SetSamplerState( 2, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
            m_pd3dDevice->SetSamplerState( 2, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
            m_pd3dDevice->SetSamplerState( 2, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR );            

            switch ( m_ColorSubMode )
            {
                case COLORSUBMODE_PLAYERREMOVAL:
                    RenderToTexture( m_pColorBackgroundTexture, m_pUpdateColorBackgroundPS );
                    break;

                case COLORSUBMODE_BACKGROUNDREMOVAL:
                    RenderToTexture( m_pColorPlayerTexture, m_pUpdateColorPlayerPS );
                    break;

                case COLORSUBMODE_TRANSPARENTBLEND:
                    if ( !m_bPaused )
                        RenderToTexture( m_pColorBackgroundTexture, m_pUpdateColorBackgroundPS );
                    
                    m_pd3dDevice->SetTexture( 1, m_pPerlinNoiseTexture );
                    RenderToTexture( m_pColorBlendedTexture, m_pBlendColorPlayerBackgroundPS );
                    break;

                case COLORSUBMODE_HEATDISTORTION:
                    if ( !m_bPaused )
                        RenderToTexture( m_pColorBackgroundTexture, m_pUpdateColorBackgroundPS );

                    RenderToTexture( m_pDepthPlayerMaskTexture, m_pPlayerMaskPS );
                    m_PostProcess.GaussBlur5x5Texture( m_pDepthPlayerMaskTexture, m_pDepthPlayerMaskTexture );
                    m_PostProcess.GaussBlur5x5Texture( m_pDepthPlayerMaskTexture, m_pDepthPlayerMaskTexture );
                    m_PostProcess.GaussBlur5x5Texture( m_pDepthPlayerMaskTexture, m_pDepthPlayerMaskTexture );
                    m_PostProcess.GaussBlur5x5Texture( m_pDepthPlayerMaskTexture, m_pDepthPlayerMaskTexture );

                    m_pd3dDevice->SetTexture( 0, m_pDepthPlayerMaskTexture );
                    m_pd3dDevice->SetTexture( 1, m_pPerlinNoiseTexture );
                    m_pd3dDevice->SetSamplerState( 1, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP );
                    m_pd3dDevice->SetSamplerState( 1, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP );
                    FLOAT fAppTime = (FLOAT)m_Timer.GetAppTime()/10;                    
                    m_pd3dDevice->SetPixelShaderConstantF( 0, ( FLOAT* )&fAppTime, 1 );
                    RenderToTexture( m_pColorBlendedTexture, m_pHeatDistortionPS );
                    break;
            }                

            m_pd3dDevice->SetTexture( 0, NULL );      
            m_pd3dDevice->SetTexture( 1, NULL );            
            m_pd3dDevice->SetTexture( 2, NULL );
        }            

        m_pip.SetColorTexture( pColorImageFrame->pFrameTexture );
        NuiImageStreamReleaseFrame( m_hColorStream, pColorImageFrame );
    }          

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Name: Render()
// Desc: Called once per frame to render the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Render()
{
    // Draw a black background    
    ATG::RenderBackground( 0xff000000, 0xff000000 );

    // Set default states
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, FALSE );
    
    // Show title, frame rate, and help
    m_Timer.MarkFrame();
    if( m_bDrawHelp )
    {
        m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
    }
    else
    {
        // Render the processed video stream
        {
            m_pd3dDevice->SetVertexShader( m_pVideoVertexShader );            

            m_pd3dDevice->SetRenderState( D3DRS_VIEWPORTENABLE, FALSE );
            m_pd3dDevice->SetVertexDeclaration( m_pVideoVertexDecl );            
            m_pd3dDevice->SetStreamSource( 0, m_pDisplayVB, 0, sizeof( VideoFeedVertex ) );
            switch ( m_DemoMode )
            {
                case DEMOMODE_DEPTH:
                    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_POINT );
                    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_POINT );
                    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MIPFILTER, D3DTEXF_POINT );

                    m_pd3dDevice->SetPixelShader( m_pVideoPixelShaderF16 );
                    switch ( m_DepthSubMode )
                    {
                        case DEPTHSUBMODE_PLAYERREMOVAL:                                                        
                            m_pd3dDevice->SetTexture( 0, m_pDepthBackgroundTexture );
                            break;

                        case DEPTHSUBMODE_BACKGROUNDREMOVAL:
                            m_pd3dDevice->SetTexture( 0, m_pDepthPlayerTexture );
                            break;
                    }
                    break;

                case DEMOMODE_COLOR:
                    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
                    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
                    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR );

                    m_pd3dDevice->SetPixelShader( m_pVideoPixelShaderRGB );
                    switch ( m_ColorSubMode )
                    {
                        case COLORSUBMODE_PLAYERREMOVAL:
                            m_pd3dDevice->SetTexture( 0, m_pColorBackgroundTexture );
                            break;

                        case COLORSUBMODE_BACKGROUNDREMOVAL:
                            m_pd3dDevice->SetTexture( 0, m_pColorPlayerTexture );
                            break;

                        case COLORSUBMODE_TRANSPARENTBLEND:
                        case COLORSUBMODE_HEATDISTORTION:
                            m_pd3dDevice->SetTexture( 0, m_pColorBlendedTexture );
                            break;
                    }                    
                    break;
            }

            m_pd3dDevice->DrawPrimitive( D3DPT_TRIANGLESTRIP, 0, 2 );
        }

        // Draw the raw depth and image map as visualization.
        const FLOAT drawWidth = 200.0f;
        const FLOAT drawHeight = drawWidth * 3.0f / 4.0f;
        const FLOAT drawX = m_DisplayWindow.fX + m_DisplayWindow.fWidth + 20.f;
        const FLOAT drawY = m_DisplayWindow.fY + 100.f;
        m_pip.BeginRender();
        m_pip.RenderColorStream( drawX, drawY, drawWidth, drawHeight );
        m_pip.RenderDepthStream( drawX, drawY + drawHeight + 10, drawWidth, drawHeight );
        
        m_pip.EndRender();  
                
        m_Font.Begin();

        m_Font.SetScaleFactors( 1.2f, 1.2f );
        FLOAT fTextWidth;
        switch ( m_DemoMode )
        {
            case DEMOMODE_DEPTH:
                m_Font.DrawText( 0, 0, 0xffffffff,  L"Depth - " );
                fTextWidth = m_Font.GetTextWidth( L"Depth - " );

                switch ( m_DepthSubMode )
                {
                    case DEPTHSUBMODE_PLAYERREMOVAL:
                        m_Font.DrawText( fTextWidth, 0, 0xffffffff,  L"player removal" );
                        break;

                    case DEPTHSUBMODE_BACKGROUNDREMOVAL:
                        m_Font.DrawText( fTextWidth, 0, 0xffffffff,  L"background removal" );
                        break;
                }
                break;

            case DEMOMODE_COLOR:
                m_Font.DrawText( 0, 0, 0xffffffff,  L"Color - " );
                fTextWidth = m_Font.GetTextWidth( L"Depth - " );

                switch ( m_ColorSubMode )
                {
                    case COLORSUBMODE_PLAYERREMOVAL:
                        m_Font.DrawText( fTextWidth, 0, 0xffffffff,  L"player removal" );
                        break;

                    case COLORSUBMODE_BACKGROUNDREMOVAL:
                        m_Font.DrawText( fTextWidth, 0, 0xffffffff,  L"background removal" );
                        break;

                    case COLORSUBMODE_TRANSPARENTBLEND:
                        m_Font.DrawText( fTextWidth, 0, 0xffffffff,  L"ghost blend" );
                        break;

                    case COLORSUBMODE_HEATDISTORTION:
                        m_Font.DrawText( fTextWidth, 0, 0xffffffff,  L"heat distortion" );
                        break;
                }
                break;
        }
        
        m_Font.SetScaleFactors( 1.0f, 1.0f ); 
        m_Font.DrawText( 180, 500, 0xffffff00, L"Toggle demo modes: ", ATGFONT_RIGHT );
        m_Font.DrawText( 200, 500, 0xffffff00, GLYPH_A_BUTTON );
        m_Font.DrawText( 180, 525, 0xffffff00, L"Toggle sub-demo modes: ", ATGFONT_RIGHT );
        m_Font.DrawText( 200, 525, 0xffffff00, GLYPH_B_BUTTON );

        m_Font.DrawText( 0, 0, 0xffffff00, m_Timer.GetFrameRate(), ATGFONT_RIGHT );

        m_Font.End();
    }

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}
