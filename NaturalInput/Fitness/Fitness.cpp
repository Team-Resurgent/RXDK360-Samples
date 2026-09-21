//--------------------------------------------------------------------------------------
// Fitness.cpp
//
// Microsoft Advanced Technology Group (ATG)
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include <xtl.h>
#include <assert.h>
#include <nuiapi.h>
#include <AtgApp.h>
#include <AtgDebugDraw.h>
#include <AtgFont.h>
#include <AtgInput.h>
#include <AtgNuiCommon.h>
#include <AtgNuiMenu.h>
#include <AtgNuiVisualization.h>
#include <AtgSimpleShaders.h>


#include "SinglePlayerIdentityManager.h"
#include "KinectSensor.h"

//--------------------------------------------------------------------------------------
// Name: Sizes and colors
// Desc: Define sizes and colors for various sample elements.
//--------------------------------------------------------------------------------------

// Skeleton colors
const D3DCOLOR SKELETON_COLOR = D3DCOLOR_ARGB( 0xff, 0x90, 0x90, 0x90 );

// Size of the color image on screen
const DWORD COLOR_IMAGE_RENDER_WIDTH  = 640;
const DWORD COLOR_IMAGE_RENDER_HEIGHT = 480;


//--------------------------------------------------------------------------------------
// Name: Buffer sizes
// Desc: Define sizes for various buffers.
//--------------------------------------------------------------------------------------
const DWORD STRING_MESSAGE_SIZE = 128;


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
// Name: SAMPLE_STATE
// Desc: Define the states that sample can find itself in.
//--------------------------------------------------------------------------------------
enum SAMPLE_STATE
{
    SAMPLESTATE_MAIN = 0, // Main screen is displayed
    SAMPLESTATE_STATS,    // Stats screen is displayed. Profile ust be signed-in prior to entering this screen.
    SAMPLESTATE_MAX
};


//--------------------------------------------------------------------------------------
// Name: FITNESS_ENUMERATOR_STATE
// Desc: Define the states the Fitness event enumerator can be in.
//--------------------------------------------------------------------------------------
enum FITNESS_ENUMERATOR_STATE
{
    NOT_ENUMERATING = 0,  // No data available
    IO_PENDING,           // An async operation is in progress
    DATA_AVAILABLE,       // An enumation has completed. Call XEnumerate to retrieve the next block of data.
};


//--------------------------------------------------------------------------------------
// Name: Menu strings
// Desc: These strings are used by the menus and changes made to their content are 
//       directly reflected in the menu.
//--------------------------------------------------------------------------------------
WCHAR g_wszCurrentPlayerName[ XUSER_NAME_SIZE ] = L""; // Holds the current player name, or 
                                                       // a message to the effect that the player has no account name.
WCHAR g_wszIdentityStatus[ STRING_MESSAGE_SIZE ] = L""; // Hold a text message describing the state of identity for the player.

WCHAR g_wszTitleTotalDuration[ STRING_MESSAGE_SIZE ] = L"";
WCHAR g_wszTitleTotalJoules[ STRING_MESSAGE_SIZE ]   = L"";
WCHAR g_wszTitleAverageMET[ STRING_MESSAGE_SIZE ]    = L"";


//--------------------------------------------------------------------------------------
// Name: Menu static strings
// Desc: These strings are used by the menus to describes  active settings.
//--------------------------------------------------------------------------------------
const WCHAR* ROLLUP_PER_YEAR_STRING   = L"Rollup Per Year";
const WCHAR* ROLLUP_PER_MONTH_STRING  = L"Rollup Per Month";
const WCHAR* ROLLUP_PER_DAY_STRING    = L"Rollup Per Day";
const WCHAR* ROLLUP_PER_RECORD_STRING = L"Rollup Per Record";


//--------------------------------------------------------------------------------------
// Name: ITEM_ID_ 
// Desc: Declares unique identifiers for the dynamic menu items
//--------------------------------------------------------------------------------------
#define ITEM_ID_MANAGE_IDENTITY_BUTTON  1
#define ITEM_ID_HWAG_BUTTON             2
#define ITEM_ID_STATS_BUTTON            3
#define ITEM_ID_PER_YEAR_BUTTON         4
#define ITEM_ID_PER_MONTH_BUTTON        5
#define ITEM_ID_PER_DAY_BUTTON          6
#define ITEM_ID_PER_RECORD_BUTTON       7
#define ITEM_ID_MORE_BUTTON             8
#define ITEM_ID_BACK_BUTTON             9
#define ITEM_ID_ROLLUP_TEXT            10


//--------------------------------------------------------------------------------------
// Name: g_aMainMenuItem[]
// Desc: Declares the item layout for the main menu.
//--------------------------------------------------------------------------------------
const DWORD PAUSE_MAIN_COLUMNS  = 24;
const DWORD PAUSE_MAIN_ROWS     = 24;
ATG::NUI_MENU_ITEM g_aMainMenuItem[] =
{
    ATG_NUI_MENU_ITEM_BUTTON(  1,  8,  4,  2,  ITEM_ID_MANAGE_IDENTITY_BUTTON,   L"Sign-In" ),
    ATG_NUI_MENU_ITEM_BUTTON(  1, 11,  4,  2,  ITEM_ID_HWAG_BUTTON,              L"Attributes" ),
    ATG_NUI_MENU_ITEM_BUTTON(  1, 14,  4,  2,  ITEM_ID_STATS_BUTTON,             L"Stats" ),
    ATG_NUI_MENU_ITEM_TEXT(    1,  4, 22,  3,  ATG::NUI_MENU_ITEM_ID_UNDEFINED,  g_wszCurrentPlayerName ),
    ATG_NUI_MENU_ITEM_TEXT(    1, 20, 22,  3,  ATG::NUI_MENU_ITEM_ID_UNDEFINED,  g_wszIdentityStatus ),
};


