//--------------------------------------------------------------------------------------
// SimpleHeadTracking.cpp
//
// Microsoft Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include <xtl.h>
#include <nuiapi.h>
#include <AtgApp.h>
#include <AtgDebugDraw.h>
#include <AtgFont.h>
#include <AtgInput.h>
#include <AtgNuiCommon.h>
#include <AtgNuiVisualization.h>
#include <AtgSimpleShaders.h>


//--------------------------------------------------------------------------------------
// Name: Buffer sizes
// Desc: Define sizes for various buffers.
//--------------------------------------------------------------------------------------
const DWORD STRING_MESSAGE_SIZE = 128;


//--------------------------------------------------------------------------------------
// Name: class Sample
// Desc: Main class to run this application. Most functionality is inherited
//       from the ATG::Application base class.
//--------------------------------------------------------------------------------------
class Sample : public ATG::Application
{
public:
    
    virtual HRESULT Initialize();
    virtual HRESULT Update();
    virtual HRESULT Render();


private:

    // General sample data
    ATG::Timer              m_Timer;       // General usage timer
    ATG::Font               m_Font;        // Font used to display text
    ATG::NuiVisualization   m_NuiRenderer; // Used to render the depth map

    // Data used to recover Kinect depth map and head tracking information
    HANDLE                  m_hFrameEndEvent;             // This event is signaled when a new depth frame is available
    HANDLE                  m_hDepth320x240;              // Handle to the depth stream (used only for display in this sample)
    HANDLE                  m_hHeadPositionFrameEndEvent; // This event is signaled when head tracking processing has completed
    NUI_HEAD_POSITION_FRAME m_HeadTrackingFrame;          // Data from the latest head tracking frame
};


//--------------------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the program
//--------------------------------------------------------------------------------------
VOID __cdecl main()
{
    Sample atgApp;
    ATG::GetVideoSettings( &atgApp.m_d3dpp.BackBufferWidth, &atgApp.m_d3dpp.BackBufferHeight );
    atgApp.m_d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_TWO;

    // Make sure display is gamma correct.
    atgApp.m_d3dpp.BackBufferFormat =  ( D3DFORMAT )MAKESRGBFMT( D3DFMT_A8R8G8B8 );
    atgApp.m_d3dpp.FrontBufferFormat = ( D3DFORMAT )MAKESRGBFMT( D3DFMT_LE_X8R8G8B8 );

    atgApp.Run();
}


