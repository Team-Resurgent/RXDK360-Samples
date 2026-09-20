//--------------------------------------------------------------------------------------
// XContentCrossTitle.cpp
//
// The sample shows how to enumerate XContent and Marketplace data for various titles
// using the XContentCrossTitle API set.
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include <xtl.h>
#include <d3d9.h>
#include <xam.h>
#include <xbox.h>
#include "AtgApp.h"
#include "AtgDebugDraw.h"
#include "AtgFont.h"
#include "AtgHelp.h"
#include "AtgInput.h"
#include "AtgResource.h"
#include "AtgSignIn.h"
#include "AtgSimpleShaders.h"
#include "AtgUtil.h"

//--------------------------------------------------------------------------------------
// Maximum number of content name data structures allowed.
//--------------------------------------------------------------------------------------
static const DWORD MAX_DATA_COUNT = 10;

//--------------------------------------------------------------------------------------
// Save root name to use. This can be any valid directory name.
//--------------------------------------------------------------------------------------
static CHAR g_szSaveRoot[] = "sample";

//--------------------------------------------------------------------------------------
// Invalid user index
//--------------------------------------------------------------------------------------
static const DWORD INVALID_USER_INDEX = 0xffffffff;

//--------------------------------------------------------------------------------------
// Color values
//--------------------------------------------------------------------------------------
#define SEL_COLOR           0xffff0000          // selection color
#define UNSEL_COLOR         0xffffffff          // unselection color

#define MSG_COLOR           0xffffffff          // message color
#define INFO_COLOR          0xffffff00          // information display color

#define TOP_BACK_COLOR      0xffbbffbb          // background gradient colors
#define BOTTOM_BACK_COLOR   0xff000000

//--------------------------------------------------------------------------------------
// Enum to represent which sample's data we'd like to enumerate. This is passed to
// EnumerateContentNames.
//--------------------------------------------------------------------------------------
enum SAMPLETOENUMERATE
{
    MARKETPLACESAMPLE,
    XCONTENTSAMPLE,
};

//--------------------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//--------------------------------------------------------------------------------------
ATG::HELP_CALLOUT g_HelpCallouts[] =
{
    { ATG::HELP_BACK_BUTTON, ATG::HELP_PLACEMENT_2, L"Display\nhelp" },
    { ATG::HELP_A_BUTTON,    ATG::HELP_PLACEMENT_1, L"Load Selected Content" },
    { ATG::HELP_DPAD,        ATG::HELP_PLACEMENT_1, L"Select Saved Game" },
    { ATG::HELP_B_BUTTON,    ATG::HELP_PLACEMENT_1, L"Signin" },
    { ATG::HELP_Y_BUTTON,    ATG::HELP_PLACEMENT_1, L"Select Device" },
    { ATG::HELP_X_BUTTON,    ATG::HELP_PLACEMENT_1, L"Change Sample to List" },
};
static const DWORD NUM_HELP_CALLOUTS = ARRAYSIZE(g_HelpCallouts);


//--------------------------------------------------------------------------------------
// Name: Sample
// Desc: Main class to run this application. Most functionality is inherited
//       from the ATG::Application base class.
//--------------------------------------------------------------------------------------
class Sample : public ATG::Application
{
    ATG::Timer      m_Timer;
    ATG::Font       m_Font;
    ATG::Help       m_Help;
    BOOL            m_bDrawHelp;
    XCONTENT_CROSS_TITLE_DATA m_ContentData[MAX_DATA_COUNT];  // Data containing display names
    DWORD           m_dwContentDataCount;           // Number of data elements
    DWORD           m_dwSelectedIndex;              // Selected element in UI
    WCHAR           m_szMessage[128];               // Message to be displayed in UI
    XOVERLAPPED     m_Overlapped;                   // Overlapped object for device selector UI
    XCONTENTDEVICEID m_DeviceID;                    // Device identifier returned by device selector UI
    BOOL            m_bFirstSignin;                 // Flag indicating first signin
    BOOL            m_bDeviceUIActive;              // Is the device UI showing
    HANDLE          m_hNotification;
    WCHAR           m_szPackageContents[MAX_DATA_COUNT][128]; // First file found in the XContent package
    DWORD           m_dwPackageContentsCount;       // Number of files in the container
    SAMPLETOENUMERATE m_sampleToEnum;               // Sample whose content we'll enumerate (XContent or Marketplace)

