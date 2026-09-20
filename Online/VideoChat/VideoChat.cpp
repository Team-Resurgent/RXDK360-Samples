//--------------------------------------------------------------------------------------
// VideoChat.cpp
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include <xtl.h>
#include <winsockx.h>
#include <xonline.h>
#include <d3d9.h>
#include <xaudio2.h>
#include <xhv2.h>
#include <xcam.h>
#include <algorithm>
#include <nuiapi.h>
#include "AtgApp.h"
#include "AtgFont.h"
#include "AtgHelp.h"
#include "AtgInput.h"
#include "AtgResource.h"
#include "AtgUtil.h"
#include "AtgNuiCommon.h"
#include "AtgSignIn.h"
#include "OptInDialog.h"
#include "Socket.h"
#include "Stopwatch.h"
#include "VideoChat.h"


//--------------------------------------------------------------------------------------
// Constatns and definitions
//--------------------------------------------------------------------------------------
// Port 1000 gives 0 extra port overhead on the wire
// Ports 1001-1255 give 2 bytes overhead on the wire
// All other ports give 4 bytes overhead on the wire
const WORD          DIRECT_PORT = 1000;  // Any port other than BROADCAST_PORT
const WORD          VIDEO_PORT = 1001;  // Video data port
const WORD          BROADCAST_PORT = 1002;  // Could be any port
const WORD          RELIABLE_PORT = 1003;  // Port for low-bandwidth reliable msgs

const INT           BITRATE_STEP = 16;           // Bitrate change step

const FLOAT         PAN_FACTOR = 200.f;

// Display positions
const FLOAT         LOCALPREVIEW_X1 = 0.6f;
const FLOAT         LOCALPREVIEW_X2 = 0.4f;
const FLOAT         LOCALPREVIEW_Y1 = 0.2f;
const FLOAT         LOCALPREVIEW_Y2 = 0.9f;


const FLOAT         REMOTE_Y1 = 0.32f;
const FLOAT         REMOTE_Y2 = 0.2f;

const FLOAT         REMOTE0_X1_1 = 0.4f;
const FLOAT         REMOTE0_X2_1 = 0.6f;

const FLOAT         REMOTE0_X1_2 = 0.20f;
const FLOAT         REMOTE0_X2_2 = 0.40f;
const FLOAT         REMOTE1_X1_2 = 0.60f;
const FLOAT         REMOTE1_X2_2 = 0.80f;

const FLOAT         REMOTE0_X1_3 = 0.075f;
const FLOAT         REMOTE0_X2_3 = 0.275f;
const FLOAT         REMOTE2_X1_3 = 0.4f;
const FLOAT         REMOTE2_X2_3 = 0.6f;
const FLOAT         REMOTE1_X1_3 = 0.725f;
const FLOAT         REMOTE1_X2_3 = 0.925f;

const FLOAT         CURSOR0_X = LOCALPREVIEW_X2 - 0.01f;
const FLOAT         CURSOR0_Y = LOCALPREVIEW_Y2 - 0.38f;
const FLOAT         LOCALPREVIEW_ERROR_X1 = LOCALPREVIEW_X2 - 0.05f;
const FLOAT         LOCALPREVIEW_ERROR_Y1 = LOCALPREVIEW_Y2 - 0.32f;

const FLOAT         CURSOR1_X_1 = REMOTE0_X1_1 - 0.01f;
const FLOAT         CURSOR1_X_2 = REMOTE0_X1_2 - 0.01f;
const FLOAT         CURSOR2_X_2 = REMOTE1_X1_2 - 0.01f;
const FLOAT         CURSOR1_X_3 = REMOTE0_X1_3 - 0.01f;
const FLOAT         CURSOR2_X_3 = REMOTE1_X1_3 - 0.01f;
const FLOAT         CURSOR3_X_3 = REMOTE2_X1_3 - 0.01f;
const FLOAT         CURSOR1_Y = REMOTE_Y1 - 0.12f;
const FLOAT         CURSOR2_Y = REMOTE_Y1 - 0.12f;
const FLOAT         CURSOR3_Y = REMOTE_Y1 - 0.12f;


// Drop stream if bitrate is low for this amount of seconds
const DWORD         LOW_BITRATE_TIMEOUT = 10;
// And falls between the minimum bitrate and minimum bitrate + offset
const DWORD         LOW_BITRATE_OFFSET = 16;

//--------------------------------------------------------------------------------------
// Globals
//--------------------------------------------------------------------------------------
XCAMFRAMERATE g_FrameRate[] =
{
    XCAMFRAMERATE_30,
    XCAMFRAMERATE_25,
    XCAMFRAMERATE_20,
    XCAMFRAMERATE_15,
    XCAMFRAMERATE_10,
    XCAMFRAMERATE_5,
};
const INT           NUMBER_OF_RATE = sizeof( g_FrameRate ) / sizeof( g_FrameRate[0] );

// Menu text options for resolutions supported for USB camera
const WCHAR* const  g_strUSBMenu[] =
{
    L"QQVGA (160x120)",
    L"QCIF (176x144)",
    L"QVGA (320x240)",
    L"CIF (352x288)",
    L"VGA (640x480)"
};

// Menu text options for resolutions supported for Kinect camera
const WCHAR* const  g_strKinectMenu[] =
{
    L"QQVGA (160x120)",
    L"QVGA (320x240)",
    L"VGA (640x480)"
};

//--------------------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the program
//--------------------------------------------------------------------------------------
VOID __cdecl main()
{
    VideoSample atgApp;
    ATG::GetVideoSettings( &atgApp.m_d3dpp.BackBufferWidth, &atgApp.m_d3dpp.BackBufferHeight );
    atgApp.Run();
}


//-----------------------------------------------------------------------------
// Name: VideoSample()
// Desc: Constructor
//-----------------------------------------------------------------------------
VideoSample::VideoSample() : ATG::Application(),
                             m_Font        (),
                             m_OnlineIconsFont(),
                             m_Help        (),
                             m_DisplayHelp ( FALSE ),
                             m_FullscreenMode ( FALSE ),
                             m_State       ( STATE_MENU ),
                             m_CurrMenuItem( 0 ),
                             m_CurrGameIndex( 0 ),
                             m_Games       (),
                             m_Machines    (),
                             m_bIsHost             ( FALSE ),
                             m_xnHostKeyID         (),
                             m_xnHostKeyExchange   (),
                             m_xnTitleAddress      (),
                             m_inHostAddr          (),
                             m_BroadSock           (),
                             m_DirectSock          (),
                             m_ReliableSock        (),
                             m_VideoSock           (),
                             m_Nonce               (),
                             m_SignedInMask        ( 0 ),
                             m_msgVoiceData        ( MSG_VOICEDATA ),
                             m_SinglePacketDrop    ( FALSE ),
                             m_ContinuousPacketDrop ( FALSE ),
                             m_bUseKinectCamera    ( FALSE ),
                             m_bFirstKinectFrameReceived ( FALSE ),
                             m_hNextFrameEventHandle ( NULL ),
                             m_hEncodeDoneEventHandle ( NULL ),
                             m_hNuiColorStreamHandle ( NULL ),
                             m_pEncoderFrameBuffer   ( NULL ), 
                             m_dwEncoderFrameBufferSize ( 0 )
{
    srand( GetTickCount() ); // for generating game/player names

    // Initialize strings
    m_strGameName[ 0 ] = L'\0';

    // Initialize low bitrate timer
    m_dwLowBitrateTime = GetTickCount();
}


