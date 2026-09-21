//--------------------------------------------------------------------------------------
// XStorage.cpp
//
// The sample demonstrates the usage of the XStorage API. The XStorage API allows for
// storing and retreiving custom data on the Live servers. There are three kinds of
// storage facilities: Game Clip, Per Title and Per User. Each kind is meant for a 
// specific purpose and offers different data capacities. Game clips can be at most
// 11 MB in size and a title can store up to 1 GB of game clips. Per title storage
// files can be at most 5 MB in size and the title can store at most 50 files. Per user
// storage is limited to 8 files of at most 64 KB.
//
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include <xtl.h>
#include <d3d9.h>
#include <xam.h>
#include "AtgApp.h"
#include "AtgFont.h"
#include "AtgHelp.h"
#include "AtgInput.h"
#include "AtgResource.h"
#include "AtgSignIn.h"
#include "AtgUtil.h"
#include "AtgSimpleShaders.h"
#include "AtgDebugDraw.h"

#include "XStorage.spa.h"


//--------------------------------------------------------------------------------------
// Color values
//--------------------------------------------------------------------------------------
const DWORD         SEL_COLOR = 0xffff0000;         // selection color
const DWORD         UNSEL_COLOR = 0xffffffff;         // unselection color

const DWORD         MSG_COLOR = 0xffffffff;         // message color
const DWORD         INFO_COLOR = 0xffffff00;         // information display color

const DWORD         TOP_BACK_COLOR = 0xff0000ff;         // background gradient colors
const DWORD         BOTTOM_BACK_COLOR = 0xff000000;


//--------------------------------------------------------------------------------------
// Storage facilities
//--------------------------------------------------------------------------------------
const DWORD g_dwStorageFacilities[] =
{
    XSTORAGE_FACILITY_PER_USER_TITLE,
    XSTORAGE_FACILITY_PER_TITLE,
    XSTORAGE_FACILITY_GAME_CLIP
};

WCHAR*              g_strStorageDesc[] =
{
    L"Storage Facility: User Storage",
    L"Storage Facility: Per Title Storage",
    L"Storage Facility: Game Clip Storage"
};


//--------------------------------------------------------------------------------------
// Per-user storage items
//--------------------------------------------------------------------------------------
const DWORD         NUM_PER_USER_ITEMS = 4;
const WCHAR*        g_strPerUserItems[ NUM_PER_USER_ITEMS ] =
{
    L"UserFile0.dat",
    L"UserFile1.dat",
    L"UserFile2.dat",
    L"UserFile3.dat"
};

//--------------------------------------------------------------------------------------
// Misc constants
//--------------------------------------------------------------------------------------
const DWORD         MAX_STORAGE_RESULTS = 20;      // Maximum number of results to return when enumerating files
const DWORD         STORAGE_NAME_LENGTH = 256;

//--------------------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//--------------------------------------------------------------------------------------
ATG::HELP_CALLOUT g_HelpCallouts[] =
{
    { ATG::HELP_BACK_BUTTON,  ATG::HELP_PLACEMENT_2, L"Display\nhelp" },
    { ATG::HELP_A_BUTTON, ATG::HELP_PLACEMENT_1, L"Create file" },
    { ATG::HELP_X_BUTTON, ATG::HELP_PLACEMENT_1, L"Read file" },
    { ATG::HELP_LEFT_SHOULDER, ATG::HELP_PLACEMENT_2, L"Change storage\nfacility" },
    { ATG::HELP_RIGHT_SHOULDER, ATG::HELP_PLACEMENT_2, L"Change storage\nfacility" },
    { ATG::HELP_DPAD, ATG::HELP_PLACEMENT_1, L"Select file" },
    { ATG::HELP_Y_BUTTON, ATG::HELP_PLACEMENT_1, L"Delete file" },
};
static const DWORD  NUM_HELP_CALLOUTS = sizeof( g_HelpCallouts ) / sizeof( g_HelpCallouts[0] );


