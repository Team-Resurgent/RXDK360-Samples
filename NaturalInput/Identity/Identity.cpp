//--------------------------------------------------------------------------------------
// Identity.cpp
//
// XNA Developer Connection.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include <xtl.h>
#include <xnamath.h>
#include <AtgApp.h>
#include <AtgDebugDraw.h>
#include <AtgFont.h>
#include <AtgHelp.h>
#include <AtgInput.h>
#include <AtgLockFreePipe.h>
#include <AtgMesh.h>
#include <AtgResource.h>
#include <AtgUtil.h>
#include <AtgSimpleShaders.h>
#include <AtgNuiVisualization.h>
#include <AtgNuiCommon.h>

#include "Gestures.h"

// Disable CodeAnalysis stack usage warning:
// warning C6262: Function uses '43908' bytes of stack: exceeds /analyze:stacksize'32768'. Consider moving some data to heap
#pragma warning(disable : 6262)
 
//--------------------------------------------------------------------------------------
// Callouts for labeling the gamepad on the help screen
//--------------------------------------------------------------------------------------
ATG::HELP_CALLOUT g_HelpCallouts[] =
{
    { ATG::HELP_A_BUTTON, ATG::HELP_PLACEMENT_1, L"Toggle Gesture Visual Aids." },
    { ATG::HELP_BOTTOM_CENTER, ATG::HELP_PLACEMENT_1, L"See this sample's documentation for a list of recognized gestures." }
};
#define NUM_HELP_CALLOUTS (sizeof(g_HelpCallouts) / sizeof(g_HelpCallouts[0]))
 
 
//--------------------------------------------------------------------------------------
// Name: struct SKELETON_SLOT_INFO
// Desc: Keeps track of the skeletons associated to the in-sample available slots
//       (up to two tracked skeletons supported)
//--------------------------------------------------------------------------------------
struct SKELETON_SLOT_INFO
{
    BOOL     bIsInUse;
    DWORD    dwSkeletonIndex;
};

const DWORD SKELETON_SLOT_COUNT = 2;


//--------------------------------------------------------------------------------------
// Name: struct SKELETON_IDENTITY_INFO
// Desc: Keeps track of sample specific data related to a skeleton's identity
//--------------------------------------------------------------------------------------
struct SKELETON_IDENTITY_INFO
{
    BOOL     bIsIdentifying; // TRUE if the player is tracked but not identified.
    BOOL     bIsEnrolling;   // TRUE if the player is identified but not enrolled.

    DWORD    dwQualityFlags; // Quality flags as returned by identify and enroll functions
    HRESULT  hrLastResult;   // Last return code from identify and enroll functions

    BOOL     bIsPlaying;     // TRUE if the player is drawing circles in the air!
};

 
//--------------------------------------------------------------------------------------
// Name: struct PLAYER_INFO
// Desc: Keeps track of current and past player specific information
//--------------------------------------------------------------------------------------
struct PLAYER_INFO
{
    WCHAR    wszPlayerName[ XUSER_NAME_SIZE ];
    WCHAR    wszGamerTag[ XUSER_NAME_SIZE ];
    D3DCOLOR Color;

    BOOL   bHasShownIntentToPlay; 
    DWORD  dwAirCirclesCount;     // Current count
    DWORD  dwAirCirclesTotal;     // Total air circles completed across all enrollment sessions for this player

    DWORD  dwEnrollmentCount;     // Number of times this player enrolled into the sample.
    DOUBLE fInnactiveSince;       // Absolute time in seconds since player was last active
};


//--------------------------------------------------------------------------------------
// Name: enum PLAYER_REQUEST
// Desc:
//--------------------------------------------------------------------------------------
enum PLAYER_REQUEST{ PLAYER_NOREQUEST, PLAYER_NEEDIDENTIFY, PLAYER_WANTENROLL, PLAYER_WANTREENROLL };


//--------------------------------------------------------------------------------------
// Name: struct PLAYER_REQUEST_MSG
// Desc:
//--------------------------------------------------------------------------------------
struct PLAYER_REQUEST_MSG
{ 
    PLAYER_REQUEST eRequest;
    DWORD          dwData;
};


//--------------------------------------------------------------------------------------
// Name: enum IDENTIY_CALLBACK_REASON
// Desc:
//--------------------------------------------------------------------------------------
enum IDENTITY_CALLBACK_REASON{ IDENTITY_CALLBACK_IDENTIFY, IDENTITY_CALLBACK_ENROLL };


//--------------------------------------------------------------------------------------
// Name: struct IDENTITY_CALLBACK_MESSAGE
// Desc:
//--------------------------------------------------------------------------------------
struct IDENTITY_CALLBACK_MESSAGE
{ 
    IDENTITY_CALLBACK_REASON eReason;
    NUI_IDENTITY_MESSAGE     Message;
};


//Note:  We are using the ATG::LockFreePipe as a conduit to forward messages sent to the sample 
//       ( through a callback and on a different thread ) by the NuiIdentity API during the identification 
//       and enrollment process.
//
//       Titles should use the most appropriate method to handle multi-threading programming in 
//       their games. The LockFreePipe is only one of many options available to developers. 
const BYTE IDENTITY_MESSAGE_PIPE_SIZE = 11; // Buffer size is 1 << 11 = 2048 bytes
typedef ATG::LockFreePipe< IDENTITY_MESSAGE_PIPE_SIZE > IdentityMessagePipe;

const DOUBLE GESTURE_BLACKOUT_DELAY = 3; // Time in seconds that a gesture blackout will be effective.

//--------------------------------------------------------------------------------------
// Name: class Sample
// Desc: Main class to run this application.
//--------------------------------------------------------------------------------------
class Sample : public ATG::Application
{
    ATG::Timer m_Timer;
    ATG::Font  m_Font;
    ATG::Help  m_Help;

    BOOL m_bDrawHelp;               // TRUE if the help screen is displayed.
    BOOL m_bDrawHistoryUI;          // TRUE if the History screen is displayed.
    BOOL m_bDrawCorrectYourIdUI;    // TRUE if the Correct Your ID screen is displayed.
    BOOL m_bDrawPauseMenuUI;        // TRUE if the Pause Menu is displayed.
    BOOL m_bShowGestureGuides;      // TRUE if user wants gesture guides displayed.
    
    ATG::Timer m_GestureBlackoutTimer; // Time elapsed since latest gesture blackout request.

    BOOL   m_bSystemUIShowing;      // TRUE if a system UI is displayed.
    HANDLE m_hNotification;

    IdentityMessagePipe m_MessagePipe; // The pipe used to forward Identity nofications to the main thread.

    // Gesture recognition objects used by this sample.
    FlickLeftGesture     m_FlickLeftGesture[ NUI_SKELETON_COUNT ];
    FlickRightGesture    m_FlickRightGesture[ NUI_SKELETON_COUNT ];
    ListSelectionGesture m_CorrectYourIdOptionList[ NUI_SKELETON_COUNT ];
    ListSelectionGesture m_PauseMenuOptionList[ NUI_SKELETON_COUNT ];
    StopGesture          m_StopGesture[ NUI_SKELETON_COUNT ];
    WaveGesture          m_WaveGesture[ NUI_SKELETON_COUNT ];
    AirCirclesGesture    m_AirCirclesGesture[ NUI_SKELETON_COUNT ];

    NUI_SKELETON_FRAME     m_SkeletonFrame; // Contains the last data from the skeleton pipeline
                                            // as retrieved by NuiSkeletonGetNextFrame().

    // Picture in picture feature
    HANDLE                 m_hImage;        // Handle to color stream
    const NUI_IMAGE_FRAME* m_pImageFrame;   // Pointer to a color buffer owned by NUI
    ATG::NuiVisualization  m_pip;           // Picture in picture object.

 
    SKELETON_SLOT_INFO     m_SkeletonSlotInfo[ SKELETON_SLOT_COUNT ];         // Up to two skeletons can be tracked at once.
    SKELETON_IDENTITY_INFO m_SkeletonIdentityInfo[ NUI_SKELETON_COUNT ];      // Extended skeleton data specific to this sample.
    PLAYER_INFO            m_PlayerInfo[ NUI_IDENTITY_MAX_ENROLLMENT_COUNT ]; // Preserves player data even when not playing.

    DWORD m_adwAsyncOpTrackingID[ NUI_SKELETON_COUNT ]; // Associates a skeleton index with the tracking ID used for an Identify or Enroll operation.
                                                        // If the tracking is lost during an async operation, this will become the only way to 
                                                        // associate the dwTrackingID in the completion message to the skeleton index the operation was launched for.
                                                        // The dwTrackingID that is part of the NUI_SKELETON_DATA structure will become invalid once tracking is lost.

    PLAYER_REQUEST_MSG m_PlayerRequest[ NUI_SKELETON_COUNT ];

    // Identifies the player who controls a specific UI. Not all UI uses this, 
    // most will let both players issue commands
    DWORD m_dwPauseMenuSkeletonInControl; 
    DWORD m_dwCorrectYourIdSkeletonInControl;

    DWORD m_dwTotalPlayerCount; // Total number of players enrolled since the beginning of the session.

    // Data for scaling depth image to screen space.
    FLOAT m_fDepthDisplayScaleX;
    FLOAT m_fDepthDisplayScaleY;

    // Confine all drawing and text to the TV's safe area
    D3DRECT m_SafeArea;

    // Keep latest player message a minimum amount of time on screen.
    ATG::Timer m_MessageTimer[ NUI_SKELETON_COUNT ];
 
private:
       
    HRESULT Initialize();
    HRESULT StartupCamera();
    VOID ResetSkeletonIdentityInfo( DWORD dwIndex );
    VOID ResetPlayerInfo( DWORD dwIndex );
    VOID ResetSkeletonSlotInfo( DWORD dwIndex );

    HRESULT Update();
    VOID ProcessNotifications();
    VOID ProcessFrameProcessedMessage( const NUI_IDENTITY_MESSAGE* pIdentityMessage );
    VOID ProcessIdentificationCompletedMessage( const NUI_IDENTITY_MESSAGE* pIdentityMessage );
    VOID ProcessEnrollmentCompletedMessage( const NUI_IDENTITY_MESSAGE* pIdentityMessage );
    VOID UpdatePlayerSlotInfo();
    VOID UpdateController();
    VOID UpdatePlayersState();
    VOID UpdatePlayersGamerTag();
    DWORD DetermineLonguestInnactivePlayer( BOOL bExcludeSignedInPlayers = FALSE );
    VOID ProcessIdentify( DWORD dwSkeletonIndex, DWORD dwEnrollmentIndex );
    VOID ProcessEnroll( DWORD dwSkeletonIndex, DWORD dwEnrollmentIndex );
    VOID ProcessAirCirclesGesture( DWORD dwSkeletonIndex );
    D3DCOLOR GetPlayerColor( DWORD dwPlayerSlotIndex );
    BOOL  IsPlayerSignedIn( DWORD dwEnrollmentIndex );
    WCHAR* GetPreferredPlayerName( DWORD dwEnrollmentIndex );
    DWORD CountEnrolledPlayers() const;
    DWORD CountEnrolledSkeletons() const;
    DWORD GetSkeletonIndexFromTrackingId( DWORD dwTrackingID ) const;
    DWORD GetSkeletonIndexFromTrackingIdForAsyncOps( DWORD dwTrackingID ) const;

    HRESULT Render();
    VOID RenderGreetings();
    VOID RenderPlayerHistoryUI();
    VOID RenderCorrectYourIdUI();
    VOID RenderPauseMenuUI();
    VOID RenderSkeleton( DWORD dwSkeletonIndex );
    VOID RenderPlayerInfo( DWORD dwPlayerSlotIndex );
    VOID RenderPlayerMessage( DWORD dwPlayerSlotIndex );
    VOID RenderTitleInfo( const WCHAR* pwszMode );
    VOID RenderFlickLeftGestureGuide( const FlickLeftGesture& flickLeftGesture );
    VOID RenderFlickRightGestureGuide( const FlickRightGesture& flickRightGesture );
    VOID RenderListSelectionGestureGuide( const ListSelectionGesture& listSelectionGesture );
    VOID RenderStopGestureGuide( const StopGesture& stopGesture );
    VOID RenderWaveGestureGuide( const WaveGesture& waveGesture );
    VOID RenderAirCirclesGestureGuide( const AirCirclesGesture& airCirclesGesture );

    XMFLOAT2* ProjectToScreen( const XMVECTOR& vSource, XMFLOAT2* pProjected ) const;
    XMFLOAT2* ProjectToScreen( const XMVECTOR vSourceArray[], DWORD dwDataCount, XMFLOAT2 pProjectedArray[] ) const;

    const WCHAR* GetSkeletonStatusText( DWORD dwPlayerSlotIndex ) const;

    VOID UpdatePIPImage();
};

BOOL IdentityIdentifyCallback( PVOID pvContext, NUI_IDENTITY_MESSAGE* pMessage );
BOOL IdentityEnrollCallback( PVOID pvContext, NUI_IDENTITY_MESSAGE* pMessage );

const WCHAR* GetLastResultText( HRESULT hrLastResult );
 

