//--------------------------------------------------------------------------------------
// XContent.cpp
//
// The sample shows how to enumerate XContent types, read and save data to XContent
// devices.
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


//--------------------------------------------------------------------------------------
// Maximum number of content name data structures allowed.
//--------------------------------------------------------------------------------------
static const DWORD  MAX_DATA_COUNT = 10;

//--------------------------------------------------------------------------------------
// Save root name to use. This can be any valid directory name.
//--------------------------------------------------------------------------------------
static CHAR g_szSaveRoot[] = "save";

//--------------------------------------------------------------------------------------
// Dummy file name to use for the save game.
//--------------------------------------------------------------------------------------
static CHAR g_szSaveGame[] = "save:\\savegame.txt";

//--------------------------------------------------------------------------------------
// Thumbnail image path.
//--------------------------------------------------------------------------------------
static CHAR g_szThumbnailImage[] = "game:\\XContentThumbnail.png";

//--------------------------------------------------------------------------------------
// Invliad user index
//--------------------------------------------------------------------------------------
static const DWORD  INVALID_USER_INDEX = 0xffffffff;

//--------------------------------------------------------------------------------------
// Color values
//--------------------------------------------------------------------------------------
#define SEL_COLOR           0xffff0000          // selection color
#define UNSEL_COLOR         0xffffffff          // unselection color

#define MSG_COLOR           0xffffffff          // message color
#define INFO_COLOR          0xffffff00          // information display color

#define TOP_BACK_COLOR      0xff0000ff          // background gradient colors
#define BOTTOM_BACK_COLOR   0xff000000