//--------------------------------------------------------------------------------------
// Name: Sample
// Desc: Main class to run this application. Most functionality is inherited
//       from the ATG::Application base class.
//--------------------------------------------------------------------------------------
class Sample : public ATG::Application
{
    ATG::Timer m_Timer;
    ATG::Font m_Font;
    ATG::Help m_Help;
    BOOL m_bDrawHelp;
    WCHAR           m_strMessage[256];
    BOOL m_bEnumerateFlag;
    DWORD m_dwStorageIndex;
    DWORD m_dwStorageListSize;
    XSTORAGE_ENUMERATE_RESULTS* m_pStorageList;
    WCHAR           m_strStorageFiles[MAX_STORAGE_RESULTS][STORAGE_NAME_LENGTH];
    DWORD m_dwListIndex;
    DWORD m_dwListSize;
    DWORD           m_dwFileBuffer[256];
    DWORD           m_dwFileReadBuffer[256];

    HRESULT         EnumerateStorage();

private:
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
// Name: Initialize
// Desc: This creates all device-dependent display objects.
//--------------------------------------------------------------------------------------
HRESULT Sample::Initialize()
{
    m_bDrawHelp = FALSE;
    m_dwStorageIndex = 0;
    m_dwListIndex = 0;
    m_dwListSize = 0;
    m_strMessage[0] = 0;
    m_bEnumerateFlag = TRUE;

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

    // Create storage enumeration list
    m_dwStorageListSize = sizeof( XSTORAGE_ENUMERATE_RESULTS ) + ( MAX_STORAGE_RESULTS *
                                                                   ( sizeof( XSTORAGE_FILE_INFO ) +
                                                                     ( XONLINE_MAX_PATHNAME_LENGTH * sizeof
                                                                       ( WCHAR ) ) ) );
    m_pStorageList = ( XSTORAGE_ENUMERATE_RESULTS* )malloc( m_dwStorageListSize );
    if( !m_pStorageList )
        return E_FAIL;

    ZeroMemory( m_pStorageList, m_dwStorageListSize );

    // Fill file buffer with random numbers so that it can be verified when
    // read back
    for( DWORD i = 0; i < ARRAYSIZE( m_dwFileBuffer ); ++i )
        m_dwFileBuffer[i] = rand();

    // Initialize Live
    if( FAILED( XOnlineStartup() ) )
        return E_FAIL;

    // Initialize logon
    ATG::SignIn::Initialize( 1, 1, TRUE, 1 );

    // Start sample by showing the signin UI for one user
    ATG::SignIn::ShowSignInUI();

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
    if( !ATG::SignIn::AreUsersSignedIn() )
    {
        return S_OK;
    }

    // Get input from all the gamepads
    ATG::GAMEPAD* pGamepad = ATG::Input::GetMergedInput();

    // Toggle help
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_BACK )
        m_bDrawHelp = !m_bDrawHelp;

    // Enumerate storage files
    if( m_bEnumerateFlag )
    {
        EnumerateStorage();
        m_bEnumerateFlag = FALSE;
    }