//--------------------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the program
//--------------------------------------------------------------------------------------
VOID __cdecl main()
{
    Sample atgApp;
    ATG::GetVideoSettings( &atgApp.m_d3dpp.BackBufferWidth, &atgApp.m_d3dpp.BackBufferHeight );

    atgApp.m_d3dpp.BackBufferFormat =  ( D3DFORMAT )MAKESRGBFMT( D3DFMT_A8R8G8B8 );
    atgApp.m_d3dpp.FrontBufferFormat = ( D3DFORMAT )MAKESRGBFMT( D3DFMT_LE_X8R8G8B8 );

    atgApp.m_d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_TWO;

    atgApp.Run();
}
 
 
//--------------------------------------------------------------------------------------
// Name: Initialize()
//--------------------------------------------------------------------------------------
HRESULT Sample::Initialize()
{
    // Initialize base member variables  
    m_bDrawHelp            = FALSE;
    m_bDrawHistoryUI       = FALSE;
    m_bDrawCorrectYourIdUI = FALSE;
    m_bDrawPauseMenuUI     = FALSE;
    m_bShowGestureGuides   = FALSE;

    m_bSystemUIShowing     = FALSE;

    m_dwPauseMenuSkeletonInControl     = 0; 
    m_dwCorrectYourIdSkeletonInControl = 0;

    m_dwTotalPlayerCount   = 0;

    XMemSet( &m_SkeletonFrame, 0, sizeof( m_SkeletonFrame ) );

    for( INT i = 0; i < NUI_SKELETON_COUNT; ++ i )
    {
        ResetSkeletonIdentityInfo( i );
        m_PlayerRequest[ i ].eRequest = PLAYER_NOREQUEST;
    }

    for( INT i = 0; i < NUI_IDENTITY_MAX_ENROLLMENT_COUNT; ++ i )
    {
        ResetPlayerInfo( i );
    }

    for( INT i = 0; i < SKELETON_SLOT_COUNT; ++ i )
    {
        ResetSkeletonSlotInfo( i );
    }

    // Required for XShowNuiSigninUI()
    if( FAILED( XOnlineStartup() ) )
    {
        ATG_PrintError( "XOnlineStartup() failed" );
    }

    // Initialize the Kinect Sensor Array
    StartupCamera();

    // Creating a notification listener to keep track of changes in profile sign-in status
    m_hNotification = XNotifyCreateListener( XNOTIFY_SYSTEM );
    if( m_hNotification == NULL || m_hNotification == INVALID_HANDLE_VALUE )
    { 
        ATG_PrintError( "Unable to create XNotify listener" );
    }

    // Confine all drawing to the title safe area
    m_SafeArea = ATG::GetTitleSafeArea();

    // Create the font
    if( FAILED( m_Font.Create( "game:\\Media\\Fonts\\Arial_16.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Create the help
    if( FAILED( m_Help.Create( "game:\\Media\\Help\\Help.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;  
 
    // Simple shader are required for the picture in picture feature.
    ATG::SimpleShaders::Initialize( NULL, NULL );

    if( FAILED( m_pip.Initialize( m_pd3dDevice, NUI_INITIALIZE_FLAG_USES_COLOR |
                                                NUI_INITIALIZE_FLAG_USES_SKELETON,
                                  NUI_IMAGE_RESOLUTION_640x480 ) ) )
    {
        ATG_PrintError( "Picture in Picture initialization failed" );
    }

    // Update data for player that were already signed-in before launching this sample.
    UpdatePlayersGamerTag();

    return S_OK;
}

 
//--------------------------------------------------------------------------------------
// Name: Sample::StartupCamera()
// Desc: Setup the sensor array for RGB streaming and initializes the skeletal pipeline.
//--------------------------------------------------------------------------------------
HRESULT Sample::StartupCamera()
{
    // Initializes the Natural Input system. Note that the color stream is required only 
    // for the picture in picture feature
    HRESULT hr = NuiInitialize( NUI_INITIALIZE_FLAG_USES_SKELETON |
                                NUI_INITIALIZE_FLAG_USES_COLOR |
                                NUI_INITIALIZE_FLAG_USES_DEPTH,
                                NUI_INITIALIZE_DEFAULT_HARDWARE_THREAD );

    if( FAILED( hr ) )
    {
        ATG::NuiPrintError( hr, "NuiInitialize" );
        return E_FAIL;
    }

    // Open the color stream
    hr = NuiImageStreamOpen( NUI_IMAGE_TYPE_COLOR, NUI_IMAGE_RESOLUTION_640x480, 0, 1, NULL, &m_hImage );
    if( FAILED ( hr ) )
    {
        ATG::NuiPrintError( hr, "NuiImageStreamOpen" );
        return E_FAIL;
    }

    // Enable skeletal tracking
    hr = NuiSkeletonTrackingEnable( NULL, 0 );
    if( FAILED ( hr ) )
    {
        ATG::NuiPrintError( hr, "NuiSkeletonTrackingEnable" );
        return E_FAIL;
    }

    // Depth buffer is always 320 x 240.
    m_fDepthDisplayScaleX = (FLOAT) m_d3dpp.BackBufferWidth / 320;
    m_fDepthDisplayScaleY = (FLOAT) m_d3dpp.BackBufferHeight / 240;

    return hr;
}

 
//--------------------------------------------------------------------------------------
// Name: Sample::ResetSkeletonIdentityInfo(()
// Desc: Put the skeleton identity data structure for the specified player back into
//       its original state.
//--------------------------------------------------------------------------------------
VOID Sample::ResetSkeletonIdentityInfo( DWORD dwIndex )
{
    assert( dwIndex < NUI_SKELETON_COUNT );

    XMemSet( &m_SkeletonIdentityInfo[ dwIndex ], 0, sizeof( SKELETON_IDENTITY_INFO ) );
}

 
//--------------------------------------------------------------------------------------
// Name: Sample::ResetPlayerInfo()
// Desc: Put the extended skeleton data structure for the specified player back into
//       its original state.
//--------------------------------------------------------------------------------------
VOID Sample::ResetPlayerInfo( DWORD dwIndex )
{
    assert( dwIndex < NUI_IDENTITY_MAX_ENROLLMENT_COUNT );

    XMemSet( &m_PlayerInfo[ dwIndex ], 0, sizeof( PLAYER_INFO ) );

    const static D3DCOLOR PlayerColors[ NUI_IDENTITY_MAX_ENROLLMENT_COUNT ] = { 0xff3fff44, 0xffd1ff3f, 0xff693fff, 0xffff863f, 
                                                                                0xffff3f40, 0xffc33fff, 0xff3fc7ff, 0xffffc83f };

    m_PlayerInfo[ dwIndex ].Color = PlayerColors[ dwIndex ];

    m_PlayerInfo[ dwIndex ].fInnactiveSince = m_Timer.GetAbsoluteTime();
}


//--------------------------------------------------------------------------------------
// Name: Sample::ResetSkeletonSlotInfo()
// Desc: Put the player slot info data structure for the specified player back into
//       its original state.
//--------------------------------------------------------------------------------------
VOID Sample::ResetSkeletonSlotInfo( DWORD dwIndex )
{
    assert( dwIndex < SKELETON_SLOT_COUNT );

    XMemSet( &m_SkeletonSlotInfo[ dwIndex ], 0, sizeof( SKELETON_SLOT_INFO ) );
}


//--------------------------------------------------------------------------------------
// Name: Update()
// Desc: Called once per frame to update the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Update()
{
    // Process system notifications and any message received from NuiIdentity
    ProcessNotifications();

    // Check the controller, even in Kinect title, you never know if the sensor array is working properly
    UpdateController();

    // Update the background image for the Picture in Picture feature
    UpdatePIPImage();

    // Retrieve the skeletal data for this frame
    // The rest of the update function will only get executed if a new skeleton frame is available. Normally this occurs every 33ms
    // but it some circonstances the time delta between two skeleton frames may be quite long (think secondsa or minutes). 
    // So ensure that the code following this call only needs to be executed when there is a new skeleton and the title can 
    // spend a long time not executing it.
    // Long delay between skeleton frames will hapen if the skeleton frame is being re-routed to the XAM. This happens 
    // if the guide is open for example.
    HRESULT hr = NuiSkeletonGetNextFrame( NUI_CAMERA_TIMEOUT_DEFAULT, &m_SkeletonFrame );
    if( FAILED( hr ) )
    {
        return S_OK;
    }

    UpdatePlayerSlotInfo();

    // Do some low latecy smoothing on the data 
    NuiTransformSmooth( &m_SkeletonFrame, NULL );

    // Update the skeleton data used by the picture in picture feature
    m_pip.SetSkeletons( &m_SkeletonFrame );

    // Update players state based on the new skeleton states
    UpdatePlayersState();

    // Identify and enroll / re-enroll the players as needed
    for( DWORD i = 0; i < NUI_SKELETON_COUNT; ++ i )
    {
        DWORD dwTrackingID = m_SkeletonFrame.SkeletonData[ i ].dwTrackingID;   
 
        switch( m_PlayerRequest[ i ].eRequest )
        {
            case PLAYER_NEEDIDENTIFY:
            {
                hr = NuiIdentityIdentify( dwTrackingID,             // Identify for this specific skeleton
                                          0,                        // Flags, reserved - must be 0
                                          IdentityIdentifyCallback, // NuiIdentity will call this function to report progress
                                          &m_MessagePipe    );      // Message pipe for our callback to use to forward messages
                assert( hr != E_INVALIDARG );
                assert( hr == E_PENDING || hr == E_NUI_IDENTITY_BUSY );
                m_adwAsyncOpTrackingID[ i ] = dwTrackingID;
                break;
            }

            case PLAYER_WANTENROLL:
            {
                hr = NuiIdentityEnroll( dwTrackingID,                   // Enroll this skeleton 
                                        0,                              // Ignored, unless re-enrolling
                                        NUI_IDENTITY_ENROLL_SKELETON,   // Enroll using skeleton data
                                        IdentityEnrollCallback,         // NuiIdentity will call this function to report progress
                                        &m_MessagePipe );               // Message pipe for our callback to use to forward messages
                                        
                if( hr == E_NUI_IDENTITY_ENROLLMENT_LIMIT )
                {
                    // If we have reached the maximium number of players that can be enrolled at once, 
                    //  then remove the least recently active player, that isn't signed-in and re-enroll this player.
                    DWORD dwPlayerToUnenroll = DetermineLonguestInnactivePlayer( TRUE );
                    hr = NuiIdentityUnenroll( dwPlayerToUnenroll );
                    assert( SUCCEEDED( hr ) );
                    ResetPlayerInfo( dwPlayerToUnenroll );
                    hr = NuiIdentityEnroll( dwTrackingID,                   // Enroll this skeleton 
                                            0,                              // Ignored, unless re-enrolling
                                            NUI_IDENTITY_ENROLL_SKELETON,   // Enroll using skeleton data
                                            IdentityEnrollCallback,         // NuiIdentity will call this function to report progress
                                            &m_MessagePipe );               // Message pipe for our callback to use to forward messages
                }

                assert( hr != E_INVALIDARG );
                assert( hr == E_PENDING || hr == E_NUI_IDENTITY_BUSY ); 
                m_adwAsyncOpTrackingID[ i ] = dwTrackingID;
                break;
            }

            case PLAYER_WANTREENROLL:
            {
                assert( m_PlayerRequest[ i ].dwData <= NUI_IDENTITY_MAX_ENROLLMENT_COUNT || 
                        m_PlayerRequest[ i ].dwData == NUI_IDENTITY_ENROLLMENT_INDEX_UNKNOWN );

                hr = NuiIdentityEnroll( dwTrackingID,                      // ID of the skeleton we want to re-enroll 
                                        m_PlayerRequest[ i ].dwData,       // New enrollment index requested
                                        NUI_IDENTITY_ENROLL_SKELETON |     // Enroll using skeleton data
                                        NUI_IDENTITY_FORCE_ENROLL,         // Ask for re-enrollment
                                        IdentityEnrollCallback,            // Callback to process feedback from NuiIdentity API
                                        &m_MessagePipe );                  // Message pipe for our callback to use to forward messages

                assert( hr == E_PENDING || hr == E_NUI_IDENTITY_BUSY ); 
                m_adwAsyncOpTrackingID[ i ] = dwTrackingID;
                m_SkeletonIdentityInfo[ i ].bIsEnrolling = TRUE;
                break;
            }

            case PLAYER_NOREQUEST:
            {
                break;
            }

            default:
            {
                assert( false );
                break;
            }
        }
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Sample::ProcessNotifications
// Desc: Dispatches system messages and NuiIdentity messages to the appropriate 
//       processing functions.
//--------------------------------------------------------------------------------------
VOID Sample::ProcessNotifications()
{
    DWORD dwNotificationID;
    ULONG_PTR ulParam;

    if( XNotifyGetNext( m_hNotification, 0, &dwNotificationID, &ulParam ) )
    {
        switch( dwNotificationID )
        {
           case XN_SYS_NUIPAUSE:
                m_dwPauseMenuSkeletonInControl = GetSkeletonIndexFromTrackingId( ulParam );
                if( m_dwPauseMenuSkeletonInControl == E_FAIL )
                {
                    // GetSkeletonIndexFromTrackingId will fail if tracking was lost between the time the 
                    // message was sent and now. In this situation, there isn't much that can be done but bail out.
                    return;
                }

                assert( m_dwPauseMenuSkeletonInControl < NUI_SKELETON_COUNT );
                m_PauseMenuOptionList[ m_dwPauseMenuSkeletonInControl ].SetNumItems( 2 );
                m_bDrawPauseMenuUI = TRUE;
                break;

            case XN_SYS_NUIBINDINGCHANGED:
                UpdatePlayersGamerTag();
                break;

            case XN_SYS_UI:
                m_bSystemUIShowing = ( BOOL )ulParam;
                break;
        }
    }

    bool bWasDataRead;
    do
    {
        IDENTITY_CALLBACK_MESSAGE IdentityMessage;
        bWasDataRead = m_MessagePipe.Read( &IdentityMessage, sizeof( IDENTITY_CALLBACK_MESSAGE ) );

        if( bWasDataRead )
        {
            switch( IdentityMessage.Message.MessageId )
            {
                case NUI_IDENTITY_MESSAGE_ID_FRAME_PROCESSED:
                {
                    ProcessFrameProcessedMessage( &IdentityMessage.Message );
                    break;
                }

                case NUI_IDENTITY_MESSAGE_ID_COMPLETE:
                {
                    switch( IdentityMessage.eReason )
                    {
                        case IDENTITY_CALLBACK_IDENTIFY:
                            ProcessIdentificationCompletedMessage( &IdentityMessage.Message );
                            break;

                        case IDENTITY_CALLBACK_ENROLL:
                            ProcessEnrollmentCompletedMessage( &IdentityMessage.Message );
                            break;

                        default:
                            assert( false );
                    }
                    break;
                }
         
                default:
                {
                    assert( false );
                    break;
                }
            }
        }
    }
    while( bWasDataRead );
}


//--------------------------------------------------------------------------------------
// Name: Sample::ProcessFrameProcessedMessage
// Desc: Update player specific quality feedback status based on the content of the
//       message from NuiIdentity.
//--------------------------------------------------------------------------------------
VOID Sample::ProcessFrameProcessedMessage( const NUI_IDENTITY_MESSAGE* pIdentityMessage )
{
    DWORD dwSkeletonIndex = GetSkeletonIndexFromTrackingIdForAsyncOps( pIdentityMessage->dwTrackingID );
    if( dwSkeletonIndex == E_FAIL )
    {
        // GetSkeletonIndexFromTrackingIdForAsyncOps will fail if tracking was lost between the time the 
        // message was sent and now. In this situation, there isn't much that can be done but bail out.
        return;
    }

    assert( dwSkeletonIndex < NUI_SKELETON_COUNT );
    m_MessageTimer[ dwSkeletonIndex ].Reset();
    m_SkeletonIdentityInfo[ dwSkeletonIndex ].dwQualityFlags = pIdentityMessage->Data.FrameProcessed.dwQualityFlags;
    m_SkeletonIdentityInfo[ dwSkeletonIndex ].hrLastResult   = S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Sample::ProcessIdentificationCompletedMessage
// Desc: Update player specific information based on the success or failure of 
//       a call to NuiIdentityIdentify().
//--------------------------------------------------------------------------------------
VOID Sample::ProcessIdentificationCompletedMessage( const NUI_IDENTITY_MESSAGE* pIdentityMessage )
{
    DWORD dwSkeletonIndex = GetSkeletonIndexFromTrackingIdForAsyncOps( pIdentityMessage->dwTrackingID );
    if( dwSkeletonIndex == E_FAIL )
    {
        // GetSkeletonIndexFromTrackingIdForAsyncOps will fail if tracking was lost between the time the 
        // message was sent and now. In this situation, there isn't much that can be done but bail out.
        return;
    }

    assert( dwSkeletonIndex < NUI_SKELETON_COUNT );
    m_MessageTimer[ dwSkeletonIndex ].Reset();
    m_SkeletonIdentityInfo[ dwSkeletonIndex ].dwQualityFlags = 0;
    m_SkeletonIdentityInfo[ dwSkeletonIndex ].hrLastResult   = pIdentityMessage->Data.Complete.hrResult;

    if( SUCCEEDED( pIdentityMessage->Data.Complete.hrResult ) )
    {
        ProcessIdentify( dwSkeletonIndex, pIdentityMessage->Data.Complete.dwEnrollmentIndex );
    }
}


//--------------------------------------------------------------------------------------
// Name: Sample::ProcessEnrollmentCompletedMessage
// Desc: Update player specific information based on the success or failure of 
//       a call to NuiIdentityEnroll().
//--------------------------------------------------------------------------------------
VOID Sample::ProcessEnrollmentCompletedMessage( const NUI_IDENTITY_MESSAGE* pIdentityMessage )
{
    DWORD dwSkeletonIndex = GetSkeletonIndexFromTrackingIdForAsyncOps( pIdentityMessage->dwTrackingID );
    if( dwSkeletonIndex == E_FAIL )
    {
        // GetSkeletonIndexFromTrackingIdForAsyncOps will fail if tracking was lost between the time the 
        // message was sent and now. In this situation, there isn't much that can be done but bail out.
        return;
    }

    assert( dwSkeletonIndex < NUI_SKELETON_COUNT );
    m_MessageTimer[ dwSkeletonIndex ].Reset();
    m_SkeletonIdentityInfo[ dwSkeletonIndex ].dwQualityFlags = 0;
    m_SkeletonIdentityInfo[ dwSkeletonIndex ].hrLastResult   = pIdentityMessage->Data.Complete.hrResult;

    if( SUCCEEDED( pIdentityMessage->Data.Complete.hrResult ) )
    {
        ProcessEnroll( dwSkeletonIndex, pIdentityMessage->Data.Complete.dwEnrollmentIndex );
        m_PlayerInfo[ pIdentityMessage->Data.Complete.dwEnrollmentIndex ].bHasShownIntentToPlay = TRUE;
    }
}


//--------------------------------------------------------------------------------------
// Name: UpdateController()
// Desc: 
//--------------------------------------------------------------------------------------
VOID Sample::UpdateController()
{
    // Get the current gamepad status
    ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput();

    // Toggle help
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
        m_bDrawHelp = !m_bDrawHelp; 

    // Toggle gesture guides
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_A )
        m_bShowGestureGuides = !m_bShowGestureGuides; 
}


//--------------------------------------------------------------------------------------
// Name: UpdatePIPImage()
// Desc: Update the color image for the picture in picture feature.
//--------------------------------------------------------------------------------------
VOID Sample::UpdatePIPImage()
{
    HRESULT hr = NuiImageStreamGetNextFrame( m_hImage, NUI_CAMERA_TIMEOUT_DEFAULT, &m_pImageFrame );
    if( SUCCEEDED( hr ) )
    {
        m_pip.SetColorTexture( m_pImageFrame->pFrameTexture );
        NuiImageStreamReleaseFrame( m_hImage, m_pImageFrame );
    }
}


//--------------------------------------------------------------------------------------
// Name: UpdatePlayerSlotInfo()
// Desc: 
//--------------------------------------------------------------------------------------
VOID Sample::UpdatePlayerSlotInfo()
{
    // Clean-up any player slots for which tracking has been lost
    for( DWORD i = 0; i < SKELETON_SLOT_COUNT; ++ i )
    {
        if( m_SkeletonSlotInfo[ i ].bIsInUse )
        {
            assert( m_SkeletonSlotInfo[ i ].dwSkeletonIndex < NUI_SKELETON_COUNT );
            if( m_SkeletonFrame.SkeletonData[  m_SkeletonSlotInfo[ i ].dwSkeletonIndex ].eTrackingState != NUI_SKELETON_TRACKED )
            {
                ResetSkeletonSlotInfo( i );
            }
        }
    }

    // Establish link to any new tracked skeleton
    for( DWORD i = 0; i < NUI_SKELETON_COUNT; ++ i)
    {
        if( m_SkeletonFrame.SkeletonData[ i ].eTrackingState == NUI_SKELETON_TRACKED )
        {
            DWORD dwFirstOpenSlot = SKELETON_SLOT_COUNT;
            BOOL  bFound = FALSE;
            for( DWORD j = 0; j < SKELETON_SLOT_COUNT; ++ j )
            {
                if( m_SkeletonSlotInfo[ j ].bIsInUse )
                {
                    if( m_SkeletonSlotInfo[ j ].dwSkeletonIndex == i )
                    {
                        bFound = TRUE;
                    }
                }
                else
                {
                    if( j < dwFirstOpenSlot )
                    {
                        dwFirstOpenSlot = j;
                    }
                }
            }

            if( !bFound )
            {
                assert( dwFirstOpenSlot < SKELETON_SLOT_COUNT );
                m_SkeletonSlotInfo[ dwFirstOpenSlot ].bIsInUse = TRUE;
                m_SkeletonSlotInfo[ dwFirstOpenSlot ].dwSkeletonIndex = i;

            }

        }
    }
}


//--------------------------------------------------------------------------------------
// Name: UpdatePlayersState()
// Desc: Handle player identification, menu navigation and gameplay
//--------------------------------------------------------------------------------------
VOID Sample::UpdatePlayersState()
{
    for( DWORD i = 0; i < NUI_SKELETON_COUNT; ++ i )
    {
        m_PlayerRequest[ i ].eRequest = PLAYER_NOREQUEST;

        // Update the gestures for this skeleton.
        if( m_GestureBlackoutTimer.GetAppTime() >= GESTURE_BLACKOUT_DELAY )
        {
            m_FlickLeftGesture[ i ].Update( m_SkeletonFrame.SkeletonData[ i ] );
            m_FlickRightGesture[ i ].Update( m_SkeletonFrame.SkeletonData[ i ] );
            m_CorrectYourIdOptionList[ i ].Update( m_SkeletonFrame.SkeletonData[ i ] );
            m_PauseMenuOptionList[ i ].Update( m_SkeletonFrame.SkeletonData[ i ] );
            m_StopGesture[ i ].Update( m_SkeletonFrame.SkeletonData[ i ] );
            m_WaveGesture[ i ].Update( m_SkeletonFrame.SkeletonData[ i ] );
            m_AirCirclesGesture[ i ].Update( m_SkeletonFrame.SkeletonData[ i ] );
        }

        // There is nothing to do if the player isn't tracked, except closing any UI for which this player was in controll.
        if( m_SkeletonFrame.SkeletonData[ i ].eTrackingState != NUI_SKELETON_TRACKED )
        {
            // Close the Pause menu UI, if the owning player has been lost.
            if( m_bDrawPauseMenuUI && m_dwPauseMenuSkeletonInControl == i )
            {
                m_bDrawPauseMenuUI = FALSE;
                m_StopGesture[ i ].Reset();
            }

            // Close the Correct Your ID UI, if the owning player has been lost.
            if( m_bDrawCorrectYourIdUI && m_dwCorrectYourIdSkeletonInControl == i )
            {
                m_bDrawCorrectYourIdUI = FALSE;
                m_GestureBlackoutTimer.Reset();
                m_StopGesture[ i ].Reset();
            }

            continue;
        }     
 
        // If this player is controlling the Pause Menu UI...
        if( m_bDrawPauseMenuUI && m_dwPauseMenuSkeletonInControl == i )
        {
            // If the player has selected an option from the list...
            if( m_PauseMenuOptionList[ i ].GetStatus() == GESTURE_COMPLETED )
            {
                DWORD dwSelection = m_PauseMenuOptionList[ i ].GetSelection();
                switch( dwSelection )
                {
                    case 0:
                        XShowNuiGuideUI( m_SkeletonFrame.SkeletonData[ m_dwPauseMenuSkeletonInControl ].dwTrackingID );
                        break;

                    case 1:
                        m_bDrawPauseMenuUI = FALSE;
                        break;

                    default:
                        assert( false );
                        break;
                }
           }

            continue;
        }

        // If this player is controlling the Correct Your ID UI...
        if( m_bDrawCorrectYourIdUI && m_dwCorrectYourIdSkeletonInControl == i )
        {
            // If the player wants to cancel the operation
            if( m_StopGesture[ i ].GetStatus() == GESTURE_COMPLETED )
            {
                m_bDrawCorrectYourIdUI = FALSE;
                m_GestureBlackoutTimer.Reset();
                m_StopGesture[ i ].Reset();
            }
	
            // If the player has selected an enrollment option...
            if( m_CorrectYourIdOptionList[ i ].GetStatus() == GESTURE_COMPLETED )
            {
                m_bDrawCorrectYourIdUI = FALSE;
                m_GestureBlackoutTimer.Reset();
                m_SkeletonIdentityInfo[ i ].bIsEnrolling = TRUE;

                DWORD dwReEnrollIndex = m_CorrectYourIdOptionList[ i ].GetSelection();
                assert( dwReEnrollIndex >= 0 && dwReEnrollIndex <= NUI_IDENTITY_MAX_ENROLLMENT_COUNT );
                if( dwReEnrollIndex == NUI_IDENTITY_MAX_ENROLLMENT_COUNT )
                {
                    dwReEnrollIndex  = DetermineLonguestInnactivePlayer( TRUE );
                    NuiIdentityUnenroll( dwReEnrollIndex );
                    ResetPlayerInfo( dwReEnrollIndex );
                    m_PlayerRequest[ i ].dwData = NUI_IDENTITY_ENROLLMENT_INDEX_UNKNOWN;
                }
                else if( dwReEnrollIndex ==  m_CorrectYourIdOptionList[ i ].GetCount() - 1 )
                {
                    m_PlayerRequest[ i ].dwData = NUI_IDENTITY_ENROLLMENT_INDEX_UNKNOWN;
                }
                else
                {
                    m_PlayerRequest[ i ].dwData = dwReEnrollIndex;
                }
                m_PlayerRequest[ i ].eRequest = PLAYER_WANTREENROLL;
            }

            continue;
        }

        // Check if a player requested the sign-in UI be opened
        if( !m_bSystemUIShowing && !m_bDrawCorrectYourIdUI && !m_bDrawPauseMenuUI && m_WaveGesture[ i ].GetStatus() == GESTURE_COMPLETED )
        {  
            XShowNuiSigninUI( m_SkeletonFrame.SkeletonData[ i ].dwTrackingID, 0 );
            m_SkeletonIdentityInfo[ i ].bIsEnrolling = TRUE;
            m_bSystemUIShowing = TRUE;
            continue;
        }

        // Check if a player requested the correct your ID UI be opened
        if( !m_bSystemUIShowing && !m_bDrawCorrectYourIdUI && !m_bDrawPauseMenuUI && m_StopGesture[ i ].GetStatus() == GESTURE_COMPLETED )
        {
            // A player needs to have to been identified before she can correct her identity.
            if( m_SkeletonFrame.SkeletonData[ i ].dwEnrollmentIndex < NUI_IDENTITY_MAX_ENROLLMENT_COUNT || m_SkeletonFrame.SkeletonData[ i ].dwEnrollmentIndex == NUI_IDENTITY_ENROLLMENT_INDEX_UNKNOWN )
            {
                m_dwCorrectYourIdSkeletonInControl = i;
                m_CorrectYourIdOptionList[ m_dwCorrectYourIdSkeletonInControl ].SetNumItems( CountEnrolledPlayers() + 1 );
                m_bDrawCorrectYourIdUI = TRUE;
                continue;
            }
        }

        switch( m_SkeletonFrame.SkeletonData[ i ].dwEnrollmentIndex )
        {
            // If the Identity system is already busy identifying or enrolling the player
            case NUI_IDENTITY_ENROLLMENT_INDEX_BUSY:
            {
                break;
            }
        
            // If this is a new player not yet identified
            case NUI_IDENTITY_ENROLLMENT_INDEX_CALL_IDENTIFY:
            {
                m_SkeletonIdentityInfo[ i ].bIsIdentifying = TRUE;
                m_MessageTimer[ i ].Reset();
                m_PlayerRequest[ i ].eRequest = PLAYER_NEEDIDENTIFY;
                break;
            }

            // if the player has been identified but is unknown
            case NUI_IDENTITY_ENROLLMENT_INDEX_UNKNOWN:
            {
                if( m_SkeletonIdentityInfo[ i ].bIsIdentifying )
                {
                    m_SkeletonIdentityInfo[ i ].bIsIdentifying = FALSE;
                }

                if( m_AirCirclesGesture[ i ].GetStatus() == GESTURE_COMPLETED )
                {
                    m_PlayerInfo[ i ].bHasShownIntentToPlay = TRUE;
                    m_SkeletonIdentityInfo[ i ].bIsEnrolling = TRUE;
                    m_MessageTimer[ i ].Reset();
                    m_PlayerRequest[ i ].eRequest = PLAYER_WANTENROLL;
                }

                break;
            }

            // An identification or enrollment operation failed for one of the following reasons:
            // - The title doesn't provide a callback to an identity operation. 
            // - The NUI_IDENTITY_FORCE_ENROLL flag is not set. 
            // - The operation times out because of quality issues. 
            // - The operation is cancelled by the title. 
            case NUI_IDENTITY_ENROLLMENT_INDEX_FAILURE:
            {
                // In this case we do nothing and let the player request the "Correct Your ID" Ui to select how to sign-in. 
                // A title could also choose to FORCE_ENROLL the player in this case.
                break;
            }

            // If the player is enrolled. This is the mode a title spends most of its time in
            default:
            {
                DWORD dwErollmentIndex = m_SkeletonFrame.SkeletonData[ i ].dwEnrollmentIndex;
                assert( dwErollmentIndex < NUI_IDENTITY_MAX_ENROLLMENT_COUNT );
                
                if( m_SkeletonIdentityInfo[ i ].bIsEnrolling == TRUE )
                {
                    ProcessEnroll( i, dwErollmentIndex );
                    m_PlayerInfo[ dwErollmentIndex ].bHasShownIntentToPlay = TRUE;
                    m_MessageTimer[ i ].Reset();

                }

                if( m_PlayerInfo[ dwErollmentIndex ].bHasShownIntentToPlay )
                {
                    if( !m_bDrawHistoryUI && m_FlickRightGesture[ i ].GetStatus() == GESTURE_COMPLETED )
                    {
                        m_bDrawHistoryUI = TRUE;
                    }
          
                    if( m_bDrawHistoryUI && m_FlickLeftGesture[ i ].GetStatus() == GESTURE_COMPLETED )
                    {
                        m_bDrawHistoryUI = FALSE;
                    }
        
                    m_PlayerInfo[ dwErollmentIndex ].fInnactiveSince = m_Timer.GetAbsoluteTime();

                    // Let's play...
                    ProcessAirCirclesGesture( i );
                }
                else
                {
                    if( m_AirCirclesGesture[ i ].GetStatus() == GESTURE_COMPLETED )
                    {
                        m_PlayerInfo[ dwErollmentIndex ].bHasShownIntentToPlay = TRUE;
                        m_MessageTimer[ i ].Reset();
                    }
                }

                break;
            }
        }
    }
}


//--------------------------------------------------------------------------------------
// Name: UpdatePlayersGamerTag
// Desc: Refreshes the GamerTag for each player based on the associated sign-in index
//--------------------------------------------------------------------------------------
VOID Sample::UpdatePlayersGamerTag()
{
    for( DWORD i = 0 ; i < NUI_IDENTITY_MAX_ENROLLMENT_COUNT; ++ i )
    {
        NUI_ENROLLMENT_INFORMATION EnrollmentInformation;
        NuiIdentityGetEnrollmentInformation( i, &EnrollmentInformation );

        if( EnrollmentInformation.dwEnrollmentFlags == 0 )
        {
            // Player isn't enrolled... 
            ResetPlayerInfo( i );
        }
        else if( EnrollmentInformation.dwUserIndex ==  XUSER_INDEX_NONE )
        {
            m_PlayerInfo[ i ].wszGamerTag[ 0 ] = L'\0';
        }
        else
        {
            assert( EnrollmentInformation.dwUserIndex < XUSER_MAX_COUNT );

            CHAR szGamerTag[ XUSER_NAME_SIZE ];
#if defined(NDEBUG) 
            XUserGetName( EnrollmentInformation.dwUserIndex, szGamerTag, XUSER_NAME_SIZE );
#else
            HRESULT hr = XUserGetName( EnrollmentInformation.dwUserIndex, szGamerTag, XUSER_NAME_SIZE );
            assert( hr == ERROR_SUCCESS );
#endif
            if( strlen( szGamerTag ) > 0 )
            {
                MultiByteToWideChar( CP_UTF8, 0, szGamerTag, -1, m_PlayerInfo[ i ].wszGamerTag, XUSER_NAME_SIZE );
            }
            else
            {
                m_PlayerInfo[ i ].wszGamerTag[ 0 ] = L'\0';
            }
        }
    }
}


//--------------------------------------------------------------------------------------
// Name: IdentityIdentifyCallback
// Desc: Callback used to obtain feedback from calls to NuiIdentityIdentify() and 
//       forwards to the main thread.
//--------------------------------------------------------------------------------------
BOOL IdentityIdentifyCallback( PVOID pvContext, NUI_IDENTITY_MESSAGE* pMessage )
{
    assert( pvContext != NULL );
    assert( pMessage != NULL );

    IdentityMessagePipe* pMessagePipe = ( IdentityMessagePipe* ) pvContext;

    IDENTITY_CALLBACK_MESSAGE IdentityMessage = { IDENTITY_CALLBACK_IDENTIFY, *pMessage };
#if defined(NDEBUG) 
    pMessagePipe->Write( &IdentityMessage, sizeof( IDENTITY_CALLBACK_MESSAGE ) );
#else
    bool bWasSent = pMessagePipe->Write( &IdentityMessage, sizeof( IDENTITY_CALLBACK_MESSAGE ) );
    assert( bWasSent );
#endif

    return TRUE;
}


//--------------------------------------------------------------------------------------
// Name: IdentityEnrollCallback
// Desc: Callback used to obtain feedback from calls to NuiIdentityEnroll() and 
//       forwards to the main thread.
//--------------------------------------------------------------------------------------
BOOL IdentityEnrollCallback( PVOID pvContext, NUI_IDENTITY_MESSAGE* pMessage )
{
    assert( pvContext != NULL );
    assert( pMessage != NULL );

    IdentityMessagePipe* pMessagePipe = ( IdentityMessagePipe* ) pvContext;

    IDENTITY_CALLBACK_MESSAGE IdentityMessage = { IDENTITY_CALLBACK_ENROLL, *pMessage };
#if defined(NDEBUG) 
    pMessagePipe->Write( &IdentityMessage, sizeof( IDENTITY_CALLBACK_MESSAGE ) );
#else
    bool bWasSent = pMessagePipe->Write( &IdentityMessage, sizeof( IDENTITY_CALLBACK_MESSAGE ) );
    assert( bWasSent );
#endif

    return TRUE;
}


//--------------------------------------------------------------------------------------
// Name: Sample::DetermineLonguestInnactivePlayer
// Desc: Find the player that has not been active for the longuest time.
//--------------------------------------------------------------------------------------
DWORD Sample:: DetermineLonguestInnactivePlayer( BOOL bExcludeSignedInPlayers )
{
    DWORD dwReEnrollIndex = NUI_IDENTITY_MAX_ENROLLMENT_COUNT;
 
    for( DWORD i = 0; i < NUI_IDENTITY_MAX_ENROLLMENT_COUNT; ++ i )
    {
        BOOL bActiveAndEnrolled = FALSE;
        for( DWORD j = 0; j < NUI_SKELETON_COUNT; ++ j )
        {
            if( m_SkeletonFrame.SkeletonData[ j ].eTrackingState == NUI_SKELETON_TRACKED &&
                m_SkeletonFrame.SkeletonData[ j ].dwEnrollmentIndex == i                    )
            {
                bActiveAndEnrolled = TRUE;
            }
        }

        if( !bActiveAndEnrolled && ( !bExcludeSignedInPlayers || !IsPlayerSignedIn( i ) ) && 
            ( dwReEnrollIndex == NUI_IDENTITY_MAX_ENROLLMENT_COUNT || m_PlayerInfo[ i ].fInnactiveSince < m_PlayerInfo[ dwReEnrollIndex ].fInnactiveSince ) )
        {
            dwReEnrollIndex = i;
        }
    }
 
    assert( dwReEnrollIndex >= 0 && dwReEnrollIndex < NUI_IDENTITY_MAX_ENROLLMENT_COUNT );
    return dwReEnrollIndex;
}


//--------------------------------------------------------------------------------------
// Name: Sample::Sample::ProcessIdentify
// Desc: Process the outcome from a call to NuiIdentityIdentify()
//--------------------------------------------------------------------------------------
VOID Sample::ProcessIdentify( DWORD dwSkeletonIndex, DWORD dwEnrollmentIndex )
{
    assert( dwSkeletonIndex < NUI_SKELETON_COUNT );

    m_SkeletonIdentityInfo[ dwSkeletonIndex ].bIsIdentifying = FALSE;

    if( dwEnrollmentIndex < NUI_IDENTITY_MAX_ENROLLMENT_COUNT )
    {
        ProcessEnroll( dwSkeletonIndex, dwEnrollmentIndex );
    }
}

//--------------------------------------------------------------------------------------
// Name: Sample::Sample::ProcessEnroll
// Desc: Process the outcome from a call to NuiIdentityEnroll()
//--------------------------------------------------------------------------------------
VOID Sample::ProcessEnroll( DWORD dwSkeletonIndex, DWORD dwEnrollmentIndex )
{
    assert( dwSkeletonIndex < NUI_SKELETON_COUNT );
    assert( dwEnrollmentIndex < NUI_IDENTITY_MAX_ENROLLMENT_COUNT );

    ++ m_PlayerInfo[ dwEnrollmentIndex ].dwEnrollmentCount;

    // Let's update the sign-in info to reflect any changes that could be tied to the change in the player line-up.
    UpdatePlayersGamerTag();

    m_SkeletonIdentityInfo[ dwSkeletonIndex ].bIsEnrolling = FALSE;

    if( m_PlayerInfo[ dwEnrollmentIndex ].wszPlayerName[ 0 ] == L'\0' )
    {
        // Create a unique name for this new player
        _snwprintf_s(m_PlayerInfo[ dwEnrollmentIndex ].wszPlayerName, XUSER_NAME_SIZE, _TRUNCATE, L"Player %d", ++ m_dwTotalPlayerCount);
    }
}


//--------------------------------------------------------------------------------------
// Name: Sample::ProcessAirCirclesGesture()
// Desc: Update game status for specified player
//--------------------------------------------------------------------------------------
VOID Sample::ProcessAirCirclesGesture( DWORD dwSkeletonIndex )
{
    switch( m_AirCirclesGesture[ dwSkeletonIndex ].GetStatus() )
    {
        case GESTURE_NOTSTARTED:
        {
            m_SkeletonIdentityInfo[ dwSkeletonIndex ].bIsPlaying = FALSE;
            break;
        }

        case GESTURE_INPROGRESS:
        {
            if( !m_SkeletonIdentityInfo[ dwSkeletonIndex ].bIsPlaying )
            {
                m_PlayerInfo[ m_SkeletonFrame.SkeletonData[ dwSkeletonIndex ].dwEnrollmentIndex ].dwAirCirclesCount = 0;

                m_SkeletonIdentityInfo[ dwSkeletonIndex ].bIsPlaying = TRUE;
            }

            break;
        }

        case GESTURE_COMPLETED:
        {
            ++ m_PlayerInfo[ m_SkeletonFrame.SkeletonData[ dwSkeletonIndex ].dwEnrollmentIndex ].dwAirCirclesCount;
            ++ m_PlayerInfo[ m_SkeletonFrame.SkeletonData[ dwSkeletonIndex ].dwEnrollmentIndex ].dwAirCirclesTotal;
                
            break;
        }

        default:
        {
            assert( false );
            break;
        }
    }
}


//--------------------------------------------------------------------------------------
// Name: Sample::Render()
// Desc: Called once per frame to render the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Render()
{
    // Set default states
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, FALSE );
  
    m_Timer.MarkFrame();
    if( m_bDrawHelp )
    {
        ATG::RenderBackground( 0xff000000, 0xff000000 ); // Use a black backgrounf for the help screen

        m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
    }
    else if( m_bDrawPauseMenuUI )
    {
        ATG::RenderBackground( 0xff0000ff, 0xff00ffff );

        RenderPauseMenuUI();
        RenderTitleInfo( L"Paused" );

        if( m_bShowGestureGuides )
        {
            RenderListSelectionGestureGuide( m_PauseMenuOptionList[ m_dwPauseMenuSkeletonInControl ] );
        }
    }
    else if( m_bDrawCorrectYourIdUI )
    {
        ATG::RenderBackground( 0xff0000ff, 0xff00ffff );

        RenderCorrectYourIdUI();
        RenderTitleInfo( L"Correct Your Identity" );

        if( m_bShowGestureGuides )
        {
            RenderListSelectionGestureGuide( m_CorrectYourIdOptionList[ m_dwCorrectYourIdSkeletonInControl ] );
            RenderStopGestureGuide( m_StopGesture[ m_dwCorrectYourIdSkeletonInControl ] );
        }
    }
    else if( m_bDrawHistoryUI )
    {
        ATG::RenderBackground( 0xff0000ff, 0xff00ffff );

        RenderPlayerHistoryUI();
        RenderTitleInfo( L"History" );

        if( m_bShowGestureGuides )
        {
            for( INT i = 0; i < NUI_SKELETON_COUNT; ++ i )
            {
                RenderFlickLeftGestureGuide( m_FlickLeftGesture[ i ] );
                RenderStopGestureGuide( m_StopGesture[ i ] );
                RenderWaveGestureGuide( m_WaveGesture[ i ] );
            }
        }
    }
    else
    {
        // Use a black background for the main screen to help better see the colored skeletons and the text
        ATG::RenderBackground( 0xff000000, 0xff000000 );
        
        for( INT i = 0; i < SKELETON_SLOT_COUNT; ++ i )
        {
            RenderPlayerMessage( i );
            RenderPlayerInfo( i );

            if( m_SkeletonSlotInfo[ i ].bIsInUse )
            {
                RenderSkeleton( i );

                 if( m_bShowGestureGuides )
                 {
                    if( m_AirCirclesGesture[ i ].GetStatus() == GESTURE_INPROGRESS || 
                        m_AirCirclesGesture[ i ].GetStatus() == GESTURE_COMPLETED )
                    {
                        RenderAirCirclesGestureGuide( m_AirCirclesGesture[ i ] );
                    }
                    else
                    {
                        RenderFlickRightGestureGuide( m_FlickRightGesture[ i ] );
                        RenderStopGestureGuide( m_StopGesture[ i ] );
                        RenderWaveGestureGuide( m_WaveGesture[ i ] );
                    }
                }
            }
        }

        // Display greetings for the first 30 seconds...
        if( m_Timer.GetAppTime() < 30.0 )
        {
            RenderGreetings();
        }
        else
        {
            BOOL fTracked = FALSE;
            
            for (UINT uCurrentSkeleton = 0; uCurrentSkeleton < NUI_SKELETON_COUNT; uCurrentSkeleton++)
            {
                if ( m_SkeletonFrame.SkeletonData[ uCurrentSkeleton ].eTrackingState == NUI_SKELETON_TRACKED)
                {
                    fTracked = TRUE;
                    break;
                }
            }
            
            if (!fTracked)
            {
                m_Font.Begin();
                m_Font.DrawText( m_d3dpp.BackBufferWidth / 2.0f, m_d3dpp.BackBufferHeight / 2.0f, 0xffffffff, 
                                 L"Please adjust your standing position,", ATGFONT_CENTER_X );          
                m_Font.DrawText( m_d3dpp.BackBufferWidth / 2.0f, m_d3dpp.BackBufferHeight / 2.0f + 30, 0xffffffff, 
                                 L"so that the camera captures you", ATGFONT_CENTER_X );
                m_Font.End();
            }
        }
 
        RenderTitleInfo( NULL );

        if( m_bShowGestureGuides )
        {
            for( INT i = 0; i < NUI_SKELETON_COUNT; ++ i )
            {
                RenderFlickRightGestureGuide( m_FlickRightGesture[ i ] );
                RenderStopGestureGuide( m_StopGesture[ i ] );
                RenderWaveGestureGuide( m_WaveGesture[ i ] );
            }
        }

    }

           
    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}

 
//--------------------------------------------------------------------------------------
// Name: Sample::RenderGreetings()
// Desc: Displays some quick tips on screen to help guide first timers.
//--------------------------------------------------------------------------------------
VOID Sample::RenderGreetings()
{
    m_Font.Begin();

    FLOAT fX = m_d3dpp.BackBufferWidth / 2.0f;
    FLOAT fY = m_SafeArea.y1 + 220.0f;

    m_Font.DrawText( fX, fY, 0xffffffff, 
                     L"A Few Quick Tips\n(See documentation for complete instructions)", ATGFONT_CENTER_X );          

    m_Font.DrawText( fX, fY + 70, 0xffffffff, 
                     L"This is an \"Air Circle\" competition\nUse your right hand to draw circle in the air and score points.", ATGFONT_CENTER_X );          

    m_Font.DrawText( fX, fY + 140, 0xffffffff, 
                     L"Raise both hands over your shoulders to\nbring up the \"Correct Your Identity\" UI.", ATGFONT_CENTER_X );    

    m_Font.DrawText( fX, fY + 210, 0xffffffff, 
                     L"Raise your right hand over your shoulder to\nbring up the \"Profile Sign-In\" UI.", ATGFONT_CENTER_X );    

    m_Font.DrawText( fX, fY + 280, 0xffffffff, 
                     L"Flick your right hand to the right to\nbring up the \"Player History\" UI.", ATGFONT_CENTER_X );

    m_Font.End();
}


//--------------------------------------------------------------------------------------
// Name: Sample::RenderPlayerHistoryUI()
// Desc: Display a list of all enrolled players and their stats
//--------------------------------------------------------------------------------------
VOID Sample::RenderPlayerHistoryUI()
{
    // grid metrics
    const FLOAT TopRow = m_SafeArea.y1 + 50.0f;
    const FLOAT RowHeight = ( m_SafeArea.y2 - TopRow ) / NUI_IDENTITY_MAX_ENROLLMENT_COUNT;

    const DWORD NUM_COLUMNS = 3;
    static const FLOAT s_ColumnWidths[ NUM_COLUMNS ] = { 250, 400, 150 };
    FLOAT fTotalWidth = 0;
    for( INT i = 0; i < NUM_COLUMNS; ++ i )
    {
        fTotalWidth += s_ColumnWidths[ i ];
    }    
    FLOAT fFirstColumn = ( m_d3dpp.BackBufferWidth - fTotalWidth ) / 2;

    // Draw grid
    XMFLOAT2 startPoint;
    XMFLOAT2 endPoint;
    startPoint.x = fFirstColumn;
    startPoint.y = TopRow;
    endPoint.x   = fFirstColumn;
    endPoint.y   = TopRow + NUI_IDENTITY_MAX_ENROLLMENT_COUNT * RowHeight;  
    ATG::DebugDraw::DrawScreenSpaceLine( startPoint, endPoint, D3DCOLOR_ARGB( 255, 255, 255, 255 ), 3 );      
    for( INT i = 0; i < NUM_COLUMNS; ++ i )
    {
        startPoint.x += s_ColumnWidths[ i ];
        endPoint.x = startPoint.x;
        ATG::DebugDraw::DrawScreenSpaceLine( startPoint, endPoint, D3DCOLOR_ARGB( 255, 255, 255, 255 ), 3 );      
    }

    startPoint.x = fFirstColumn;
    startPoint.y = TopRow;
    endPoint.x   = fFirstColumn + fTotalWidth;
    endPoint.y   = TopRow;  
    ATG::DebugDraw::DrawScreenSpaceLine( startPoint, endPoint, D3DCOLOR_ARGB( 255, 255, 255, 255 ), 3 );      
    for( DWORD row = 0; row < NUI_IDENTITY_MAX_ENROLLMENT_COUNT; ++ row )
    {
        m_Font.Begin();
        WCHAR wszBuf[ 256 ];

        BOOL bIsPlayerLogged = FALSE;
        if( m_PlayerInfo[ row ].dwEnrollmentCount > 0 )
        {
            D3DCOLOR color = m_PlayerInfo[ row ].Color;

            FLOAT fXOffset = fFirstColumn + 5;
            FLOAT fYOffset = startPoint.y + 5;
            m_Font.DrawText( fXOffset, startPoint.y, color, GetPreferredPlayerName( row ) );

            fXOffset += s_ColumnWidths[ 0 ];
            if( m_PlayerInfo[ row ].dwEnrollmentCount == 1 )
            {
                swprintf_s( wszBuf, L"Identified once" );
            }
            else
            {
                swprintf_s( wszBuf, L"Identified %d times", m_PlayerInfo[ row ].dwEnrollmentCount );
            }
            m_Font.DrawText( fXOffset, fYOffset, D3DCOLOR_ARGB( 255, 255, 255, 255 ), wszBuf );

            if( !bIsPlayerLogged )
            {
                swprintf_s( wszBuf, L"Seconds since stepped out: %0.0f", m_Timer.GetAbsoluteTime() - m_PlayerInfo[ row ].fInnactiveSince );
                m_Font.DrawText( fXOffset, fYOffset + 25, D3DCOLOR_ARGB( 255, 255, 255, 255 ), wszBuf );
            }

            fXOffset += s_ColumnWidths[ 1 ];
            swprintf_s( wszBuf, L"Circles: %d", m_PlayerInfo[ row ].dwAirCirclesCount );
            m_Font.DrawText( fXOffset, fYOffset, D3DCOLOR_ARGB( 255, 255, 255, 255 ), wszBuf ); 
 
            swprintf_s( wszBuf, L"Total: %d", m_PlayerInfo[ row ].dwAirCirclesTotal );
            m_Font.DrawText( fXOffset, fYOffset + 25, D3DCOLOR_ARGB( 255, 255, 255, 255 ), wszBuf ); 
        }
        m_Font.End();

        startPoint.y += RowHeight;
        endPoint.y = startPoint.y;
        ATG::DebugDraw::DrawScreenSpaceLine( startPoint, endPoint, D3DCOLOR_ARGB( 255, 255, 255, 255 ), 3 );       
    }
}

 
//--------------------------------------------------------------------------------------
// Name: RenderCorrectYourIdUI()
// Desc: Display the manual enrollment UI
//--------------------------------------------------------------------------------------
VOID Sample::RenderCorrectYourIdUI()
{
    const FLOAT SPACING = 10;
    const DWORD dwItemCount = CountEnrolledPlayers() + 1 ;

    // Determine the heigh of an item as a fraction of the total height available for display, making some allowance for space between 
    // the items and center the list vertically.
    FLOAT top = m_SafeArea.y1 + 50.0f;
    FLOAT itemHeight = ( m_SafeArea.y2 - top - ( NUI_IDENTITY_MAX_ENROLLMENT_COUNT ) * SPACING ) / ( NUI_IDENTITY_MAX_ENROLLMENT_COUNT + 1 );
    top += ( m_SafeArea.y2 - top - ( ( dwItemCount - 1 ) * SPACING + dwItemCount * itemHeight ) ) / 2;

    // Find what is the item currently selected, if there is a gesture in progress.
    BOOL bIsSelected = m_CorrectYourIdOptionList[ m_dwCorrectYourIdSkeletonInControl ].GetStatus() == GESTURE_INPROGRESS;
    DWORD dwSelection  = m_CorrectYourIdOptionList[ m_dwCorrectYourIdSkeletonInControl ].GetSelection();

    XMFLOAT2 size( ( m_d3dpp.BackBufferWidth ) / 2.0f, itemHeight );
    for( DWORD i = 0; i < dwItemCount; ++ i )
    {
        if( bIsSelected && dwSelection == i )
        {
            FLOAT fBrithness = m_CorrectYourIdOptionList[ m_dwCorrectYourIdSkeletonInControl ].GetProgress();
            ATG::DebugDraw::DrawScreenSpaceRect( XMFLOAT2( ( m_d3dpp.BackBufferWidth - size.x ) / 2, top + i * ( itemHeight + SPACING ) ), 
                                                 size, -1,  D3DXCOLOR( 1, fBrithness, fBrithness, 1 ) );
        }
        else
        {
            ATG::DebugDraw::DrawScreenSpaceRect( XMFLOAT2( ( m_d3dpp.BackBufferWidth - size.x ) / 2, top + i * ( itemHeight + SPACING ) ), 
                                                 size, -1,  D3DCOLOR_ARGB( 255, 0, 0, 255 ) );
        }
    }

    m_Font.Begin();
    DWORD dwCount = 0;
    for( DWORD i = 0; i < NUI_IDENTITY_MAX_ENROLLMENT_COUNT; ++ i )
    {
        if( m_PlayerInfo[ i ].dwEnrollmentCount > 0 )
        {
            m_Font.DrawText( ( m_d3dpp.BackBufferWidth - size.x ) / 2 + 150, 
                               top + dwCount * ( itemHeight + SPACING ), 
                               D3DCOLOR_ARGB( 255, 255, 255, 255 ), GetPreferredPlayerName( i ) );
            ++ dwCount;
        }
    }
    assert( dwCount == dwItemCount - 1 );
    m_Font.DrawText( ( m_d3dpp.BackBufferWidth - size.x ) / 2 + 150, 
                      top + ( dwItemCount - 1 ) * ( itemHeight + SPACING ), 
                      D3DCOLOR_ARGB( 255, 255, 255, 255 ), L"New player" );
    m_Font.End();

}

 
//--------------------------------------------------------------------------------------
// Name: RenderPauseMenuUI()
// Desc: Display the pause menu
//--------------------------------------------------------------------------------------
VOID Sample::RenderPauseMenuUI()
{
    const FLOAT SPACING = 10;
    const DWORD dwItemCount = 2;

    // Determine the heigh of an item as a fraction of the total height available for display, making some allowance for space between 
    // the items and center the list vertically.
    FLOAT top = m_SafeArea.y1 + 50.0f;
    FLOAT itemHeight = ( m_SafeArea.y2 - top - ( NUI_IDENTITY_MAX_ENROLLMENT_COUNT ) * SPACING ) / ( NUI_IDENTITY_MAX_ENROLLMENT_COUNT + 1 );
    top += ( m_SafeArea.y2 - top - ( ( dwItemCount - 1 ) * SPACING + dwItemCount * itemHeight ) ) / 2;

    // Find what is the item currently selected, if there is a gesture in progress.
    BOOL bIsSelected = m_PauseMenuOptionList[ m_dwPauseMenuSkeletonInControl ].GetStatus() == GESTURE_INPROGRESS;
    DWORD dwSelection  = m_PauseMenuOptionList[ m_dwPauseMenuSkeletonInControl ].GetSelection();

    XMFLOAT2 size( ( m_d3dpp.BackBufferWidth ) / 2.0f, itemHeight );
    for( DWORD i = 0; i < dwItemCount; ++ i )
    {
        if( bIsSelected && dwSelection == i )
        {
            FLOAT fBrithness = m_PauseMenuOptionList[ m_dwPauseMenuSkeletonInControl ].GetProgress();
            ATG::DebugDraw::DrawScreenSpaceRect( XMFLOAT2( ( m_d3dpp.BackBufferWidth - size.x ) / 2, top + i * ( itemHeight + SPACING ) ), 
                                                 size, -1,  D3DXCOLOR( 1, fBrithness, fBrithness, 1 ) );
        }
        else
        {
            ATG::DebugDraw::DrawScreenSpaceRect( XMFLOAT2( ( m_d3dpp.BackBufferWidth - size.x ) / 2, top + i * ( itemHeight + SPACING ) ), 
                                                 size, -1,  D3DCOLOR_ARGB( 255, 0, 0, 255 ) );
        }
    }

    m_Font.Begin();
    m_Font.DrawText( ( m_d3dpp.BackBufferWidth - size.x ) / 2 + 150, 
                       top, 
                       D3DCOLOR_ARGB( 255, 255, 255, 255 ), L"Open Guide" );
    m_Font.DrawText( ( m_d3dpp.BackBufferWidth - size.x ) / 2 + 150, 
                      top + itemHeight + SPACING, 
                      D3DCOLOR_ARGB( 255, 255, 255, 255 ), L"Resume Sample" );
    m_Font.End();

}


//--------------------------------------------------------------------------------------
// Name: Sample::RenderSkeleton()
// Desc: Renders the match stick skeleton associated to the player slot.
//--------------------------------------------------------------------------------------
VOID Sample::RenderSkeleton( DWORD dwPlayerSlotIndex )
{
    assert( dwPlayerSlotIndex < SKELETON_SLOT_COUNT );
    assert( m_SkeletonSlotInfo[ dwPlayerSlotIndex ].bIsInUse );
    assert( m_SkeletonSlotInfo[ dwPlayerSlotIndex ].dwSkeletonIndex < NUI_SKELETON_COUNT );

    DWORD dwSkeletonIndex = m_SkeletonSlotInfo[ dwPlayerSlotIndex ].dwSkeletonIndex;

    XMFLOAT2 ScreenSpaceJoints[ NUI_SKELETON_POSITION_COUNT ];
   
    struct BONE_JOINTS
    {
        NUI_SKELETON_POSITION_INDEX   StartJoint;
        NUI_SKELETON_POSITION_INDEX   EndJoint;
    };
  
 
    // Define the bones in the skeleton using joint indices
    static const BONE_JOINTS s_Bones[] =
    {
        // Head
        { NUI_SKELETON_POSITION_HEAD, NUI_SKELETON_POSITION_SHOULDER_CENTER },              // Top of head to top of neck

        // Right arm
        { NUI_SKELETON_POSITION_SHOULDER_CENTER, NUI_SKELETON_POSITION_SHOULDER_RIGHT },    // Neck bottom to right shoulder internal
        { NUI_SKELETON_POSITION_SHOULDER_RIGHT, NUI_SKELETON_POSITION_ELBOW_RIGHT },        // Right shoulder internal to right elbow
        { NUI_SKELETON_POSITION_ELBOW_RIGHT, NUI_SKELETON_POSITION_HAND_RIGHT },            // Right elbow to right wrist

        // Left arm
        { NUI_SKELETON_POSITION_SHOULDER_CENTER, NUI_SKELETON_POSITION_SHOULDER_LEFT },     // Neck bottom to left shoulder internal
        { NUI_SKELETON_POSITION_SHOULDER_LEFT, NUI_SKELETON_POSITION_ELBOW_LEFT },          // Left shoulder internal to left elbow
        { NUI_SKELETON_POSITION_ELBOW_LEFT, NUI_SKELETON_POSITION_HAND_LEFT },              // Left elbow to left wrist

        // Right leg and foot
        { NUI_SKELETON_POSITION_HIP_RIGHT, NUI_SKELETON_POSITION_KNEE_RIGHT },              // Right hip internal to right knee
        { NUI_SKELETON_POSITION_KNEE_RIGHT, NUI_SKELETON_POSITION_ANKLE_RIGHT },             // Right knee to right ankle

        // Left leg and foot
        { NUI_SKELETON_POSITION_HIP_LEFT, NUI_SKELETON_POSITION_KNEE_LEFT },                // Left hip internal to left knee
        { NUI_SKELETON_POSITION_KNEE_LEFT, NUI_SKELETON_POSITION_ANKLE_LEFT },               // Left knee to left ankle

        // Spine
        { NUI_SKELETON_POSITION_SHOULDER_CENTER, NUI_SKELETON_POSITION_SPINE },             // Neck bottom to spine
        { NUI_SKELETON_POSITION_SPINE, NUI_SKELETON_POSITION_HIP_CENTER },                  // Spine to hip center

        // Hips
        { NUI_SKELETON_POSITION_HIP_RIGHT, NUI_SKELETON_POSITION_HIP_CENTER },              // Right hip to hip center
        { NUI_SKELETON_POSITION_HIP_CENTER, NUI_SKELETON_POSITION_HIP_LEFT }                // Hip center to left hip
    };

    const DWORD dwNumBones = ARRAYSIZE( s_Bones );
  
    ProjectToScreen( m_SkeletonFrame.SkeletonData[ dwSkeletonIndex ].SkeletonPositions, NUI_SKELETON_POSITION_COUNT, ScreenSpaceJoints );

    // Draw each bone in the skeleton using the screen space joints
    D3DCOLOR SkeletonColor = GetPlayerColor( dwPlayerSlotIndex );
    for ( DWORD i = 0; i < dwNumBones; i++ )
    {
        // Use the minimum joint confidence for the bone confidence
        NUI_SKELETON_POSITION_TRACKING_STATE JointConfidence = 
            m_SkeletonFrame.SkeletonData[ dwSkeletonIndex ].eSkeletonPositionTrackingState[ s_Bones[ i ].StartJoint ];

        D3DXCOLOR color[2];
        if ( JointConfidence == NUI_SKELETON_POSITION_NOT_TRACKED )
        {
            continue;
        }
        else
        {
            color[0] = SkeletonColor;
        }

        JointConfidence = m_SkeletonFrame.SkeletonData[ dwSkeletonIndex ].eSkeletonPositionTrackingState[ s_Bones[ i ].EndJoint ];
        if ( JointConfidence == NUI_SKELETON_POSITION_NOT_TRACKED )
        {
            continue;
        }
        else
        {
            color[1] = SkeletonColor;
        }

        XMFLOAT2 vPntArray[ 2 ];
        vPntArray[ 0 ].x = (FLOAT)ScreenSpaceJoints[ s_Bones[ i ].StartJoint ].x;
        vPntArray[ 0 ].y = (FLOAT)ScreenSpaceJoints[ s_Bones[ i ].StartJoint ].y;
        vPntArray[ 1 ].x = (FLOAT)ScreenSpaceJoints[ s_Bones[ i ].EndJoint ].x;
        vPntArray[ 1 ].y = (FLOAT)ScreenSpaceJoints[ s_Bones[ i ].EndJoint ].y;

        ATG::DebugDraw::DrawScreenSpaceLine( vPntArray[ 0 ], color[ 0 ], vPntArray[ 1 ], color[ 1 ], 5 );
    }
}

 
//--------------------------------------------------------------------------------------
// Name: Sample::RenderPlayerInfo()
// Desc: Displays the specific info about the player associated with dwPlayerSlotIndex.
//--------------------------------------------------------------------------------------
VOID Sample::RenderPlayerInfo( DWORD dwPlayerSlotIndex )
{
    assert( dwPlayerSlotIndex < SKELETON_SLOT_COUNT );

    // Compute origin and size of the info area for this specific player.
    const FLOAT spacing = m_SafeArea.x1 / 2.0f; // Spacing between player info display areas.
    XMFLOAT2 size( ( ( m_SafeArea.x2 - m_SafeArea.x1 ) - spacing * ( SKELETON_SLOT_COUNT - 1 ) ) / SKELETON_SLOT_COUNT, 100.0f );
    XMFLOAT2 origin( m_SafeArea.x1 + dwPlayerSlotIndex * ( size.x + spacing ), m_SafeArea.y1 + 85.0f );

    D3DCOLOR SkeletonColor = GetPlayerColor( dwPlayerSlotIndex );

    // Draw info area delimiters
    ATG::DebugDraw::DrawScreenSpaceRect( origin, size, 3, SkeletonColor );


    // Display information specific to this player.
    m_Font.Begin();

    m_Font.DrawText( origin.x + 205, origin.y, SkeletonColor, GetSkeletonStatusText( dwPlayerSlotIndex ) );
  
    if( m_SkeletonSlotInfo[ dwPlayerSlotIndex ].bIsInUse )
    {
        assert( m_SkeletonSlotInfo[ dwPlayerSlotIndex ].dwSkeletonIndex < NUI_SKELETON_COUNT );

        DWORD dwSkeletonIndex = m_SkeletonSlotInfo[ dwPlayerSlotIndex ].dwSkeletonIndex;
 
        if( m_SkeletonFrame.SkeletonData[ dwSkeletonIndex ].eTrackingState == NUI_SKELETON_TRACKED )
        {
            if( m_SkeletonFrame.SkeletonData[ dwSkeletonIndex ].dwEnrollmentIndex < NUI_IDENTITY_MAX_ENROLLMENT_COUNT )
            {
                    m_Font.DrawText( origin.x + 5, origin.y, SkeletonColor,
                                     GetPreferredPlayerName( m_SkeletonFrame.SkeletonData[ dwSkeletonIndex ].dwEnrollmentIndex ) ); 

                WCHAR wszBuf[ 256 ];
                swprintf_s( wszBuf, L"Circles drawn: %d", m_PlayerInfo[ m_SkeletonFrame.SkeletonData[ dwSkeletonIndex ].dwEnrollmentIndex ].dwAirCirclesCount );
                m_Font.DrawText( origin.x + 5, origin.y + 50, SkeletonColor, wszBuf ); 
 
                swprintf_s( wszBuf, L"Total circles drawn: %d", m_PlayerInfo[ m_SkeletonFrame.SkeletonData[ dwSkeletonIndex ].dwEnrollmentIndex ].dwAirCirclesTotal );
                m_Font.DrawText( origin.x + 205, origin.y + 50, SkeletonColor, wszBuf ); 
             }
        }  
    }
    m_Font.End();
}

 
//--------------------------------------------------------------------------------------
// Name: Sample::RenderPlayerMessage()
// Desc: Displays a message (identification troubleshooting, greetings, etc) to the 
//       player, when appropriate.
//--------------------------------------------------------------------------------------
VOID Sample::RenderPlayerMessage( DWORD dwPlayerSlotIndex )
{
    assert( dwPlayerSlotIndex < SKELETON_SLOT_COUNT );

    // Compute origin and size of the info area for this specific player.
    const FLOAT spacing = m_SafeArea.x1 / 2.0f; // Spacing between player info display areas.
    XMFLOAT2 size( ( ( m_SafeArea.x2 - m_SafeArea.x1 ) - spacing * ( NUI_SKELETON_MAX_TRACKED_COUNT - 1 ) ) / NUI_SKELETON_MAX_TRACKED_COUNT, 50.0f );
    XMFLOAT2 origin( m_SafeArea.x1 + dwPlayerSlotIndex * ( size.x + spacing ), m_SafeArea.y1 + 30.0f );

    D3DCOLOR SkeletonColor = GetPlayerColor( dwPlayerSlotIndex );

    // Draw info area delimiters
    ATG::DebugDraw::DrawScreenSpaceRect( origin, size, 3, SkeletonColor );

    if( !m_SkeletonSlotInfo[ dwPlayerSlotIndex ].bIsInUse )
        return;

    assert( m_SkeletonSlotInfo[ dwPlayerSlotIndex ].dwSkeletonIndex < NUI_SKELETON_COUNT );
    DWORD dwSkeletonIndex = m_SkeletonSlotInfo[ dwPlayerSlotIndex ].dwSkeletonIndex;


    if( ( m_SkeletonIdentityInfo[ dwSkeletonIndex ].bIsIdentifying || m_SkeletonIdentityInfo[ dwSkeletonIndex ].bIsEnrolling ) && 
          m_SkeletonIdentityInfo[ dwSkeletonIndex ].dwQualityFlags )
    {
        if( m_MessageTimer[ dwSkeletonIndex ].GetAppTime() > 15.0f )
            return;

        m_Font.DrawText( origin.x + 5, origin.y, SkeletonColor,
                         ATG::GetIdentityQualityFlagPrompt( m_SkeletonIdentityInfo[ dwSkeletonIndex ].dwQualityFlags ) );
    }
    else if( ( m_SkeletonIdentityInfo[ dwSkeletonIndex ].bIsIdentifying || m_SkeletonIdentityInfo[ dwSkeletonIndex ].bIsEnrolling ) && 
               m_SkeletonIdentityInfo[ dwSkeletonIndex ].hrLastResult != S_OK )
    {
        if( m_MessageTimer[ dwSkeletonIndex ].GetAppTime() > 15.0f )
            return;

        m_Font.DrawText( origin.x + 5, origin.y, SkeletonColor,
                         GetLastResultText( m_SkeletonIdentityInfo[ dwSkeletonIndex ].hrLastResult ) );
    }
    else if( m_SkeletonFrame.SkeletonData[ dwSkeletonIndex ].eTrackingState == NUI_SKELETON_TRACKED &&
             m_SkeletonFrame.SkeletonData[ dwSkeletonIndex ].dwEnrollmentIndex == NUI_IDENTITY_ENROLLMENT_INDEX_UNKNOWN )
    {
        m_Font.DrawText( origin.x + 5, origin.y, SkeletonColor,
                         L"Draw a circle in the air,\nwith your right hand to Enroll!" );
    }
    else if( m_SkeletonFrame.SkeletonData[ dwSkeletonIndex ].eTrackingState == NUI_SKELETON_TRACKED &&
             m_SkeletonFrame.SkeletonData[ dwSkeletonIndex ].dwEnrollmentIndex < NUI_IDENTITY_MAX_ENROLLMENT_COUNT )
    {     

// ELx       assert( m_PlayerInfo[ m_SkeletonFrame.SkeletonData[ dwSkeletonIndex ].dwEnrollmentIndex ].dwEnrollmentCount != 0 );

        WCHAR wszBuf[ 256 ];
        if( !m_PlayerInfo[ m_SkeletonFrame.SkeletonData[ dwSkeletonIndex ].dwEnrollmentIndex  ].bHasShownIntentToPlay )
        {
            swprintf_s( wszBuf, L"Draw a circle in the air,\nwith your right hand to Play!" );
        }
        else if( m_PlayerInfo[ m_SkeletonFrame.SkeletonData[ dwSkeletonIndex ].dwEnrollmentIndex ].dwEnrollmentCount == 1 )
        {
            if( m_MessageTimer[ dwSkeletonIndex ].GetAppTime() > 15.0f )
                return;

            swprintf_s( wszBuf, L"Hello %s", 
                        GetPreferredPlayerName( m_SkeletonFrame.SkeletonData[ dwSkeletonIndex ].dwEnrollmentIndex ) );
        }
        else
        {
            if( m_MessageTimer[ dwSkeletonIndex ].GetAppTime() > 15.0f )
                return;

            swprintf_s( wszBuf, L"Welcome back %s", 
                        GetPreferredPlayerName( m_SkeletonFrame.SkeletonData[ dwSkeletonIndex ].dwEnrollmentIndex ) );
        }

        m_Font.DrawText( origin.x + 5, origin.y, SkeletonColor, wszBuf );
    }

}


//--------------------------------------------------------------------------------------
// Name: Sample::RenderTitleInfo()
// Desc: Displays the sample title appending any text in passed in szMode along with the
//       FPS and the picutre in picture video streams and skeletons.
//--------------------------------------------------------------------------------------
VOID Sample::RenderTitleInfo( const WCHAR* pwszMode )
{
    m_Font.Begin();

    // Draw sample ame and any text in pwszMode.
    m_Font.SetScaleFactors( 1.2f, 1.2f );
    if( pwszMode == NULL )
    {
        m_Font.DrawText( (FLOAT)m_SafeArea.x1, (FLOAT)m_SafeArea.y1, 0xffffffff, L"Identity" );
    }
    else
    {
        WCHAR wszBuf[ 256 ];
        swprintf_s( wszBuf,L"Identity - %s", pwszMode );
        m_Font.DrawText( (FLOAT)m_SafeArea.x1, (FLOAT)m_SafeArea.y1, 0xffffffff,  wszBuf );
    }

    // Draw FPS.
    m_Font.SetScaleFactors( 1.0f, 1.0f );
    m_Font.DrawText( (FLOAT)m_SafeArea.x2, (FLOAT)m_SafeArea.y1, 0xffffff00, m_Timer.GetFrameRate(), ATGFONT_RIGHT );

    m_Font.End();

    // Draw the raw depth and image map with both skeletons overlaid.
    static const FLOAT s_fDrawWidth = 200.0f;
    static const FLOAT s_fDrawHeight = s_fDrawWidth * 3.0f / 4.0f; // 4:3 aspect ratio.
    static const FLOAT s_fDrawX = 25.0f;
    static const FLOAT s_fDrawY = (FLOAT)m_SafeArea.y2 - s_fDrawHeight;

    m_pip.BeginRender();
    m_pip.RenderColorStream( s_fDrawX, s_fDrawY, s_fDrawWidth, s_fDrawHeight );
    m_pip.RenderSkeletons( s_fDrawX, s_fDrawY, s_fDrawWidth, s_fDrawHeight, TRUE );
    m_pip.EndRender();
}

 
//--------------------------------------------------------------------------------------
// Name: Sample::RenderFlickLeftGestureGuide()
// Desc: Displays helper guides for the flick left gesture.
//--------------------------------------------------------------------------------------
VOID Sample::RenderFlickLeftGestureGuide( const FlickLeftGesture& flickLeftGesture )
{
    if( ( flickLeftGesture.GetStatus() ) == GESTURE_INPROGRESS )
    {
        XMFLOAT2 projectedPoints[ 2 ];

        XMVECTOR vBounds[ 2 ];
        flickLeftGesture.GetBounds( vBounds );
        ProjectToScreen( vBounds, 2, projectedPoints );
        D3DRECT boundingRect = { (LONG)projectedPoints[ 0 ].x, (LONG)projectedPoints[ 0 ].y, 
                                 (LONG)projectedPoints[ 1 ].x, (LONG)projectedPoints[ 1 ].y };
        ATG::DebugDraw::DrawScreenSpaceRect( boundingRect, 3, D3DXCOLOR( 1, 0, 0, 1 ) );

        ATG::DebugDraw::DrawScreenSpaceLine( *ProjectToScreen( flickLeftGesture.GetStartPosition(), &projectedPoints[ 0 ] ),
                                             *ProjectToScreen( flickLeftGesture.GetCurrentPosition(), &projectedPoints[ 1 ] ),
                                             D3DXCOLOR( 1, 0, 0, 1 ), 3 );
    }
}

 
//--------------------------------------------------------------------------------------
// Name: Sample::RenderFlickRightGestureGuide()
// Desc: Displays helper guides for the flick right gesture.
//--------------------------------------------------------------------------------------
VOID Sample::RenderFlickRightGestureGuide( const FlickRightGesture& flickRightGesture )
{
    if( ( flickRightGesture.GetStatus() ) == GESTURE_INPROGRESS )
    {
        XMFLOAT2 projectedPoints[ 2 ];

        XMVECTOR vBounds[ 2 ];
        flickRightGesture.GetBounds( vBounds );
        ProjectToScreen( vBounds, 2, projectedPoints );
        D3DRECT boundingRect = { (LONG)projectedPoints[ 0 ].x, (LONG)projectedPoints[ 0 ].y, 
                                 (LONG)projectedPoints[ 1 ].x, (LONG)projectedPoints[ 1 ].y };
        ATG::DebugDraw::DrawScreenSpaceRect( boundingRect, 3, D3DXCOLOR( 1, 0, 0, 1 ) );

        ATG::DebugDraw::DrawScreenSpaceLine( *ProjectToScreen( flickRightGesture.GetStartPosition(), &projectedPoints[ 0 ] ),
                                             *ProjectToScreen( flickRightGesture.GetCurrentPosition(), &projectedPoints[ 1 ] ),
                                             D3DXCOLOR( 1, 0, 0, 1 ), 3 );
    }
}

 
//--------------------------------------------------------------------------------------
// Name: Sample::RenderListSelectionGestureGuide()
// Desc: Displays helper guides for the list selection gesture.
//--------------------------------------------------------------------------------------
VOID Sample::RenderListSelectionGestureGuide( const ListSelectionGesture& listSelectionGesture )
{
    if( ( listSelectionGesture.GetStatus() ) == GESTURE_INPROGRESS )
    {
        XMFLOAT2 projectedPoints[ 2 ];

        XMVECTOR vBounds[ 2 ];
        listSelectionGesture.GetBounds( vBounds );
        ProjectToScreen( vBounds, 2, projectedPoints );
        D3DRECT boundingRect = { (LONG)projectedPoints[ 0 ].x, (LONG)projectedPoints[ 0 ].y, 
                                 (LONG)projectedPoints[ 1 ].x, (LONG)projectedPoints[ 1 ].y };
        ATG::DebugDraw::DrawScreenSpaceRect( boundingRect, 3, D3DXCOLOR( 1, 0, 0, 1 ) );

        listSelectionGesture.GetBoundsForSelection( vBounds );
        ProjectToScreen( vBounds, 2, projectedPoints );
        D3DRECT boundingRectSel = { (LONG)projectedPoints[ 0 ].x, (LONG)projectedPoints[ 0 ].y, 
                                    (LONG)projectedPoints[ 1 ].x, (LONG)projectedPoints[ 1 ].y };
        FLOAT fBrithness = listSelectionGesture.GetProgress();
        ATG::DebugDraw::DrawScreenSpaceRect( boundingRectSel, 3, D3DXCOLOR( 1, fBrithness, fBrithness, 1 ) );

        ATG::DebugDraw::DrawScreenSpaceLine( *ProjectToScreen( listSelectionGesture.GetStartPosition(), &projectedPoints[ 0 ] ),
                                             *ProjectToScreen( listSelectionGesture.GetCurrentPosition(), &projectedPoints[ 1 ] ),
                                             D3DXCOLOR( 1, 0, 0, 1 ), 3 );
    }
}

 
//--------------------------------------------------------------------------------------
// Name: Sample::RenderStopGestureGuide()
// Desc: Displays helper guides for the stop gesture.
//--------------------------------------------------------------------------------------
VOID Sample::RenderStopGestureGuide( const StopGesture& stopGesture )
{
    if( ( stopGesture.GetStatus() ) == GESTURE_INPROGRESS )
    {
        XMVECTOR vBounds[ 2 ];
        XMFLOAT2 projectedPoints[ 2 ];
        D3DRECT boundingRect;

        stopGesture.GetBoundsLeftHand( vBounds );
        ProjectToScreen( vBounds, 2, projectedPoints );
        boundingRect.x1 = (LONG)projectedPoints[ 0 ].x;
        boundingRect.y1 = (LONG)projectedPoints[ 0 ].y;
        boundingRect.x2 = (LONG)projectedPoints[ 1 ].x;
        boundingRect.y2 = (LONG)projectedPoints[ 1 ].y;
        ATG::DebugDraw::DrawScreenSpaceRect( boundingRect, 3, D3DXCOLOR( 1, 0, 0, 1 ) );

        ATG::DebugDraw::DrawScreenSpaceLine( *ProjectToScreen( stopGesture.GetStartPositionLeftHand(), &projectedPoints[ 0 ] ),
                                             *ProjectToScreen( stopGesture.GetCurrentPositionLeftHand(), &projectedPoints[ 1 ] ),
                                             D3DXCOLOR( 1, 0, 0, 1 ), 3 );

        stopGesture.GetBoundsRightHand( vBounds );
        ProjectToScreen( vBounds, 2, projectedPoints );
        boundingRect.x1 = (LONG)projectedPoints[ 0 ].x;
        boundingRect.y1 = (LONG)projectedPoints[ 0 ].y;
        boundingRect.x2 = (LONG)projectedPoints[ 1 ].x;
        boundingRect.y2 = (LONG)projectedPoints[ 1 ].y;
        ATG::DebugDraw::DrawScreenSpaceRect( boundingRect, 3, D3DXCOLOR( 1, 0, 0, 1 ) );

        ATG::DebugDraw::DrawScreenSpaceLine( *ProjectToScreen( stopGesture.GetStartPositionRightHand(), &projectedPoints[ 0 ] ),
                                             *ProjectToScreen( stopGesture.GetCurrentPositionRightHand(), &projectedPoints[ 1 ] ),
                                             D3DXCOLOR( 1, 0, 0, 1 ), 3 );
    }
}

 
//--------------------------------------------------------------------------------------
// Name: Sample::RenderWaveGestureGuide()
// Desc: Displays helper guides for the stop gesture.
//--------------------------------------------------------------------------------------
VOID Sample::RenderWaveGestureGuide( const WaveGesture& waveGesture )
{
    if( ( waveGesture.GetStatus() ) == GESTURE_INPROGRESS )
    {
        XMVECTOR vBounds[ 2 ];
        XMFLOAT2 projectedPoints[ 2 ];
        D3DRECT boundingRect;

        waveGesture.GetBounds( vBounds );
        ProjectToScreen( vBounds, 2, projectedPoints );
        boundingRect.x1 = (LONG)projectedPoints[ 0 ].x;
        boundingRect.y1 = (LONG)projectedPoints[ 0 ].y;
        boundingRect.x2 = (LONG)projectedPoints[ 1 ].x;
        boundingRect.y2 = (LONG)projectedPoints[ 1 ].y;
        ATG::DebugDraw::DrawScreenSpaceRect( boundingRect, 3, D3DXCOLOR( 1, 0, 0, 1 ) );

        ATG::DebugDraw::DrawScreenSpaceLine( *ProjectToScreen( waveGesture.GetStartPosition(), &projectedPoints[ 0 ] ),
                                             *ProjectToScreen( waveGesture.GetCurrentPosition(), &projectedPoints[ 1 ] ),
                                             D3DXCOLOR( 1, 0, 0, 1 ), 3 );
    }
}


//--------------------------------------------------------------------------------------
// Name: Sample::RenderAirCirclesGestureGuide()
// Desc: Displays helper guides for the rub gesture
//--------------------------------------------------------------------------------------
VOID Sample::RenderAirCirclesGestureGuide( const AirCirclesGesture& airCirclesGesture )
{
    if( ( airCirclesGesture.GetStatus() ) == GESTURE_INPROGRESS )
    {
        XMVECTOR vPoints[ 2 ];

        FLOAT fInnerRadius;
        FLOAT fOuterRadius;
        airCirclesGesture.GetBounds( &vPoints[ 0 ], &fInnerRadius, &fOuterRadius );

        vPoints[ 1 ] = airCirclesGesture.GetCurrentPosition();
        XMFLOAT2 projectedPoints[ 2 ];
        ProjectToScreen( vPoints, 2, projectedPoints );

        FLOAT fBrithness = airCirclesGesture.GetProgress();
        ATG::DebugDraw::DrawScreenSpaceLine( projectedPoints[ 0 ], projectedPoints[ 1 ], D3DXCOLOR( 1, fBrithness, fBrithness, 1 ), 3 );
    }
}


//--------------------------------------------------------------------------------------
// Name: Sample::GetPlayerColor()
// Desc: Return the color associated to a specific player slot.
//--------------------------------------------------------------------------------------
D3DCOLOR Sample::GetPlayerColor( DWORD dwPlayerSlotIndex )
{
    assert( dwPlayerSlotIndex < SKELETON_SLOT_COUNT );

    if( m_SkeletonSlotInfo[ dwPlayerSlotIndex ].bIsInUse &&
        m_SkeletonFrame.SkeletonData[ m_SkeletonSlotInfo[ dwPlayerSlotIndex ].dwSkeletonIndex ].eTrackingState == NUI_SKELETON_TRACKED && 
        m_SkeletonFrame.SkeletonData[ m_SkeletonSlotInfo[ dwPlayerSlotIndex ].dwSkeletonIndex ].dwEnrollmentIndex < NUI_IDENTITY_MAX_ENROLLMENT_COUNT )
    {
        return m_PlayerInfo[ m_SkeletonFrame.SkeletonData[ m_SkeletonSlotInfo[ dwPlayerSlotIndex ].dwSkeletonIndex ].dwEnrollmentIndex ].Color;
    }
    else
    {
        switch( dwPlayerSlotIndex )
        {
            case 0:
                return 0xff71719d;

            case 1:
                return 0xff7e7eaf;

            default:
                assert( false );
                return 0xff7e7eaf;
        }
    }
}

//--------------------------------------------------------------------------------------
// Name: Sample::GetPreferredPlayerName()
// Desc: Return the player's GamerTag if available. Otherwise returns the player name.
//--------------------------------------------------------------------------------------
WCHAR* Sample::GetPreferredPlayerName( DWORD dwEnrollmentIndex )
{
    assert( dwEnrollmentIndex < NUI_IDENTITY_MAX_ENROLLMENT_COUNT );

    if( m_PlayerInfo[ dwEnrollmentIndex ].wszGamerTag[ 0 ] != L'\0' )
    {
        return m_PlayerInfo[ dwEnrollmentIndex ].wszGamerTag;
    }
    else
    {
        return m_PlayerInfo[ dwEnrollmentIndex ].wszPlayerName;
    }
}

//--------------------------------------------------------------------------------------
// Name: Sample::IsPlayerSignedIn()
// Desc: Return TRUE or FALSE to indicate if the player is signed-in to a profile.
//--------------------------------------------------------------------------------------
BOOL Sample::IsPlayerSignedIn( DWORD dwEnrollmentIndex )
{
    assert( dwEnrollmentIndex < NUI_IDENTITY_MAX_ENROLLMENT_COUNT );

    if( m_PlayerInfo[ dwEnrollmentIndex ].wszGamerTag[ 0 ] != L'\0' )
    {
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}


//--------------------------------------------------------------------------------------
// Name: Sample::CountEnrolledPlayers()
// Desc: Determines how many PLAYER_INFO structures have been filled
//--------------------------------------------------------------------------------------
DWORD Sample::CountEnrolledPlayers() const
{
    DWORD nCount = 0;

    for( INT i = 0; i < NUI_IDENTITY_MAX_ENROLLMENT_COUNT; ++ i )
    {
        if( m_PlayerInfo[ i ].dwEnrollmentCount > 0 )
        {
            ++ nCount;
       }
    }

    return nCount;
}


//--------------------------------------------------------------------------------------
// Name: Sample::CountEnrolledSkeletons()
// Desc: Determines how many of the skeletons being tracked have been enrolled.
//--------------------------------------------------------------------------------------
DWORD Sample::CountEnrolledSkeletons() const
{
    DWORD nCount = 0;

    for( INT i = 0; i < NUI_SKELETON_COUNT; ++ i )
    {
        if( m_SkeletonFrame.SkeletonData[ i ].eTrackingState == NUI_SKELETON_TRACKED &&
            m_SkeletonFrame.SkeletonData[ i ].dwEnrollmentIndex >= 0 &&
            m_SkeletonFrame.SkeletonData[ i ].dwEnrollmentIndex < NUI_IDENTITY_MAX_ENROLLMENT_COUNT )
        {
            ++ nCount;
       }
    }

    return nCount;
}


//--------------------------------------------------------------------------------------
// Name: Sample::GetSkeletonIndexFromTrackingId()
// Desc: Return the skeleton index corresponding to a specific tracking ID or E_FAIL if 
//       a matching skeleton is not found.
//--------------------------------------------------------------------------------------
DWORD Sample::GetSkeletonIndexFromTrackingId( DWORD dwTrackingID ) const
{
    for( DWORD i = 0; i < NUI_SKELETON_COUNT; ++ i )
    {
        if( m_SkeletonFrame.SkeletonData[ i ].dwTrackingID == dwTrackingID )
        {
                return i;
        }
    }

    return ( DWORD )E_FAIL;
}


//--------------------------------------------------------------------------------------
// Name: Sample::GetSkeletonIndexFromTrackingIdForAsyncOps()
// Desc: Return the skeleton index that was associated to a specific tracking ID at the 
//       begining of the last async operation launched on the skeleton. It returns E_FAIL
//       if a matching skeleton is not found.
//--------------------------------------------------------------------------------------
DWORD Sample::GetSkeletonIndexFromTrackingIdForAsyncOps( DWORD dwTrackingID ) const
{
    for( DWORD i = 0; i < NUI_SKELETON_COUNT; ++ i )
    {
        if( m_adwAsyncOpTrackingID[ i ] == dwTrackingID )
        {
                return i;
        }
    }

    return ( DWORD )E_FAIL;
}

//--------------------------------------------------------------------------------------
// Name: Sample::ProjectToScreen()
// Desc: Projects the specified XMVECTOR from skeletal space into 2D screen space. For
//       your convenience, a version that takes an array of XMVECTOR objects is
//       also available.
//--------------------------------------------------------------------------------------
XMFLOAT2* Sample::ProjectToScreen( const XMVECTOR& vSource, XMFLOAT2* pProjected ) const
{
    FLOAT fX;
    FLOAT fY;
    NuiTransformSkeletonToDepthImage(vSource, &fX, &fY);
    pProjected->x = fX * m_fDepthDisplayScaleX;
    pProjected->y = fY * m_fDepthDisplayScaleY;
    return pProjected;
}
 
 
//--------------------------------------------------------------------------------------
// Name: Sample::ProjectToScreen()
// Desc: Projects the points specified in the XMVECTOR array from skeletal space into 2D
//       screen space.
//--------------------------------------------------------------------------------------
XMFLOAT2* Sample::ProjectToScreen( const XMVECTOR vSourceArray[], DWORD dwDataCount, XMFLOAT2 pProjectedArray[] ) const
{
    FLOAT fX;
    FLOAT fY;
    for( DWORD i = 0; i < dwDataCount; ++ i )
    {
        NuiTransformSkeletonToDepthImage(
            vSourceArray[i],
            &fX,
            &fY
            );

        pProjectedArray[ i ].x = fX * m_fDepthDisplayScaleX;
        pProjectedArray[ i ].y = fY * m_fDepthDisplayScaleY;
    }

    return pProjectedArray;
}

 
//--------------------------------------------------------------------------------------
// Name: Sample::GetSkeletonStatusText
// Desc: Returns a text string describing the current status for the specified skeleton.
//--------------------------------------------------------------------------------------
const WCHAR* Sample::GetSkeletonStatusText( DWORD dwPlayerSlotIndex ) const
{
    assert( dwPlayerSlotIndex < SKELETON_SLOT_COUNT );

    if( !m_SkeletonSlotInfo[ dwPlayerSlotIndex ].bIsInUse )
    {
        return L"Locating...";
    }
    
    assert( m_SkeletonSlotInfo[ dwPlayerSlotIndex ].dwSkeletonIndex < NUI_SKELETON_COUNT );
    DWORD dwSkeletonIndex = m_SkeletonSlotInfo[ dwPlayerSlotIndex ].dwSkeletonIndex;
    switch( m_SkeletonFrame.SkeletonData[ dwSkeletonIndex ].dwEnrollmentIndex )
    {
        case NUI_IDENTITY_ENROLLMENT_INDEX_BUSY:
        {
            if( m_SkeletonIdentityInfo[ dwSkeletonIndex ].bIsIdentifying )
            {
                return L"Identifying...";
            }

            if( m_SkeletonIdentityInfo[ dwSkeletonIndex ].bIsEnrolling )
            {
                return L"Enrolling...";
            }
        
            return L"Busy...";
        }
    
        case NUI_IDENTITY_ENROLLMENT_INDEX_CALL_IDENTIFY:
        {
            return L"Identifying...";
        }

        case NUI_IDENTITY_ENROLLMENT_INDEX_UNKNOWN:
        {
            return L"Identified as unknown" ;
        }

        case NUI_IDENTITY_ENROLLMENT_INDEX_FAILURE:
        {
            return L"Identification failed";
        }

        default:
        {
            assert( m_SkeletonFrame.SkeletonData[ dwSkeletonIndex ].dwEnrollmentIndex  < NUI_IDENTITY_MAX_ENROLLMENT_COUNT );

            if( m_SkeletonFrame.SkeletonData[ dwSkeletonIndex ].dwUserIndex < XUSER_MAX_COUNT )
            {
                return L"Signed-In";
            }

            return L"Enrolled";
        }
    }
}
 
 
//--------------------------------------------------------------------------------------
// Name: GetLastResultText
// Desc: Return a text string explaining the return code form the last identity 
//       operation for the specified player.
//--------------------------------------------------------------------------------------
const WCHAR* GetLastResultText( HRESULT hrLastResult )
{
    struct IDENTITY_RESULT_STRING
    {
        HRESULT hrLastResult;
        WCHAR*  pwszText;
    };

    static const IDENTITY_RESULT_STRING s_QualityStringTable[] =
    {
        { S_OK,                       L"Succeeded!" },
        { E_INVALIDARG,               L"Couldn't proceed with enrollment." },
        { E_ABORT,                    L"The operation was aborted by the player." },
        { E_NUI_IDENTITY_BUSY,        L"The operation was aborted by the system.\nHas a player opened the guide?" },
        { E_NUI_IDENTITY_LOST_TRACK,  L"Tracking was lost." },
        { E_NUI_DEVICE_NOT_CONNECTED, L"The Natural User Input sensor array is not connected." },
        { E_NUI_IDENTITY_UI_REQUIRED, L"Couldn't proceed with enrollment." },
        { E_NUI_DEVICE_NOT_READY,     L"The device is not ready." }
    };

    for( DWORD i = 0; i < ARRAYSIZE( s_QualityStringTable ); ++ i )
    {
        if ( hrLastResult ==  s_QualityStringTable[i].hrLastResult )
        {
            return s_QualityStringTable[ i ].pwszText;
        }
    }

    assert( false ); //Unknown flag...
    return L"";
}
