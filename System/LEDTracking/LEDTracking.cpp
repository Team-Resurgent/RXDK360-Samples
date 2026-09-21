//--------------------------------------------------------------------------------------
// LEDTracking.cpp
//
// The sample shows an example method of tracking multiple LEDs with the Xbox 360 video 
// camera.
//
// LEDs are trackerd by using a filter mask. The LEDTracker class processes each frame
// retreived from the camera and returns an array of LED coordinates. The tracking
// itself requires that there is a small dark region around the LED spot. For example,
// holding up a discrete LED in a room will work, holding up a discrete LED in front of
// a window won't. But, if the LED is cased into an enclosure, the LED will be
// trackable even in front of a window. The case makes the LED spot stand out and not
// blend in with the outside light. 
//
// The color of the LED is also important. The Xbox 360 camera, as all CMOS cameras, is
// most sensitive to red light. Red LEDs are the best to use. Also, red LEDs tend to 
// have the highest light intensity. Infrared or orange LEDs might also work well and
// green or blue LEDs are barely detectable by the camera.
//
// There is also a lot that can be done with the returned coordinates. Depending on the
// application, various filters can be applied to smooth and elmiminate outliers. One
// such filter is the Kalman filter. The Kalman filter is not demonstrated in this
// sample, but there is ample literature on it on the web and in the AI research 
// community.
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include <xtl.h>
#include <xboxmath.h>
#include <xgraphics.h>
#include <xcam.h>
#include <AtgApp.h>
#include <AtgFont.h>
#include <AtgHelp.h>
#include <AtgInput.h>
#include <AtgResource.h>
#include <AtgUtil.h>
#include <AtgDebugDraw.h>
#include <AtgSimpleShaders.h>

#include "LEDTracker.h"


//--------------------------------------------------------------------------------------
// Camera parameters. Set to 640x480 to allow maximum resolution at 30fps.
//--------------------------------------------------------------------------------------
const XCAMRESOLUTION    g_nCameraResolution = XCAMRESOLUTION_640x480;
const DWORD             g_dwImageWidth = 640;
const DWORD             g_dwImageHeight = 480;
const DWORD             g_dwExposure = 1;

// Number of buffered frames. Needs to be a minimum of 2.
const DWORD             NUM_FRAME_BUFFERS = 2;

// LED tracker parameters. With these particular parameters, an LED can be detected
// in the 4 to 10 foot range. Increasing the radius, will detect LEDs that are closer
// to the camera. Decreasing the confidence may detect non-LED objects (false positives).
// The image threshold needs to be set such that based on the camera exposure and LED
// brightness, the LED can be seen by the camera. Setting the image threshold too low
// will allow more noise in the image and will increase the LED radius. Setting it 
// too high may not detect any LEDs.
const DWORD             g_dwLEDRadius = 2;
const FLOAT             g_fLEDMinimumConfidence = 0.80;
const DWORD             g_dwImageThreshold = 100;

//--------------------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//--------------------------------------------------------------------------------------
ATG::HELP_CALLOUT g_HelpCallouts[] =
{
    { ATG::HELP_BACK_BUTTON,  ATG::HELP_PLACEMENT_2, L"Display\nhelp" },
    { ATG::HELP_A_BUTTON,     ATG::HELP_PLACEMENT_1, L"Reset boxes" },
};
static const DWORD      NUM_HELP_CALLOUTS = sizeof( g_HelpCallouts ) / sizeof( g_HelpCallouts[ 0 ] );


//--------------------------------------------------------------------------------------
// Name: class Sample
// Desc: Main class to run this application. Most functionality is inherited
//       from the ATG::Application base class.
//--------------------------------------------------------------------------------------
class Sample : public ATG::Application
{
    ATG::Timer m_Timer;
    ATG::Font m_Font;
    ATG::PackedResource m_Resource;
    ATG::Help m_Help;
    BOOL m_bDrawHelp;

    IXCamVideo* m_pIXCamVideo;
    BOOL m_bCaptureModeSet;
    BYTE* m_pReadBuffers[NUM_FRAME_BUFFERS];
    BYTE* m_pCurrentFrame;
    XOVERLAPPED     m_ReadRequests[NUM_FRAME_BUFFERS];

    CLEDTracker m_LEDTracker;           // LED tracker related variables
    LEDCoordinate   m_LEDCoordinates[10];
    DWORD m_dwCoordinateCount;