//--------------------------------------------------------------------------------------
// Name: g_aStatsMenuItem[]
// Desc: Declares the item layout for the stats menu.
//--------------------------------------------------------------------------------------
const DWORD STATS_MENU_COLUMNS  = 24;
const DWORD STATS_MENU_ROWS     = 24;
ATG::NUI_MENU_ITEM g_aStatsMenuItem[] =
{
    ATG_NUI_MENU_ITEM_TEXT(    1,  4, 14,  2,  ITEM_ID_ROLLUP_TEXT,              ROLLUP_PER_RECORD_STRING ),
    ATG_NUI_MENU_ITEM_BUTTON(  1,  4,  4,  2,  ITEM_ID_PER_YEAR_BUTTON,          L"Year" ),
    ATG_NUI_MENU_ITEM_BUTTON(  1,  7,  4,  2,  ITEM_ID_PER_MONTH_BUTTON,         L"Month" ),
    ATG_NUI_MENU_ITEM_BUTTON(  1, 10,  4,  2,  ITEM_ID_PER_DAY_BUTTON,           L"Day" ),
    ATG_NUI_MENU_ITEM_BUTTON(  1, 13,  4,  2,  ITEM_ID_PER_RECORD_BUTTON,        L"Record" ),
    ATG_NUI_MENU_ITEM_FRAME(   8, 15,  4,  6,  ATG::NUI_MENU_ITEM_ID_UNDEFINED ),
    ATG_NUI_MENU_ITEM_FRAME(  12, 15,  4,  6,  ATG::NUI_MENU_ITEM_ID_UNDEFINED ),
    ATG_NUI_MENU_ITEM_TEXT(    8, 15,  4,  2,  ATG::NUI_MENU_ITEM_ID_UNDEFINED,  L"Total Duration" ),
    ATG_NUI_MENU_ITEM_FRAME(   8, 17,  8,  2,  ATG::NUI_MENU_ITEM_ID_UNDEFINED ),
    ATG_NUI_MENU_ITEM_TEXT(    8, 17,  4,  2,  ATG::NUI_MENU_ITEM_ID_UNDEFINED,  L"Total Joules" ),
    ATG_NUI_MENU_ITEM_TEXT(    8, 19,  4,  2,  ATG::NUI_MENU_ITEM_ID_UNDEFINED,  L"Average MET" ),
    ATG_NUI_MENU_ITEM_TEXT(   12, 15,  4,  2,  ATG::NUI_MENU_ITEM_ID_UNDEFINED,  g_wszTitleTotalDuration ),
    ATG_NUI_MENU_ITEM_TEXT(   12, 17,  4,  2,  ATG::NUI_MENU_ITEM_ID_UNDEFINED,  g_wszTitleTotalJoules ),
    ATG_NUI_MENU_ITEM_TEXT(   12, 19,  4,  2,  ATG::NUI_MENU_ITEM_ID_UNDEFINED,  g_wszTitleAverageMET ),
    ATG_NUI_MENU_ITEM_BUTTON( 18, 14,  4,  2,  ITEM_ID_MORE_BUTTON,              L"More..." ),
    ATG_NUI_MENU_ITEM_BUTTON(  1, 20,  4,  2,  ITEM_ID_BACK_BUTTON,              L"Back" ),
};


//--------------------------------------------------------------------------------------
// Name: Fitness API related constants
// Desc: Defines constants that help interprets results fro mthe Fitness API
//--------------------------------------------------------------------------------------
const FLOAT MAX_JOULE_PER_SECONDS = 3219.0 * 1000 / ( 60 * 60 ); // Moderate 2048, Intense 3219, extreme 4096.
const DWORD MAX_VAR_JOULE_PER_SECONDS = ( DWORD )( MAX_JOULE_PER_SECONDS * 0.2f ); // Allow 20% variation
//ELx comment and name correctly the previous vars


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

    VOID ProcessKinectFrame();
    VOID UpdatePlayerInfo( const KinectFrame* pKinectFrame );
    VOID SetRollUpString();
    HRESULT UpdateSummary();
    HRESULT BeginEnumeratingFitnessData();
    HRESULT EnumerateFitnessData();
    HRESULT ProcessReceivedFitnessData( XOVERLAPPED *pOverlapped );

    HRESULT RenderRollingHistogram();
    HRESULT RenderStatsHistogram();


