//--------------------------------------------------------------------------------------
// SimpleIdentity.cpp
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
#include <AtgNuiMenu.h>
#include <AtgNuiVisualization.h>
#include <AtgSimpleShaders.h>
#include <AtgUtil.h>


#include "SinglePlayerIdentityManager.h"
#include "KinectSensor.h"


//--------------------------------------------------------------------------------------
// Name: Buffer sizes
// Desc: Define sizes for various buffers.
//--------------------------------------------------------------------------------------
const DWORD STRING_MESSAGE_SIZE = 128;


//--------------------------------------------------------------------------------------
// Name: enum SCENE
// Desc: List of scenes the player will go through while using the sample.
//--------------------------------------------------------------------------------------
enum SCENE
{
    SCENE_INTENT,      // Captures a player's intent to play
    SCENE_IDENTIFYING, // Provides feedback to the player during identification
    SCENE_LOBBY,       // Single player lobby allows player confirm his or her identity and 
                       // select the type of game to play
    SCENE_GAME         // Shows the type game being played and player status.
};

//--------------------------------------------------------------------------------------
// Name: enum SIGNIN_STATE
// Desc: For XN_SYS_SIGNINCHANGED, handle the case where the system sends a spurious notification. 
//       See the FAQ on Xbox 360 Central: https://xds.xbox.com/xbox360/nav.aspx?Page=devsupport/sitefaq.htm#misc17
//       for a description of the workaround
//--------------------------------------------------------------------------------------
enum SIGNIN_STATE
{
    SIGNIN_NONE,
    SIGNIN_NOTIFIED,
    SIGNIN_CONFIRMED,
};

//--------------------------------------------------------------------------------------
// Name: ITEM_ID_ 
// Desc: Declares unique identifiers for the dynamic menu items
//--------------------------------------------------------------------------------------
#define ITEM_ID_END_GAME_BUTTON         1
#define ITEM_ID_GUIDE_BUTTON            2
#define ITEM_ID_MANAGE_IDENTITY_BUTTON  3
#define ITEM_ID_PLAY_BUTTON             4
#define ITEM_ID_RESUME_BUTTON           5
#define ITEM_ID_SNAPSHOT_BUTTON         6


//--------------------------------------------------------------------------------------
// Name: ITEM_TEXT_
// Desc: Defines variations static text that changes in the menu depending on the 
//       context.
//--------------------------------------------------------------------------------------
#define ITEM_TEXT_SIGN_IN_BUTTON_TEXT         L"Sign In"         // Text of Lobby manage identity button used when the player hasn't been signed-in 
#define ITEM_TEXT_CHANGE_PROFILE_BUTTON_TEXT  L"Change Profile"  // Text of Lobby manage identity button used when the player is signed-in


//--------------------------------------------------------------------------------------
// Name: Menu strings
// Desc: These strings are used by the menus and changes to their content is directly 
//       reflected in the menu.
//--------------------------------------------------------------------------------------
WCHAR g_wszCurrentPlayerName[ XUSER_NAME_SIZE ] = L""; // Holds the current player name, or 
                                                       // a message to the effect that the player has no account name.

WCHAR g_wszIdentityStatus[ STRING_MESSAGE_SIZE ] = L""; // Hold a text message describing the state of identity for the player.


//--------------------------------------------------------------------------------------
// Name: g_aIdentifyingMenuItem[]
// Desc: Declares the menu layout for the player identification scene.
//--------------------------------------------------------------------------------------
const DWORD IDENTIFYING_MENU_COLUMNS  = 24;
const DWORD IDENTIFYING_MENU_ROWS     = 24;
ATG::NUI_MENU_ITEM g_aIdentifyingMenuItem[] =
{
    ATG_NUI_MENU_ITEM_TEXT(     6,  8, 12,  3,  ATG::NUI_MENU_ITEM_ID_UNDEFINED,  g_wszIdentityStatus ),
    ATG_NUI_MENU_ITEM_TEXT(     9, 12,  6,  2,  ATG::NUI_MENU_ITEM_ID_UNDEFINED,  L"Please face the sensor." ),
    ATG_NUI_MENU_ITEM_SPINNER( 10, 14,  4,  1,  ATG::NUI_MENU_ITEM_ID_UNDEFINED ),
};


//--------------------------------------------------------------------------------------
// Name: g_aLobbyMenuItem[]
// Desc: Declares the menu layout for the lobby scene.
//--------------------------------------------------------------------------------------
const DWORD LOBBY_MENU_COLUMNS  = 32;
const DWORD LOBBY_MENU_ROWS     = 32;
ATG::NUI_MENU_ITEM g_aLobbyMenuItem[] =
{
    ATG_NUI_MENU_ITEM_FRAME(   9,  7, 14,  13,  ATG::NUI_MENU_ITEM_ID_UNDEFINED ),
    ATG_NUI_MENU_ITEM_TEXT(   10,  8, 12,  4,  ATG::NUI_MENU_ITEM_ID_UNDEFINED,  g_wszCurrentPlayerName ),
    ATG_NUI_MENU_ITEM_BUTTON( 11, 14, 10,  4,  ITEM_ID_MANAGE_IDENTITY_BUTTON,   ITEM_TEXT_SIGN_IN_BUTTON_TEXT ),
    ATG_NUI_MENU_ITEM_BUTTON(  9, 23, 14,  4,  ITEM_ID_PLAY_BUTTON,              L"Start Game" ),
};