    FLOAT           m_fObjectX[4];          // Objects that are dragged
    FLOAT           m_fObjectY[4];
    FLOAT           m_fCursorX[10];         // Current LED cursor coordinates
    FLOAT           m_fCursorY[10];
    FLOAT           m_fLastCursorX[10];     // Last LED cursor coordinates
    FLOAT           m_fLastCursorY[10];

    HRESULT         SetCameraDefaults();
    BOOL            UpdateVideoFeed();
    VOID            ReadFrame( DWORD dwIndex );

public:
    virtual HRESULT Initialize();
    virtual HRESULT Update();
    virtual HRESULT Render();
};


//--------------------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the program
//--------------------------------------------------------------------------------------
INT __cdecl main()
{
    Sample atgApp;

    ATG::GetVideoSettings( &atgApp.m_d3dpp.BackBufferWidth, &atgApp.m_d3dpp.BackBufferHeight );
    atgApp.Run();
}


//--------------------------------------------------------------------------------------
// Name: Initialize()
// Desc: This creates all device-dependent display objects.
//--------------------------------------------------------------------------------------
HRESULT Sample::Initialize()
{
    // Initialize variables
    m_pIXCamVideo = NULL;
    m_bDrawHelp = FALSE;
    m_bCaptureModeSet = FALSE;
    m_dwCoordinateCount = 0;

    // Initialize rectangle objects at random locations, these objects will
    // be moved by the LED cursor
    srand( GetTickCount() );
    for( DWORD i = 0; i < ARRAYSIZE( m_fObjectX ); ++i )
    {
        m_fObjectX[i] = ( FLOAT )( rand() % ( m_d3dpp.BackBufferWidth - 100 ) ) + 50.0f;
        m_fObjectY[i] = ( FLOAT )( rand() % ( m_d3dpp.BackBufferHeight - 100 ) ) + 50.0f;
    }

    ZeroMemory( m_fCursorX, sizeof( m_fCursorX ) );
    ZeroMemory( m_fCursorY, sizeof( m_fCursorY ) );
    ZeroMemory( m_fLastCursorX, sizeof( m_fLastCursorX ) );
    ZeroMemory( m_fLastCursorY, sizeof( m_fLastCursorY ) );

    // Create the font
    if( FAILED( m_Font.Create( "game:\\Media\\Fonts\\Arial_16.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Confine text drawing to the title safe area
    m_Font.SetWindow( ATG::GetTitleSafeArea() );

    // Create the help
    if( FAILED( m_Help.Create( "game:\\Media\\Help\\Help.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Initialize simple shaders
    ATG::SimpleShaders::Initialize( NULL, NULL );


    // Initialize camera
    if( ERROR_SUCCESS != XCamInitialize() )
        return E_FAIL;

    if( ERROR_SUCCESS != XCamCreateVideo( &m_pIXCamVideo ) )
    {
        XCamShutdown();
        return E_FAIL;
    }

    // Create XOVERLAPPED structures and image textures
    // Since all textures are YUY2, each pixel is 2 bytes
    const DWORD dwImageBytes = g_dwImageWidth * g_dwImageHeight * 2;
    ZeroMemory( m_ReadRequests, sizeof( m_ReadRequests ) );
    for( DWORD i = 0; i < NUM_FRAME_BUFFERS; ++i )
    {
        m_ReadRequests[i].hEvent = CreateEvent( NULL, FALSE, FALSE, NULL );
        m_pReadBuffers[i] = ( BYTE* )XPhysicalAlloc( dwImageBytes, MAXULONG_PTR, 0, PAGE_READWRITE );
        if( !m_pReadBuffers[i] )
            return E_FAIL;

        ZeroMemory( m_pReadBuffers[i], dwImageBytes );
    }

    m_pCurrentFrame = ( BYTE* )XPhysicalAlloc( dwImageBytes, MAXULONG_PTR, 0, PAGE_READWRITE );
    if( !m_pCurrentFrame )
        return E_FAIL;
    ZeroMemory( m_pCurrentFrame, dwImageBytes );

    // Initialize LED tracker class
    if( FAILED( m_LEDTracker.Initialize( g_dwLEDRadius, g_fLEDMinimumConfidence, g_dwImageThreshold ) ) )
        return E_FAIL;

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Update()
// Desc: Called once per frame, the call is the entry point for animating the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Update()
{
    // Get the current gamepad state
    ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput();

    // Toggle help
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
        m_bDrawHelp = !m_bDrawHelp;

    // Get the current camera state
    XCAMDEVICESTATE deviceState = XCamGetStatus();

    // If the camera device is active (not disconnected or initializing)
    if( deviceState == XCAMDEVICESTATE_INITIALIZED )
    {
        // Set camera parameters
        if( !m_bCaptureModeSet )
        {
            if( m_pIXCamVideo->SetCaptureMode( g_nCameraResolution, XCAMFRAMERATE_30, NULL ) != ERROR_SUCCESS )
                return E_FAIL;

            if( FAILED( SetCameraDefaults() ) )
                return E_FAIL;

            // Submit initial frame read requests for 2 frames only
            ReadFrame( 0 );
            ReadFrame( 1 );

            m_bCaptureModeSet = TRUE;
        }

        // The camera is active - update the current image
        if( UpdateVideoFeed() )
        {
            // Search for LEDs in the latest image buffer
            m_dwCoordinateCount = ARRAYSIZE( m_LEDCoordinates );
            m_LEDTracker.ProcessImage( m_pCurrentFrame, g_dwImageWidth, g_dwImageHeight,
                                       m_LEDCoordinates, &m_dwCoordinateCount );

            // Update LED cursor positions
            for( DWORD dwCursors = 0; dwCursors < m_dwCoordinateCount; ++dwCursors )
            {
                m_fLastCursorX[dwCursors] = m_fCursorX[dwCursors];
                m_fLastCursorY[dwCursors] = m_fCursorY[dwCursors];
                m_fCursorX[dwCursors] = ( ( FLOAT )g_dwImageWidth - m_LEDCoordinates[dwCursors].x ) * ( FLOAT )
                    m_d3dpp.BackBufferWidth / ( FLOAT )g_dwImageWidth;
                m_fCursorY[dwCursors] = m_LEDCoordinates[dwCursors].y * ( FLOAT )m_d3dpp.BackBufferHeight / ( FLOAT )
                    g_dwImageHeight;
            }
        }

        // Reset draggable rectangles
        if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_A )
        {
            for( DWORD i = 0; i < ARRAYSIZE( m_fObjectX ); ++i )
            {
                m_fObjectX[i] = ( FLOAT )( rand() % ( m_d3dpp.BackBufferWidth - 100 ) ) + 50.0f;
                m_fObjectY[i] = ( FLOAT )( rand() % ( m_d3dpp.BackBufferHeight - 100 ) ) + 50.0f;
            }
        }
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Render()
// Desc: Sets up render states, clears the viewport, and renders the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Render()
{
    // Draw a gradient filled background
    ATG::RenderBackground( 0xff0000ff, 0xff000000 );

    m_Timer.MarkFrame();

    // Show title, frame rate, and help
    if( m_bDrawHelp )
    {
        m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
    }
    else
    {
        m_Font.Begin();
        m_Font.SetScaleFactors( 1.2f, 1.2f );
        m_Font.DrawText( 0, 0, 0xffffffff, L"VideoCamera" );
        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( 0, 0, 0xffffff00, m_Timer.GetFrameRate(), ATGFONT_RIGHT );


        m_Font.DrawText( 0, 40, 0xffffffff, L"Camera Status: " );
        switch( XCamGetStatus() )
        {
            case XCAMDEVICESTATE_DISCONNECTED:
                m_Font.DrawText( 0xffffff00, L"Unplugged" );    break;
            case XCAMDEVICESTATE_CONNECTED:
                m_Font.DrawText( 0xffffff00, L"Plugged-in" );   break;
            case XCAMDEVICESTATE_INITIALIZED:
                m_Font.DrawText( 0xffffff00, L"Initialized" );  break;
            case XCAMDEVICESTATE_IN_ERROR:
                m_Font.DrawText( 0xffffff00, L"Error! (try reconnecting camera)" );
        }

        D3DRECT Rect;

        // Draw LED cursors and check collision with LED cursors with rectangle objects
        for( DWORD dwCursors = 0; dwCursors < m_dwCoordinateCount; ++dwCursors )
        {
            // Check collisions and move rectangles
            for( DWORD dwObjects = 0; dwObjects < ARRAYSIZE( m_fObjectX ); ++dwObjects )
            {
                if( fabs( m_fCursorX[dwCursors] - m_fObjectX[dwObjects] ) < 50.0f &&
                    fabs( m_fCursorY[dwCursors] - m_fObjectY[dwObjects] ) < 50.0f )
                {
                    m_fObjectX[dwObjects] += m_fCursorX[dwCursors] - m_fLastCursorX[dwCursors];
                    m_fObjectY[dwObjects] += m_fCursorY[dwCursors] - m_fLastCursorY[dwCursors];
                }
            }

            // Draw LED cursor
            Rect.x1 = ( LONG )m_fCursorX[dwCursors] - 5;
            Rect.x2 = Rect.x1 + 10;
            Rect.y1 = ( LONG )m_fCursorY[dwCursors] - 5;
            Rect.y2 = Rect.y1 + 10;
            ATG::DebugDraw::DrawScreenSpaceRect( Rect, 5.0f, 0xffff0000 );
        }

        // Draw rectangle objects
        for( DWORD dwObjects = 0; dwObjects < ARRAYSIZE( m_fObjectX ); ++dwObjects )
        {
            Rect.x1 = ( LONG )m_fObjectX[dwObjects] - 50;
            Rect.x2 = Rect.x1 + 100;
            Rect.y1 = ( LONG )m_fObjectY[dwObjects] - 50;
            Rect.y2 = Rect.y1 + 100;
            ATG::DebugDraw::DrawScreenSpaceRect( Rect, 3.0f, 0xff00ffff );
        }

        m_Font.End();
    }

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: UpdateVideoFeed()
// Desc: Retrieve most recent video frame. If there is a new complete frame
//       available, the function returns TRUE.
//--------------------------------------------------------------------------------------
BOOL Sample::UpdateVideoFeed()
{
    BOOL bNewFrame = FALSE;

    // Get the latest frame that is available from the camera. The loop is necessary
    // in order to reduce latency by always getting the most recent frame.
    for( DWORD dwBufferIndex = 0; dwBufferIndex < NUM_FRAME_BUFFERS; ++dwBufferIndex )
    {
        if( XHasOverlappedIoCompleted( &m_ReadRequests[dwBufferIndex] ) )
        {
            if( XGetOverlappedExtendedError( &m_ReadRequests[dwBufferIndex] ) == ERROR_SUCCESS )
            {
                XMemCpy( m_pCurrentFrame, m_pReadBuffers[dwBufferIndex], g_dwImageWidth * g_dwImageHeight * 2 );
                bNewFrame = TRUE;
            }

            ReadFrame( dwBufferIndex );
            break;
        }
    }

    return bNewFrame;
}


//--------------------------------------------------------------------------------------
// Name: ReadFrame()
// Desc: Submit request to begin reading a video frame.
//--------------------------------------------------------------------------------------
VOID Sample::ReadFrame( DWORD dwIndex )
{
    D3DLOCKED_RECT LockedRect;
    LockedRect.Pitch = g_dwImageWidth * 2;      // The YUY2 texture format has 16 bits per pixel
    LockedRect.pBits = ( void* )m_pReadBuffers[dwIndex];

    if( m_pIXCamVideo->ReadFrame( &LockedRect, &m_ReadRequests[dwIndex] ) != ERROR_IO_PENDING )
    {
        ATG::DebugSpew( "XCamReadFrame failed\n" );
    }
}


//--------------------------------------------------------------------------------------
// Name: SetCameraDefaults()
// Desc: Set camera default parameters.
//--------------------------------------------------------------------------------------
HRESULT Sample::SetCameraDefaults()
{
    // Turn off auto exposure
    if( XCamSetConfig( XCAMCONFIGID_AE_MODE, FALSE, NULL ) != ERROR_SUCCESS )
        return E_FAIL;

    // Set contrast and brightness
    if( XCamSetConfig( XCAMCONFIGID_BRIGHTNESS, 127, NULL ) != ERROR_SUCCESS )
        return E_FAIL;

    if( XCamSetConfig( XCAMCONFIGID_CONTRAST, 32, NULL ) != ERROR_SUCCESS )
        return E_FAIL;

    // Set gain to 0 to reduce noise
    if( XCamSetConfig( XCAMCONFIGID_GAIN, 0, NULL ) != ERROR_SUCCESS )
        return E_FAIL;

    // Set saturation to 0, to reduce noise and because LED color detection is not robust
    if( XCamSetConfig( XCAMCONFIGID_SATURATION, 0, NULL ) != ERROR_SUCCESS )
        return E_FAIL;

    // Set default gamma
    if( XCamSetConfig( XCAMCONFIGID_GAMMA, 180, NULL ) != ERROR_SUCCESS )
        return E_FAIL;

    // Set auto WB
    if( XCamSetConfig( XCAMCONFIGID_AUTO_WHITE_BAL, TRUE, NULL ) != ERROR_SUCCESS )
        return E_FAIL;

    // Set exsposure
    if( XCamSetConfig( XCAMCONFIGID_EXPOSURE_TIME, ( LONG )g_dwExposure, NULL ) != ERROR_SUCCESS )
        return E_FAIL;

    return S_OK;
}