private:
    ATG::NuiMenu m_PageMenu[ SAMPLESTATE_MAX ];
    SAMPLE_STATE m_CurrentPage;

    // General sample data
    ATG::Timer                  m_Timer;           // General usage timer
    ATG::Font                   m_Font;            // Font used to display text
    KinectSensor                m_kinectSensor;    // Sensor array manager used to retrive data from the skeleton and image streams.
    SinglePlayerIdentityManager m_IdentityManager; 
    ATG::NuiVisualization       m_NuiRenderer;     // Used to render the depth map
    HANDLE                      m_hNotification;   // Handle to retrieve system notifications

    // For XN_SYS_SIGNINCHANGED, handle the case where the system sends a spurious notification. 
    // See the FAQ on Xbox 360 Central: https://xds.xbox.com/xbox360/nav.aspx?Page=devsupport/sitefaq.htm#misc17
    // for a description of the workaround    
    static const DWORD TICK_TO_CONFIRM_SIGNIN = 1000;
    SIGNIN_STATE m_eSignInState;
    DWORD m_dwLastSignInChangeTick;

    // Player Data
    SKELETON_IDENTITY_STATE m_IdentityState;
    DWORD                   m_dwTrackingID;
    DWORD                   m_dwUserIndex;

    // Timer data to recover current progress
    ATG::Timer m_FitnessTimer;
    FLOAT      m_fFitnessDataInterval;            // in seconds
    FLOAT      m_fFitnessDataIntervalCountdown;

    // Timer data to simulate different fitness activities
    ATG::Timer m_ActivityTimer;
    FLOAT      m_fActivityDataInterval;           // in seconds
    FLOAT      m_fActivityDataIntervalCountdown;

    // Rolling Joules histogram data
    static const DWORD FITNESS_DATA_MAX = 20;
    DWORD m_dwFitnessDataCount;
    DWORD m_dwFitnessData[ FITNESS_DATA_MAX ];
    DWORD m_dwFitnessDataRef;
    DWORD m_dwMaxJoulesPerSecond;
    DWORD m_dwRawFitnessData;

    // Variables used to retrieve Fitness Records through XEnumerate
    HANDLE      m_hFitnessRecordEnum;
    XOVERLAPPED m_FitnessRecordOverlapped;

    // Latest block of data retreived through XEnumerate()
    static const DWORD FITNESS_STATS_DATA_MAX = 30;
    XFITNESS_RECORD m_FitnessStatsData[ 2 ][ FITNESS_STATS_DATA_MAX ]; // Double buffered so we can display aset a data
                                                                       // while the next one is being retrieved asynchronously.
    XFITNESS_RECORD m_FitnessStatsDataRef;
    DWORD m_StatsDisplayBuffer;
    DWORD m_StatsBusyBuffer;
    FITNESS_ENUMERATOR_STATE m_FitnessEnumState;
    XFITNESS_QUERY_ROLLUP_VALUES m_QueryRollup;
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

    // Confine text drawing to the safe area
    m_Font.SetWindow( ATG::GetTitleSafeArea() );

    // Creating a notification listener to keep track of changes in profile sign-in status
    m_hNotification = XNotifyCreateListener( XNOTIFY_SYSTEM );
    if( m_hNotification == NULL || m_hNotification == INVALID_HANDLE_VALUE )
    { 
        ATG_PrintError( "Unable to create XNotify listener" );
        return E_FAIL;
    }

    // Initialize the simple shaders and the NUI sensor
    ATG::SimpleShaders::Initialize( NULL, NULL );
    RETURN_ON_FAIL( m_kinectSensor.Initialize( NUI_INITIALIZE_FLAG_USES_SKELETON | 
                                               NUI_INITIALIZE_FLAG_USES_DEPTH |
                                               NUI_INITIALIZE_FLAG_USES_COLOR |
                                               NUI_INITIALIZE_FLAG_USES_FITNESS |
                                               NUI_INITIALIZE_FLAG_AUTOMATIC_IDENTITY ) );

    // Initialize the menus for the main screen
    RETURN_ON_FAIL( m_PageMenu[ SAMPLESTATE_MAIN ].Initialize( m_pd3dDevice, m_d3dpp.BackBufferWidth, m_d3dpp.BackBufferHeight, &m_Font ) );
    m_PageMenu[ SAMPLESTATE_MAIN ].SetMenuLayout( g_aMainMenuItem, _countof( g_aMainMenuItem ), PAUSE_MAIN_COLUMNS, PAUSE_MAIN_ROWS );
    m_PageMenu[ SAMPLESTATE_MAIN ].DisableItem( ITEM_ID_HWAG_BUTTON );
    m_PageMenu[ SAMPLESTATE_MAIN ].DisableItem( ITEM_ID_STATS_BUTTON );

    // Initialize the menus for the stats screen
    RETURN_ON_FAIL(  m_PageMenu[ SAMPLESTATE_STATS ].Initialize( m_pd3dDevice, m_d3dpp.BackBufferWidth, m_d3dpp.BackBufferHeight, &m_Font ) );
    m_PageMenu[ SAMPLESTATE_STATS ].SetMenuLayout( g_aStatsMenuItem, _countof( g_aStatsMenuItem ), STATS_MENU_COLUMNS, STATS_MENU_ROWS );

    // Set starting menu page
    m_CurrentPage = SAMPLESTATE_MAIN;

    // Initalize player data
    m_dwTrackingID  = NUI_SKELETON_INVALID_TRACKING_ID;
    m_IdentityState = SKELETON_IDENTITY_STATE_INVALID;
    m_dwUserIndex   = XUSER_INDEX_NONE;

    // Initialize the array that will hold current player Joules expenditure.
    m_fFitnessDataInterval = 1.0f;                            // Poll for current plyer fitness data every second.
    m_fFitnessDataIntervalCountdown = m_fFitnessDataInterval;
    ZeroMemory( &m_dwFitnessData, sizeof( m_dwFitnessData ) );
    m_dwFitnessDataRef = 0;
    m_dwFitnessDataCount = FITNESS_DATA_MAX;
    m_dwMaxJoulesPerSecond = ( DWORD )MAX_JOULE_PER_SECONDS;
    m_dwRawFitnessData = 0;

    // Initialize the fitness activity timer
    m_fActivityDataInterval = 60.0f;                            // Start a new activity every minute
    m_fActivityDataIntervalCountdown = m_fActivityDataInterval;

    // Initialize the array that will hold the enumerated fitness records.
    m_hFitnessRecordEnum = NULL;
    m_FitnessEnumState = NOT_ENUMERATING;
    ZeroMemory( &m_FitnessStatsData, sizeof( m_FitnessStatsData ) );
    ZeroMemory( &m_FitnessStatsDataRef, sizeof( m_FitnessStatsDataRef ) );
    m_StatsDisplayBuffer = 0;
    m_StatsBusyBuffer = 1;
    
    // Default stats screen record rollup value
    m_QueryRollup = XFITNESS_QUERY_ROLLUP_PER_RECORD;

    // Initialize NuiVisualization object to render the depth map
    if( FAILED( m_NuiRenderer.Initialize( m_pd3dDevice, NUI_INITIALIZE_FLAG_USES_SKELETON | NUI_INITIALIZE_FLAG_USES_COLOR ) ) )
    {
        ATG_PrintError( "Picture in Picture initialization failed" );
    }

    // Set skeleton color
    ATG::NUI_VISUALIZATION_SKELETON_RENDER_INFO SkeletonRenderInfo;
    SkeletonRenderInfo.Initialize( SKELETON_COLOR, SKELETON_COLOR );
    for( DWORD dwSkeletonIndex = 0; dwSkeletonIndex < NUI_SKELETON_COUNT; ++ dwSkeletonIndex )
    {
        m_NuiRenderer.SetSkeletonRenderInfo( dwSkeletonIndex, &SkeletonRenderInfo );
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Update()
// Desc: Called once per frame
//--------------------------------------------------------------------------------------
HRESULT Sample::Update()
{
    // Decrement current expenditure and activity reset timers.
    m_fFitnessDataIntervalCountdown  -= ( FLOAT )m_FitnessTimer.GetElapsedTime();
    m_fActivityDataIntervalCountdown -= ( FLOAT )m_ActivityTimer.GetElapsedTime();

    // Process the game controller so developpers can exit the sample by
    // holding both triggers and both bumpers simultaneously.
    ATG::Input::GetMergedInput();

    // Check if next block of fitness records has become available 
    if( m_FitnessEnumState == IO_PENDING && XHasOverlappedIoCompleted( &m_FitnessRecordOverlapped ) )
    {
        ProcessReceivedFitnessData( &m_FitnessRecordOverlapped );
    }

    // Retrieve new data from the sensor array
    DWORD dwPreviousTrackindID = m_dwTrackingID;
    SKELETON_IDENTITY_STATE PreviousIdentityState = m_IdentityState;
    ProcessKinectFrame();

    // Start Fitness tracking as soon as we have a new player
    if( m_dwTrackingID != dwPreviousTrackindID )
    {
        if( dwPreviousTrackindID != NUI_SKELETON_INVALID_TRACKING_ID )
        {
            NuiFitnessStopTracking( dwPreviousTrackindID );
        }

        if( m_dwTrackingID != NUI_SKELETON_INVALID_TRACKING_ID )
        {
            if( m_IdentityState == SKELETON_IDENTITY_STATE_SIGNED_IN )
            {
                assert( m_dwUserIndex != XUSER_INDEX_NONE );
                NuiFitnessStartTracking( NUI_FITNESS_TRACKING_AUTO, m_dwTrackingID, m_dwUserIndex );
            }
            else
            {
                NuiFitnessStartTracking( NUI_FITNESS_TRACKING_AUTO, m_dwTrackingID, XUSER_INDEX_NONE );
            }
        }
    }
    else
    {
        // If the current player just got signed-in we stop the tracking and restart it using the user profile.
        if( m_dwTrackingID != NUI_SKELETON_INVALID_TRACKING_ID && 
            PreviousIdentityState != SKELETON_IDENTITY_STATE_SIGNED_IN && 
            m_IdentityState == SKELETON_IDENTITY_STATE_SIGNED_IN          )
        {
            NuiFitnessStopTracking( dwPreviousTrackindID );

            assert( m_dwUserIndex != XUSER_INDEX_NONE );
            NuiFitnessStartTracking( NUI_FITNESS_TRACKING_AUTO, m_dwTrackingID, m_dwUserIndex );
        }
    }

    // Simulate a title recording activities by stapping and restarting fitness tracking.
    // We keep the activities short for the sample needs but activities can last as long as needed.
    if( m_fActivityDataIntervalCountdown <= 0.0f)
    {
        m_fActivityDataIntervalCountdown = m_fActivityDataInterval;

        if( m_dwTrackingID != NUI_SKELETON_INVALID_TRACKING_ID )
        {
            NuiFitnessStopTracking( dwPreviousTrackindID );

            if( m_IdentityState == SKELETON_IDENTITY_STATE_SIGNED_IN )
            {
                assert( m_dwUserIndex != XUSER_INDEX_NONE );
                NuiFitnessStartTracking( NUI_FITNESS_TRACKING_AUTO, m_dwTrackingID, m_dwUserIndex );
            }
            else
            {
                NuiFitnessStartTracking( NUI_FITNESS_TRACKING_AUTO, m_dwTrackingID, XUSER_INDEX_NONE );
            }
        }
    }

    // Check if it is time to update the current player's expenditure level.
    if( m_fFitnessDataIntervalCountdown <= 0.0f)
    {
        m_fFitnessDataIntervalCountdown = m_fFitnessDataInterval;
        ++ m_dwFitnessDataCount;

        // We can only retrive data if we are tracking a player
        if( m_dwTrackingID != NUI_SKELETON_INVALID_TRACKING_ID )
        {
            NUI_FITNESS_DATA FitnessData;
            HRESULT hr = NuiFitnessGetCurrentFitnessData( m_dwTrackingID, &FitnessData );

            if( SUCCEEDED( hr ) )
            {

                // Compute the Joules expended for the last second
                if( FitnessData.Joules > m_dwFitnessDataRef )
                {
                    m_dwFitnessData[ m_dwFitnessDataCount % FITNESS_DATA_MAX] = FitnessData.Joules - m_dwFitnessDataRef;
                }
                else
                {
                    m_dwFitnessData[ m_dwFitnessDataCount % FITNESS_DATA_MAX] = FitnessData.Joules;
                }

                // Keep the original value before applying any smoothing
                DWORD dwNewRawFitnessData = m_dwFitnessData[ m_dwFitnessDataCount % FITNESS_DATA_MAX];

                // Perform basic smoothing to avoid large fluctuations
                m_dwFitnessData[ m_dwFitnessDataCount % FITNESS_DATA_MAX] = min( m_dwFitnessData[ m_dwFitnessDataCount % FITNESS_DATA_MAX ], 
                                                                                 m_dwFitnessData[ ( m_dwFitnessDataCount - 1 ) % FITNESS_DATA_MAX ] + MAX_VAR_JOULE_PER_SECONDS );

                // Raise the ceiling for the histogram if the player is working out beyond the preset ceiling.
                if( m_dwFitnessData[ m_dwFitnessDataCount % FITNESS_DATA_MAX] > m_dwMaxJoulesPerSecond )
                {
                    m_dwMaxJoulesPerSecond = m_dwFitnessData[ m_dwFitnessDataCount % FITNESS_DATA_MAX];
                }

                // Keep a copy of this for the next time around.
                m_dwFitnessDataRef = FitnessData.Joules;
                m_dwRawFitnessData = dwNewRawFitnessData;
            }
            else
            {
                m_dwFitnessData[ m_dwFitnessDataCount % FITNESS_DATA_MAX ] = 0;
            }
        }
        else
        {
            m_dwFitnessData[ m_dwFitnessDataCount % FITNESS_DATA_MAX ] = 0;
        }
    }
    
    // Enable / disable menu items that require a user profile.
    if( m_dwTrackingID != NUI_SKELETON_INVALID_TRACKING_ID &&  
        m_dwUserIndex != XUSER_INDEX_NONE )
    {
        m_PageMenu[ SAMPLESTATE_MAIN ].EnableItem( ITEM_ID_HWAG_BUTTON );
        m_PageMenu[ SAMPLESTATE_MAIN ].EnableItem( ITEM_ID_STATS_BUTTON );
    }
    else
    {
        m_PageMenu[ SAMPLESTATE_MAIN ].DisableItem( ITEM_ID_HWAG_BUTTON );
        m_PageMenu[ SAMPLESTATE_MAIN ].DisableItem( ITEM_ID_STATS_BUTTON );
    }

    // Atctive the "More" button if additional records are available.
    if( m_FitnessEnumState == DATA_AVAILABLE )
    {
        m_PageMenu[ SAMPLESTATE_STATS ].EnableItem( ITEM_ID_MORE_BUTTON );
    }
    else
    {
        m_PageMenu[ SAMPLESTATE_STATS ].DisableItem( ITEM_ID_MORE_BUTTON );
    }

    // Process player menu selection
    switch( m_PageMenu[ m_CurrentPage ].GetActiveItem() )
    {
        case ATG::NUI_MENU_ITEM_ID_NONE:
            break;

        case ITEM_ID_HWAG_BUTTON:
            XShowNuiFitnessBodyProfileUI( m_dwTrackingID, m_dwUserIndex );
            break;

        case ITEM_ID_MANAGE_IDENTITY_BUTTON:
        {
            XShowNuiSigninUI( m_dwTrackingID, XSSUI_FLAGS_LOCALSIGNINONLY | XSSUI_FLAGS_DISALLOW_GUEST );
            break;
        }

        case ITEM_ID_STATS_BUTTON:
        {
            UpdateSummary();
            SetRollUpString();
            BeginEnumeratingFitnessData();
            m_CurrentPage = SAMPLESTATE_STATS;
            break;
        }

        case ITEM_ID_PER_RECORD_BUTTON:
            m_QueryRollup = XFITNESS_QUERY_ROLLUP_PER_RECORD;
            SetRollUpString();
            BeginEnumeratingFitnessData();
            break;

        case ITEM_ID_PER_DAY_BUTTON:
            m_QueryRollup = XFITNESS_QUERY_ROLLUP_PER_DAY;
            SetRollUpString();
            BeginEnumeratingFitnessData();
            break;

        case ITEM_ID_PER_MONTH_BUTTON:
            m_QueryRollup = XFITNESS_QUERY_ROLLUP_PER_MONTH;
            SetRollUpString();
            BeginEnumeratingFitnessData();
            break;

        case ITEM_ID_PER_YEAR_BUTTON:
            m_QueryRollup = XFITNESS_QUERY_ROLLUP_PER_YEAR;
            SetRollUpString();
            BeginEnumeratingFitnessData();
            break;

        case ITEM_ID_MORE_BUTTON:
            EnumerateFitnessData();
            break;

        case ITEM_ID_BACK_BUTTON:
        {
            m_CurrentPage = SAMPLESTATE_MAIN;
            break;
        }

        default:
            assert( false );
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: SetRollUpString()
// Desc: Sets the stats histogram title according to the currently selected roll up 
//       option.
//--------------------------------------------------------------------------------------
VOID Sample::SetRollUpString()
{
    switch( m_QueryRollup )
    {
        case XFITNESS_QUERY_ROLLUP_PER_RECORD:
            m_PageMenu[ SAMPLESTATE_STATS ].SetItemText( ITEM_ID_ROLLUP_TEXT, ROLLUP_PER_RECORD_STRING );
            break;

        case XFITNESS_QUERY_ROLLUP_PER_DAY:
            m_PageMenu[ SAMPLESTATE_STATS ].SetItemText( ITEM_ID_ROLLUP_TEXT, ROLLUP_PER_DAY_STRING );
            break;

        case XFITNESS_QUERY_ROLLUP_PER_MONTH:
            m_PageMenu[ SAMPLESTATE_STATS ].SetItemText( ITEM_ID_ROLLUP_TEXT, ROLLUP_PER_MONTH_STRING );
            break;

        case XFITNESS_QUERY_ROLLUP_PER_YEAR:
            m_PageMenu[ SAMPLESTATE_STATS ].SetItemText( ITEM_ID_ROLLUP_TEXT, ROLLUP_PER_YEAR_STRING );
            break;

        default:
            assert( false );
    }
}


//--------------------------------------------------------------------------------------
// Name: ProcessKinectFrame()
// Desc: Called once per frame
//--------------------------------------------------------------------------------------
VOID Sample::ProcessKinectFrame()
{
    DWORD dwTrackedSkeletonCount = 0; // Number of actively tracked skeletons, update each frame.

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
            for( DWORD dwSkeletonIndex = 0; dwSkeletonIndex < NUI_SKELETON_COUNT; ++ dwSkeletonIndex )
            {
                if( kinectFrame.SkeletonFrame.SkeletonData[ dwSkeletonIndex ].eTrackingState == NUI_SKELETON_TRACKED )
                {
                    ++ dwTrackedSkeletonCount;
                }
            }

            m_NuiRenderer.SetSkeletons( &kinectFrame.SkeletonFrame );

            // Process changes in the identity of the current player
            m_IdentityManager.Update( &kinectFrame.SkeletonFrame );
            if( m_dwTrackingID != NUI_SKELETON_INVALID_TRACKING_ID )
            {
                // Update player info according to most recent identity status 
                HRESULT hrIdentityManager = m_IdentityManager.GetPlayerState( m_dwTrackingID, &m_IdentityState );
                if( hrIdentityManager == E_NUI_IDENTITY_LOST_TRACK )
                {
                    m_dwTrackingID = NUI_SKELETON_INVALID_TRACKING_ID;
                    m_IdentityState = SKELETON_IDENTITY_STATE_INVALID;
                }
            }

            // Make the player closest to the Kinect sensor, the current player, if no one is assuming this role yet
            if( m_dwTrackingID == NUI_SKELETON_INVALID_TRACKING_ID && dwTrackedSkeletonCount > 0 )
            {
                m_dwTrackingID = GetClosestSkeletonTrackingID( &kinectFrame.SkeletonFrame );

                m_IdentityManager.GetPlayerState( m_dwTrackingID, &m_IdentityState );

                UpdatePlayerInfo( &kinectFrame );
            }

            DWORD dwNotificationID;
            ULONG_PTR ulParam;

            // Process sytem notifications
            while( XNotifyGetNext( m_hNotification, 0, &dwNotificationID, &ulParam ) )
            {
                switch( dwNotificationID )
                {
                    case XN_SYS_SIGNINCHANGED:
                        m_eSignInState = SIGNIN_NOTIFIED;
                        m_dwLastSignInChangeTick = GetTickCount();
                        break;

                    case XN_SYS_NUIBINDINGCHANGED:
                            UpdatePlayerInfo( &kinectFrame );
                        break;
                }
            }

            if (m_eSignInState == SIGNIN_NOTIFIED &&
                GetTickCount() - m_dwLastSignInChangeTick > TICK_TO_CONFIRM_SIGNIN )
            {
                m_eSignInState = SIGNIN_CONFIRMED;
            }

            if( SUCCEEDED( kinectFrame.hrSkeletonRetrieved ) &&  SUCCEEDED( kinectFrame.pDepthFrame320x240 ) &&  SUCCEEDED( kinectFrame.hr80x60Retrieved ) &&
                m_dwTrackingID != NUI_SKELETON_INVALID_TRACKING_ID )
            {
                // Update current player information based on the related identity status
                switch( m_IdentityState )
                {
                    case SKELETON_IDENTITY_STATE_INVALID:
                        ATG_PrintError( "Invalid identity state detected" );
                        assert( false );
                        break;

                    case SKELETON_IDENTITY_STATE_BUSY:
                    {               
                        DWORD dwQualityFlags;
                        HRESULT hrQF = m_IdentityManager.GetQualityFlags( m_dwTrackingID, &dwQualityFlags );
                        if( SUCCEEDED( hrQF ) && dwQualityFlags )
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
                        g_wszIdentityStatus[ 0 ] = '\0';
                        break;

                    case SKELETON_IDENTITY_STATE_SIGNED_IN:
                        UpdatePlayerInfo( &kinectFrame );
                        g_wszIdentityStatus[ 0 ] = '\0';
                        break;

                    default:
                        assert( false );
                }

                m_PageMenu[ m_CurrentPage ].Update( &kinectFrame.SkeletonFrame, m_dwTrackingID, kinectFrame.pDepthFrame320x240, kinectFrame.pDepthFrame80x60 );
            }

             // Check if new data is available for the color stream and update the related NuiRenderer
            if( SUCCEEDED( kinectFrame.hrImageRetrieved ) )
            {
                m_NuiRenderer.SetColorTexture( kinectFrame.pImageFrame->pFrameTexture, &kinectFrame.pImageFrame->ViewArea );
            }

        }

        m_kinectSensor.ReleaseKinectFrame( &kinectFrame );
    }
    else
    {
        ATG::NuiPrintError( hr, "KinectSensor_AquireKinectFrame" );
    }
}


//--------------------------------------------------------------------------------------
// Name: Sample::UpdateSummary()
// Desc: Updates the overall summary for fitness activities using the player's profile.
//--------------------------------------------------------------------------------------
HRESULT Sample::UpdateSummary()
{
    assert( m_dwUserIndex != XUSER_INDEX_NONE );

    // Start / top tracking to flush the latest records to the profile.
    NuiFitnessStopTracking( m_dwTrackingID );
    NuiFitnessStartTracking( NUI_FITNESS_TRACKING_AUTO, m_dwTrackingID, m_dwUserIndex );

    NUI_FITNESS_TITLE_SUMMARY summary;

    g_wszTitleTotalDuration[ 0 ] = '\0';
    g_wszTitleTotalJoules[ 0 ]   = '\0';
    g_wszTitleAverageMET[ 0 ]    = '\0';

    // NOTE: A "super" title can also call NuiFitnessGetOverallSummary() to retrieve overal values for all titles 
    // played by the player.
    HRESULT hr = NuiFitnessGetCurrentTitleSummary( m_dwUserIndex, &summary );
    if( SUCCEEDED( hr ) )
    {
        swprintf_s( g_wszTitleTotalDuration, L"%dh :%02dm :%02ds", summary.TotalDuration / 60 / 60, summary.TotalDuration / 60, summary.TotalDuration % 60 );
        swprintf_s( g_wszTitleTotalJoules,   L"%d", summary.TotalJoules );
        swprintf_s( g_wszTitleAverageMET,    L"%f", summary.AverageMET );
    }

    return hr;
}


//--------------------------------------------------------------------------------------
// Name: Sample::BeginEnumeratingFitnessData()
// Desc: Request the first block of Fitness data from the user profile. The opration 
//       will run asynchronously.
//--------------------------------------------------------------------------------------
HRESULT Sample::BeginEnumeratingFitnessData()
{
    assert( m_dwUserIndex != XUSER_INDEX_NONE );

    XFITNESS_QUERY_PARAMETERS query = {0};

    // Specify the granularity of the search.
    query.QueryRollupValues = m_QueryRollup;

    // Set the user index that we want to query.
    query.UserIndexToQuery = m_dwUserIndex;

    // Termionate current enumeration, if any.
    if( m_hFitnessRecordEnum != NULL )
    {
        XCloseHandle( m_hFitnessRecordEnum );
        m_hFitnessRecordEnum = NULL;
    }

    // Create the Query handler.
    DWORD dwBufferSize;
    HRESULT hr = XFitnessQueryCreateEnumerator(m_dwUserIndex, // The user doing the search.
                                               &query,
                                               FITNESS_STATS_DATA_MAX,
                                               &dwBufferSize,
                                               &m_hFitnessRecordEnum);
    
    // Retrieve the first block of data
    if( SUCCEEDED( hr ) && ( m_hFitnessRecordEnum !=  NULL ) )
    {
        assert( dwBufferSize == sizeof( m_FitnessStatsData[ m_StatsBusyBuffer ] ) );
        ZeroMemory( &m_FitnessRecordOverlapped, sizeof( m_FitnessRecordOverlapped ) );
        hr = EnumerateFitnessData();
    }

    return hr;
}


//--------------------------------------------------------------------------------------
// Name: Sample::EnumerateFitnessData()
// Desc: Requests the next block of fitness data stored in the player's profile.
//--------------------------------------------------------------------------------------
HRESULT Sample::EnumerateFitnessData()
{
    HRESULT hr = E_FAIL;

    if( ( m_hFitnessRecordEnum !=  NULL ) )
    {
        ZeroMemory( &m_FitnessStatsData[ m_StatsBusyBuffer ], sizeof( m_FitnessStatsData[ m_StatsBusyBuffer ] ) );

        DWORD dwError = XEnumerate( m_hFitnessRecordEnum, 
                                   ( void* )m_FitnessStatsData[ m_StatsBusyBuffer ], // Cast XFITNESS_RECORD* to void*.
                                   sizeof( m_FitnessStatsData[ m_StatsBusyBuffer ] ), 
                                   NULL, 
                                   &m_FitnessRecordOverlapped );

        if( dwError == ERROR_IO_PENDING )
        {
            m_FitnessEnumState = IO_PENDING;
            hr = S_OK;
        }
    }

    return hr;
}


//--------------------------------------------------------------------------------------
// Name: Sample::ProcessReceivedFitnessData()
// Desc: Process a newly received block of fitness data
//--------------------------------------------------------------------------------------
HRESULT Sample::ProcessReceivedFitnessData( XOVERLAPPED *pOverlapped )
{
    // Display the just received black and use the old one to recover the next block of data.
    DWORD dwTemp = m_StatsDisplayBuffer;
    m_StatsDisplayBuffer = m_StatsBusyBuffer;
    m_StatsBusyBuffer = dwTemp;

    // Determine if more records are available.
    if( pOverlapped->InternalHigh == FITNESS_STATS_DATA_MAX )
    {
        m_FitnessEnumState = DATA_AVAILABLE;
    }
    else
    {
        XCloseHandle( m_hFitnessRecordEnum );
        m_hFitnessRecordEnum = NULL;

        m_FitnessEnumState = NOT_ENUMERATING;

    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Sample::Render()
// Desc: Sets up render states, clears the viewport, and renders the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Render()
{
    
    // Default color
    const D3DCOLOR DEFAULT_COLOR = D3DCOLOR_RGBA( 255, 255, 255, 255 );

    m_Timer.MarkFrame();

    //Draw a gradient filled background
    ATG::RenderBackground( D3DCOLOR_ARGB( 255, 0, 0, 255 ), D3DCOLOR_ARGB( 255, 0, 0, 0 ) );

    switch( m_CurrentPage )
    {
        case SAMPLESTATE_MAIN:
        {
            // Origin of the color image on screen
            const DWORD dwColorImageX = ( DWORD )( ( m_d3dpp.BackBufferWidth - COLOR_IMAGE_RENDER_WIDTH ) * 0.5f );
            const DWORD dwColorImageY = 200;

            // Show the color image
            m_NuiRenderer.RenderColorStream( ( FLOAT )dwColorImageX, ( FLOAT )dwColorImageY, ( FLOAT )COLOR_IMAGE_RENDER_WIDTH, ( FLOAT )COLOR_IMAGE_RENDER_HEIGHT );
    
            // Show the skeletons over the color image, cliping the skeleton to the image area and converting it from depth map to color space
            m_NuiRenderer.RenderSkeletons( ( FLOAT )dwColorImageX, ( FLOAT )dwColorImageY, ( FLOAT )COLOR_IMAGE_RENDER_WIDTH, ( FLOAT )COLOR_IMAGE_RENDER_HEIGHT, TRUE, TRUE );

            RenderRollingHistogram();
            break;
        }

        case SAMPLESTATE_STATS:
            RenderStatsHistogram();
            break;

        default:
            assert( FALSE );
    }

    // Render the menu for the currently slected screen
    m_PageMenu[ m_CurrentPage ].Render();

    // Show title and frame rate
    m_Font.Begin();
    m_Font.SetScaleFactors( 1.2f, 1.2f );
    m_Font.DrawText( 0, 0, DEFAULT_COLOR,  L"Fitness" );

    m_Font.SetScaleFactors( 1.0f, 1.0f );
    m_Font.DrawText( 0, 0, DEFAULT_COLOR, m_Timer.GetFrameRate(), ATGFONT_RIGHT );
    m_Font.End();

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Sample::RenderRollingHistogram()
// Desc: Renders the real-time histogram for Joules being expended by the player.
//--------------------------------------------------------------------------------------
HRESULT Sample::RenderRollingHistogram()
{
    D3DRECT rect = { 1025, 250, 1225, 425 };
    ATG::DebugDraw::DrawScreenSpaceRect( rect, 3, 0xffffffff );

    DWORD barWidth = ( rect.x2 - rect.x1 ) / FITNESS_DATA_MAX;
    assert( m_dwFitnessDataCount >= ( FITNESS_DATA_MAX - 1 ) );
    for( DWORD i = 0; i < FITNESS_DATA_MAX; ++ i )
    {
        DWORD index = i + m_dwFitnessDataCount - ( FITNESS_DATA_MAX - 1 );
        if( m_dwFitnessData[ index % FITNESS_DATA_MAX ] > 0 )
        {
            D3DRECT bar = rect;
            bar.x1 += i * barWidth;
            bar.x2 = bar.x1 + barWidth;
            DWORD dwBarHeight = bar.y2 - bar.y1;
            FLOAT factor = m_dwFitnessData[ index % FITNESS_DATA_MAX ] / ( FLOAT )m_dwMaxJoulesPerSecond;
            bar.y1 = bar.y2 - ( LONG )( dwBarHeight * factor );
            ATG::DebugDraw::DrawScreenSpaceRect( bar, 3, 0xffffffff );
        }
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Sample::RenderStatsHistogram()
// Desc: Renders the MET average values from past record from the player's profile.
//--------------------------------------------------------------------------------------
HRESULT Sample::RenderStatsHistogram()
{
    D3DRECT rect = { 300, 200, 1225, 400 };
    ATG::DebugDraw::DrawScreenSpaceRect( rect, 3, 0xffffffff );

    FLOAT fBarWidth = ( FLOAT )( rect.x2 - rect.x1 ) / FITNESS_STATS_DATA_MAX;
    for( DWORD i = 0; i < FITNESS_STATS_DATA_MAX; ++ i )
    {
        if( m_FitnessStatsData[ m_StatsDisplayBuffer ][ i ].AverageMET > 0.0f )
        {
            D3DRECT bar = rect;
            bar.x1 += ( LONG )( i * fBarWidth ) + 4;
            bar.x2 = bar.x1 + ( LONG )fBarWidth - 8;
            DWORD dwBarHeight = bar.y2 - bar.y1;
            FLOAT factor = m_FitnessStatsData[ m_StatsDisplayBuffer ][ i ].AverageMET / 20.0f;
            bar.y1 = bar.y2 - ( LONG )( dwBarHeight * factor );
            ATG::DebugDraw::DrawScreenSpaceRect( bar, 0, 0xffffffff );
        }
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Sample::UpdatePlayerInfo()
// Desc: Updates the profile information pertaining to the  current player.
//--------------------------------------------------------------------------------------
VOID Sample::UpdatePlayerInfo( const KinectFrame* pKinectFrame )
{

    if( m_dwTrackingID != NUI_SKELETON_INVALID_TRACKING_ID )
    {
        const NUI_SKELETON_DATA* pSkeletonData = GetSkeletonDataFromTrackingID( &pKinectFrame->SkeletonFrame, m_dwTrackingID );
        assert( pSkeletonData != NULL );
    
        if( pSkeletonData->dwUserIndex == XUSER_INDEX_NONE )
        {
            m_dwUserIndex = XUSER_INDEX_NONE;
            wcscpy_s( g_wszCurrentPlayerName, L"Guest Player" );
        }
        else
        {
            XUSER_SIGNIN_INFO UserInfo;
            HRESULT hrUserGetInfo = XUserGetSigninInfo( pSkeletonData->dwUserIndex, XUSER_GET_SIGNIN_INFO_OFFLINE_XUID_ONLY, &UserInfo );
            if( SUCCEEDED( hrUserGetInfo ) )
            {
                m_dwUserIndex = pSkeletonData->dwUserIndex;
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