//--------------------------------------------------------------------------------------
// Name: g_aGameMenuItem[]
// Desc: Declares the menu layout for the game scene.
//--------------------------------------------------------------------------------------
const DWORD GAME_MENU_COLUMNS  = 16;
const DWORD GAME_MENU_ROWS     = 16;
ATG::NUI_MENU_ITEM g_aGameMenuItem[] =
{
    ATG_NUI_MENU_ITEM_BUTTON( 9,  9,  6,  2, ITEM_ID_SNAPSHOT_BUTTON, L"Take Snapshot" ),
    ATG_NUI_MENU_ITEM_BUTTON( 9, 12,  6,  2, ITEM_ID_END_GAME_BUTTON, L"End Game" ),
};


//--------------------------------------------------------------------------------------
// Name: g_aPauseMenuItem[]
// Desc: Declares the menu layout for the pause menu.
//--------------------------------------------------------------------------------------
const DWORD PAUSE_MENU_COLUMNS  = 16;
const DWORD PAUSE_MENU_ROWS     = 16;
ATG::NUI_MENU_ITEM g_aPauseMenuItem[] =
{
    ATG_NUI_MENU_ITEM_PANEL(  4,  1,  8, 14, ATG::NUI_MENU_ITEM_ID_UNDEFINED ),
    ATG_NUI_MENU_ITEM_FRAME(  4,  1,  8, 14, ATG::NUI_MENU_ITEM_ID_UNDEFINED ),
    ATG_NUI_MENU_ITEM_TEXT(   4,  2,  8,  3, ATG::NUI_MENU_ITEM_ID_UNDEFINED, L"Paused!" ),
    ATG_NUI_MENU_ITEM_BUTTON( 5,  6,  6,  2, ITEM_ID_RESUME_BUTTON,           L"Resume" ),
    ATG_NUI_MENU_ITEM_BUTTON( 5,  9,  6,  2, ITEM_ID_GUIDE_BUTTON,            L"Kinect Guide" ),
    ATG_NUI_MENU_ITEM_BUTTON( 5, 12,  6,  2, ITEM_ID_END_GAME_BUTTON,         L"End Game" ),
};


//--------------------------------------------------------------------------------------
// Name: GetSkeletonDataFromTrackingID()
// Desc: Returns a pointer to the NUI_SKELETON_DATA structure belonging to the skeleton 
//       identified by dwTrackingID.
//       It returns NULL if no matching skeleton is found.
//--------------------------------------------------------------------------------------
const NUI_SKELETON_DATA* GetSkeletonDataFromTrackingID( const NUI_SKELETON_FRAME* pSkeletonFrame, DWORD dwTrackingID )
{
    assert( pSkeletonFrame != NULL );
    assert( dwTrackingID != NUI_SKELETON_INVALID_TRACKING_ID );
    
    const NUI_SKELETON_DATA* pSkeletonData = NULL;
    for( DWORD dwSkeletonIndex = 0; dwSkeletonIndex < NUI_SKELETON_COUNT; ++ dwSkeletonIndex )
    {
        if( pSkeletonFrame->SkeletonData[ dwSkeletonIndex ].dwTrackingID == dwTrackingID )
        {
            pSkeletonData = &pSkeletonFrame->SkeletonData[ dwSkeletonIndex ];
        }
    }
    
    return pSkeletonData;
}


//--------------------------------------------------------------------------------------
// Name: GetClosestSkeletonTrackingID
// Desc: Returns the tracking ID for the fully tracked skeleton closest to the Kinect 
//       sensor.
//       Returns NUI_SKELETON_INVALID_TRACKING_ID if there are no skeletons fully
//       tracked.
//--------------------------------------------------------------------------------------
DWORD GetClosestSkeletonTrackingID( const NUI_SKELETON_FRAME* pSkeletonFrame )
{
    DWORD dwTrackingID = NUI_SKELETON_INVALID_TRACKING_ID; 
    FLOAT fDistance = FLT_MAX; // Start with an arbitrary large number, far beyond the range of the sensor array.

    for( DWORD dwSkeletonIndex = 0; dwSkeletonIndex < NUI_SKELETON_COUNT; ++ dwSkeletonIndex )
    {
        if( pSkeletonFrame->SkeletonData[ dwSkeletonIndex ].eTrackingState == NUI_SKELETON_TRACKED &&
            pSkeletonFrame->SkeletonData[ dwSkeletonIndex ].Position.z < fDistance )
        {
            fDistance = pSkeletonFrame->SkeletonData[ dwSkeletonIndex ].Position.z;
            dwTrackingID = pSkeletonFrame->SkeletonData[ dwSkeletonIndex ].dwTrackingID;
        }
    }

    return dwTrackingID;
}


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

    VOID OpenPauseMenu();
    VOID ClosePauseMenu();

    VOID SwitchToScene( SCENE eNewScene, const KinectFrame* pKinectFrame );

    VOID UpdateIntentScene();
    VOID RenderIntentScene();

    VOID UpdateIdentifyingScene( const KinectFrame* pKinectFrame );
    VOID RenderIdentifyingScene();

    VOID UpdateLobbyScene( const KinectFrame* pKinectFrame );
    VOID RenderLobbyScene();

    VOID UpdateGameScene( const KinectFrame* pKinectFrame );
    VOID RenderGameScene();

    VOID UpdatePlayerInfo( const KinectFrame* pKinectFrame );