//--------------------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//--------------------------------------------------------------------------------------
ATG::HELP_CALLOUT g_HelpCallouts[] =
{
    { ATG::HELP_BACK_BUTTON,  ATG::HELP_PLACEMENT_2, L"Display\nhelp" },
    { ATG::HELP_A_BUTTON, ATG::HELP_PLACEMENT_1, L"Save Game" },
    { ATG::HELP_X_BUTTON, ATG::HELP_PLACEMENT_1, L"Load Game" },
    { ATG::HELP_DPAD, ATG::HELP_PLACEMENT_1, L"Select Saved Game" },
    { ATG::HELP_B_BUTTON, ATG::HELP_PLACEMENT_1, L"Signin" },
    { ATG::HELP_Y_BUTTON, ATG::HELP_PLACEMENT_1, L"Select Device" },
    { ATG::HELP_START_BUTTON, ATG::HELP_PLACEMENT_1, L"Transfer Content" },
    { ATG::HELP_LEFT_SHOULDER, ATG::HELP_PLACEMENT_1, L"Delete Content" },
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
    XCONTENT_DATA       m_ContentData[MAX_DATA_COUNT];  // Data containing display names
    LPDIRECT3DTEXTURE9  m_ThumbnailTextures[MAX_DATA_COUNT];
    DWORD m_dwContentDataCount;           // Number of data elements
    DWORD m_dwSelectedIndex;              // Selected element in UI
    WCHAR               m_szMessage[128];               // Message to be displayed in UI
    XOVERLAPPED m_Overlapped;                   // Overlapped object for device selector UI
    XCONTENTDEVICEID m_DeviceID;                   // Device identifier returned by device selector UI
    BOOL m_bFirstSignin;                 // Flag indicating first signin
    BOOL m_bDeviceUIActive;              // Is the device UI showing
    HANDLE m_hNotification;

    VOID                EnumerateContentNames();
    DWORD WriteSaveGame( CONST CHAR* szFileName, CONST WCHAR* szDisplayName );
    DWORD               ReadSaveGame( XCONTENT_DATA* ContentData );
    VOID                ShowDeviceUI();
    BOOL                ReadThumbnail( XCONTENT_DATA* pContentData, LPDIRECT3DTEXTURE9* pThumbnailTexture );
    BOOL WriteThumbnail( CONST CHAR* szFileName, CHAR* szThumbnailFile );
    BOOL                TransferContent( XCONTENT_DATA* pContentData );
    BOOL                DeleteContent( XCONTENT_DATA* pContentData );

private:
    virtual HRESULT     Initialize();
    virtual HRESULT     Update();
    virtual HRESULT     Render();
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
    m_bDrawHelp = FALSE;
    m_dwSelectedIndex = 0;
    m_dwContentDataCount = 0;

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

    m_bDeviceUIActive = FALSE;
    m_DeviceID = 0;

    ZeroMemory( m_ThumbnailTextures, sizeof( m_ThumbnailTextures ) );

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

    // Enumerate content names once device UI has been closed
    if( m_bDeviceUIActive )
    {
        if( XHasOverlappedIoCompleted( &m_Overlapped ) )
        {
            if( m_DeviceID == XCONTENTDEVICE_ANY )
            {
                ShowDeviceUI();
            }
            else
            {
                EnumerateContentNames();
                m_bDeviceUIActive = FALSE;
            }
        }
    }

    // Write a new content name and thumbnail image
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_A )
    {
        CHAR szFileName[XCONTENT_MAX_FILENAME_LENGTH];
        WCHAR szDisplayName[XCONTENT_MAX_DISPLAYNAME_LENGTH];

        // generate random name, duplicates will be overwritten
        DWORD dwGameIndex = ( GetTickCount() % 100 ) / 10;
        swprintf_s( szDisplayName, L"game%d", dwGameIndex );
        sprintf_s( szFileName, "game%d", dwGameIndex );

        if( WriteSaveGame( szFileName, szDisplayName ) == ERROR_SUCCESS &&
            WriteThumbnail( szFileName, g_szThumbnailImage ) )
        {
            wcscpy_s( m_szMessage, L"Game Saved..." );
        }
        else
        {
            wcscpy_s( m_szMessage, L"Save Failed..." );
        }

        EnumerateContentNames();
    }

    // Read save game and thumbnail image
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_X )
    {
        if( m_dwSelectedIndex < m_dwContentDataCount )
        {
            DWORD dwErr = ReadSaveGame( &m_ContentData[m_dwSelectedIndex] );
            if( dwErr == ERROR_SUCCESS && ReadThumbnail( &m_ContentData[m_dwSelectedIndex],
                                                         &m_ThumbnailTextures[m_dwSelectedIndex] ) )
            {
                wcscpy_s( m_szMessage, L"Game Loaded..." );
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

    // Transfer content package unaltered - as if it was copied through the network
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_START )
    {
        if( m_dwSelectedIndex < m_dwContentDataCount )
        {
            if( TransferContent( &m_ContentData[m_dwSelectedIndex] ) )
                wcscpy_s( m_szMessage, L"Package Transferred" );
            else
                wcscpy_s( m_szMessage, L"Package Transfer Failed..." );
        }
    }

    // Delete the content package
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_LEFT_SHOULDER )
    {
        if( m_dwSelectedIndex < m_dwContentDataCount )
        {
            if( DeleteContent( &m_ContentData[m_dwSelectedIndex] ) )
            {
                EnumerateContentNames();
                wcscpy_s( m_szMessage, L"Package Deleted" );
            }
            else
            {
                wcscpy_s( m_szMessage, L"Package Deletion Failed..." );
            }
        }
    }

    // Allow user to select among content display names
    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_UP )
    {
        if( m_dwSelectedIndex > 0 )
        {
            m_dwSelectedIndex--;
        }
        m_szMessage[0] = L'\0';
    }

    if( pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_DOWN )
    {
        if( m_dwSelectedIndex < ( m_dwContentDataCount - 1 ) )
        {
            m_dwSelectedIndex++;
        }
        m_szMessage[0] = L'\0';
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
        m_Font.DrawText( 0, 0, MSG_COLOR, L"XContent" );
        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( 0, 0, INFO_COLOR, m_Timer.GetFrameRate(), ATGFONT_RIGHT );

        // Display list of commands

        // Draw list of content display names
        m_Font.SetScaleFactors( 1.f, 1.f );
        m_Font.DrawText( 0, -34, MSG_COLOR, GLYPH_B_BUTTON L" Re-sign in\n" );
        m_Font.DrawText( 0, -10, MSG_COLOR, GLYPH_Y_BUTTON L" Select device\n" );
        m_Font.DrawText( 200, -34, MSG_COLOR, GLYPH_A_BUTTON L" Save game\n" );
        m_Font.DrawText( 200, -10, MSG_COLOR, GLYPH_X_BUTTON L" Load game\n" );

        m_Font.SetScaleFactors( 1.3f, 1.3f );
        m_Font.DrawText( 36, 32, INFO_COLOR, L"XContent Display Names\n" );

        m_Font.SetScaleFactors( 1.0f, 1.0f );

        for( DWORD i = 0; i < m_dwContentDataCount; ++i )
        {
            FLOAT fTextX;
            FLOAT fTextY;
            if( i < ( MAX_DATA_COUNT / 2 ) )
            {
                fTextX = 0;
                fTextY = ( FLOAT )( 70 + ( i * 40 ) );
            }
            else
            {
                fTextX = 300;
                fTextY = ( FLOAT )( 70 + ( ( i - ( MAX_DATA_COUNT / 2 ) ) * 40 ) );
            }

            // Draw thumbnail
            if( m_ThumbnailTextures[i] != NULL )
            {
                D3DRECT rect;
                rect.x1 = ATG::GetTitleSafeArea().x1 + ( LONG )fTextX;
                rect.y1 = ATG::GetTitleSafeArea().y1 + ( LONG )fTextY;
                rect.x2 = rect.x1 + 32;
                rect.y2 = rect.y1 + 32;
                m_Font.End();
                ATG::DebugDraw::DrawScreenSpaceTexturedRect( rect, m_ThumbnailTextures[i] );
                m_Font.Begin();
            }

            // Display content name
            WCHAR szDisplayName[256];

            swprintf_s( szDisplayName, L"%s\n", m_ContentData[i].szDisplayName );

            m_Font.DrawText( 60 + fTextX, fTextY, i == m_dwSelectedIndex ? SEL_COLOR : UNSEL_COLOR, szDisplayName );
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
                                   0,                       // No special flags
                                   iBytesRequested,         // Size of the device data struct
                                   &m_DeviceID,            // Return selected device information
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

    // Create enumerator for the default device
    DWORD dwRet;
    dwRet = XContentCreateEnumerator( ATG::SignIn::GetSignedInUser(), // Access data associated with this user
                                      m_DeviceID,         // Pass in selected device
                                      XCONTENTTYPE_SAVEDGAME,
                                      0,                     // No special flags
                                      ARRAYSIZE( m_ContentData ),
                                      &cbBuffer,
                                      &hEnum );

    if( dwRet != ERROR_SUCCESS )
    {
        ATG::FatalError( "EnumerateContentNames: Couldn't create content enumerator.\n" );
    }
    else
    {
        // Since XCONTENT_DATA is fixed size, the total buffer size should
        // be the same as the size of the m_ContentData array.
        assert( cbBuffer == sizeof( m_ContentData ) );
    }

    // Enumerate display names
    m_dwContentDataCount = 0;

    DWORD dwReturnCount;
    if( XEnumerate( hEnum, m_ContentData, sizeof( m_ContentData ),
                    &dwReturnCount, NULL ) == ERROR_SUCCESS )
    {
        m_dwContentDataCount = dwReturnCount;
    }

    CloseHandle( hEnum );
}


//--------------------------------------------------------------------------------------
// Name: WriteSaveGame
// Desc: Write a dummy save game file to the given content display name.
//--------------------------------------------------------------------------------------
DWORD Sample::WriteSaveGame( CONST CHAR *szFileName, CONST WCHAR* szDisplayName )
 {
    // Create event for asynchronous writing
    HANDLE hEventComplete = CreateEvent( NULL, FALSE, FALSE, NULL );

    if( hEventComplete == NULL )
         ATG::FatalError( "WriteSaveGame: Couldn't create event.\n" );

    XOVERLAPPED xov = {0};
    xov.hEvent = hEventComplete;

    XCONTENT_DATA contentData = {0};
    strcpy_s( contentData.szFileName, szFileName );
    wcscpy_s( contentData.szDisplayName, szDisplayName );
    contentData.dwContentType = XCONTENTTYPE_SAVEDGAME;
    contentData.DeviceID = m_DeviceID;

    // Mount the device associated with the display name for writing
    DWORD dwErr = XContentCreate( ATG::SignIn::GetSignedInUser(), g_szSaveRoot, &contentData,
                        XCONTENTFLAG_CREATEALWAYS, NULL, NULL, &xov );
    if( dwErr != ERROR_IO_PENDING )
 {
        CloseHandle( hEventComplete );
        ATG::DebugSpew( "WriteSaveGame: XContentCreate failed.\n" );
        return dwErr;
    }

    // Wait on hEventComplete handle
    if( XGetOverlappedResult( &xov, NULL, TRUE ) == ERROR_SUCCESS )
 {
        HANDLE hFile = CreateFile( g_szSaveGame, GENERIC_WRITE, 0,
            NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL );

        if( hFile != INVALID_HANDLE_VALUE )
 {
    // Write dummy data to the file
            CHAR szBuffer[] = "Test save game data.\n";
            DWORD dwWritten;

            if( WriteFile( hFile, ( VOID* )szBuffer, strlen( szBuffer ), &dwWritten, NULL ) == 0 )
 {
                CloseHandle( hFile );
                XContentClose( g_szSaveRoot, &xov );
                XGetOverlappedResult( &xov, NULL, TRUE );
                CloseHandle( hEventComplete );

                ATG::DebugSpew( "WriteSaveGame: WriteFile failed. Error = %08x\n", GetLastError() );
                return GetLastError();
            }

            CloseHandle( hFile );
        }
        else
 {
            ATG::DebugSpew( "WriteSaveGame: CreateFile failed. Error = %08x\n", GetLastError() );
            return GetLastError();
        }
    }

    XContentClose( g_szSaveRoot, &xov );

    // Wait for XCloseContent to complete
    XGetOverlappedResult( &xov, NULL, TRUE );

    CloseHandle( hEventComplete );

    return ERROR_SUCCESS;
}


//--------------------------------------------------------------------------------------
// Name: ReadSaveGame
// Desc: Read the previously saved dummy game file.
//--------------------------------------------------------------------------------------
DWORD Sample::ReadSaveGame( XCONTENT_DATA* ContentData )
{
    // Create event for asynchronous reading
    HANDLE hEventComplete = CreateEvent( NULL, FALSE, FALSE, NULL );

    if( hEventComplete == NULL )
        ATG::FatalError( "ReadSaveGame: Couldn't create event.\n" );

    XOVERLAPPED xov = {0};
    xov.hEvent = hEventComplete;

    // Mount the device for reading
    DWORD dwErr = XContentCreate( ATG::SignIn::GetSignedInUser(), g_szSaveRoot, ContentData,
                                  XCONTENTFLAG_OPENEXISTING, NULL, NULL, &xov );
    if( dwErr != ERROR_IO_PENDING )
    {
        CloseHandle( hEventComplete );
        ATG::DebugSpew( "ReadSaveGame: XContentCreate failed.\n" );
        return dwErr;
    }

    dwErr = XGetOverlappedResult( &xov, NULL, TRUE );
    if( dwErr == ERROR_SUCCESS )
    {
        HANDLE hFile = CreateFile( g_szSaveGame, GENERIC_READ,
                                   FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL );

        if( hFile != INVALID_HANDLE_VALUE )
        {
            //
            // Read some data from the file
            // Note, that when using XCONTENTFLAG_OPENEXISTING in XContentCreate, only
            // reading is allowed from the file, writing will have no effect.
            //

            CHAR szBuffer[16];
            DWORD dwRead;

            if( ReadFile( hFile, ( VOID* )szBuffer, ARRAYSIZE( szBuffer ), &dwRead, NULL ) == 0 )
            {
                CloseHandle( hFile );
                XContentClose( g_szSaveRoot, &xov );
                XGetOverlappedResult( &xov, NULL, TRUE );
                CloseHandle( hEventComplete );

                ATG::DebugSpew( "ReadSaveGame: ReadFile failed. Error = %08x\n", GetLastError() );
                return GetLastError();
            }

            CloseHandle( hFile );
        }
        else
        {
            ATG::DebugSpew( "ReadSaveGame: CreateFile failed. Error = %08x\n", GetLastError() );
            return GetLastError();
        }
    }
    else
    {
        XContentClose( g_szSaveRoot, &xov );
        XGetOverlappedResult( &xov, NULL, TRUE );
        CloseHandle( hEventComplete );

        ATG::DebugSpew( "ReadSaveGame: XContentCreate failed. Error = %08x\n", dwErr );
        return dwErr;
    }

    XContentClose( g_szSaveRoot, &xov );

    // Wait for XCloseContent to complete
    XGetOverlappedResult( &xov, NULL, TRUE );

    CloseHandle( hEventComplete );

    return ERROR_SUCCESS;
}

//--------------------------------------------------------------------------------------
// Name: ReadThumbnail
// Desc: Read an XContent thumbnail image. Returns TRUE if successful.
//--------------------------------------------------------------------------------------
BOOL Sample::ReadThumbnail( XCONTENT_DATA* pContentData, LPDIRECT3DTEXTURE9* pThumbnailTexture )
{
    // Retreive the size of the thumbnail image
    DWORD dwThumbnailBytes;
    if( XContentGetThumbnail( ATG::SignIn::GetSignedInUser(), pContentData, NULL,
                              &dwThumbnailBytes, NULL ) != ERROR_SUCCESS )
    {
        ATG::DebugSpew( "ReadThumbnail: Failed to get thumbnail image size.\n" );
        return FALSE;
    }

    // Allocate memory and retreive the actual image
    BYTE* pbThumbnailImage = new BYTE[dwThumbnailBytes];
    if( pbThumbnailImage == NULL )
    {
        ATG::DebugSpew( "ReadThumbnail: Failed to allocate memory.\n" );
        return FALSE;
    }

    if( XContentGetThumbnail( ATG::SignIn::GetSignedInUser(), pContentData, pbThumbnailImage,
                              &dwThumbnailBytes, NULL ) != ERROR_SUCCESS )
    {
        delete [] pbThumbnailImage;
        ATG::DebugSpew( "ReadThumbnail: Failed to get thumbnail image.\n" );
        return FALSE;
    }

    // Free last texture
    if( *pThumbnailTexture != NULL )
    {
        ( *pThumbnailTexture )->Release();
        *pThumbnailTexture = NULL;
    }

    // Convert thumbnail image from PNG to texture
    if( FAILED( D3DXCreateTextureFromFileInMemory( m_pd3dDevice, pbThumbnailImage,
                                                   dwThumbnailBytes, pThumbnailTexture ) ) )
    {
        delete [] pbThumbnailImage;
        ATG::DebugSpew( "ReadThumbnail: D3DXCreateTextureFromFileInMemory failed.\n" );
        return FALSE;
    }

    delete [] pbThumbnailImage;

    return TRUE;
}

//--------------------------------------------------------------------------------------
// Name: WriteThumbnail
// Desc: Write a XContent thumbnail image. Returns TRUE if successful.
//--------------------------------------------------------------------------------------
BOOL Sample::WriteThumbnail( CONST CHAR* szFileName, CHAR* szThumbnailFile )
 {
    XCONTENT_DATA contentData = {0};

    strcpy_s( contentData.szFileName, szFileName );
    contentData.dwContentType = XCONTENTTYPE_SAVEDGAME;
    contentData.DeviceID = m_DeviceID;

    // Read the thumbnail image file
    HANDLE hFile = CreateFile( szThumbnailFile, GENERIC_READ, FILE_SHARE_READ, NULL,
                                OPEN_EXISTING, 0, NULL );
    if( hFile == INVALID_HANDLE_VALUE )
 {
        ATG::DebugSpew( "WriteThumbnail: CreateFile failed for thumbnail image.\n" );
        return FALSE;
    }

    DWORD dwThumbnailBytes = GetFileSize( hFile, NULL );

    BYTE* pbThumbnailImage = new BYTE[dwThumbnailBytes];
    if( pbThumbnailImage == NULL )
 {
        CloseHandle( hFile );
        ATG::DebugSpew( "WriteThumbnail: Failed to allocate memory.\n" );
        return FALSE;
    }

    if( ReadFile( hFile, pbThumbnailImage, dwThumbnailBytes, &dwThumbnailBytes, NULL ) == 0 )
 {
        CloseHandle( hFile );
        delete [] pbThumbnailImage;
        ATG::DebugSpew( "WriteThumbnail: Failed to read thumbnail image.\n" );
        return FALSE;
    }

    if( XContentSetThumbnail( ATG::SignIn::GetSignedInUser(), &contentData, pbThumbnailImage,
                              dwThumbnailBytes, NULL ) != ERROR_SUCCESS )
 {
        CloseHandle( hFile );
        delete [] pbThumbnailImage;
        ATG::DebugSpew( "WriteThumbnail: Failed to set thumbnail image.\n" );
        return FALSE;
    }

    CloseHandle( hFile );
    delete [] pbThumbnailImage;

    return TRUE;
}


//--------------------------------------------------------------------------------------
// Name: TransferContent
// Desc: Example function that shows how to transfer a content package over the network
//       unaltered. When content packages are transferred through the network, the 
//       XContentOpenaPackage (for reading) and XContentCreatePackage (for writing) APIs
//       must be used ensure the content package gets transffered unaltered. If the
//       title uses some other mechanism to transfer content, it will be in violation
//       of TCR #065.
//
//       Returns TRUE if successful.
//--------------------------------------------------------------------------------------
BOOL Sample::TransferContent( XCONTENT_DATA* pContentData )
{
    //
    // Open content package for reading. This is done on the console that initiates the
    // package transfer.
    //
    HANDLE hPackageSrc;
    DWORD dwErr = XContentOpenPackage( ATG::SignIn::GetSignedInUser(), pContentData, &hPackageSrc );
    if( dwErr != ERROR_SUCCESS )
        return FALSE;

    // Read package file
    DWORD dwPackageSize = GetFileSize( hPackageSrc, NULL );
    if( dwPackageSize == 0 )
    {
        CloseHandle( hPackageSrc );
        return FALSE;
    }

    BYTE* pPackage = new BYTE[dwPackageSize];
    if( !pPackage )
    {
        CloseHandle( hPackageSrc );
        return FALSE;
    }

    if( !ReadFile( hPackageSrc, pPackage, dwPackageSize, &dwPackageSize, NULL ) )
    {
        delete [] pPackage;
        CloseHandle( hPackageSrc );
        return FALSE;
    }

    CloseHandle( hPackageSrc );

    //
    // Transfer package here...
    //

    //
    // Once the package file has been received, write it.
    //
    HANDLE hPackageDst;
    dwErr = XContentCreatePackage( ATG::SignIn::GetSignedInUser(), pContentData, FALSE, &hPackageDst );
    if( dwErr != ERROR_SUCCESS )
    {
        delete [] pPackage;
        return FALSE;
    }

    if( !WriteFile( hPackageDst, pPackage, dwPackageSize, &dwPackageSize, NULL ) )
    {
        delete [] pPackage;
        CloseHandle( hPackageDst );
        return FALSE;
    }

    CloseHandle( hPackageDst );

    delete [] pPackage;

    return TRUE;
}


//--------------------------------------------------------------------------------------
// Name: DeleteContent
// Desc: Deletes a content package. Returns TRUE if successful.
//--------------------------------------------------------------------------------------
BOOL Sample::DeleteContent( XCONTENT_DATA* pContentData )
{
    DWORD dwErr = XContentDelete( ATG::SignIn::GetSignedInUser(), pContentData, NULL );

    if( dwErr != ERROR_SUCCESS )
        return FALSE;

    return TRUE;
}