    VOID    EnumerateContentNames();
    DWORD   DisplayPackageContents( XCONTENT_CROSS_TITLE_DATA* ContentData );
    VOID    ShowDeviceUI();

private:
    virtual HRESULT Initialize();
    virtual HRESULT Update();
    virtual HRESULT Render();
};


//--------------------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the program
//--------------------------------------------------------------------------------------
VOID __cdecl main()
{
    Sample atgApp;
    ATG::GetVideoSettings( &atgApp.m_d3dpp.BackBufferWidth, &atgApp.m_d3dpp.BackBufferHeight );
    atgApp.Run();
}


//--------------------------------------------------------------------------------------
// Name: Initialize
// Desc: This creates all device-dependent display objects.
//--------------------------------------------------------------------------------------
HRESULT Sample::Initialize()
{
    m_bDrawHelp              = FALSE;
    m_dwSelectedIndex        = 0;
    m_dwContentDataCount     = 0;
    m_dwPackageContentsCount = 0;
    m_sampleToEnum = MARKETPLACESAMPLE;

    // Create the font
    if( FAILED( m_Font.Create( "game:\\Media\\Fonts\\Arial_16.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Confine text drawing to the title safe area
    m_Font.SetWindow( ATG::GetTitleSafeArea() );

    // Initialize the simple shaders
    ATG::SimpleShaders::Initialize( NULL, NULL );

    // Create the help
    if( FAILED( m_Help.Create( "game:\\Media\\Help\\Help.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    // Initialize Live
    if( FAILED( XOnlineStartup() ) )
        return E_FAIL;

    // Initialize logon
    ATG::SignIn::Initialize( 1, 1, FALSE, 1 );

    m_hNotification = XNotifyCreateListener( XNOTIFY_SYSTEM );
    if( m_hNotification == NULL )
        return E_FAIL;

    // Start sample by showing the signin UI for one user
    m_bFirstSignin = TRUE;
    ATG::SignIn::ShowSignInUI();

    // Clear message string
    m_szMessage[0] = L'\0';

    // Clear the file name string
    ZeroMemory(m_szPackageContents,  ARRAYSIZE(m_szPackageContents[0]) * MAX_DATA_COUNT);

    m_bDeviceUIActive = FALSE;
    m_DeviceID = 0;

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Update
// Desc: Called once per frame, the call is the entry point for animating
//       the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Update()
{
    // Update login
    ATG::SignIn::Update();

    // If we're not signed in, wait until we are
    if( !ATG::SignIn::AreUsersSignedIn() || ATG::SignIn::IsSystemUIShowing() )
    {
        return S_OK;
    }

    DWORD dwNotificationId;
    ULONG ulParam;
    if( XNotifyGetNext( m_hNotification, 0, &dwNotificationId, &ulParam ) )
    {
        switch( dwNotificationId )
        {
        case XN_SYS_STORAGEDEVICESCHANGED:
            swprintf_s( m_szMessage, L"Devices have been removed or inserted." );
            ShowDeviceUI();
            break;
        }
    }

    // Get input from all the gamepads
    ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput();

    // Toggle help
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
        m_bDrawHelp = !m_bDrawHelp;

    // Show the signin UI
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_B )
    {
        m_dwSelectedIndex = 0;
        ATG::SignIn::ShowSignInUI();
    }

    // Show the device selector UI the first time the user has signed in
    if( m_bFirstSignin )
    {
        m_bFirstSignin = FALSE;
        ShowDeviceUI();
    }

    // Wait for the device selector to close
    if( m_bDeviceUIActive && XHasOverlappedIoCompleted( &m_Overlapped ) )
    {
        if( m_DeviceID == XCONTENTDEVICE_ANY )
        {
            ShowDeviceUI();
        }
        else
        {
            m_bDeviceUIActive = FALSE;
        }
        EnumerateContentNames();
    }

    // Load selected data
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_A )
    {
        if( m_dwSelectedIndex < m_dwContentDataCount )
        {
            DWORD dwErr = DisplayPackageContents( &m_ContentData[m_dwSelectedIndex] );
            if( dwErr == ERROR_SUCCESS )
            {
                wcscpy_s( m_szMessage, L"Contents Listed..." );
            }
            else if( dwErr == ERROR_FILE_CORRUPT || dwErr == ERROR_DISK_CORRUPT )
            {
                wcscpy_s( m_szMessage, L"Game is Corrupt" );
            }
            else
            {
                wcscpy_s( m_szMessage, L"Load Failed..." );
            }
        }
    }

    // Enumerate Marketplace sample data
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_X )
    {
        if ( m_sampleToEnum == MARKETPLACESAMPLE )
            m_sampleToEnum = XCONTENTSAMPLE;
        else
            m_sampleToEnum = MARKETPLACESAMPLE;

        EnumerateContentNames();
    }

    // Allow user to select among content display names
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_UP )
    {
        if( m_dwSelectedIndex > 0 )
        {
            m_dwSelectedIndex--;
        }
        m_szMessage[0] = L'\0';
        ZeroMemory(m_szPackageContents,  ARRAYSIZE(m_szPackageContents[0]) * MAX_DATA_COUNT);
    }

    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_DOWN )
    {
        if( m_dwSelectedIndex < ( m_dwContentDataCount - 1 ) )
        {
            m_dwSelectedIndex++;
        }
        m_szMessage[0] = L'\0';
        ZeroMemory(m_szPackageContents,  ARRAYSIZE(m_szPackageContents[0]) * MAX_DATA_COUNT);
    }

    // Show device selector UI
    if( !m_bDeviceUIActive && pGamepad->wPressedButtons & XINPUT_GAMEPAD_Y )
    {
        ShowDeviceUI();
    }

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Name: Render
// Desc: Sets up render states, clears the viewport, and renders the scene.
//--------------------------------------------------------------------------------------
HRESULT Sample::Render()
{
    // Draw a gradient filled background
    ATG::RenderBackground( TOP_BACK_COLOR, BOTTOM_BACK_COLOR );

    // Show title, frame rate, and help
    m_Timer.MarkFrame();
    if( m_bDrawHelp )
    {
        m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
    }
    else if( ATG::SignIn::AreUsersSignedIn() && !ATG::SignIn::IsSystemUIShowing() )
    {
        m_Font.Begin();
        m_Font.SetScaleFactors( 1.2f, 1.2f );
        m_Font.DrawText( 0, 0, MSG_COLOR, L"XContentCrossTitle" );
        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( 0, 0, INFO_COLOR, m_Timer.GetFrameRate(), ATGFONT_RIGHT );

        // Display list of commands

        // Draw list of content display names
        m_Font.SetScaleFactors( 1.f, 1.f );
        m_Font.DrawText( 0, -34,   MSG_COLOR, GLYPH_B_BUTTON L" Re-sign in\n" );
        m_Font.DrawText( 0, -10,   MSG_COLOR, GLYPH_Y_BUTTON L" Select device\n" );
        m_Font.DrawText( 200, -34, MSG_COLOR, GLYPH_A_BUTTON L" List contents\n" );
        m_Font.DrawText( 200, -10, MSG_COLOR, GLYPH_X_BUTTON L" Change Sample\n" );

        m_Font.SetScaleFactors( 1.3f, 1.3f );
        switch ( m_sampleToEnum )
        {
        case XCONTENTSAMPLE:
            m_Font.DrawText( 36, 32, INFO_COLOR, L"XContent Sample Data Display Names\n" );
            break;
        case MARKETPLACESAMPLE:
            m_Font.DrawText( 36, 32, INFO_COLOR, L"Marketplace Sample Data Display Names\n" );
            break;
        }

        m_Font.SetScaleFactors( 1.0f, 1.0f );

        // Display the package names
        if ( 0 != m_dwContentDataCount )
        {
            for( DWORD i = 0; i < m_dwContentDataCount; ++i )
            {
                FLOAT fTextX;
                FLOAT fTextY;
                if( i < ( MAX_DATA_COUNT / 2 ) )
                {
                    fTextX = 0;
                    fTextY = (FLOAT)( 70 + ( i * 40 ) );
                }
                else
                {
                    fTextX = 300;
                    fTextY = (FLOAT)( 70 + ( ( i - ( MAX_DATA_COUNT / 2 ) ) * 40 ) );
                }

                // Display content name
                WCHAR szDisplayName[256];

                swprintf_s( szDisplayName, L"%s\n", m_ContentData[i].szDisplayName );

                m_Font.DrawText( 60 + fTextX, fTextY, i == m_dwSelectedIndex ? SEL_COLOR : UNSEL_COLOR, szDisplayName );
            }
        }
        else
        {
            m_Font.SetScaleFactors( 1.3f, 1.3f );
            switch ( m_sampleToEnum )
            {
            case MARKETPLACESAMPLE:
                m_Font.DrawText( 60, 70, UNSEL_COLOR, L"No content found. Run the Marketplace Sample first.");
                break;
            case XCONTENTSAMPLE:
                m_Font.DrawText( 60, 70, UNSEL_COLOR, L"No content found. Run the XContent Sample first.");
                break;
            }
        }

        // Display package contents
        for( DWORD i = 0; i < m_dwPackageContentsCount; ++i )
        {
            FLOAT fTextX;
            FLOAT fTextY;
            if( i < ( MAX_DATA_COUNT / 2 ) )
            {
                fTextX = 0;
                fTextY = (FLOAT)( 70 + ( i * 40 ) );
            }
            else
            {
                fTextX = 300;
                fTextY = (FLOAT)( 70 + ( ( i - ( MAX_DATA_COUNT / 2 ) ) * 40 ) );
            }

            // Display file name
            WCHAR szDisplayName[256];

            swprintf_s( szDisplayName, L"%s\n", m_szPackageContents[i] );

            m_Font.DrawText( 450 + fTextX, fTextY, UNSEL_COLOR, szDisplayName );
        }


        // Display message
        m_Font.DrawText( 0, -60, MSG_COLOR, m_szMessage );

        m_Font.End();
    }
    else
    {
        m_Font.Begin();
        m_Font.SetScaleFactors( 1.0f, 1.0f );

        m_Font.DrawText( 40.0f, 100.0f, MSG_COLOR, L"Please use the Guide to signin." );

        m_Font.End();
    }

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}



//--------------------------------------------------------------------------------------
// Name: ShowDeviceUI
// Desc: Display the device selector UI.
//--------------------------------------------------------------------------------------
VOID Sample::ShowDeviceUI()
{
    DWORD dwRet;

    ZeroMemory( &m_Overlapped, sizeof( XOVERLAPPED ) );
    m_DeviceID = XCONTENTDEVICE_ANY;
    ULARGE_INTEGER iBytesRequested = {0};

    dwRet = XShowDeviceSelectorUI( ATG::SignIn::GetSignedInUser(), // User to receive input from
                                   XCONTENTTYPE_SAVEDGAME,   // List only save game devices
                                   0,
                                   iBytesRequested,         // How much data should the device have?
                                   &m_DeviceID,             // Return selected device information
                                   &m_Overlapped );

    assert( dwRet == ERROR_IO_PENDING );

    m_bDeviceUIActive = TRUE;
}


//--------------------------------------------------------------------------------------
// Name: EnumerateContentNames
// Desc: Enumerates the display names for the saved game content type.
//--------------------------------------------------------------------------------------
VOID Sample::EnumerateContentNames()
{
    HANDLE hEnum;
    DWORD cbBuffer;
    DWORD dwContentType = XCONTENTTYPE_SAVEDGAME;
    DWORD dwUserIndex = ATG::SignIn::GetSignedInUser();

    m_szMessage[0] = L'\0';
    ZeroMemory(m_szPackageContents,  ARRAYSIZE(m_szPackageContents[0]) * MAX_DATA_COUNT);

    // Determine which content type we're enumerating based on the sample requested
    switch ( m_sampleToEnum )
    {
    case MARKETPLACESAMPLE:
        dwContentType = XCONTENTTYPE_MARKETPLACE;
        dwUserIndex = XUSER_INDEX_ANY;  // In order to enum marketplace data available to everyone, you must pass in XUSER_INDEX_ANY.
        break;
    case XCONTENTSAMPLE:
        dwContentType = XCONTENTTYPE_SAVEDGAME;
        dwUserIndex = ATG::SignIn::GetSignedInUser();
        break;
    }

    // Create enumerator for the selected device. Note that we can enumerate data
    // across all devices using XCONTENTDEVICE_ANY instead of m_DeviceID
    DWORD dwRet;

    dwRet = XContentCreateCrossTitleEnumerator( dwUserIndex,        // Access data associated with this user
                                                m_DeviceID,         // Pass in selected device
                                                dwContentType,      // Saved game data or marketplace data
                                                0,                  // No special flags
                                                1,                  // Cross title enumeration returns 1 item at a time
                                                &cbBuffer,
                                                &hEnum );

    if( dwRet != ERROR_SUCCESS )
    {
        ATG::FatalError( "EnumerateContentNames: Couldn't create content enumerator.\n" );
    }

    // Enumerate display names
    m_dwContentDataCount = 0;
    DWORD dwEnumCount = 0;

    // XEnumerateCrossTitle returns one item at a time, unlike XEnumerate
    while( XEnumerateCrossTitle( hEnum, &m_ContentData[m_dwContentDataCount], sizeof( XCONTENT_CROSS_TITLE_DATA ),
                                 &dwEnumCount, NULL ) == ERROR_SUCCESS )
    {
        m_dwContentDataCount += dwEnumCount;

        // Don't overflow
        if ( m_dwContentDataCount == MAX_DATA_COUNT )
            break;
    }

    CloseHandle( hEnum );
}


//--------------------------------------------------------------------------------------
// Name: DisplayPackageContents
// Desc: Read the game data of another title. Here we'll enumerate the files in the
//       container, but we won't actually read the file contents. See the XContent
//       sample for more information on doing that.
//--------------------------------------------------------------------------------------
DWORD Sample::DisplayPackageContents( XCONTENT_CROSS_TITLE_DATA* ContentData )
{
    DWORD dwUserIndex = ATG::SignIn::GetSignedInUser();

    if ( MARKETPLACESAMPLE == m_sampleToEnum )
        dwUserIndex = XUSER_INDEX_ANY;

    // Create event for asynchronous reading
    HANDLE hEventComplete = CreateEvent( NULL, FALSE, FALSE, NULL );

    if( hEventComplete == NULL )
        ATG::FatalError( "DisplayPackageContents: Couldn't create event.\n" );

    XOVERLAPPED xov = {0};
    xov.hEvent = hEventComplete;

    // Mount the container
    ULARGE_INTEGER uliContentSize = {0};
    DWORD dwErr = XContentCrossTitleCreate( dwUserIndex, g_szSaveRoot, ContentData,
                                            XCONTENTFLAG_OPENEXISTING, NULL, NULL, 0, uliContentSize, &xov );
    if( dwErr != ERROR_IO_PENDING )
    {
        CloseHandle( hEventComplete );
        ATG::DebugSpew( "DisplayPackageContents: XContentCrossTitleCreate failed.\n" );
        return dwErr;
    }

    dwErr = XGetOverlappedResult( &xov, NULL, TRUE );
    if( dwErr == ERROR_SUCCESS )
    {
        // Display the contents of the container
        WIN32_FIND_DATA fd;
        HANDLE hFind;
        CHAR szSrchString[ MAX_PATH ];
        m_dwPackageContentsCount = 0;

        sprintf_s( szSrchString, "%s:\\*.*", g_szSaveRoot );
        hFind = FindFirstFile( szSrchString, &fd );

        if ( hFind != INVALID_HANDLE_VALUE )
        {
            do
            {
                MultiByteToWideChar( CP_ACP, 0, fd.cFileName, strlen( fd.cFileName ) + 1,
                                     m_szPackageContents[m_dwPackageContentsCount], 
                                     ARRAYSIZE( m_szPackageContents[m_dwPackageContentsCount] ) );

                ++m_dwPackageContentsCount;

                if ( m_dwPackageContentsCount == MAX_DATA_COUNT )
                    break;
            } while ( FindNextFile( hFind, &fd ) );
            FindClose( hFind );
        }
    }
    else
    {
        XContentClose( g_szSaveRoot, &xov );
        XGetOverlappedResult( &xov, NULL, TRUE );
        CloseHandle( hEventComplete );

        ATG::DebugSpew( "DisplayPackageContents: XContentCrossTitleCreate failed. Error = %08x\n", dwErr );
        return dwErr;
    }

    // We're done, so close the container
    XContentClose( g_szSaveRoot, &xov );

    // Wait for XCloseContent to complete
    XGetOverlappedResult( &xov, NULL, TRUE );

    CloseHandle( hEventComplete );

    return ERROR_SUCCESS;
}