private:

    // General sample data
    ATG::Timer   m_Timer;         // General usage timer
    ATG::Font    m_Font;          // Font shared by the sample and menus
    HANDLE       m_hNotification; // Handle to retrieve system notifications

    ATG::NuiMenu m_SceneMenu;  // Since  only one scene can be active at a time, they share the same menu instance.
    BOOL         m_bPaused;    // TRUE if the sample has been paused.
    ATG::NuiMenu m_PauseMenu;  // Pause menu instance is separate from the scene menus so it can be overlaid.

    KinectSensor m_kinectSensor; // Sensor array manager used to retrive data from the skeleton and image streams.


    // Identity manager demonstrated in this sample
    SinglePlayerIdentityManager m_IdentityManager;


    // Sample state data
    DWORD      m_dwTrackedSkeletonCount; // Number of actively tracked skeletons, update each frame.
    SCENE      m_eCurrentScene;          // The scene the sample is currently in.
    SCENE      m_ePendingScene;          // The scene the sample will switch to at the next opportunity. 
    ATG::Timer m_CurrentSceneTimer;      // Tells how long a scene has been active. Reset each time a scene is entered.
    WCHAR      m_wszGameEndedReason[ STRING_MESSAGE_SIZE ]; // Reason player was returned to Intent scene.


    // For XN_SYS_SIGNINCHANGED, handle the case where the system sends a spurious notification. 
    // See the FAQ on Xbox 360 Central: https://xds.xbox.com/xbox360/nav.aspx?Page=devsupport/sitefaq.htm#misc17
    // for a description of the workaround    
    SIGNIN_STATE m_eSignInState;
    DWORD m_dwLastSignInChangeTick;


    // Data associated with the current player
    DWORD                   m_dwCurrentPlayerTrackingID;
    FLOAT                   m_fCurrentPlayerIntentProgress;
    SKELETON_IDENTITY_STATE m_CurrentPlayerIdentityState;
    DWORD                   m_dwGameUserIndex; // User index identifying the profile selected by the player to be used
                                               // during this gaming session.


    // Snapshot data
    BOOL                  m_bTakeNewSnapshot;
    ATG::NuiVisualization m_SnapshotRenderer;
};