//-----------------------------------------------------------------------------
// Name: Initialize()
// Desc: 
//-----------------------------------------------------------------------------
HRESULT VideoSample::Initialize()
{
    // Create a font
    if( FAILED( m_Font.Create( "game:\\Media\\Fonts\\Arial_16.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Create a font for the online icons
    if( FAILED( m_OnlineIconsFont.Create( "game:\\Media\\Fonts\\OnlineIcons.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Initialize the help system
    if( FAILED( m_Help.Create( "game:\\Media\\Help\\Help.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Confine text drawing to the title safe area
    m_Font.SetWindow( ATG::GetTitleSafeArea() );

    // Prepare networking
    if( !InitXNet() )
        return E_FAIL;

    // Register our notification listener
    m_hNotification = XNotifyCreateListener( XNOTIFY_SYSTEM );
    if( m_hNotification == NULL || m_hNotification == INVALID_HANDLE_VALUE )
    {
        ATG::FatalError( "Failed to create state notification listener.\n" );
    }

    m_VideoPackets = new VideoPacket[ NUM_VIDEO_PACKET_BUFFERS ];
    for( DWORD i = 0; i < NUM_VIDEO_PACKET_BUFFERS; ++i )
    {
        ZeroMemory( &m_VideoPackets[ i ].xoverlapped, sizeof( XOVERLAPPED ) );
        m_AvailableVideoPackets.push_back( &m_VideoPackets[ i ] );
    }

    // Initialize opt-in dialog related variables
    ZeroMemory( &m_DialogOverlapped, sizeof( XOVERLAPPED ) );
    ZeroMemory( &m_DialogResult, sizeof( OPTIN_DIALOG_RESULT ) );
    m_bDialogShown = FALSE;
    m_bDialogDismissed = FALSE;
    m_bSystemUIShowing = FALSE;

    // Start the XHV Engine
    InitXHV();

    // Begin searching for games on the network
    SendFindGame();

    return S_OK;
}


//-----------------------------------------------------------------------------
// Name: Update()
// Desc: 
//-----------------------------------------------------------------------------
HRESULT VideoSample::Update()
{
    m_FrameSeconds = m_FrameTimer.GetElapsedSeconds();
    m_FrameTimer.StartZero();

    // Check for new notifications
    SystemNotificationsUpdate();

    ATG::SignIn::Update();

    // Update the gamepad structures...
    ATG::Input::GetMergedInput( m_SignedInMask );

    // Toggle the help screen
    if( ATG::Input::m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_BACK )
        m_DisplayHelp = !m_DisplayHelp;

    switch( m_State )
    {
        case STATE_MENU:
            UpdateMenu();        break;
        case STATE_GAME:
            UpdateGame();        break;
        case STATE_SELECT_SOURCE_AND_RESOLUTION:
            UpdateSelectSourceAndResolution(); break;
        case STATE_SELECT_NAME:
            UpdateSelectName();  break;
        case STATE_REQUEST_CONNECT:
        case STATE_REQUEST_JOIN:
            UpdateRequestJoin(); break;
        case STATE_ERROR:
            UpdateError(); break;
    }

    return S_OK;
}


//-----------------------------------------------------------------------------
// Name: UpdateMenu()
// Desc: 
//-----------------------------------------------------------------------------
VOID VideoSample::UpdateMenu()
{
    ATG::GAMEPAD* input = &ATG::Input::m_DefaultGamepad;

    ProcessBroadcastMessage();

    if( input->wPressedButtons & XINPUT_GAMEPAD_A )
    {
        // keep the index of the game we will be joining 
        m_CurrGameIndex = m_CurrMenuItem - 1;

        if( m_CurrMenuItem == 0 )
        {
            // Create the session key ID and exchange key
            INT iKeyCreated = XNetCreateKey( &m_xnHostKeyID,
                                             &m_xnHostKeyExchange );

            // Register the session
            INT iKeyRegistered = XNetRegisterKey( &m_xnHostKeyID,
                                                  &m_xnHostKeyExchange );
            if( iKeyCreated != NO_ERROR || iKeyRegistered != NO_ERROR )
            {
                ATG::FatalError( "Unable to start game session - problem creating/registering key\n" );
            }

            // We're the host
            m_bIsHost = TRUE;

            // TCR Naming of Multiple Game Sessions for System Link Play
            // Build a list of potential game names
            for( DWORD i = 0; i < MAX_GAME_NAMES; ++i )
                GenRandom( m_GameNames[ i ], MAX_GAME_NAME_LENGTH );

            // Start at the top of the list
            m_CurrMenuItem = 0;

            m_State = STATE_SELECT_NAME;
        }
        else
        {
            m_State = STATE_SELECT_SOURCE_AND_RESOLUTION;
        }
    }
    else if( input->wPressedButtons & XINPUT_GAMEPAD_DPAD_UP )
    {
        if( m_CurrMenuItem == 0 )
            m_CurrMenuItem = m_Games.size();
        else
            --m_CurrMenuItem;
    }
    else if( input->wPressedButtons & XINPUT_GAMEPAD_DPAD_DOWN )
    {
        if( m_CurrMenuItem == m_Games.size() )
            m_CurrMenuItem = 0;
        else
            ++m_CurrMenuItem;
    }
    else if( input->wPressedButtons & XINPUT_GAMEPAD_B )
    {
        // Begin searching for games on the network
        SendFindGame();
    }
}


//-----------------------------------------------------------------------------
// Name: UpdateGame()
// Desc: Animate game
//-----------------------------------------------------------------------------
VOID VideoSample::UpdateGame()
{
    if( m_bUseKinectCamera )
    {

        if( WAIT_OBJECT_0 == WaitForSingleObject( m_hNextFrameEventHandle, NUI_FRAME_END_TIMEOUT_DEFAULT ) )
        {
            const NUI_IMAGE_FRAME* pColorImageFrame;

            HRESULT hrImage = NuiImageStreamGetNextFrame( m_hNuiColorStreamHandle, 0, &pColorImageFrame );
            if( SUCCEEDED( hrImage ) )
            {
                m_bFirstKinectFrameReceived = TRUE; 

                D3DLOCKED_RECT LockedSrc;
                pColorImageFrame->pFrameTexture->LockRect( 0, &LockedSrc, NULL, D3DLOCK_READONLY );

                ResizeUYVYToYUY2( m_pEncoderFrameBuffer, m_XCamStreamInitParams.VideoResolution, (BYTE*)LockedSrc.pBits, XCAMRESOLUTION_640x480 );

                pColorImageFrame->pFrameTexture->UnlockRect( 0 );
                NuiImageStreamReleaseFrame( m_hNuiColorStreamHandle, pColorImageFrame  );

            }
            else
            { 
                // NuiImageStreamGetNextFrame may return E_NUI_FRAME_NO_DATA, which is 
                // indicating no image frame is avaialble at this time. 
                ATG::NuiPrintError( hrImage, "NuiImageStreamGetNextFrame" );
            }

            // Submit the YUY2 frame buffer to XCam encoder for encoding
            XOVERLAPPED overlapped;
            ZeroMemory( &overlapped, sizeof( XOVERLAPPED ) );
            overlapped.hEvent = m_hEncodeDoneEventHandle;
            DWORD dwResult = m_XCamStreamEngine->SubmitLocalVideoFrame( m_pEncoderFrameBuffer , &overlapped );
            assert( dwResult == ERROR_SUCCESS );

            // Wait for the XCam encoder to finish encoding this frame 
            XGetOverlappedResult( &overlapped, &dwResult, TRUE);

        }
        else
        {
            ATG::DebugSpew( "Waiting for Nui new image signal has timed out or failed \n" ); 
        }
    }

    // Handle net messages
    while( ProcessBroadcastMessage() )
    {
    }
    while( ProcessDirectMessage() )
    {
    }
    while( ProcessReliableMessage() )
    {
    }
    while( ProcessVideoMessage() )
    {
    }

    // Send keep-alives
    if( m_HeartbeatTimer.GetElapsedSeconds() > PLAYER_HEARTBEAT )
    {
        // Send hearbeat via VDP directly to all other players
        Message msgHeartbeat( MSG_HEARTBEAT );
        SendMessage( &msgHeartbeat, FALSE );
        m_HeartbeatTimer.StartZero();
    }

    // Handle other players dropping
    ProcessMachineDropouts();

    CheckMicrophones();

    // Make sure we send voice data at an appropriate rate
    if( m_VoiceTimer.GetElapsedSeconds() > VOICE_PACKET_INTERVAL )
        SendVoiceDataToAll();

    SendVideoDataToAll();


    ATG::GAMEPAD* input = &ATG::Input::m_DefaultGamepad;

    // Change encoder bitrate
    if( input->wPressedButtons & XINPUT_GAMEPAD_DPAD_UP )
    {
        m_TargetBitrate = min( m_TargetBitrate + BITRATE_STEP, m_XCamStreamInitParams.MaximumBitrate );
        m_XCamStreamEngine->SetEncoderTargetBitrate( m_TargetBitrate );
    }
    else if( input->wPressedButtons & XINPUT_GAMEPAD_DPAD_DOWN )
    {
        m_TargetBitrate = max( m_TargetBitrate - BITRATE_STEP, m_XCamStreamInitParams.MinimumBitrate );
        m_XCamStreamEngine->SetEncoderTargetBitrate( m_TargetBitrate );
    }

    // Change encoder framerate
    if( ( input->wPressedButtons & XINPUT_GAMEPAD_DPAD_RIGHT ) )
    {
        if( m_XCamFramerateIndex > 0 ) m_XCamFramerateIndex--;
        m_XCamStreamEngine->SetEncoderTargetFramerate( ( XCAMFRAMERATE )g_FrameRate[ m_XCamFramerateIndex ] );
    }
    else if( ( input->wPressedButtons & XINPUT_GAMEPAD_DPAD_LEFT ) )
    {
        if( m_XCamFramerateIndex < ( NUMBER_OF_RATE - 1 ) ) m_XCamFramerateIndex++;
        m_XCamStreamEngine->SetEncoderTargetFramerate( ( XCAMFRAMERATE )g_FrameRate[ m_XCamFramerateIndex ] );
    }
    
    // Toggle fullscreen mode
    if( input->wPressedButtons & XINPUT_GAMEPAD_A )
    {
        m_FullscreenMode = !m_FullscreenMode;
        RecalculateDisplayPositions();
    }

    // Cycle through the video feeds
    if( input->wPressedButtons & XINPUT_GAMEPAD_X )
        m_CurrMenuItem = ( m_CurrMenuItem == m_Machines.size() ) ? 0 : m_CurrMenuItem + 1;

    // Initiate a single packet drop
    if( input->wPressedButtons & XINPUT_GAMEPAD_Y )
        m_SinglePacketDrop = TRUE;

    // Back to  menu
    if( input->wPressedButtons & XINPUT_GAMEPAD_B )
    {
        // We will release the XCam stream engine here and 
        // go back to main menu.
        m_CurrMenuItem = 0;
        m_State = STATE_MENU;
        
        if( m_bUseKinectCamera ) 
            ShutdownKinectCamera();

        // Before releasing the XCam stream engine all registered remote machines 
        // must be unregistered. 
        for( DWORD i = 0; i < m_Machines.size(); ++i )
            m_XCamStreamEngine->UnregisterRemoteConsole( m_Machines[ i ].xnAddr );
       
        m_XCamStreamEngine->Release();
        return;
    }

    // Continuously drop packets while X is held down
    m_ContinuousPacketDrop = ( input->wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER );

    // Compare current actual bitrate to minimum bitrate + offset. 
    // If actual bitrate is continuously lower for LOW_BITRATE_TIMEOUT seconds, end the stream
    if( m_XCamStreamEngine->GetEncoderActualBitrate() < ( m_XCamStreamInitParams.MinimumBitrate -
                                                          LOW_BITRATE_OFFSET ) &&
        ( GetTickCount() - m_dwLowBitrateTime ) > ( LOW_BITRATE_TIMEOUT * 1000 ) )
    {
        ATG::DebugSpew( "XCam Encoder bitrate has been lower than the specified minimum bitrate for longer than the timeout period, ending the video chat stream. \n");
                   
        m_CurrMenuItem = 0;
        m_State = STATE_MENU;
        m_XCamStreamEngine->Release();
    }
    else
    {
        m_dwLowBitrateTime = GetTickCount();
    }
}


VOID VideoSample::UpdateSelectSourceAndResolution()
{

    ATG::GAMEPAD* input = &ATG::Input::m_DefaultGamepad;

    if( input->wPressedButtons & XINPUT_GAMEPAD_A )
    {
        m_bUseKinectCamera = ( m_CurrMenuItem < KINECT_CAMERA_RESOLUTION_MENU_MAX ) ? TRUE : FALSE; 

        if( m_bUseKinectCamera )
        {
            switch( m_CurrMenuItem )
            {
            case KINECT_CAMERA_RESOLUTION_MENU_QQVGA:
                m_XCamStreamInitParams.VideoResolution = XCAMRESOLUTION_160x120; break;
            case KINECT_CAMERA_RESOLUTION_MENU_QVGA:
                m_XCamStreamInitParams.VideoResolution = XCAMRESOLUTION_320x240; break;
            case KINECT_CAMERA_RESOLUTION_MENU_VGA:
                m_XCamStreamInitParams.VideoResolution = XCAMRESOLUTION_640x480; break;            
            }
        }
        else
        {
            switch( m_CurrMenuItem - KINECT_CAMERA_RESOLUTION_MENU_MAX )
            {
            case USB_CAMERA_RESOLUTION_MENU_QQVGA:
                m_XCamStreamInitParams.VideoResolution = XCAMRESOLUTION_160x120; break;
            case USB_CAMERA_RESOLUTION_MENU_QCIF:
                m_XCamStreamInitParams.VideoResolution = XCAMRESOLUTION_176x144; break;
            case USB_CAMERA_RESOLUTION_MENU_QVGA:
                m_XCamStreamInitParams.VideoResolution = XCAMRESOLUTION_320x240; break;
            case USB_CAMERA_RESOLUTION_MENU_CIF:
                m_XCamStreamInitParams.VideoResolution = XCAMRESOLUTION_352x288; break;
            case USB_CAMERA_RESOLUTION_MENU_VGA:
                m_XCamStreamInitParams.VideoResolution = XCAMRESOLUTION_640x480; break;
            }  
        }

        if( m_bUseKinectCamera )
        {
            if( FAILED( InitializeKinectCamera() ) )
                ATG::FatalError( "Failed to initialize NUI" );
        }

        if( m_CurrGameIndex != 0 )
        {
            InitializeXCam();

            m_CurrMenuItem = 0;
            m_State = STATE_GAME;
            m_HeartbeatTimer.StartZero();
            StartVoice();

            // Start listening for client connections
            m_ReliableSock.Listen();
        }
        else
        {
            assert( m_CurrGameIndex < m_Games.size() );
            InitiateJoin( m_CurrGameIndex );
        }
    }
    else if( input->wPressedButtons & XINPUT_GAMEPAD_B )
    {
        if( m_CurrMenuItem == 0 )
        {
            INT iKeyUnRegistered = XNetUnregisterKey( &m_xnHostKeyID );
            if( iKeyUnRegistered != NO_ERROR )
            {
                ATG::FatalError( "Unable to start game session - problem unregistering key\n" );
            }

            // Register the session
            INT iKeyRegistered = XNetRegisterKey( &m_xnHostKeyID,
                                                  &m_xnHostKeyExchange );
            if( iKeyRegistered != NO_ERROR )
            {
                ATG::FatalError( "Unable to start game session - problem registering key\n" );
            }

            // We're the host
            m_bIsHost = TRUE;

            // TCR Naming of Multiple Game Sessions for System Link Play
            // Build a list of potential game names
            for( DWORD i = 0; i < MAX_GAME_NAMES; ++i )
                GenRandom( m_GameNames[ i ], MAX_GAME_NAME_LENGTH );

            // Start at the top of the list
            m_CurrMenuItem = 0;

            m_State = STATE_SELECT_NAME;
        }
        else
            InitiateJoin( m_CurrGameIndex );
    }
    else if( input->wPressedButtons & XINPUT_GAMEPAD_DPAD_UP )
        m_CurrMenuItem = ( m_CurrMenuItem - 1 + USB_CAMERA_RESOLUTION_MENU_MAX + KINECT_CAMERA_RESOLUTION_MENU_MAX  ) % ( USB_CAMERA_RESOLUTION_MENU_MAX + KINECT_CAMERA_RESOLUTION_MENU_MAX);
    else if( input->wPressedButtons & XINPUT_GAMEPAD_DPAD_DOWN )
        m_CurrMenuItem = ( m_CurrMenuItem + 1 ) % (USB_CAMERA_RESOLUTION_MENU_MAX + KINECT_CAMERA_RESOLUTION_MENU_MAX );
}

//-----------------------------------------------------------------------------
// Name: UpdateSelectName()
// Desc: Animate game name selection
//-----------------------------------------------------------------------------
VOID VideoSample::UpdateSelectName()
{
    ATG::GAMEPAD* input = &ATG::Input::m_DefaultGamepad;

    if( input->wPressedButtons & XINPUT_GAMEPAD_A )
    {
        // Use the selected name
        wcscpy_s( m_strGameName, m_GameNames[ m_CurrMenuItem ] );

        m_CurrMenuItem = 0;
        m_State = STATE_SELECT_SOURCE_AND_RESOLUTION;
    }
    else if( input->wPressedButtons & XINPUT_GAMEPAD_B )
    {
        INT iKeyUnRegistered = XNetUnregisterKey( &m_xnHostKeyID );
        if( iKeyUnRegistered != NO_ERROR )
        {
            ATG::FatalError( "Unable to start game session - problem unregistering key\n" );
        }
        m_bIsHost = FALSE;
        m_CurrMenuItem = 0;
        m_State = STATE_MENU;
    }
    else if( input->wPressedButtons & XINPUT_GAMEPAD_DPAD_UP )
        m_CurrMenuItem = ( m_CurrMenuItem - 1 + MAX_GAME_NAMES ) % MAX_GAME_NAMES;
    else if( input->wPressedButtons & XINPUT_GAMEPAD_DPAD_DOWN )
        m_CurrMenuItem = ( m_CurrMenuItem + 1 ) % MAX_GAME_NAMES;
}


//-----------------------------------------------------------------------------
// Name: UpdateRequestJoin()
// Desc: Animate join request
//-----------------------------------------------------------------------------
VOID VideoSample::UpdateRequestJoin()
{
    // We wait for up to GAME_JOIN_TIME seconds. If the game didn't
    // respond, display an error message
    if( m_GameJoinTimer.GetElapsedSeconds() > GAME_JOIN_TIME )
    {
        m_GameJoinTimer.Stop();
        wcscpy_s( m_strError, L"Game did not respond!" );
        m_State = STATE_ERROR;
        return;
    }

    // First, we have to wait to see if our connection has completed
    if( m_State == STATE_REQUEST_CONNECT )
    {
        BOOL bWrite;
        BOOL bError;
        m_ReliableSock.Select( NULL, &bWrite, &bError );
        if( bError )
        {
            wcscpy_s( m_strError, L"Game did not respond!" );
            m_State = STATE_ERROR;
            return;
        }
        else if( bWrite )
        {
            // Request join approval from the game and await a response
            SOCKADDR_IN sa;
            sa.sin_family = AF_INET;
            sa.sin_addr = m_inHostAddr;
            sa.sin_port = htons( DIRECT_PORT );
            SendJoinGame( sa );
            m_State = STATE_REQUEST_JOIN;
        }
    }
    else
    {
        // See if the host has replied
        ProcessReliableMessage();
    }
}


//-----------------------------------------------------------------------------
// Name: UpdateError();
// Desc: 
//-----------------------------------------------------------------------------
VOID VideoSample::UpdateError()
{
    // Do nothing but wait for pressing A button

    ATG::GAMEPAD* input = &ATG::Input::m_DefaultGamepad;

    if( input->wPressedButtons & XINPUT_GAMEPAD_A )
    {
        m_State = STATE_MENU;
    }
}


//-----------------------------------------------------------------------------
// Name: Render()
// Desc: Render the scene
//-----------------------------------------------------------------------------
HRESULT VideoSample::Render()
{
    m_Font.SetScaleFactors( 1.0f, 1.0f );

    // Draw a gradient filled background
    ATG::RenderBackground( 0xff0000ff, 0xff000000 );

    switch( m_State )
    {
        case STATE_MENU:
            RenderMenu();        break;
        case STATE_GAME:
            RenderGame();        break;
        case STATE_SELECT_SOURCE_AND_RESOLUTION:
            RenderSelectResolution(); break;
        case STATE_SELECT_NAME:
            RenderSelectName();  break;
        case STATE_REQUEST_CONNECT:
        case STATE_REQUEST_JOIN:
            RenderRequestJoin(); break;
        case STATE_ERROR:
            RenderError(); break;
    }


    if( m_DisplayHelp )
    {
        // Need to enable the viewport since it is disabled when rendering the preview
        m_pd3dDevice->SetRenderState( D3DRS_VIEWPORTENABLE, TRUE );

        if( m_State == STATE_GAME )
            RenderGameHelp();
        else if( m_State == STATE_MENU )
            RenderStartHelp();
        else
            RenderHelp();
    }

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}


//-----------------------------------------------------------------------------
// Name: RenderMenu()
// Desc: Display menu
//-----------------------------------------------------------------------------
VOID VideoSample::RenderMenu()
{
    RenderHeader();

    m_Font.DrawText( 180, 80, 0xffffffff, L"Select game to join" );

    FLOAT fYtop = 150.0f;
    FLOAT fYdelta = 30.0f;
    DWORD dwColor = ( m_CurrMenuItem == 0 ) ? COLOR_HIGHLIGHT : COLOR_NORMAL;
    m_Font.DrawText( 180, fYtop, dwColor, L"** Start New Game **" );

    // Show list of games
    for( DWORD i = 0; i < m_Games.size(); ++i )
    {
        dwColor = ( m_CurrMenuItem == i + 1 ) ? COLOR_HIGHLIGHT : COLOR_NORMAL;
        WCHAR strGameInfo[ 1024 ];
        swprintf_s( strGameInfo, L"%.*s", MAX_GAME_NAME_LENGTH, m_Games[ i ].strGameName );

        // Denote full games
        if( m_Games[ i ].byNumPlayers == MAX_REMOTE_MACHINES )
            lstrcatW( strGameInfo, L" (full)" );

        m_Font.DrawText( 180, fYtop + ( i + 1 ) * fYdelta, dwColor, strGameInfo );
        switch( m_Games[i].Resolution )
        {
            case XCAMRESOLUTION_160x120:
                m_Font.DrawText( dwColor, g_strUSBMenu[0] );  break;
            case XCAMRESOLUTION_176x144:
                m_Font.DrawText( dwColor, g_strUSBMenu[1] );  break;
            case XCAMRESOLUTION_320x240:
                m_Font.DrawText( dwColor, g_strUSBMenu[2] );  break;
            case XCAMRESOLUTION_352x288:
                m_Font.DrawText( dwColor, g_strUSBMenu[3] );  break;
            case XCAMRESOLUTION_640x480:
                m_Font.DrawText( dwColor, g_strUSBMenu[4] );  break;
        }

    }
    // Show selected item with little triangle
    m_Font.DrawText( 150.0f, fYtop + ( fYdelta * m_CurrMenuItem ), 0xffffffff, GLYPH_RIGHT_TICK );

    // Show selected item with little triangle
    m_Font.DrawText( 150, 300, 0xffffffff, L"Press B button to update sessions" );
}


//--------------------------------------------------------------------------------------
// Helper function to get Width and Height from XCAM_RESOLUTION value
//--------------------------------------------------------------------------------------
inline FLOAT GetAspectRatio( XCAMRESOLUTION Resolution )
{
    return( Resolution == XCAMRESOLUTION_176x144 || Resolution == XCAMRESOLUTION_352x288 )
        ? ( 11.0f / 9.0f ) : ( 4.0f / 3.0f );
}


//-----------------------------------------------------------------------------
// Name: RenderGame()
// Desc: Display game
//-----------------------------------------------------------------------------
VOID VideoSample::RenderGame()
{
    D3DDISPLAYMODE mode;
    m_pd3dDevice->GetDisplayMode( 0, &mode );

    if( m_FullscreenMode )
    {
        if( m_CurrMenuItem == m_Machines.size() )
        {
            // Render the local preview frame if Kinect camera has started giving us frames, or 
            // if USB camera is initialized
            if( ( m_bUseKinectCamera && m_bFirstKinectFrameReceived ) || ( !m_bUseKinectCamera && ( XCamGetStatus() == XCAMDEVICESTATE_INITIALIZED ) ) )
            {
                m_XCamStreamEngine->RenderLocalPreview();
            }
            else
            {
                if( m_bUseKinectCamera )
                {
                    m_Font.DrawText( ( FLOAT )LOCALPREVIEW_ERROR_X1 * mode.Width,
                                     ( FLOAT )LOCALPREVIEW_ERROR_Y1 * mode.Height,
                                     0xffff0000,
                                     L"Kinect camera is initializing" );
                }
                else
                {
                    m_Font.DrawText( ( FLOAT )LOCALPREVIEW_ERROR_X1 * mode.Width,
                                     ( FLOAT )LOCALPREVIEW_ERROR_Y1 * mode.Height,
                                     0xffff0000,
                                     L"USB camera is disconnected or is initializing" );
                }


            }
        }
        else
            m_XCamStreamEngine->RenderRemoteConsole( m_Machines[ m_CurrMenuItem ].xnAddr );
    }
    else
    {
        // Render the local preview frame if Kinect camer has started giving us frames, or 
        // if USB camera is initialized
        if( m_bFirstKinectFrameReceived || ( XCamGetStatus() == XCAMDEVICESTATE_INITIALIZED ) )
        {
            m_XCamStreamEngine->RenderLocalPreview();
        }
        else
        {
            if( m_bUseKinectCamera )
            {
                m_Font.DrawText( ( FLOAT )LOCALPREVIEW_ERROR_X1 * mode.Width,
                                 ( FLOAT )LOCALPREVIEW_ERROR_Y1 * mode.Height,
                                 0xffff0000,
                                 L"Kinect camera is initializing" );
            }
            else
            {
                m_Font.DrawText( ( FLOAT )LOCALPREVIEW_ERROR_X1 * mode.Width,
                                 ( FLOAT )LOCALPREVIEW_ERROR_Y1 * mode.Height,
                                 0xffff0000,
                                 L"USB camera is disconnected or is initializing" );
            }
        }


        for( DWORD i = 0; i < m_Machines.size(); ++i )
            m_XCamStreamEngine->RenderRemoteConsole( m_Machines[ i ].xnAddr );


        FLOAT fPositions[][ 8 ] =
        {
            { CURSOR0_X, CURSOR0_Y, CURSOR0_X, CURSOR0_Y, CURSOR0_X, CURSOR0_Y, CURSOR0_X, CURSOR0_Y },
            { CURSOR1_X_1, CURSOR1_Y, CURSOR0_X, CURSOR0_Y, 0.f, 0.f, 0.f, 0.f },
            { CURSOR1_X_2, CURSOR1_Y, CURSOR2_X_2, CURSOR1_Y, CURSOR0_X, CURSOR0_Y, 0.f, 0.f },
            { CURSOR1_X_3, CURSOR1_Y, CURSOR2_X_3, CURSOR1_Y, CURSOR3_X_3, CURSOR1_Y, CURSOR0_X, CURSOR0_Y },
        };

        FLOAT x = fPositions[ m_Machines.size() ][ m_CurrMenuItem * 2 ] * mode.Width;
        FLOAT y = fPositions[ m_Machines.size() ][ m_CurrMenuItem * 2 + 1 ] * mode.Height;

        // Show selected stream with little triangle
        m_Font.DrawText( x, y, 0xffffffff, GLYPH_DOWN_TICK, ATGFONT_CENTER_Y );
    }

    RenderHeader();

    // Show video source and resolution, game name and stats
    if( m_bUseKinectCamera )
    {
        m_Font.DrawText( 80, 30, COLOR_GREEN, L"Source: Kinect Camera" );
    }
    else
    {
        m_Font.DrawText( 80, 30, COLOR_GREEN, L"Source: USB Camera" );
    }

    switch( m_XCamStreamInitParams.VideoResolution )
    {
        case XCAMRESOLUTION_160x120:
            m_Font.DrawText( 80, 55, COLOR_GREEN, g_strUSBMenu[ 0 ] );
            break;
        case XCAMRESOLUTION_176x144:
            m_Font.DrawText( 80, 55, COLOR_GREEN, g_strUSBMenu[ 1 ] );
            break;
        case XCAMRESOLUTION_320x240:
            m_Font.DrawText( 80, 55, COLOR_GREEN, g_strUSBMenu[ 2 ] );
            break;
        case XCAMRESOLUTION_352x288:
            m_Font.DrawText( 80, 55,  COLOR_GREEN, g_strUSBMenu[ 3 ] );
            break;
        case XCAMRESOLUTION_640x480:
            m_Font.DrawText( 80, 55, COLOR_GREEN, g_strUSBMenu[ 4 ] );
            break;
    }

    WCHAR buffer[ 1024 ];

    swprintf_s( buffer, L" / %d fps / %d kbps", g_FrameRate[ m_XCamFramerateIndex ], m_TargetBitrate );
    m_Font.DrawText( COLOR_GREEN, buffer );

    swprintf_s( buffer, L"Game name: %.*s", MAX_GAME_NAME_LENGTH, m_strGameName );
    m_Font.DrawText( 80, 80, COLOR_GREEN, buffer );

    m_Font.DrawText( COLOR_HIGHLIGHT, L"  Video Feed: " );

    if( m_FullscreenMode )
    {
        if( m_CurrMenuItem == m_Machines.size() )
        {
            m_Font.DrawText( COLOR_HIGHLIGHT, L"Self Preview" );
        }
        else
        {
            swprintf_s( buffer, L"Remote %d/%d", m_CurrMenuItem + 1, m_Machines.size() );
            m_Font.DrawText( COLOR_HIGHLIGHT, buffer );
        }
    }
    else
        m_Font.DrawText( COLOR_HIGHLIGHT, L"All" );
}


//-----------------------------------------------------------------------------
// Name: RenderSelectResolution()
// Desc: Display resolution menu
//-----------------------------------------------------------------------------
VOID VideoSample::RenderSelectResolution()
{
    RenderHeader();

    m_Font.DrawText( 180, 80, 0xffffffff, L"Select video source and resolution" );

    FLOAT fYTop = 150.0f;
    FLOAT fY = fYTop;
    FLOAT fYdelta = 30.0f;

    DWORD dwMenuTextColor = ( IsKinectCameraConnected() ) ? COLOR_NORMAL : COLOR_NOT_CONNECTED;
    
    m_Font.DrawText( 180, fY, 0xffffffff, ( ( IsKinectCameraConnected() ) ? L"Kinect Camera: (Connected)" : L"Kinect Camera: (Not connected)") );
    fY += fYdelta;
   
    // Draw Resolution Menu for Kinect camera
    for( DWORD i = 0; i < KINECT_CAMERA_RESOLUTION_MENU_MAX; ++i )
    {
        DWORD dwColor = dwMenuTextColor;

        // Show a little triangle beside the selected item, and use a highlighted color for text
        if ( m_CurrMenuItem == i )
        {
            dwColor = COLOR_HIGHLIGHT;
            m_Font.DrawText( 170.0f, fY, 0xffffffff, GLYPH_RIGHT_TICK );
        }
        
        m_Font.DrawText( 200, fY, dwColor, g_strKinectMenu[ i ] );
        
        
        fY += fYdelta;
    }

    fY += fYdelta;

    dwMenuTextColor =  ( IsUSBCameraConnected() ) ? COLOR_NORMAL : COLOR_NOT_CONNECTED;
       
    m_Font.DrawText( 180, fY, 0xffffffff, ( ( IsUSBCameraConnected() ) ? L"USB XCamera: (Connected)" : L"USB XCamera: (Not connected)" ) );
    fY += fYdelta;

    // Draw Resolution Menu for USB camera
    for( DWORD i = 0; i < USB_CAMERA_RESOLUTION_MENU_MAX; ++i )
    {
        DWORD dwColor = dwMenuTextColor; 
        
        // Show a little triangle beside the selected item, and use a highlighted color for text
        if ( m_CurrMenuItem == ( i + KINECT_CAMERA_RESOLUTION_MENU_MAX ) ) 
        {
            dwColor = COLOR_HIGHLIGHT;
            m_Font.DrawText( 170.0f, fY, 0xffffffff, GLYPH_RIGHT_TICK );
        }
        
        m_Font.DrawText( 200, fY, dwColor, g_strUSBMenu[ i ] );
        
        fY = fY + fYdelta;
    }
}


//-----------------------------------------------------------------------------
// Name: RenderSelectName()
// Desc: Display game name selection
//-----------------------------------------------------------------------------
VOID VideoSample::RenderSelectName()
{
    RenderHeader();

    m_Font.DrawText( 180, 80, 0xffffffff, L"Select a game name" );

    FLOAT fYtop = 150.0f;
    FLOAT fYdelta = 30.0f;

    // Show list of game names
    for( DWORD i = 0; i < MAX_GAME_NAMES; ++i )
    {
        DWORD dwColor = ( m_CurrMenuItem == i ) ? COLOR_HIGHLIGHT : COLOR_NORMAL;
        m_Font.DrawText( 180, fYtop + fYdelta * i, dwColor,
                         m_GameNames[ i ] );
    }

    // Show selected item with little triangle
    m_Font.DrawText( 150.0f, fYtop + fYdelta * m_CurrMenuItem, 0xffffffff, GLYPH_RIGHT_TICK );
}


//-----------------------------------------------------------------------------
// Name: RenderRequestJoin()
// Desc: Display join request sequence
//-----------------------------------------------------------------------------
VOID VideoSample::RenderRequestJoin()
{
    RenderHeader();
    m_Font.DrawText( 320, 240, 0xffffffff, L"Joining game",
                     ATGFONT_CENTER_X | ATGFONT_CENTER_Y );
}


//-----------------------------------------------------------------------------
// Name: RenderError()
// Desc: Display error message
//-----------------------------------------------------------------------------
VOID VideoSample::RenderError()
{
    RenderHeader();
    m_Font.DrawText( 180, 150, 0xffffffff, m_strError,
                     ATGFONT_CENTER_X | ATGFONT_CENTER_Y );
    m_Font.DrawText( 150, 300, 0xffffffff, L"Press A button to return" );
}


//-----------------------------------------------------------------------------
// Name: RenderHeader()
// Desc: Display standard text
//-----------------------------------------------------------------------------
VOID VideoSample::RenderHeader()
{
    WCHAR strName[ 64 ];
    wcscpy_s( strName, L"VideoChat" );

    if( m_bIsHost )
        lstrcatW( strName, L" (host)" );

    m_Font.SetScaleFactors( 1.2f, 1.2f );
    m_Font.DrawText( 0, 0, 0xffffffff, strName );
    m_Font.SetScaleFactors( 1.0f, 1.0f );
}


//-----------------------------------------------------------------------------
// Name: RenderStartHelp()
// Desc: Display help
//-----------------------------------------------------------------------------
VOID VideoSample::RenderStartHelp()
{
    static ATG::HELP_CALLOUT HelpCallouts[] =
    {
        { ATG::HELP_BACK_BUTTON,   ATG::HELP_PLACEMENT_1, L"Display help" },
        { ATG::HELP_A_BUTTON,      ATG::HELP_PLACEMENT_2, L"Select menu\nitem" },
        { ATG::HELP_DPAD,          ATG::HELP_PLACEMENT_1, L"Menu navigation" },
    };
    m_Help.Render( &m_Font, HelpCallouts, sizeof( HelpCallouts ) / sizeof( HelpCallouts[ 0 ] ) );
}


//-----------------------------------------------------------------------------
// Name: RenderHelp()
// Desc: Display help
//-----------------------------------------------------------------------------
VOID VideoSample::RenderHelp()
{
    static ATG::HELP_CALLOUT HelpCallouts[] =
    {
        { ATG::HELP_BACK_BUTTON,   ATG::HELP_PLACEMENT_1, L"Display help" },
        { ATG::HELP_A_BUTTON,      ATG::HELP_PLACEMENT_2, L"Select menu\nitem" },
        { ATG::HELP_B_BUTTON,      ATG::HELP_PLACEMENT_1, L"Back to\nprevious menu" },
        { ATG::HELP_DPAD,          ATG::HELP_PLACEMENT_1, L"Menu navigation" },
    };
    m_Help.Render( &m_Font, HelpCallouts, sizeof( HelpCallouts ) / sizeof( HelpCallouts[ 0 ] ) );
}


//-----------------------------------------------------------------------------
// Name: RenderGameHelp()
// Desc: Display help
//-----------------------------------------------------------------------------
VOID VideoSample::RenderGameHelp()
{
    static ATG::HELP_CALLOUT HelpCallouts[] =
    {
        { ATG::HELP_BACK_BUTTON,   ATG::HELP_PLACEMENT_1, L"Display help" },
        { ATG::HELP_DPAD,          ATG::HELP_PLACEMENT_2, L"Up/Down: Bitrate\nLeft/Right: Framerate" },
        { ATG::HELP_A_BUTTON,      ATG::HELP_PLACEMENT_2, L"Toggle\nFullscreen" },
        { ATG::HELP_X_BUTTON,      ATG::HELP_PLACEMENT_2, L"Cycle through\nvideo streams" },
        { ATG::HELP_Y_BUTTON,      ATG::HELP_PLACEMENT_2, L"Drop single\npacket" },
        { ATG::HELP_B_BUTTON,      ATG::HELP_PLACEMENT_1, L"Back to\nmenu" },
        { ATG::HELP_RIGHT_SHOULDER,ATG::HELP_PLACEMENT_1, L"Continuous\npacket loss" },
    };
    m_Help.Render( &m_Font, HelpCallouts, sizeof( HelpCallouts ) / sizeof( HelpCallouts[ 0 ] ) );
}


//-----------------------------------------------------------------------------
// Name: InitiateJoin()
// Desc: Send a join request to the specified game
//-----------------------------------------------------------------------------
VOID VideoSample::InitiateJoin( DWORD iCurrGame )
{
    assert( m_Games.size() > 0 );
    // Determine which game the player wants to join
    GameInfo gameInfo = m_Games[ iCurrGame ];

    m_XCamStreamInitParams.VideoResolution = gameInfo.Resolution;

    InitializeXCam();

    // Establish a session with the host game
    INT iResult = XNetRegisterKey( &gameInfo.xnHostKeyID,
                                   &gameInfo.xnHostKey );

    // Note that these errors should be handled gracefully.
    assert( iResult == NO_ERROR );

    // Save the key ID because we need to unregister it
    // Note that we don't need the key itself once it's been registered.
    CopyMemory( &m_xnHostKeyID, &gameInfo.xnHostKeyID, sizeof( XNKID ) );

    // Save the game and player name of the host
    lstrcpynW( m_strGameName, gameInfo.strGameName, MAX_GAME_NAME_LENGTH );

    // Convert the XNADDR of the host to the INADDR we'll use to
    // join the game
    iResult = XNetXnAddrToInAddr( &gameInfo.xnHostAddr,
                                  &m_xnHostKeyID, &m_inHostAddr );
    assert( iResult == NO_ERROR );

    // Open up a reliable socket to the host - this will be used
    // for low-bandwidth communications, such as join requests,
    // communicator status, etc.  We have to wait for the connection
    // to complete before sending out join request
    SOCKADDR_IN saHost;
    saHost.sin_family = AF_INET;
    saHost.sin_addr = m_inHostAddr;
    saHost.sin_port = htons( RELIABLE_PORT );

    m_ReliableSock.Connect( &saHost );

    m_GameJoinTimer.StartZero();
    m_State = STATE_REQUEST_CONNECT;

    // Physically clear the list of games to obliterate the key and XNADDR info
    // from prying eyes. This particular method works because m_Games
    // is a vector; if m_Games is not a vector each game must be
    // cleared individually
    if( !m_Games.empty() )
    {
        GameInfo* pGameList = &m_Games[ 0 ];
        ZeroMemory( pGameList, m_Games.size() * sizeof( GameInfo ) );

        // Destroy the list of games
        m_Games.clear();
    }
}


//-----------------------------------------------------------------------------
// Name: StartVoice()
// Desc: Initialize voice
//-----------------------------------------------------------------------------
VOID VideoSample::StartVoice()
{
    m_msgVoiceData.GetMsgVoiceData().wVoicePackets = 0;
    m_VoiceTimer.StartZero();
}


//-----------------------------------------------------------------------------
// Name: InitXNet()
// Desc: Initialize the network stack.
//-----------------------------------------------------------------------------
BOOL VideoSample::InitXNet()
{
    DWORD dwStatus = XNetGetEthernetLinkStatus();

    m_bIsOnline = ( dwStatus & XNET_ETHERNET_LINK_ACTIVE ) != 0;

    if( !m_bIsOnline )
        return FALSE;

    // Initialize the network stack
    INT iResult = XNetStartup( NULL );
    if( iResult != NO_ERROR )
        return FALSE;

    // Standard WinSock startup
    WSADATA WsaData;
    iResult = WSAStartup( MAKEWORD( 2, 2 ), &WsaData );
    if( iResult != NO_ERROR )
        return FALSE;

    // Online startup
    if( FAILED( XOnlineStartup() ) )
        return FALSE;

    // Make sure someone is signed in, as audio chat needs users to be signed into XBOX Live
    ATG::SignIn::Initialize( 1, 4, TRUE, 4 );

    // Start the asynchronous local address acquisition
    DWORD getXnAddrStatus = XNetGetTitleXnAddr( &m_xnTitleAddress );


    // The broadcast socket is a non-blocking socket on port BROADCAST_PORT.
    // All broadcast messages are automatically always encrypted.
    BOOL bSuccess = m_BroadSock.Open( CSocket::Type_UDP );
    if( !bSuccess )
        return FALSE;

    SOCKADDR_IN broadAddr;
    broadAddr.sin_family = AF_INET;
    broadAddr.sin_addr.s_addr = INADDR_ANY;
    broadAddr.sin_port = htons( BROADCAST_PORT );
    iResult = m_BroadSock.Bind( &broadAddr );
    assert( iResult != SOCKET_ERROR );

    DWORD dwNonBlocking = 1;
    iResult = m_BroadSock.IoCtlSocket( FIONBIO, &dwNonBlocking );
    assert( iResult != SOCKET_ERROR );

    BOOL bBroadcast = TRUE;
    iResult = m_BroadSock.SetSockOpt( SOL_SOCKET, SO_BROADCAST,
                                      &bBroadcast, sizeof( bBroadcast ) );
    assert( iResult != SOCKET_ERROR );

    // The direct socket is a non-blocking socket on port DIRECT_PORT.
    // Sockets are encrypted by default, but can have encryption disabled
    // as an optimization for non-secure messaging
    bSuccess = m_DirectSock.Open( CSocket::Type_VDP );
    if( !bSuccess )
        return FALSE;

    SOCKADDR_IN directAddr;
    directAddr.sin_family = AF_INET;
    directAddr.sin_addr.s_addr = INADDR_ANY;
    directAddr.sin_port = htons( DIRECT_PORT );
    iResult = m_DirectSock.Bind( &directAddr );
    assert( iResult != SOCKET_ERROR );
    iResult = m_DirectSock.IoCtlSocket( FIONBIO, &dwNonBlocking );
    assert( iResult != SOCKET_ERROR );

    // Create a reliable socket to use for low-bandwidth messages
    // that need to be sent reliably.  Clients will use this
    // socket to connect to the host.  The host uses this socket to
    // listen for incoming client connections.
    bSuccess = m_ReliableSock.Open( CSocket::Type_TCP );
    if( !bSuccess )
        return FALSE;

    SOCKADDR_IN reliableAddr;
    reliableAddr.sin_family = AF_INET;
    reliableAddr.sin_addr.s_addr = INADDR_ANY;
    reliableAddr.sin_port = htons( RELIABLE_PORT );
    iResult = m_ReliableSock.Bind( &reliableAddr );
    assert( iResult != SOCKET_ERROR );
    iResult = m_ReliableSock.IoCtlSocket( FIONBIO, &dwNonBlocking );
    assert( iResult != SOCKET_ERROR );

    // The video socket is a non-blocking socket on port DIRECT_PORT.
    // Sockets are encrypted by default, but can have encryption disabled
    // as an optimization for non-secure messaging
    bSuccess = m_VideoSock.Open( CSocket::Type_VDP );
    if( !bSuccess )
        return FALSE;

    SOCKADDR_IN videoAddr;
    videoAddr.sin_family = AF_INET;
    videoAddr.sin_addr.s_addr = INADDR_ANY;
    videoAddr.sin_port = htons( VIDEO_PORT );
    iResult = m_VideoSock.Bind( &videoAddr );
    assert( iResult != SOCKET_ERROR );
    iResult = m_VideoSock.IoCtlSocket( FIONBIO, &dwNonBlocking );
    assert( iResult != SOCKET_ERROR );

    // Wait for the asynchronous address aquistion to complete
    while( getXnAddrStatus == XNET_GET_XNADDR_PENDING )
    {
        getXnAddrStatus = XNetGetTitleXnAddr( &m_xnTitleAddress );
    }
    assert( getXnAddrStatus != XNET_GET_XNADDR_NONE );

    ZeroMemory( &m_LocalMachine, sizeof( Machine ) );
    m_LocalMachine.xnAddr = m_xnTitleAddress;

    // Note that this sample does not call either WSACleanup() or
    // XNetCleanup(). These functions should be called by your game to
    // free system resources when the player is no longer online but
    // is still playing the game (e.g. switched to single-player mode).


    return TRUE;
}


//-----------------------------------------------------------------------------
// Name: InitXHV
// Desc: Initializes XHV
//-----------------------------------------------------------------------------
HRESULT VideoSample::InitXHV()
{
    HRESULT hr = S_OK;

    // Zero out the local xuid list
    for( DWORD dwPort = 0; dwPort < XUSER_MAX_COUNT; ++dwPort )
        m_LocalXUIDs[dwPort] = INVALID_XUID;

	// Initialize XAudio2
	UINT32 flags = 0;
	m_pXAudio2 = 0;
	hr = XAudio2Create( &m_pXAudio2, flags );
	if( FAILED( hr ) || ( m_pXAudio2 == 0 ) )
		ATG::FatalError( "Failed to initialize XAudio" );

	// Create a mastering voice
	IXAudio2MasteringVoice *pMasteringVoice = 0;
	hr = m_pXAudio2->CreateMasteringVoice( &pMasteringVoice );
	if( FAILED( hr ) || ( pMasteringVoice == 0 ) )
        ATG::FatalError( "Failed to create Master voice" );

	// Set up parameters for the voice chat engine
	XHV_INIT_PARAMS xhvParams = {0};
	xhvParams.dwMaxRemoteTalkers            = XHV_MAX_REMOTE_TALKERS;
	xhvParams.dwMaxLocalTalkers             = XHV_MAX_LOCAL_TALKERS;
	xhvParams.localTalkerEnabledModes       = &XHV_VOICECHAT_MODE;
	xhvParams.remoteTalkerEnabledModes      = &XHV_VOICECHAT_MODE;
	xhvParams.dwNumLocalTalkerEnabledModes  = 1;
	xhvParams.dwNumRemoteTalkerEnabledModes = 1;
	xhvParams.pXAudio2                      = m_pXAudio2;

	// Create the engine
	hr = XHV2CreateEngine( &xhvParams, &m_hWorkerThread, &m_pXHV2Engine );
    if( FAILED( hr ) )
        ATG::FatalError( "Failed to create XHV Engine!" );

    return hr;
}


VOID VideoSample::InitializeXCam()
{
    switch( m_XCamStreamInitParams.VideoResolution )
    {
        case XCAMRESOLUTION_160x120:
        case XCAMRESOLUTION_176x144:
            m_XCamStreamInitParams.InitialTargetBitrate = 128;
            break;

        case XCAMRESOLUTION_320x240:
        case XCAMRESOLUTION_352x288:
            m_XCamStreamInitParams.InitialTargetBitrate = 256;
            break;

        case XCAMRESOLUTION_640x480:
            m_XCamStreamInitParams.InitialTargetBitrate = 512;
            break;
    }

    m_TargetBitrate = m_XCamStreamInitParams.InitialTargetBitrate;
    m_XCamFramerateIndex = 0;

    // Start the XVID Engine
    m_XCamStreamInitParams.MinimumBitrate = 64;
    m_XCamStreamInitParams.MaximumBitrate = 1000;
    m_XCamStreamInitParams.InitialTargetFramerate = XCAMFRAMERATE_30;
    m_XCamStreamInitParams.MaxRemoteConsoles = MAX_REMOTE_MACHINES;
    m_XCamStreamInitParams.NetworkDataPacketSize = MAX_VIDEO_PACKET_SIZE;
    m_XCamStreamInitParams.ThreadProcessorID = 4;
    m_XCamStreamInitParams.GenerateLocalPreview = TRUE;
    
    if ( m_bUseKinectCamera )
        m_XCamStreamInitParams.GetDataDirectFromCamera = FALSE;
    else
        m_XCamStreamInitParams.GetDataDirectFromCamera = TRUE;

    m_XCamStreamInitParams.CameraDataReadyCallback = NULL;
    m_XCamStreamInitParams.CameraDataReadyCallbackUserdata = NULL;
    m_XCamStreamInitParams.pD3DDevice = m_pd3dDevice;

    if( ERROR_SUCCESS != XCamCreateStreamEngine( &m_XCamStreamInitParams, &m_XCamStreamEngine ) )
        ATG::FatalError( "Unable to create XVID Streaming Engine!\n" );

    RecalculateDisplayPositions();

    //
    // Disable the screen saver so that the screen does not go dim
    // while video chatting!
    //
    XEnableScreenSaver( FALSE );
}


//-----------------------------------------------------------------------------
// Name: InitializeKinectCamera
// Desc: Initializes the Nui library and opens the color image stream using 
//       UYVY format.
//-----------------------------------------------------------------------------
HRESULT VideoSample::InitializeKinectCamera()
{
    // Create events for Nui next frame and XCam encode done events, both of them 
    // should be "Manual Reset" type events.
    RETURN_ON_NULL( m_hNextFrameEventHandle = CreateEvent( NULL, TRUE, FALSE, NULL ) ); 
    RETURN_ON_NULL( m_hEncodeDoneEventHandle = CreateEvent( NULL, TRUE, FALSE, NULL ) );

    // Initialize NUI system, we only need to use the color image stream for video chat 
    HRESULT hr = NuiInitialize( NUI_INITIALIZE_FLAG_USES_COLOR | NUI_INITIALIZE_FLAG_NUI_GUIDE_DISABLED , NUI_INITIALIZE_DEFAULT_HARDWARE_THREAD ) ;
    if( FAILED( hr ) )
    {
        ATG::NuiPrintError( hr, "NuiInitialize" );
        return E_FAIL;
    }
    
    hr = NuiImageStreamOpen( NUI_IMAGE_TYPE_COLOR_YUV,
                             NUI_IMAGE_RESOLUTION_640x480,
                             0,
                             1,
                             m_hNextFrameEventHandle,
                             &m_hNuiColorStreamHandle);
    if ( FAILED( hr ) )
    {
        ATG::NuiPrintError( hr, "NuiImageStreamOpen" );
        return E_FAIL;
    }

    // The XCam encoder accepts frames in YUY2 format, which uses 16 bits per pixel. 
    switch( m_XCamStreamInitParams.VideoResolution )
    {
        case XCAMRESOLUTION_160x120:
            m_dwEncoderFrameBufferSize = 160 * 120 * 2;
            break;
        case XCAMRESOLUTION_320x240:
            m_dwEncoderFrameBufferSize = 320 * 240 * 2;
            break;
        case XCAMRESOLUTION_640x480:
            m_dwEncoderFrameBufferSize = 640 * 480 * 2;
            break;
        default:
            ATG::FatalError( "The VideoChat sample does not support this encode resolution when Kinect camera is used as source.\n" );
    }

    // Allocate the buffer that will hold the captured image from NUI camera in YUY2 format. 
    RETURN_ON_NULL( m_pEncoderFrameBuffer = (BYTE*) malloc( m_dwEncoderFrameBufferSize ));

    return S_OK;
}

//-----------------------------------------------------------------------------
// Name: ShutdownKinectCamera
// Desc: Shutdowns the Nui library and releases the buffer and handles used for 
//       Kinect camera.
//-----------------------------------------------------------------------------
HRESULT VideoSample::ShutdownKinectCamera()
{
    if( m_pEncoderFrameBuffer != NULL )
    {
        free( m_pEncoderFrameBuffer );
        m_pEncoderFrameBuffer = NULL;
    }

    NuiShutdown();

    if( m_hEncodeDoneEventHandle != NULL )
    {
        CloseHandle( m_hEncodeDoneEventHandle );
        m_hEncodeDoneEventHandle = NULL;
    }
    
    if( m_hNextFrameEventHandle != NULL )
    {
        CloseHandle( m_hNextFrameEventHandle );
        m_hNextFrameEventHandle = NULL;
    }

    // Set this flag to false here, so that when we re-enter the chat session we 
    // do not show garbage at the preview screen while Kinect camera initializes.
    m_bFirstKinectFrameReceived = FALSE;
   
    return S_OK;
}

//-----------------------------------------------------------------------------
// Name: CheckMicrophones
// Desc: See if there's data to be sent
//-----------------------------------------------------------------------------
VOID VideoSample::CheckMicrophones()
{
    HRESULT hr;
    BYTE rgBuffer[ XHV_VOICECHAT_MODE_PACKET_SIZE ];
    DWORD dwBytesWritten;
    DWORD dwPort;

    for( dwPort = 0; dwPort < XUSER_MAX_COUNT; ++dwPort )
    {
        if( m_LocalXUIDs[ dwPort ] == INVALID_XUID )
          continue;

        do
        {
            dwBytesWritten = sizeof( rgBuffer );

            hr = m_pXHV2Engine->GetLocalChatData( dwPort, rgBuffer, &dwBytesWritten, NULL );

            if( SUCCEEDED( hr ) )
            {
                MsgVoiceData& msg = m_msgVoiceData.GetMsgVoiceData();

                if( msg.wVoicePackets < MAX_VOICE_PER_PACKET )
                {
                    msg.VoicePackets[ msg.wVoicePackets ].xuidSrc = m_LocalXUIDs[ dwPort ];
                    memcpy( msg.VoicePackets[ msg.wVoicePackets ].byData, rgBuffer, dwBytesWritten );
                    msg.wVoicePackets++;
                }

                // We've set up our voice timer such that it SHOULD cause us to send out
                // our buffered voice data before the buffer fills up.  However, things
                // like framerate glitches, etc., could cause us to fill up before we
                // notice the timer has fired.
                if( msg.wVoicePackets == MAX_VOICE_PER_PACKET )
                    SendVoiceDataToAll();
            }
        } while( SUCCEEDED( hr ) );
    }
}



//--------------------------------------------------------------------------------------
// Name: SystemNotificationsUpdate()
// Desc: Poll for system notifications
//--------------------------------------------------------------------------------------
VOID VideoSample::SystemNotificationsUpdate()
{
    DWORD dwNotificationId;
    ULONG_PTR ulParam;

    if( XNotifyGetNext( m_hNotification, 0, &dwNotificationId, &ulParam ) )
    {
        switch( dwNotificationId )
        {
            case XN_SYS_SIGNINCHANGED:
                // Save off the player mask
                m_SignedInMask = ulParam;

                // Get the local player info
                m_LocalMachine.byNumPlayers = 0;
                for( DWORD i = 0; i < XUSER_MAX_COUNT; i++ )
                {
                    XUSER_SIGNIN_STATE State = XUserGetSigninState( i );

                    if( State != eXUserSigninState_NotSignedIn )
                    {
                        // Check to see if we didn't already have someone signed in
                        // to this slot, and if so, then register them with XHV
                        if( m_LocalXUIDs[i] == INVALID_XUID )
                        {
							if( m_pXHV2Engine->RegisterLocalTalker( i ) == S_OK )
							{
								m_pXHV2Engine->StartLocalProcessingModes( i, &XHV_VOICECHAT_MODE, 1 );
							}
                        }

                        // Retrieve XUID and name for each logged-in user
                        XUserGetXUID( i, &m_LocalXUIDs[i] );

                        m_LocalMachine.Players[ m_LocalMachine.byNumPlayers ].xuid = m_LocalXUIDs[ i ];
                        XUserGetName( i, m_LocalMachine.Players[ m_LocalMachine.byNumPlayers ].strGamertag,
                                      XUSER_NAME_SIZE );
                        ++m_LocalMachine.byNumPlayers;
                    }
                    else
                    {
                        if( m_LocalXUIDs[i] != INVALID_XUID )
                        {
                            m_pXHV2Engine->UnregisterLocalTalker( i );
                        }

                        m_LocalXUIDs[ i ] = INVALID_XUID;
                    }
                }

                //BUGBUG: Notify other machines that the sign-on list changed

                break;

            case XN_SYS_UI:
                m_bSystemUIShowing = static_cast<BOOL>( ulParam );
                break;
        }
    }

    // Check for the first user to sign in to live and optionally show opt-in dialog
    for( DWORD i = 0; i < XUSER_MAX_COUNT; i++ )
    {
        XUSER_SIGNIN_STATE State = XUserGetSigninState( i );

        // Show opt-in dialog once a user has signed in
        if( State == eXUserSigninState_SignedInToLive )
        {
            if( !m_bDialogShown && !m_bSystemUIShowing )
            {
                // Read the user profiles to retrieve the video enabled and dialog shown bits
                // This is a title supplied function
                // ReadUserProfiles( i, &m_DialogResult.bVideoEnabled, &m_DialogResult.bOptInDialogShown );

                // Show opt-in dialog
                DisplayOptInVideoDialog( i, &m_DialogResult, &m_DialogOverlapped );
                m_bDialogShown = TRUE;
            }
        }
    }


    // Check if the opt-in dialog has been closed
    if( !m_bDialogDismissed && m_bDialogShown && XHasOverlappedIoCompleted( &m_DialogOverlapped ) )
    {
        // Title supplied function to write video enabled and dialog shown bits to user profile
        // WriteUserProfiles( dwSignedInUser, &m_DialogResult.bVideoEnabled, &m_DialogResult.bOptInDialogShown );

        m_bDialogDismissed = TRUE;
    }
}


//-----------------------------------------------------------------------------
// Name: SendFindGame()
// Desc: Broadcast a MSG_FIND_GAME from our client to any available host
//-----------------------------------------------------------------------------
VOID VideoSample::SendFindGame()
{
    assert( !m_bIsHost );
    Message msgFindGame( MSG_FIND_GAME );
    MsgFindGame& msg = msgFindGame.GetFindGame();

    // Generate a nonce (random bytes). When a potential host responds with
    // information about a game, he must respond via a broadcast message
    // since a secure session hasn't been established. The broadcast message
    // will contain the same nonce so we can verify that message is really
    // for us. If we receive a broadcast "found game" message with a different
    // nonce, we ignore it, because it was broadcast to a different client
    // than us.
    INT iResult = XNetRandom( ( BYTE* )( &msg.nonce ), sizeof( msg.nonce ) );
    assert( iResult == NO_ERROR );
    ( VOID )iResult;

    // Save the nonce for comparison later
    CopyMemory( &m_Nonce, &msg.nonce, sizeof( msg.nonce ) );

    SOCKADDR_IN saBroad;
    saBroad.sin_family = AF_INET;
    saBroad.sin_addr.s_addr = INADDR_BROADCAST;
    saBroad.sin_port = htons( BROADCAST_PORT );
    INT nBytes = m_BroadSock.SendTo( &msgFindGame, msgFindGame.GetSize(),
                                     &saBroad );
    // This assert was removed because Send no longer is guaranteed to always work
    // If the security association times out, the number of bytes returned will
    // NOT be equal to the size of the message.  A good thing to do here would
    // be to drop the player
    //   assert( nBytes == SOCKET_ERROR || nBytes == msgFindGame.GetSize() );
    ( VOID )nBytes;
}


//-----------------------------------------------------------------------------
// Name: SendGameFound()
// Desc: Broadcast a MSG_GAME_FOUND from our host to the world
//-----------------------------------------------------------------------------
VOID VideoSample::SendGameFound( const Nonce& nonceClient )
{
    assert( m_bIsHost );
    Message msgGameFound( MSG_GAME_FOUND );
    MsgGameFound& msg = msgGameFound.GetGameFound();

    // Resend the nonce that we received from the client so he can verify
    // that this message is really for him
    CopyMemory( &msg.nonce, &nonceClient, sizeof( nonceClient ) );

    // Send information about the session that we're hosting
    CopyMemory( &msg.xnHostKeyID, &m_xnHostKeyID, sizeof( XNKID ) );
    CopyMemory( &msg.xnHostKey, &m_xnHostKeyExchange, sizeof( XNKEY ) );
    CopyMemory( &msg.xnHostAddr, &m_xnTitleAddress, sizeof( XNADDR ) );

    // Send the current information about the game
    DWORD TotalPlayers = 0;
    for( DWORD i = 0; i < m_Machines.size(); ++i )
    {
        TotalPlayers += m_Machines[ i ].dwNumPlayers;
    }
    msg.byNumPlayers = BYTE( TotalPlayers );
    msg.Resolution = m_XCamStreamInitParams.VideoResolution;
    lstrcpynW( msg.strGameName, m_strGameName, MAX_GAME_NAME_LENGTH );

    // We don't have the XNADDR of the requesting client, so we
    // can't send this message directly back. Instead, we broadcast the
    // message to everybody on the net. The requesting client can
    // check the nonce to verify that the response is really for them.
    // Broadcast messages are automatically encrypted.

    SOCKADDR_IN saBroad;
    saBroad.sin_family = AF_INET;
    saBroad.sin_addr.s_addr = INADDR_BROADCAST;
    saBroad.sin_port = htons( BROADCAST_PORT );
    INT nBytes = m_BroadSock.SendTo( &msgGameFound, msgGameFound.GetSize(),
                                     &saBroad );
    // This assert was removed because Send no longer is guaranteed to always work
    // If the security association times out, the number of bytes returned will
    // NOT be equal to the size of the message.  A good thing to do here would
    // be to drop the player

    //assert( nBytes == SOCKET_ERROR || nBytes == msgGameFound.GetSize() );
    ( VOID )nBytes;
}


//-----------------------------------------------------------------------------
// Name: SendJoinGame()
// Desc: Issue a MSG_JOIN_GAME from our client to the game host
//-----------------------------------------------------------------------------
VOID VideoSample::SendJoinGame( const SOCKADDR_IN& saGameHost )
{
    assert( !m_bIsHost );
    Message msgJoinGame( MSG_JOIN_GAME );
    MsgJoinGame& msg = msgJoinGame.GetJoinGame();

    // Include our local player information
    CopyMemory( &msg.NewMachine, &m_LocalMachine, sizeof( Machine ) );

    // Send join game message reliably to the host
    INT nBytes = SendMessage( &msgJoinGame, TRUE, &saGameHost );

    // This assert was removed because Send no longer is guaranteed to always work
    // If the security association times out, the number of bytes returned will
    // NOT be equal to the size of the message.  A good thing to do here would
    // be to drop the player
    //assert( nBytes == SOCKET_ERROR || nBytes == msgJoinGame.GetSize() );
    ( VOID )nBytes;

}


//-----------------------------------------------------------------------------
// Name: SendJoinApproved()
// Desc: Issue a MSG_JOIN_APPROVED from our host to the requesting client.
//-----------------------------------------------------------------------------
VOID VideoSample::SendJoinApproved( const SOCKADDR_IN& saClient )
{
    assert( m_bIsHost );
    Message msgJoinApproved( MSG_JOIN_APPROVED );
    MsgJoinApproved& msg = msgJoinApproved.GetJoinApproved();

    // The host is us
    CopyMemory( &msg.Host, &m_LocalMachine, sizeof( Machine ) );

    // Send the list of all the current players to the new player.
    // We don't send the host player info, since the new player
    // already has all of the information it needs about the host player.

    BYTE i = 0;
    for( MachineList::const_iterator machine = m_Machines.begin();
         machine != m_Machines.end(); ++machine, ++i )
    {
        msg.Machines[ i ].xnAddr = machine->xnAddr;
        msg.Machines[ i ].byNumPlayers = ( BYTE )machine->dwNumPlayers;

        for( DWORD j = 0; j < machine->dwNumPlayers; ++j )
        {
            msg.Machines[ i ].Players[ j ].xuid = machine->Players[ j ].xuid;
            strcpy_s( msg.Machines[ i ].Players[ j ].strGamertag,
                      XUSER_NAME_SIZE,
                      machine->Players[ j ].strGamertag );
        }
    }

    msg.byNumMachines = i;

    // Send the join approved message reliably to the client
    INT nBytes = SendMessage( &msgJoinApproved, TRUE, &saClient );

    // This assert was removed because Send no longer is guaranteed to always work
    // If the security association times out, the number of bytes returned will
    // NOT be equal to the size of the message.  A good thing to do here would
    // be to drop the player

    //assert( nBytes == SOCKET_ERROR || nBytes == msgJoinApproved.GetSize() );
    ( VOID )nBytes;
}


//-----------------------------------------------------------------------------
// Name: SendJoinDenied()
// Desc: Issue a MSG_JOIN_DENIED from our host to the requesting client
//-----------------------------------------------------------------------------
VOID VideoSample::SendJoinDenied( const SOCKADDR_IN& saClient )
{
    assert( m_bIsHost );
    Message msgJoinDenied( MSG_JOIN_DENIED );

    // Send join denied message reliably back to the client
    INT nBytes = SendMessage( &msgJoinDenied, TRUE, &saClient );

    // This assert was removed because Send no longer is guaranteed to always work
    // If the security association times out, the number of bytes returned will
    // NOT be equal to the size of the message.  A good thing to do here would
    // be to drop the player

    //assert( nBytes == SOCKET_ERROR || nBytes == msgJoinDenied.GetSize() );
    ( VOID )nBytes;
}


//-----------------------------------------------------------------------------
// Name: SendVoiceDataToAll
// Desc: Sends accumulated voice data out to other players in the game
//-----------------------------------------------------------------------------
VOID VideoSample::SendVoiceDataToAll()
{
    // Make sure we actually have data to send...
    if( m_msgVoiceData.GetMsgVoiceData().wVoicePackets > 0 )
    {
        // Send voice data via VDP directly to all other players
        INT nBytes = SendMessage( &m_msgVoiceData, FALSE );

        // This assert was removed because Send no longer is guaranteed to always work
        // If the security association times out, the number of bytes returned will
        // NOT be equal to the size of the message.  A good thing to do here would
        // be to drop the player

        //assert( nBytes == SOCKET_ERROR || nBytes == m_msgVoiceData.GetSize() );
        ( VOID )nBytes;
    }

    m_msgVoiceData.GetMsgVoiceData().wVoicePackets = 0;
    m_VoiceTimer.StartZero();
}


//-----------------------------------------------------------------------------
// Name: SendVideoDataToAll
// Desc: Sends all pending video packets out to other players in the game
//-----------------------------------------------------------------------------
VOID VideoSample::SendVideoDataToAll()
{
    // Send video data
    Message msgVideoData( MSG_VIDEODATA );
    MsgVideoData& msg = msgVideoData.GetMsgVideoData();
    SOCKADDR_IN sa;
    sa.sin_family = AF_INET;
    sa.sin_port = htons( VIDEO_PORT );
    DWORD returnValue, bufferSize;
    bool morePackets = true;

    while( morePackets )
    {
        morePackets = false;
        for( MachineList::const_iterator machine = m_Machines.begin();
             machine != m_Machines.end(); ++machine )
        {
            bufferSize = MAX_VIDEO_PACKET_SIZE;
            returnValue = m_XCamStreamEngine->GetNetworkData( machine->xnAddr, msg.byData, &bufferSize );
            switch( returnValue )
            {
                case ERROR_IO_PENDING:
                    break;

                case ERROR_SUCCESS:
                    if( ( m_SinglePacketDrop || m_ContinuousPacketDrop ) &&
                        m_CurrMenuItem == m_Machines.size() )
                    {
                        //drop the packet
                        m_SinglePacketDrop = FALSE;
                    }
                    else
                    {
                        msg.wVideoDataSize = ( WORD )bufferSize;
                        sa.sin_addr = machine->inAddr;
                        if( SOCKET_ERROR == SendMessage( &msgVideoData, FALSE, &sa ) )
                            ATG::DebugSpew( "Socket Error: %d\n", WSAGetLastError() );
                    }   
                    break;
                case  ERROR_INVALID_PARAMETER:
                    ATG::FatalError( "GetNetworkData failed for one of remote consoles!!\n" );
                    break;
                default:
                    morePackets = true;
            }
        }
    }
}


//-----------------------------------------------------------------------------
// Name: SendMessage
// Desc: Handles the logic of actually sending a message out over the network
//       There are two options, each with two possibilites
//       bReliable - If TRUE, send over reliable channel.  This host is
//          responsible for relaying reliable messages between clients.  If
//          FALSE, then send directly via VDP.
//       psaDest - Optional parameter that defaults to NULL.  If a player
//          address is specified, the message is intended for that player only
//
//-----------------------------------------------------------------------------
INT VideoSample::SendMessage( const Message* pMsg,
                              BOOL bReliable,
                              const SOCKADDR_IN* psaDest )
{
    INT nBytes = 0;

    if( bReliable )
    {
        // Reliable messages are sent via TCP connection, and so should
        // only be used for low-bandwidth, low-frequency messages.
        // Consider actively throttling the amount of data sent reliably
        if( m_bIsHost )
        {
            // The host can send directly to one client or directly
            // to all clients, because he's got a reliable connection
            // with every client
            if( psaDest )
            {
                // Send directly to the player over the reliable socket
                MatchInAddr matchInAddr( psaDest->sin_addr );
                SocketList::iterator it = std::find_if( m_ClientSockets.begin(), m_ClientSockets.end(), matchInAddr );
                assert( it != m_ClientSockets.end() );

                nBytes += send( it->sock, ( char* )pMsg, pMsg->GetSize(), 0 );
            }
            else
            {
                // We're the host, so we can just iterate over each of our
                // reliable sockets and send them the message directly
                for( SocketList::iterator it = m_ClientSockets.begin();
                     it < m_ClientSockets.end();
                     ++it )
                {
                    if( it->bAccepted )
                    {
                        nBytes += send( it->sock, ( char* )pMsg, pMsg->GetSize(), 0 );
                    }
                }
            }
        }
        else
        {
            // We're a client - our only reliable connection is to the
            // host, so send him the message and he will forward it if
            // necessary (see ProcessVoiceInfo)
            if( m_ReliableSock.IsOpen() )
                nBytes += m_ReliableSock.Send( pMsg, pMsg->GetSize() );
        }
    }
    else
    {
        // Non-reliable message - these get sent directly via VDP
        // regardless of whether or not we're the host
        if( psaDest )
        {
            // If destined for a specific player, send straight to them
            nBytes += m_DirectSock.SendTo( pMsg, pMsg->GetSize(), psaDest );
        }
        else
        {
            // If destined for everyone, loop over the machines list
            for( MachineList::iterator it = m_Machines.begin(); it != m_Machines.end(); ++it )
            {
                SOCKADDR_IN sa;
                sa.sin_family = AF_INET;
                sa.sin_addr = it->inAddr;
                sa.sin_port = htons( DIRECT_PORT );

                nBytes += m_DirectSock.SendTo( pMsg, pMsg->GetSize(), &sa );
            }
        }
    }

    return nBytes;
}


//-----------------------------------------------------------------------------
// Name: ProcessBroadcastMessage()
// Desc: Checks to see if any broadcast messages are waiting on the broadcast
//       socket. If a message is waiting, it is routed and processed.
//       If no messages are waiting, the function returns immediately.
//       Returns TRUE if a message was processed.
//-----------------------------------------------------------------------------
BOOL VideoSample::ProcessBroadcastMessage()
{
    if( !m_BroadSock.IsOpen() )
        return FALSE;

    // See if a network broadcast message is waiting for us
    Message msg;
    INT iResult = m_BroadSock.Recv( &msg, msg.GetMaxSize() );

    // If message waiting, process it
    if( iResult != SOCKET_ERROR && iResult > 0 )
    {
        // The only messages w/ unencrypted data are VOICEDATA messages,
        // and those should never be sent over broadcast.
        assert( msg.GetUnEncryptedSize() == 0 );

        // There is possibility to recieve illegal packet in systemlink play mode
        if( iResult != msg.GetSize() ) return false;

        // Process the message
        switch( msg.GetId() )
        {
                // From client to host; processed by host
            case MSG_FIND_GAME:
                ProcessFindGame( msg.GetFindGame() );   break;

                // From host to client: processed by client
            case MSG_GAME_FOUND:
                ProcessGameFound( msg.GetGameFound() ); break;

                // Any other message on this port is invalid and we ignore it
            default:
                assert( FALSE ); break;
        }

        return TRUE;
    }
    return FALSE;
}


//-----------------------------------------------------------------------------
// Name: ProcessDirectMessage()
// Desc: Checks to see if any direct messages are waiting on the direct socket.
//       If a message is waiting, it is routed and processed.
//       If no messages are waiting, the function returns immediately.
//       Returns TRUE if a message was processed.
//-----------------------------------------------------------------------------
BOOL VideoSample::ProcessDirectMessage()
{
    if( !m_DirectSock.IsOpen() )
        return FALSE;

    // See if a network message is waiting for us
    Message msg;
    SOCKADDR_IN saFromIn;
    INT iResult;

    // Process until no more messages are available
    do
    {
        iResult = m_DirectSock.RecvFrom( &msg, msg.GetMaxSize(), &saFromIn );
        SOCKADDR_IN saFrom( saFromIn );

        // If message waiting, process it
        if( iResult != SOCKET_ERROR && iResult > 0 )
        {
            assert( iResult == msg.GetSize() );
            ProcessMessage( msg, saFrom );
        }
        else
        {
            assert( WSAGetLastError() == WSAEWOULDBLOCK );
        }
    } while( iResult != SOCKET_ERROR && iResult > 0 );

    return FALSE;
}


//-----------------------------------------------------------------------------
// Name: ProcessReliableMessage()
// Desc: First checks to see if any new connections have been attempted, and if
//       we have room, accepts them.  Then, scans all reliable client
//       connections to see if any have messages pending.
//       If a message is waiting, it is routed and processed.
//       If no messages are waiting, the function returns immediately.
//       Returns TRUE if a message was processed.
//-----------------------------------------------------------------------------
BOOL VideoSample::ProcessReliableMessage()
{
    if( !m_ReliableSock.IsOpen() )
        return FALSE;

    if( m_bIsHost )
    {
        // Process any pending socket connections
        for(; ; )
        {
            ClientSocket cs;
            cs.sock = m_ReliableSock.Accept( &cs.sa );
            if( cs.sock == INVALID_SOCKET )
                break;

            // Initialize the ClientSocket struct - if we don't get a join
            // request on the socket w/in the timeout, we close the connection
            cs.bAccepted = FALSE;
            cs.fTimeout = 0.0f;

            m_ClientSockets.push_back( cs );
        }

        // Poll each of our clients for messages and timeout
        for( SocketList::iterator it = m_ClientSockets.begin();
             it < m_ClientSockets.end();
             ++it )
        {
            if( !it->bAccepted )
            {
                //$TODO                it->fTimeout += m_fElapsedTime;
                if( it->fTimeout > PLAYER_TIMEOUT / 1000.0f )
                {
                    closesocket( it->sock );
                    m_ClientSockets.erase( it );
                    continue;
                }
            }

            // Try to parse out a message from the socket.  If message was
            // completed, process the message
            HRESULT hr = it->msgPending.Read( it->sock );
            if( FAILED( hr ) )
            {
                // Socket has been disconnected
                MatchInAddr matchInAddr( it->sa.sin_addr );
                MachineList::iterator p = std::find_if( m_Machines.begin(), m_Machines.end(), matchInAddr );
                if( p != m_Machines.end() )
                {
                    OnMachineDisconnect( &( *p ) );
                    break;
                }
            }
            else if( S_OK == hr )
            {
                ProcessMessage( it->msgPending.m_msg, SOCKADDR_IN( it->sa ) );
                it->msgPending.Reset();
            }
        }
    }
    else
    {
        HRESULT hr = m_msgPending.Read( m_ReliableSock.GetSocket() );
        if( FAILED( hr ) )
        {
            if( m_Machines.size() > 0 )
            {
                // Must have gotten disconnected from host
                OnMachineDisconnect( &m_Machines[ 0 ] );
            }
        }
        else if( S_OK == hr )
        {
            SOCKADDR_IN sa;
            sa.sin_family = AF_INET;
            sa.sin_addr = m_inHostAddr;
            sa.sin_port = htons( RELIABLE_PORT );
            ProcessMessage( m_msgPending.m_msg, sa );
            m_msgPending.Reset();
        }
    }

    return FALSE;
}


//-----------------------------------------------------------------------------
// Name: ProcessVideoMessage()
// Desc: Checks to see if any video messages are waiting on the video socket.
//       If a message is waiting, it is routed and processed.
//       If no messages are waiting, the function returns immediately.
//       Returns TRUE if a message was processed.
//-----------------------------------------------------------------------------
BOOL VideoSample::ProcessVideoMessage()
{
    if( !m_VideoSock.IsOpen() )
        return FALSE;

    SOCKADDR_IN saFromIn;
    INT iResult;
    VideoPacket* pPacket;

    // Return finished packets to the available packet queue
    VideoPacketList::iterator iter = m_InUseVideoPackets.begin();
    while( iter != m_InUseVideoPackets.end() )
    {
        pPacket = *iter;
        if( XHasOverlappedIoCompleted( &pPacket->xoverlapped ) )
        {
            iter = m_InUseVideoPackets.erase( iter );
            m_AvailableVideoPackets.push_back( pPacket );
        }
        else
        {
            iter++;
        }
    }

    // Process until no more messages are available
    do
    {
        // This should not ever be possible!  64 packets per stream should guarantee
        // enough packets to never run out of available packets!
        assert( !m_AvailableVideoPackets.empty() );

        // See if a network message is waiting for us
        pPacket = m_AvailableVideoPackets.back();

        iResult = m_VideoSock.RecvFrom( &pPacket->message, pPacket->message.GetMaxSize(), &saFromIn );

        // If message waiting, process it
        if( iResult != SOCKET_ERROR && iResult > 0 )
        {
            assert( iResult == pPacket->message.GetSize() );
            assert( pPacket->message.GetId() == MSG_VIDEODATA );

            MatchInAddr matchInAddr( saFromIn );
            MachineList::iterator it = std::find_if( m_Machines.begin(), m_Machines.end(), matchInAddr );
            if( it != m_Machines.end() )
            {
                if( ( m_SinglePacketDrop || m_ContinuousPacketDrop ) &&
                    m_CurrMenuItem != m_Machines.size() )
                {
                    if( &( *it ) == &m_Machines[ m_CurrMenuItem ] )
                    {
                        // Drop the packet
                        m_SinglePacketDrop = FALSE;
                        continue;
                    }
                }

                m_XCamStreamEngine->SubmitNetworkData( it->xnAddr, pPacket->message.GetMsgVideoData().byData,
                                                       pPacket->message.GetMsgVideoData().wVideoDataSize,
                                                       &pPacket->xoverlapped );

                m_AvailableVideoPackets.pop_back();
                m_InUseVideoPackets.push_back( pPacket );
            }
        }
        else
        {
            assert( WSAGetLastError() == WSAEWOULDBLOCK );
        }
    } while( iResult != SOCKET_ERROR && iResult > 0 );

    return FALSE;
}


//-----------------------------------------------------------------------------
// Name: ProcessVoiceData
// Desc: Handles receipt of a voice data packet
//-----------------------------------------------------------------------------
VOID VideoSample::ProcessVoiceData( const MsgVoiceData& msg, const SOCKADDR_IN& saFrom )
{
    OutputDebugString( "Processing incoming voice data\n" );

    for( WORD i = 0; i < msg.wVoicePackets; i++ )
    {
        const VoicePacket* pPacket = &msg.VoicePackets[ i ];
        DWORD packetSize = XHV_VOICECHAT_MODE_PACKET_SIZE;
        m_pXHV2Engine->SubmitIncomingChatData( pPacket->xuidSrc, ( BYTE* )pPacket->byData, &packetSize );
    }
}


//-----------------------------------------------------------------------------
// Name: ProcessMessage()
// Desc: Routes any direct messages
//-----------------------------------------------------------------------------
VOID VideoSample::ProcessMessage( Message& msg, const SOCKADDR_IN& saFrom )
{
    // Process the message
    switch( msg.GetId() )
    {
            // From client to host; processed by host
        case MSG_JOIN_GAME:
            ProcessJoinGame( msg.GetJoinGame(), saFrom ); break;

            // From host to client: processed by client
        case MSG_JOIN_APPROVED:
            ProcessJoinApproved( msg.GetJoinApproved(), saFrom ); break;
        case MSG_JOIN_DENIED:
            ProcessJoinDenied( saFrom ); break;
        case MSG_MACHINE_JOINED:
            ProcessMachineJoined( msg.GetMachineJoined(), saFrom ); break;

            // From player to player: processed by client player
        case MSG_HEARTBEAT:
            ProcessHeartbeat( saFrom ); break;

        case MSG_VOICEDATA:
            ProcessVoiceData( msg.GetMsgVoiceData(), saFrom ); break;

            //    // From player to player, but may have been (or need to be) relayed
            //    // by the host
            //case MSG_VOICEINFO:     ProcessVoiceInfo( msg.GetMsgVoiceInfo(), saFrom ); break;

            // Any other message on this port is invalid and we ignore it
        default:
            assert( FALSE ); break;
    }
}



//-----------------------------------------------------------------------------
// Name: ProcessFindGame()
// Desc: Process the find game message
//-----------------------------------------------------------------------------
VOID VideoSample::ProcessFindGame( const MsgFindGame& findGame )
{
    // If we're not hosting a game, we don't care about receiving "find game"
    // messages. Only hosts respond to "find game" messages
    if( !m_bIsHost )
        return;

    // We're hosting a game
    // Respond with the game information
    SendGameFound( findGame.nonce );
}


//-----------------------------------------------------------------------------
// Name: ProcessGameFound()
// Desc: Process the game found message
//-----------------------------------------------------------------------------
VOID VideoSample::ProcessGameFound( const MsgGameFound& gameFound )
{
    // If we're hosting, we don't care about receiving "game found" messages.
    // Only potential clients care about "game found" messages.
    if( m_bIsHost )
        return;

    // If we didn't send the corresponding "find game" message, we don't
    // care about this particular "game found" message
    if( memcmp( &gameFound.nonce, &m_Nonce, NONCE_BYTES ) != 0 )
        return;

    // Check if the game is already found
    for( DWORD i = 0; i < m_Games.size(); ++i )
    {
        if( memcmp( ( void* )&m_Games[ i ].xnHostKeyID, ( void* )&gameFound.xnHostKeyID, sizeof( XNKID ) ) == 0 )
            return;
    }

    // We found a game!
    // Add it to our list of potential games
    GameInfo gameInfo;
    CopyMemory( &gameInfo.xnHostKeyID, &gameFound.xnHostKeyID, sizeof( XNKID ) );
    CopyMemory( &gameInfo.xnHostKey, &gameFound.xnHostKey, sizeof( XNKEY ) );
    CopyMemory( &gameInfo.xnHostAddr, &gameFound.xnHostAddr, sizeof( XNADDR ) );
    gameInfo.byNumPlayers = gameFound.byNumPlayers;
    gameInfo.Resolution = gameFound.Resolution;
    lstrcpynW( gameInfo.strGameName, gameFound.strGameName, MAX_GAME_NAME_LENGTH );

    m_Games.push_back( gameInfo );
}


//-----------------------------------------------------------------------------
// Name: ProcessJoinGame()
// Desc: Process the join game message
//-----------------------------------------------------------------------------
VOID VideoSample::ProcessJoinGame( const MsgJoinGame& joinGame,
                                   const SOCKADDR_IN& saFrom )
{
    // Only hosts should receive "join game" messages
    assert( m_bIsHost );

    // Find this client in our pool of client connections to mark
    // the socket as accepted
    MatchInAddr matchInAddr( saFrom.sin_addr );
    SocketList::iterator it = std::find_if( m_ClientSockets.begin(), m_ClientSockets.end(), matchInAddr );
    assert( it != m_ClientSockets.end() );

    // A session exists between us (the host) and the client. We can now
    // convert the incoming IP address (saFrom) into a valid XNADDR.
    XNADDR xnAddrClient;
    INT iResult = XNetInAddrToXnAddr( saFrom.sin_addr, &xnAddrClient,
                                      &m_xnHostKeyID );
    if( iResult == SOCKET_ERROR )
    {
        // If the client INADDR can't be converted to an XNADDR, then
        // this client does not have a valid session established, and
        // we ignore the message.
        ATG::FatalError( "Unable to convert INADDR to XNADDR\n" );
    }

    // A player may join if we haven't reached the player limit.
    // In a real game, you would need to "lock" the game during a join
    // or track the number of joins in progress so that if multiple
    // players were attempting to join at the same time, they wouldn't
    // all be granted access and then exceed the player maximum.
    if( m_Machines.size() < MAX_REMOTE_MACHINES )
    {
        Message msgMachineJoined( MSG_MACHINE_JOINED );
        MsgMachineJoined& msg = msgMachineJoined.GetMachineJoined();

        CopyMemory( &msg.NewMachine, &joinGame.NewMachine, sizeof( Machine ) );
        msg.NewMachine.xnAddr = xnAddrClient;

        // Send the player joined message reliably to all players in the game
        SendMessage( &msgMachineJoined, TRUE );

        // We send the approval to the player AFTER we've told
        // everyone else.  This way, he doesn't get a PlayerJoined
        // message for himself
        SendJoinApproved( saFrom );
        it->bAccepted = TRUE;

        OnMachineJoined( joinGame.NewMachine, xnAddrClient, &saFrom.sin_addr );
    }
    else
    {
        SendJoinDenied( saFrom );
        closesocket( it->sock );
        m_ClientSockets.erase( it );
    }
}



//-----------------------------------------------------------------------------
// Name: ProcessJoinApproved()
// Desc: Process the join approved message
//-----------------------------------------------------------------------------
VOID VideoSample::ProcessJoinApproved( const MsgJoinApproved& joinApproved,
                                       const SOCKADDR_IN& saFrom )
{
    // If for some reason we receive a "join approved" message and we're hosting
    // a game, ignore the message. Only clients handle this message
    if( m_bIsHost )
        return;

    // Add the host
    OnMachineJoined( joinApproved.Host, joinApproved.Host.xnAddr, &saFrom.sin_addr );

    // Build the list of the other players
    for( BYTE i = 0; i < joinApproved.byNumMachines; ++i )
    {
        OnMachineJoined( joinApproved.Machines[ i ],
                         joinApproved.Machines[ i ].xnAddr,
                         NULL );
    }

    // Enter into the game UI
    m_State = STATE_GAME;
    m_CurrMenuItem = 0;
    m_HeartbeatTimer.StartZero();

    StartVoice();
}


//-----------------------------------------------------------------------------
// Name: ProcessJoinDenied()
// Desc: Process the join denied message
//-----------------------------------------------------------------------------
VOID VideoSample::ProcessJoinDenied( const SOCKADDR_IN& )
{
    // If for some reason we receive a "join denied" message and we're hosting
    // a game, ignore the message. Only clients handle this message
    if( m_bIsHost )
        return;

    // If for some reason we receive a "join denied" message and we're
    // already playing a game, ignore the message.
    if( m_State == STATE_GAME )
        return;

    wcscpy_s( m_strError, L"The game is full !!" );
    m_State = STATE_ERROR;
}


//----------------------------------------------------------------------------
// Name: ProcessPlayerJoined()
// Desc: Process the player joined message
//-----------------------------------------------------------------------------
VOID VideoSample::ProcessMachineJoined( const MsgMachineJoined& machineJoined,
                                        const SOCKADDR_IN& saFrom )
{
    // saFrom is the address of the host that sent this message, but we
    // we already have his address, so throw it away
    ( VOID )saFrom;

    const Machine& machine = machineJoined.NewMachine;

    OnMachineJoined( machine, machine.xnAddr, NULL );
}


//-----------------------------------------------------------------------------
// Name: ProcessHeartbeat()
// Desc: Process the heartbeat message
//-----------------------------------------------------------------------------
VOID VideoSample::ProcessHeartbeat( const SOCKADDR_IN& saFrom )
{
    MatchInAddr matchInAddr( saFrom );

    MachineList::iterator it = std::find_if( m_Machines.begin(), m_Machines.end(), matchInAddr );
    if( it != m_Machines.end() )
        it->dwLastHeartbeat = GetTickCount();
}


//-----------------------------------------------------------------------------
// Name: OnMachineJoined
// Desc: Called whenever we've detected a remote machine disconnect
//-----------------------------------------------------------------------------
HRESULT VideoSample::OnMachineJoined( const Machine& pMachine, XNADDR xnAddr, const in_addr* pinAddr )
{
    MachineInfo machineInfo;

    machineInfo.xnAddr = xnAddr;
    machineInfo.dwLastHeartbeat = GetTickCount();
    machineInfo.bHasVideo = 0;
    machineInfo.dwNumPlayers = pMachine.byNumPlayers;
    for( DWORD i = 0; i < machineInfo.dwNumPlayers; ++i )
    {
        strcpy_s( machineInfo.Players[ i ].strGamertag, XUSER_NAME_SIZE, pMachine.Players[ i ].strGamertag );
        machineInfo.Players[ i ].xuid = pMachine.Players[ i ].xuid;
        machineInfo.Players[ i ].bHasVoice = 0;
        machineInfo.Players[ i ].bMuted = 0;
        machineInfo.Players[ i ].bRemoteMuted = 0;
    }

    if( pinAddr != NULL )
    {
        machineInfo.inAddr = *pinAddr;
    }
    else
    {
        // Need to convert XNADDR to in_addr
        INT iResult = XNetXnAddrToInAddr( &machineInfo.xnAddr,
                                          &m_xnHostKeyID,
                                          &machineInfo.inAddr );
        if( iResult == SOCKET_ERROR )
        {
            ATG::FatalError( "Unable to convert client XNADDR into an INADDR\n" );
        }
    }

    // Add the new player to our list
    m_Machines.push_back( machineInfo );

    XUID RemoteXUIDs[ 4 ] = { 0 };
    for( DWORD i = 0; i < machineInfo.dwNumPlayers; ++i )
    {
        // Register the new player with XHV
        if( m_pXHV2Engine->RegisterRemoteTalker( machineInfo.Players[ i ].xuid, NULL, NULL, NULL )  != S_OK )
        {
            ATG::FatalError( "Unable to register remote talker with XHV!\n" );
        }
        m_pXHV2Engine->StartRemoteProcessingModes( machineInfo.Players[ i ].xuid,
                                                 ( PXHV_PROCESSING_MODE )&XHV_VOICECHAT_MODE, 1 );

        RemoteXUIDs[ i ] = machineInfo.Players[ i ].xuid;
    }

    DWORD dwRet = m_XCamStreamEngine->RegisterRemoteConsole( machineInfo.xnAddr, RemoteXUIDs, machineInfo.dwNumPlayers,
                                                             TRUE, TRUE );
    if( dwRet == ERROR_SUCCESS )
    {
        // Remote video stream successfully registered/updated
    }
    else if( dwRet == ERROR_ACCESS_DENIED )
    {
        ATG::DebugSpew( "Parental controls prohibit communication with that remote console\n" );
        // Parental controls prohibit communication with this machine
    }
    else
    {
        ATG::FatalError( "Unable to register remote console with XCam stream engine!\n" );
    }

    RecalculateDisplayPositions();

    return S_OK;
}


//-----------------------------------------------------------------------------
// Name: OnMachineDisconnect
// Desc: Called whenever we've detected a remote machine disconnect
//-----------------------------------------------------------------------------
HRESULT VideoSample::OnMachineDisconnect( MachineInfo* pMachine )
{
    // Update status message to reflect the newly departed player

    MatchInAddr matchInAddr( pMachine->inAddr );

    // The host needs to close down the reliable channel
    if( m_bIsHost )
    {
        // Find the matching entry in list of reliable sockets
        SocketList::iterator it = std::find_if( m_ClientSockets.begin(), m_ClientSockets.end(), matchInAddr );
        if( it != m_ClientSockets.end() )
        {
            // Close the socket
            closesocket( it->sock );
            m_ClientSockets.erase( it );
        }
    }

    // Unregister the players from this address from the voice engine
    for( DWORD i = 0; i < pMachine->dwNumPlayers; ++i )
    {
        m_pXHV2Engine->UnregisterRemoteTalker( pMachine->Players[ i ].xuid );
    }

    // Unregister the address from the video engine
    m_XCamStreamEngine->UnregisterRemoteConsole( pMachine->xnAddr );

    MachineList::iterator it = std::find_if( m_Machines.begin(), m_Machines.end(), matchInAddr );
    assert( it != m_Machines.end() );
    m_Machines.erase( it );
    m_CurrMenuItem = 0;

    RecalculateDisplayPositions();

    if( pMachine->inAddr.S_un.S_addr == m_inHostAddr.S_un.S_addr )
    {
        // TODO: Since we don't have a reliable channel without the host,
        // and don't support host migration, we should end the game here
        assert( !m_bIsHost );
        // Host left the session, thus returning to the menu
        m_XCamStreamEngine->Release();
        wcscpy_s( m_strError, L"Host left the session !!" );
        m_State = STATE_ERROR;
        m_ReliableSock.Close();
        m_inHostAddr.s_addr = 0;
    }

    return S_OK;
}


//-----------------------------------------------------------------------------
// Name: ProcessPlayersDropouts()
// Desc: Process players and determine if anybody has left the game
//-----------------------------------------------------------------------------
VOID VideoSample::ProcessMachineDropouts()
{
    DWORD dwTickCount = GetTickCount();
    for( DWORD i = 0; i < m_Machines.size(); ++i )
    {
        DWORD dwElapsed = dwTickCount - m_Machines[ i ].dwLastHeartbeat;
        if( dwElapsed > PLAYER_TIMEOUT )
        {
            if( m_CurrMenuItem > i )
            {
                m_CurrMenuItem -= 1;
            }
            else if( m_CurrMenuItem == i && m_FullscreenMode )
            {
                m_FullscreenMode = FALSE;
                m_CurrMenuItem = 0;
            }

            OnMachineDisconnect( &m_Machines[ i ] );
            break;
        }
    }
}


//-----------------------------------------------------------------------------
// Name: RecalculateDisplayPositions()
// Desc: Shift the remote console windows around the screen to balance them
//-----------------------------------------------------------------------------
VOID VideoSample::RecalculateDisplayPositions()
{
    D3DRECT rect;
    D3DDISPLAYMODE mode;
    m_pd3dDevice->GetDisplayMode( 0, &mode );
    FLOAT AspectRatio = GetAspectRatio( m_XCamStreamInitParams.VideoResolution );

    if( m_FullscreenMode )
    {
        rect.y1 = 0;
        rect.y2 = mode.Height;
        rect.x1 = ( LONG )( mode.Width / 2 - AspectRatio * mode.Height / 2 );
        rect.x2 = ( LONG )( mode.Width / 2 + AspectRatio * mode.Height / 2 );

        for( DWORD i = 0; i < m_Machines.size(); ++i )
        {
            m_XCamStreamEngine->SetRemoteConsoleRect( m_Machines[ i ].xnAddr, &rect );
        }

        // Flip the preview window
        DWORD temp = rect.x1;
        rect.x1 = rect.x2;
        rect.x2 = temp;
        m_XCamStreamEngine->SetLocalPreviewRect( &rect );

    }
    else
    {
        rect.x1 = ( LONG )( mode.Width * LOCALPREVIEW_X1 );
        rect.x2 = ( LONG )( mode.Width * LOCALPREVIEW_X2 );
        rect.y2 = ( LONG )( LOCALPREVIEW_Y2 * mode.Height );
        rect.y1 = rect.y2 - ( LONG )( LOCALPREVIEW_Y1 * ( mode.Width / AspectRatio ) );
        m_XCamStreamEngine->SetLocalPreviewRect( &rect );

        rect.y1 = ( LONG )( REMOTE_Y1 * mode.Height );
        rect.y2 = rect.y1 + ( LONG )( REMOTE_Y2 * ( mode.Width / AspectRatio ) );

        FLOAT fPositions[][ 6 ] =
        {
            { REMOTE0_X1_1, REMOTE0_X2_1, 0.f, 0.f, 0.f, 0.f },
            { REMOTE0_X1_2, REMOTE0_X2_2, REMOTE1_X1_2, REMOTE1_X2_2, 0.f, 0.f },
            { REMOTE0_X1_3, REMOTE0_X2_3, REMOTE1_X1_3, REMOTE1_X2_3, REMOTE2_X1_3, REMOTE2_X2_3 },
        };

        assert( m_Machines.size() <= 3 );
        for( UINT i = 0; i < m_Machines.size(); ++i )
        {
            rect.x1 = ( LONG )( mode.Width * fPositions[ m_Machines.size() - 1 ][ i * 2 ] );
            rect.x2 = ( LONG )( mode.Width * fPositions[ m_Machines.size() - 1 ][ i * 2 + 1 ] );
            m_XCamStreamEngine->SetRemoteConsoleRect( m_Machines[ i ].xnAddr, &rect );
        }
    }
}


//-----------------------------------------------------------------------------
// Name: GenRandom()
// Desc: Generate a random name
//-----------------------------------------------------------------------------
VOID VideoSample::GenRandom( WCHAR* strName, DWORD dwSize ) // static
{
    // Name consists of two to five parts.
    //
    // 1) consonant or consonant group (e.g. th, qu, st) [optional]
    // 2) vowel or vowel group (e.g. ea, ee, au)
    // 3) consonant or consonant group
    // 4) vowel or vowel group [optional]
    // 5) consonant or consonant group [optional]

    WCHAR strRandom[ 128 ];
    strRandom[ 0 ] = 0;
    if( ( rand() % 2 == 0 ) )
        AppendConsonant( strRandom, TRUE );
    AppendVowel( strRandom );
    AppendConsonant( strRandom, FALSE );
    if( ( rand() % 2 == 0 ) )
    {
        AppendVowel( strRandom );
        if( ( rand() % 2 == 0 ) )
            AppendConsonant( strRandom, FALSE );
    }

    *strRandom = towupper( *strRandom );
    lstrcpynW( strName, strRandom, dwSize );
}




//-----------------------------------------------------------------------------
// Name: GetRandVowel()
// Desc: Get a random vowel
//-----------------------------------------------------------------------------
WCHAR VideoSample::GetRandVowel() // static
{
    for(; ; )
    {
        WCHAR c = WCHAR( L'a' + ( rand() % 26 ) );
        if( wcschr( L"aeiou", c ) != NULL )
            return c;
    }
}


//-----------------------------------------------------------------------------
// Name: GetRandConsonant()
// Desc: Get a random consonant
//-----------------------------------------------------------------------------
WCHAR VideoSample::GetRandConsonant() // static
{
    for(; ; )
    {
        WCHAR c = WCHAR( L'a' + ( rand() % 26 ) );
        if( wcschr( L"aeiou", c ) == NULL )
            return c;
    }
}


//-----------------------------------------------------------------------------
// Name: AppendConsonant()
// Desc: Append consonant or consonant group to string
//-----------------------------------------------------------------------------
VOID VideoSample::AppendConsonant( WCHAR* strRandom, BOOL bLeading ) // static
{
    if( ( rand() % 2 == 0 ) )
    {
        WCHAR strChar[ 2 ] = { GetRandConsonant(), 0 };
        lstrcatW( strRandom, strChar );
    }
    else
    {
        WCHAR* strLeadConGroup[32] =
        {
            L"bl", L"br", L"cl", L"cr", L"dr", L"fl", L"fr", L"gh", L"gl", L"gn",
            L"gr", L"kl", L"kn", L"kr", L"ph", L"pl", L"pr", L"ps", L"qu", L"sc",
            L"sk", L"sl", L"sn", L"sp", L"st", L"sw", L"th", L"tr", L"vh", L"vl",
            L"wh", L"zh"
        };
        WCHAR* strTrailConGroup[32] =
        {
            L"rt", L"ng", L"bs", L"cs", L"ds", L"gs", L"hs", L"sh", L"ss", L"ks",
            L"ms", L"ns", L"ps", L"rs", L"ts", L"gh", L"ph", L"sk", L"st", L"tt",
            L"nd", L"nk", L"nt", L"nx", L"pp", L"rd", L"rg", L"rk", L"rn", L"rv",
            L"th", L"ys"
        };
        if( bLeading )
            lstrcatW( strRandom, strLeadConGroup[ rand() % 32 ] );
        else
            lstrcatW( strRandom, strTrailConGroup[ rand() % 32 ] );
    }
}




//-----------------------------------------------------------------------------
// Name: AppendVowel()
// Desc: Append vowel or vowel group to string
//-----------------------------------------------------------------------------
VOID VideoSample::AppendVowel( WCHAR* strRandom ) // static
{
    if( ( rand() % 2 == 0 ) )
    {
        WCHAR strChar[ 2 ] =
        {
            GetRandVowel(), 0
        };
        lstrcatW( strRandom, strChar );
    }
    else
    {
        WCHAR* strVowelGroup[10] =
        {
            L"ai", L"au", L"ay", L"ea", L"ee", L"ie", L"oa", L"oi", L"oo", L"ou"
        };
        lstrcatW( strRandom, strVowelGroup[ rand() % 10 ] );
    }
}


BOOL VideoSample::IsUSBCameraConnected()
{
    if( XCamGetStatus() == XCAMDEVICESTATE_DISCONNECTED )
        return FALSE;
    else
        return TRUE;
}

BOOL VideoSample::IsKinectCameraConnected()
{
    if( ( XNuiGetHardwareStatus() & XNUI_HARDWARE_STATUS_CONNECTED ) ) 
        return TRUE;
    else
        return FALSE; 
}

VOID VideoSample::ResizeUYVYToYUY2( BYTE* pDst, XCAMRESOLUTION dstResolution, BYTE* pSrc, XCAMRESOLUTION srcResolution)
{
    if( ( XCAMRESOLUTION_640x480 != srcResolution ) ||  
        ( ( XCAMRESOLUTION_160x120 != dstResolution ) &&
          ( XCAMRESOLUTION_320x240 != dstResolution ) &&
          ( XCAMRESOLUTION_640x480 != dstResolution ) ) )
    {
        ATG::FatalError( "The VideoChat sample doesn't supports this resolution.\n" );
    }

    if(srcResolution == dstResolution)
    {
        // The source and destination are both in the same 640x480 resolution, 
        // so no resizing is needed. We just need to reorder the data from UYVY 
        // to YUY2 format, which can be done by swapping every data byte with 
        // its following byte for each pixel while copying to destination buffer.
        for( DWORD i = 0; i < 640 * 480; ++i )
        {
            *pDst++ = *(pSrc+1);
            *pDst++ = *pSrc;
            pSrc += 2;
        }
    }
    else if ( XCAMRESOLUTION_320x240 == dstResolution )
    {
        // Decimate the source image to half width and height using a simple 2x2 avergae filter.
        for( DWORD row = 0; row < 480; row += 2 )
        {
            // Source buffer stride is 640*2 since UYVY uses 16 bits per pixel
            BYTE* srcRow1 = &pSrc[ row * 640 * 2 ];
            BYTE* srcRow2 = &pSrc[ (row+1) * 640 * 2 ];

            // Destination buffer stride is 320*2 since YUY2 uses 16 bits per pixel as well
            BYTE* dst = &pDst[ 320 * (row/2) * 2];
            
            for( DWORD col = 0; col < 640; col += 4 )
            {
                // Read the Y, U, and V samples in UYVY order, and write the averaged samples in YUYV order
                dst[ 0 ] = ( srcRow1[ 1 ] + srcRow1[ 3 ] + srcRow2[ 1 ] + srcRow2[ 3 ] ) >> 2;
                dst[ 2 ] = ( srcRow1[ 5 ] + srcRow1[ 7 ] + srcRow2[ 5 ] + srcRow2[ 7 ] ) >> 2;

                dst[ 1 ] = ( srcRow1[ 0 ] + srcRow1[ 4 ] + srcRow2[ 0 ] + srcRow2[ 4 ] ) >> 2;
                dst[ 3 ] = ( srcRow1[ 2 ] + srcRow1[ 6 ] + srcRow2[ 2 ] + srcRow2[ 6 ] ) >> 2;

                dst += 4;
                srcRow1 += 8;
                srcRow2 += 8;
            }
        }
    }
    else if ( XCAMRESOLUTION_160x120 == dstResolution )
    {
        // Decimate the source buffer to quarter width and height using a simple 4x4 avergae filter.
        for( DWORD row = 0; row < 480; row += 4 )
        {
            // Source buffer stride is 640*2 since UYVY uses 16 bits per pixel
            BYTE* srcRow1 = &pSrc[ row * 640 * 2 ];
            BYTE* srcRow2 = &pSrc[ (row+1) * 640 * 2 ];
            BYTE* srcRow3 = &pSrc[ (row+2) * 640 * 2 ];
            BYTE* srcRow4 = &pSrc[ (row+3) * 640 * 2 ];
            
            // Destination buffer stride is 320*2 since YUY2 uses 16 bits per pixel as well
            BYTE* dst = &pDst[160 * (row/4) * 2];
            
            for( DWORD col = 0; col < 640; col += 8 )
            {
                // Read the Y, U, and V samples in UYVY order, and write the averaged samples in YUYV order
                dst[ 0 ] = ( ( srcRow1[ 1 ] + srcRow1[ 3 ] + srcRow1[ 5 ] + srcRow1[ 7 ] ) + 
                             ( srcRow2[ 1 ] + srcRow2[ 3 ] + srcRow2[ 5 ] + srcRow2[ 7 ] ) + 
                             ( srcRow3[ 1 ] + srcRow3[ 3 ] + srcRow3[ 5 ] + srcRow3[ 7 ] ) + 
                             ( srcRow4[ 1 ] + srcRow4[ 3 ] + srcRow4[ 5 ] + srcRow4[ 7 ] ) ) >> 4;
                
                dst[ 2 ] = ( ( srcRow1[ 9 ] + srcRow1[ 11 ] + srcRow1[ 13 ] + srcRow1[ 15 ] ) + 
                             ( srcRow2[ 9 ] + srcRow2[ 11 ] + srcRow2[ 13 ] + srcRow2[ 15 ] ) + 
                             ( srcRow3[ 9 ] + srcRow3[ 11 ] + srcRow3[ 13 ] + srcRow3[ 15 ] ) + 
                             ( srcRow4[ 9 ] + srcRow4[ 11 ] + srcRow4[ 13 ] + srcRow4[ 15 ] ) ) >> 4; 

                dst[ 1 ] = ( ( srcRow1[ 0 ] + srcRow1[ 4 ] + srcRow1[ 8 ] + srcRow1[ 12 ] ) + 
                             ( srcRow2[ 0 ] + srcRow2[ 4 ] + srcRow2[ 8 ] + srcRow2[ 12 ] ) + 
                             ( srcRow3[ 0 ] + srcRow3[ 4 ] + srcRow3[ 8 ] + srcRow3[ 12 ] ) + 
                             ( srcRow4[ 0 ] + srcRow4[ 4 ] + srcRow4[ 8 ] + srcRow4[ 12 ] ) ) >> 4; 
                           
                          
                dst[ 3 ] = ( ( srcRow1[ 2 ] + srcRow1[ 6 ] + srcRow1[ 10 ] + srcRow1[ 14 ] ) + 
                             ( srcRow2[ 2 ] + srcRow2[ 6 ] + srcRow2[ 10 ] + srcRow2[ 14 ] ) + 
                             ( srcRow3[ 2 ] + srcRow3[ 6 ] + srcRow3[ 10 ] + srcRow3[ 14 ] ) + 
                             ( srcRow4[ 2 ] + srcRow4[ 6 ] + srcRow4[ 10 ] + srcRow4[ 14 ] ) ) >> 4;

                dst += 4;
                srcRow1 += 16;
                srcRow2 += 16;
                srcRow3 += 16;
                srcRow4 += 16;
            }
        }
    }
}


//-----------------------------------------------------------------------------
// Name: ReadFromSocket
// Desc: Attempts to read a message from the specified socket.  Since our
//          reliable sockets are stream-oriented, messages may be received
//          in small pieces (or many received at once), so it's important
//          to carefully parse the appropriate amount of data from the stream.
//          Returns TRUE if message is completely parsed
//-----------------------------------------------------------------------------
HRESULT PendingMessage::Read( SOCKET sock )
{
    // 1) The first thing we need is to complete the header - that way
    // we know how large the message is.  If we don't have any data, or haven't
    // completed the header, just ask for enough data to complete the header
    if( m_nBytesReceived < Message::GetHeaderSize() )
    {
        CHAR* pbReceive = ( ( CHAR* )&m_msg ) + m_nBytesReceived;
        INT nBytesReq = Message::GetHeaderSize() - m_nBytesReceived;
        INT nBytes = recv( sock, pbReceive, nBytesReq, 0 );

        // Check result
        if( nBytes == SOCKET_ERROR )
        {
            if( WSAGetLastError() != WSAEWOULDBLOCK )
                return E_FAIL;
        }
        else
        {
            m_nBytesReceived += nBytes;
        }
    }

    // If we have a complete header, but haven't yet finished parsing the
    // message payload, ask for just enough data to complete the payload
    if( m_nBytesReceived >= Message::GetHeaderSize() &&
        m_nBytesReceived < m_msg.GetSize() )
    {
        CHAR* pbReceive = ( ( CHAR* )&m_msg ) + m_nBytesReceived;
        INT nBytesReq = m_msg.GetSize() - m_nBytesReceived;
        INT nBytes = recv( sock, pbReceive, nBytesReq, 0 );

        // Check result
        if( nBytes == SOCKET_ERROR )
        {
            if( WSAGetLastError() != WSAEWOULDBLOCK )
                return E_FAIL;
        }
        else
            m_nBytesReceived += nBytes;
    }

    // Determine if we now have a complete message - note that we still have
    // to verify we have at least the header before asking for the size of the
    // message
    if( m_nBytesReceived >= Message::GetHeaderSize() &&
        m_nBytesReceived == m_msg.GetSize() )
        return S_OK;
    else
        return S_FALSE;
}