    // Show the signin UI
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_B && !ATG::SignIn::IsSystemUIShowing() )
    {
        ATG::SignIn::ShowSignInUI();
    }

    // Create file
    //
    // Creating a file is allowed for per-user and game clip storage. When creating and uploading larger files,
    // be sure to pass an XOVERLAPPED structure to XStorageUploadFromMemory and call the XStorageUploadFromMemoryGetProgress API
    // repeteadly to get the percentage indicator.
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_A &&
        g_dwStorageFacilities[m_dwStorageIndex] != XSTORAGE_FACILITY_PER_TITLE )
    {
        DWORD dwErr;

        // Write the file
        dwErr = XStorageUploadFromMemory( ATG::SignIn::GetSignedInUser(), m_strStorageFiles[m_dwListIndex],
                                          sizeof( m_dwFileBuffer ), ( const BYTE* )m_dwFileBuffer, NULL );

        if( dwErr == ERROR_SUCCESS )
            swprintf_s( m_strMessage, L"File was successfully written." );
        else
            swprintf_s( m_strMessage, L"Error: File was not written." );

        m_bEnumerateFlag = TRUE;
    }

    // Read and verify file
    //
    // All storage facilities allow for reading a file. However, because the per-title storage does not allow writing
    // files programmatically (files are uploaded through a tool), only the per-user and game clip storage files
    // are verified. When downloading larger files, be sure to pass an XOVERLAPPED structure to XStorageDownloadToMemory
    // and call the XStorageDownloadToMemoryGetProgress API repeteadly to get the percentage indicator.
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_X )
    {
        DWORD dwErr;

        XSTORAGE_DOWNLOAD_TO_MEMORY_RESULTS pResults = {0};
        ZeroMemory( m_dwFileReadBuffer, sizeof( m_dwFileReadBuffer ) );

        dwErr = XStorageDownloadToMemory( ATG::SignIn::GetSignedInUser(), m_strStorageFiles[m_dwListIndex],
                                          sizeof( m_dwFileReadBuffer ), ( const BYTE* )m_dwFileReadBuffer,
                                          sizeof( XSTORAGE_DOWNLOAD_TO_MEMORY_RESULTS ), &pResults, NULL );

        if( dwErr != ERROR_SUCCESS )
        {
            swprintf_s( m_strMessage, L"Error: File was not read." );
        }
        else if( g_dwStorageFacilities[m_dwStorageIndex] != XSTORAGE_FACILITY_PER_TITLE )
        {
            if( pResults.dwBytesTotal == sizeof( m_dwFileBuffer ) &&
                memcmp( ( void* )m_dwFileBuffer, m_dwFileReadBuffer, pResults.dwBytesTotal ) == 0 )
                swprintf_s( m_strMessage, L"File was successfully read and verified." );
            else
                swprintf_s( m_strMessage, L"File did not pass verification." );
        }
        else
        {
            swprintf_s( m_strMessage, L"File was successfully read." );
        }
    }

    // Delete file
    //
    // Deleting files is only allowed for the per-user storage facility.
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_Y )
    {
        if( m_dwListSize > 0 && g_dwStorageFacilities[m_dwStorageIndex] == XSTORAGE_FACILITY_PER_USER_TITLE )
        {
            DWORD dwErr = XStorageDelete( ATG::SignIn::GetSignedInUser(), m_strStorageFiles[m_dwListIndex], NULL );
            if( dwErr == ERROR_SUCCESS )
                swprintf_s( m_strMessage, L"File was successfully deleted." );
            else
                swprintf_s( m_strMessage, L"Error: File was not deleted." );
        }
    }

    // Change storage facility
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_LEFT_SHOULDER )
    {
        m_dwStorageIndex--;
        if( m_dwStorageIndex < 0 )
            m_dwStorageIndex = ARRAYSIZE( g_dwStorageFacilities ) - 1;

        m_bEnumerateFlag = TRUE;

        m_strMessage[0] = 0;
    }

    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER )
    {
        m_dwStorageIndex = ( ++m_dwStorageIndex ) % ARRAYSIZE( g_dwStorageFacilities );

        m_bEnumerateFlag = TRUE;

        m_strMessage[0] = 0;
    }

    // Allow user to select among content display names
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_UP )
    {
        if( m_dwListIndex > 0 )
            m_dwListIndex--;

        m_strMessage[0] = 0;
    }

    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_DOWN )
    {
        if( m_dwListIndex < ( m_dwListSize - 1 ) )
            m_dwListIndex++;

        m_strMessage[0] = 0;
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
        m_Font.DrawText( 0, 0, MSG_COLOR, L"XStorage" );
        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( 0, 0, INFO_COLOR, m_Timer.GetFrameRate(), ATGFONT_RIGHT );

        // Display files
        m_Font.DrawText( 0, 50, MSG_COLOR, g_strStorageDesc[m_dwStorageIndex] );

        m_Font.SetScaleFactors( 0.9f, 0.9f );
        for( DWORD i = 0; i < 8; ++i )
        {
            DWORD dwIndex = i + ( m_dwListIndex / 8 );

            if( dwIndex < m_dwListSize && dwIndex < MAX_STORAGE_RESULTS )
            {
                m_Font.DrawText( 0, ( FLOAT )( 100 + ( dwIndex * 30 ) ),
                                 dwIndex == m_dwListIndex ? SEL_COLOR : UNSEL_COLOR, m_strStorageFiles[dwIndex] );
            }
        }

        // Display message
        m_Font.SetScaleFactors( 0.9f, 0.9f );
        m_Font.DrawText( 0, -80, MSG_COLOR, m_strMessage );

        // Display list of commands
        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( 0, -34, MSG_COLOR, GLYPH_B_BUTTON L" Sign in" );
        m_Font.DrawText( 0, -10, MSG_COLOR, GLYPH_X_BUTTON L" Read file" );
        if( g_dwStorageFacilities[m_dwStorageIndex] != XSTORAGE_FACILITY_PER_TITLE )
        {
            m_Font.DrawText( 200, -34, MSG_COLOR, GLYPH_A_BUTTON L" Create file" );

            if( g_dwStorageFacilities[m_dwStorageIndex] == XSTORAGE_FACILITY_PER_USER_TITLE )
                m_Font.DrawText( 200, -10, MSG_COLOR, GLYPH_Y_BUTTON L" Delete file" );
        }

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
// Name: EnumerateStorage
// Desc: Enumerates stored files on the currently selected storage facility.
//--------------------------------------------------------------------------------------
HRESULT Sample::EnumerateStorage()
{
    DWORD dwErr;
    WCHAR strServerPath[512];
    DWORD dwServerPathLen;

    switch( g_dwStorageFacilities[m_dwStorageIndex] )
    {
            // Since there is one game clip file allowed per leaderboard, there is no need to enumerate. 
            // In this case, leaderboard id 0 was used. To use more leaderboards, first change the 
            // XLAST file specifying the maximum number of attachments, then specify the appropriate ids in 
            // the dwLeaderboardID field of the XSTORAGE_FACILITY_INFO_GAME_CLIP structure.
            // To retrieve a game clip for a specific leaderboard row, use the XUID returned in XUSER_STATS_ROW
            // struct. Then call XStorageBuildServerPathByXuid to build the server path.
        case XSTORAGE_FACILITY_GAME_CLIP:
        {
            dwServerPathLen = ARRAYSIZE( strServerPath );
            XSTORAGE_FACILITY_INFO_GAME_CLIP pFacilityInfo = {0};
            pFacilityInfo.dwLeaderboardID = 0;      // Use leaderboard with id of 0

            // Note that pwszItemName is set to L"" because the dwLeaderboardID determines the path
            dwErr = XStorageBuildServerPath( ATG::SignIn::GetSignedInUser(), XSTORAGE_FACILITY_GAME_CLIP,
                                             ( void* )&pFacilityInfo, sizeof( pFacilityInfo ), L"",
                                             strServerPath, &dwServerPathLen );
            if( dwErr != ERROR_SUCCESS )
                return E_FAIL;

            wcscpy_s( m_strStorageFiles[0], strServerPath );
            m_dwListSize = 1;
        }
            break;

            // Per-title storage is the only storage facility that can be enumerated using the XStorageEnumerate API.
            // The storage files are uploaded using a tool, thus the files can only be read from.
        case XSTORAGE_FACILITY_PER_TITLE:
        {
            dwServerPathLen = ARRAYSIZE( strServerPath );
            dwErr = XStorageBuildServerPath( ATG::SignIn::GetSignedInUser(), XSTORAGE_FACILITY_PER_TITLE,
                                             NULL, 0, L"*", strServerPath, &dwServerPathLen );

            if( dwErr != ERROR_SUCCESS )
                return E_FAIL;

            // Enumerate files
            dwErr = XStorageEnumerate( ATG::SignIn::GetSignedInUser(), strServerPath, 0,
                                       MAX_STORAGE_RESULTS, m_dwStorageListSize, m_pStorageList, NULL );
            if( dwErr != ERROR_SUCCESS )
                return E_FAIL;

            for( DWORD i = 0; i < m_pStorageList->dwNumItemsReturned; ++i )
            {
                swprintf_s( m_strStorageFiles[i], L"%s", m_pStorageList->pItems[i].pwszPathName );
            }
            m_dwListSize = m_pStorageList->dwNumItemsReturned;
        }
            break;

            // Per-user storage allows for only 8 files of at most 64KB per user. In this particular case
            // "enumerate" the files using a constant list defined above.
        case XSTORAGE_FACILITY_PER_USER_TITLE:
        {
            m_dwListSize = 0;
            for( DWORD i = 0; i < NUM_PER_USER_ITEMS; ++i )
            {
                dwServerPathLen = ARRAYSIZE( strServerPath );
                dwErr = XStorageBuildServerPath( ATG::SignIn::GetSignedInUser(), XSTORAGE_FACILITY_PER_USER_TITLE,
                                                 NULL, 0, ( WCHAR* )g_strPerUserItems[i],
                                                 strServerPath, &dwServerPathLen );

                if( dwErr != ERROR_SUCCESS )
                    return E_FAIL;

                swprintf_s( m_strStorageFiles[m_dwListSize], L"%s", strServerPath );
                m_dwListSize++;
            }
        }
            break;
    }

    m_dwListIndex = 0;

    return S_OK;
}