//--------------------------------------------------------------------------------------
// Name: Sample::Initialize()
// Desc: This creates all device-dependent display objects.
//--------------------------------------------------------------------------------------
HRESULT Sample::Initialize()
{
    // Initialize general sample states

    // Create the font
    RETURN_ON_FAIL( m_Font.Create( "game:\\Media\\Fonts\\Arial_20.xpr" ) );

    // Confine text drawing to the safe area
    m_Font.SetWindow( ATG::GetTitleSafeArea() );

    // Initialize the simple shaders, sensor and menus
    ATG::SimpleShaders::Initialize( NULL, NULL );

    ZeroMemory( &m_HeadTrackingFrame, sizeof( m_HeadTrackingFrame ) );

    // Create an event to be signaled when frame processing ends
    m_hFrameEndEvent = CreateEvent( NULL,
                                    FALSE,  // auto-reset
                                    FALSE,  // create unsignaled
                                    "NuiFrameEndEvent" );
    if ( ! m_hFrameEndEvent )
    {
        ATG_PrintError( "Failed to create NuiFrameEndEvent" );
        return E_FAIL;
    }

    // Create an event to be signaled when head position processing ends
    m_hHeadPositionFrameEndEvent = CreateEvent( NULL,
                                                FALSE,  // auto-reset
                                                FALSE,  // create unsignaled
                                                "NuiHeadPositionFrameEndEvent" );
    if ( ! m_hHeadPositionFrameEndEvent )
    {
        ATG_PrintError( "Failed to create NuiHeadPositionFrameEndEvent" );
        return E_FAIL;
    }

    // Initializes the Natural Input system on the default thread
    HRESULT hr = NuiInitialize( NUI_INITIALIZE_FLAG_USES_DEPTH | NUI_INITIALIZE_FLAG_USES_HEAD_POSITION | NUI_INITIALIZE_FLAG_NUI_GUIDE_DISABLED, NUI_INITIALIZE_DEFAULT_HARDWARE_THREAD );
    if ( FAILED( hr ) )
    {
        ATG::NuiPrintError( hr, "NuiInitialize" );
        return E_FAIL;
    }

    // Register frame end event with NUI
    hr = NuiSetFrameEndEvent( m_hFrameEndEvent, 0 );
    if( FAILED( hr ) )
    {
        ATG::NuiPrintError( hr, "NuiSetFrameEndEvent" );
        return E_FAIL;
    }

    // Open the 320 x 240 depth stream.
	hr = NuiImageStreamOpen( NUI_IMAGE_TYPE_DEPTH, NUI_IMAGE_RESOLUTION_320x240, 0, 1, NULL, &m_hDepth320x240 );
    if ( FAILED( hr ) )
    {
        ATG::NuiPrintError( hr, "NuiImageStreamOpen depth image 320x240" );
        return E_FAIL;
    }

    // Enable head position tracking
    hr = NuiHeadPositionEnable( m_hHeadPositionFrameEndEvent, 0 );
    if( FAILED( hr ) )
    {
        ATG::NuiPrintError( hr, "NuiHeadPositionEnable" );
        return E_FAIL;
    }

    // Initialize NuiVisualization object to render the depth map
    if( FAILED( m_NuiRenderer.Initialize( m_pd3dDevice, NUI_INITIALIZE_FLAG_USES_DEPTH ) ) )
    {
        ATG_PrintError( "Picture in Picture initialization failed" );
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Update()
// Desc: Called once per frame, the call is the entry point for animating the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Update()
{
    // Process the game inputs and allow user to exit sample
    ATG::Input::GetMergedInput();

    // Wait for frame processing to end
    if( WaitForSingleObject( m_hFrameEndEvent, NUI_FRAME_END_TIMEOUT_DEFAULT ) == WAIT_OBJECT_0 )
    {
        // Get data from the next camera depth frame
        CONST NUI_IMAGE_FRAME* pDepthFrame320x240;
        HRESULT hr = NuiImageStreamGetNextFrame( m_hDepth320x240, 0, &pDepthFrame320x240 );

        if( SUCCEEDED( hr ) )
        {
            m_NuiRenderer.SetDepthTexture( pDepthFrame320x240->pFrameTexture, FALSE );
        
            NuiImageStreamReleaseFrame( m_hDepth320x240, pDepthFrame320x240 );
        }       
    }

    // Wait for frame processing to end
    if( WaitForSingleObject( m_hHeadPositionFrameEndEvent, NUI_FRAME_END_TIMEOUT_DEFAULT ) == WAIT_OBJECT_0 )
    {
        NuiHeadPositionGetNextFrame( 0, &m_HeadTrackingFrame );
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Sample::Render()
// Desc: Sets up render states, clears the viewport, and renders the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Render()
{
    m_Timer.MarkFrame();

    //Draw a gradient filled background
    ATG::RenderBackground( D3DCOLOR_ARGB( 255, 0, 0, 255 ), D3DCOLOR_ARGB( 255, 0, 0, 0 ) );

    // Size of the depth map on screen
    const DWORD DEPTH_MAP_RENDER_WIDTH  = 640;
    const DWORD DEPTH_MAP_RENDER_HEIGHT = 480;

    // Origin of the depth map on screen
    DWORD dwDepthMapX = ( m_d3dpp.BackBufferWidth - DEPTH_MAP_RENDER_WIDTH ) / 2;
    DWORD dwDepthMapY = 200;


    // Show the depth map
    m_NuiRenderer.RenderDepthStream( ( FLOAT )dwDepthMapX, ( FLOAT )dwDepthMapY, ( FLOAT )DEPTH_MAP_RENDER_WIDTH, ( FLOAT )DEPTH_MAP_RENDER_HEIGHT );
    
    // Default text color
    const D3DCOLOR DEFAULT_COLOR = D3DCOLOR_RGBA( 255, 255, 255, 255 );

    // Render information relative to each tracked head
    for( UINT i = 0; i < NUI_HEAD_POSITION_COUNT; ++ i )
    {
        if( m_HeadTrackingFrame.HeadPositionData[ i ].TrackingID != NUI_SKELETON_INVALID_TRACKING_ID )
        {
            D3DCOLOR BoundsColor = DEFAULT_COLOR;

            // Compute the bounds in  depth map space for this head
            D3DRECT bounds;
            bounds.x1 = ( LONG )( dwDepthMapX + m_HeadTrackingFrame.HeadPositionData[ i ].Bounds.left * ( DEPTH_MAP_RENDER_WIDTH / 320.0 ) );
            bounds.y1 = ( LONG )( dwDepthMapY + m_HeadTrackingFrame.HeadPositionData[ i ].Bounds.top * ( DEPTH_MAP_RENDER_HEIGHT / 240.0 ) );
            bounds.x2 = ( LONG )( dwDepthMapX + m_HeadTrackingFrame.HeadPositionData[ i ].Bounds.right * ( DEPTH_MAP_RENDER_WIDTH / 320.0 ) );
            bounds.y2 = ( LONG )( dwDepthMapY + m_HeadTrackingFrame.HeadPositionData[ i ].Bounds.bottom * ( DEPTH_MAP_RENDER_HEIGHT / 240.0 ) );

            // Draw bounds for heads already bound or all heads if debug view is active
            ATG::DebugDraw::DrawScreenSpaceRect( bounds, 3,  BoundsColor );

            // Show debug info
            m_Font.Begin();
            D3DRECT OriginalWindow;
            m_Font.GetWindow( OriginalWindow );
            m_Font.SetWindow( bounds );
            m_Font.SetScaleFactors( 1.0f, 1.0f );
            WCHAR wszMessage[ STRING_MESSAGE_SIZE ];
            swprintf_s( wszMessage, L"0x%X / %d", m_HeadTrackingFrame.HeadPositionData[ i ].TrackingID, 
                                                  m_HeadTrackingFrame.HeadPositionData[ i ].Rank );
            m_Font.DrawText( ( FLOAT )( bounds.x2 - bounds.x1 ) / 2.0f, 0.0f, BoundsColor,  wszMessage, ATGFONT_CENTER_X );
            m_Font.SetWindow( OriginalWindow );
            m_Font.End();
        }
    }

    // Show title and frame rate
    m_Font.Begin();
    m_Font.SetScaleFactors( 1.2f, 1.2f );
    m_Font.DrawText( 0, 0, DEFAULT_COLOR,  L"Simple Head Tracking" );

    m_Font.SetScaleFactors( 1.0f, 1.0f );
    m_Font.DrawText( 0, 0, DEFAULT_COLOR, m_Timer.GetFrameRate(), ATGFONT_RIGHT );
    m_Font.End();

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}