//--------------------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the program
//--------------------------------------------------------------------------------------
INT __cdecl main()
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
    // Create the font
    RETURN_ON_FAIL( m_Font.Create( "game:\\Media\\Fonts\\Arial_20.xpr" ) );

        // Creating a notification listener to keep track of changes in profile sign-in status
    m_hNotification = XNotifyCreateListener( XNOTIFY_SYSTEM );
    if( m_hNotification == NULL || m_hNotification == INVALID_HANDLE_VALUE )
    { 
        ATG_PrintError( "Unable to create XNotify listener" );
        return E_FAIL;
    }

    // Confine text drawing to the safe area
    m_Font.SetWindow( ATG::GetTitleSafeArea() );

    // Initialize the simple shaders, sensor and menus
    ATG::SimpleShaders::Initialize( NULL, NULL );
    RETURN_ON_FAIL( m_kinectSensor.Initialize() );
    RETURN_ON_FAIL( m_SceneMenu.Initialize( m_pd3dDevice, m_d3dpp.BackBufferWidth, m_d3dpp.BackBufferHeight, &m_Font ) );
    RETURN_ON_FAIL( m_PauseMenu.Initialize( m_pd3dDevice, m_d3dpp.BackBufferWidth, m_d3dpp.BackBufferHeight, &m_Font ) );
    m_bPaused = FALSE;

    // Initalize player data
    m_dwCurrentPlayerTrackingID    = NUI_SKELETON_INVALID_TRACKING_ID;
    m_fCurrentPlayerIntentProgress = 0.0f;
    m_CurrentPlayerIdentityState   = SKELETON_IDENTITY_STATE_INVALID;
    m_dwGameUserIndex              = XUSER_INDEX_NONE;

    // Initialize Snapshot related data
    m_bTakeNewSnapshot = FALSE;
    m_SnapshotRenderer.Initialize( m_pd3dDevice, NUI_INITIALIZE_FLAG_USES_COLOR, NUI_IMAGE_RESOLUTION_640x480 );

    // Activate the wave recognizer
    HRESULT hr = NuiWaveSetEnabled( TRUE ); 
    if( FAILED( hr ) ) 
    {
        ATG::NuiPrintError( hr, "NuiWaveSetEnabled" );
    }
   
    // Begin at the intent scene
    m_dwTrackedSkeletonCount  = 0;
    m_eCurrentScene           = SCENE_INTENT;
    m_ePendingScene           = SCENE_INTENT;
    m_CurrentSceneTimer.Reset();
    m_wszGameEndedReason[ 0 ] = L'\0';

    m_eSignInState = SIGNIN_NONE;
    m_dwLastSignInChangeTick = GetTickCount();

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Update()
// Desc: Called once per frame, the call is the entry point for animating the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Update()
{
    // Process the game controller so developpers can exit the sample by
    // holding both triggers and both bumpers simultaneously.
    ATG::Input::GetMergedInput();


    // Retrieve new data from the sensor array
    KinectFrame kinectFrame;
    HRESULT hr = m_kinectSensor.AcquireKinectFrame( &kinectFrame );
    if( SUCCEEDED( hr ) )
    {
        //  Update the sample only if a new skeleton frame is available
        if( SUCCEEDED( kinectFrame.hrSkeletonRetrieved ) )
        {
            // Update the count of players being actively tracked. The sample makes decision based on the actual 
            // number of players tracked.
            m_dwTrackedSkeletonCount = 0;
            for( DWORD dwSkeletonIndex = 0; dwSkeletonIndex < NUI_SKELETON_COUNT; ++ dwSkeletonIndex )
            {
                if( kinectFrame.SkeletonFrame.SkeletonData[ dwSkeletonIndex ].eTrackingState == NUI_SKELETON_TRACKED )
                {
                    ++ m_dwTrackedSkeletonCount;
                }
            }

            // Process changes in the identity of the current player
            m_IdentityManager.Update( &kinectFrame.SkeletonFrame );
            if( m_dwCurrentPlayerTrackingID != NUI_SKELETON_INVALID_TRACKING_ID )
            {
                // Update player info according to most recent identity status 
                SKELETON_IDENTITY_STATE eNewState;
                HRESULT hrIdentityManager = m_IdentityManager.GetPlayerState( m_dwCurrentPlayerTrackingID, &eNewState );
                if( hrIdentityManager == E_NUI_IDENTITY_LOST_TRACK )
                {
                    m_dwCurrentPlayerTrackingID = NUI_SKELETON_INVALID_TRACKING_ID;
                    m_CurrentPlayerIdentityState = SKELETON_IDENTITY_STATE_INVALID;
                }
                else
                {
                    assert( SUCCEEDED( hr ) );

                    if( eNewState != m_CurrentPlayerIdentityState )
                    {
                        if( ! m_bPaused && m_eCurrentScene != SCENE_GAME )
                        {
                            UpdatePlayerInfo( &kinectFrame );
                        }

                        m_CurrentPlayerIdentityState = eNewState;
                    }
                }
            }

            // Make the player closest to the Kinect sensor, the current player, if no one is assuming this role yet
            if( m_dwCurrentPlayerTrackingID == NUI_SKELETON_INVALID_TRACKING_ID && m_dwTrackedSkeletonCount > 0 )
            {
                m_dwCurrentPlayerTrackingID = GetClosestSkeletonTrackingID( &kinectFrame.SkeletonFrame );

                m_IdentityManager.GetPlayerState( m_dwCurrentPlayerTrackingID, &m_CurrentPlayerIdentityState );

                if( ! m_bPaused && m_eCurrentScene != SCENE_GAME )
                    UpdatePlayerInfo( &kinectFrame );
            }
    
            if( m_ePendingScene != m_eCurrentScene && !m_bPaused )
            {
                SwitchToScene( m_ePendingScene, &kinectFrame );
                m_eCurrentScene = m_ePendingScene;
            }

            DWORD dwNotificationID;
            ULONG_PTR ulParam;

            // Process sytem notifications
            while( XNotifyGetNext( m_hNotification, 0, &dwNotificationID, &ulParam ) )
            {
                switch( dwNotificationID )
                {
                    case XN_SYS_NUIPAUSE:
                        OpenPauseMenu();
                        break;

                    case XN_SYS_SIGNINCHANGED:
                        m_eSignInState = SIGNIN_NOTIFIED;
                        m_dwLastSignInChangeTick = GetTickCount();
                        break;

                    case XN_SYS_NUIBINDINGCHANGED:
                        if( ! m_bPaused && m_eCurrentScene != SCENE_GAME )
                        {
                            // Ignoring these messages when the pause menu is up or the sample is in the SCENE_GAME scene 
                            // is compensated by the fact that UpdatePlayerInfo is called each time the pause menu is 
                            // exited or the SCENE_GAME scene is exited.

                            UpdatePlayerInfo( &kinectFrame );
                        }
                        break;
                }
            }

            static const DWORD dwTickToConfirmSignInChange = 1000;
            if (m_eSignInState == SIGNIN_NOTIFIED &&
                GetTickCount() - m_dwLastSignInChangeTick > dwTickToConfirmSignInChange)
            {
                m_eSignInState = SIGNIN_CONFIRMED;
            }

            // Handle scene specific updates
            switch( m_eCurrentScene )
            {
                case SCENE_INTENT:
                    UpdateIntentScene();
                    break;

                case SCENE_IDENTIFYING:
                    UpdateIdentifyingScene( &kinectFrame );
                    break;

                case SCENE_LOBBY:
                    UpdateLobbyScene( &kinectFrame );
                    break;

                case SCENE_GAME:
                    UpdateGameScene( &kinectFrame );
                    break;

                default:
                    assert( false );
            }


            // Process the pause menu, if the sample is paused
            if( m_bPaused && m_dwCurrentPlayerTrackingID != NUI_SKELETON_INVALID_TRACKING_ID )
            {
                m_PauseMenu.Update( &kinectFrame.SkeletonFrame, m_dwCurrentPlayerTrackingID, kinectFrame.pDepthFrame320x240, kinectFrame.pDepthFrame80x60 );
                switch( m_PauseMenu.GetActiveItem() )
                {
                    case ATG::NUI_MENU_ITEM_ID_NONE:
                        break;

                    case ITEM_ID_GUIDE_BUTTON:
                    {
                        XShowNuiGuideUI( m_dwCurrentPlayerTrackingID );
                        break;
                    }

                    case ITEM_ID_RESUME_BUTTON:
                    {
                        ClosePauseMenu();
                        if( m_eCurrentScene != SCENE_GAME )
                        {
                            UpdatePlayerInfo( &kinectFrame );
                        }
                        m_IdentityManager.PlayerHasShownIntentToPlay( m_dwCurrentPlayerTrackingID );
                        break;
                    }

                    case ITEM_ID_END_GAME_BUTTON:
                    {
                        ClosePauseMenu();
                        if( m_eCurrentScene != SCENE_GAME )
                        {
                            UpdatePlayerInfo( &kinectFrame );
                        }
                        m_ePendingScene = SCENE_INTENT;
                        break;
                    }

                    default:
                        assert( false );

                }
            }
        }

        m_kinectSensor.ReleaseKinectFrame( &kinectFrame );
    }
    else
    {
        ATG::NuiPrintError( hr, "KinectSensor_AcuireKinectFrame" );
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Sample::Render()
// Desc: Sets up render states, clears the viewport, and renders the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Render()
{
    const D3DCOLOR TEXT_COLOR = D3DCOLOR_ARGB( 255, 255, 255, 255 );

    //Draw a gradient filled background
    ATG::RenderBackground( D3DCOLOR_ARGB( 255, 0, 0, 255 ), D3DCOLOR_ARGB( 255, 0, 0, 0 ) );

    // Show title, frame rate, and help
    m_Timer.MarkFrame();

    // Render scene specific information
    switch( m_eCurrentScene )
    {
        case SCENE_INTENT:
            RenderIntentScene();
            break;

        case SCENE_IDENTIFYING:
            RenderIdentifyingScene();
            break;

        case SCENE_LOBBY:
            RenderLobbyScene();
            break;

        case SCENE_GAME:
            RenderGameScene();
            break;

        default:
            assert( false );
    }

    // Draw title text
    m_Font.Begin();
    m_Font.SetScaleFactors( 1.2f, 1.2f );
    m_Font.DrawText( 0, 0, TEXT_COLOR,  L"Simple Identity" );

    m_Font.SetScaleFactors( 1.0f, 1.0f );
    m_Font.DrawText( 0, 0, D3DCOLOR_ARGB( 255, 255, 255, 0 ), m_Timer.GetFrameRate(), ATGFONT_RIGHT );
 
    m_Font.End();

    // Render the pause menu if the sample is paused
    if( m_bPaused )
    {
        m_PauseMenu.Render();
    }

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Sample::OpenPauseMenu()
// Desc: Disable the scene menu, reset the pause menu and set the class pause flag to 
//       TRUE.
//--------------------------------------------------------------------------------------
VOID Sample::OpenPauseMenu()
{
    m_SceneMenu.DisableMenu();
    m_PauseMenu.SetMenuLayout( g_aPauseMenuItem, _countof( g_aPauseMenuItem ), PAUSE_MENU_COLUMNS, PAUSE_MENU_ROWS );
    m_bPaused = TRUE;
}


//--------------------------------------------------------------------------------------
// Name: Sample::ClosePauseMenu()
// Desc: Reactivater the scene menu and unpause the sample.
//--------------------------------------------------------------------------------------
VOID Sample::ClosePauseMenu()
{
    m_SceneMenu.EnableMenu();
    m_bPaused = FALSE;
}


//--------------------------------------------------------------------------------------
// Name: Sample::SwitchToScene()
// Desc: Moves the sample to a new scene. Update all necessary states based on the 
//       current scene and the scene being traveled to.
//--------------------------------------------------------------------------------------
VOID Sample::SwitchToScene( SCENE eNewScene, const KinectFrame* pKinectFrame )
{
    assert( ! m_bPaused );
    assert( eNewScene != m_eCurrentScene );

    // Turn off any scene specific services before exiting the scene
    switch( m_eCurrentScene )
    {
        case SCENE_INTENT:
        {
            // We won't need the wave recognizer in other scenes
            HRESULT hr = NuiWaveSetEnabled( FALSE ); 
            if( FAILED( hr ) ) 
            {
                ATG::NuiPrintError( hr, "NuiWaveSetEnabled" );
            }

            break;
        }

        case SCENE_IDENTIFYING:
            break;

        case SCENE_LOBBY:
            break;

        case SCENE_GAME:
            Sample::UpdatePlayerInfo( pKinectFrame );
            break;

        default:
            assert( false );
    }

    // Initialize the new scene
    switch( eNewScene )
    {
        case SCENE_INTENT:
        {
            // Clear current player data until we acquire a new player 
            m_dwCurrentPlayerTrackingID    = NUI_SKELETON_INVALID_TRACKING_ID;
            m_fCurrentPlayerIntentProgress = 0.0f;
            m_CurrentPlayerIdentityState   = SKELETON_IDENTITY_STATE_INVALID;
            m_dwGameUserIndex              = XUSER_INDEX_NONE;

            // Activate the wave recognizer
            HRESULT hr = NuiWaveSetEnabled( TRUE ); 
            if( FAILED( hr ) ) 
            {
                ATG::NuiPrintError( hr, "NuiWaveSetEnabled" );
            }

            break;
        }

        case SCENE_IDENTIFYING:
        {
            assert( m_eCurrentScene == SCENE_INTENT );

            // Set the player identification menu to an initial state
            m_SceneMenu.SetMenuLayout( g_aIdentifyingMenuItem , _countof( g_aIdentifyingMenuItem ), IDENTIFYING_MENU_COLUMNS, IDENTIFYING_MENU_ROWS );
            break;
        }

        case SCENE_LOBBY:
        {
            assert( m_eCurrentScene == SCENE_INTENT || m_eCurrentScene == SCENE_IDENTIFYING );

            // Set the Lobby menu to an initial state
            m_SceneMenu.SetMenuLayout( g_aLobbyMenuItem , _countof( g_aLobbyMenuItem ), LOBBY_MENU_COLUMNS, LOBBY_MENU_ROWS );
            break;
        }

        case SCENE_GAME:
        {
            assert( m_eCurrentScene == SCENE_LOBBY );

            // Activate the game scene menu
            m_SceneMenu.SetMenuLayout( g_aGameMenuItem , _countof( g_aGameMenuItem ), GAME_MENU_COLUMNS, GAME_MENU_ROWS );
            m_SceneMenu.EnableItem( ITEM_ID_SNAPSHOT_BUTTON );
            break;
        }

        default:
            assert( false );
    }

    // Ensure NuiIdentity is running for this scene
    // It is possible that the scene switch occured while NuiIdentity was paused, 
    // for example if a snapshot as being taken at that moment.
    NuiIdentityResume();

    // Set the new active scene and reset the timer
    m_eCurrentScene = eNewScene;
    m_CurrentSceneTimer.Reset();
}


//--------------------------------------------------------------------------------------
// Name: Sample::UpdateIntentScene()
// Desc: Looks for the first player to signal intent to play then makes this player the 
//       active player before switching to the Lobby scene.
//--------------------------------------------------------------------------------------
VOID Sample::UpdateIntentScene()
{
    // There is nothing to update in this scene, if the sample is paused.
    if( m_bPaused )
    {
        return;
    }

    DWORD dwWaveOwnerTrackingID;
    if( SUCCEEDED( NuiWaveGetGestureOwnerProgress( &dwWaveOwnerTrackingID,  &m_fCurrentPlayerIntentProgress) ) )
    {
        if( m_fCurrentPlayerIntentProgress >= 1.0f )
        {
            // Put the first player to wave in control, notify the identity manager that the player has shown 
            // intent to play ( this will cause identity to sign-in the player if there is a matching biometric 
            // profile ) and move to the lobby scene 
            m_dwCurrentPlayerTrackingID = dwWaveOwnerTrackingID;
            m_IdentityManager.PlayerHasShownIntentToPlay( dwWaveOwnerTrackingID );
            m_ePendingScene = SCENE_IDENTIFYING;
        }
        else
        {
            // If any player has started to wave, erase any player message displayed on screen
            if( m_fCurrentPlayerIntentProgress > 0.1f )
            {
                m_wszGameEndedReason[ 0 ] = L'\0';
            }
        }
    }
}


//--------------------------------------------------------------------------------------
// Name: Sample::RenderIntentScene()
// Desc: Displays the progress the player is making toward performing the wave gesture.
//--------------------------------------------------------------------------------------
VOID Sample::RenderIntentScene()
{
    D3DRECT rc;

    m_Font.Begin();
    m_Font.GetWindow( rc );
    m_Font.SetWindow( 0, 0, m_d3dpp.BackBufferWidth, m_d3dpp.BackBufferHeight );
        
    m_Font.DrawText( m_d3dpp.BackBufferWidth / 2.0f, m_d3dpp.BackBufferHeight - 2.0f * m_Font.GetFontHeight(), 0xffffffff, m_wszGameEndedReason, ATGFONT_CENTER_X );            

    if( m_dwTrackedSkeletonCount > 0 )
	{
        const FLOAT PROGRESS_BAR_LENGTH = 200;
        const FLOAT PROGRESS_BAR_HEIGHT =  m_Font.GetFontHeight() / 2.0f;

        // Display progress for the most advanced player
        m_Font.DrawText( m_d3dpp.BackBufferWidth / 2.0f, m_d3dpp.BackBufferHeight / 2.0f, 0xffffffff, L"Wave to Begin", ATGFONT_CENTER_X | ATGFONT_CENTER_Y );

        ATG::DebugDraw::DrawScreenSpaceRect( XMFLOAT2( ( m_d3dpp.BackBufferWidth - PROGRESS_BAR_LENGTH ) / 2.0f , m_d3dpp.BackBufferHeight / 2.0f + m_Font.GetFontHeight() ), 
                                             XMFLOAT2( PROGRESS_BAR_LENGTH, PROGRESS_BAR_HEIGHT ), 1, 0xffffffff );
        
        ATG::DebugDraw::DrawScreenSpaceRect( XMFLOAT2( ( m_d3dpp.BackBufferWidth - PROGRESS_BAR_LENGTH ) / 2.0f , m_d3dpp.BackBufferHeight / 2.0f + m_Font.GetFontHeight() ), 
                                             XMFLOAT2( PROGRESS_BAR_LENGTH * m_fCurrentPlayerIntentProgress * 1.25f, PROGRESS_BAR_HEIGHT ), 0, 0xffffffff );
	}
	else
	{
        // Display a message if no player in view
        m_Font.DrawText( m_d3dpp.BackBufferWidth / 2.0f, m_d3dpp.BackBufferHeight / 2.0f, 0xffffffff, L"Please adjust your standing position,\nso that the sensor captures you", ATGFONT_CENTER_X | ATGFONT_CENTER_Y );            
	}

    m_Font.SetWindow( rc );
    m_Font.End();
}


//--------------------------------------------------------------------------------------
// Name: Sample::UpdateIdentifyingScene()
// Desc: The player identification scene provides feedback to player while he or she is 
//       being identified, enrolled and signed-in.
//--------------------------------------------------------------------------------------
VOID Sample::UpdateIdentifyingScene( const KinectFrame* pKinectFrame )
{
    assert( pKinectFrame != NULL );

    // Unless the sample is paused, the sample will return to the intent scene if the current player is lost 
    if( ! m_bPaused && m_dwCurrentPlayerTrackingID == NUI_SKELETON_INVALID_TRACKING_ID )
    {
        wcscpy_s( m_wszGameEndedReason, L"Player Was Lost" );
        m_ePendingScene = SCENE_INTENT;
        return;
    }

    // Update current player information based on the related identity status
    switch( m_CurrentPlayerIdentityState )
    {
        case SKELETON_IDENTITY_STATE_INVALID:
            assert( false );
            break;

        case SKELETON_IDENTITY_STATE_BUSY:
        {               
            DWORD dwQualityFlags;
            HRESULT hr = m_IdentityManager.GetQualityFlags( m_dwCurrentPlayerTrackingID, &dwQualityFlags );
            if( SUCCEEDED( hr ) && dwQualityFlags )
            {
                swprintf_s( g_wszIdentityStatus, ATG::GetIdentityQualityFlagPrompt( dwQualityFlags ) );
            }
            else
            {
                g_wszIdentityStatus[ 0 ] = '\0';
            }
            
            break;
        }

        case SKELETON_IDENTITY_STATE_GUEST:
        case SKELETON_IDENTITY_STATE_SIGNED_IN:
            m_ePendingScene = SCENE_LOBBY;
            break;

        default:
            assert( false );
    }

    // Update the menu
    m_SceneMenu.Update( &pKinectFrame->SkeletonFrame, m_dwCurrentPlayerTrackingID, pKinectFrame->pDepthFrame320x240, pKinectFrame->pDepthFrame80x60 );
}


//--------------------------------------------------------------------------------------
// Name: Sample::UpdateLobbyScene()
// Desc: The lobby scene manages the player identity. It provides an opportunity for a 
//       player to sign-in to a profile before the game starts.
//--------------------------------------------------------------------------------------
VOID Sample::UpdateLobbyScene( const KinectFrame* pKinectFrame )
{
    assert( pKinectFrame != NULL );

    // Unless the sample is paused, the sample will return to the intent scene if the current player is lost 
    if( ! m_bPaused && m_dwCurrentPlayerTrackingID == NUI_SKELETON_INVALID_TRACKING_ID )
    {
        wcscpy_s( m_wszGameEndedReason, L"Player Was Lost" );
        m_ePendingScene = SCENE_INTENT;
        return;
    }

    // Update current player information based on the related identity status
    switch( m_CurrentPlayerIdentityState )
    {
        case SKELETON_IDENTITY_STATE_INVALID:
            assert( false );
            m_SceneMenu.SetItemText( ITEM_ID_MANAGE_IDENTITY_BUTTON, ITEM_TEXT_SIGN_IN_BUTTON_TEXT );
            break;

        case SKELETON_IDENTITY_STATE_BUSY:
        case SKELETON_IDENTITY_STATE_GUEST:
            m_SceneMenu.SetItemText( ITEM_ID_MANAGE_IDENTITY_BUTTON, ITEM_TEXT_SIGN_IN_BUTTON_TEXT );
            break;

        case SKELETON_IDENTITY_STATE_SIGNED_IN:
            m_SceneMenu.SetItemText( ITEM_ID_MANAGE_IDENTITY_BUTTON, ITEM_TEXT_CHANGE_PROFILE_BUTTON_TEXT );
            break;

        default:
            m_SceneMenu.SetItemText( ITEM_ID_MANAGE_IDENTITY_BUTTON, ITEM_TEXT_SIGN_IN_BUTTON_TEXT );
            assert( false );
    }

    // Process the lobby menu
    m_SceneMenu.Update( &pKinectFrame->SkeletonFrame, m_dwCurrentPlayerTrackingID, pKinectFrame->pDepthFrame320x240, pKinectFrame->pDepthFrame80x60 );
    switch( m_SceneMenu.GetActiveItem() )
    {
        case ATG::NUI_MENU_ITEM_ID_NONE:
            break;

        case ITEM_ID_MANAGE_IDENTITY_BUTTON:
        {
            assert( m_dwCurrentPlayerTrackingID != NUI_SKELETON_INVALID_TRACKING_ID );
            XShowNuiSigninUI( m_dwCurrentPlayerTrackingID, XSSUI_FLAGS_LOCALSIGNINONLY | XSSUI_FLAGS_DISALLOW_GUEST );
            break;
        }

        case ITEM_ID_PLAY_BUTTON:   
            m_ePendingScene = SCENE_GAME;
            break;

        default:
            assert( false );
    }
}


//--------------------------------------------------------------------------------------
// Name: Sample::RenderIdentifyingScene()
// Desc: Renders the player identification scene
//--------------------------------------------------------------------------------------
VOID Sample::RenderIdentifyingScene()
{
    m_SceneMenu.Render();
}


//--------------------------------------------------------------------------------------
// Name: Sample::RenderLobbyScene()
// Desc: Renders the lobby scene
//--------------------------------------------------------------------------------------
VOID Sample::RenderLobbyScene()
{
    m_SceneMenu.Render();
}


//--------------------------------------------------------------------------------------
// Name: Sample::UpdateGameScene()
// Desc: Updates the game scene, including taking snapshots and exiting the game if the 
//       player account in use is signed out.
//--------------------------------------------------------------------------------------
VOID Sample::UpdateGameScene( const KinectFrame* pKinectFrame )
{
    assert( pKinectFrame != NULL );

    // Outside the lobby, the sample doesn't care for changes in signed-in or out users, except if the profile we are
    // using is signed out. At that point the game is immediately terminated.
    if( m_dwGameUserIndex != XUSER_INDEX_NONE &&
        m_eSignInState == SIGNIN_CONFIRMED )
    {
        if( XUserGetSigninState( m_dwGameUserIndex ) == eXUserSigninState_NotSignedIn )
        {
            wcscpy_s( m_wszGameEndedReason, L"Player Was Signed Out" );
            m_ePendingScene = SCENE_INTENT;
            return;
        }

        m_eSignInState = SIGNIN_NONE;
    }

    // There is nothing to update if the sample is paused
    if( m_bPaused )
    {
        return;
    }

    // If all players have left, open the pause menu
    if( m_dwTrackedSkeletonCount == 0 )
    {
        OpenPauseMenu();
        return;
    }

    // Update the snapshot if the color feed is available and a snapshot was requested, then resume identity
    if( m_bTakeNewSnapshot && pKinectFrame->pImageFrame )
    {
        m_SnapshotRenderer.SetColorTexture( pKinectFrame->pImageFrame->pFrameTexture );
        m_bTakeNewSnapshot = FALSE;
        NuiIdentityResume();
        m_SceneMenu.EnableItem( ITEM_ID_SNAPSHOT_BUTTON );
    }

    // Process the scene menu
    m_SceneMenu.Update( &pKinectFrame->SkeletonFrame, m_dwCurrentPlayerTrackingID, pKinectFrame->pDepthFrame320x240, pKinectFrame->pDepthFrame80x60 );
    switch( m_SceneMenu.GetActiveItem() )
    {
        case ATG::NUI_MENU_ITEM_ID_NONE:
            break;

        case ITEM_ID_SNAPSHOT_BUTTON:
            NuiIdentityPause();
            m_bTakeNewSnapshot = TRUE;
            m_SceneMenu.DisableItem( ITEM_ID_SNAPSHOT_BUTTON );
            break;

        case ITEM_ID_END_GAME_BUTTON:              
            m_wszGameEndedReason[ 0 ] = L'\0';
            m_ePendingScene = SCENE_INTENT;
            break;

        default:
            assert( false );
    }
}


//--------------------------------------------------------------------------------------
// Name: Sample::RenderGameScene()
// Desc: Displays the various elements used in the game scene, including snapshot and 
//       menu.
//--------------------------------------------------------------------------------------
VOID Sample::RenderGameScene()
{
    m_Font.Begin();

    const FLOAT fLineAdvance = m_Font.GetFontHeight();
    const FLOAT LEFT_MARGIN = 10.0f;

    WCHAR wszText[ 256 ];

    if( m_dwGameUserIndex == XUSER_INDEX_NONE )
    {
        m_Font.DrawText( LEFT_MARGIN, 4 * fLineAdvance, 0xffffffff, L"Player progress - for example save\ngames andachievements - will be\nlost at the end of the session", ATGFONT_LEFT );
    }
    else
    {
        swprintf_s( wszText,  L"Player progress - like save games and\nachievements - would be assigned to\n\"%s\"", g_wszCurrentPlayerName );
        m_Font.DrawText( LEFT_MARGIN, 4 * fLineAdvance, 0xffffffff,wszText, ATGFONT_LEFT );            

        swprintf_s( wszText,  L"If \"%s\" is signed out\nthe game will terminate immediately.\n\nPerform the system gesture to bring up\nthe Sign-In UI and manage profiles.", g_wszCurrentPlayerName );
        m_Font.DrawText( LEFT_MARGIN, 8 * fLineAdvance, 0xffffffff, wszText, ATGFONT_LEFT );            
    }

    m_Font.DrawText( LEFT_MARGIN, 15 * fLineAdvance, 0xffffffff, L"Anyone can jump in and take control\nwhile the game is paused.", ATGFONT_LEFT );            

    m_Font.End();

    // Display the latest snapshot taken
    m_SnapshotRenderer.RenderColorStream( 800, 140, 320, 320 * 3 / 4 );

    // Render the menu last, so the cursor is displayed on top of everything
    m_SceneMenu.Render();
}


//--------------------------------------------------------------------------------------
// Name: Sample::UpdatePlayerInfo()
// Desc: Updates the profile information pertaining to the  current player.
//--------------------------------------------------------------------------------------
VOID Sample::UpdatePlayerInfo( const KinectFrame* pKinectFrame )
{

    if( m_dwCurrentPlayerTrackingID != NUI_SKELETON_INVALID_TRACKING_ID )
    {
        const NUI_SKELETON_DATA* pSkeletonData = GetSkeletonDataFromTrackingID( &pKinectFrame->SkeletonFrame, m_dwCurrentPlayerTrackingID );
        assert( pSkeletonData != NULL );
    
        if( pSkeletonData->dwUserIndex == XUSER_INDEX_NONE )
        {
            m_dwGameUserIndex = XUSER_INDEX_NONE;
            wcscpy_s( g_wszCurrentPlayerName, L"Guest Player" );
        }
        else
        {
            XUSER_SIGNIN_INFO UserInfo;
            HRESULT hrUserGetInfo = XUserGetSigninInfo( pSkeletonData->dwUserIndex, XUSER_GET_SIGNIN_INFO_OFFLINE_XUID_ONLY, &UserInfo );
            if( SUCCEEDED( hrUserGetInfo ) )
            {
                m_dwGameUserIndex = pSkeletonData->dwUserIndex;
                if( strlen( UserInfo.szUserName ) > 0 )
                {
                    MultiByteToWideChar( CP_UTF8, 0, UserInfo.szUserName, -1, g_wszCurrentPlayerName, XUSER_NAME_SIZE );
                }
                else
                {   
                    g_wszCurrentPlayerName[ 0 ] = L'\0';         
                }
            }
        }
    }

}
